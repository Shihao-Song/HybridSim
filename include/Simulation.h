#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include "Sim/config.hh"
#include "Sim/trace.hh"
#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"

typedef Simulator::Config Config;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

enum class Memories : int
{
    L1_CACHE,
    L2_CACHE,
    L3_CACHE,
    eDRAM,
    PCM
};

auto createMemObject(Config &cfg, Memories mem_type)
{
    if (mem_type == Memories::PCM)
    {
        return PCMSim::createPCMSimMemorySystem(cfg);
    }
}

// Run simulation
auto runMemTrace(MemObject *mem_obj, const char *trace_name)
{
    Trace mem_trace(trace_name);

    Request req;

    std::cout << "\nMemory-trace driven simulation...\n";
    uint64_t Tick = 0;
    bool stall, end = false;

    while (!end || mem_obj->pendingRequests())
    {
        if (!end && !stall)
        {
            end = !(mem_trace.getMemtraceRequest(req));
        }

        if (!end)
        {
            stall = !(mem_obj->send(req));
        }

        mem_obj->tick();
        ++Tick;
    }

    std::cout << "\nEnd Execution Time: " << Tick << "\n";
}

#endif
