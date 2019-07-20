#ifndef __MMU_HH__
#define __MMU_HH__

#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/mapper.hh"

namespace System
{
// TODO, this is still a primitive MMU.
class MMU
{
  protected:
    std::vector<Simulator::Mapper> mappers;

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

    TrainedMMU(int num_of_cores, Config &cfg)
        : MMU(num_of_cores)
    {}

    virtual void train(Addr va) {}
};

// Strategy 1, bring MFU pages to the near rows.
// TODO, Limitations
// (1) I assume a page is mapped across 4 channels, 8 banks and 2 ranks, which gives
//     us the page size of 64 * 4 * 8 * 2 = 4 kB;
// (2) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank.
// (3) Tracking order: fill the column first -> then rows -> next rank group
// (4) When all the near pages are filled, we don't do any re-allocations (this is
//     a future research topic)
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
          num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 * cfg.num_of_tiles /
                                     cfg.block_size),
          num_of_near_rows(num_of_rows_per_partition * cfg.num_of_parts /
                           cfg.num_stages),
	  mem_addr_decoding_bits(cfg.mem_addr_decoding_bits),
          max_near_page_row_id(num_of_near_rows - 1),
          max_near_page_col_id(num_of_cache_lines_per_row - 1),
          max_near_page_dep_id(2)
    {}

  protected:
    const unsigned max_near_page_row_id;
    const unsigned max_near_page_col_id;
    const unsigned max_near_page_dep_id;

    // To determine near pages
    struct NearPageTracking
    {
        unsigned row_id = 0;
        unsigned col_id = 0;
        unsigned dep_id = 0;
    };
    NearPageTracking next_avai_near_page;

    // TODO, need to maintain a hash table to record whether the page
    // has been occupied or not.

  protected:
    struct PageEntry
    {
        Addr page_id; // Original physical page ID

        uint64_t num_refs = 0;
    };
    typedef std::unordered_map<Addr,PageEntry> PageHash;
    PageHash pages; // All the touched pages
};
}

#endif
