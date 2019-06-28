#ifndef __CACHE_REPLACEMENT_POLICY_HH__
#define __CACHE_REPLACEMENT_POLICY_HH__

#include "CacheSim/cache_blk.hh"
#include "CacheSim/tags/cache_tags.hh"

namespace CacheSimulator
{
template<class T>
class ReplacementPolicy
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

  public:
    ReplacementPolicy() {}

    virtual void upgrade(T *blk, Tick cur_clk = 0) {}
    virtual void downgrade(T *blk) {}
};	

class SetWayAssocReplacementPolicy
      : public ReplacementPolicy<SetWayBlk>
{
  public:
    SetWayAssocReplacementPolicy()
        : ReplacementPolicy()
    {}

    virtual SetWayBlk* findVictim(const std::vector<SetWayBlk *>&set) const = 0; 
};

class FAReplacementPolicy
      : public ReplacementPolicy<FABlk>
{
  public:
    FAReplacementPolicy()
        : ReplacementPolicy()
    {}

    virtual void policyInit(FABlk *blks, FABlk **head, FABlk **tail)
    {
        blks = blks;
        head = head;
        tail = tail;
    }

    virtual FABlk* findVictim(Addr addr) = 0;

  protected:
    FABlk *blks;

    FABlk **head;
    FABlk **tail;
};
}
#endif
