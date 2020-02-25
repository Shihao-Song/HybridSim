#include <iostream>
#include <fstream>

#include "Simulation.h"

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file);

void MemControllerDesign(std::string &mem_trace, std::string &stats_output_file);

void TraceGen(std::vector<Config> &cfgs,
              std::vector<std::string> &trace_lists,
              std::string &trace_output_file,
              std::string &stats_output_file);

int main(int argc, const char *argv[])
{
    auto [mode,
          dram_cfg_file,
          pcm_cfg_file,
          trace_lists,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          stats_output_file,
          trace_output_file] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);

    if (mode == "mem-ctrl-design")
    {
        MemControllerDesign(trace_lists[0], stats_output_file);
    }

    /*
    std::vector<Config> cfgs;
    cfgs.emplace_back(dram_cfg_file);
    cfgs.emplace_back(pcm_cfg_file);

    if (trace_output_file != "N/A")
    {
        TraceGen(cfgs,
                 trace_lists,
                 trace_output_file,
                 stats_output_file);
    }
    else
    {
        // For a Hybrid system, the first config file should be for DRAM and the second
        // one should be PCM.
        Hybrid_DRAM_PCM_Full_System_Simulation(cfgs,
                                               trace_lists,
                                               num_instrs_per_phase,
                                               stats_output_file);
    }
    */
}

void Hybrid_DRAM_PCM_Full_System_Simulation(std::vector<Config> &cfgs,
                                            std::vector<std::string> &trace_lists,
                                            int64_t num_instrs_per_phase,
                                            std::string &stats_output_file)
{
    // TODO, for any shared caches, multiply their mshr and wb sizes to num_of_cores, please
    // see our example configuration files for more information.
    unsigned num_of_cores = trace_lists.size();
    Config &dram_cfg = cfgs[0];
    Config &pcm_cfg = cfgs[1];

    // Memory System Creation
    std::unique_ptr<MemObject> DRAM_PCM(createHybridSystem(dram_cfg, pcm_cfg));
    // TODO, delete offline... function, we should only output important information to 
    // the stats file only.
    // DRAM_PCM->offlineReqAnalysis(offline_request_analysis_dir);

    // Cache system
    std::vector<std::unique_ptr<MemObject>> L1Ds;
    std::vector<std::unique_ptr<MemObject>> L2s;
    std::vector<std::unique_ptr<MemObject>> L3s;
    // Skylake cache system
    for (int i = 0; i < num_of_cores; i++)
    {
        std::unique_ptr<MemObject> L1_D(createMemObject(pcm_cfg,
                                                        Memories::L1_D_CACHE,
                                                        isNonLLC));

        std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, Memories::L2_CACHE, isNonLLC));

        std::unique_ptr<MemObject> L3(createMemObject(pcm_cfg, Memories::L3_CACHE, isLLC));

        L1_D->setId(i);
        L2->setId(i);
        L3->setId(i);

        L3->setBoundaryMemObject(); // Boundary memory object.

        L1_D->setNextLevel(L2.get());
        L2->setNextLevel(L3.get());
        L3->setNextLevel(DRAM_PCM.get());

        L2->setPrevLevel(L1_D.get());
        L3->setPrevLevel(L2.get());

        L3->setInclusive(); // L3 is inclusive (to L2 and L1-D)

        L1Ds.push_back(std::move(L1_D));
        L2s.push_back(std::move(L2));
        L3s.push_back(std::move(L3));
    }

    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, dram_cfg, pcm_cfg));
    mmu->setMemSystem(DRAM_PCM.get());
    DRAM_PCM->setMMU(mmu.get());

    // Create Processor
    std::unique_ptr<Processor> processor(new Processor(pcm_cfg.on_chip_frequency,
                                                       pcm_cfg.off_chip_frequency,
                                                       trace_lists, DRAM_PCM.get()));
    processor->setMMU(mmu.get());
    processor->numInstPerPhase(num_instrs_per_phase);
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1Ds[i].get());
    }

    std::cout << "\nSimulation Stage (Hybrid DRAM-PCM system)...\n\n";
    runCPUTrace(processor.get());

    // Collecting Stats
    Stats stats;

    processor->registerStats(stats);

    for (auto &L1D : L1Ds)
    {
        L1D->registerStats(stats);
    }
    for (auto &L2 : L2s)
    {
        L2->registerStats(stats);
    }
    for (auto &L3 : L3s)
    {
        L3->registerStats(stats);
    }

    mmu->registerStats(stats);
    stats.registerStats("Execution Time (CPU cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
}

#include "MemSim/qos/qos_base.hh"
void MemControllerDesign(std::string &trace, std::string &stats_output_file)
{
    Simulator::Trace mem_trace(trace);

    Request req;

    std::cout << "\nMemory Controller Exploration Mode...\n";
    uint64_t Tick = 0;
    bool stall = false;
    bool end = false;
   
    std::unique_ptr<MemObject> mem = std::make_unique<QoS::QoSBase>();

    while (!end || mem->pendingRequests())
    {
        if (!end && !stall)
        {
            end = !(mem_trace.getMemtraceRequest(req));
        }

        if (!end)
        {
            stall = !(mem->send(req));
        }

        mem->tick();
        ++Tick;
    }
}

void TraceGen(std::vector<Config> &cfgs,
              std::vector<std::string> &trace_lists,
              std::string &trace_output_file,
              std::string &stats_output_file)
{
    // TODO, for any shared caches, multiply their mshr and wb sizes to num_of_cores, please
    // see our example configuration files for more information.
    unsigned num_of_cores = trace_lists.size();
    Config &cfg = cfgs[0];

    // Memory System Creation
    std::unique_ptr<MemObject> trace_probe = 
        std::make_unique<Simulator::TraceProbe>(trace_output_file);

    // Cache system
    std::vector<std::unique_ptr<MemObject>> L1Ds;
    std::vector<std::unique_ptr<MemObject>> L2s;
    std::vector<std::unique_ptr<MemObject>> L3s;
    // Skylake cache system
    for (int i = 0; i < num_of_cores; i++)
    {
        std::unique_ptr<MemObject> L1_D(createMemObject(cfg,
                                                        Memories::L1_D_CACHE,
                                                        isNonLLC));

        std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isNonLLC));

        std::unique_ptr<MemObject> L3(createMemObject(cfg, Memories::L3_CACHE, isLLC));

        L1_D->setId(i);
        L2->setId(i);
        L3->setId(i);

        L3->setBoundaryMemObject(); // Boundary memory object.

        L1_D->setNextLevel(L2.get());
        L2->setNextLevel(L3.get());
        L3->setNextLevel(trace_probe.get());

        L2->setPrevLevel(L1_D.get());
        L3->setPrevLevel(L2.get());

        L3->setInclusive();

        L1Ds.push_back(std::move(L1_D));
        L2s.push_back(std::move(L2));
        L3s.push_back(std::move(L3));
    }

    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, cfg));

    // Create Processor
    std::unique_ptr<Processor> processor(new Processor(cfg.on_chip_frequency,
                                                       cfg.off_chip_frequency,
                                                       trace_lists, trace_probe.get()));
    processor->setMMU(mmu.get());
    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1Ds[i].get());
    }

    std::cout << "\nTrace Generating...\n\n";
    runCPUTrace(processor.get());

    // Collecting Stats
    Stats stats;

    processor->registerStats(stats);

    for (auto &L1D : L1Ds)
    {
        L1D->registerStats(stats);
    }
    for (auto &L2 : L2s)
    {
        L2->registerStats(stats);
    }
    for (auto &L3 : L3s)
    {
        L3->registerStats(stats);
    }

    mmu->registerStats(stats);
    stats.registerStats("Execution Time (CPU cycles) = " + 
                        std::to_string(processor->exeTime()));
    stats.outputStats(stats_output_file);
}

