#ifndef __PCMSIM_CONTROLLER_HH__
#define __PCMSIM_CONTROLLER_HH__

#include "../request.hh"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <list>

namespace PCMSim
{
class Array;

class Controller
{
  public:
    Array *channel;

  private:
    Tick clk;

    std::list<Request> r_w_q;
    const int max = 64; // Max size of r_w_q

    std::deque<Request> r_w_pending_queue;

  private:
    // This section contains scheduler
    bool scheduled;
    std::list<Request>::iterator scheduled_req;

  public:
    Controller(Array *_channel) : channel(_channel), clk(0),
                                  scheduled(0) {}
    ~Controller() { delete channel; }

    int getQueueSize() { return r_w_q.size() + r_w_pending_queue.size(); }
    
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

    void tick();

  private:
    bool issueAccess();
    void servePendingAccesses();
    void channelAccess();
};
}

#endif
