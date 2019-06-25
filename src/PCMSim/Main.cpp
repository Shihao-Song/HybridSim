#include "Memory_System/pcm_sim_memory_system.hh"
#include "Trace/pcm_sim_trace.hh"

#include <iostream>

namespace PCMSim
{
    class PCMSimMemorySystem;
    class Request;
    class Trace;
}

void runMemtraces(PCMSim::PCMSimMemorySystem &mem_sys, const char* tracename);

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

    PCMSim::PCMSimMemorySystem mem_system(argv[1]);
    runMemtraces(mem_system, argv[2]);
}

void runMemtraces(PCMSim::PCMSimMemorySystem &mem_sys, const char* tracename)
{
    std::cout << "Running trace: " << tracename << "\n\n";
    /*
        Initialize memory trace
    */
    PCMSim::Trace trace(tracename);

    /*
        Run simulation
    */
    bool stall = false;
    bool end = false; // Indicate end of the trace file

    /*
        Initialize Request
    */
    Addr addr = 0;
    PCMSim::Request::Request_Type req_type = PCMSim::Request::Request_Type::READ;

    Tick clks = 0;
    
    while(!end || mem_sys.pendingRequests())
    {
        if (!end && !stall)
        {
            end = !trace.getMemtraceRequest(addr, req_type);
        }

        if(!end)
        {
            PCMSim::Request req(addr, req_type);

	    req.addr = addr;
            req.req_type = req_type;

            stall = !mem_sys.send(req);
        }

        mem_sys.tick();
        clks++;
    }
}

