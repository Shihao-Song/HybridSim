#ifndef __PCMSIM_CONTROLLER_HH__
#define __PCMSIM_CONTROLLER_HH__

#include "Sim/config.hh"
#include "Sim/request.hh"
#include "Sim/stats.hh"

#include "PCMSim/Array_Architecture/pcm_sim_array.hh"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>

namespace PCMSim
{
class BaseController
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    typedef Simulator::Config Config;
    typedef Simulator::Request Request;

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
    virtual int pendingRequests() = 0;

    // accepts a (general) request and push it into queue.
    virtual bool enqueue(Request& req) = 0;

    // tick
    virtual void tick() {}

    virtual void reInitialize()
    {
        clk = 0;

        // Re-initialize channel as well
        channel->reInitialize();
    }

     // Some Proxy functions
  protected:
    auto issueable(auto req)
    {
        int target_rank = (req->addr_vec)[int(Config::Decoding::Rank)];
        int target_bank = (req->addr_vec)[int(Config::Decoding::Bank)];

        return channel->isFree(target_rank, target_bank);
    }

    auto postAccess(auto scheduled_req,
                    unsigned channel_latency,
                    unsigned rank_latency,
                    unsigned bank_latency)
    {
        int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
        int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

        channel->postAccess(rank_id, bank_id,
                            channel_latency,
                            rank_latency,
                            bank_latency);
    }

    auto displayReqInfo(auto &req)
    {
        std::cout << "C:" << req.addr_vec[int(Config::Decoding::Channel)] << ","
                  << "R:" << req.addr_vec[int(Config::Decoding::Rank)] << ","
                  << "B:" << req.addr_vec[int(Config::Decoding::Bank)] << ","
                  << "P:" << req.addr_vec[int(Config::Decoding::Partition)] << ","
                  << "R:" << req.addr_vec[int(Config::Decoding::Row)] << ",";

        if (req.req_type == Request::Request_Type::READ)
        {
            std::cout << "R,";
        }
        else
        {
            std::cout << "W,";
        }

        std::cout << req.begin_exe << ","
                  << req.end_exe << "\n";
    }
};

class FCFSController : public BaseController
{
  protected:
    std::list<Request> r_w_q;
    const int max = 64; // Max size of r_w_q

    std::deque<Request> r_w_pending_queue;

  protected:
    const unsigned singleReadLatency;
    const unsigned bankDelayCausedBySingleRead;
    const unsigned singleWriteLatency;
    const unsigned bankDelayCausedBySingleWrite;
    const unsigned dataTransferLatency; 

  public:
    FCFSController(int _id, Config &cfg)
        : BaseController(_id, cfg),
          singleReadLatency(channel->singleReadLatency()),
          bankDelayCausedBySingleRead(channel->bankDelayCausedBySingleRead()),
          singleWriteLatency(channel->singleWriteLatency()),
          bankDelayCausedBySingleWrite(channel->bankDelayCausedBySingleWrite()),
          dataTransferLatency(channel->dataTransferLatency())
    {}

    int pendingRequests() override 
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
        
        req.OrderID = r_w_q.size(); // To track back-logging.
	r_w_q.push_back(req);
        
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
            channelAccess(scheduled_req);
            scheduled_req->commuToMMU();

            r_w_pending_queue.push_back(std::move(*scheduled_req));
            r_w_q.erase(scheduled_req);

            // Update back-logging information.
            for (auto &waiting_req : r_w_q)
            {
                --waiting_req.OrderID;
            }
        }
    }

  protected:
    void servePendingAccesses()
    {
        if (!r_w_pending_queue.size())
        {
            return;
        }

        Request &req = r_w_pending_queue[0];
        if (req.end_exe <= clk)
        {
            if (req.callback)
            {
                if (req.callback(req.addr))
                {
                    r_w_pending_queue.pop_front();
                }
            }
            else
            {
                r_w_pending_queue.pop_front();
            }
        }
    }

    virtual std::pair<bool,std::list<Request>::iterator> getHead()
    {
        if (r_w_q.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, r_w_q.end());
        }

        auto req = r_w_q.begin();
        if (issueable(req))
        {
            return std::make_pair(true, req);
        }
        return std::make_pair(false, r_w_q.end());
    }
    
    virtual void channelAccess(std::list<Request>::iterator& scheduled_req)
    {
        scheduled_req->begin_exe = clk;

        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = 0;

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            req_latency = singleReadLatency;
            bank_latency = bankDelayCausedBySingleRead;
            channel_latency = dataTransferLatency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            req_latency = singleWriteLatency;
            bank_latency = bankDelayCausedBySingleWrite;
            channel_latency = dataTransferLatency;
        }
        else
        {
            std::cerr << "Unknown Request Type. \n";
            exit(0);
        }

        scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

        // Post access
        postAccess(scheduled_req,
                   channel_latency,
                   req_latency, // This is rank latency for other ranks.
                                // Since there is no rank-level parall,
                                // other ranks must wait until the current rank
                                // to be fully de-coupled.
                   bank_latency);
    }

};

class FRFCFSController : public FCFSController
{
  public:
    FRFCFSController(int _id, Config &cfg)
        : FCFSController(_id, cfg)
    {}

  protected:
    std::pair<bool,std::list<Request>::iterator> getHead() override
    {
        if (r_w_q.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, r_w_q.end());
        }

        for (auto iter = r_w_q.begin(); iter != r_w_q.end(); ++iter)
        {
            if (issueable(iter))
            {
                return std::make_pair(true, iter);
            }
        }
        return std::make_pair(false, r_w_q.end());
    }
};
}

#endif
