#ifndef __LASER_HH__
#define __LASER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

#include <unordered_map>

namespace PCMSim
{
struct CP_STATIC{};
struct LASER_1{};
struct LASER_2{};

template<typename Scheduler>
class LASER : public FCFSController
{
  protected:
    enum class CP_Type : int { RCP, WCP, MAX };

    const int back_logging_threshold = -16; // Only for critical reads
    
    // Charge pump status for each bank (two charge pumps per bank).
    enum class CP_Status : int
    {
        RCP_ON, // Only read charge pump is ON
        WCP_ON, // Only write charge pump is ON
        BOTH_ON, // Both pumps are ON
        BOTH_OFF, // Both pumps are OFF
        MAX // Invalid
    };

    // Time to charge/discharge the write charge pump
    // Initialized in constructor
    const unsigned nclks_wcp;
    // Time to charge/discharge the read charge pump
    // Initialized in constructor
    const unsigned nclks_rcp;

    // status table keeps track of charge pump status, e.g., which pump is ON.
    struct Status_Entry
    {
        CP_Status cp_status; // Read CP is on? Write CP is on? Both?
    };
    // One record for each bank
    std::vector<std::vector<Status_Entry>> sTab;

    // working table keeps track of how many clock cycles a bank have worked.
    struct Working_Entry
    {
        Tick working;
    };
    // One record for each bank
    std::vector<std::vector<Working_Entry>> wTab;

    // idle table keeps track of how many clock cycles a bank have stayed idle.
    struct Idle_Entry
    {
        Tick idle; // This is the total idle

        Tick rcp_idle; // Only for RCP
        Tick wcp_idle; // Only for WCP
    };
    // One record for each bank
    std::vector<std::vector<Idle_Entry>> iTab;

    // request table keeps track of the number of requests have been served by
    // each bank
    struct Request_Record
    {
        unsigned num_of_reads;
        unsigned num_of_writes;
    };
    std::vector<std::vector<Request_Record>> rTab; // request table

    struct CP_Record
    {
        Tick read_cp_begin_charging;
        Tick read_cp_end_charging;

        Tick write_cp_begin_charging;
        Tick write_cp_end_charging;
    };
    std::vector<std::vector<CP_Record>> cpTab; // charge pump table


  public:
    LASER(int _id, Config &cfg)
        : FCFSController(_id, cfg)
        // I take 1/10 of the read/write latency as the read/write pump charging/discharging
        // time.
        , nclks_wcp(singleWriteLatency * 0.2)
        , nclks_rcp(singleReadLatency * 0.2)
    {
        // status table keeps track of charge pump status, e.g., which pump is ON.
        // For example, to serve a read request, a read charge pump has to be charged.
        sTab.resize(num_of_ranks);
        // working table keeps track of how many clock cycles a bank have worked.
        wTab.resize(num_of_ranks);
        // idle table keeps track of how many clock cycles a bank have stayed idle.
        iTab.resize(num_of_ranks);
        // request table keeps track of the number of requests have been served by 
        // the bank.
        rTab.resize(num_of_ranks);
        // charge pump table keeps track of when pumps charge/discharge.
        cpTab.resize(num_of_ranks);

        for (int i = 0; i < num_of_ranks; i++)
        {
            sTab[i].resize(num_of_banks);
            wTab[i].resize(num_of_banks);
            iTab[i].resize(num_of_banks);
            rTab[i].resize(num_of_banks);
            cpTab[i].resize(num_of_banks);

            for (int j = 0; j < num_of_banks; j++)
            {
                // Initially, all the charge pumps are off.
                sTab[i][j].cp_status = CP_Status::BOTH_OFF;

                // Initialize aging record
                wTab[i][j].working = 0;

                // Initialize idle record
                iTab[i][j].idle = 0; // Only keep one idle tick per bank.
                iTab[i][j].rcp_idle = 0;
                iTab[i][j].wcp_idle = 0;

                // Initialize request record
                rTab[i][j].num_of_reads = 0;
                rTab[i][j].num_of_writes = 0;
            }
        }
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
            if (writeq.size() > int(wr_high_watermark * max) || (readq.size() == 0 && writeq.size() != 0))
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() < int(wr_low_watermark * max) && readq.size() != 0)
                write_mode = false;
        }
	// 3. Update xTab information at tick level (fine-grained control).
        tableUpdate();
        dischargeOpenBanks();

        // 4. Schedule the request
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
    std::pair<bool,std::list<Request>::iterator> getHead(std::list<Request>& queue) override
    {
        if (queue.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, queue.end());
        }

        // For CP_STATIC, we always pick the first issueable request
        if constexpr (std::is_same<CP_STATIC, Scheduler>::value)
        {
            auto req = queue.begin();
            if (issueable(req))
            {
                return std::make_pair(true, req);
            }
            return std::make_pair(false, queue.end());
        }
        
        if constexpr (std::is_same<LASER_1, Scheduler>::value ||
                      std::is_same<LASER_2, Scheduler>::value)
        {
            // Step one: make sure the oldest read request is not waiting too long. 
            auto oldest_req = queue.begin();
            if (oldest_req->OrderID <= back_logging_threshold && 
                write_mode == false)
            {
                if (issueable(oldest_req))
                {
                    return std::make_pair(true, oldest_req);
                }
                return std::make_pair(false, queue.end());
            }

            // Step two: find an open-bank. 
            // Current selection policy:
            // (1) A free bank;
            // (2) Bank's peripheral ciruit is open;
            // (3) If there are more than one bank obeys (1) and (2), select the bank
            // that stays idle for the longest.
            int most_idle = -1;
            auto most_idle_req = queue.begin();
            for (auto q_iter = queue.begin(); q_iter != queue.end(); q_iter++)
            {
                int target_rank = (q_iter->addr_vec)[int(Config::Decoding::Rank)];
                int target_bank = (q_iter->addr_vec)[int(Config::Decoding::Bank)];

                if (q_iter->req_type == Request::Request_Type::READ)
                {
                    if (sTab[target_rank][target_bank].cp_status == CP_Status::RCP_ON || 
                        sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_ON)
                    {
                        // issueable() tells if the bank is free or not
                        if (issueable(q_iter))
                        {
                            int bank_idle = iTab[target_rank][target_bank].idle;
                            if (most_idle == -1)
                            {
                                most_idle = bank_idle;
                                most_idle_req = q_iter;
                            }
                            else
                            {
                                if (bank_idle > most_idle)
                                {
                                    most_idle = bank_idle;
                                    most_idle_req = q_iter;
                                }
                           }
                        }
                    }
                }

                if (q_iter->req_type == Request::Request_Type::WRITE)
                {
                    // When serving a write request, both pumps have to be ON.
                    if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_ON)
                    {
                        // issueable() tells if the bank is free or not
                        if (issueable(q_iter))
                        {
                            int bank_idle = iTab[target_rank][target_bank].idle;
                            if (most_idle == -1)
                            {
                                most_idle = bank_idle;
                                most_idle_req = q_iter;
                            }
                            else
                            {
                                if (bank_idle > most_idle)
                                {
                                    most_idle = bank_idle;
                                    most_idle_req = q_iter;
                                }
                           }
                        }
                    }
                }
            }
            if (most_idle != -1)
            {
                assert(issueable(most_idle_req));
                return std::make_pair(true, most_idle_req);
            }

            auto req = queue.begin();
            if (issueable(req))
            {
                return std::make_pair(true, req);
            }
            return std::make_pair(false, queue.end());
        }
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        scheduled_req->begin_exe = clk;

        // Step one, determine the charging latency and update charge pump status.
        unsigned charging_latency = 0;
        int target_rank = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
        int target_bank = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

        // For CP-Static and LASER_1, both pump operate in parallel.
        if constexpr (std::is_same<CP_STATIC, Scheduler>::value || 
                      std::is_same<LASER_1, Scheduler>::value)
        {
            if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_OFF)
            {
                sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;
                // Both pumps charge at the same time so the time to charge write pump
                // dominates the preparation time.
                charging_latency = nclks_wcp;

                cpTab[target_rank][target_bank].write_cp_begin_charging = clk;
                cpTab[target_rank][target_bank].write_cp_end_charging = clk + 
                                                                        charging_latency;
            }
            // Both pumps has to be on at this stage.
            assert(sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_ON);
        }

        // For LASER2, individual pump can operate in parallel.
        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            if constexpr (std::is_same<LASER_2, Scheduler>::value)
            {
                // if (!write_mode)
                //     assert(sTab[target_rank][target_bank].cp_status != CP_Status::BOTH_ON);

                // If both charge pumps are OFF, turn on the read charge pump.
                // Now, only the read charge pump is ON.
                if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_OFF)
                {
                    sTab[target_rank][target_bank].cp_status = CP_Status::RCP_ON;

                    // The latency can be hidden by switching to the write mode
                    charging_latency = 0;
                    // charging_latency = nclks_rcp;

                    // Record charging time
                    cpTab[target_rank][target_bank].read_cp_begin_charging = clk;
                    cpTab[target_rank][target_bank].read_cp_end_charging = clk + 
                                                                        nclks_rcp;
                }
		else if (sTab[target_rank][target_bank].cp_status == CP_Status::WCP_ON)
                {
                    sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;

                    // The latency can be hidden by switching to the write mode
                    charging_latency = 0;
                    // charging_latency = nclks_rcp;

                    // Record charging time
                    cpTab[target_rank][target_bank].read_cp_begin_charging = clk;
                    cpTab[target_rank][target_bank].read_cp_end_charging = clk + 
                                                                        nclks_rcp;
                }
            }

            // Record a new read request.
            rTab[target_rank][target_bank].num_of_reads++;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            // No charging latency for WCP since it can overlap with the read-write mode
            // switching
            if constexpr (std::is_same<LASER_2, Scheduler>::value)
            {
                charging_latency = 0;

                // To serve a write request, all pumps must be ON.
                if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_OFF)
                {
                    sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;
                    cpTab[target_rank][target_bank].write_cp_begin_charging = clk;
                    cpTab[target_rank][target_bank].write_cp_end_charging = clk + nclks_wcp; 
                }
                // If only the read charge pump is ON, turn on the write charge pump.
                // Now, both charge pumps are ON.
                else if (sTab[target_rank][target_bank].cp_status == CP_Status::RCP_ON)
                {
                    sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;
                    cpTab[target_rank][target_bank].write_cp_begin_charging = clk;
                    cpTab[target_rank][target_bank].write_cp_end_charging = clk + nclks_wcp; 
                }
                else if (sTab[target_rank][target_bank].cp_status == CP_Status::WCP_ON)
                {
		    sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;
                    cpTab[target_rank][target_bank].write_cp_begin_charging = clk;
                    cpTab[target_rank][target_bank].write_cp_end_charging = clk + nclks_rcp; 
                }
            }
            
            // Record a write request.
            rTab[target_rank][target_bank].num_of_writes++;
        }
        
        unsigned req_latency = charging_latency;
        unsigned bank_latency = 0;
        unsigned channel_latency = 0;

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            req_latency += singleReadLatency;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            req_latency += singleWriteLatency;
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

  protected:
    void tableUpdate()
    {
        for (int i = 0; i < num_of_ranks; i++)
        {
            for (int j = 0; j < num_of_banks; j++)
            {
                // Step one, update read charge pump.
                if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                    sTab[i][j].cp_status == CP_Status::WCP_ON ||
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    if(!channel->isBankFree(i,j))
                    {
                        ++wTab[i][j].working;
                    }
                    else
                    {
                        ++iTab[i][j].idle;

                        if (sTab[i][j].cp_status == CP_Status::WCP_ON)
                            ++iTab[i][j].wcp_idle;
                        if (sTab[i][j].cp_status == CP_Status::RCP_ON)
                            ++iTab[i][j].rcp_idle;
                    }
                }
            }
        }
    }

    void dischargeOpenBanks()
    {
        if constexpr (std::is_same<LASER_1, Scheduler>::value)
        {
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    if(sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        unsigned total_idle = iTab[i][j].idle; 

                        unsigned num_of_reads_done = rTab[i][j].num_of_reads;
                        unsigned num_of_writes_done = rTab[i][j].num_of_writes;

                        double ps_aging = 580.95 * (double)num_of_writes_done +
                                          0.03 * (double)total_idle;

                        double sa_aging = 59.63 * (double)num_of_reads_done +
                                          0.03 * (double)total_idle;

			// Discharge because of aging
                        if (ps_aging > 1000.0 ||
                            sa_aging > 1000.0)
                        {
                            dischargeSingleBank(i, j);
                        }
                        else // no aging exceeds
                        {
                            // Discharge because of no more requests
                            if (num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] 
                                == 0 &&
                                num_reqs_to_banks[int(Request::Request_Type::READ)][i][j]
                                == 0)
                            {
                                dischargeSingleBank(i, j);
                            }
                        }
                    }
                }
            }
        }

        // Write charge pump has no discharging latency in any situations.
        // Also, write charge pumps can be pre-charged before switching operation mode.
        // Read charge pump has no discharging latency only in write mode.
        if constexpr (std::is_same<LASER_2, Scheduler>::value)
        {
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    unsigned wcp_idle = iTab[i][j].wcp_idle;
                    unsigned rcp_idle = iTab[i][j].rcp_idle;

                    unsigned num_of_reads_done = rTab[i][j].num_of_reads;
                    unsigned num_of_writes_done = rTab[i][j].num_of_writes;

                    double ps_aging = 580.95 * (double)num_of_writes_done +
                                      0.03 * (double)wcp_idle;

                    double sa_aging = 59.63 * (double)num_of_reads_done +
                                      0.03 * (double)rcp_idle;

                    // Discharge write charge pump
                    if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        // If aging exceeds or no more writes to this bank.
                        if (ps_aging > 1000.0 || 
                            num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] == 0) 
                        {
                            dischargeSingleCP(CP_Type::WCP, i, j);
                        }
	            }

                    // Discharge read charge pumps
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        // If aging exceeds or no more reads/writes to this bank
			if (sa_aging > 1000.0 ||
                            (num_reqs_to_banks[int(Request::Request_Type::READ)][i][j] == 0 &&
                             num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] == 0)) 
                        {
                            dischargeSingleCP(CP_Type::RCP, i, j);
                        }
                    }
                }
            }
        }

        // Important for CP-Static, the Pumps have to back on if there are still requests
        // left in the queue.
        if constexpr (std::is_same<CP_STATIC, Scheduler>::value)
	{
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    if (sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        if (rTab[i][j].num_of_writes > 0)
                        {
                            // Per-write discharge
                            dischargeSingleBank(i, j);
                        }
                        else
                        {
                            int total_aging = wTab[i][j].working + iTab[i][j].idle;
                            if (total_aging >= 1000)
                            {
                                dischargeSingleBank(i, j);
                            }
                        }
                    }
                }
            }
        }
    }

    void dischargeSingleCP(CP_Type cp_type, int rank_id, int bank_id)
    {
        if (channel->isBankFree(rank_id, bank_id))
        {
            recordCPInfo(cp_type, rank_id, bank_id);

            if (cp_type == CP_Type::WCP)
            {
                if (sTab[rank_id][bank_id].cp_status == CP_Status::WCP_ON)
                {
                    sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;
                }
                else if (sTab[rank_id][bank_id].cp_status == CP_Status::BOTH_ON)
                {
                    sTab[rank_id][bank_id].cp_status = CP_Status::RCP_ON;
                }

                wTab[rank_id][bank_id].working = 0;

                iTab[rank_id][bank_id].wcp_idle = 0;

                rTab[rank_id][bank_id].num_of_writes = 0;
            }

            if (cp_type == CP_Type::RCP)
            {
                if (sTab[rank_id][bank_id].cp_status == CP_Status::RCP_ON)
                {
                    sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;
                }
                else if (sTab[rank_id][bank_id].cp_status == CP_Status::BOTH_ON)
                {
                    sTab[rank_id][bank_id].cp_status = CP_Status::WCP_ON;
                }

                wTab[rank_id][bank_id].working = 0;

                iTab[rank_id][bank_id].rcp_idle = 0;

                rTab[rank_id][bank_id].num_of_reads = 0;
            }
	}
    }

    // Discharge all charge pumps in a bank (read and write charge pump must happen
    // in parallel)
    void dischargeSingleBank(int rank_id, int bank_id)
    {
        // Make sure the bank is free (not serving any request)
        if (channel->isBankFree(rank_id, bank_id))
        {
            recordCPInfo(CP_Type::MAX, rank_id, bank_id);

            Tick discharging_latency = 10; // Give all pumps 10 extra cycles to de-stress
            discharging_latency += nclks_wcp; // Discharge both pumps, use write pump
                                              // discharging latency.
            
            // Reset the timings (The following aging has already been considered)
	    wTab[rank_id][bank_id].working = 0;
            iTab[rank_id][bank_id].idle = 0;

            rTab[rank_id][bank_id].num_of_reads = 0;
            rTab[rank_id][bank_id].num_of_writes = 0;

            channel->addBankLatency(rank_id, bank_id, discharging_latency);

            sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;
        }
    }

    void recordCPInfo(CP_Type cp_type, int rank_id, int bank_id)
    {
        Tick begin_charging = 0;
        Tick end_charging = 0;
        Tick begin_discharging = 0;
        Tick end_discharging = 0;

        if (cp_type == CP_Type::RCP)
        {
            begin_charging = cpTab[rank_id][bank_id].read_cp_begin_charging;
            end_charging = cpTab[rank_id][bank_id].read_cp_end_charging;
            begin_discharging = clk;
            end_discharging = begin_discharging + nclks_rcp;

            Tick on_time = begin_discharging - end_charging;
            max_on_time = (max_on_time < on_time) ? on_time : max_on_time;
            min_on_time = (min_on_time > on_time) ? on_time : min_on_time;
	    
            unsigned num_of_reads_done = rTab[rank_id][bank_id].num_of_reads;
            unsigned idle = iTab[rank_id][bank_id].rcp_idle;

            double aging = 59.63 * (double)num_of_reads_done +
                           0.03 * (double)idle;

            if (max_aging < aging) {max_aging = aging;}

            return;
        }
        else if (cp_type == CP_Type::WCP)
        {
            begin_charging = cpTab[rank_id][bank_id].write_cp_begin_charging;
            end_charging = cpTab[rank_id][bank_id].write_cp_end_charging;
            begin_discharging = clk;
            end_discharging = begin_discharging + nclks_wcp;
	    
            Tick on_time = begin_discharging - end_charging;
            max_on_time = (max_on_time < on_time) ? on_time : max_on_time;
            min_on_time = (min_on_time > on_time) ? on_time : min_on_time;

            unsigned num_of_writes_done = rTab[rank_id][bank_id].num_of_writes;
            unsigned idle = iTab[rank_id][bank_id].wcp_idle;
            
            double aging = 580.95 * (double)num_of_writes_done +
                           0.03 * (double)idle;

            if (max_aging < aging) {max_aging = aging;}

            return;
        }


        begin_charging = cpTab[rank_id][bank_id].write_cp_begin_charging;
        end_charging = cpTab[rank_id][bank_id].write_cp_end_charging;
        begin_discharging = clk;
        end_discharging = begin_discharging + nclks_wcp;

        Tick on_time = begin_discharging - end_charging;
        max_on_time = (max_on_time < on_time) ? on_time : max_on_time;
        min_on_time = (min_on_time > on_time) ? on_time : min_on_time;
        
        unsigned idle = iTab[rank_id][bank_id].idle;

        unsigned num_of_reads_done = rTab[rank_id][bank_id].num_of_reads;
        unsigned num_of_writes_done = rTab[rank_id][bank_id].num_of_writes;

        double ps_aging = 580.95 * (double)num_of_writes_done +
                          0.03 * (double)idle;
        double aging = ps_aging;

        double vl_aging = 171.26 * (double)num_of_writes_done + 
                          0.03 * (double)idle;
        if (vl_aging > aging) { aging = vl_aging; }

        double sa_aging = 59.63 * (double)num_of_reads_done +
                          0.03 * (double)idle;
        if (sa_aging > aging) { aging = sa_aging; }

        if (max_aging < aging) {max_aging = aging;}
    }

  protected:
    bool offline_cp_analysis_mode = false;
    std::ofstream *offline_cp_ana_output;

  public:
    virtual void offlineCPAnalysis(std::ofstream *out)
    {
        offline_cp_analysis_mode = true;
        offline_cp_ana_output = out;
    }

  // Stats
  public:
    float max_aging = 0.0;
    Tick max_on_time = 0;
    Tick min_on_time = (Tick) - 1;
};

typedef LASER<LASER_2> LASER_2_Controller;

typedef LASER<LASER_1> LASER_1_Controller;

typedef LASER<CP_STATIC> CP_Static;
}

#endif
