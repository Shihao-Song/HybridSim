#include "../PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "../PCMSim/Trace/pcm_sim_trace.hh"

#include "eDRAM_Cache_Side/eDRAM_cache.hh"

#include <iostream>

namespace PCMSim
{
    class PCMSimMemorySystem;
    class Request;
    class Trace;
}

namespace eDRAMSimulator
{
    class eDRAMCache;
}

typedef PCMSim::PCMSimMemorySystem PCMSimMemorySystem;
typedef PCMSim::Request Request;
typedef PCMSim::Trace Trace;

typedef eDRAMSimulator::eDRAMCache eDRAMCache;

void runMemtraces(eDRAMCache &eDRAM, const char* tracename);

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

    PCMSimMemorySystem mem_system(argv[1]);
    eDRAMCache eDRAM(&mem_system);
    runMemtraces(eDRAM, argv[2]);
    eDRAM.printStats();
}

void runMemtraces(eDRAMCache &eDRAM, const char* tracename)
{
    std::cout << "Running trace: " << tracename << "\n\n";

    // Initialize memory trace
    Trace trace(tracename);

    // Run simulation
    bool stall = false;
    bool end = false; // Indicate end of the trace file

    // Initialize Request
    Addr addr = 0;
    Request::Request_Type req_type = PCMSim::Request::Request_Type::READ;

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
