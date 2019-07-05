#include <iostream>

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

    // Create eDRAM
    std::unique_ptr<MemObject> eDRAM(createMemObject(cfg, Memories::eDRAM)); 
    eDRAM->setNextLevel(PCM.get());

    // Create L3
    std::unique_ptr<MemObject> L3(createMemObject(cfg, Memories::L3_CACHE));
    L3->setNextLevel(eDRAM.get());

    /* Create Processor */
    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE));
    L2->setNextLevel(L3.get());

    // Create L1-D
    std::unique_ptr<MemObject> L1_D(createMemObject(cfg, Memories::L1_D_CACHE));
    L1_D->setNextLevel(L2.get());

    // Create Processor 
    std::unique_ptr<Processor> processor(new Processor(trace_lists));    
    processor->setDCache(0, L1_D.get());

    /* Simulation */
    runCPUTrace(processor.get());

    L1_D->debugPrint();
    L2->debugPrint();
    L3->debugPrint();
    eDRAM->debugPrint();
}
