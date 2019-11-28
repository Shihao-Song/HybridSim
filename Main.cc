#include <iostream>
#include <fstream>

#include "Simulation.h"

void eDRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                      std::vector<std::string> &trace_lists,
                                      int64_t num_instrs_per_phase,
                                      std::string &stats_output_file,
                                      std::string &offline_request_analysis_file);

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file,
                                            std::string &offline_request_analysis_file);

int main(int argc, const char *argv[])
{
    auto [cfg_files,
          trace_lists,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          stats_output_file,
          offline_request_analysis_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);

    // TODO, limitation, if there are two config files, the first one should always be DRAM, and
    // the second one should always be PCM.
    std::cout << "\n";
    auto i = 0;
    for (auto &cfg_file : cfg_files)
    {
        std::cout << "Configuration file " << i << ": " << cfg_file << "\n";
        ++i;
    }
    std::cout << "\nStats output file: " << stats_output_file << "\n\n";

    std::vector<Config> cfgs;
    for (auto &cfg_file : cfg_files)
    {
        cfgs.emplace_back(cfg_file);
    }

    if (cfgs.size() > 1)
    {
        // For a Hybrid system, the first config file should be for DRAM and the second
        // one should be PCM.
        Hybrid_DRAM_PCM_Full_System_Simulation(cfgs,
                                               trace_lists,
                                               num_instrs_per_phase,
                                               stats_output_file,
                                               offline_request_analysis_file);
    }
    else
    {
        eDRAM_PCM_Full_System_Simulation(cfgs,
                                         trace_lists,
                                         num_instrs_per_phase,
                                         stats_output_file,
                                         offline_request_analysis_file);
    }
}

void eDRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                      std::vector<std::string> &trace_lists,
                                      int64_t num_instrs_per_phase,
                                      std::string &stats_output_file,
                                      std::string &offline_request_analysis_file)
{
    unsigned num_of_cores = trace_lists.size();
    Config &pcm_cfg = cfgs[0];

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(pcm_cfg, Memories::PCM));
//    PCM->offlineReqAnalysis(offline_request_analysis_file);

    // Create eDRAM
    std::unique_ptr<MemObject> eDRAM(createMemObject(pcm_cfg, Memories::eDRAM, isLLC));
    eDRAM->setNextLevel(PCM.get());
   
    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isNonLLC));
    L2->setNextLevel(eDRAM.get());
    L2->setArbitrator(num_of_cores);

    /* Create Processor */
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
    std::unique_ptr<System::TrainedMMU> mmu(createTrainedMMU(num_of_cores, pcm_cfg));

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }
    
    std::cout << "\nSimulation Stage...\n\n";
    runCPUTrace(processor.get());

    /* Collecting Stats */
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

}

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file,
                                            std::string &offline_request_analysis_file)
{
    unsigned num_of_cores = trace_lists.size();
    Config &pcm_cfg = cfgs[1];

    /* Memory System Creation */
    std::unique_ptr<MemObject> DRAM_PCM(createHybridSystem(cfgs[0], cfgs[1]));
  
    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isLLC));
    L2->setNextLevel(DRAM_PCM.get());
    L2->setArbitrator(num_of_cores);

    /* Create Processor */
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
    std::unique_ptr<System::TrainedMMU> mmu(createTrainedMMU(num_of_cores, pcm_cfg));
    mmu->setMemSystem(DRAM_PCM.get());

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }

    std::cout << "\nSimulation Stage...\n\n";
    runCPUTrace(processor.get());

    /* Collecting Stats */
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
}

