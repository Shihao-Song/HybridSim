#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"
#include "PCMSim/CP_Aware_Controller/laser.hh"

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
    bool offline_req_analysis_mode = false;
    std::ofstream offline_req_ana_output;

    bool offline_cp_analysis_mode = false;
    std::ofstream offline_cp_ana_output;

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

        if (offline_cp_analysis_mode)
        {
            offline_cp_ana_output.close();
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
        else if (memory_node == int(Config::Memory_Node::PCM) || 
                 memory_node == -1)
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

    // TODO, print all necessary information to stats file.
    /*
    void offlineReqAnalysis(std::string &dir) override
    {
        offline_req_analysis_mode = true;

        std::string req_file = dir + "/req_info.csv";
        offline_req_ana_output.open(req_file);

        for (auto &pcm_controller : pcm_controllers)
        {
            pcm_controller->offlineReqAnalysis(&offline_req_ana_output);
        }

        if constexpr (std::is_same<LAS_PCM_Base, PCMController>::value || 
                      std::is_same<LAS_PCM_Static, PCMController>::value ||
                      std::is_same<LAS_PCM_Controller, PCMController>::value ||
                      std::is_same<LASER_Controller, PCMController>::value ||
                      std::is_same<LASER_2_Controller, PCMController>::value)
        {
	    offline_cp_analysis_mode = true;

            std::string cp_file = dir + "/cp_info.csv";
            offline_cp_ana_output.open(cp_file);

            for (auto &pcm_controller : pcm_controllers)
            {
                pcm_controller->offlineCPAnalysis(&offline_cp_ana_output);
            }
        }
    }
    */

    void registerStats(Simulator::Stats &stats) override
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
        }
        /*
        if constexpr (std::is_same<LAS_PCM_Base, PCMController>::value ||
                      std::is_same<LAS_PCM_Static, PCMController>::value ||
                      std::is_same<LAS_PCM_Controller, PCMController>::value ||
                      std::is_same<LASER_Controller, PCMController>::value ||
                      std::is_same<LASER_2_Controller, PCMController>::value)
        {
            for (auto &controller : pcm_controllers)
            {
                std::string prin = "PCM_Channel_" + std::to_string(controller->id)
                                   + "_Total_Idle = "
                                   + std::to_string(controller->total_idle);
                stats.registerStats(prin);

                prin = "PCM_Channel_" + std::to_string(controller->id)
                       + "_Total_PS_Aging = "
                       + std::to_string(controller->total_ps_aging);
                stats.registerStats(prin);

                prin = "PCM_Channel_" + std::to_string(controller->id)
                       + "_Total_VL_Aging = "
                       + std::to_string(controller->total_vl_aging);
                stats.registerStats(prin);

                prin = "PCM_Channel_" + std::to_string(controller->id)
                       + "_Total_SA_Aging = "
                       + std::to_string(controller->total_sa_aging);
                stats.registerStats(prin);

                prin = "PCM_Channel_" + std::to_string(controller->id)
                       + "_Total_MAX_Aging = "
                       + std::to_string(controller->total_max_aging);
                stats.registerStats(prin);

		prin = "PCM_Channel_" + std::to_string(controller->id)
                       + "_Total_Discharge = "
                       + std::to_string(controller->total_discharge);
                stats.registerStats(prin);
            }
        }
        */
        if constexpr (std::is_same<CPAwareController, PCMController>::value)
        {
            for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
            {
                std::string technology = "N/A";
                if (m == int(Config::Memory_Node::DRAM))
                { technology = "DRAM_"; }
                else if (m == int(Config::Memory_Node::PCM))
                { technology = "PCM_"; }

                unsigned num_stages = 0;
                if (m == int(Config::Memory_Node::DRAM))
                {
                    if (dram_controllers.size())
                    {
                        num_stages = dram_controllers[0]->numStages();
                    }
                }
                else if (m == int(Config::Memory_Node::PCM))
                {
                    if (pcm_controllers.size())
                    {
                        num_stages = pcm_controllers[0]->numStages();
                    }
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
/*
typedef PCMSimMemorySystem<LAS_PCM_Controller> LASPCM_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LAS_PCM_Static> LASPCM_Static_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LAS_PCM_Base> LASPCM_Base_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LASER_Controller> LASER_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LASER_2_Controller> LASER_2_PCMSimMemorySystem;
*/

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

        /*
        pcm_factories["LASER-2"] = [](Config &pcm_cfg)
                          {
                              return std::make_unique<LASER_2_PCMSimMemorySystem>(pcm_cfg);
                          };


        pcm_factories["LASER"] = [](Config &pcm_cfg)
                          {
                              return std::make_unique<LASER_PCMSimMemorySystem>(pcm_cfg);
                          };


        pcm_factories["LAS-PCM"] = [](Config &pcm_cfg)
                          {
                              return std::make_unique<LASPCM_PCMSimMemorySystem>(pcm_cfg);
                          };

        pcm_factories["LAS-PCM-Static"] = [](Config &pcm_cfg)
            {
                return std::make_unique<LASPCM_Static_PCMSimMemorySystem>(pcm_cfg);
            };

        pcm_factories["LAS-PCM-Base"] = [](Config &pcm_cfg)
            {
                return std::make_unique<LASPCM_Base_PCMSimMemorySystem>(pcm_cfg);
            };


        hybrid_factories["CP-AWARE"] = [](Config &dram_cfg, Config &pcm_cfg)
                          {
                              return std::make_unique<CP_Aware_PCMSimMemorySystem>(dram_cfg,
                                                                                   pcm_cfg);
                          };

        hybrid_factories["LASER-2"] = [](Config &dram_cfg, Config &pcm_cfg)
                          {
                              return std::make_unique<LASER_2_PCMSimMemorySystem>(dram_cfg,
                                                                                  pcm_cfg);
                          };

        hybrid_factories["LASER"] = [](Config &dram_cfg, Config &pcm_cfg)
                          {
                              return std::make_unique<LASER_PCMSimMemorySystem>(dram_cfg,
                                                                                pcm_cfg);
                          };

        hybrid_factories["LAS-PCM-Static"] = [](Config &dram_cfg, Config &pcm_cfg)
            {
                return std::make_unique<LASPCM_Static_PCMSimMemorySystem>(dram_cfg, pcm_cfg);
            };

        */
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
