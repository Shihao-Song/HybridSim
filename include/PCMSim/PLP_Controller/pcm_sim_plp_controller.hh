#ifndef __PCMSIM_PLP_CONTROLLER_HH__
#define __PCMSIM_PLP_CONTROLLER_HH__

#include "PCMSim/PLP_Controller/pcm_sim_plp_queue.hh"

namespace PCMSim
{
class PLPController : public BaseController
{
  public:
    typedef Simulator::Request Request;
    typedef Simulator::PLPRequest PLPRequest;

    PLPController(int _id, Config &cfg) 
        : BaseController(_id, cfg)
          , singleReadLatency(channel->singleReadLatency())
          , singleWriteLatency(channel->singleWriteLatency())
          , dataTransferLatency(channel->dataTransferLatency())
          , readWithReadLatency(channel->readWithReadLatency())
          , readWhileWriteLatency(channel->readWhileWriteLatency())
          , powerPerBitRead(channel->powerPerBitRead())
          , powerPerBitWrite(channel->powerPerBitWrite())
          , power_limit_enabled(cfg.power_limit_enabled)
          , starv_free_enabled(cfg.starv_free_enabled)
          , RAPL(cfg.RAPL)
          , THB(cfg.THB)
    {}

  protected:
    PLPReqQueue r_w_q;
    std::deque<PLPRequest> r_w_pending_queue;

  // (Special) timing and power info
  // Please refer to our paper for the detailed calculations.
  protected:
    const unsigned singleReadLatency;
    const unsigned singleWriteLatency;
    const unsigned dataTransferLatency;    
    const unsigned readWithReadLatency;
    const unsigned readWhileWriteLatency;
    const double powerPerBitRead;
    const double powerPerBitWrite;

  protected:
    double power = 0.0; // Running average power of the channel

    void update_power_read()
    {
        power = ((clk * power) + singleReadLatency * powerPerBitRead) /
                (clk + singleReadLatency);
    }

    void update_power_write()
    {
        power = ((clk * power) + singleWriteLatency * powerPerBitWrite) /
                (clk + singleWriteLatency);
    }

    double power_rr()
    {
        double tmp_power = ((clk * power) +
                           singleReadLatency * powerPerBitRead * 2.0) /
                           (clk + readWithReadLatency);
        return tmp_power;
    }

    double power_rw()
    {
        double tmp_power = ((clk * power) +
                           singleReadLatency * powerPerBitRead +
                           singleWriteLatency * powerPerBitWrite) /
                           (clk + readWhileWriteLatency);
        return tmp_power;
    }

  // Scheduling section
  protected:
    const bool rr_enabled = true; // Exploit R-R parallelism?

    const bool power_limit_enabled = false;
    const bool starv_free_enabled = false;

    const double RAPL; // running average power limit
    const int THB; // back-logging threshold
};
}

#endif
