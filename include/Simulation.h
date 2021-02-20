#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include <boost/program_options.hpp>
#include <cstring>

#include "Sim/config.hh"
#include "Sim/stats.hh"
#include "Sim/trace.hh"

#include "System/mmu_factory.hh"

#include "CacheSim/cache.hh"
#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "Processor/processor.hh"
#include "Processor/Prefetcher_Prep/pref_eval.hh"

#include "Sim/trace_probe.hh"
#include "Sim/dummy_mem_object.hh"

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
    std::string mode;
    std::string dram_cfg_file;
    std::string pcm_cfg_file;
    std::string trace_gen_cfg_file;
    std::vector<std::string> trace_lists;
    int64_t num_instrs_per_phase;
    std::string stats_output_file;
    std::string trace_output_file;
};
ParseArgsRet parse_args(int argc, const char *argv[]);

auto createMMU(int num_of_cores, Config &dram_cfg, Config &pcm_cfg)
{
    return System::createMMU(num_of_cores, dram_cfg, pcm_cfg);
}

auto createMMU(int num_of_cores, Config &pcm_cfg)
{
    return System::createMMU(num_of_cores, pcm_cfg);
}

auto createHybridSystem(Config &dram_cfg,
                        Config &pcm_cfg)
{
    return PCMSim::createHybridMemorySystem(dram_cfg, pcm_cfg);
}

auto createMemObject(Config &cfg,
                     Memories mem_type,
                     bool LLC = false)
{
    if (mem_type == Memories::PCM)
    {
        return PCMSim::createPCMMemorySystem(cfg);
    }
    else
    {
        if (mem_type == Memories::L1_I_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1I, cfg, LLC);
        }
        else if (mem_type == Memories::L1_D_CACHE)
        {
            return CacheSimulator::createCache(Config::Cache_Level::L1D, cfg, LLC);
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
    while (!processor->done())
    {
        processor->tick();
    }
}

ParseArgsRet parse_args(int argc, const char *argv[])
{
    std::string mode = "N/A";
    std::string dram_cfg_file = "N/A";
    std::string pcm_cfg_file = "N/A";
    std::string trace_gen_cfg_file = "N/A";
    std::vector<std::string> traces;
    int64_t num_instrs_per_phase = -1;
    std::string stats_output;
    std::string trace_output_file = "N/A";

    namespace po = boost::program_options;
    po::options_description desc("Options"); 
    desc.add_options() 
        ("help", "Print help messages")
        ("mode", po::value<std::string>(&mode),
                 "Mode: dram-only, pcm-only, hybrid, bp-eval, pref-eval, llc-trace-gen, l1-trace-gen")
        ("dram-config", po::value<std::string>(&dram_cfg_file),
                   "Configuration file for DRAM (if hybrid system)")
        ("pcm-config", po::value<std::string>(&pcm_cfg_file),
                   "Configuration file for PCM (if hybrid system)")
        ("trace-gen-config", po::value<std::string>(&trace_gen_cfg_file),
                   "Configuration file for trace generation")
        ("trace", po::value<std::vector<std::string>>(&traces),
                      "CPU trace or MEM trace (when mem-ctrl-design is set)")
        ("num_instrs_per_phase", po::value<int64_t>(&num_instrs_per_phase),
                   "Number of instructions per phase (Optional)")
        ("stat_output", po::value<std::string>(&stats_output),
                        "Stats output file/Stats output directory (bp-eval mode)")
        ("trace_output", po::value<std::string>(&trace_output_file),
                         "Trace output file (Optional)");

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

        po::notify(vm);	
    } 
    catch(po::error& e) 
    { 
        std::cerr << "ERROR: " << e.what() << "\n\n"; 
        std::cerr << desc << "\n"; 
        exit(0);
    }

    return ParseArgsRet{mode,
                        dram_cfg_file,
                        pcm_cfg_file,
                        trace_gen_cfg_file,
                        traces,
                        num_instrs_per_phase,
                        stats_output,
                        trace_output_file};
}

// Function to test cache behavior.
// Please also un-comment std::cout sections in corresponding tag class.
auto runCacheTest(const char* cfg_file, const char *trace_name)
{
    Config cfg(cfg_file);
    Simulator::Trace cpu_trace(trace_name);
    
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

auto L1TraceGen(std::string& cfg_file,
                std::string& cpu_trace_file,
                std::string& trace_out_file)
{
    Config cfg(cfg_file);
    Simulator::Trace cpu_trace(cpu_trace_file);
    
    Simulator::Instruction instr;

    std::ofstream trace_out(trace_out_file);

    // To test Set-Assoc tag with LRU replacement policy.
    CacheSimulator::LRUSetWayAssocTags tags(int(Config::Cache_Level::L1D), cfg);
    // tags.printTagInfo();
    // std::cout << "\nCache (tag) stressing mode...\n";

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
                trace_out << "0 " << instr.eip << " " << aligned_addr << " L\n";

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

    trace_out.close();
}

auto runMemTrace(MemObject *mem_obj,
                 const char *trace_name,
                 System::MMU *mmu)
{
    Simulator::Trace mem_trace(trace_name);

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
