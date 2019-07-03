#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include <cstring>

#include "Sim/config.hh"
#include "Sim/trace.hh"

#include "CacheSim/cache.hh"
#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"

typedef Simulator::Config Config;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

enum class Memories : int
{
    L1_I_CACHE,
    L1_D_CACHE,
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
    else
    {
        if (mem_type == Memories::L1_I_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1I, cfg);
        }
        else if (mem_type == Memories::L1_D_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1D, cfg);
        }
        else if (mem_type == Memories::L2_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L2, cfg);
        }
        else if (mem_type == Memories::L3_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L3, cfg);
        }
        else if (mem_type == Memories::eDRAM)
        {
            return CacheSimulator::createCache(Config::Cache_Level::eDRAM, cfg);
        }
    }
}

// Run simulation
auto runMemTrace(MemObject *mem_obj, const char *trace_name)
{
    Trace mem_trace(trace_name);

    Request req;

    std::cout << "\nMemory-trace driven simulation...\n";
    uint64_t Tick = 0;
    bool stall = false;
    bool end = false;

    while (!end || mem_obj->pendingRequests())
    {
        if (!end && !stall)
        {
            static uint64_t num_reqs = 0;
            end = !(mem_trace.getMemtraceRequest(req));
            std::cout << ++num_reqs << "\n";
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

void parse_args(int argc, const char *argv[],
                std::vector<const char*> &trace_lists)
{
    int trace_start = -1;
    int config_start = -1;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--traces") == 0)
        {
            trace_start = i + 1;
        }

        if (strcmp(argv[i], "--config") == 0)
        {
            config_start = i + 1;
        }
    }
    assert(config_start != -1);
    assert(config_start < argc);

    assert(trace_start != -1);
    assert(trace_start < argc);

    int num_traces;
    if (config_start < trace_start)
    {
        num_traces = argc - trace_start;
    }
    else
    {
        num_traces = config_start - 1 - trace_start;
    }

    for (int i = 0; i < num_traces; i++)
    {
        trace_lists.push_back(argv[trace_start + i]);
    }
}

auto runCPUTrace(int argc, const char *argv[])
{
    std::vector<const char*> trace_lists;
    parse_args(argc, argv, trace_lists);
    assert(trace_lists.size() != 0);
    for (int i = 0; i < trace_lists.size(); i++)
    {
        std::cout << "Core " << i
                  << " is running trace: " << trace_lists[i] << "\n";
    }
}
#endif
