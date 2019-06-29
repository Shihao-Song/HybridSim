#ifndef __CACHE_TAGS_HH__
#define __CACHE_TAGS_HH__

#include "Sim/config.hh"
#include "Sim/request.hh"

#include "CacheSim/cache_blk.hh"
#include "CacheSim/tags/replacement_policies/replacement_policy.hh"

namespace CacheSimulator
{
template<class T>
class Tags
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;
    const Addr MaxAddr = (Addr) - 1;

    typedef Simulator::Config Config;

  public:
    // Must be a constructor if there are any const type
    Tags(int level, Config &cfg)
        : block_size(cfg.block_size),
          block_mask(block_size - 1),
          size(cfg.caches[level].size),
          num_blocks(size / block_size),
          blks(new T[num_blocks])
    {}

  protected:
    const unsigned block_size; // cache-line (block) size in bytes
    const Addr block_mask;
    const unsigned long long size; // (entire) cache size in bytes
    const unsigned num_blocks; // number of blocks in the cache

  protected:
    std::unique_ptr<T[]> blks; // All cache blocks

  public:

    // return val: <hit in cache?, block-aligned address>
    virtual std::pair<bool, Addr> accessBlock(Addr addr, Tick cur_clk = 0) = 0;

    // return val: <write-back required?, write-back address>
    virtual std::pair<bool, Addr> insertBlock(Addr addr, Tick cur_clk = 0) = 0;
    
  protected:

    // Initialize tag
    virtual void tagsInit() {}

    Addr blkAlign(Addr addr) const
    {
        return addr & ~block_mask;
    }

    // Regenerate the original physical address
    virtual Addr regenerateAddr(T *blk) const = 0;
    
    // Extract tag for a given address
    virtual Addr extractTag(Addr addr) const = 0;

    // Locate the cache block based on physical address
    virtual T* findBlock(Addr addr) const = 0;    
    
    // Find the victim block to be replaced
    virtual std::tuple<bool, Addr, T*> findVictim(Addr addr) = 0;
    
    // 
    virtual void invalidate(T* victim) {}
};

class TagsWithFABlk : public Tags<FABlk>
{
  public:
    TagsWithFABlk(int level, Config &cfg) :
        Tags(level, cfg) {}

    virtual unsigned numOccupiedBlocks() = 0; // Only make sense for FA

   protected:
    FABlk *head;
    FABlk *tail;

  protected:
    std::unique_ptr<FAReplacementPolicy> policy;
};
}
#endif
