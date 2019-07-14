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
    
    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists, L2.get()));
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1_D_all[i].get());
    }

    /* Simulation */
    runCPUTrace(processor.get());
/*
    std::cout << "Number of stores: "
              << processor->numStores()
              << "\n"
              << "Number of loads: "
              << processor->numLoads()
              << "\n";
*/
    /* Record Simulation Stats */
    // Currently, we only care about the evictions and loads from the LLC (eDRAM).
    // We need to evaluate different benchmarks first.
//    std::ofstream stats;
//    stats.open("workload_eval.csv", std::ios_base::app);
    // stats << argv[1] << ","; // Workload's name
    // eDRAM->debugPrint(stats);

    /*
    std::cout << "\n***************** Cache Stats *****************\n"; 
    for (int i = 0; i < num_of_cores; i++)
    {
        std::cout << "\nCore " << i << " L1-DCache: \n";
        L1_D_all[i]->debugPrint(stats);
        std::cout << "\nCore " << i << " L2-Cache: \n";
        L2_all[i]->debugPrint(stats);
    }
    std::cout << "\nL3-Cache: \n";
    L3->debugPrint(stats);
    std::cout << "\neDRAM: \n";
    eDRAM->debugPrint(stats);
    std::cout << "\nProcessor Execution Time: " << processor->exeTime() << "\n";
    */
}
