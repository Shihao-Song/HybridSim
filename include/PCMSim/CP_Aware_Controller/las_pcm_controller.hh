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
        , back_logging_threshold(cfg.THB)
        , aging_threshold(cfg.THA)
        , idle_threshold(cfg.THI)
    {
        sTab.resize(cfg.num_of_ranks);
        aTab.resize(cfg.num_of_ranks);
        iTab.resize(cfg.num_of_ranks);

        total_charging.resize(cfg.num_of_ranks);

        for (int i = 0; i < cfg.num_of_ranks; i++)
        {
            sTab[i].resize(cfg.num_of_banks);
            aTab[i].resize(cfg.num_of_banks);
            iTab[i].resize(cfg.num_of_banks);

            total_charging[i].resize(cfg.num_of_banks);

            for (int j = 0; j < cfg.num_of_banks; j++)
            {
                sTab[i][j].open = false;
                sTab[i][j].cp_type = int(CP_Type::MAX);

                aTab[i][j].aging.resize(int(CP_Type::MAX), 0);

                iTab[i][j].idle.resize(int(CP_Type::MAX), 0);

                total_charging[i][j] = 0;
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
        
        if constexpr (std::is_same<Base, Scheduler>::value || 
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
            
        }
    }

  // Technology-specific parameters (You should tune it based on the need of your system)
  protected:
    const int back_logging_threshold;
    const int aging_threshold;
    const int idle_threshold;

  protected:
    // For simplicity, we generalize two types of charge pump (Read CP and Write CP)
    enum class CP_Type : int
    {
        RCP, // Read charge pump
        WCP, // Write charge pump
        MAX
    }cp_type;

    const unsigned nclks_wcp = 48; // Charging or discharging
    const unsigned nclks_rcp = 11; // Charging or discharging

    struct Status_Entry
    {
        bool open;
        int cp_type; // Read CP is on? Write CP is on? Both?
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
        for (int i = 0; i < sTab.size(); i++)
        {
            for (int j = 0; j < sTab[0].size(); j++)
            {
                for (int cp = int(CP_Type::RCP); cp < int(CP_Type::MAX); cp++)
                {
                    if (sTab[i][j].open && sTab[i][j].cp_type == cp ||
                        sTab[i][j].open && sTab[i][j].cp_type == -1)
                    {
                        if (!channel->isFree(i,j))
                        {
                            ++aTab[i][j].aging[cp];
                        }
                        else
                        {
                            ++iTab[i][j].idle[cp];
                        }
                    }
                }
            }
        }
    }

    void dischargeOpenBanks()
    {
        for (int i = 0; i < sTab.size(); i++)
        {
            for (int j = 0; j < sTab[0].size(); j++)
            {
                for (int cp = int(CP_Type::RCP); cp < int(CP_Type::MAX); cp++)
                {
                    if (sTab[i][j].open && sTab[i][j].cp_type == cp ||
                        sTab[i][j].open && sTab[i][j].cp_type == -1)
                    {
                        Tick total_aging = aTab[i][j].aging[cp] + 
                                           iTab[i][j].idle[cp];

                        if constexpr (std::is_same<LAS_PCM, Scheduler>::value)
                        {
                            // Intelligent dis-charging for both read charge pump and
                            // write charge pump
                            if (total_aging >= aging_threshold)
                            {
                                dischargeSingleBank(cp, i, j);
                            }
                        }

                        if constexpr (std::is_same<CP_STATIC, Scheduler>::value)
                        {
                            // Intelligent discharging for Read Charge Pump
                            if (cp == int(CP_Type::RCP) && 
                                total_aging >= aging_threshold ||
                                cp == int(CP_Type::WCP))
                            {
                                dischargeSingleBank(cp, i, j);
                            }
                        }
                        
                        if constexpr (std::is_same<BASE, Scheduler>::value)
                        {
                            dischargeSingleBank(cp, i, j);
                        }
                    }
                }
            }
        }
    }

    void dischargeSingleBank(int cp_type, int rank_id, int bank_id)
    {
        // Discharge the bank when it's done serving on-going request
        if (channel->isFree(rank_id, bank_id))
        {
            Tick discharging_latency = 0;
            if (cp_type == int(CP_Type::RCP))
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

            if (sTab[rank_id][bank_id].cp_type == -1)
            {
                if (cp_type == int(CP_Type::RCP))
                {
                    // Only write charge pump is left ON
                    sTab[rank_id][bank_id].cp_type = int(CP_Type::WCP);
                }
                else if (cp_type == int(CP_Type::WCP))
                {
                    // Only read charge pump is left ON
                    sTab[rank_id][bank_id].cp_type = int(CP_Type::RCP);
                }
            }
            else
            {
                // Both pumps are OFF
                sTab[rank_id][bank_id].open = false;
                sTab[rank_id][bank_id].cp_type = int(CP_Type::MAX);
            }
        }
    }

  // Stats
  protected:
    std::vector<std::vector<Tick>> total_charging;
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
