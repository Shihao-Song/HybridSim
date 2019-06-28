#include <iostream>

#include "Simulation.h"

#include "CacheSim/cache_queue.hh"
#include "CacheSim/tags/cache_tags.hh"

int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <configs-file>"
                  << " <trace-file>"
                  << "\n";
        return 0;
    }

    /* Object Creation */
    Config cfg(argv[1]);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    /* Simulation */
    runMemTrace(PCM.get(), argv[2]);
}
