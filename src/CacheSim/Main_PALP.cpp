#include "../Sim/config.hh"
#include "../PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "../PCMSim/Trace/pcm_sim_trace.hh"

#include "src/cache.hh"
#include "src/tags/fa_tags.hh"
#include "src/tags/set_assoc_tags.hh"

#include <iostream>

typedef PCMSim::PCMSimMemorySystem PCMSimMemorySystem;
typedef PCMSim::Request Request;
typedef PCMSim::Trace Trace;

typedef Configuration::Config Config;

void runMemtraces(const char* cfg_file, const char* tracename);

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

    runMemtraces(argv[1], argv[2]);    
}

void runMemtraces(const char* cfg_file, const char* tracename)
{
    Config cfg(cfg_file);
    // Set up the workload name
    std::string tmp(tracename);
    tmp = tmp.substr(0, tmp.find("."));
    tmp = tmp.substr(tmp.find_last_of("/\\") + 1);
    cfg.workload = tmp;
    
    // PCM memory system
    PCMSimMemorySystem mem_system(cfg);

    // eDRAM system
    CacheSimulator::Cache<CacheSimulator::FABlk,
        CacheSimulator::FATags> eDRAM(Config::Cache_Level::eDRAM, cfg);
    CacheSimulator::FATags tags(int(Config::Cache_Level::eDRAM), cfg);

    eDRAM.setTags(&tags);
    eDRAM.setNextLevel(&mem_system);

    // Simulation
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

    mem_system.printStats();    
    eDRAM.printStats();
}
