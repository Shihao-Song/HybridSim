#include <iostream>
#include <fstream>

#include "Simulation.h"

int main(int argc, const char *argv[])
{
    auto [cfg_file, trace_lists, output_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    unsigned num_of_cores = trace_lists.size();
    std::cout << "\nConfiguration file: " << cfg_file << "\n";
    std::cout << "(Stats) Output file: " << output_file << "\n\n";

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

    /* Collecting Stats */
    Stats stats;

    for (auto &L1_D : L1_D_all)
    {
        L1_D->registerStats(stats);
    }
    L2->registerStats(stats);
    stats.outputStats(output_file);
}
