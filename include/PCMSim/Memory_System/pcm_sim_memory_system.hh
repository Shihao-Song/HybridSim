#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"
#include "PCMSim/CP_Aware_Controller/las_pcm_controller.hh"

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
        // std::cout << req.addr  << "\n";
        int memory_node = mmu->memoryNode(req);
        if (memory_node == int(Config::Memory_Node::DRAM))
        {
            // std::cout << "In DRAM. \n";

            req.addr_vec.resize(int(Config::Decoding::MAX));
            Decoder::decode(req.addr, dram_memory_addr_decoding_bits, req.addr_vec);

            int channel_id = req.addr_vec[int(Config::Decoding::Channel)];
            if(dram_controllers[channel_id]->enqueue(req))
            {
                return true;
            }
        }
        else if (memory_node == int(Config::Memory_Node::PCM))
        {
            // std::cout << "In PCM. \n";

            req.addr_vec.resize(int(Config::Decoding::MAX));
            Decoder::decode(req.addr, pcm_memory_addr_decoding_bits, req.addr_vec);
	    
            int channel_id = req.addr_vec[int(Config::Decoding::Channel)];
            if(pcm_controllers[channel_id]->enqueue(req))
            {
                return true;
            }
        }
	
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
        if constexpr (std::is_same<CPAwareController, PCMController>::value)
        {
            for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
            {
                if (m == int(Config::Memory_Node::DRAM) && 
                    dram_controllers.size() == 0)
                { continue; }

                if (m == int(Config::Memory_Node::PCM) && 
                    pcm_controllers.size() == 0)
                { continue; }

                uint64_t total_reqs = 0;
                uint64_t total_waiting_time = 0;

                if (m == int(Config::Memory_Node::DRAM))
                {
                    for (auto &controller : dram_controllers)
                    {
                        total_reqs += controller->finished_requests;
                        total_waiting_time += controller->total_waiting_time;
                    }
                }
                else if (m == int(Config::Memory_Node::PCM))
                {
                    for (auto &controller : pcm_controllers)
                    {
                        total_reqs += controller->finished_requests;
                        total_waiting_time += controller->total_waiting_time;
                    }
                }

                std::string technology = "N/A";
                if (m == int(Config::Memory_Node::DRAM))
                { technology = "DRAM_"; }
                else if (m == int(Config::Memory_Node::PCM))
                { technology = "PCM_"; }

                std::string req_info = technology + "Total_Number_Requests = " + 
                                       std::to_string(total_reqs);
                std::string waiting_info = technology + "Total_Waiting_Time = " + 
                                       std::to_string(total_waiting_time);
                std::string access_latency = technology + "Access_Latency = " + 
                                       std::to_string(double(total_waiting_time) / 
                                                      double(total_reqs));
                stats.registerStats(req_info);
                stats.registerStats(waiting_info);
                stats.registerStats(access_latency);

                unsigned num_stages = 0;
                if (m == int(Config::Memory_Node::DRAM))
                {
                    num_stages = dram_controllers[0]->numStages();
                }
                else if (m == int(Config::Memory_Node::PCM))
                {
                    num_stages = pcm_controllers[0]->numStages();
                }

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
                        if (m == int(Config::Memory_Node::DRAM))
                        {
                            for (auto &controller : dram_controllers)
                            {
                                // i - request type; j - stage ID.
                                stage_accesses += controller->stageAccess(i, j);
                            }
                        }
                        else if (m == int(Config::Memory_Node::PCM))
                        {
                            for (auto &controller : pcm_controllers)
                            {
                                // i - request type; j - stage ID.
                                stage_accesses += controller->stageAccess(i, j);
                            }
                        }

                        std::string stage_access_prin = technology + 
                                                        "Stage_" + std::to_string(j) + "_"
                                                        + target + "_Access"
                                                        + " = "
                                                        + std::to_string(stage_accesses);
                        stats.registerStats(stage_access_prin);
                    }
                }
            }
        }
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
            dram_controllers.push_back(std::move(std::make_unique<TLDRAMController>(i, 
                                                                                    dram_cfg)));
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
        std::function<std::unique_ptr<MemObject>(Config&)>> pcm_factories;

    std::unordered_map<std::string,
        std::function<std::unique_ptr<MemObject>(Config&,Config&)>> hybrid_factories;

  public:
    PCMSimMemorySystemFactory()
    {
        pcm_factories["FCFS"] = [](Config &pcm_cfg)
                            {
                                return std::make_unique<FCFS_PCMSimMemorySystem>(pcm_cfg);
                            };

        pcm_factories["FR-FCFS"] = [](Config &pcm_cfg)
                            {
                                return std::make_unique<FR_FCFS_PCMSimMemorySystem>(pcm_cfg);
                            };

        pcm_factories["CP-AWARE"] = [](Config &pcm_cfg)
                            {
                                return std::make_unique<CP_Aware_PCMSimMemorySystem>(pcm_cfg);
                            };

        pcm_factories["LASPCM"] = [](Config &pcm_cfg)
                          {
                              return std::make_unique<LASPCM_PCMSimMemorySystem>(pcm_cfg);
                          };

        hybrid_factories["CP-AWARE"] = [](Config &dram_cfg, Config &pcm_cfg)
                          {
                              return std::make_unique<CP_Aware_PCMSimMemorySystem>(dram_cfg,
                                                                                   pcm_cfg);
                          };

        hybrid_factories["LASPCM"] = [](Config &dram_cfg, Config &pcm_cfg)
                          {
                              return std::make_unique<LASPCM_PCMSimMemorySystem>(dram_cfg,
                                                                                 pcm_cfg);
                          };
 
    }

    auto createPCMMemorySystem(Config &pcm_cfg)
    {
        std::string type = pcm_cfg.mem_controller_type;
        if (auto iter = pcm_factories.find(type);
            iter != pcm_factories.end())
        {
            return iter->second(pcm_cfg);
        }
        else
        {
            std::cerr << "Unsupported memory controller type. \n";
            exit(0);
        }
    }

    auto createHybridMemorySystem(Config &dram_cfg, Config &pcm_cfg)
    {
        std::string type = pcm_cfg.mem_controller_type;
        if (auto iter = hybrid_factories.find(type);
            iter != hybrid_factories.end())
        {
            return iter->second(dram_cfg, pcm_cfg);
        }
        else
        {
            std::cerr << "Unsupported memory controller type. \n";
            exit(0);
        }
    }

};

static PCMSimMemorySystemFactory PCMSimMemorySystemFactories;
static auto createPCMMemorySystem(Simulator::Config &pcm_cfg)
{
    return PCMSimMemorySystemFactories.createPCMMemorySystem(pcm_cfg);
}

static auto createHybridMemorySystem(Simulator::Config &dram_cfg, Simulator::Config &pcm_cfg)
{
    return PCMSimMemorySystemFactories.createHybridMemorySystem(dram_cfg, pcm_cfg);
}
}
#endif
