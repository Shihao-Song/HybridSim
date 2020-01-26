#ifndef __BP_FACTORY_HH__
#define __BP_FACTORY_HH__

#include "Processor/Branch_Predictor/2bit_local.hh"
// #include "Processor/Branch_Predictor/tournament.hh"

#include <memory>
#include <string>

namespace CoreSystem
{
auto createBP(std::string type)
{
    if (type == "2-bit-local")
    {
        return std::make_unique<Two_Bit_Local>();
    }
    // else if (type == "tournament")
    // {
    //     return std::make_unique<Tournament>();
    // }
    else
    {
        std::cerr << "Unsupported Branch Predictor Type." << std::endl;
        exit(0);
    }
}
}
#endif
