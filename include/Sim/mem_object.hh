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
    ~MemObject() {}

    virtual int pendingRequests() = 0;

    virtual bool send(Request &req) = 0;
    virtual void tick() {}
    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }
    virtual void setPrevLevel(MemObject *_prev_level) { prev_levels.push_back(_prev_level); }

    virtual void drainPendingReqs() {}
    virtual void outputMemContents(std::string &_fn) {}
    virtual std::vector<bool> getAccessInfo() {}

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void registerStats(Stats &stats) {}

    // Re-initialize cache
    virtual void reInitialize() {}

    // Write-back all the physical address belong to the given page_id.
    // virtual bool writeback(uint64_t page_id) = 0;
    void setMMU(System::MMU *_mmu) { mmu = _mmu; }

    bool isOnChip() const { return on_chip; }

    virtual void setBoundaryMemObject()
    {
        boundary = true;
    }

  protected:
    bool on_chip = false;
    // bool inclusive = false; // Is the mem object inclusive? Default: non-inclusive.

  protected:
    bool svf_extr = false;

  public:
    virtual void setSVFExtr() { svf_extr = true; }
    virtual void setVictimExe() { return; }
    virtual void resetVictimExe() { return; }

  protected:
    System::MMU *mmu; // Give mem object access to MMU

    MemObject *next_level;

    std::vector<MemObject*> prev_levels;

    int id = -1;

    bool boundary = false; // I'm the boundary of my group.

  public:
    virtual void debugPrint(){}

};
}

#endif
