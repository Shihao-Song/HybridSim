#ifndef __eDRAM_CACHE_FA_BLK_HH__
#define __eDRAM_CACHE_FA_BLK_HH__

#include <cassert>
#include <cstdint>

namespace eDRAMSimulator
{
typedef uint64_t Addr;
typedef uint64_t Tick;

class eDRAMCacheFABlk
{
  public:
    // Initially, all blks should not be valid
    eDRAMCacheFABlk() : prev(nullptr), next(nullptr), valid(0) {}

    void insert(const Addr _tag)
    {
        assert(valid == 0); // Should never insert to a valid block
        this->tag = _tag;
        valid = 1;
    }
    
    void invalidate()
    {
        assert(valid == 1);
        valid = 0;
    }

    bool isValid()
    {
        return valid;
    }
    /*
     * prev and next are determined by the replacement policy. For example,
     * when LRU is used, prev means the previous block in LRU order.
     *
     * prev (recently used), block, next (least recently used)
     * */
    eDRAMCacheFABlk *prev;

    eDRAMCacheFABlk *next;

    Addr tag;

    bool valid;
};
}
#endif
