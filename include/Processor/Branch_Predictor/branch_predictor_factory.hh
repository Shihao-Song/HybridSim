#ifndef __BP_FACTORY_HH__
#define __BP_FACTORY_HH__

#include "Processor/Branch_Predictor/2bit_local.hh"

#include "Processor/Branch_Predictor/tournament.hh"

#include "Processor/Branch_Predictor/TAGE/tage.hh"

#include "Processor/Branch_Predictor/LTAGE/ltage.hh"

#include "Processor/Branch_Predictor/TAGE_SC_L/tage_sc_l_64KB.hh"

#include "Processor/Branch_Predictor/Multiperspective_Perceptron/multiperspective_perceptron_64KB.hh"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace CoreSystem
{
// TODO, think of a smarter to manage the parameters.
std::vector<std::unique_ptr<Params>> params;
std::unique_ptr<Branch_Predictor> createBP(std::string type)
{
    // TODO, tune it to 64KB
    if (type == "2-bit-local") // This is just a bimodal predictor.
    {
        return std::make_unique<Two_Bit_Local>();
    }
    // TODO, tune it to 64KB
    else if (type == "tournament")
    {
        return std::make_unique<Tournament>();
    }
    // For fair comparisons, set tage and ltage the same size as tage_sc_l.
    else if (type == "tage")
    {
        auto tage_base_params = std::make_unique<TAGE_SC_L_TAGE_64KBParams>();
        auto tage_base = std::make_unique<TAGE_SC_L_TAGE_64KB>(tage_base_params.get());
        tage_base->init();
        auto tage_params = std::make_unique<TAGEParams>();
        tage_params->tage = std::move(tage_base);

        auto tage = std::make_unique<TAGE>(tage_params.get());
        tage->init();

        params.push_back(std::move(tage_base_params));
        params.push_back(std::move(tage_params));

        return tage;
    }
// TODO, for fair comparisons, set tage and ltage the same size as tage_sc_l.
    else if (type == "ltage")
    {
        auto ltage_params = std::make_unique<LTAGEParams>();

        auto lp_params = std::make_unique<TAGE_SC_L_64KB_LoopPredictorParams>();
        ltage_params->loop_predictor =
            std::make_unique<TAGE_SC_L_LoopPredictor>(lp_params.get());
        ltage_params->loop_predictor->init();

        auto tage_base_params = std::make_unique<TAGE_SC_L_TAGE_64KBParams>();
        auto tage_base = std::make_unique<TAGE_SC_L_TAGE_64KB>(tage_base_params.get());
        tage_base->init();
        auto tage_params = std::make_unique<TAGEParams>();
        tage_params->tage = std::move(tage_base);

        ltage_params->tage_params = std::move(tage_params);
        auto ltage = std::make_unique<LTAGE>(ltage_params.get());
        ltage->init();

        params.push_back(std::move(lp_params));
        params.push_back(std::move(ltage_params));

        return ltage;
    }
    else if (type == "tage_sc_l")
    {
        auto tage_sc_l_params = std::make_unique<TAGE_SC_L_64KBParams>();

        auto lp_params = std::make_unique<TAGE_SC_L_64KB_LoopPredictorParams>();
        tage_sc_l_params->loop_predictor = 
            std::make_unique<TAGE_SC_L_LoopPredictor>(lp_params.get());
        tage_sc_l_params->loop_predictor->init();

        auto sc_params = std::make_unique<TAGE_SC_L_64KB_StatisticalCorrectorParams>();
        tage_sc_l_params->statistical_corrector = 
            std::make_unique<TAGE_SC_L_64KB_StatisticalCorrector>(sc_params.get());
        tage_sc_l_params->statistical_corrector->init();

        auto tage_base_params = std::make_unique<TAGE_SC_L_TAGE_64KBParams>();
        auto tage_base = std::make_unique<TAGE_SC_L_TAGE_64KB>(tage_base_params.get());
        tage_base->init();
        auto tage_params = std::make_unique<TAGEParams>();
        tage_params->tage = std::move(tage_base);
        tage_sc_l_params->tage_params = std::move(tage_params);

        auto tage_sc_l = std::make_unique<TAGE_SC_L_64KB>(tage_sc_l_params.get());
        tage_sc_l->init();

        params.push_back(std::move(lp_params));
        params.push_back(std::move(sc_params));
        params.push_back(std::move(tage_sc_l_params));
        return tage_sc_l;
    }
    /*
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
    else if (type == "mpp")
    {
        auto mpp_params = std::make_unique<MultiperspectivePerceptron64KBParams>();
        auto mpp = std::make_unique<MultiperspectivePerceptron64KB>(mpp_params.get());
        mpp->init();

        params.push_back(std::move(mpp_params));
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
