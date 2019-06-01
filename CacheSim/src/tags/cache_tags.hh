#ifndef __CACHE_TAGS_HH__
#define __CACHE_TAGS_HH__

#include "../../../Configs/config.hh"
#include "../../../PCMSim/request.hh"
#include "cache_blk.hh"

namespace CacheSimulator
{
class FAReplacementPolicy;

// TODO, build tags based on cache level
template<class T>
class Tags
{
    typedef Configuration::Config Config;

  public:
    // Must be a constructor if there are any const type
    // TODO, size should be dependent on cache level
    Tags(int level, Config &cfg)
        : blkSize(cfg.blkSize),
          blkMask(blkSize - 1),
          size(cfg.caches[level].size * 1024),
          numBlocks(size / blkSize)
    {}

    virtual ~Tags()
    {
        delete blks;
    }

  protected:
    const unsigned blkSize; // cache-line (block) size
    const Addr blkMask;
    const unsigned long long size; // (entire) cache size
    const unsigned numBlocks; // number of blocks in the cache

  protected:
    T *blks; // All cache blocks

  public:
    virtual void tagsInit() {}

    virtual Addr extractTag(Addr addr) const = 0;

    virtual T* accessBlock(Addr addr) = 0;

    virtual T* findVictim(Addr addr) = 0;

    virtual void insertBlock(Addr addr, T* victim) {}

    virtual void invalidate(T* victim) {}

    virtual unsigned numOccupiedBlocks() = 0; // Only make sense for FA

  protected:
    virtual T* findBlock(Addr addr) const = 0;

    // This is a universal function
    Addr blkAlign(Addr addr) const
    {
        return addr & ~blkMask;
    }

    virtual Addr regenerateAddr(T *blk) const = 0;
};

class TagsWithFABlk : public Tags<FABlk>
{
    typedef Configuration::Config Config;

  public:
    TagsWithFABlk(int level, Config &cfg) :
        Tags(level, cfg) {}

  protected:
    FABlk *head;
    FABlk *tail;

  protected:
    FAReplacementPolicy *policy;

  friend class FAReplacementPolicy;
};
}
#endif
