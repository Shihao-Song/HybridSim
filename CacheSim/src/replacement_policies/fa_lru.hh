#ifndef __CACHE_FA_LRU_HH__
#define __CACHE_FA_LRU_HH__

#include "replacement_policy.hh"

namespace CacheSimulator
{
class FALRU : public FAReplacementPolicy
{
  public:
    FALRU();

    void upgrade(FABlk *blk) override;
    void downgrade(FABlk *blk) override;
    FABlk* findVictim(Addr addr) override;
};
}
#endif
