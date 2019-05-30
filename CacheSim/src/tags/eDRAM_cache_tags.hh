#ifndef __eDRAM_CACHE_TAGS_HH__
#define __eDRAM_CACHE_TAGS_HH__

#include "../../../PCMSim/request.hh"
#include "eDRAM_cache_fa_blk.hh"
#include "../consts.hh"

namespace eDRAMSimulator
{
class eDRAMCacheFAReplacementPolicy;

template<class T>
class eDRAMCacheTags
{
  public:
    // Must be a constructor if there are any const type
    eDRAMCacheTags()
        : blkSize(Constants::BLOCK_SIZE),
          blkMask(Constants::BLOCK_SIZE - 1),
          size(Constants::SIZE),
          numBlocks(size / blkSize)
    {}

    virtual ~eDRAMCacheTags()
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

class eDRAMCacheTagsWithFABlk :
    public eDRAMCacheTags<eDRAMCacheFABlk>
{
  public:
    eDRAMCacheTagsWithFABlk()
        : eDRAMCacheTags()
    {}

  protected:
    eDRAMCacheFABlk *head;
    eDRAMCacheFABlk *tail;

  protected:
    eDRAMCacheFAReplacementPolicy *policy;

  friend class eDRAMCacheFAReplacementPolicy;
};
}
#endif
