#ifndef __CACHE_SET_ASSOC_TAGS_HH__
#define __CACHE_SET_ASSOC_TAGS_HH__

#include <assert.h>

#include "cache_tags.hh"
#include "../replacement_policies/set_way_lru.hh"
#include "../../../Configs/config.hh"

namespace CacheSimulator
{
class SetAssocTags : public TagsWithSetWayBlk
{
    typedef uint64_t Addr;
    typedef Configuration::Config Config;

  private:
    const int assoc;

    const uint32_t numSets;

    const int setShift;

    const unsigned setMask;

    const int tagShift;

    std::vector<std::vector<SetWayBlk *>> sets;

  public:
    SetAssocTags(int level, Config &cfg)
        : TagsWithSetWayBlk(level, cfg),
          assoc(cfg.caches[level].assoc),
          numSets(size / (blkSize * assoc)),
          setShift(log2(blkSize)),
          setMask(numSets - 1),
          sets(numSets),
          tagShift(setShift + log2(numSets))
    {
        for (uint32_t i = 0; i < numSets; i++)
        {
            sets[i].resize(assoc);
        }
        blks = new SetWayBlk[numBlocks];
        policy = new SetWayAssocLRU();

        tagsInit();
    }

    void tagsInit() override
    {
        for (unsigned i = 0; i < numBlocks; i++)
        {
            SetWayBlk *blk = &blks[i];
            uint32_t set = i / assoc;
            uint32_t way = i % assoc;

            sets[set][way] = blk;
            blk->setPosition(set, way);
        }
    }

    Addr extractSet(Addr addr) const
    {
        return (addr >> setShift) & setMask;
    }

    Addr extractTag(Addr addr) const override
    {
        return (addr >> tagShift);
    }

    SetWayBlk *accessBlock(Addr addr, Tick cur_clk = 0) override
    {
        SetWayBlk *blk = findBlock(addr);

        // If there is hit, upgrade
        if (blk != nullptr)
        {
            policy->upgrade(blk, cur_clk);
        }
    }

    SetWayBlk* findVictim(Addr addr) override
    {
        // Extract the set
        const std::vector<SetWayBlk *> set = sets[extractSet(addr)];
        SetWayBlk *victim = policy->findVictim(set);
        assert(victim != nullptr);

        return victim;
    }

    void invalidate(SetWayBlk* victim) override
    {
        victim->invalidate();
        policy->downgrade(victim);
        assert(!victim->isValid());
    }

    void insertBlock(Addr addr, SetWayBlk* victim, Tick cur_clk)
    {
        assert(!victim->isValid());

        victim->insert(extractTag(addr));

        policy->upgrade(victim, cur_clk);
    }

  protected:
    SetWayBlk* findBlock(Addr addr) const override
    {
        // Extract block tag
        Addr tag = extractTag(addr);

        // Extract the set
        const std::vector<SetWayBlk *> set = sets[extractSet(addr)];

        for (const auto& way : set)
        {
            if (way->tag == tag && way->isValid())
            {
                return way;
            }
        }

        return nullptr;
    }

    Addr regenerateAddr(SetWayBlk *blk) const override
    {
        return (blk->tag << tagShift) | (blk->getSet() << setShift);
    }
};
}

#endif
