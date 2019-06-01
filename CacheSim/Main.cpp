#include "../Configs/config.hh"
#include "../PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "../PCMSim/Trace/pcm_sim_trace.hh"

#include "src/cache.hh"

#include <iostream>

namespace PCMSim
{
    class PCMSimMemorySystem;
    class Request;
    class Trace;
}

namespace CacheSimulator
{
    class Cache;
}

typedef PCMSim::PCMSimMemorySystem PCMSimMemorySystem;
typedef PCMSim::Request Request;
typedef PCMSim::Trace Trace;

typedef CacheSimulator::Cache Cache;

typedef Configuration::Config Config;

void runMemtraces(Cache &eDRAM, const char* tracename);

int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0]
                  << " <configs-file>"
                  << " <trace-file>"
                  << "\n";
        return 0;
    }

    Config cfg(argv[1]);
    // PCM memory system
    PCMSimMemorySystem mem_system(cfg);

    // eDRAM system
    Cache eDRAM(Config::Cache_Level::eDRAM, cfg);
    eDRAM.setNextLevel(&mem_system);

    runMemtraces(eDRAM, argv[2]);
    eDRAM.printStats();

    /*
    for (int i = 0; i < int(Config::Cache_Level::MAX); i++)
    {
        if (i == int(Config::Cache_Level::L1I))
        {
            std::cout << "L1I: \n";
        }
        if (i == int(Config::Cache_Level::L1D))
        {
            std::cout << "L1D: \n";
        }
        if (i == int(Config::Cache_Level::L2))
        {
            std::cout << "L2: \n";
        }
        if (i == int(Config::Cache_Level::L3))
        {
            std::cout << "L3: \n";
        }
        if (i == int(Config::Cache_Level::eDRAM))
        {
            std::cout << "eDRAM: \n";
        }
        std::cout << "assoc: " << cfg.caches[i].assoc << "\n";
        std::cout << "size: " << cfg.caches[i].size << "\n";
        std::cout << "write only: " << cfg.caches[i].write_only << "\n";
        std::cout << "num mshrs: " << cfg.caches[i].num_mshrs << "\n";
        std::cout << "num wb entries: " << cfg.caches[i].num_wb_entries << "\n";
        std::cout << "lat: " << cfg.caches[i].tag_lookup_latency << "\n\n";
    }
    */
}

void runMemtraces(Cache &eDRAM, const char* tracename)
{
    std::cout << "Running trace: " << tracename << "\n\n";

    // Initialize memory trace
    Trace trace(tracename);

    // Run simulation
    bool stall = false;
    bool end = false; // Indicate end of the trace file

    // Initialize Request
    Addr addr = 0;
    Request::Request_Type req_type = Request::Request_Type::READ;

    Tick clks = 0;
    
    while(!end || eDRAM.numOutstanding())
    {
        if (!end && !stall)
        {
            end = !trace.getMemtraceRequest(addr, req_type);
        }

        if(!end)
        {
            Request req(addr, req_type);

	    req.addr = addr;
            req.req_type = req_type;

            stall = !eDRAM.access(req);
        }

        eDRAM.tick();
        clks++;
    }
}
