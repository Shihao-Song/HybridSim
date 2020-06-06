#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include <fstream>

#include "Sim/request.hh"
#include "Sim/stats.hh"

// This class should be the base class for all the memory component
namespace System { class MMU; }
namespace Simulator
{
class MemObject
{
  public:
    MemObject(){}
    ~MemObject()
    {
        if (mem_trace_extr_mode)
        {
            mem_trace.close();
        }
    }

    virtual int pendingRequests() = 0;

    virtual bool send(Request &req) = 0;
    virtual void tick() {}
    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }
    virtual void setPrevLevel(MemObject *_prev_level) { prev_levels.push_back(_prev_level); }

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void setArbitrator(int _num_clients)
    {
        arbitrator = true;
        num_clients = _num_clients;
    }

    virtual void setBoundaryMemObject()
    {
        boundary = true;
    }

    virtual void registerStats(Stats &stats) {}

    // Re-initialize cache
    virtual void reInitialize() {}

    // Inclusive invalidation
    virtual bool incluInval(uint64_t addr) {} 

    // Do we want to extract memory traces from this mem_object?
    virtual void setTraceOutput(const char* file)
    {
        mem_trace_extr_mode = true;
        mem_trace.open(file);
    }

    // Write-back all the physical address belong to the given page_id.
    // virtual bool writeback(uint64_t page_id) = 0;
    void setMMU(System::MMU *_mmu) { mmu = _mmu; }

    bool isOnChip() const { return on_chip; }

    void setInclusive() { inclusive = true; }
    bool isInclusive() const { return inclusive; }

  protected:
    bool on_chip = false;
    bool inclusive = false; // Is the mem object inclusive? Default: non-inclusive.

  protected:
    System::MMU *mmu; // Give mem object access to MMU

    MemObject *next_level;

    std::vector<MemObject*> prev_levels;

    int id = -1;

    bool arbitrator = false; // Am i the arbitrator.
    int num_clients = -1;
    int selected_client = 0; // Which client is allowed to pass.

    bool boundary = false; // I'm the boundary of my group.

    bool mem_trace_extr_mode = false;
    std::ofstream mem_trace;

  public:
    virtual void debugPrint(){}

};
}

#endif
