#ifndef __DRAM_PCM_CONTROLLER_HH__
#define __DRAM_PCM_CONTROLLER_HH__

#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"

namespace PCMSim
{
// TODO, should contain one PCM controller and one DRAM controller.
template<typename DCtrl, typename PCtrl>
class DRAMPCMController
{
    typedef Simulator::Config Config;

  protected:
    DCtrl DRAM_controller;
    PCtrl PCM_controller;

    unsigned base_rank_id_pcm;
    unsigned base_rank_id_dram;

  public:
    DRAMPCMController(int _id, Config &dram_cfg, Config &pcm_cfg)
        : DRAM_controller(_id, dram_cfg)
        , PCM_controller(_id, pcm_cfg)
        , base_rank_id_pcm(0)
        , base_rank_id_dram(dram_cfg.num_of_ranks / 2)
    {
        assert(dram_cfg.mmu_type == "Hybrid");
        assert(pcm_cfg.mmu_type == "Hybrid");
        assert(dram_cfg.mem_controller_type == "Hybrid");
        assert(pcm_cfg.mem_controller_type == "Hybrid");

        // std::cout << dram_cfg.num_of_ranks << "\n";
        // std::cout << base_rank_id_pcm << "\n";
        // std::cout << base_rank_id_dram << "\n";
        // exit(0);
    }

    void offlineReqAnalysis(std::ofstream *out)
    {
    
    }

    void reInitialize()
    {
        // TODO, reInitialize both PCM controller and DRAM controller.
    }

    int pendingRequests()
    {
        int outstandings = DRAM_controller.pendingRequests() +
                           PCM_controller.pendingRequests();

        return outstandings;
    }

    void tick()
    {
        DRAM_controller.tick();
        PCM_controller.tick();
    }

    bool enqueue(Request& req)
    {
        int rank_id = req.addr_vec[int(Config::Decoding::Rank)];
        // std::cout << rank_id  << "\n";

        if (rank_id >= base_rank_id_dram)
        {
            // std::cout << "DRAM\n";
            req.addr_vec[int(Config::Decoding::Rank)] = req.addr_vec[int(Config::Decoding::Rank)] - 
                                                        base_rank_id_dram;
            return DRAM_controller.enqueue(req);
        }
        else
        {
            // std::cout << "PCM\n";
            return PCM_controller.enqueue(req);
        }
    }
};

typedef DRAMPCMController<TLDRAMController, CPAwareController> TLDRAMPCMController;
}

#endif
