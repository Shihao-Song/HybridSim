#ifndef __PCMSIM_CONTROLLER_HH__
#define __PCMSIM_CONTROLLER_HH__

#include "Sim/request.hh"
#include "Sim/config.hh"

#include "PCMSim/Array_Architecture/pcm_sim_array.hh"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>

namespace PCMSim
{
// TODO, BaseController should be re-considered in the future.
class BaseController
{
  public:
    typedef Simulator::Config Config;

  public:
    std::unique_ptr<Array> channel;
    int id; // controller/channel ID

  protected:
    Tick clk;

  public:  

    BaseController(int _id, Config &cfg) : id(_id), clk(0)
    {
        channel = std::make_unique<Array>(Config::Array_Level::Channel, cfg);
        channel->id = _id;
    }
    
    virtual ~BaseController() { }

    // How many requests left to get served.
    virtual int getQueueSize() = 0;

    // Push request into queue
//    virtual bool enqueue(Request& req) = 0;

    // tick
    virtual void tick() {}
};

class Controller : public BaseController
{
    typedef Simulator::Request Request;

  protected:
    std::list<Request> r_w_q;
    const int max = 64; // Max size of r_w_q

    std::deque<Request> r_w_pending_queue;

  protected:
    bool scheduled;
    std::list<Request>::iterator scheduled_req;

  public:

    Controller(int _id, Config &cfg)
        : BaseController(_id, cfg),
          scheduled(0)
    {
        // Initialize timing info
        read_latency = channel->arr_info.tRCD +
                       channel->arr_info.tData +
                       channel->arr_info.tCL;

        read_bank_latency = channel->arr_info.tRCD +
                            channel->arr_info.tCL;

        write_latency = channel->arr_info.tRCD +
                        channel->arr_info.tData +
                        channel->arr_info.tWL +
                        channel->arr_info.tWR;

        tData = channel->arr_info.tData;
    }

    int getQueueSize() override 
    {
        return r_w_q.size() + r_w_pending_queue.size();
    }
    
    bool enqueue(Request& req)
    {
        if (r_w_q.size() == max)
        {
            // Queue is full
            return false;
        }
        
	r_w_q.push_back(req);
        
        return true;
    }

    void tick() override;

  protected:
    unsigned read_latency;
    unsigned read_bank_latency;
    unsigned write_latency;
    unsigned tData; 

  protected:
    bool issueAccess();
    void servePendingAccesses();
    void channelAccess();
};
}

#endif
