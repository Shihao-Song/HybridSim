#ifndef __LAS_PCM_CONTROLLER_HH__
#define __LAS_PCM_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

#include <unordered_map>

namespace PCMSim
{
// TODO, limitation, only 1-stage charging is supported so far.
// To fully integrate LAS-PCM with multiple charging and PALP, we may need to change 
// the existing architecture (Future Work).
// FCFS_OR_FRFCFS - Should our scheduler be fairness-centric (FCFS) or
//                  performance-centric (FRFCFS)
// Scheduler - LAS-PCM? Base? CP-Static?
struct FCFS{};
struct FRFCFS{};

struct BASE{};
struct CP_STATIC{};
struct LAS_PCM{};
struct LASER{};
struct LASER_2{};

template<typename FCFS_OR_FRFCFS, typename Scheduler>
class LASPCM : public FCFSController
{
  public:
    LASPCM(int _id, Config &cfg)
        : FCFSController(_id, cfg)
        , nclks_wcp(singleWriteLatency * 0.1)
        , nclks_rcp(singleReadLatency * 0.1)
    {
        sTab.resize(num_of_ranks);
        aTab.resize(num_of_ranks);
        iTab.resize(num_of_ranks);
        rTab.resize(num_of_ranks);

        for (int i = 0; i < num_of_ranks; i++)
        {
            sTab[i].resize(num_of_banks);
            aTab[i].resize(num_of_banks);
            iTab[i].resize(num_of_banks);
            rTab[i].resize(num_of_banks);

            for (int j = 0; j < num_of_banks; j++)
            {
                // Initially, all the charge pumps are off.
                sTab[i][j].cp_status = CP_Status::BOTH_OFF;
                // Initially, none of the charge pump is busy.
                sTab[i][j].cur_busy_cp = CP_Type::MAX;

                aTab[i][j].aging.resize(int(CP_Type::MAX), 0);

                iTab[i][j].idle.resize(int(CP_Type::MAX), 0);

                rTab[i][j].num_of_reads = 0;
                rTab[i][j].num_of_writes = 0;
            }
        }
    }

    void tick() override
    {
        clk++;
        channel->update(clk);
        // Update xTab information at tick level (fine-grained control).
        tableUpdate();
        dischargeOpenBanks();

        servePendingAccesses();

        if (auto [scheduled, scheduled_req] = getHead();
            scheduled)
        {
            channelAccess(scheduled_req);

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
    std::pair<bool,std::list<Request>::iterator> getHead() override
    {
        if (r_w_q.size() == 0)
        {
            // Queue is empty, nothing to be scheduled.
            return std::make_pair(false, r_w_q.end());
        }

        if constexpr (std::is_same<BASE, Scheduler>::value || 
                      std::is_same<CP_STATIC, Scheduler>::value)
        {
            auto req = r_w_q.begin();
            if (issueable(req))
            {
                return std::make_pair(true, req);
            }
            return std::make_pair(false, r_w_q.end());
        }
        
        if constexpr (std::is_same<LAS_PCM, Scheduler>::value || 
                      std::is_same<LASER, Scheduler>::value ||
                      std::is_same<LASER_2, Scheduler>::value)
        {
            // Step one: make sure the oldest request is not waiting too long. 
            auto req = r_w_q.begin();
            if (req->OrderID <= back_logging_threshold)
            {
                if (issueable(req))
                {
                    return std::make_pair(true, req);
                }
                return std::make_pair(false, r_w_q.end());
            }

            // Step two: find an open-bank
            int most_idle = -1;
            auto most_idle_req = r_w_q.begin();
            for (auto q_iter = r_w_q.begin(); q_iter != r_w_q.end(); q_iter++)
            {
                int target_rank = (q_iter->addr_vec)[int(Config::Decoding::Rank)];
                int target_bank = (q_iter->addr_vec)[int(Config::Decoding::Bank)];

                if (q_iter->req_type == Request::Request_Type::READ)
                {
                    if (sTab[target_rank][target_bank].cp_status == CP_Status::RCP_ON || 
                        sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_ON)
                    {
                        if (issueable(q_iter))
                        {
                            int bank_idle = 
                                iTab[target_rank][target_bank].idle[int(CP_Type::RCP)];
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
                    if (sTab[target_rank][target_bank].cp_status == CP_Status::WCP_ON || 
                        sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_ON)
                    {
                        if (issueable(q_iter))
                        {
                            int bank_idle = 
                                iTab[target_rank][target_bank].idle[int(CP_Type::WCP)];
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

            // Step three, has to schedule the first request.
            req = r_w_q.begin();
            if (issueable(req))
            {
                return std::make_pair(true, req);
            }
            return std::make_pair(false, r_w_q.end());
        }
    }

    void channelAccess(std::list<Request>::iterator& scheduled_req) override
    {
        scheduled_req->begin_exe = clk;

        // Step one, determine the charging latency and update charge pump status.
        unsigned charging_latency = 0;
        int target_rank = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
        int target_bank = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

        if (scheduled_req->req_type == Request::Request_Type::READ)
        {
            // If both charge pumps are OFF, turn on the read charge pump.
            // Now, only the read charge pump is ON.
            if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_OFF)
            {
                sTab[target_rank][target_bank].cp_status = CP_Status::RCP_ON;

                charging_latency = nclks_rcp;
            }
            // If the write charge pump is ON, turn on the read charge pump. 
            // Now, both charge pumps are ON.
            else if (sTab[target_rank][target_bank].cp_status == CP_Status::WCP_ON)
            {
                sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;

                charging_latency = nclks_rcp;
            }

            if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
            {
                // For LAS-PCM, the pump is no longer idle
                aTab[target_rank][target_bank].aging[int(CP_Type::RCP)] += 
                    iTab[target_rank][target_bank].idle[int(CP_Type::RCP)];
                iTab[target_rank][target_bank].idle[int(CP_Type::RCP)] = 0;
            }

            // Record a new read request.
            if (rTab[target_rank][target_bank].num_of_reads == 0)
            {
                assert(charging_latency > 0);
                // Read cp starts to charge now.
                rTab[target_rank][target_bank].read_cp_begin_charging = clk;
                rTab[target_rank][target_bank].read_cp_end_charging = clk + charging_latency;
            }
            rTab[target_rank][target_bank].num_of_reads++;

            // Read charge pump is now the busy pump.
            sTab[target_rank][target_bank].cur_busy_cp = CP_Type::RCP;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            // If both charge pumps are OFF, turn on the write charge pump.
            // Now, only the write charge pump is ON. 
            if (sTab[target_rank][target_bank].cp_status == CP_Status::BOTH_OFF)
            {
                sTab[target_rank][target_bank].cp_status = CP_Status::WCP_ON;

                charging_latency = nclks_wcp;
            }
            // If only the read charge pump is ON, turn on the write charge pump.
            // Now, both charge pumps are ON.
            else if (sTab[target_rank][target_bank].cp_status == CP_Status::RCP_ON)
            {
                sTab[target_rank][target_bank].cp_status = CP_Status::BOTH_ON;

                charging_latency = nclks_wcp;
            }

            if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
            {
                // For LAS-PCM, the pump is no longer idle
                aTab[target_rank][target_bank].aging[int(CP_Type::WCP)] += 
                    iTab[target_rank][target_bank].idle[int(CP_Type::WCP)];
                iTab[target_rank][target_bank].idle[int(CP_Type::WCP)] = 0;
            }

            // Record a write request.
            if (rTab[target_rank][target_bank].num_of_writes == 0)
            {
                assert(charging_latency > 0);
                rTab[target_rank][target_bank].write_cp_begin_charging = clk;
                rTab[target_rank][target_bank].write_cp_end_charging = clk + charging_latency;
            }
            rTab[target_rank][target_bank].num_of_writes++;

            // Write charge pump is now the busy pump.
            sTab[target_rank][target_bank].cur_busy_cp = CP_Type::WCP;
        }

        if constexpr (std::is_same<BASE, Scheduler>::value)
        {
            // For Base, there has to a charging for any new request.
            assert(charging_latency > 0);
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
                   req_latency, // This is rank latency for other ranks.
                                // Since there is no rank-level parall,
                                // other ranks must wait until the current rank
                                // to be fully de-coupled.
                   bank_latency);
    }

  // Technology-specific parameters (You should tune it based on the need of your system)
  // Only for LAS-PCM
  protected:
    const int back_logging_threshold = -16;
    const int aging_threshold = 1500;
    const int idle_threshold = 200;

    // For simplicity, we generalize two types of charge pump (Read CP and Write CP)
    enum class CP_Type : int
    {
        RCP, // Read charge pump
        WCP, // Write charge pump
        MAX
    };

    // Charge pump status for each bank (two charge pumps per bank).
    enum class CP_Status : int
    {
        RCP_ON,
        WCP_ON,
        BOTH_ON,
        BOTH_OFF,
        MAX
    };

    // Time to charge/discharge the write charge pump
    const unsigned nclks_wcp;
    // Time to charge/discharge the read charge pump
    const unsigned nclks_rcp;

    struct Status_Entry
    {
        CP_Status cp_status; // Read CP is on? Write CP is on? Both?

        CP_Type cur_busy_cp; // We also need to record the current busy cp
    };
    // One record for each bank
    std::vector<std::vector<Status_Entry>> sTab;

    struct Aging_Entry
    {
        // One for Read CP, one for Write CP
        std::vector<Tick> aging;
    };
    // One record for each bank
    std::vector<std::vector<Aging_Entry>> aTab;

    struct Idle_Entry
    {
        std::vector<Tick> idle;
    };
    // One record for each bank
    std::vector<std::vector<Idle_Entry>> iTab;

    struct Access_Record
    {
        Tick read_cp_begin_charging;
        Tick read_cp_end_charging;

        Tick write_cp_begin_charging;
        Tick write_cp_end_charging;

        unsigned num_of_reads;
        unsigned num_of_writes;
    };
    std::vector<std::vector<Access_Record>> rTab; // request table

    void tableUpdate()
    {
        for (int i = 0; i < num_of_ranks; i++)
        {
            for (int j = 0; j < num_of_banks; j++)
            {
                // Step one, update read charge pump.
                if (sTab[i][j].cp_status == CP_Status::RCP_ON || 
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    if (sTab[i][j].cur_busy_cp == CP_Type::RCP && 
                        !channel->isBankFree(i,j))
                    {
                        // Bank's read charge pump is currently working.
                        ++aTab[i][j].aging[int(CP_Type::RCP)];
                    }
                    else
                    {
                        // Bank's read charge pump is left idle.
                        ++iTab[i][j].idle[int(CP_Type::RCP)];
                    }
                }

                // Step two, update write charge pump.
                if (sTab[i][j].cp_status == CP_Status::WCP_ON || 
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    if (sTab[i][j].cur_busy_cp == CP_Type::WCP && 
                        !channel->isBankFree(i,j))
                    {
                        // Bank's write charge pump is currently working.
                        ++aTab[i][j].aging[int(CP_Type::WCP)];
                    }
                    else
                    {
	                // Bank's write charge pump is left idle.
                        ++iTab[i][j].idle[int(CP_Type::WCP)];
                    }
                }
            }
        }
    }

    void dischargeOpenBanks()
    {
        if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
        {
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    // Discharge read charge pumps
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        int total_aging = aTab[i][j].aging[int(CP_Type::RCP)] +
                                          iTab[i][j].idle[int(CP_Type::RCP)];
                    
                        // Discharge because of aging
                        if (total_aging >= aging_threshold)
                        {
                            dischargeSingleBank(CP_Type::RCP, i, j);
                        }
                        // Discharge because of idle
                        else if (iTab[i][j].idle[int(CP_Type::RCP)] >= idle_threshold)
                        {
                              dischargeSingleBank(CP_Type::RCP, i, j);
                        }
                        // Discharge because of there is no more request to the bank
                        else if (num_reqs_to_banks[int(Request::Request_Type::READ)][i][j] 
                                 == 0)
                        {
                            dischargeSingleBank(CP_Type::RCP, i, j);
                        }
                    }

                    // Discharge write charge pumps	
                    if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        int total_aging = aTab[i][j].aging[int(CP_Type::WCP)] +
                                          iTab[i][j].idle[int(CP_Type::WCP)];

                        // Discharge because of aging
                        if (total_aging >= aging_threshold)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
                        }
                        // Discharge because of idle
                        else if (iTab[i][j].idle[int(CP_Type::WCP)] >= idle_threshold)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
                        }
                        else if (num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] 
                                 == 0)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
                        }
                    }
                }
            }
        }

        if constexpr (std::is_same<LASER, Scheduler>::value)
        {
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        unsigned total_idle = iTab[i][j].idle[int(CP_Type::WCP)] + 
                                              iTab[i][j].idle[int(CP_Type::RCP)];

                        unsigned num_of_reads_done = rTab[i][j].num_of_reads;
                        unsigned num_of_writes_done = rTab[i][j].num_of_writes;

                        double ps_aging = 1.82 * (double)num_of_reads_done + 
                                          580.95 * (double)num_of_writes_done +
                                          0.03 * (double)total_idle;

                        double sa_aging = 59.63 * (double)num_of_reads_done +
                                          5.22 * (double)num_of_writes_done +
                                          0.03 * (double)total_idle;

			// Discharge because of aging
                        if (ps_aging > 1000.0 ||
                            sa_aging > 1000.0)
                        {
                            if (num_of_writes_done)
                            {
                                // write charge pump is on, use write charge pump discharging
                                // time.
                                dischargeSingleBank(CP_Type::WCP, i, j);
                            }
                            else 
                            {
                                dischargeSingleBank(CP_Type::RCP, i, j);
                            }
                        }
                        else // no aging exceeds
                        {
                            // Discharge because of no more requests
                            if (num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] 
                                == 0 &&
                                num_reqs_to_banks[int(Request::Request_Type::READ)][i][j]
                                == 0)
                            {
                                if (num_of_writes_done)
                                {
                                    dischargeSingleBank(CP_Type::WCP, i, j);
                                }
                                else
                                {
                                    dischargeSingleBank(CP_Type::RCP, i, j);
                                }
                            }
                        }
                    }
                }
            }
        }

        if constexpr (std::is_same<LASER_2, Scheduler>::value)
        {
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        unsigned total_idle = iTab[i][j].idle[int(CP_Type::WCP)] + 
                                              iTab[i][j].idle[int(CP_Type::RCP)];

                        unsigned num_of_reads_done = rTab[i][j].num_of_reads;
                        unsigned num_of_writes_done = rTab[i][j].num_of_writes;

                        double ps_aging = 1.82 * (double)num_of_reads_done + 
                                          580.95 * (double)num_of_writes_done +
                                          0.03 * (double)total_idle;

                        double sa_aging = 59.63 * (double)num_of_reads_done +
                                          5.22 * (double)num_of_writes_done +
                                          0.03 * (double)total_idle;

                        // Discharge write charge pump
                        if ((ps_aging > 1000.0 && num_of_writes_done > 0))
                        {
                            dischargeSingleCP(CP_Type::WCP, i, j);
                        }

                        // Discharge read charge pump
			if ((sa_aging > 1000.0 && num_of_reads_done > 0)) 
                        {
                            dischargeSingleCP(CP_Type::RCP, i, j);
                        }
	
                        // When no aging exceeds, then proceed.
                        if (ps_aging < 1000.0 && sa_aging < 1000.0)
                        {
			    // Discharge because of no more requests
                            if (num_reqs_to_banks[int(Request::Request_Type::WRITE)][i][j] 
                                == 0 &&
                                num_reqs_to_banks[int(Request::Request_Type::READ)][i][j]
                                == 0)
                            {
                                if (num_of_writes_done)
                                {
                                    dischargeSingleCP(CP_Type::WCP, i, j);
                                }
                                else
                                {
                                    dischargeSingleCP(CP_Type::RCP, i, j);
                                }
                            }
                        }
                    }
                }
            }
        }

        if constexpr (std::is_same<CP_STATIC, Scheduler>::value)
	{
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    // Discharge read charge pumps
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        int total_aging = aTab[i][j].aging[int(CP_Type::RCP)] +
                                          iTab[i][j].idle[int(CP_Type::RCP)];

                        if (total_aging >= 1000)
                        {
                            dischargeSingleBank(CP_Type::RCP, i, j);
                        }
                    }

                    // Discharge write charge pumps	
                    if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        // CP_STATIC and BASE discharge write charge pump after every
                        // request.
                        dischargeSingleBank(CP_Type::WCP, i, j);
                    }
                }
            }
        }

        if constexpr (std::is_same<BASE, Scheduler>::value)
	{
            for (int i = 0; i < num_of_ranks; i++)
            {
                for (int j = 0; j < num_of_banks; j++)
                {
                    // Discharge read charge pumps
                    if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        // BASE discharges read charge for every request
                        dischargeSingleBank(CP_Type::RCP, i, j);
                    }

                    // Discharge write charge pumps	
                    if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                        sTab[i][j].cp_status == CP_Status::BOTH_ON)
                    {
                        // CP_STATIC and BASE discharge write charge pump after every
                        // request.
                        dischargeSingleBank(CP_Type::WCP, i, j);
                    }
                }
            }
        }
    }

    void dischargeSingleCP(CP_Type cp_type, int rank_id, int bank_id)
    {
        // Condition one: The CP we are trying to discharge happens to be currently busy CP
        //                && The CP has done its service. 
        bool condition_one = ((cp_type == sTab[rank_id][bank_id].cur_busy_cp) &&
                               channel->isBankFree(rank_id, bank_id));
        // Condition two: The CP we are trying to discharge is not the currently busy CP
        bool condition_two = (cp_type != sTab[rank_id][bank_id].cur_busy_cp);

        // If any condition holds, discharge the CP.
        if (condition_one || condition_two)
        {
            if (offline_cp_analysis_mode)
            {
                recordCPInfo(cp_type, rank_id, bank_id);
            }

            Tick discharging_latency = 10; // Give all pumps 10 extra cycles to de-stress
            if (cp_type == CP_Type::RCP)
            {
                discharging_latency = nclks_rcp; // Same as charging
            }
            else
            {
                discharging_latency = nclks_wcp; // Same as charging
            }

            // Update bank's status and reset all the trackings.
            if (sTab[rank_id][bank_id].cp_status == CP_Status::BOTH_ON)
            {
                if (cp_type == CP_Type::RCP)
                {
                    // Turn off the read charge pump. 
                    // Only the write charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::WCP_ON;
                }
                else if (cp_type == CP_Type::WCP)
                {
                    // Turn off the write charge pump.
                    // Only read charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::RCP_ON;
                }
            }
            else
            {
                // Both pumps are OFF
                sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;
            }

	    // Reset the timings (all has to be cleared)
            aTab[rank_id][bank_id].aging[int(CP_Type::RCP)] = 0;
            iTab[rank_id][bank_id].idle[int(CP_Type::RCP)] = 0;

            aTab[rank_id][bank_id].aging[int(CP_Type::WCP)] = 0;
            iTab[rank_id][bank_id].idle[int(CP_Type::WCP)] = 0;

            rTab[rank_id][bank_id].num_of_reads = 0;
            rTab[rank_id][bank_id].num_of_writes = 0;

            if (channel->isBankFree(rank_id, bank_id))
            {
                channel->addBankLatency(rank_id, bank_id, discharging_latency);
            }
            assert(!channel->isBankFree(rank_id, bank_id));
        }
    }
    // Discharge all charge pumps in a bank
    void dischargeSingleBank(CP_Type cp_type, int rank_id, int bank_id)
    {
        // Make sure the bank is free (not serving any request)
        if (channel->isBankFree(rank_id, bank_id))
        {
            if (offline_cp_analysis_mode)
            {
                // Output, TODO, output is not correct
                if (rTab[rank_id][bank_id].num_of_reads > 0)
                {
                    recordCPInfo(CP_Type::RCP, rank_id, bank_id);
                }
                if (rTab[rank_id][bank_id].num_of_writes > 0)
                {
                    recordCPInfo(CP_Type::WCP, rank_id, bank_id);
                }
            }

            Tick discharging_latency = 10; // Give all pumps 10 extra cycles to de-stress
            if (cp_type == CP_Type::RCP)
            {
                discharging_latency = nclks_rcp; // Same as charging
            }
            else
            {
                discharging_latency = nclks_wcp; // Same as charging
            }

            // Shut down all the pumps
            sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;

            // Reset the timings
            aTab[rank_id][bank_id].aging[int(CP_Type::RCP)] = 0;
            iTab[rank_id][bank_id].idle[int(CP_Type::RCP)] = 0;

            aTab[rank_id][bank_id].aging[int(CP_Type::WCP)] = 0;
            iTab[rank_id][bank_id].idle[int(CP_Type::WCP)] = 0;

            rTab[rank_id][bank_id].num_of_reads = 0;
            rTab[rank_id][bank_id].num_of_writes = 0;

            channel->addBankLatency(rank_id, bank_id, discharging_latency);
            assert(!channel->isBankFree(rank_id, bank_id));
        }
    }

    void recordCPInfo(CP_Type cp_type, int rank_id, int bank_id)
    {
        Tick begin_charging = 0;
        Tick end_charging = 0;
        Tick begin_discharging = 0;
        Tick end_discharging = 0;

        Tick total_working = 0;

        if (cp_type == CP_Type::RCP)
        {
            assert(rTab[rank_id][bank_id].num_of_reads > 0);

            begin_charging = rTab[rank_id][bank_id].read_cp_begin_charging;
            end_charging = rTab[rank_id][bank_id].read_cp_end_charging;
            begin_discharging = clk;
            end_discharging = begin_discharging + nclks_rcp;
            total_working = rTab[rank_id][bank_id].num_of_reads * singleReadLatency;
        }
        else
        {
            assert(rTab[rank_id][bank_id].num_of_writes > 0);

            begin_charging = rTab[rank_id][bank_id].write_cp_begin_charging;
            end_charging = rTab[rank_id][bank_id].write_cp_end_charging;
            begin_discharging = clk;
            end_discharging = begin_discharging + nclks_wcp;
            total_working = rTab[rank_id][bank_id].num_of_writes * singleWriteLatency;
        }

        // Update global records
        Tick total_charging = begin_discharging - end_charging;
        if (max_charging == -1 && min_charging == -1)
        {
            max_charging = total_charging;
            min_charging = total_charging; 
        }
        else
        {
            if (total_charging > max_charging) { max_charging = total_charging; }
            if (total_charging < min_charging) { min_charging = total_charging; }
        }

        if (max_working == -1 && min_working == -1)
        {
            max_working = total_working;
            min_working = total_working;
        }
        else
        {
            if (total_working > max_working) { max_working = total_working; }
            if (total_working < min_working) { min_working = total_working; }
        }

        unsigned total_idle = iTab[rank_id][bank_id].idle[int(CP_Type::WCP)] +
                              iTab[rank_id][bank_id].idle[int(CP_Type::RCP)];

        unsigned num_of_reads_done = rTab[rank_id][bank_id].num_of_reads;
        unsigned num_of_writes_done = rTab[rank_id][bank_id].num_of_writes;

        double ps_aging = 1.82 * (double)num_of_reads_done +
                          580.95 * (double)num_of_writes_done +
                          0.03 * (double)total_idle;
        total_ps_aging += ps_aging;

        double vl_aging = 1.82 * (double)num_of_reads_done + 
                          171.26 * (double)num_of_writes_done + 
                          0.03 * (double)total_idle;
        total_vl_aging += vl_aging;

        double sa_aging = 59.63 * (double)num_of_reads_done +
                          5.22 * (double)num_of_writes_done +
                          0.03 * (double)total_idle;
        total_sa_aging += sa_aging;

        // Output
        if (cp_type == CP_Type::RCP)
        {
            *offline_cp_ana_output << "RCP,";
        }
        else if (cp_type == CP_Type::WCP)
        {
            *offline_cp_ana_output << "WCP,";
        }

        unsigned uni_bank_id = id * num_of_ranks * num_of_banks +
                               rank_id * num_of_banks + bank_id;

        /*
        *offline_cp_ana_output << uni_bank_id << ","
                               << begin_charging << ","
                               << end_charging << ","
                               << begin_discharging << ","
                               << end_discharging << ","
                               << total_charging << ","
                               << total_working << "\n";
        */
        *offline_cp_ana_output << uni_bank_id << ","
                               << ps_aging << ","
                               << vl_aging << ","
                               << sa_aging << "\n";

        *offline_cp_ana_output << std::flush;
    }

  public:
    // End of simulation, make sure all the pumps are shut down.
    virtual void drained()
    {
        for (int i = 0; i < num_of_ranks; i++)
        {
            for (int j = 0; j < num_of_banks; j++)
            {
                int uni_bank_id = i * num_of_banks + j;

                // Discharge read charge pumps
                if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    dischargeSingleBank(CP_Type::RCP, i, j);
                }

                // Discharge write charge pumps	
                if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    dischargeSingleBank(CP_Type::WCP, i, j);
		}

                assert(sTab[i][j].cp_status == CP_Status::BOTH_OFF);	
            }
        }
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
    // std::vector<std::vector<Tick>> total_charging;
    int max_charging = -1;
    int min_charging = -1;

    int max_working = -1;
    int min_working = -1;

    double total_ps_aging = 0.0;
    double total_vl_aging = 0.0;
    double total_sa_aging = 0.0;
};

typedef LASPCM<FCFS,LASER_2> LASER_2_Controller;

typedef LASPCM<FCFS,LASER> LASER_Controller;

typedef LASPCM<FCFS,LAS_PCM> LAS_PCM_Controller;

typedef LASPCM<FCFS,CP_STATIC> LAS_PCM_Static;

typedef LASPCM<FCFS,BASE> LAS_PCM_Base;
}

#endif
