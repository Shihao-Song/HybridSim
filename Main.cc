#include <iostream>

#include "Simulation.h"

#include "CacheSim/cache_queue.hh"
#include "CacheSim/tags/set_assoc_tags.hh"

int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <configs-file>"
                  << " <trace-file>"
                  << "\n";
        return 0;
    }

    /* Object Creation */
    Config cfg(argv[1]);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    CacheSimulator::LRUSetWayAssocTags tags(int (Config::Cache_Level::L3), cfg);
    
    if (auto [hit, aligned_addr] = tags.accessBlock(562950015231488);
        !hit)
    {
        if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr);
            !wb_required)
        {
            std::cout << "No write-back is required. \n";
        }
    }
    
    if (auto [hit, aligned_addr] = tags.accessBlock(844425010634752);
        !hit)
    {
        if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr);
            !wb_required)
        {
            std::cout << "No write-back is required. \n";
        }
    }

    if (auto [hit, aligned_addr] = tags.accessBlock(281475055142528);
        !hit)
    {
        if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr);
            !wb_required)
        {
            std::cout << "No write-back is required. \n";
        }
        else
        {
            std::cout << "Write-back address: " << wb_addr << "\n";
        }
    }

/*
    if (auto [wb_required, wb_addr, blk] = tags.findVictim(562950015231488);
        wb_required)
    {
        std::cout << "Write-back address: " << wb_addr << "\n";
    }
    else
    {
        tags.insertBlock(562950015231488, blk, 2);
        std::cout << "Set: " << blk->set << "\n";
        std::cout << "Way: " << blk->way << "\n";
        std::cout << "Tag: " << blk->tag << "\n\n";
    }
    
    if (auto [wb_required, wb_addr, blk] = tags.findVictim(844425010634752);
        wb_required)
    {
        std::cout << "Write-back address: " << wb_addr << "\n";
    }
    else
    {
        tags.insertBlock(844425010634752, blk, 4);
        std::cout << "Set: " << blk->set << "\n";
        std::cout << "Way: " << blk->way << "\n";
        std::cout << "Tag: " << blk->tag << "\n\n";
    }

    if (auto [wb_required, wb_addr, blk] = tags.findVictim(281475055142528);
        wb_required)
    {
        std::cout << "Write-back address: " << wb_addr << "\n";

	tags.insertBlock(281475055142528, blk, 8);
        std::cout << "Set: " << blk->set << "\n";
        std::cout << "Way: " << blk->way << "\n";
        std::cout << "Tag: " << blk->tag << "\n\n";
    }
*/

/*
    [wb, blk] = tags.findVictim(844425010634752);
    tags.insertBlock(844425010634752, blk, 4);
    std::cout << "Set: " << blk->set << "\n";
    std::cout << "Way: " << blk->way << "\n";
    std::cout << "Tag: " << blk->tag << "\n\n";

    [wb, blk] = tags.findVictim(281475055142528);
    invalidate();
    tags.insertBlock(281475055142528, blk, 6);
    std::cout << "Set: " << blk->set << "\n";
    std::cout << "Way: " << blk->way << "\n";
    std::cout << "Tag: " << blk->tag << "\n\n";
*/
    /*

    auto [hit, queueBlk] = tags.accessBlock(844425010634752, 10);
    if (hit)
    {
        std::cout << "Set: " << queueBlk->set << "\n";
        std::cout << "Way: " << queueBlk->way << "\n";
        std::cout << "Tag: " << queueBlk->tag << "\n";
    }
    */

    exit(0);
    /* Simulation */
    runMemTrace(PCM.get(), argv[2]);
}
