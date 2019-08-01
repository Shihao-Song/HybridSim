#include <iostream>
#include <fstream>

#include "Simulation.h"

void FullSystemSimulation(Config &cfg,
                          std::vector<std::string> &trace_lists,
                          std::vector<uint64_t> &profiling_limits,
                          std::vector<int> &trained_mmu_required_sizes,
                          std::string &stats_output_file,
                          std::string &mmu_profiling_data_output_file);

int main(int argc, const char *argv[])
{
    auto [cfg_file,
          charge_pump_info,
          trace_lists,
          profiling_limits,
          num_profiling_entries,
          stats_output_file,
          mmu_profiling_data_output_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    
    std::cout << "\nConfiguration file: " << cfg_file << "\n";
    std::cout << "Stats output file: " << stats_output_file << "\n";
    if (mmu_profiling_data_output_file != "N/A")
    {
        std::cout << "MMU profiling data output file: "
                  << mmu_profiling_data_output_file << "\n\n";
    }

    std::vector<int> trained_mmu_required_sizes;
    trained_mmu_required_sizes.push_back(num_profiling_entries);

    Config cfg(cfg_file);
    cfg.parseChargePumpInfo(charge_pump_info.c_str());
    FullSystemSimulation(cfg,
                         trace_lists,
                         profiling_limits,
                         trained_mmu_required_sizes,
                         stats_output_file,
                         mmu_profiling_data_output_file);
}

void FullSystemSimulation(Config &cfg,
                          std::vector<std::string> &trace_lists,
                          std::vector<uint64_t> &profiling_limits,
                          std::vector<int> &trained_mmu_required_sizes,
                          std::string &stats_output_file,
                          std::string &mmu_profiling_data_output_file)
{
    unsigned num_of_cores = trace_lists.size();
    
    /* Memory System Creation */
    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    // Create eDRAM
    std::unique_ptr<MemObject> eDRAM(createMemObject(cfg, Memories::eDRAM, isLLC));
    eDRAM->setNextLevel(PCM.get());

    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isNonLLC));
    L2->setNextLevel(eDRAM.get());
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
    mmu->setSizes(trained_mmu_required_sizes);
    
    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }
   
    if (profiling_limits.size())
    {
        // Nofity processor that we are in profiling stage;
        processor->profiling(profiling_limits);
        mmu->setProfilingStage();

        std::cout << "\nProfiling Stage...\n\n";
        runCPUTrace(processor.get());

        // Re-initialize all the states.
        processor->reInitialize();
        mmu->setInferenceStage();
    }

    std::cout << "\nSimulation Stage...\n\n";
    runCPUTrace(processor.get());

    /* Optional, collecting MMU trained data */
    if (mmu_profiling_data_output_file != "N/A")
    {
        mmu->profilingDataOutput(mmu_profiling_data_output_file);
        mmu->printProfiling();
    }

    /* Collecting Stats */
    Stats stats;

    for (auto &L1_D : L1_D_all)
    {
        L1_D->registerStats(stats);
    }
    L2->registerStats(stats);
    eDRAM->registerStats(stats);
    PCM->registerStats(stats);
    stats.registerStats("Execution Time (cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
}
