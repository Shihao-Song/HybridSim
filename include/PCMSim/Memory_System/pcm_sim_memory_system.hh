#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/laser.hh"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// TODO, delete all the memory controller except the basic ones
// TODO, don't forget to clean the request section

namespace PCMSim
{
// Since the main focus is PCM, the DRAM controller is simply a Tiered-Latency 
// FR-FCFS PCM controller with different timings.
template<typename PCMController>
class PCMSimMemorySystem : public Simulator::MemObject
{
  private:
    std::vector<std::unique_ptr<PCMController>> pcm_controllers;
    std::vector<std::unique_ptr<DRAMController>> dram_controllers;

    std::vector<int> pcm_memory_addr_decoding_bits;
    std::vector<int> dram_memory_addr_decoding_bits;

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

        on_chip = false;
    }

    PCMSimMemorySystem(Config &dram_cfg, Config &pcm_cfg) : Simulator::MemObject()
    {
        init(dram_cfg, pcm_cfg);

        on_chip = false;
    }

    ~PCMSimMemorySystem()
    {}

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

            if constexpr (std::is_same<CP_Static, PCMController>::value ||
                          std::is_same<LASER_1_Controller, PCMController>::value ||
                          std::is_same<LASER_2_Controller, PCMController>::value)
            {
                if (m == int(Config::Memory_Node::PCM))
                {
                    uint64_t max_on_time = 0;
                    uint64_t min_on_time = (uint64_t) - 1;
                    float max_aging = 0.0;

                    for (auto &ctrl : pcm_controllers)
                    {
                        max_on_time = (max_on_time < ctrl->max_on_time) ? ctrl->max_on_time 
                                                                        : max_on_time;

                        min_on_time = (min_on_time > ctrl->min_on_time) ? ctrl->min_on_time 
                                                                        : min_on_time;

                        max_aging = (max_aging < ctrl->max_aging) ? ctrl->max_aging
                                                                  : max_aging;
                    }

                    std::string str_max_on = technology + "Max_On_Time = " + 
                                             std::to_string(max_on_time);
                    std::string str_min_on = technology + "Min_On_Time = " +
                                             std::to_string(min_on_time);
                    std::string str_max_aging = technology + "Max_Aging = " +
                                             std::to_string(max_aging);

                    stats.registerStats(str_max_on);
                    stats.registerStats(str_min_on);
                    stats.registerStats(str_max_aging);
                }
            }
            stats.registerStats(req_info);
            stats.registerStats(waiting_info);
            stats.registerStats(access_latency);
        }
    }

  private:
    void init(Config &pcm_cfg)
    {
        for (int i = 0; i < pcm_cfg.num_of_channels; i++)
        {
            pcm_controllers.push_back(std::move(std::make_unique<PCMController>(i, 
                pcm_cfg)));
        }
        pcm_memory_addr_decoding_bits = pcm_cfg.mem_addr_decoding_bits;
    }
    
    void init(Config &dram_cfg, Config &pcm_cfg)
    {
        for (int i = 0; i < dram_cfg.num_of_channels; i++)
        {
            dram_controllers.push_back(std::move(std::make_unique<DRAMController>(i, 
                dram_cfg)));
        }
        dram_memory_addr_decoding_bits = dram_cfg.mem_addr_decoding_bits;

        init(pcm_cfg);
    }

};

typedef PCMSimMemorySystem<FCFSController> FCFS_PCMSimMemorySystem;
typedef PCMSimMemorySystem<FRFCFSController> FR_FCFS_PCMSimMemorySystem;

typedef PCMSimMemorySystem<CP_Static> CP_Static_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LASER_1_Controller> LASER_1_PCMSimMemorySystem;
typedef PCMSimMemorySystem<LASER_2_Controller> LASER_2_PCMSimMemorySystem;


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
	
        hybrid_factories["FCFS"] = [](Config &dram_cfg, Config &pcm_cfg)
                    {
                        return std::make_unique<FCFS_PCMSimMemorySystem>(dram_cfg,
                            pcm_cfg);
                    };

        hybrid_factories["FR-FCFS"] = [](Config &dram_cfg, Config &pcm_cfg)
                    {
                        return std::make_unique<FR_FCFS_PCMSimMemorySystem>(dram_cfg,
                            pcm_cfg);
                    };
	
        hybrid_factories["HEBE"] = [](Config &dram_cfg, Config &pcm_cfg)
                    {
                        return std::make_unique<LASER_1_PCMSimMemorySystem>(dram_cfg,
                            pcm_cfg);
                    };
	
        hybrid_factories["HEBE-D"] = [](Config &dram_cfg, Config &pcm_cfg)
                    {
                        return std::make_unique<LASER_2_PCMSimMemorySystem>(dram_cfg,
                            pcm_cfg);
                    };

        hybrid_factories["CP_Static"] = [](Config &dram_cfg, Config &pcm_cfg)
                    {
                        return std::make_unique<CP_Static_PCMSimMemorySystem>(dram_cfg,
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

static auto createHybridMemorySystem(Simulator::Config &dram_cfg, 
                                     Simulator::Config &pcm_cfg)
{
    return PCMSimMemorySystemFactories.createHybridMemorySystem(dram_cfg, pcm_cfg);
}
}
#endif
