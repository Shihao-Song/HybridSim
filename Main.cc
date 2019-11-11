#include <iostream>
#include <fstream>

#include "Simulation.h"

void FullSystemSimulation(Config &cfg,
                          std::vector<std::string> &trace_lists,
                          int64_t num_instrs_per_phase,
                          int num_ftis_per_phase,
                          std::string &stats_output_file,
                          std::string &offline_request_analysis_file);

int main(int argc, const char *argv[])
{
    auto [cfg_file,
          trace_lists,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          num_ftis_per_phase, // Max. # FTIs recorded for each phase
          stats_output_file,
          offline_request_analysis_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    
    std::cout << "\nConfiguration file: " << cfg_file << "\n";
    std::cout << "Stats output file: " << stats_output_file << "\n";

    Config cfg(cfg_file);

    FullSystemSimulation(cfg,
                         trace_lists,
                         num_instrs_per_phase,
                         num_ftis_per_phase,
                         stats_output_file,
                         offline_request_analysis_file);
}

void FullSystemSimulation(Config &cfg,
                          std::vector<std::string> &trace_lists,
                          int64_t num_instrs_per_phase,
                          int num_ftis_per_phase,
                          std::string &stats_output_file,
                          std::string &offline_request_analysis_file)
{
    unsigned num_of_cores = trace_lists.size();
    
    /* Memory System Creation */
    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));
//    PCM->offlineReqAnalysis(offline_request_analysis_file);

//    exit(0);
    // Create eDRAM
//    std::unique_ptr<MemObject> eDRAM(createMemObject(cfg, Memories::eDRAM, isLLC));
//    eDRAM->setNextLevel(PCM.get());
   
    // Create L2
//    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isNonLLC));
    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isLLC));
//    L2->setNextLevel(eDRAM.get());
    L2->setNextLevel(PCM.get());
    L2->setArbitrator(num_of_cores);

    /* Create Processor */
    std::vector<std::unique_ptr<MemObject>> L1_D_all;
    for (int i = 0; i < num_of_cores; i++)
    {
        // Create L1-D
        std::unique_ptr<MemObject> L1_D(createMemObject(cfg, Memories::L1_D_CACHE, isNonLLC));
        L1_D->setId(i);
        L1_D->setBoundaryMemObject();
        L1_D->setNextLevel(L2.get());

        L1_D_all.push_back(std::move(L1_D));
    }
    
    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::TrainedMMU> mmu(createTrainedMMU(num_of_cores, cfg));
    // mmu->setSizes(trained_mmu_required_sizes); // TODO, re-write this function!

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }

   
    /* Run the program in profiling stage */
    mmu->setProfilingStage();
    runCPUTrace(processor.get());
    processor->reStartTrace(); // Re-start traces
    processor->reInitialize(); // Re-initialize all components
    mmu->setInferenceStage(); // Re-allocate all MFU pages

    std::cout << "\nSimulation Stage...\n\n";
    runCPUTrace(processor.get());

    /* Collecting Stats */
    Stats stats;

    for (auto &L1_D : L1_D_all)
    {
        L1_D->registerStats(stats);
    }
    L2->registerStats(stats);
//    eDRAM->registerStats(stats);
    PCM->registerStats(stats);
    stats.registerStats("Execution Time (cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
}
