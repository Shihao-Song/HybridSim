#ifndef __CACHE_FA_TAGS_HH__
#define __CACHE_FA_TAGS_HH__

#include <assert.h>
#include <unordered_map>

#include "../../../Configs/config.hh"
#include "cache_tags.hh"

namespace CacheSimulator
{
class FATags : public TagsWithFABlk
{
    typedef Configuration::Config Config;

  protected:
    // To make block indexing faster, a hash based address mapping is used
    typedef std::unordered_map<Addr, FABlk *> TagHash;
    TagHash tagHash;

  public:
    FATags(int level, Config &cfg);
    
    void tagsInit() override;

    Addr extractTag(Addr addr) const override
    {
        return blkAlign(addr);
    }

    FABlk* accessBlock(Addr addr) override;
    FABlk* findVictim(Addr addr) override;
    void insertBlock(Addr addr, FABlk* victim) override;
    void invalidate(FABlk* victim) override;

    unsigned numOccupiedBlocks() override { return tagHash.size(); }

  protected:
    FABlk* findBlock(Addr addr) const;

    Addr regenerateAddr(FABlk *blk) const override
    {
        return blk->tag;
    }
};
}
#endif
