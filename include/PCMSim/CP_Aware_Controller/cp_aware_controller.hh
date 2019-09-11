#ifndef __CP_AWARE_CONTROLLER_HH__
#define __CP_AWARE_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

namespace PCMSim
{
class CPAwareController : public FRFCFSController
{
  protected:
    const unsigned num_stages;
    const unsigned num_rows_per_stage;

    const std::vector<Config::Charging_Stage> 
          charging_lookaside_buffer[int(Config::Charge_Pump_Opr::MAX)];

  protected:
    std::vector<uint64_t> stage_accesses[int(Config::Charge_Pump_Opr::MAX)];
    std::vector<uint64_t> stage_total_charging_time[int(Config::Charge_Pump_Opr::MAX)];

  // TODO, tmp modifications for CAL submission
  protected:
    // One for reading and one for writing
    std::vector<unsigned> latency_lookaside_buffer[int(Config::Charge_Pump_Opr::MAX)];
    const float clk_period = 3.76;
    const float read_latencies_ns[8] = {37.6169351, 
                                        37.9677406,
                                        38.5524163,
                                        39.3709623,
                                        40.4233787,
                                        41.7096653,
                                        43.2298222,
                                        44.9838494};

    const float set_latencies_ns[8] = {120.129504,
                                       120.518016,
                                       121.165536,
                                       122.072064,
                                       123.2376,
                                       124.662143,
                                       126.345695,
                                       128.288255};

    const float reset_latencies_ns[8] = {30.1561587,
                                         30.6246347,
                                         31.405428,
                                         32.4985386,
                                         33.9039666,
                                         35.6217119,
                                         37.6517745,
                                         39.9941545};

  public:
    CPAwareController(int _id, Config &cfg)
        : FRFCFSController(_id, cfg),
          num_stages(cfg.num_stages),
          num_rows_per_stage(cfg.num_of_word_lines_per_tile / num_stages),
          charging_lookaside_buffer(cfg.charging_lookaside_buffer)
    {
        assert(charging_lookaside_buffer[int(Config::Charge_Pump_Opr::SET)].size());
        assert(charging_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)].size());
        assert(charging_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)].size());

        for (int i = 0; i < int(Config::Charge_Pump_Opr::MAX); i++)
        {
            stage_accesses[i].resize(num_stages, 0);
            stage_total_charging_time[i].resize(num_stages, 0);
        }

        // TODO, tmp modification for CAL submission
        latency_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)].resize(8);
        latency_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)].resize(8);
        latency_lookaside_buffer[int(Config::Charge_Pump_Opr::SET)].resize(8);

        for (int i = 0; i < 8; i++)
        {
            latency_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)][i] = 
                ceil(read_latencies_ns[i] / clk_period);
//            std::cout << "READ-Stage-" << i << ": " << 
//                latency_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)][i] << "\n";

            latency_lookaside_buffer[int(Config::Charge_Pump_Opr::SET)][i] = 
                ceil(set_latencies_ns[i] / clk_period);
//            std::cout << "SET-Stage-" << i << ": " <<
//                latency_lookaside_buffer[int(Config::Charge_Pump_Opr::SET)][i] << "\n";
            
            latency_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)][i] = 
                ceil(reset_latencies_ns[i] / clk_period);
//            std::cout << "RESET-Stage-" << i << ": " <<
//                latency_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)][i] << "\n\n";
        }
//        exit(0);
    }

    void reInitialize() override
    {
        for (int i = 0; i < int(Config::Charge_Pump_Opr::MAX); i++)
        {
            stage_accesses[i].clear();
            stage_accesses[i].shrink_to_fit();
            stage_total_charging_time[i].clear();
            stage_total_charging_time[i].shrink_to_fit();

            stage_accesses[i].resize(num_stages, 0);
            stage_total_charging_time[i].resize(num_stages, 0);
        }

        BaseController::reInitialize();
    }
    
    unsigned numStages() const
    {
        return num_stages;
    }

    uint64_t stageAccess(int cp_opr, int stage)
    {
        return stage_accesses[cp_opr][stage];
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        // Step one, to determine stage level.
        int row_id = scheduled_req->addr_vec[int(Config::Decoding::Row)];
        unsigned stage_id = row_id / num_rows_per_stage;
        // std::cout << "Stage ID: " << stage_id << "\n";

        // Step two, to determine timings.
        scheduled_req->begin_exe = clk;

        unsigned charging_latency = 0;
        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = 0;

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            ++stage_accesses[int(Config::Charge_Pump_Opr::READ)][stage_id];
            
	    charging_latency = charging_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)]
                                                        [stage_id].nclks_charge_or_discharge;
	    req_latency = charging_latency + singleReadLatency + charging_latency;
            bank_latency = req_latency;
            channel_latency = dataTransferLatency;
            
	    stage_total_charging_time[int(Config::Charge_Pump_Opr::READ)][stage_id] +=
                                     singleReadLatency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            ++stage_accesses[int(Config::Charge_Pump_Opr::SET)][stage_id];
            ++stage_accesses[int(Config::Charge_Pump_Opr::RESET)][stage_id];

            charging_latency = charging_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)]
                                                        [stage_id].nclks_charge_or_discharge;
            req_latency = charging_latency + singleWriteLatency + charging_latency;
            bank_latency = req_latency;
            channel_latency = dataTransferLatency;

            stage_total_charging_time[int(Config::Charge_Pump_Opr::SET)][stage_id] +=
                                     singleWriteLatency;
            stage_total_charging_time[int(Config::Charge_Pump_Opr::RESET)][stage_id] +=
                                     singleWriteLatency;
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
}

#endif
