#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include <cstring>

#include "Sim/config.hh"
#include "Sim/trace.hh"

#include "CacheSim/cache.hh"
#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "Processor/processor.hh"

typedef Simulator::Config Config;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

typedef CoreSystem::Processor Processor;

enum class Memories : int
{
    L1_I_CACHE,
    L1_D_CACHE,
    L2_CACHE,
    L3_CACHE,
    eDRAM,
    PCM
};

std::pair<const char*, std::vector<const char*>> parse_args(int argc, const char *argv[]);
auto createMemObject(Config &cfg, Memories mem_type, int core_id = -1)
{
    if (mem_type == Memories::PCM)
    {
        return PCMSim::createPCMSimMemorySystem(cfg);
    }
    else
    {
        if (mem_type == Memories::L1_I_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1I, cfg, core_id);
        }
        else if (mem_type == Memories::L1_D_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1D, cfg, core_id);
        }
        else if (mem_type == Memories::L2_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L2, cfg, core_id);
        }
        else if (mem_type == Memories::L3_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L3, cfg, core_id);
        }
        else if (mem_type == Memories::eDRAM)
        {
            return CacheSimulator::createCache(Config::Cache_Level::eDRAM, cfg, core_id);
        }
    }
}

auto runCPUTrace(Processor *processor)
{
    std::cout << "\nCPU-trace driven simulation...\n\n";
    while (!processor->done())
    {
        processor->tick();
    }
}

std::pair<const char*, std::vector<const char*>> parse_args(int argc, const char *argv[])
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

    std::vector<const char*> trace_lists;
    for (int i = 0; i < num_traces; i++)
    {
        trace_lists.push_back(argv[trace_start + i]);
    }
    return std::make_pair(argv[config_start], trace_lists);
}

// Function to test cache behavior.
// Please also un-comment std::cout sections in corresponding tag class.
auto runCacheTest(const char* cfg_file, const char *trace_name)
{
    Config cfg(cfg_file);
    Trace cpu_trace(trace_name);
    
    Simulator::Instruction instr;

    // To test Set-Assoc tag with LRU replacement policy.
    CacheSimulator::LRUSetWayAssocTags tags(int(Config::Cache_Level::L1D), cfg);
    tags.printTagInfo();

    std::cout << "\nCache (tag) stressing mode...\n";

    uint64_t cycles = 0;

    uint64_t num_evictions = 0;
    uint64_t num_hits = 0;
    uint64_t num_misses = 0;

    Simulator::Mapper mapper(0);

    bool more_insts = cpu_trace.getInstruction(instr);
    while (more_insts)
    {
        /*
        static uint64_t counter = 0;
        std::cout << ++counter << ": ";
        std::cout << instr.eip << " ";
        if (instr.opr == Simulator::Instruction::Operation::EXE)
        {
            std::cout << "EXE \n";
        }
        else
        {
            if (instr.opr == Simulator::Instruction::Operation::LOAD)
            {
                std::cout << "LOAD ";
            }

            if (instr.opr == Simulator::Instruction::Operation::STORE)
            {
                std::cout << "STORE ";
            }
            std::cout << instr.target_addr << " ";
            std::cout << instr.size << "\n";
        }
        std::cout << "\n";
        */
        if (instr.opr == Simulator::Instruction::Operation::LOAD ||
            instr.opr == Simulator::Instruction::Operation::STORE)
        {
            /*
            if (instr.opr == Simulator::Instruction::Operation::LOAD)
            {
                std::cout << "LOAD ";
            }
            else
            {
                std::cout << "STORE ";
            }
            */

            uint64_t addr = mapper.va2pa(instr.target_addr);
            if (auto [hit, aligned_addr] = tags.accessBlock(addr,
                                       instr.opr == Simulator::Instruction::Operation::STORE ?
                                       true : false,
                                       cycles);
                !hit)
            {
                ++num_misses;
                // std::cout << "Missed; ";
                if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr,
                                       instr.opr == Simulator::Instruction::Operation::STORE ?
                                       true : false,
                                       cycles);
                    wb_required)
		{
                    ++num_evictions;
                //    std::cout << "Inserted; WB: " << wb_addr << "\n";
                }
                // else
                // {
                //     std::cout << "Inserted.\n";
                // }
            }
            else
            {
                ++num_hits;
                // std::cout << "hit\n";
            }
        }
        more_insts = cpu_trace.getInstruction(instr);
        ++cycles;
    }
    double hit_rate = (double)num_hits / ((double)num_misses + (double)num_hits);
    std::cout << "Hit rate: " << hit_rate << "\n";
    std::cout << "Number of evictions: " << num_evictions << "\n";
}

// TODO, provide an example to this feature.
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

#endif
