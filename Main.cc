#include <iostream>

#include "Simulation.h"

int main(int argc, const char *argv[])
{
    auto [cfg_file, trace_lists] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);
    std::cout << "Configuration file: " << cfg_file << "\n";
    for (int i = 0; i < trace_lists.size(); i++)
    {
        std::cout << "Core " << i
                  << " is running trace: " << trace_lists[i] << "\n";
    }

    // runCPUTrace(argc, argv);
    /*
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <configs-file>"
                  << " <trace-file>"
                  << "\n";
        return 0;
    }
    */

    /* Object Creation */
//    Config cfg(argv[1]);
/*
    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    // Create eDRAM
    std::unique_ptr<MemObject> eDRAM(createMemObject(cfg, Memories::eDRAM)); 
    eDRAM->setNextLevel(PCM.get());

    // Create L3
    std::unique_ptr<MemObject> L3(createMemObject(cfg, Memories::L3_CACHE));
    L3->setNextLevel(eDRAM.get());

    // Create L2
    std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE));
    L2->setNextLevel(L3.get());

    // Create L1-D
    std::unique_ptr<MemObject> L1_D(createMemObject(cfg, Memories::L1_D_CACHE));
    L1_D->setNextLevel(L2.get());
*/
    /* Simulation */
//    runMemTrace(L1_D.get(), argv[2]);

//    L1_D->debugPrint();
//    L2->debugPrint();
//    L3->debugPrint();
//    eDRAM->debugPrint();
}
