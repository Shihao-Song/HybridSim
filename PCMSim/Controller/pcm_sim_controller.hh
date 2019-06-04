#ifndef __PCMSIM_CONTROLLER_HH__
#define __PCMSIM_CONTROLLER_HH__

#include "../request.hh"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <list>

namespace Configuration
{
    class Config;
}

namespace PCMSim
{
class Array;

class BaseController
{
  public:
    Array *channel;

  protected:
    Tick clk;

  public:  
    typedef Configuration::Config Config;

    BaseController(Array *_channel)
        : channel(_channel), clk(0) {}
    
    virtual ~BaseController() { delete channel; }

    // How many requests left to get served.
    virtual int getQueueSize() = 0;

    // Push request into queue
    virtual bool enqueue(Request& req) = 0;

    // tick
    virtual void tick() {}
};

class Controller : public BaseController
{
  protected:
    std::list<Request> r_w_q;
    const int max = 64; // Max size of r_w_q

    std::deque<Request> r_w_pending_queue;

  protected:
    bool scheduled;
    std::list<Request>::iterator scheduled_req;

  public:

    Controller(Array *_channel)
        : BaseController(_channel),
          scheduled(0) {}

    int getQueueSize() override 
    {
        return r_w_q.size() + r_w_pending_queue.size();
    }
    
    bool enqueue(Request& req) override
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
    bool issueAccess();
    void servePendingAccesses();
    void channelAccess();
};
}

#endif
