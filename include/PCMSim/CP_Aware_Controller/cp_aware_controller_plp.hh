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
                
        // Determine charging latency and operation latency
        unsigned charging_latency = 0;
        unsigned opr_latency = 0;

        if (scheduled_req->master == 1)
        {
            if (scheduled_req->pair_type == PLPRequest::Pairing_Type::RR)
            {
                // In this case, the opreration latency is readWithRead
                opr_latency = readWithReadLatency;
                // The charging latency is determined by the highest stage
                unsigned charging_stage = getHighestStageID(scheduled_req);
                charging_latency =
                    charging_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)]
                                             [charging_stage].nclks_charge_or_discharge;
            }
            else if (scheduled_req->pair_type == PLPRequest::Pairing_Type::RW)
            {
                // In this case, the operation latency is readWhileWriteLatency.
                opr_latency = readWhileWriteLatency;
                // The charging latency is determined by the WRITE request
                int charging_stage = -1;
                if (scheduled_req->req_type == Request::Request_Type::WRITE)
                {
                    charging_stage = getStageID(scheduled_req);
                }
                else if (scheduled_req->slave_req->req_type == Request::Request_Type::WRITE)
                {
                    charging_stage = getStageID(scheduled_req->slave_req);
                }
                charging_latency = 
                    charging_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)]
                                       [charging_stage].nclks_charge_or_discharge;
            }
        }
        else
	{
            int charging_stage = getStageID(scheduled_req);
            if (scheduled_req->req_type == Request::Request_Type::READ)
            {
                opr_latency = singleReadLatency;
                charging_latency =
                    charging_lookaside_buffer[int(Config::Charge_Pump_Opr::READ)]
                                             [charging_stage].nclks_charge_or_discharge;
            }
            else if (scheduled_req->req_type == Request::Request_Type::WRITE)
            {
                opr_latency = singleWriteLatency;
                charging_latency =
                    charging_lookaside_buffer[int(Config::Charge_Pump_Opr::RESET)]
                                       [charging_stage].nclks_charge_or_discharge;
            }
	}

        unsigned req_latency = 0;
        unsigned bank_latency = 0;
        unsigned channel_latency = 0;

        req_latency = setLatencies(scheduled_req,
                                   opr_latency,
                                   charging_latency);
        if (scheduled_req->master == 1)
        {
            setLatencies(scheduled_req->slave_req,
                         opr_latency,
                         charging_latency);
        }

        bank_latency = req_latency;
        channel_latency = dataTransferLatency;

        return std::make_tuple(req_latency, bank_latency, channel_latency);
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

    unsigned getHighestStageID(auto scheduled_req) const
    {
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

    // Return back the request latency.
    Tick setLatencies(auto scheduled_req,
                      unsigned opr_latency,
                      unsigned charging_latency)
    {
        int req_stage = getStageID(scheduled_req);

        unsigned req_latency = 0;

        scheduled_req->begin_exe = clk;
        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            // stage access is determined by the request stage.
            ++stage_accesses[int(Config::Charge_Pump_Opr::READ)][req_stage];
            
	    req_latency = charging_latency + opr_latency + charging_latency;
           
            // stage charging time is determined by the request stage.
	    stage_total_charging_time[int(Config::Charge_Pump_Opr::READ)][req_stage] +=
                                     singleReadLatency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            // stage access is determined by the request stage.
            ++stage_accesses[int(Config::Charge_Pump_Opr::SET)][req_stage];
            ++stage_accesses[int(Config::Charge_Pump_Opr::RESET)][req_stage];

            req_latency = charging_latency + opr_latency + charging_latency;

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
