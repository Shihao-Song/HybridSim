#include <iostream>

#include "Simulation.h"

int main(int argc, const char *argv[])
{
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

    Config cfg(argv[1]);

    // Create (PCM) main memory
    std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

    // TODO, test PCM before proceeding further.
}
