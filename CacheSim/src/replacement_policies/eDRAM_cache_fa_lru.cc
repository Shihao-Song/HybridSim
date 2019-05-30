#include "eDRAM_cache_fa_lru.hh"

namespace eDRAMSimulator
{
eDRAMCacheFALRU::eDRAMCacheFALRU()
    : eDRAMCacheFAReplacementPolicy()
{
}

// LRU policy - move the MRU block to the head
void eDRAMCacheFALRU::upgrade(eDRAMCacheFABlk *blk)
{
    // If block is not already head, do the moving
    if (blk != *head) {
        // If block is tail, set previous block as new tail
        if (blk == *tail){
            assert(blk->next == nullptr);
            (*tail) = blk->prev;
            (*tail)->next = nullptr;
        // Inform block's surrounding blocks that it has been moved
        } else {
            blk->prev->next = blk->next;
            blk->next->prev = blk->prev;
        }
        // Swap pointers
        blk->next = *head;
        blk->prev = nullptr;
        (*head)->prev = blk;
        *head = blk;
    }

    assert(blk == *head);
}

void eDRAMCacheFALRU::downgrade(eDRAMCacheFABlk *blk)
{
    // If block is not already tail, do the moving
    if (blk != *tail) {
        // If block is head, set next block as new head
        if (blk == *head){
            assert(blk->prev == nullptr);
            (*head) = blk->next;
            (*head)->prev = nullptr;
        // Inform block's surrounding blocks that it has been moved
        } else {
            blk->prev->next = blk->next;
            blk->next->prev = blk->prev;
        }

	// Swap pointers
        blk->prev = *tail;
        blk->next = nullptr;
        (*tail)->next = blk;
        *tail = blk;
    }

    assert(blk == *tail);
}

eDRAMCacheFABlk* eDRAMCacheFALRU::findVictim(Addr addr)
{
    return *tail;
}
}
