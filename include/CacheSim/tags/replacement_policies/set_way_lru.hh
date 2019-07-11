#ifndef __CACHE_SET_WAY_ASSOC_LRU_HH__
#define __CACHE_SET_WAY_ASSOC_LRU_HH__

#include "CacheSim/tags/replacement_policies/replacement_policy.hh"

namespace CacheSimulator
{
class SetWayAssocLRU : public SetWayAssocReplacementPolicy
{
  public:
    SetWayAssocLRU() : SetWayAssocReplacementPolicy() {}

    void upgrade(SetWayBlk *blk, Tick cur_clk = 0) override
    {
        blk->when_touched = cur_clk;
    }

    void downgrade(SetWayBlk *victim) override
    {
        victim->when_touched = 0;
    }

    std::pair<bool, SetWayBlk*> findVictim(const std::vector<SetWayBlk *> &set) override
    {
        assert(set.size() > 0);

        // Step one, is there an invalid block
        SetWayBlk *victim = nullptr;
        for (const auto way : set)
        {
            if (!way->isValid())
            {
                victim = way;
                assert(!victim->isValid());
                break;
            }
        }

        bool send_back_required = false;
        if (victim != nullptr) { return std::make_pair(send_back_required, victim); }

        // All the ways are valid
        victim = set[0];
        for (const auto way : set)
        {
            assert(way->isValid());
            if (way->when_touched < victim->when_touched)
            {
                victim = way;
            }
        }
        send_back_required = victim->isDirty() ? true : false;
        return std::make_pair(send_back_required, victim);
    }
};
}

#endif
