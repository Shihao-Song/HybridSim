#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include <boost/program_options.hpp>
#include <cstring>

#include "Sim/config.hh"
#include "Sim/stats.hh"
#include "Sim/trace.hh"

#include "System/mmu.hh"

#include "CacheSim/cache.hh"
#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "Processor/processor.hh"

#define isLLC 1
#define isNonLLC 0

typedef Simulator::Config Config;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Stats Stats;

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

struct ParseArgsRet
{
    const char* cfg_file;
    std::vector<const char*> trace_lists;
    std::vector<uint64_t> warmups;
    const char* output_file;
};
ParseArgsRet parse_args(int argc, const char *argv[]);

auto createMemObject(Config &cfg,
                     Memories mem_type,
                     bool LLC = false)
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
            return CacheSimulator::createCache(Config::Cache_Level::L2, cfg, LLC);
        }
        else if (mem_type == Memories::L3_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L3, cfg, LLC);
        }
        else if (mem_type == Memories::eDRAM)
        {
            return CacheSimulator::createCache(Config::Cache_Level::eDRAM, cfg, LLC);
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

ParseArgsRet parse_args(int argc, const char *argv[])
{
    namespace po = boost::program_options;
    po::options_description desc("Options"); 
    desc.add_options() 
        ("help", "Print help messages")
        ("config", "Configuration file")
        ("traces", "CPU traces")
        ("warmups", "Number of warmup instructions")
        ("output", "(Stats) Output file");
 
    po::variables_map vm;

    try 
    { 
        po::store(po::parse_command_line(argc, argv, desc), vm); // can throw 
 
        if (vm.count("help")) 
        { 
            std::cout << "A CPU-trace driven PCM Simulator.\n" 
                      << desc << "\n"; 
            exit(0);
        } 

        	
    } 
    catch(po::error& e) 
    { 
        std::cerr << "ERROR: " << e.what() << "\n\n"; 
        std::cerr << desc << "\n"; 
        exit(0);
    } 

    /*
    int trace_start = -1;
    int warmups_start = -1;
    int config_start = -1;
    int output_start = -1;

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

        if (strcmp(argv[i], "--output") == 0)
        {
            output_start = i + 1;
        }

        if (strcmp(argv[i], "--warmups") == 0)
        {
            warmups_start = i + 1;
        }
    }

    if (trace_start == -1 || config_start == -1 || output_start == -1)
    {
        std::cerr << argv[0] << " --config YOUR_CONFIG_FILE"
                             << " --traces YOUR_TRACE_FILES"
                             << " --warmups WARMUPS_FOR_EACH_TRACE"
                             << " --output YOUR_OUTPUT_FILE\n";
        exit(0);
    }

    int trace_end_deter = mode_start < config_start ? config_start : mode_start;
    trace_end_deter = trace_end_deter < output_start ? output_start : trace_end_deter;

    int num_traces;
    if (trace_end_deter < trace_start)
    {
        num_traces = argc - trace_start;
    }
    else
    {
        num_traces = trace_end_deter - 1 - trace_start;
    }

    std::vector<const char*> trace_lists;
    for (int i = 0; i < num_traces; i++)
    {
        trace_lists.push_back(argv[trace_start + i]);
    }
    return {argv[mode_start], argv[config_start], trace_lists, argv[output_start]};
    */
}

// Function to test cache behavior.
// Please also un-comment std::cout sections in corresponding tag class.
auto runCacheTest(const char* cfg_file, const char *trace_name)
{
    Config cfg(cfg_file);
    Simulator::ProtobufTrace cpu_trace(trace_name);
    
    Simulator::Instruction instr;

    // To test Set-Assoc tag with LRU replacement policy.
    CacheSimulator::LRUSetWayAssocTags tags(int(Config::Cache_Level::L1D), cfg);
    tags.printTagInfo();

    std::cout << "\nCache (tag) stressing mode...\n";

    uint64_t cycles = 0;

    uint64_t num_evictions = 0;
    uint64_t num_hits = 0;
    uint64_t num_misses = 0;

    bool more_insts = cpu_trace.getInstruction(instr);
    while (more_insts)
    {
        if (instr.opr == Simulator::Instruction::Operation::LOAD ||
            instr.opr == Simulator::Instruction::Operation::STORE)
        {
            
            uint64_t addr = instr.target_vaddr;
            if (auto [hit, aligned_addr] = tags.accessBlock(addr,
                                       instr.opr == Simulator::Instruction::Operation::STORE ?
                                       true : false,
                                       cycles);
                !hit)
            {
                ++num_misses;
                if (auto [wb_required, wb_addr] = tags.insertBlock(aligned_addr,
                                       instr.opr == Simulator::Instruction::Operation::STORE ?
                                       true : false,
                                       cycles);
                    wb_required)
		{
                    ++num_evictions;
                }
            }
            else
            {
                ++num_hits;
            }
        }
        more_insts = cpu_trace.getInstruction(instr);
        ++cycles;
    }
    double hit_rate = (double)num_hits / ((double)num_misses + (double)num_hits);
    std::cout << "Hit rate: " << hit_rate << "\n";
    std::cout << "Number of evictions: " << num_evictions << "\n";
}

auto runMemTrace(MemObject *mem_obj,
                 const char *trace_name,
                 System::TrainedMMU *mmu)
{
    Simulator::TXTTrace mem_trace(trace_name);

    Request req;

    std::cout << "\nMemory-trace driven simulation...\n";
    uint64_t Tick = 0;
    bool stall = false;
    bool end = false;

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

    return Tick;
}

#endif
