#ifndef __DRAM_PCM_CONTROLLER_HH__
#define __DRAM_PCM_CONTROLLER_HH__

#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"

namespace PCMSim
{
// TODO, should contain one PCM controller and one DRAM controller.
template<typename DCtrl, typename PCtrl>
class DRAMPCMController
{
    typedef Simulator::Config Config;

  protected:
    DCtrl DRAM_controller;
    PCtrl PCM_controller;

  public:
    DRAMPCMController(int _id, Config &dram_cfg, Config &pcm_cfg)
        : DRAM_controller(_id, dram_cfg)
        , PCM_controller(_id, pcm_cfg)
    {
    }

    void offlineReqAnalysis(std::ofstream *out)
    {
    
    }

    void reInitialize()
    {
        // TODO, reInitialize both PCM controller and DRAM controller.
    }

    int pendingRequests()
    {
        // TODO, get number of pending requests from both PCM controller and 
        // DRAM controller.

        int outstandings = 0;

        return outstandings;
    }

    void tick()
    {
        // TODO, tick both PCM and DRAM controller
    }

    bool enqueue(Request& req)
    {
        // TODO, should send to either PCM controller or DRAM controller
    
    }
};

typedef DRAMPCMController<TLDRAMController, CPAwareController> TLDRAMPCMController;
}

#endif
