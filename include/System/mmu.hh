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

  public:
    MFUToNearRows(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg)
    {}
};
}

#endif
