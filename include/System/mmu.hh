#ifndef __MMU_HH__
#define __MMU_HH__

#include <vector>

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

    // To train an MMU and record
    // (1) all the touched pages,
    // (2) reference counts for each touch page
    // void train(Addr va)
    // {
    
    // }
};
}

#endif
