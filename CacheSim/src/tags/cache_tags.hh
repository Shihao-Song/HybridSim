#ifndef __CACHE_TAGS_HH__
#define __CACHE_TAGS_HH__

#include "../../../Configs/config.hh"
#include "../../../PCMSim/request.hh"
#include "cache_blk.hh"

namespace CacheSimulator
{
class SetWayAssocReplacementPolicy;
class FAReplacementPolicy;

template<class T>
class Tags
{
    typedef Configuration::Config Config;

  public:
    // Must be a constructor if there are any const type
    Tags(int level, Config &cfg)
        : blkSize(cfg.blkSize),
          blkMask(blkSize - 1),
          size(cfg.caches[level].size * 1024),
          numBlocks(size / blkSize)
    {
        std::cout << "Number of blocks: " << numBlocks << "\n\n";
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

    virtual T* accessBlock(Addr addr, Tick cur_clk = 0) = 0;

    virtual T* findVictim(Addr addr) = 0;

    virtual void insertBlock(Addr addr, T* victim, Tick cur_clk = 0) {}

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

class TagsWithSetWayBlk : public Tags<SetWayBlk>
{
    typedef Configuration::Config Config;

  public:
    TagsWithSetWayBlk(int level, Config &cfg) :
        Tags(level, cfg) {}

    virtual ~TagsWithSetWayBlk()
    {
        delete blks;
        delete policy;
    }

  protected:
    SetWayAssocReplacementPolicy *policy;
};

class TagsWithFABlk : public Tags<FABlk>
{
    typedef Configuration::Config Config;

  public:
    TagsWithFABlk(int level, Config &cfg) :
        Tags(level, cfg) {}

    virtual ~TagsWithFABlk()
    {
        delete blks;
        delete policy;
    }

   protected:
    FABlk *head;
    FABlk *tail;

  protected:
    FAReplacementPolicy *policy;

  friend class FAReplacementPolicy;
};
}
#endif
