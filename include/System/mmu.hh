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

    void setProfilingStage() { profiling_stage = true; inference_stage = false; }
    void setInferenceStage() { inference_stage = true; profiling_stage = false; }

  protected:
    bool profiling_stage = false;
    bool inference_stage = false;

    std::string mmu_profiling_data_output_file = "N/A";
    std::ofstream mmu_profiling_data_out;

  public:
    void profilingDataOutput(std::string file)
    {
        mmu_profiling_data_output_file = file;
        mmu_profiling_data_out.open(file);
        assert(mmu_profiling_data_out.good());
    }
};

// TODO, Limitations
// (1) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank, 1 GB bank;
// (2) I assume Decoding is Rank, Partition, Row, Col, Bank, Channel, Cache_Line, MAX
class NearRegionAware : public TrainedMMU
{
  protected:
    const unsigned num_of_rows_per_partition;
    const unsigned num_of_cache_lines_per_row;

    // How many rows are the near rows.
    const unsigned num_of_near_rows;

    // mem_addr_decoding_bits is used to determine the physical location of the page.
    const std::vector<int> mem_addr_decoding_bits;

  public:
    NearRegionAware(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg),
          num_of_rows_per_partition(cfg.num_of_word_lines_per_tile),
          num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 / cfg.block_size *
                                     cfg.num_of_tiles),
          num_of_near_rows(num_of_rows_per_partition * cfg.num_of_parts /
                           cfg.num_stages),
          mem_addr_decoding_bits(cfg.mem_addr_decoding_bits),
          max_near_page_row_id(num_of_near_rows - 1),
          max_near_page_col_id(num_of_cache_lines_per_row - 1),
          max_near_page_dep_id(cfg.num_of_ranks)
    {}
  // Define data structures
  protected:
    const unsigned max_near_page_row_id;
    const unsigned max_near_page_col_id;
    const unsigned max_near_page_dep_id;
    
    struct PageLoc // Physical location
    {
        unsigned row_id = 0;
        unsigned col_id = 0;
        unsigned dep_id = 0;

        PageLoc& operator=(PageLoc other)
        {
            row_id = other.row_id;
            col_id = other.col_id;
            dep_id = other.dep_id;
            return *this;
        }

	bool operator==(const PageLoc &other) const
        {
            return row_id == other.row_id && 
                   col_id == other.col_id &&
                   dep_id == other.dep_id;
        }
    };

    struct PageLocHashKey
    {
        template<typename T = PageLoc>
        std::size_t operator()(T &p) const
        {
            return std::hash<unsigned>()(p.row_id) ^ 
                   std::hash<unsigned>()(p.col_id) ^ 
                   std::hash<unsigned>()(p.dep_id);
        }
    };
    typedef std::unordered_map<PageLoc, bool, PageLocHashKey> PageLocHash;

    bool near_region_full = false;
    PageLoc cur_re_alloc_page;
    void nextReAllocPage();    
};

// Strategy 1, bring MFU pages to the near rows.
class MFUPageToNearRows : public NearRegionAware
{
  public:
    MFUPageToNearRows(int num_of_cores, Config &cfg)
        : NearRegionAware(num_of_cores, cfg)
    {}

    void va2pa(Request &req) override;
    void printProfiling() override
    {
        std::vector<RWCount> profiling_data;
        for (auto [key, value] : first_touch_instructions)
        {
            profiling_data.push_back(value);
        }
        std::sort(profiling_data.begin(), profiling_data.end(),
                  [](const RWCount &a, const RWCount &b)
                  {
                      return (a.reads + a.writes) > (b.reads + b.writes);
                  });
        for (auto entry : profiling_data)
        {
            mmu_profiling_data_out << entry.eip << " "
                                   << entry.reads << " "
                                   << entry.writes << "\n";
        }
    }

  protected:
    void inference(Request&);
    void profiling(Request&);
    auto profilingCallBack()
    {
        return [this](Request &req)
               {
                   auto iter = first_touch_instructions.find(req.eip);
                   assert(iter != first_touch_instructions.end());

                   if (req.req_type == Request::Request_Type::READ)
                   {
                       ++(iter->second).reads;
                   }
                   else
                   {
                       ++(iter->second).writes;
                   }
               };
    }

    std::unordered_map<Addr,bool> pages; // All the touched (allocated) pages, used in
                                         // profiling stage.

    std::unordered_map<Addr,Addr> re_alloc_pages; // All the re-allocated MFU pages, used in
                                                  // inference stage.

    struct RWCount
    {
        Addr eip;

        uint64_t reads = 0;
        uint64_t writes = 0;
    };
    std::unordered_map<Addr,RWCount> first_touch_instructions;
};

// Strategy 2, give the control of near pages to memory controller.
}

#endif
