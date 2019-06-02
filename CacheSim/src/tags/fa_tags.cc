#include "fa_tags.hh"
#include "../replacement_policies/fa_lru.hh"

namespace CacheSimulator
{
FATags::FATags(int level, Config &cfg) : TagsWithFABlk(level, cfg)
{
    blks = new FABlk[numBlocks];
    policy = new FALRU();

    tagsInit();
}

void FATags::tagsInit()
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

FABlk* FATags::accessBlock(Addr addr, Tick cur_clk)
{
    FABlk *blk = findBlock(addr);

    if (blk && blk->isValid())
    {
        policy->upgrade(blk);
    }

    return blk;
}

FABlk* FATags::findVictim(Addr addr)
{
    FABlk* victim = policy->findVictim(addr);

    return victim;
}

void FATags::invalidate(FABlk* victim)
{
    assert(tagHash.erase(victim->tag));

    victim->invalidate();

    policy->downgrade(victim);
}

void FATags::insertBlock(Addr addr, FABlk* victim, Tick cur_clk)
{
    assert(!victim->isValid());

    victim->insert(extractTag(addr));

    policy->upgrade(victim);

    tagHash[victim->tag] = victim;
}

FABlk* FATags::findBlock(Addr addr) const
{
    FABlk *blk = nullptr;

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

