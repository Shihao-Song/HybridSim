#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/decoder.hh"
#include "Sim/mapper.hh"
#include "Sim/trace.hh"

namespace System
{
// TODO, this is still a primitive MMU.
class MMU
{
  protected:
    typedef Simulator::Mapper Mapper;
    std::vector<Mapper> mappers;

  public:
    typedef uint64_t Addr;

    MMU(int num_cores)
    {
        for (int i = 0; i < num_cores; i++)
        {
            mappers.emplace_back(i);
        }
    }

    virtual Addr va2pa(Addr va, int core_id)
    {
        return mappers[core_id].va2pa(va);
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

    virtual void train(std::vector<const char*> &traces) {}
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

  public:
    MFUPageToNearRows(int num_of_cores, Config &cfg)
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

    void train(std::vector<const char*> &traces) override;

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
    typedef std::unordered_map<Addr,PageEntry> PageHash;

  protected:
    PageHash pages; // All the touched pages
    PageLocHash touched_near_pages; // All the touched pages who are near pages

    std::vector<PageEntry> pages_mfu_order;

    bool near_region_full = false;
    PageLoc cur_near_page;
    void nextNearPage();

    PageHash re_alloc_pages; // Re-mapped pages;
};
}

#endif
