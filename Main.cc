#include <iostream>
#include <fstream>

#include "Simulation.h"

int main(int argc, const char *argv[])
{
    auto [cfg_file, trace_lists] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    unsigned num_of_cores = trace_lists.size();
    std::cout << "\nConfiguration file: " << cfg_file << "\n";

    /* Memory System Creation */
    Config cfg(cfg_file);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    // Create eDRAM (temp disabled)
    std::unique_ptr<MemObject> eDRAM(createMemObject(cfg, Memories::eDRAM)); 
    eDRAM->setNextLevel(PCM.get());
    
    // Create L3
    std::unique_ptr<MemObject> L3(createMemObject(cfg, Memories::L3_CACHE));
    L3->setNextLevel(eDRAM.get());

    /* Create Processor */
    std::vector<std::unique_ptr<MemObject>> L2_all;
    std::vector<std::unique_ptr<MemObject>> L1_D_all;
    for (int i = 0; i < num_of_cores; i++)
    {
        // Create L2
        std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, i));
        L2->setNextLevel(L3.get());

        // Create L1-D
        std::unique_ptr<MemObject> L1_D(createMemObject(cfg, Memories::L1_D_CACHE, i));
        L1_D->setNextLevel(L2.get());

        L2_all.push_back(std::move(L2));
        L1_D_all.push_back(std::move(L1_D));
    }

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L3.get()));
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }

    /* Simulation */
    runCPUTrace(processor.get());

    /* Record Simulation Stats */
    // Currently, we only care about the evictions and loads from the LLC (eDRAM).
    std::ofstream stats;
    stats.open("workload_eval.csv", std::ios_base::app);
    stats << argv[1] << ","; // Workload's name
    eDRAM->debugPrint(stats);

    /*
    std::cout << "\n***************** Cache Stats *****************\n"; 
    for (int i = 0; i < num_of_cores; i++)
    {
        std::cout << "\nCore " << i << " L1-DCache: \n";
        L1_D_all[i]->debugPrint();
        std::cout << "\nCore " << i << " L2-Cache: \n";
        L2_all[i]->debugPrint();
    }
    std::cout << "\nL3-Cache: \n";
    L3->debugPrint();
    std::cout << "\neDRAM: \n";
    eDRAM->debugPrint();
    std::cout << "\nProcessor Execution Time: " << processor->exeTime() << "\n";
    */
}
