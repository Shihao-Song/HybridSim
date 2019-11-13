#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller_plp.hh"
#include "PCMSim/CP_Aware_Controller/las_pcm_controller.hh"
#include "PCMSim/PLP_Controller/pcm_sim_plp_controller.hh"
#include "PCMSim/DRAM_PCM_Controller/dram_pcm_controller.hh"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PCMSim
{
template<typename T>
class PCMSimMemorySystem : public Simulator::MemObject
{
  private:
    std::vector<std::unique_ptr<T>> controllers;
    std::vector<int> memory_addr_decoding_bits;

  private:
    bool offline_req_analysis_mode = false;
    std::ofstream offline_req_ana_output;

  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Request Request;

    PCMSimMemorySystem(Config &cfg) : Simulator::MemObject()
    {
        // Initialize
        init(cfg);
    }

    PCMSimMemorySystem(Config &dram_cfg, Config &pcm_cfg) : Simulator::MemObject()
    {
        for (int i = 0; i < dram_cfg.num_of_channels; i++)
        {
            controllers.push_back(std::move(std::make_unique<T>(i, dram_cfg, pcm_cfg)));
        }
        memory_addr_decoding_bits = pcm_cfg.mem_addr_decoding_bits;
    }

    ~PCMSimMemorySystem()
    {
        if (offline_req_analysis_mode)
        {
            offline_req_ana_output.close();
        }
    }

    int pendingRequests() override
    {
        int outstandings = 0;

        for (auto &controller : controllers)
        {
            outstandings += controller->pendingRequests();
        }

        return outstandings;
    }

    bool send(Request &req) override
    {
        req.addr_vec.resize(int(Config::Decoding::MAX));

        Decoder::decode(req.addr, memory_addr_decoding_bits, req.addr_vec);
/*
        std::cout << "\n Address: " << req.addr << "\n";
        std::cout << "Rank: " << req.addr_vec[int(Config::Decoding::Rank)] << "\n"; 
        std::cout << "Partition: " << req.addr_vec[int(Config::Decoding::Partition)] << "\n";
        std::cout << "Tile: " << req.addr_vec[int(Config::Decoding::Tile)] << "\n";
        std::cout << "Row: " << req.addr_vec[int(Config::Decoding::Row)] << "\n";
        std::cout << "Col: " << req.addr_vec[int(Config::Decoding::Col)] << "\n";
        std::cout << "Bank: " << req.addr_vec[int(Config::Decoding::Bank)] << "\n";
        std::cout << "Channel: " << req.addr_vec[int(Config::Decoding::Channel)] << "\n";
        std::cout << "Block: " << req.addr_vec[int(Config::Decoding::Cache_Line)] << "\n";
*/
        int channel_id = req.addr_vec[int(Config::Decoding::Channel)];

        if(controllers[channel_id]->enqueue(req))
        {
            if (mem_trace_extr_mode)
            {
                mem_trace << req.addr << " ";
                if (req.req_type == Request::Request_Type::READ)
                {
                    mem_trace << "R\n";
                }
                else if (req.req_type == Request::Request_Type::WRITE)
                {
                    mem_trace << "W\n";
                }
            }
            return true;
        }

        return false;
    }

    void tick() override
    {
        for (auto &controller : controllers)
        {
            controller->tick();
        }
    }

    void reInitialize() override
    {
        for (auto &controller : controllers)
        {
            controller->reInitialize();
        }
    }

    void offlineReqAnalysis(std::string &file) override
    {
        offline_req_analysis_mode = true;
        offline_req_ana_output.open(file);

        for (auto &controller : controllers)
        {
            controller->offlineReqAnalysis(&offline_req_ana_output);
        }
    }

    void registerStats(Simulator::Stats &stats) override
    {
        if constexpr (std::is_same<TLDRAMPCMController, T>::value)
        {
            unsigned num_stages = controllers[0]->numStages();
            for (int k = 0; k < 2; k++)
            {
            for (int i = 0; i < int(CPAwareController::Req_Type::MAX); i++)
            {
                std::string target = "READ";
                if (i == int(CPAwareController::Req_Type::READ))
                {
                    target = "READ";
                }
                if (i == int(CPAwareController::Req_Type::WRITE))
                {
                    target = "WRITE";
                }
                
                for (int j = 0; j < num_stages; j++)
                {
                    int64_t stage_accesses = 0;
                    for (auto &controller : controllers)
                    {
                        // i - request type; j - stage ID.
                        stage_accesses += controller->stageAccess(i, j, k);
                    }
                    std::string stage_access_prin =
                                                "Stage-" + std::to_string(j) + "-"
                                                + target + "-Access"
                                                + " = "
                                                + std::to_string(stage_accesses);
                    if (k == 0) { stage_access_prin = "DRAM-" + stage_access_prin; }
                    else { stage_access_prin = "PCM-" + stage_access_prin;  }
                    stats.registerStats(stage_access_prin);
                }
            }
            }
        }

        if constexpr (std::is_same<CPAwareController, T>::value)
        {
            uint64_t total_reqs = 0;
            uint64_t total_waiting_time = 0;
            for (auto &controller : controllers)
            {
                total_reqs += controller->finished_requests;
                total_waiting_time += controller->total_waiting_time;
            }

            std::string req_info = "Total_Number_Requests = " + 
                                   std::to_string(total_reqs);
            std::string waiting_info = "Total_Waiting_Time = " + 
                                   std::to_string(total_waiting_time);
            std::string access_latency = "Access_Latency = " + 
                                   std::to_string(double(total_waiting_time) / 
                                                  double(total_reqs));
            stats.registerStats(req_info);
            stats.registerStats(waiting_info);
            stats.registerStats(access_latency);

            unsigned num_stages = controllers[0]->numStages();
            for (int i = 0; i < int(CPAwareController::Req_Type::MAX); i++)
            {
                std::string target = "READ";
                if (i == int(CPAwareController::Req_Type::READ))
                {
                    target = "READ";
                }
                if (i == int(CPAwareController::Req_Type::WRITE))
                {
                    target = "WRITE";
                }
                
                for (int j = 0; j < num_stages; j++)
                {
                    int64_t stage_accesses = 0;
                    for (auto &controller : controllers)
                    {
                        // i - request type; j - stage ID.
                        stage_accesses += controller->stageAccess(i, j);
                    }
                    std::string stage_access_prin =
                                                "Stage-" + std::to_string(j) + "-"
                                                + target + "-Access"
                                                + " = "
                                                + std::to_string(stage_accesses);
                    stats.registerStats(stage_access_prin);
                }
            }
        }
    }

  private:
    void init(Config &cfg)
    {
        for (int i = 0; i < cfg.num_of_channels; i++)
        {
            controllers.push_back(std::move(std::make_unique<T>(i, cfg)));
        }
        memory_addr_decoding_bits = cfg.mem_addr_decoding_bits;
    }
};

typedef PCMSimMemorySystem<FCFSController> FCFS_PCMSimMemorySystem;
typedef PCMSimMemorySystem<FRFCFSController> FR_FCFS_PCMSimMemorySystem;
typedef PCMSimMemorySystem<PLPController> PLP_PCMSimMemorySystem;
typedef PCMSimMemorySystem<CPAwareController> CP_Aware_PCMSimMemorySystem;
typedef PCMSimMemorySystem<PLPCPAwareController> CP_Aware_PLP_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LAS_PCM_Controller> LASPCM_PCMSimMemorySystem;
typedef PCMSimMemorySystem<TLDRAMPCMController> HybridDRAMPCMSystem;

class PCMSimMemorySystemFactory
{
    typedef Simulator::Config Config;
    typedef Simulator::MemObject MemObject;

  private:
    std::unordered_map<std::string,
                       std::function<std::unique_ptr<MemObject>(Config&)>> factories;

  public:
    PCMSimMemorySystemFactory()
    {
        factories["FCFS"] = [](Config &cfg)
                            {
                                return std::make_unique<FCFS_PCMSimMemorySystem>(cfg);
                            };

        factories["FR-FCFS"] = [](Config &cfg)
                            {
                                return std::make_unique<FR_FCFS_PCMSimMemorySystem>(cfg);
                            };

        factories["PLP"] = [](Config &cfg)
                            {
                                return std::make_unique<PLP_PCMSimMemorySystem>(cfg);
                            };

        factories["CP-AWARE"] = [](Config &cfg)
                                {
                                    return std::make_unique<CP_Aware_PCMSimMemorySystem>(cfg);
                                };

        factories["CP-AWARE-PLP"] = [](Config &cfg)
                            {
                                return std::make_unique<CP_Aware_PLP_PCMSimMemorySystem>(cfg);
                            };

        factories["LASPCM"] = [](Config &cfg)
                          {
                              return std::make_unique<LASPCM_PCMSimMemorySystem>(cfg);
                          };
    }

    auto createHybridSystem(Config &dram_cfg, Config &pcm_cfg)
    {
        return std::make_unique<HybridDRAMPCMSystem>(dram_cfg, pcm_cfg);
    }

    auto createPCMSimMemorySystem(Config &cfg)
    {
        std::string type = cfg.mem_controller_type;
        if (auto iter = factories.find(type);
            iter != factories.end())
        {
            return iter->second(cfg);
        }
        else
        {
            std::cerr << "Unsupported memory controller type. \n";
            exit(0);
        }
    }
};

static PCMSimMemorySystemFactory PCMSimMemorySystemFactories;
static auto createPCMSimMemorySystem(Simulator::Config &cfg)
{
    return PCMSimMemorySystemFactories.createPCMSimMemorySystem(cfg);
}

static auto createHybridSystem(Simulator::Config &dram_cfg, Simulator::Config &pcm_cfg)
{
    return PCMSimMemorySystemFactories.createHybridSystem(dram_cfg, pcm_cfg);
}

}
#endif
