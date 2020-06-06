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

// TODO, improvements
// There has to be two queues, one for read and one for write, the reason is that turning
// around the direction of the memory bus when alternating between reads and writes requires 
// additional latenty.
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
    const unsigned num_of_ranks;
    const unsigned num_of_banks; // per rank

    // One for read and one for write
    // This is a per-bank record on how many reads and writes left to a bank.
    std::vector<std::vector<int>> num_reqs_to_banks[2];

  protected:
    Tick clk;

  public:
    uint64_t total_waiting_time = 0;
    uint64_t finished_requests = 0;

  public:  

    BaseController(int _id, Config &cfg) : id(_id)
                                         , num_of_ranks(cfg.num_of_ranks)
                                         , num_of_banks(cfg.num_of_banks)
                                         , clk(0)
    {
        channel = std::make_unique<Array>(Config::Array_Level::Channel, cfg);
        channel->id = _id;

        for (int k = 0; k < 2; k++)
        {
            num_reqs_to_banks[k].resize(num_of_ranks);
            for (int i = 0; i < num_of_ranks; i++)
            {
                num_reqs_to_banks[k][i].resize(num_of_banks);

                for (int j = 0; j < num_of_banks; j++)
                {
                    num_reqs_to_banks[k][i][j] = 0;
                }
            }
        }
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
                    unsigned bank_latency)
    {
        int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
        int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

        channel->postAccess(rank_id, bank_id,
                            channel_latency,
                            bank_latency);
    }

    auto displayReqInfo(auto &req)
    {
        if (req.req_type == Request::Request_Type::READ)
        {
            std::cout << "READ,";
        }
        else
        {
            std::cout << "WRITE,";
        }

        std::cout << "Channel:" << req.addr_vec[int(Config::Decoding::Channel)] << ","
                  << "Rank:" << req.addr_vec[int(Config::Decoding::Rank)] << ","
                  << "Bank:" << req.addr_vec[int(Config::Decoding::Bank)] << ","
                  << "Part:" << req.addr_vec[int(Config::Decoding::Partition)] << ","
                  << "Tile:" << req.addr_vec[int(Config::Decoding::Tile)] << ","
                  << "Row:" << req.addr_vec[int(Config::Decoding::Row)] << ","
                  << "Col:" << req.addr_vec[int(Config::Decoding::Col)] << ",";

        std::cout << req.begin_exe << ","
                  << req.end_exe << ","
                  << (req.end_exe - req.begin_exe) << "\n";
    }
};

class FCFSController : public BaseController
{
  protected:
    const int max = 32; // Max size of each queue
    std::list<Request> readq;
    std::list<Request> writeq;

    bool write_mode = false; // whether write requests should be prioritized over reads
    float wr_high_watermark = 0.8f; // threshold for switching to write mode
    float wr_low_watermark = 0.2f; // threshold for switching back to read mode
    
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
        return readq.size() + writeq.size() + r_w_pending_queue.size();
    }
    
    bool enqueue(Request& req) override
    {
        auto &queue = getQueue(req);
        if (queue.size() == max)
        {
            // Queue is full
            return false;
        }
       
        int rank_id = req.addr_vec[int(Config::Decoding::Rank)];
        int bank_id = req.addr_vec[int(Config::Decoding::Bank)];
        num_reqs_to_banks[int(req.req_type)][rank_id][bank_id]++;

        req.queue_arrival = clk;	
        req.OrderID = queue.size(); // To track back-logging.
	queue.push_back(req);

        return true;
    }

    void tick() override
    {
        clk++;
        channel->update(clk);

        // 1. Serve pending requests
        servePendingAccesses();

        // 2. Determine write/read mode
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() > int(wr_high_watermark * max) || readq.size() == 0)
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() < int(wr_low_watermark * max) && readq.size() != 0)
                write_mode = false;
        }

        // 3. Schedule the request
        auto& queue = !write_mode ? readq : writeq;
        if (auto [scheduled, scheduled_req] = getHead(queue);
            scheduled)
        {
            channelAccess(scheduled_req);

            r_w_pending_queue.push_back(std::move(*scheduled_req));
            queue.erase(scheduled_req);

            // Update back-logging information.
            for (auto &waiting_req : queue)
            {
                --waiting_req.OrderID;
            }
        }
    }

  protected:
    std::list<Request>& getQueue(auto req)
    {
        if (req.req_type == Request::Request_Type::READ)
        {
            return readq;
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            return writeq;
        }
        else
        {
            std::cerr << "Un-supported request type. " << std::endl;
            exit(0);
        }
    }

    // TODO, has to be a loop right?
    void servePendingAccesses()
    {
        if (!r_w_pending_queue.size())
        {
            return;
        }

        Request &req = r_w_pending_queue[0];
        if (req.end_exe <= clk)
        {
            /*
            if (req.req_type == Request::Request_Type::READ)
            {
                std::cout << "R,";
            }
            else
            {
                std::cout << "W,";
            }

            unsigned uni_bank_id = req.addr_vec[int(Config::Decoding::Channel)] *
                                   num_of_ranks * num_of_banks +
                                   req.addr_vec[int(Config::Decoding::Rank)] *
                                   num_of_banks +
                                   req.addr_vec[int(Config::Decoding::Bank)];
            std::cout << uni_bank_id << ","
                      // << req.addr_vec[int(Config::Decoding::Row)] << ","
                      << req.queue_arrival << ","
                      << req.begin_exe << ","
                      << req.end_exe << "\n";
            */
            if (req.callback)
            {
                if (req.callback(req))
                {
                    if (!req.mig)
                    {
                        uint64_t waiting_time = (req.begin_exe - req.queue_arrival);
                        total_waiting_time += waiting_time;
                        ++finished_requests;
                    }

                    int rank_id = req.addr_vec[int(Config::Decoding::Rank)];
                    int bank_id = req.addr_vec[int(Config::Decoding::Bank)];
                    num_reqs_to_banks[int(req.req_type)][rank_id][bank_id]--;

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

                int rank_id = req.addr_vec[int(Config::Decoding::Rank)];
                int bank_id = req.addr_vec[int(Config::Decoding::Bank)];
                num_reqs_to_banks[int(req.req_type)][rank_id][bank_id]--;

                r_w_pending_queue.pop_front();
            }
        }
    }

    virtual std::pair<bool,std::list<Request>::iterator> getHead(std::list<Request>& queue)
    {
        if (queue.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, queue.end());
        }

        auto req = queue.begin();
        if (issueable(req))
        {
            return std::make_pair(true, req);
        }
        return std::make_pair(false, queue.end());
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
            // Program-and-verify scheme.
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
    std::pair<bool,std::list<Request>::iterator> getHead(std::list<Request>& queue) override
    {
        if (queue.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, queue.end());
        }

        for (auto iter = queue.begin(); iter != queue.end(); ++iter)
        {
            if (issueable(iter))
            {
                return std::make_pair(true, iter);
            }
        }
        return std::make_pair(false, queue.end());
    }
};
}

#endif
