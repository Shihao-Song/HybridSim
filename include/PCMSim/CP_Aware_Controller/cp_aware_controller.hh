#ifndef __CP_AWARE_CONTROLLER_HH__
#define __CP_AWARE_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

// TODO, the entire class has to be re-written. It should not be called CPAwareController
// anymore.
namespace PCMSim
{
// The TL has been disabled for the pending submission.
class CPAwareController : public FRFCFSController
{
  public:
    enum class Req_Type : int {READ,WRITE,MAX};

  protected:
    // Only consider near and far segments.
    unsigned num_stages = 2;
    // unsigned num_stages = 1;
    const unsigned num_rows_per_stage = 512;

  protected:
    std::vector<uint64_t> stage_accesses[int(Req_Type::MAX)];

  protected:
    // One for reading and one for writing
    std::vector<unsigned> latency_lookaside_buffer[int(Req_Type::MAX)];
    unsigned tRRD;
    
    const float clk_period = 1.0; // For 1GHz memory clock frequency
    const float tRRD_ns = 15.0;

    // const float read_latencies_ns[2] = {27.8, 52.5};
    // const float write_latencies_ns[2] = {27.8, 52.5};

    const float read_latencies_ns[2] = {41.25, 56.25};
    const float write_latencies_ns[2] = {119.75, 161.55};

    bool tl_enable = true;
//    bool tl_enable = false;

  public:
    CPAwareController(int _id, Config &cfg) : FRFCFSController(_id, cfg)
    {
        tRRD = ceil(tRRD_ns / clk_period);

        for (int i = 0; i < int(Req_Type::MAX); i++)
        {
            stage_accesses[i].resize(num_stages, 0);

            latency_lookaside_buffer[i].resize(num_stages, 0);
        }

        for (int i = 0; i < 2; i++)
        {
            latency_lookaside_buffer[int(Req_Type::READ)][i] = 
                ceil(read_latencies_ns[i] / clk_period);
//            std::cout << "READ-Stage-" << i << ": " << 
//                latency_lookaside_buffer[int(Req_Type::READ)][i] << "\n";

            latency_lookaside_buffer[int(Req_Type::WRITE)][i] = 
                ceil(write_latencies_ns[i] / clk_period);
//            std::cout << "WRITE-Stage-" << i << ": " <<
//                latency_lookaside_buffer[int(Req_Type::WRITE)][i] << "\n";
        }
    }

    void disableTL() { tl_enable = false; num_stages = 1; }

    void reInitialize() override
    {
        for (int i = 0; i < int(Req_Type::MAX); i++)
        {
            stage_accesses[i].clear();
            stage_accesses[i].shrink_to_fit();

            stage_accesses[i].resize(num_stages, 0);
        }

        BaseController::reInitialize();
    }
    
    unsigned numStages() const
    {
        return num_stages;
    }

    uint64_t stageAccess(int req_type, int stage)
    {
        return stage_accesses[req_type][stage];
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        /*
        std::cout << scheduled_req->addr << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Rank)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Partition)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Tile)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Row)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Col)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Bank)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Channel)] << "\n";
        exit(0);
        */

        // Step one, to determine stage level.
        int row_id = scheduled_req->addr_vec[int(Config::Decoding::Row)];
//        std::cout << "Row ID: " << row_id << "\n";

        unsigned stage_id = (row_id / num_rows_per_stage >= num_stages - 1)
                              ? num_stages - 1 : row_id / num_rows_per_stage;
//        std::cout << "Stage ID: " << stage_id << "\n";

        // Step two, to determine timings.
        scheduled_req->begin_exe = clk;

        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = tRRD;

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            if (!(scheduled_req->mig))
            {
                ++stage_accesses[int(Req_Type::READ)][stage_id];
            }

            if (tl_enable)
            {
                // std::cerr << "Testing... \n";
                // exit(0);

                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::READ)][stage_id];
            }
            else
            {
                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::READ)][1];
            }

            bank_latency = req_latency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            if (!(scheduled_req->mig))
            {
                ++stage_accesses[int(Req_Type::WRITE)][stage_id];
            }

            if (tl_enable)
            {
                // std::cerr << "Testing... \n";
                // exit(0);

                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::READ)][stage_id] + 
                    latency_lookaside_buffer[int(Req_Type::WRITE)][stage_id];
            }
            else
            {
                req_latency =
                    latency_lookaside_buffer[int(Req_Type::READ)][1] + 
                    latency_lookaside_buffer[int(Req_Type::WRITE)][1];
            }

            bank_latency = req_latency;
        }
        else
        {
            std::cerr << "Unknown Request Type. \n";
            exit(0);
        }

//        std::cout << "Req latency: " << req_latency << "\n";
//        std::cout << "Channel latency: " << channel_latency << "\n\n";
        scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

        // Post access
        postAccess(scheduled_req,
                   channel_latency,
                   bank_latency);
    }
};

class TLDRAMController : public FRFCFSController
{
  public:
    enum class Req_Type : int {READ,WRITE,MAX};

  protected:
    // Only consider near and far segments.
    unsigned num_stages = 2;
//    const unsigned num_stages = 1;
    const unsigned num_rows_per_stage = 128;

  protected:
    std::vector<uint64_t> stage_accesses[int(Req_Type::MAX)];

  protected:
    // One for reading and one for writing
    std::vector<unsigned> latency_lookaside_buffer[int(Req_Type::MAX)];
    unsigned tRRD;
    
    const float clk_period = 1.0; // For 1GHz memory clock frequency
    const float tRRD_ns = 15.0;

    const float read_latencies_ns[2] = {27.8, 27.8};
    const float write_latencies_ns[2] = {27.8, 27.8}; 
    // const float read_latencies_ns[2] = {27.8, 52.5};
    // const float write_latencies_ns[2] = {27.8, 52.5};

    bool tl_enable = true;

  public:
    TLDRAMController(int _id, Config &cfg) : FRFCFSController(_id, cfg)
    {
        tRRD = ceil(tRRD_ns / clk_period);

        for (int i = 0; i < int(Req_Type::MAX); i++)
        {
            stage_accesses[i].resize(num_stages, 0);

            latency_lookaside_buffer[i].resize(num_stages, 0);
        }

        for (int i = 0; i < 2; i++)
        {
            latency_lookaside_buffer[int(Req_Type::READ)][i] = 
                ceil(read_latencies_ns[i] / clk_period);
//            std::cout << "READ-Stage-" << i << ": " << 
//                latency_lookaside_buffer[int(Req_Type::READ)][i] << "\n";

            latency_lookaside_buffer[int(Req_Type::WRITE)][i] = 
                ceil(write_latencies_ns[i] / clk_period);
//            std::cout << "WRITE-Stage-" << i << ": " <<
//                latency_lookaside_buffer[int(Req_Type::WRITE)][i] << "\n";
        }
    }

    void disableTL() { tl_enable = false; num_stages = 1; }

    void reInitialize() override
    {
        for (int i = 0; i < int(Req_Type::MAX); i++)
        {
            stage_accesses[i].clear();
            stage_accesses[i].shrink_to_fit();

            stage_accesses[i].resize(num_stages, 0);
        }

        BaseController::reInitialize();
    }
    
    unsigned numStages() const
    {
        return num_stages;
    }

    uint64_t stageAccess(int req_type, int stage)
    {
        return stage_accesses[req_type][stage];
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        /*
        std::cout << scheduled_req->addr << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Rank)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Partition)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Tile)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Row)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Col)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Bank)] << "\n";
        std::cout << scheduled_req->addr_vec[int(Config::Decoding::Channel)] << "\n";
        exit(0);
        */

        // Step one, to determine stage level.
        int row_id = scheduled_req->addr_vec[int(Config::Decoding::Row)];
//        std::cout << "Row ID: " << row_id << "\n";

        unsigned stage_id = (row_id / num_rows_per_stage >= num_stages - 1)
                              ? num_stages - 1 : row_id / num_rows_per_stage;
//        std::cout << "Stage ID: " << stage_id << "\n";

        // Step two, to determine timings.
        scheduled_req->begin_exe = clk;

        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = tRRD;

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            if (!(scheduled_req->mig))
	    {
                ++stage_accesses[int(Req_Type::READ)][stage_id];
            }

            if (tl_enable)
            {
                // std::cerr << "Testing... \n";
                // exit(0);

                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::READ)][stage_id];
            }
            else
            {
                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::READ)][1];
            }

            bank_latency = req_latency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            if (!(scheduled_req->mig))
	    {
                ++stage_accesses[int(Req_Type::WRITE)][stage_id];
            }

            if (tl_enable)
            {
                // std::cerr << "Testing... \n";
                // exit(0);

                req_latency = 
                    latency_lookaside_buffer[int(Req_Type::WRITE)][stage_id];
            }
            else
            {
                req_latency =
                    latency_lookaside_buffer[int(Req_Type::WRITE)][1];
            }

            bank_latency = req_latency;
        }
        else
        {
            std::cerr << "Unknown Request Type. \n";
            exit(0);
        }

//        std::cout << "Req latency: " << req_latency << "\n";
//        std::cout << "Channel latency: " << channel_latency << "\n\n";
        scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

        // Post access
        postAccess(scheduled_req,
                   channel_latency,
                   bank_latency);
    }
};
}

#endif
