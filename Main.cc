#include <iostream>
#include <fstream>

#include "Simulation.h"

// Preparing for prefetcher framework

struct hybridCfgArgs
{
    hybridCfgArgs() {}
    ~hybridCfgArgs() {}

    Config dram_cfg;
    Config pcm_cfg;
};
void Hybrid_DRAM_PCM_Full_System_Simulation(hybridCfgArgs &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file,
                                            std::string &svf_trace_dir);

int main(int argc, const char *argv[])
{
    // TODO, prepare for SVF integration

    auto [mode,
          dram_cfg_file,
          pcm_cfg_file,
          trace_lists,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          stats_output_file,
          svf_trace_dir] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);

    if (mode == "hybrid")
    {
        hybridCfgArgs cfgs;

        cfgs.dram_cfg.setCfgFile(dram_cfg_file);
        cfgs.pcm_cfg.setCfgFile(pcm_cfg_file);

        // For a Hybrid system, the first config file should be for DRAM and the second
        // one should be PCM.
        Hybrid_DRAM_PCM_Full_System_Simulation(cfgs,
                                               trace_lists,
                                               num_instrs_per_phase,
                                               stats_output_file,
                                               svf_trace_dir);
    }
    else
    {
        std::cerr << "Error: currently only support hybrid for this branch \n";
        exit(0);
    }
}

void Hybrid_DRAM_PCM_Full_System_Simulation(hybridCfgArgs &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file,
                                            std::string &svf_trace_dir)
{
    // TODO, for any shared caches, multiply their mshr and wb sizes to num_of_cores, please
    // see our example configuration files for more information.
    unsigned num_of_cores = trace_lists.size();
    Config &dram_cfg = cfgs.dram_cfg;
    Config &pcm_cfg = cfgs.pcm_cfg;

    // Memory System Creation
    std::unique_ptr<MemObject> DRAM_PCM(createHybridSystem(dram_cfg, pcm_cfg));

    // Cache system
    std::vector<std::unique_ptr<MemObject>> L1Ds;
    std::vector<std::unique_ptr<MemObject>> L2s;
    std::vector<std::unique_ptr<MemObject>> L3s;
    // Skylake cache system
    for (int i = 0; i < num_of_cores; i++)
    {
        std::unique_ptr<MemObject> L1_D(createMemObject(pcm_cfg,
                                                        Memories::L1_D_CACHE,
                                                        isNonLLC));

        std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, 
                                                      Memories::L2_CACHE, 
                                                      isNonLLC));

        std::unique_ptr<MemObject> L3(createMemObject(pcm_cfg, 
                                                      Memories::L3_CACHE,
                                                      isLLC));

        L1_D->setId(i);
        L2->setId(i);
        L3->setId(i);

        L3->setBoundaryMemObject(); // Boundary memory object.

        L1_D->setNextLevel(L2.get());
        L2->setNextLevel(L3.get());
        L3->setNextLevel(DRAM_PCM.get());

        L2->setPrevLevel(L1_D.get());
        L3->setPrevLevel(L2.get());

        L1Ds.push_back(std::move(L1_D));
        L2s.push_back(std::move(L2));
        L3s.push_back(std::move(L3));
    }

    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, dram_cfg, pcm_cfg));
    DRAM_PCM->setMMU(mmu.get());
    
    // Create Processor
    std::unique_ptr<Processor> processor(new Processor(pcm_cfg.on_chip_frequency,
                                                       pcm_cfg.off_chip_frequency,
                                                       trace_lists, DRAM_PCM.get()));
    
    processor->setMMU(mmu.get());
    processor->numClksPerPhase(num_instrs_per_phase);
    processor->setSVFTraceDir(svf_trace_dir);

    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1Ds[i].get());
    }

    std::cout << "\nSimulation Stage (Hybrid DRAM-PCM system)...\n\n";
    runCPUTrace(processor.get());

    // Collecting Stats
    Stats stats;

    stats.registerStats("Execution Time (CPU cycles) = " + 
                        std::to_string(processor->exeTime()));

    processor->registerStats(stats);

    for (auto &L1D : L1Ds)
    {
        L1D->registerStats(stats);
    }
    for (auto &L2 : L2s)
    {
        L2->registerStats(stats);
    }
    for (auto &L3 : L3s)
    {
        L3->registerStats(stats);
    }

    mmu->registerStats(stats);
    DRAM_PCM->registerStats(stats);
    
    stats.outputStats(stats_output_file);
}
