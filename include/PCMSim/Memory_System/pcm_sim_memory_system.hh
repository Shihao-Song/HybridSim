#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"
#include "PCMSim/CP_Aware_Controller/las_pcm_controller.hh"
#include "PCMSim/DRAM_PCM_Controller/dram_pcm_controller.hh"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PCMSim
{
// Since the main focus is PCM, the DRAM controller is simply a Tiered-Latency FR-FCFS PCM controller with 
// different timings.
template<typename PCMController>
class PCMSimMemorySystem : public Simulator::MemObject
{
  private:
    std::vector<std::unique_ptr<PCMController>> pcm_controllers;
    std::vector<std::unique_ptr<TLDRAMController>> dram_controllers;

    std::vector<int> pcm_memory_addr_decoding_bits;
    std::vector<int> dram_memory_addr_decoding_bits;

  private:
    // Only for PCM
    bool offline_req_analysis_mode = false;
    std::ofstream offline_req_ana_output;

  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Request Request;

    PCMSimMemorySystem(Config &pcm_cfg) : Simulator::MemObject()
    {
        // A PCM-Only System
        init(pcm_cfg);
    }

    PCMSimMemorySystem(Config &dram_cfg, Config &pcm_cfg) : Simulator::MemObject()
    {
        init(dram_cfg, pcm_cfg);
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

        // Pending requests at PCM memory
        for (auto &pcm_controller : pcm_controllers)
        {
            outstandings += pcm_controller->pendingRequests();
        }

        // Pending requests at DRAM memory
        for (auto &dram_controller : dram_controllers)
        {
            outstandings += dram_controller->pendingRequests();
        }

        return outstandings;
    }

    bool send(Request &req) override
    {
        // TODO, need MMU assistant to determine which memory to send
/*
        req.addr_vec.resize(int(Config::Decoding::MAX));

        Decoder::decode(req.addr, memory_addr_decoding_bits, req.addr_vec);
*/
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
/*
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
*/
        return false;
    }

    void tick() override
    {
        for (auto &pcm_controller : pcm_controllers)
        {
            pcm_controller->tick();
        }
	
        for (auto &dram_controller : dram_controllers)
        {
            dram_controller->tick();
        }
    }

    void reInitialize() override
    {
        for (auto &pcm_controller : pcm_controllers)
        {
            pcm_controller->reInitialize();
        }
	
        for (auto &dram_controller : dram_controllers)
        {
            dram_controller->reInitialize();
        }
    }

    void offlineReqAnalysis(std::string &file) override
    {
        offline_req_analysis_mode = true;
        offline_req_ana_output.open(file);

        for (auto &pcm_controller : pcm_controllers)
        {
            pcm_controller->offlineReqAnalysis(&offline_req_ana_output);
        }
    }

    void registerStats(Simulator::Stats &stats) override
    {
        // TODO, need to register stats for both DRAM and PCM
        /*
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
	*/
    }

  private:
    void init(Config &pcm_cfg)
    {
        for (int i = 0; i < pcm_cfg.num_of_channels; i++)
        {
            pcm_controllers.push_back(std::move(std::make_unique<PCMController>(i, pcm_cfg)));
        }
        pcm_memory_addr_decoding_bits = pcm_cfg.mem_addr_decoding_bits;
    }
    
    void init(Config &dram_cfg, Config &pcm_cfg)
    {
        for (int i = 0; i < dram_cfg.num_of_channels; i++)
        {
            dram_controllers.push_back(std::move(std::make_unique<TLDRAMController>(i, dram_cfg)));
        }
        dram_memory_addr_decoding_bits = dram_cfg.mem_addr_decoding_bits;

        init(pcm_cfg);
    }

};

typedef PCMSimMemorySystem<FCFSController> FCFS_PCMSimMemorySystem;
typedef PCMSimMemorySystem<FRFCFSController> FR_FCFS_PCMSimMemorySystem;
typedef PCMSimMemorySystem<CPAwareController> CP_Aware_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LAS_PCM_Controller> LASPCM_PCMSimMemorySystem;

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

        factories["CP-AWARE"] = [](Config &cfg)
                                {
                                    return std::make_unique<CP_Aware_PCMSimMemorySystem>(cfg);
                                };

        factories["LASPCM"] = [](Config &cfg)
                          {
                              return std::make_unique<LASPCM_PCMSimMemorySystem>(cfg);
                          };
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
}
#endif
