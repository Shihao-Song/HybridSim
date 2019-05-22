#ifndef __eDRAM_CACHE_REPLACEMENT_POLICY_HH__
#define __eDRAM_CACHE_REPLACEMENT_POLICY_HH__

#include "../tags/eDRAM_cache_fa_blk.hh"
#include "../tags/eDRAM_cache_tags.hh"

namespace eDRAMSimulator
{

template<class T>
class eDRAMCacheReplacementPolicy
{
  public:
    eDRAMCacheReplacementPolicy() {}

    virtual void upgrade(T *blk) {}
    virtual void downgrade(T *blk) {}
    virtual T* findVictim(Addr addr) = 0;
};	

class eDRAMCacheFAReplacementPolicy
      : public eDRAMCacheReplacementPolicy<eDRAMCacheFABlk>
{
  public:
    eDRAMCacheFAReplacementPolicy()
        : eDRAMCacheReplacementPolicy()
    {}

    virtual void policyInit(eDRAMCacheTagsWithFABlk *tags)
    {
        blks = tags->blks;
        head = &(tags->head);
        tail = &(tags->tail);
    }
  protected:
    eDRAMCacheFABlk *blks;

    eDRAMCacheFABlk **head;
    eDRAMCacheFABlk **tail;
};
}
#endif
