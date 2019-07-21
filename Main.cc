#include <iostream>
#include <fstream>

#include "Simulation.h"

void FullSystemSimulation(const char* cfg_file,
                          std::vector<const char*> trace_lists,
                          const char* output_file,
                          bool pcm_trace_extr = false);

void PCMSimulation(const char* cfg_file,
                   const char* pcm_trace,
                   const char* mmu_trained_data,
                   const char* output_file);

void MMUTraining(const char* cfg_file,
                 std::vector<const char*> trace_lists,
                 const char* output_file);

int main(int argc, const char *argv[])
{
    auto [mode, cfg_file, trace_lists, output_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);

    if (strcmp(mode, "FullSys") == 0)
    {
        FullSystemSimulation(cfg_file, trace_lists, output_file, false);
    }
    else if (strcmp(mode, "PCMTraceExtr") == 0)
    {
        FullSystemSimulation(cfg_file, trace_lists, output_file, true);
    }
    else if (strcmp(mode, "PCM-Only") == 0)
    {
        const char* mmu_trained_data = nullptr;
        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], "--mmu_trained_data") == 0)
            {
                mmu_trained_data = argv[i + 1];
                break;
            }
        }
        PCMSimulation(cfg_file, trace_lists[0], mmu_trained_data, output_file);
    }
    else if (strcmp(mode, "MMU-Training") == 0)
    {
        MMUTraining(cfg_file, trace_lists, output_file);
    }
}

void FullSystemSimulation(const char* cfg_file,
                          std::vector<const char*> trace_lists,
                          const char* output_file,
                          bool pcm_trace_extr)
{
    unsigned num_of_cores = trace_lists.size();
    std::cout << "\nConfiguration file: " << cfg_file << "\n";
    if (pcm_trace_extr)
    {
        std::cout << "(Trace) Output file: " << output_file << "\n\n";
    }
    else
    {
        std::cout << "(Stats) Output file: " << output_file << "\n\n";
    }

    /* Memory System Creation */
    Config cfg(cfg_file);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));
    if (pcm_trace_extr)
    {
        PCM->setTraceOutput(output_file);
    }

    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isLLC));
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
    std::unique_ptr<System::TrainedMMU> mmu(new System::MFUPageToNearRows(num_of_cores, cfg));
    if (cfg.trained_mmu)
    {
        std::cout << "MMU training stage... \n\n";
        mmu->train(trace_lists);
    }

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    processor->setMMU(mmu.get());
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }
    
    /* Simulation */
    runCPUTrace(processor.get());

    /* Collecting Stats */
    if (!pcm_trace_extr)
    {
        Stats stats;

        for (auto &L1_D : L1_D_all)
        {
            L1_D->registerStats(stats);
        }
        L2->registerStats(stats);
        PCM->registerStats(stats);
        stats.registerStats("Execution Time (cycles) = " + 
                            std::to_string(processor->exeTime()));
        stats.outputStats(output_file);
    }
}

void PCMSimulation(const char* cfg_file,
                   const char* pcm_trace,
                   const char* mmu_trained_data,
                   const char* output_file)
{
    /* Memory System Creation */
    Config cfg(cfg_file);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));
   
    // Create a MMU and pre-load the trained data if any
    System::TrainedMMU *mmu_ptr = nullptr;
    std::unique_ptr<System::TrainedMMU> mmu(new System::MFUPageToNearRows(0, cfg));
    if (mmu_trained_data)
    {
        mmu->preLoadTrainedData(mmu_trained_data, cfg.perc_re_alloc);
        mmu_ptr = mmu.get();
    }

    uint64_t end_exe = runMemTrace(PCM.get(), pcm_trace, mmu_ptr);

    // Stats collections
    Stats stats;
    PCM->registerStats(stats);
    stats.registerStats("Execution Time (cycles) = " +
                        std::to_string(end_exe));
    stats.outputStats(output_file);
}

void MMUTraining(const char* cfg_file,
                 std::vector<const char*> trace_lists,
                 const char* output_file)
{
    Config cfg(cfg_file);
    std::cout << "\nMMU training mode...\n";
    unsigned num_of_cores = trace_lists.size();
    std::unique_ptr<System::TrainedMMU> mmu(new System::MFUPageToNearRows(num_of_cores, cfg));
    mmu->trainedDataOutput(output_file);
    mmu->train(trace_lists);
}

