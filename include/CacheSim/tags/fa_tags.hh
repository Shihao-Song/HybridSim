#ifndef __CACHE_FA_TAGS_HH__
#define __CACHE_FA_TAGS_HH__

#include <assert.h>
#include <unordered_map>

#include "Sim/config.hh"
#include "CacheSim/tags/cache_tags.hh"
#include "CacheSim/tags/replacement_policies/fa_lru.hh"

namespace CacheSimulator
{
class TagsWithFABlk : public Tags<FABlk>
{
  public:
    TagsWithFABlk(int level, Config &cfg) :
        Tags(level, cfg) {}
};
template<typename P>
class FATags : public TagsWithFABlk
{
  protected:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    typedef Simulator::Config Config;

  protected:
    FABlk *head;
    FABlk *tail;

    std::unique_ptr<P> policy;

  protected:
    // To make block indexing faster, a hash based address mapping is used
    typedef std::unordered_map<Addr, FABlk *> TagHash;
    TagHash tagHash;

  public:
    FATags(int level, Config &cfg)
        : TagsWithFABlk(level, cfg),
          policy(new P())
    {
        tagsInit();
    }
    
    std::pair<bool, Addr> accessBlock(Addr addr, bool modify, Tick cur_clk = 0) override
    {
        bool hit = false;
        Addr blk_aligned_addr = blkAlign(addr);

	FABlk *blk = findBlock(blk_aligned_addr);
       
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
        auto [wb_required, victim_addr, victim] = findVictim(addr);

        if (modify) { victim->setDirty(); }	
	victim->insert(extractTag(addr));
        policy->upgrade(victim, cur_clk);
        tagHash[victim->tag] = victim;

        return std::make_pair(wb_required, victim_addr);
    }

    void recordMMUCommu(Addr block_addr,
                        Addr eip,
                        std::function<void(Simulator::Request&)> mmu_cb) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        FABlk *blk = findBlock(blk_aligned_addr);
        assert(blk);
        blk->clearMMUCommu();
        blk->recordMMUCommu(eip, mmu_cb);
    }
    
    std::pair<Addr, std::function<void(Simulator::Request&)>> 
        retriMMUCommu(Addr block_addr) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        FABlk *blk = findBlock(blk_aligned_addr);
        assert(blk);

        return blk->retriMMUCommu();
    }

    void clearMMUCommu(Addr block_addr) override
    {
        Addr blk_aligned_addr = blkAlign(block_addr);

        FABlk *blk = findBlock(blk_aligned_addr);
        assert(blk);

        blk->clearMMUCommu();
    }

    void printTagInfo() override
    {
        std::cout << "Number of blocks: " << num_blocks << "\n";
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
        tagHash.clear();
        tagsInit();
    }

  protected:
    void tagsInit() override
    {
        head = &(blks[0]);
        head->prev = nullptr;
        head->next = &(blks[1]);

        for (unsigned i = 1; i < num_blocks - 1; i++)
        {
            blks[i].prev = &(blks[i-1]);
            blks[i].next = &(blks[i+1]);
        }

        tail = &(blks[num_blocks - 1]);
        tail->prev = &(blks[num_blocks - 2]);
        tail->next = nullptr;

        policy->blks = blks.get();
        policy->head = &head;
        policy->tail = &tail;
    }
    
    Addr extractTag(Addr addr) const override
    {
        return blkAlign(addr);
    }

    Addr regenerateAddr(FABlk *blk) const override
    {
        return blk->tag;
    }

    FABlk* findBlock(Addr addr) const override
    {
        FABlk *blk = nullptr;

        Addr tag = extractTag(addr);

	if (auto iter = tagHash.find(tag);
            iter != tagHash.end())
        {
            blk = (*iter).second;

            assert(blk != nullptr);
            assert(blk->isValid());
            assert(blk->tag == tag);
        }

        return blk;
    }

    std::tuple<bool, Addr, FABlk*> findVictim(Addr addr) override
    {
        auto [wb_required, victim] = policy->findVictim(addr);
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

    void invalidate(FABlk* victim) override
    {
        assert(tagHash.erase(victim->tag));
        victim->invalidate();
        victim->clearDirty();
        policy->downgrade(victim);
    }
};

typedef FATags<FALRU> LRUFATags;
}
#endif
