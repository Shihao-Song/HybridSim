#ifndef __CACHE_SET_ASSOC_TAGS_HH__
#define __CACHE_SET_ASSOC_TAGS_HH__

#include <assert.h>

#include "Sim/config.hh"

#include "CacheSim/tags/cache_tags.hh"
#include "CacheSim/tags/replacement_policies/set_way_lru.hh"

namespace CacheSimulator
{
class TagsWithSetWayBlk : public Tags<SetWayBlk>
{
  public:
    TagsWithSetWayBlk(int level, Config &cfg) :
        Tags(level, cfg) {}
};

template<typename P>
class SetWayAssocTags : public TagsWithSetWayBlk
{
  protected:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

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
    SetWayAssocTags(int level, Config &cfg)
        : TagsWithSetWayBlk(level, cfg),
          assoc(cfg.caches[level].assoc),
          num_sets(size / (block_size * assoc)),
          set_shift(log2(block_size)),
          set_mask(num_sets - 1),
          sets(num_sets),
          tag_shift(set_shift + log2(num_sets)),
          policy(new P())
    {
        for (uint32_t i = 0; i < num_sets; i++)
        {
            sets[i].resize(assoc);
        }

        tagsInit();
    }

    std::pair<bool, Addr> accessBlock(Addr addr, bool modify, Tick cur_clk = 0) override
    {
        bool hit = false;
        Addr blk_aligned_addr = blkAlign(addr);
        // std::cout << "Aligned address: " << blk_aligned_addr << "; ";

        SetWayBlk *blk = findBlock(blk_aligned_addr);

        // If there is hit, upgrade
        if (blk != nullptr)
        {
            hit = true;
            policy->upgrade(blk, cur_clk);

            if (modify) { blk->setDirty(); }
        }
        return std::make_pair(hit, blk_aligned_addr);
    }

    std::pair<bool, Addr> insertBlock(Addr addr, bool modify, Tick cur_clk = 0) override
    {
        // Find a victim block 
        auto [wb_required, victim_addr, victim] = findVictim(addr); 

        if (modify) { victim->setDirty(); }
        victim->insert(extractTag(addr));
        policy->upgrade(victim, cur_clk);

        return std::make_pair(wb_required, victim_addr);
    }

    void recordMMUCommu(Addr block_addr,
                        Addr eip,
                        std::function<void(Simulator::Request&)> mmu_cb) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        SetWayBlk *blk = findBlock(blk_aligned_addr);
        assert(blk);

        blk->clearMMUCommu();
        blk->recordMMUCommu(eip, mmu_cb);
    }

    std::pair<Addr, std::function<void(Simulator::Request&)>> 
        retriMMUCommu(Addr block_addr) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        SetWayBlk *blk = findBlock(blk_aligned_addr);
        assert(blk);

        return blk->retriMMUCommu();
    }

    void clearMMUCommu(Addr block_addr) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        SetWayBlk *blk = findBlock(blk_aligned_addr);
        assert(blk);

        blk->clearMMUCommu();
    }

    void reInitialize() override
    {
        for (unsigned i = 0; i < num_blocks; i++)
        {
            blks[i].invalidate();
            blks[i].clearDirty();
            blks[i].clearMMUCommu();
            blks[i].when_touched = 0;
        }
        tagsInit();
    }

    void printTagInfo() override
    {
        std::cout << "Assoc: " << assoc << "\n";
        std::cout << "Number of sets: " << num_sets << "\n";
    }

  protected:
    void tagsInit() override
    {
        for (unsigned i = 0; i < num_blocks; i++)
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
        return (addr >> set_shift) & set_mask;
    }

    Addr extractTag(Addr addr) const override
    {
        return (addr >> tag_shift);
    }

    Addr regenerateAddr(SetWayBlk *blk) const override
    {
        return (blk->tag << tag_shift) | (blk->getSet() << set_shift);
    }

    SetWayBlk* findBlock(Addr addr) const override
    {
        // Extract block tag
        Addr tag = extractTag(addr);
        // std::cout << "Set: " << extractSet(addr) << "; ";

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

    std::tuple<bool, Addr, SetWayBlk*> findVictim(Addr addr) override
    {
        // Extract the set
        const std::vector<SetWayBlk *> set = sets[extractSet(addr)];

        // Get the victim block based on replacement policy
        auto [wb_required, victim] = policy->findVictim(set);
        assert(victim != nullptr);

        Addr victim_addr = MaxAddr;

        if (wb_required)
        {
            assert(victim->isDirty());
            victim_addr = regenerateAddr(victim);
        }

        if (victim->isValid())
        {
            invalidate(victim);
        }

        return std::make_tuple(wb_required, victim_addr, victim);
    }

    void invalidate(SetWayBlk* victim) override
    {
        victim->invalidate();
        victim->clearDirty();
        policy->downgrade(victim);
        assert(!victim->isValid());
    }
};

typedef SetWayAssocTags<SetWayAssocLRU> LRUSetWayAssocTags;
}

#endif
