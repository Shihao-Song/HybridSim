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
   
    Trace mem_trace(argv[2]);
    bool end = false;
    Request req;

    uint64_t clk = 0;
    while (!end)
    {
        if (!end)
        {
            end = !(mem_trace.getMemtraceRequest(req));
        }

        if (auto [hit, aligned_addr] = tags.accessBlock(req.addr, clk);
            !hit)
        {
            std::cout << aligned_addr << "; ";
            if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr, clk);
                !wb_required)
            {
                std::cout << "No write-back is required. \n";
            }
            else
            {
                std::cout << "Write-back address: " << wb_addr << "\n";
            }
        }
        clk++;
    }

    /*
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

    if (auto [hit, aligned_addr] = tags.accessBlock(1125899957984640);
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
    */
    exit(0);
    /* Simulation */
    // runMemTrace(PCM.get(), argv[2]);
}
