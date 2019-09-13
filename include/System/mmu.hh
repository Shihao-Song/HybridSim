#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <set>
#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/decoder.hh"
#include "Sim/mapper.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"

namespace System
{
class MMU
{
  protected:
    typedef Simulator::Mapper Mapper;
    std::vector<Mapper> mappers;

    typedef Simulator::Request Request;

  public:
    typedef uint64_t Addr;

    MMU(int num_cores)
    {
        for (int i = 0; i < num_cores; i++)
        {
            mappers.emplace_back(i);
        }
    }

    virtual void va2pa(Request &req)
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;
    }
};

class TrainedMMU : public MMU
{
  public:
    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Instruction Instruction;
    typedef Simulator::TXTTrace TXTTrace;

    TrainedMMU(int num_of_cores, Config &cfg)
        : MMU(num_of_cores)
    {}

    ~TrainedMMU()
    {
        if (mmu_profiling_data_output_file != "N/A")
        {
            mmu_profiling_data_out.close();
        }
    }

    virtual void printProfiling() {}

    virtual void setProfilingStage() { profiling_stage = true; inference_stage = false; }
    virtual void setInferenceStage() { inference_stage = true; profiling_stage = false; }


    virtual void setSizes(std::vector<int> sizes) {}

  protected:
    bool profiling_stage = false;
    bool inference_stage = false;

    std::string mmu_profiling_data_output_file = "N/A";
    std::ofstream mmu_profiling_data_out;

  public:
    void profilingDataOutput(std::string file)
    {
        mmu_profiling_data_output_file = file;
        //mmu_profiling_data_out.open(file);
        //assert(mmu_profiling_data_out.good());
    }
};

// TODO, Limitations
// (1) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank, 1 GB bank;
// (2) Decoding is fixed: Rank, Partition, Tile, Row, Col, Bank, Channel, Cache_Line
class NearRegionAware : public TrainedMMU
{
  protected:
    const unsigned num_of_cache_lines_per_row;
    const unsigned num_of_tiles;
    const unsigned num_of_partitions;
    const unsigned num_of_ranks;
    const unsigned num_of_near_rows;

    // mem_addr_decoding_bits is used to determine the physical location of the page.
    const std::vector<int> mem_addr_decoding_bits;

  public:
    NearRegionAware(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg),
          num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 / cfg.block_size),
          num_of_tiles(cfg.num_of_tiles),
          num_of_partitions(cfg.num_of_parts),
          num_of_ranks(cfg.num_of_ranks),
          num_of_near_rows(cfg.num_of_word_lines_per_tile /cfg.num_stages),
          mem_addr_decoding_bits(cfg.mem_addr_decoding_bits)
    {}

  // Define data structures
  protected:
    struct PageLoc // Physical location
    {
        unsigned rank_id = 0;
        unsigned part_id = 0;
        unsigned tile_id = 0;
        unsigned row_id = 0;
        unsigned col_id = 0;

        PageLoc& operator=(PageLoc other)
        {
            rank_id = other.rank_id;
            part_id = other.part_id;
            tile_id = other.tile_id;
            row_id = other.row_id;
            col_id = other.col_id;
            return *this;
        }

	bool operator==(const PageLoc &other) const
        {
            return rank_id == other.rank_id &&
                   part_id == other.part_id &&
                   tile_id == other.tile_id &&
                   row_id == other.row_id && 
                   col_id == other.col_id;
        }
    };

    // TODO, this hash structure is not used currently. I forgot why
    // I coded it.
    struct PageLocHashKey
    {
        template<typename T = PageLoc>
        std::size_t operator()(T &p) const
        {
            return std::hash<unsigned>()(p.rank_id) ^
                   std::hash<unsigned>()(p.part_id) ^
                   std::hash<unsigned>()(p.tile_id) ^
                   std::hash<unsigned>()(p.row_id) ^ 
                   std::hash<unsigned>()(p.col_id); 
        }
    };
    typedef std::unordered_map<PageLoc, bool, PageLocHashKey> PageLocHash;

    bool near_region_full = false;
    PageLoc cur_re_alloc_page;
    
    enum INCREMENT_LEVEL: int
    {
        COL,ROW,TILE,PARTITION,RANK
    };
    virtual bool nextReAllocPage(int);
};

// Strategy 1, bring MFU pages to the near rows.
class MFUPageToNearRows : public NearRegionAware
{
  public:
    MFUPageToNearRows(int num_of_cores, Config &cfg)
        : NearRegionAware(num_of_cores, cfg)
    {}

    void va2pa(Request &req) override;

    void setSizes(std::vector<int> sizes) override
    {
        num_profiling_entries = sizes[0];
    }

    void printProfiling() override
    {
        std::vector<Page_Info> MFU_pages_profiling;
        std::vector<Page_Info> MFU_pages_inference;

        for (auto [key, value] : pages)
        {
            MFU_pages_profiling.push_back(value);
            MFU_pages_inference.push_back(value);
        }

        std::sort(MFU_pages_profiling.begin(), MFU_pages_profiling.end(),
                  [](const Page_Info &a, const Page_Info &b)
                  {
                      return (a.reads_in_profiling_stage + a.writes_in_profiling_stage) > 
                             (b.reads_in_profiling_stage + b.writes_in_profiling_stage);
                  });
	
        std::sort(MFU_pages_inference.begin(), MFU_pages_inference.end(),
                  [](const Page_Info &a, const Page_Info &b)
                  {
                      return (a.reads_in_inference_stage + a.writes_in_inference_stage) > 
                             (b.reads_in_inference_stage + b.writes_in_inference_stage);
                  });

        // TODO, change the naming convention.
        std::string profiling_file = mmu_profiling_data_output_file + "_3M.csv";
        std::ofstream profiling_output(profiling_file);
        assert(profiling_output.good());

        // TODO, change the naming convention.
        std::string inference_file = mmu_profiling_data_output_file + "_7M.csv";
        std::ofstream inference_output(inference_file);
        assert(inference_output.good());

        for (int i = 0; i < pages.size(); i++)
        {
            unsigned accesses = MFU_pages_profiling[i].reads_in_profiling_stage +
                                MFU_pages_profiling[i].writes_in_profiling_stage;

            if (MFU_pages_profiling[i].allocated_in_profiling_stage &&
                accesses)
            {

                profiling_output << MFU_pages_profiling[i].page_id << ","
                                 << MFU_pages_profiling[i].first_touch_instruction << ","
                                 << accesses << "\n";
            }
	    else
            {
                break;
            }
        }

        for (int i = 0; i < pages.size(); i++)
        {
            unsigned accesses = MFU_pages_inference[i].reads_in_inference_stage +
                                MFU_pages_inference[i].writes_in_inference_stage;
            if (accesses)
            {
	        inference_output << MFU_pages_inference[i].page_id << ","
                                 << MFU_pages_inference[i].first_touch_instruction << ","
                                 << accesses << "\n";
            }
	    else
	    {
                break;    
            }
	}
        profiling_output.close();
	inference_output.close();
    }

  protected:
    void profiling_new(Request&);

    void inference(Request&);
    void profiling(Request&);
    
    struct Page_Info
    {
        Addr page_id;
        Addr first_touch_instruction; // The first-touch instruction that brings in this page

        bool allocated_in_profiling_stage = false;

        uint64_t reads_in_profiling_stage = 0;
        uint64_t writes_in_profiling_stage = 0;

        uint64_t reads_in_inference_stage = 0;
        uint64_t writes_in_inference_stage = 0;
    };
    std::unordered_map<Addr,Page_Info> pages; // All the touched (allocated) pages, used in
                                         // profiling stage.

    std::unordered_map<Addr,Addr> re_alloc_pages; // All the re-allocated MFU pages, used in
                                                  // inference stage.

    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;

        // Is this instruction captured in profiling stage
        bool captured_in_profiling_stage = false;

        // I keep as many information as possible, so that different ordering can be
        // applied.
        // Number of accesses in profiling stage
        uint64_t reads_profiling_stage = 0;
        uint64_t writes_profiling_stage = 0;

        // Number of accesses in inference stage
        uint64_t reads_inference_stage = 0;
        uint64_t writes_inference_stage = 0;

        // Number of pages in profiling stage
        uint64_t touched_pages_profiling_stage = 0;

        // Number of pages in inference stage
        uint64_t touched_pages_inference_stage = 0;
    };
    std::unordered_map<Addr,First_Touch_Instr_Info> first_touch_instructions;

    int num_profiling_entries = -1;
};

class TrainedMMUFactory
{
    typedef Simulator::Config Config;

  private:
    std::unordered_map<std::string,
                       std::function<std::unique_ptr<TrainedMMU>(int,Config&)>> factories;

  public:
    TrainedMMUFactory()
    {
        factories["MFUPageToNearRows"] = [](int num_of_cores, Config &cfg)
                 {
                     return std::make_unique<MFUPageToNearRows>(num_of_cores, cfg);
                 };
    }

    auto createMMU(int num_of_cores, Config &cfg)
    {
        std::string mmu_type = cfg.mmu_type;
        if (auto iter = factories.find(mmu_type);
            iter != factories.end())
        {
            return iter->second(num_of_cores, cfg);
        }
        else
        {
            std::cerr << "Unsupported MMU type. \n";
            exit(0);
        }
    }
};

static TrainedMMUFactory TrainedMMUFactories;
static auto createTrainedMMU(int num_of_cores, Simulator::Config &cfg)
{
    return TrainedMMUFactories.createMMU(num_of_cores, cfg);
}
}

#endif
