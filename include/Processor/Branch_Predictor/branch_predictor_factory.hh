#ifndef __BP_FACTORY_HH__
#define __BP_FACTORY_HH__

#include "Processor/Branch_Predictor/2bit_local.hh"
#include "Processor/Branch_Predictor/tournament.hh"
#include "Processor/Branch_Predictor/tage.hh"
#include "Processor/Branch_Predictor/loop_predictor.hh"

#include <memory>
#include <string>

namespace CoreSystem
{
// TODO, add (1) bi-mod predictor and (2) perceptron predictor
std::unique_ptr<Branch_Predictor> createBP(std::string type)
{
    if (type == "2-bit-local") // TODO, change name to Local Branch Predictor
    {
        return std::make_unique<Two_Bit_Local>();
    }
    else if (type == "tournament")
    {
        return std::make_unique<Tournament>();
    }
    else if (type == "tage")
    {
        return std::make_unique<TAGE>();
    }
    else
    {
        std::cerr << "Unsupported Branch Predictor Type." << std::endl;
        exit(0);
    }
}
}
#endif
