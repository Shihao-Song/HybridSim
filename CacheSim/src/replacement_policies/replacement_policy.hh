#ifndef __CACHE_REPLACEMENT_POLICY_HH__
#define __CACHE_REPLACEMENT_POLICY_HH__

#include "../tags/cache_blk.hh"
#include "../tags/cache_tags.hh"

namespace CacheSimulator
{

template<class T>
class ReplacementPolicy
{
  public:
    ReplacementPolicy() {}

    virtual void upgrade(T *blk) {}
    virtual void downgrade(T *blk) {}
    virtual T* findVictim(Addr addr) = 0;
};	

class FAReplacementPolicy
      : public ReplacementPolicy<FABlk>
{
  public:
    FAReplacementPolicy()
        : ReplacementPolicy()
    {}

    virtual void policyInit(TagsWithFABlk *tags)
    {
        blks = tags->blks;
        head = &(tags->head);
        tail = &(tags->tail);
    }
  protected:
    FABlk *blks;

    FABlk **head;
    FABlk **tail;
};
}
#endif
