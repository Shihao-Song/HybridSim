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
    uint64_t total_waiting_time = 0;
    uint64_t finished_requests = 0;

  protected:
    bool offline_req_analysis_mode = false;
    std::ofstream *offline_req_ana_output;

  public:  

    BaseController(int _id, Config &cfg) : id(_id), clk(0)
    {
        channel = std::make_unique<Array>(Config::Array_Level::Channel, cfg);
        channel->id = _id;
    }
    
    virtual ~BaseController()
    { 
    }

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

    virtual void offlineReqAnalysis(std::ofstream *out)
    {
        offline_req_analysis_mode = true;
        offline_req_ana_output = out;
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
        std::cout << "Channel:" << req.addr_vec[int(Config::Decoding::Channel)] << ","
                  << "Rank:" << req.addr_vec[int(Config::Decoding::Rank)] << ","
                  << "Bank:" << req.addr_vec[int(Config::Decoding::Bank)] << ","
                  << "Part:" << req.addr_vec[int(Config::Decoding::Partition)] << ","
                  << "Tile:" << req.addr_vec[int(Config::Decoding::Tile)] << ","
                  << "Row:" << req.addr_vec[int(Config::Decoding::Row)] << ","
                  << "Col:" << req.addr_vec[int(Config::Decoding::Col)] << ",";

        if (req.req_type == Request::Request_Type::READ)
        {
            std::cout << "READ\n";
        }
        else
        {
            std::cout << "WRITE\n";
        }

        // std::cout << req.begin_exe << ","
        //           << req.end_exe << "\n\n";
    }
};

class FCFSController : public BaseController
{
  protected:
    std::list<Request> r_w_q;
    const int max = 64; // Max size of r_w_q

    std::deque<Request> r_w_pending_queue;

  protected:
    const unsigned singleReadLatency = 57;
    const unsigned singleWriteLatency = 162;
    const unsigned channelDelay = 15;

  public:
    FCFSController(int _id, Config &cfg)
        : BaseController(_id, cfg)
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
        
        if (req.display) { displayReqInfo(req); }
        req.queue_arrival = clk;	
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
                        if (offline_req_analysis_mode)
            {
                if (req.req_type == Request::Request_Type::READ)
                {
                    *offline_req_ana_output << "R ";
                }
                else
                {
                    *offline_req_ana_output << "W ";
                }

                *offline_req_ana_output << req.addr_vec[int(Config::Decoding::Channel)] << " "
                    << req.addr_vec[int(Config::Decoding::Rank)] << " "
                    << req.addr_vec[int(Config::Decoding::Bank)] << " "
                    << req.addr_vec[int(Config::Decoding::Row)] << " "
                    << req.queue_arrival << " "
                    << req.begin_exe << " "
                    << req.end_exe << "\n";
		*offline_req_ana_output << std::flush;
            }

            if (req.callback)
            {
                if (req.callback(req.addr))
                {
                    if (!req.mig)
                    {
                    uint64_t waiting_time = (req.begin_exe - req.queue_arrival);
                    total_waiting_time += waiting_time;
                    ++finished_requests;
                    }
                    r_w_pending_queue.pop_front();
                }
            }
            else
            {
                if (!req.mig)
		{
                uint64_t waiting_time = (req.begin_exe - req.queue_arrival);
                total_waiting_time += waiting_time;
                ++finished_requests;
                }
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
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            req_latency = singleWriteLatency;
        }
        else
        {
            std::cerr << "Unknown Request Type. \n";
            exit(0);
        }

        bank_latency = req_latency;
        channel_latency = channelDelay;

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
