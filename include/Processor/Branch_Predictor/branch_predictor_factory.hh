#ifndef __BP_FACTORY_HH__
#define __BP_FACTORY_HH__

#include "Processor/Branch_Predictor/2bit_local.hh"
#include "Processor/Branch_Predictor/tournament.hh"
// #include "Processor/Branch_Predictor/tage.hh"
// #include "Processor/Branch_Predictor/ltage.hh"

// #include "Processor/Branch_Predictor/loop_predictor.hh"
// #include "Processor/Branch_Predictor/statistical_corrector.hh"

#include "Processor/Branch_Predictor/Multiperspective_Perceptron/multiperspective_perceptron_64KB.hh"

#include <memory>
#include <string>

namespace CoreSystem
{
// TODO, add (1) bi-mod predictor and (2) perceptron predictor
std::unique_ptr<Branch_Predictor> createBP(std::string type)
{
    if (type == "2-bit-local") // This is just a bimodal predictor.
    {
        return std::make_unique<Two_Bit_Local>();
    }
    else if (type == "tournament")
    {
        return std::make_unique<Tournament>();
    }
/*    else if (type == "tage")
    {
        std::unique_ptr<TAGEParams> p = std::make_unique<TAGEParams>();
        auto tage = std::make_unique<TAGE>(p.get());
        tage->init();
        return tage;
    }
    else if (type == "ltage")
    {
        std::unique_ptr<TAGEParams> tage_params = std::make_unique<TAGEParams>();
        std::unique_ptr<LPParams> lp_params = std::make_unique<LPParams>();
        std::unique_ptr<LTAGEParams> ltage_params = std::make_unique<LTAGEParams>();
        ltage_params->tage = tage_params.get();
        ltage_params->lp = lp_params.get();

        auto ltage = std::make_unique<LTAGE>(ltage_params.get());
        ltage->init();
        return ltage;
    }
    */
    else if (type == "MultiperspectivePerceptron")
    {
        auto mpp_params = std::make_unique<MultiperspectivePerceptron64KBParams>();
        auto mpp = std::make_unique<MultiperspectivePerceptron64KB>(mpp_params.get());
        mpp->init();

        return mpp;
    }
    else
    {
        std::cerr << "Unsupported Branch Predictor Type." << std::endl;
        exit(0);
    }
}
}
#endif
