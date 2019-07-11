#ifndef __CACHE_FA_LRU_HH__
#define __CACHE_FA_LRU_HH__

#include "CacheSim/tags/replacement_policies/replacement_policy.hh"

namespace CacheSimulator
{
class FALRU : public FAReplacementPolicy
{
  public:
    FALRU() : FAReplacementPolicy() {}

    void upgrade(FABlk *blk, Tick cur_clk = 0) override
    {
        // If block is not already head, do the moving
        if (blk != *head)
        {
            // If block is tail, set previous block as new tail
            if (blk == *tail)
            {
                assert(blk->next == nullptr);
                (*tail) = blk->prev;
                (*tail)->next = nullptr;
                // Inform block's surrounding blocks that it has been moved
            }
            else
            {
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

    void downgrade(FABlk *blk) override
    {
        // If block is not already tail, do the moving
        if (blk != *tail)
        {
            // If block is head, set next block as new head
            if (blk == *head)
            {
                assert(blk->prev == nullptr);
                (*head) = blk->next;
                (*head)->prev = nullptr;
                // Inform block's surrounding blocks that it has been moved
            }
            else
            {
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

    std::pair<bool, FABlk*> findVictim(Addr addr)
    {
        FABlk *victim = *tail;

        bool send_back_required = false;
        if (victim->isValid() && victim->isDirty())
        {
            send_back_required = true;
        }

        return std::make_pair(send_back_required, victim);
    }
};
}
#endif
