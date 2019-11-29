#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <set>
#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/decoder.hh"
#include "Sim/mapper.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/stats.hh"
#include "Sim/trace.hh"


namespace System
{
class MMU
{
  protected:
    typedef Simulator::MemObject MemObject;

    typedef Simulator::Mapper Mapper;
    std::vector<Mapper> mappers;

    typedef Simulator::Request Request;

    unsigned num_of_cores;

  public:
    typedef uint64_t Addr;

    MMU(int num_cores) : num_of_cores(num_cores)
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
  protected:
    MemObject *mem_system;

  public:
    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Instruction Instruction;
    typedef Simulator::Trace Trace;

    TrainedMMU(int num_of_cores, Config &cfg)
        : MMU(num_of_cores)
    {}

    ~TrainedMMU()
    {}

    // The MMU should be able to do page migration among different memory nodes
    virtual bool pageMig() {}

    // Our simulator can run the program in multiple phases
    virtual void phaseDone() {}

    // MMU should have acess to the memory system to perform page migration
    void setMemSystem(MemObject *_sys) { mem_system = _sys; }

    // Register MMU statistics
    virtual void registerStats(Simulator::Stats &stats) {}

  protected:
    unsigned num_of_phases = 0;
};
}

#endif
