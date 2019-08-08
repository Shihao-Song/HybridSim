#ifndef __LAS_PCM_CONTROLLER_HH__
#define __LAS_PCM_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

namespace PCMSim
{
// TODO, limitation, only 1-stage charging is supported so far.
// To fully integrate LAS-PCM with multiple charging and PALP, we may need to change 
// the existing architecture (Future Work).
// TODO, template<typename S>
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
        discharge();

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
                if (sTab[i][j].open && sTab[i][j].cp_type == int(CP_Type::RCP) ||
                    sTab[i][j].open && sTab[i][j].cp_type == -1)
                {
                    if (!channel->isFree(i,j))
                    {
                        ++aTab[i][j].aging[int(CP_Type::RCP)];
                    }
                    else
                    {
                        ++iTab[i][j].idle[int(CP_Type::RCP)];
                    }
                }

                if (sTab[i][j].open && sTab[i][j].cp_type == int(CP_Type::WCP) ||
                    sTab[i][j].open && sTab[i][j].cp_type == -1)
                {
                    if (!channel->isFree(i,j))
                    {
                        ++aTab[i][j].aging[int(CP_Type::WCP)];
                    }
                    else
                    {
                        ++iTab[i][j].idle[int(CP_Type::WCP)];
                    }
                }
            }
        }
    }

    void discharge()
    {
        
    }

  // Stats
  protected:
    std::vector<std::vector<Tick>> total_charging;
    int max_charging = -1;
    int min_charging = -1;
};
}

#endif
