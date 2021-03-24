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
    std::vector<std::string> trace_lists;
    int64_t num_clks_per_phase;
    std::string stats_output_file;
    std::string svf_trace_dir;
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
    std::vector<std::string> traces;
    int64_t num_clks_per_phase = -1;
    std::string stats_output;
    std::string svf_trace_dir;

    namespace po = boost::program_options;
    po::options_description desc("Options"); 
    desc.add_options() 
        ("help", "Print help messages")
        ("mode", po::value<std::string>(&mode),
                 "Mode: hybrid")
        ("dram-config", po::value<std::string>(&dram_cfg_file),
                   "Configuration file for DRAM (if hybrid system)")
        ("pcm-config", po::value<std::string>(&pcm_cfg_file),
                   "Configuration file for PCM (if hybrid system)")
        ("trace", po::value<std::vector<std::string>>(&traces),
                      "CPU trace or MEM trace")
        ("num_clks_per_phase", po::value<int64_t>(&num_clks_per_phase),
                   "Number of clks per phase")
        ("stat_output", po::value<std::string>(&stats_output),
                        "Stats output file/Stats output")
        ("svf_trace_dir", po::value<std::string>(&svf_trace_dir),
                        "SVF traces directory");

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
                        traces,
                        num_clks_per_phase,
                        stats_output,
                        svf_trace_dir};
}

#endif
