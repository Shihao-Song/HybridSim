#ifndef __CP_AWARE_CONTROLLER_HH__
#define __CP_AWARE_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

namespace PCMSim
{
class CPAwareController : public FRFCFSController
{
  protected:
    struct Charging_Stage
    {
        float voltage;
        unsigned nclks_charge_or_discharge;
    };

    // TODO, let config parse information
    std::vector<Charging_Stage> read_lookaside_buffer;
    std::vector<Charging_Stage> set_lookaside_buffer;
    std::vector<Charging_Stage> reset_lookaside_buffer;

  public:
    CPAwareController(int _id, Config &cfg)
        : FRFCFSController(_id, cfg) 
    {
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        FRFCFSController::channelAccess(scheduled_req);
    }
};
}

#endif
