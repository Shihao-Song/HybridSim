#ifndef __CP_AWARE_PLP_CONTROLLER_HH__
#define __CP_AWARE_PLP_CONTROLLER_HH__

#include "PCMSim/PLP_Controller/pcm_sim_plp_controller.hh"

namespace PCMSim
{
class PLPCPAwareController : public PLPController
{
  protected:
    const unsigned num_stages;

    const unsigned num_partitions_per_bank;
    const unsigned num_rows_per_partition;
    const unsigned num_rows_per_stage;

    const std::vector<Config::Charging_Stage> 
          charging_lookaside_buffer[int(Config::Charge_Pump_Opr::MAX)];

  protected:
    std::vector<uint64_t> stage_accesses[int(Config::Charge_Pump_Opr::MAX)];
    std::vector<uint64_t> stage_total_charging_time[int(Config::Charge_Pump_Opr::MAX)];

  public:
    PLPCPAwareController(int _id, Config &cfg)
        : PLPController(_id, cfg),
          num_stages(cfg.num_stages),
          num_partitions_per_bank(cfg.num_of_parts),
          num_rows_per_partition(cfg.num_of_word_lines_per_tile),
          num_rows_per_stage(num_rows_per_partition * num_partitions_per_bank /
                             num_stages),
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

  protected:
    std::tuple<unsigned,unsigned,unsigned>
            getLatencies(std::list<PLPRequest>::iterator& scheduled_req) override
    {
        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = 0;
    
        // Determine charging stage
        unsigned charging_stage = getStageID(scheduled_req);
        if (scheduled_req->master == 1)
        {
            if (unsigned slave_stage = getStageID(scheduled_req->slave_req);
                slave_stage > charging_stage)
            {
                charging_stage = slave_stage;
            }
        }
    }

  protected:
    unsigned getStageID(auto scheduled_req) const
    {
        int part_id = scheduled_req->addr_vec[int(Config::Decoding::Partition)];
        int row_id = scheduled_req->addr_vec[int(Config::Decoding::Row)];

        unsigned stage_id = (part_id * num_rows_per_partition + row_id)
                            / num_rows_per_stage;

        return stage_id;
    }

    // Return back the request latency.
    Tick setLatencies(auto scheduled_req,
                      Tick begin_exe,
                      unsigned req_stage,
                      unsigned charging_stage)
    {
        unsigned req_latency = 0;
        unsigned charging_latency = 0;

        scheduled_req->begin_exe = clk;
        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            // stage access is determined by the request stage.
            ++stage_accesses[int(Config::Charge_Pump_Opr::READ)][req_stage];
            
            // charging latency is determined by the charging stage.
	    charging_latency = charging_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)]
                                                  [charging_stage].nclks_charge_or_discharge;
	    req_latency = charging_latency + singleReadLatency + charging_latency;
           
            // stage charging time is determined by the request stage.
	    stage_total_charging_time[int(Config::Charge_Pump_Opr::READ)][req_stage] +=
                                     singleReadLatency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            // stage access is determined by the request stage.
            ++stage_accesses[int(Config::Charge_Pump_Opr::SET)][req_stage];
            ++stage_accesses[int(Config::Charge_Pump_Opr::RESET)][req_stage];

            // charging latency is determined by the charging stage
            charging_latency = charging_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)]
                                                  [charging_stage].nclks_charge_or_discharge;

            req_latency = charging_latency + singleWriteLatency + charging_latency;

            // stage charging time is determined by the request stage.
            stage_total_charging_time[int(Config::Charge_Pump_Opr::SET)][req_stage] +=
                                     singleWriteLatency;
            stage_total_charging_time[int(Config::Charge_Pump_Opr::RESET)][req_stage] +=
                                     singleWriteLatency;
        }
        else
        {
            std::cerr << "Unknown Request Type. \n";
            exit(0);
        }

        scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;
        return req_latency;
    }
    /*
    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        // Step one, to determine stage level.
        int part_id = scheduled_req->addr_vec[int(Config::Decoding::Partition)];
        int row_id = scheduled_req->addr_vec[int(Config::Decoding::Row)];
        unsigned stage_id = (part_id * num_rows_per_partition + row_id) /
                            num_rows_per_stage;
	
        // Step two, to determine timings.
        
                // Post access
        postAccess(scheduled_req,
                   channel_latency,
                   req_latency, // This is rank latency for other ranks.
                                // Since there is no rank-level parall,
                                // other ranks must wait until the current rank
                                // to be fully de-coupled.
                   bank_latency);
    }
    */
};
}

#endif
