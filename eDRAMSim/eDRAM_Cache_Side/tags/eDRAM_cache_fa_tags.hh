#ifndef __eDRAM_CACHE_FA_TAGS_HH__
#define __eDRAM_CACHE_FA_TAGS_HH__

#include <assert.h>
#include <unordered_map>

#include "eDRAM_cache_tags.hh"

namespace eDRAMSimulator
{
class eDRAMCacheFATags : public eDRAMCacheTagsWithFABlk
{
  protected:
    // To make block indexing faster, a hash based address mapping is used
    typedef std::unordered_map<Addr, eDRAMCacheFABlk *> TagHash;
    TagHash tagHash;

  public:
    eDRAMCacheFATags();
    
    void tagsInit() override;

    Addr extractTag(Addr addr) const override
    {
        return blkAlign(addr);
    }

    eDRAMCacheFABlk* accessBlock(Addr addr) override;
    eDRAMCacheFABlk* findVictim(Addr addr) override;
    void insertBlock(Addr addr, eDRAMCacheFABlk* victim) override;
    void invalidate(eDRAMCacheFABlk* victim) override;

    unsigned numOccupiedBlocks() override { return tagHash.size(); }

  protected:
    eDRAMCacheFABlk* findBlock(Addr addr) const;

    Addr regenerateAddr(eDRAMCacheFABlk *blk) const override
    {
        return blk->tag;
    }
};
}
#endif
