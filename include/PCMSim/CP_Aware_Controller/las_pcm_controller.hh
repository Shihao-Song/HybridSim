#ifndef __LAS_PCM_CONTROLLER_HH__
#define __LAS_PCM_CONTROLLER_HH__

#include "PCMSim/Controller/pcm_sim_controller.hh"

namespace PCMSim
{
// TODO, limitation, only 1-stage charging is supported so far.
class LASPCM : public FCFSController
{
  public:
    LASPCM(int _id, Config &cfg)
        : FCFSController(_id, cfg)
    {
        sTab.resize(cfg.num_of_ranks);
        aTab.resize(cfg.num_of_ranks);
        iTab.resize(cfg.num_of_ranks);

        for (int i = 0; i < cfg.num_of_ranks; i++)
        {
            sTab[i].resize(cfg.num_of_banks);
            aTab[i].resize(cfg.num_of_banks);
            iTab[i].resize(cfg.num_of_banks);

            for (int j = 0; j < cfg.num_of_banks; j++)
            {
                sTab[i][j].open = false;
                sTab[i][j].cp_type = int(CP_Type::MAX);

                aTab[i][j].aging.resize(int(CP_Type::MAX), 0);

                iTab[i][j].idle.resize(int(CP_Type::MAX), 0);
            }
        }

        
        exit(0);
    }

  protected:
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

    // We generalize two types of charge pump (Read CP and Write CP)
    enum class CP_Type : int
    {
        RCP,
        WCP,
        MAX
    }cp_type;
};
}

#endif
