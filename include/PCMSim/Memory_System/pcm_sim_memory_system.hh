#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "Sim/decoder.hh"
#include "Sim/mem_object.hh"
#include "Sim/config.hh"
#include "Sim/request.hh"

#include "PCMSim/Controller/pcm_sim_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"
#include "PCMSim/CP_Aware_Controller/cp_aware_controller_plp.hh"
#include "PCMSim/PLP_Controller/pcm_sim_plp_controller.hh"

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

    void registerStats(Simulator::Stats &stats) override
    {
        if constexpr (std::is_same<PLPCPAwareController, T>::value)
        {
            unsigned num_stages = controllers[0]->numStages();
            for (int i = 0; i < int(Config::Charge_Pump_Opr::MAX); i++)
            {
                std::string target = "READ";
                if (i == int(Config::Charge_Pump_Opr::READ))
                {
                    target = "READ";
                }
                if (i == int(Config::Charge_Pump_Opr::SET))
                {
                    target = "SET";
                }
                if (i == int(Config::Charge_Pump_Opr::RESET))
                {
                    target = "RESET";
                }
                
                for (int j = 0; j < num_stages; j++)
                {
                    int64_t stage_accesses = 0;
                    for (auto &controller : controllers)
                    {
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
auto createPCMSimMemorySystem(Simulator::Config &cfg)
{
    return PCMSimMemorySystemFactories.createPCMSimMemorySystem(cfg);
}
}
#endif
