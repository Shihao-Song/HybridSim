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

        for (int i = 0; i < num_of_ranks; i++)
        {
            sTab[i].resize(num_of_banks);
            aTab[i].resize(num_of_banks);
            iTab[i].resize(num_of_banks);

            for (int j = 0; j < num_of_banks; j++)
            {
                // Initially, all the charge pumps are off.
                sTab[i][j].cp_status = CP_Status::BOTH_OFF;
                // Initially, none of the charge pump is busy.
                sTab[i][j].cur_busy_cp = CP_Type::MAX;

                aTab[i][j].aging.resize(int(CP_Type::MAX), 0);

                iTab[i][j].idle.resize(int(CP_Type::MAX), 0);
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
        
        if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
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
                        else if (num_reqs_to_banks[i][j] == 0)
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
                        else if (num_reqs_to_banks[i][j] == 0)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
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

    void dischargeSingleBank(CP_Type cp_type, int rank_id, int bank_id)
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
            Tick discharging_latency = 0;
            if (cp_type == CP_Type::RCP)
            {
                discharging_latency = nclks_rcp; // Same as charging
            }
            else
            {
                discharging_latency = nclks_wcp; // Same as charging
            }

            
	    if (offline_cp_analysis_mode)
            {
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

                Tick total_aging = aTab[rank_id][bank_id].aging[int(cp_type)] +
                                   iTab[rank_id][bank_id].idle[int(cp_type)];

                Tick start_charging = clk - total_aging;
                Tick end_charging = start_charging + discharging_latency;
                Tick start_discharging = clk;
                Tick end_discharging = clk + discharging_latency;

                // Error checking
                if constexpr (std::is_same<BASE, Scheduler>::value)
                {
                    if (cp_type == CP_Type::RCP)
                    {
                        assert((start_discharging - end_charging) == singleReadLatency);
                    }
                    else if (cp_type == CP_Type::WCP)
                    {
                        assert((start_discharging - end_charging) == singleWriteLatency);
                    }

                    *offline_cp_ana_output << uni_bank_id << ","
                                           << start_charging << ","
                                           << end_charging << ","
                                           << start_discharging << ","
                                           << end_discharging << "\n";
                }

                if constexpr (std::is_same<CP_STATIC, Scheduler>::value || 
                              std::is_same<LAS_PCM, Scheduler>::value)
                {
                    total_charging += total_aging;
                    total_idle += iTab[rank_id][bank_id].idle[int(cp_type)];
                    total_working += aTab[rank_id][bank_id].aging[int(cp_type)];

                    if (max_charging == -1 && min_charging == -1)
                    {
                        max_charging = total_aging;
                        min_charging = total_aging; 
                    }
                    else
                    {
                        if (total_aging > max_charging) { max_charging = total_aging; }
                        if (total_aging < min_charging) { min_charging = total_aging; }
                    }

                    int only_working = aTab[rank_id][bank_id].aging[int(cp_type)];
                    if (max_working == -1 && min_working == -1)
                    {
                        max_working = only_working;
                        min_working = only_working;
                    }
                    else
                    {
                        if (only_working > max_working) { max_working = only_working; }
                        if (only_working < min_working) { min_working = only_working; }
                    }

                    int only_idle = iTab[rank_id][bank_id].idle[int(cp_type)];
                    if (max_idle == -1 && min_idle == -1)
                    {
                        max_idle = only_idle;
                        min_idle = only_idle;
                    }
                    else
                    {
                        if (only_idle > max_idle) { max_idle = only_idle; }
                        if (only_idle < min_idle) { min_idle = only_idle; }
                    }

                    *offline_cp_ana_output << uni_bank_id << ","
                                           << start_charging << ","
                                           << end_charging << ","
                                           << start_discharging << ","
                                           << end_discharging << ","
                                           << aTab[rank_id][bank_id].aging[int(cp_type)] << ","
                                           << iTab[rank_id][bank_id].idle[int(cp_type)] << "\n";
                }

                *offline_cp_ana_output << std::flush;
            }

            // Update bank's status and reset all the trackings.
            if (sTab[rank_id][bank_id].cp_status == CP_Status::BOTH_ON)
            {
                if (cp_type == CP_Type::RCP)
                {
                    // Turn off the read charge pump. 
                    // Only the write charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::WCP_ON;

                    // Reset the timings
                    aTab[rank_id][bank_id].aging[int(CP_Type::RCP)] = 0;
                    iTab[rank_id][bank_id].idle[int(CP_Type::RCP)] = 0;
                }
                else if (cp_type == CP_Type::WCP)
                {
                    // Turn off the write charge pump.
                    // Only read charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::RCP_ON;
		    
                    // Reset the timings
                    aTab[rank_id][bank_id].aging[int(CP_Type::WCP)] = 0;
                    iTab[rank_id][bank_id].idle[int(CP_Type::WCP)] = 0;
                }
            }
            else
            {
                // Both pumps are OFF
                sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;

                // Reset the timings
                aTab[rank_id][bank_id].aging[int(CP_Type::RCP)] = 0;
                iTab[rank_id][bank_id].idle[int(CP_Type::RCP)] = 0;

                aTab[rank_id][bank_id].aging[int(CP_Type::WCP)] = 0;
                iTab[rank_id][bank_id].idle[int(CP_Type::WCP)] = 0;
            }

            if (channel->isBankFree(rank_id, bank_id))
            {
                channel->addBankLatency(rank_id, bank_id, discharging_latency);
            }
            assert(!channel->isBankFree(rank_id, bank_id));
        }
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

    int max_idle = -1;
    int min_idle = -1;

    Tick total_charging = 0;
    Tick total_idle = 0;
    Tick total_working = 0;
};

typedef LASPCM<FCFS,LAS_PCM> LAS_PCM_Controller;

typedef LASPCM<FCFS,CP_STATIC> LAS_PCM_Static;

typedef LASPCM<FCFS,BASE> LAS_PCM_Base;
}

#endif
