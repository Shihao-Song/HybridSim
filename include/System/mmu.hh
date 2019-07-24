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
// TODO, this is still a primitive MMU.
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

// In this simulator, we mainly focus on the TrainedMMU.
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
        if (trained_data_output != nullptr)
        {
            trained_data_out_fd.close();
        }
    }

    virtual void profiling() {}
    virtual void printProfiling() {}
    // virtual void train(std::vector<const char*> &traces) {}

    // TODO, should dis-able the following two functions
    // virtual void inference(Addr &pa) {}
    // virtual void preLoadTrainedData(const char*, double) {}

  protected:
    const char *trained_data_output = nullptr;
    std::ofstream trained_data_out_fd;

  public:
    void trainedDataOutput(const char *file)
    {
        trained_data_output = file;
        trained_data_out_fd.open(file);
    }
};

// Strategy 1, bring MFU pages to the near rows.
// TODO, Limitations
// (1) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank, 1 GB bank;
// (2) I assume Decoding is Rank, Partition, Row, Col, Bank, Channel, Cache_Line, MAX
class MFUPageToNearRows : public TrainedMMU
{
  protected:
    const unsigned num_of_rows_per_partition;
    const unsigned num_of_cache_lines_per_row;

    // How many rows are the near rows.
    const unsigned num_of_near_rows;

    // mem_addr_decoding_bits is used to determine the physical location of the page.
    const std::vector<int> mem_addr_decoding_bits;

    // Percentage of the touched pages to be re-allocated.
    const double perc_re_alloc;
  
  public:
    MFUPageToNearRows(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg),
          num_of_rows_per_partition(cfg.num_of_word_lines_per_tile),
          num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 / cfg.block_size * 
                                     cfg.num_of_tiles),
          num_of_near_rows(num_of_rows_per_partition * cfg.num_of_parts /
                           cfg.num_stages),
	  mem_addr_decoding_bits(cfg.mem_addr_decoding_bits),
          perc_re_alloc(cfg.perc_re_alloc),
          max_near_page_row_id(num_of_near_rows - 1),
          max_near_page_col_id(num_of_cache_lines_per_row - 1),
          max_near_page_dep_id(cfg.num_of_ranks)
    {}

    void va2pa(Request &req) override;
    void printProfiling() override
    {
        for (auto [key, value] : first_touch_instructions)
        {
            std::cout << key << " " << value.reads << " "
                                    << value.writes << "\n";
        }
    }
    // void train(std::vector<const char*> &traces) override;

    // TODO, should disable the following two functions.
    // void inference(Addr &pa) override;
    // void preLoadTrainedData(const char*, double) override;

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

    struct PageEntry
    {
        Addr page_id; // Original physical page ID

        bool near_row_page = false; // Page is at near row region.
        uint64_t num_refs = 0;

        PageLoc new_loc; // Re-mapped page location
    };
    // typedef std::unordered_map<Addr,PageEntry> PageHash;
    // typedef std::unordered_map<Addr,bool> PageHash; // Using hash to improve performance

  protected:
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
                       // std::cout << "R \n";
                   }
                   else
                   {
                       ++(iter->second).writes;
                       // std::cout << "W \n";
                   }
               };
    }

    std::unordered_map<Addr,bool> pages; // All the touched (allocated) pages

    struct RWCount
    {
        uint64_t reads = 0;
        uint64_t writes = 0;
    };
    std::unordered_map<Addr,RWCount> first_touch_instructions;

    bool near_region_full = false;
    PageLoc cur_near_page;
    void nextNearPage();

    /*
    PageLocHash touched_near_pages; // All the touched pages who are near pages

    std::vector<PageEntry> pages_mfu_order;

    void nextNearPage();

    PageHash re_alloc_pages; // Re-mapped pages;
    */
};
}

#endif
