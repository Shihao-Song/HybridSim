#ifndef __eDRAM_CACHE_FA_LRU_HH__
#define __eDRAM_CACHE_FA_LRU_HH__

#include "eDRAM_cache_replacement_policy.hh"

namespace eDRAMSimulator
{
class eDRAMCacheFALRU : public eDRAMCacheFAReplacementPolicy
{
  public:
    eDRAMCacheFALRU();

    void upgrade(eDRAMCacheFABlk *blk) override;
    void downgrade(eDRAMCacheFABlk *blk) override;
    eDRAMCacheFABlk* findVictim(Addr addr) override;
};
}
#endif
