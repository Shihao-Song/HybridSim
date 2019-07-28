#ifndef __PCMSIM_PLP_CONTROLLER_HH__
#define __PCMSIM_PLP_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/PLP_Controller/pcm_sim_plp_queue.hh"

namespace PCMSim
{
typedef Simulator::Request Request;
typedef Simulator::PLPRequest PLPRequest;

class PLPController : public BaseController
{
  public:
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

    int pendingRequests() override
    {
        return r_w_q.size() + r_w_pending_queue.size();
    }

    bool enqueue(Request& req) override
    {
        // PLP needs an upgraded Request class
        PLPRequest encap(req);
        assert(encap.addr_vec.size());

        if (r_w_q.size() == r_w_q.max)
        {
            // Queue is full
            return false;
        }

        r_w_q.push_back(encap);

        return true;
    }

    void tick() override
    {
        clk++;
        channel->update(clk);

        servePendingAccesses();

        if (auto [scheduled, scheduled_req] = getHead();
            scheduled)
        {
            // TODO, be aware of slave request!
//            channelAccess(scheduled_req);
            if (scheduled_req->master == 1)
            {
                auto &slave = scheduled_req->slave_req;
                
                slave->commuToMMU();
                r_w_pending_queue.push_back(std::move(*slave));
                r_w_q.erase(slave);
            }

            scheduled_req->commuToMMU();
            r_w_pending_queue.push_back(std::move(*scheduled_req));
            r_w_q.erase(scheduled_req);
        }
    }

  protected:
    void servePendingAccesses();

    std::pair<bool,std::list<PLPRequest>::iterator> getHead();

  // Scheduler
  protected:
    bool OoOPair(std::list<PLPRequest>::iterator &req);
    void powerUpdate(std::list<PLPRequest>::iterator &req);
    void pairForRR(std::list<PLPRequest>::iterator &master,
                   std::list<PLPRequest>::iterator &slave);
    void pairForRW(std::list<PLPRequest>::iterator &master,
                   std::list<PLPRequest>::iterator &slave);

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
