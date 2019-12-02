#ifndef __LAS_PCM_CONTROLLER_HH__
#define __LAS_PCM_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

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
        , num_of_ranks(cfg.num_of_ranks)
        , num_of_banks(cfg.num_of_banks)
    {
        sTab.resize(num_of_ranks);
        aTab.resize(num_of_ranks);
        iTab.resize(num_of_ranks);

        // total_charging.resize(num_of_ranks);

        for (int i = 0; i < num_of_ranks; i++)
        {
            sTab[i].resize(num_of_banks);
            aTab[i].resize(num_of_banks);
            iTab[i].resize(num_of_banks);

            // total_charging[i].resize(num_of_banks);

            for (int j = 0; j < num_of_banks; j++)
            {
                // Initially, all the charge pumps are off.
                sTab[i][j].cp_status = CP_Status::BOTH_OFF;
                // Initially, none of the charge pump is busy.
                sTab[i][j].cur_busy_cp = CP_Type::MAX;

                aTab[i][j].aging.resize(int(CP_Type::MAX), 0);

                iTab[i][j].idle.resize(int(CP_Type::MAX), 0);

                // total_charging[i][j] = 0;
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
            // For Base and CP_STATIC, we use either FCFS or FRFCFS
            auto req = r_w_q.begin();
            if (issueable(req))
            {
                return std::make_pair(true, req);
            }
            return std::make_pair(false, r_w_q.end());
        }

        if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
        {
            auto req = r_w_q.begin();
            // To check 
            if (req->OrderID <= back_logging_threshold)
            {
                if (issueable(req))
                {
                    return std::make_pair(true, req);
                }
                return std::make_pair(false, r_w_q.end());
            }
        }
    }

  // Technology-specific parameters (You should tune it based on the need of your system)
  protected:
    const int back_logging_threshold = -16;
    const int aging_threshold = 1500;
    const int idle_threshold = -1;

  protected:
    const unsigned num_of_ranks;
    const unsigned num_of_banks;

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

    const unsigned nclks_wcp = 48; // Charging or discharging
    const unsigned nclks_rcp = 11; // Charging or discharging

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
                if (sTab[i][j].cp_status == CP_Status::RCP_ON || 
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    if (sTab[i][j].cur_busy_cp == CP_Type::RCP && 
                        !channel->isFree(i,j))
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

                if (sTab[i][j].cp_status == CP_Status::WCP_ON || 
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    if (sTab[i][j].cur_busy_cp == CP_Type::WCP && 
                        !channel->isFree(i,j))
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
        for (int i = 0; i < num_of_ranks; i++)
        {
            for (int j = 0; j < num_of_banks; j++)
            {
                // Discharge read charge pump
                if (sTab[i][j].cp_status == CP_Status::RCP_ON ||
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    Tick total_aging = aTab[i][j].aging[int(CP_Type::RCP)] +
                                       iTab[i][j].idle[int(CP_Type::RCP)];

                    // Discharge because of aging
                    if constexpr (std::is_same<LAS_PCM, Scheduler>::value || 
                                  std::is_same<CP_STATIC, Scheduler>::value)
                    {
                        // CP_STATIC and LAS_PCM discharge read charge based on
                        // an aging threshold
                        if (total_aging >= aging_threshold)
                        {
                            dischargeSingleBank(CP_Type::RCP, i, j);
                        }
                    }

                    if constexpr (std::is_same<BASE, Scheduler>::value)
                    {
                        // BASE discharges read charge for every request
                        dischargeSingleBank(CP_Type::RCP, i, j);
                    }

                    if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
                    {
                        // Discharge because of idle
                        if (idle_threshold != -1 &&
                            iTab[i][j].idle[int(CP_Type::RCP)] >= idle_threshold)
                        {
                            dischargeSingleBank(CP_Type::RCP, i, j);
                        }

                        // Discharge because there are no more requests to this
                        // charge pump
                        
                    }
                }
		
                if (sTab[i][j].cp_status == CP_Status::WCP_ON ||
                    sTab[i][j].cp_status == CP_Status::BOTH_ON)
                {
                    Tick total_aging = aTab[i][j].aging[int(CP_Type::WCP)] +
                                       iTab[i][j].idle[int(CP_Type::WCP)];

                    // Discharge because of aging
                    if constexpr (std::is_same<LAS_PCM, Scheduler>::value) 
                    {
                        // LAS_PCM discharges write charge pump based on a threshold.
                        if (total_aging >= aging_threshold)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
                        }
                    }

                    if constexpr (std::is_same<CP_STATIC, Scheduler>::value || 
                                  std::is_same<BASE, Scheduler>::value)
                    {
                        // CP_STATIC and BASE discharge write charge pump after every
                        // request.
                        dischargeSingleBank(CP_Type::WCP, i, j);
                    }

                    // Discharge because of idle.
                    if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
                    {
                        if (idle_threshold != -1 &&
                            iTab[i][j].idle[int(CP_Type::WCP)] >= idle_threshold)
                        {
                            dischargeSingleBank(CP_Type::WCP, i, j);
                        }
                    }
                }
            }
        }
    }

    void dischargeSingleBank(CP_Type cp_type, int rank_id, int bank_id)
    {
        // Discharge the bank when it's done serving on-going request.
        // Or the bank is serving another type of request.
        if (channel->isFree(rank_id, bank_id) || 
            cp_type != sTab[rank_id][bank_id].cur_busy_cp)
        {
            Tick discharging_latency = 0;
            if (cp_type == CP_Type::RCP)
            {
                discharging_latency = nclks_rcp;
            }
            else
            {
                discharging_latency = nclks_wcp;
            }
            channel->postAccess(rank_id, bank_id,
                                0,
                                discharging_latency,
                                discharging_latency);
            assert(!channel->isFree(rank_id, bank_id));

            if (sTab[rank_id][bank_id].cp_status == CP_Status::BOTH_ON)
            {
                if (cp_type == CP_Type::RCP)
                {
                    // Only write charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::WCP_ON;
                }
                else if (cp_type == CP_Type::WCP)
                {
                    // Only read charge pump is left ON
                    sTab[rank_id][bank_id].cp_status = CP_Status::RCP_ON;
                }
            }
            else
            {
                // Both pumps are OFF
                sTab[rank_id][bank_id].cp_status = CP_Status::BOTH_OFF;
            }
        }
    }

  // Stats
  protected:
    // std::vector<std::vector<Tick>> total_charging;
    int max_charging = -1;
    int min_charging = -1;
};

typedef LASPCM<FCFS,LAS_PCM> LAS_PCM_Controller;
typedef LASPCM<FRFCFS,LAS_PCM> PERF_LAS_PCM_Controller;

typedef LASPCM<FCFS,CP_STATIC> CP_STATIC_Controller;
typedef LASPCM<FRFCFS,CP_STATIC> PERF_CP_STATIC_Controller;

typedef LASPCM<FCFS,BASE> BASE_Controller;
typedef LASPCM<FRFCFS,BASE> PERF_BASE_Controller;
}

#endif
