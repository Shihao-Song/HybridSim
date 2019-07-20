#ifndef __MMU_HH__
#define __MMU_HH__

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

    Addr va2pa(Addr va, int core_id)
    {
        return mappers[core_id].va2pa(va);
    }
};

class TrainedMMU : public MMU
{
  public:
    typedef Simulator::Config Config;

    TrainedMMU(int num_of_cores, Config &cfg)
        : MMU(num_of_cores)
    {}

    virtual void train(Addr va);
};

// Strategy (1), bring MFU pages to the near rows.
class MFUToNearRows : public TrainedMMU
{
  protected:
    unsigned blk_size;

    unsigned num_of_channels;
    unsigned num_of_ranks;
    unsigned num_of_banks;
    unsigned num_of_partitions;

    unsigned num_of_rows_per_partition;
    unsigned num_of_cache_lines_per_row;

  public:
    MFUToNearRows(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg),
          blk_size(cfg.block_size),
          num_of_channels(cfg.num_of_channels),
          num_of_ranks(cfg.num_of_ranks),
          num_of_banks(cfg.num_of_banks),
          num_of_partitions(cfg.num_of_parts)
    {}

    // Limitations,
    // (1) I assume a page is mapped across 8 banks, 4 ranks and 2 channels, which gives
    // us the page size of 64 * 8 * 4 * 2 = 4 kB;
    // (2) I assume there are 4 channels in total;
    
};
}

#endif
