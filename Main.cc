#include <iostream>

#include "Simulation.h"

typedef Simulator::Config Config;

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
    
    //PCMSim::Array channel(Config::Array_Level::Channel, cfg);
    //Controller ctrl(0, cfg);
    PCMSim::NormalPCMSimMemorySystem mem_sys(cfg);
}
