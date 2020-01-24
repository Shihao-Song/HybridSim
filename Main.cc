#include <iostream>
#include <fstream>

#include "Simulation.h"

void eDRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                      std::vector<std::string> &trace_lists,
                                      int64_t num_instrs_per_phase,
                                      std::string &stats_output_file);

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file);

int main(int argc, const char *argv[])
{
    auto [dram_cfg_file,
          pcm_cfg_file,
          trace_lists,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          stats_output_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    assert(pcm_cfg_file != "N/A");

    std::vector<Config> cfgs;
    if (dram_cfg_file != "N/A")
    {
        cfgs.emplace_back(dram_cfg_file);
        cfgs.emplace_back(pcm_cfg_file);
    }
    else
    {
        cfgs.emplace_back(pcm_cfg_file);
    }

    if (cfgs.size() > 1)
    {
        // For a Hybrid system, the first config file should be for DRAM and the second
        // one should be PCM.
        Hybrid_DRAM_PCM_Full_System_Simulation(cfgs,
                                               trace_lists,
                                               num_instrs_per_phase,
                                               stats_output_file);
    }
    else
    {
        eDRAM_PCM_Full_System_Simulation(cfgs,
                                         trace_lists,
                                         num_instrs_per_phase,
                                         stats_output_file);
    }
}

// TODO, clean up this function.
void eDRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                      std::vector<std::string> &trace_lists,
                                      int64_t num_instrs_per_phase,
                                      std::string &stats_output_file)
{
    /*
    unsigned num_of_cores = trace_lists.size();
    Config &pcm_cfg = cfgs[0];

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(pcm_cfg, Memories::PCM));

    // Create eDRAM
    std::unique_ptr<MemObject> eDRAM(createMemObject(pcm_cfg, Memories::eDRAM, isLLC));
    eDRAM->setNextLevel(PCM.get());
   
    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isNonLLC));
    L2->setNextLevel(eDRAM.get());
    L2->setArbitrator(num_of_cores);

    // Create Processor
    std::vector<std::unique_ptr<MemObject>> L1_D_all;
    for (int i = 0; i < num_of_cores; i++)
    {
        // Create L1-D
        std::unique_ptr<MemObject> L1_D(createMemObject(pcm_cfg,
                                                        Memories::L1_D_CACHE,
                                                        isNonLLC));
        L1_D->setId(i);
        L1_D->setBoundaryMemObject();
        L1_D->setNextLevel(L2.get());

        L1_D_all.push_back(std::move(L1_D));
    }
    
    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, pcm_cfg));
    mmu->setMemSystem(PCM.get());
    PCM->setMMU(mmu.get());

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }
    
    std::cout << "\nSimulation Stage (eDRAM-PCM system)...\n\n";
    runCPUTrace(processor.get());

    // Collecting Stats
    Stats stats;

    for (auto &L1_D : L1_D_all)
    {
        L1_D->registerStats(stats);
    }
    L2->registerStats(stats);
    eDRAM->registerStats(stats);
    PCM->registerStats(stats);
    mmu->registerStats(stats);
    stats.registerStats("Execution Time (cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
    */
}

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file)
{
    // TODO, for any shared caches, multiply their mshr and wb sizes to num_of_cores, please
    // see our example configuration files for more information.
    unsigned num_of_cores = trace_lists.size();
    Config &dram_cfg = cfgs[0];
    Config &pcm_cfg = cfgs[1];

    // Memory System Creation
    std::unique_ptr<MemObject> DRAM_PCM(createHybridSystem(dram_cfg, pcm_cfg));
    // TODO, delete offline... function, we should only output important information to 
    // the stats file only.
    // DRAM_PCM->offlineReqAnalysis(offline_request_analysis_dir);

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

        std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isNonLLC));

        std::unique_ptr<MemObject> L3(createMemObject(pcm_cfg, Memories::L3_CACHE, isLLC));

        L1_D->setId(i);
        L2->setId(i);
        L3->setId(i);

        L3->setBoundaryMemObject();

        L1_D->setNextLevel(L2.get());
        L2->setNextLevel(L3.get());
        L3->setNextLevel(DRAM_PCM.get());

        L2->setPrevLevel(L1_D.get());
        L3->setPrevLevel(L2.get());

        L3->setInclusive();

        L1Ds.push_back(std::move(L1_D));
        L2s.push_back(std::move(L2));
        L3s.push_back(std::move(L3));
    }

    /*
    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isLLC));
    L2->setNextLevel(DRAM_PCM.get());
    L2->setArbitrator(num_of_cores);

    // Create Processor
    std::vector<std::unique_ptr<MemObject>> L1_D_all;
    for (int i = 0; i < num_of_cores; i++)
    {
        // Create L1-D
        std::unique_ptr<MemObject> L1_D(createMemObject(pcm_cfg,
                                                        Memories::L1_D_CACHE,
                                                        isNonLLC));
        L1_D->setId(i);
        L1_D->setBoundaryMemObject();
        L1_D->setNextLevel(L2.get());

        L1_D_all.push_back(std::move(L1_D));
    }
    */

    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, dram_cfg, pcm_cfg));
    mmu->setMemSystem(DRAM_PCM.get());
    DRAM_PCM->setMMU(mmu.get());

    // Create Processor
    // TODO, mem_object should have a field to indicate it's on-chip or off-chip. 
    std::unique_ptr<Processor> processor(new Processor(pcm_cfg.on_chip_frequency,
                                                       pcm_cfg.off_chip_frequency,
                                                       trace_lists, DRAM_PCM.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1Ds[i].get());
    }

    std::cout << "\nSimulation Stage (Hybrid DRAM-PCM system)...\n\n";
    runCPUTrace(processor.get());

    /*
    // Collecting Stats
    Stats stats;

    for (auto &L1_D : L1_D_all)
    {
        L1_D->registerStats(stats);
    }
    L2->registerStats(stats);
    mmu->registerStats(stats);
    DRAM_PCM->registerStats(stats);
    stats.registerStats("Execution Time (cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
    */
}

