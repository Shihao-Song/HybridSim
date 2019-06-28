#ifndef __CACHE_SET_ASSOC_TAGS_HH__
#define __CACHE_SET_ASSOC_TAGS_HH__

#include <assert.h>

#include "CacheSim/tags/cache_tags.hh"
#include "CacheSim/tags/replacement_policies/set_way_lru.hh"
#include "Sim/config.hh"

namespace CacheSimulator
{
<template P>
class SetAssocTags : public Tags<SetWayBlk>
{
  protected:
    typedef uint64_t Addr;
    typedef uint64_t Tick

    typedef Simulator::Config Config;

  protected:
    const int assoc;

    const uint32_t num_sets;

    const int set_shift;

    const unsigned set_mask;

    const int tag_shift;

    std::vector<std::vector<SetWayBlk *>> sets;

    std::unique_ptr<P> policy;

  public:
    SetAssocTags(int level, Config &cfg)
        : TagsWithSetWayBlk(level, cfg),
          assoc(cfg.caches[level].assoc),
          num_sets(size / (block_size * assoc)),
          set_shift(log2(block_size)),
          set_mask(num_sets - 1),
          sets(num_sets),
          tag_shift(set_shift + log2(num_sets))
    {
        for (uint32_t i = 0; i < numSets; i++)
        {
            sets[i].resize(assoc);
        }
        blks = new SetWayBlk[numBlocks];
        policy = new SetWayAssocLRU();

        tagsInit();

        std::cout << "Size of cache: " << size / 1024 / 1024 << "MB. \n";
        std::cout << "Number of blocks: " << numBlocks << "\n";
        std::cout << "Num of sets: " << sets.size() << "\n";
        std::cout << "Set shift: " << setShift << "\n";
        std::cout << "Set mask: " << setMask << "\n";
        std::cout << "Tag shift: " << tagShift << "\n\n";
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
        return blk;
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

    void insertBlock(Addr addr, SetWayBlk* victim, Tick cur_clk = 0) override
    {
        assert(!victim->isValid());

        victim->insert(extractTag(addr));

        policy->upgrade(victim, cur_clk);
    }

    Addr regenerateAddr(SetWayBlk *blk) const override
    {
        return (blk->tag << tagShift) | (blk->getSet() << setShift);
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
};
}

#endif
