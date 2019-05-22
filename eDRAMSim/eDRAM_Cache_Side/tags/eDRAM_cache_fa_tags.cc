#include "eDRAM_cache_fa_tags.hh"
#include "../replacement_policies/eDRAM_cache_fa_lru.hh"

namespace eDRAMSimulator
{
eDRAMCacheFATags::eDRAMCacheFATags()
    : eDRAMCacheTagsWithFABlk()
{
    blks = new eDRAMCacheFABlk[numBlocks];
    policy = new eDRAMCacheFALRU();

    tagsInit();
}

void eDRAMCacheFATags::tagsInit()
{
    head = &(blks[0]);
    head->prev = nullptr;
    head->next = &(blks[1]);
    
    for (unsigned i = 1; i < numBlocks - 1; i++) 
    {
        blks[i].prev = &(blks[i-1]);
        blks[i].next = &(blks[i+1]);
    }

    tail = &(blks[numBlocks - 1]);
    tail->prev = &(blks[numBlocks - 2]);
    tail->next = nullptr;
    
    policy->policyInit(this);
}

eDRAMCacheFABlk* eDRAMCacheFATags::accessBlock(Addr addr)
{
    eDRAMCacheFABlk *blk = findBlock(addr);

    if (blk && blk->isValid())
    {
        policy->upgrade(blk);
    }

    return blk;
}

eDRAMCacheFABlk* eDRAMCacheFATags::findVictim(Addr addr)
{
    eDRAMCacheFABlk* victim = policy->findVictim(addr);

    return victim;
}

void eDRAMCacheFATags::invalidate(eDRAMCacheFABlk* victim)
{
    assert(tagHash.erase(victim->tag));

    victim->invalidate();

    policy->downgrade(victim);
}

void eDRAMCacheFATags::insertBlock(Addr addr, eDRAMCacheFABlk* victim)
{
    assert(!victim->isValid());

    victim->insert(extractTag(addr));

    policy->upgrade(victim);

    tagHash[victim->tag] = victim;
}

eDRAMCacheFABlk* eDRAMCacheFATags::findBlock(Addr addr) const
{
    eDRAMCacheFABlk *blk = nullptr;

    Addr tag = extractTag(addr);
    
    auto iter = tagHash.find(tag);
    if (iter != tagHash.end())
    {
        blk = (*iter).second;
    }

    if (blk && blk->isValid())
    {
        assert(blk->tag == tag);
    }

    return blk;
}
}

