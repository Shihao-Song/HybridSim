#include <iostream>
#include <fstream>

#include "Simulation.h"

// Preparing for prefetcher framework

struct HybridCfgArgs
{
    HybridCfgArgs() {}
    ~HybridCfgArgs() {}

    Config dram_cfg;
    Config pcm_cfg;
};

void prefetcherPatternsExtraction(std::string &trace, 
                                  std::string &pref_patterns_output);
 
void hybridDRAMPCMFullSystemSimulation(HybridCfgArgs &cfgs,
                                       std::vector<std::string> &trace_lists,
                                       std::vector<std::string> &pref_patterns,
                                       std::string pattern_selection,
                                       unsigned pref_num,
                                       int64_t num_instrs_per_phase,
                                       std::string &stats_output_file,
                                       std::string &pref_patterns_output);

int main(int argc, const char *argv[])
{
    auto [mode,
          dram_cfg_file,
          pcm_cfg_file,
          trace_lists,
          pref_patterns,
          pattern_selection,
          pref_num,
          num_instrs_per_phase, // # instructions for each phase, e.g., 10M, 100M...
          stats_output_file,
          pref_patterns_output] = parse_args(argc, argv);
    assert(trace_lists.size() != 0);

    if (mode == "hybrid")
    {
        HybridCfgArgs cfgs;

        cfgs.dram_cfg.setCfgFile(dram_cfg_file);
        cfgs.pcm_cfg.setCfgFile(pcm_cfg_file);

        // For a Hybrid system, the first config file should be for DRAM and the second
        // one should be PCM.
       hybridDRAMPCMFullSystemSimulation(cfgs,
                                         trace_lists,
                                         pref_patterns,
                                         pattern_selection,
	                                 pref_num,
                                         num_instrs_per_phase,
                                         stats_output_file,
                                         pref_patterns_output);
    }
    else if (mode == "pref-patterns")
    {
        assert(trace_lists.size() == 1);
        prefetcherPatternsExtraction(trace_lists[0], pref_patterns_output);
    }
    else
    {
        std::cerr << "Error: currently only support hybrid for this branch \n";
        exit(0);
    }
}

#include "Processor/pref_eval.hh"
void prefetcherPatternsExtraction(std::string &trace, 
                                  std::string &pref_patterns_output)
{
    Simulator::PrefEval prefetcher;
    prefetcher.initRuntimePrint(pref_patterns_output);

    Simulator::Trace cpu_trace(trace);

    Simulator::Instruction instr;
    bool more_instrs = cpu_trace.getInstruction(instr);

    while (more_instrs)
    {
        if (instr.opr == Simulator::Instruction::Operation::LOAD ||
            instr.opr == Simulator::Instruction::Operation::STORE)
        {
            Request req;

            if (instr.opr == Simulator::Instruction::Operation::LOAD)
            {
                req.req_type = Request::Request_Type::READ;
            }
            else
            {
                req.req_type = Request::Request_Type::WRITE;
            }

            req.core_id = 0;
            req.eip = instr.eip;
            // Use virtual address here
            req.addr = instr.target_vaddr;

            prefetcher.send(req);
            // std::cout << req.eip << " " << req.addr << "\n";
        }
        more_instrs = cpu_trace.getInstruction(instr);
    }

    // Stats stats;
    // prefetcher.registerStats(stats);
    // stats.outputStats(pref_patterns_output);
}
 
void hybridDRAMPCMFullSystemSimulation(HybridCfgArgs &cfgs,
                                       std::vector<std::string> &trace_lists,
                                       std::vector<std::string> &pref_patterns,
                                       std::string pattern_selection,
                                       unsigned pref_num,
                                       int64_t num_instrs_per_phase,
                                       std::string &stats_output_file,
                                       std::string &pref_patterns_output)
{
    unsigned num_of_cores = trace_lists.size();
    Config &dram_cfg = cfgs.dram_cfg;
    Config &pcm_cfg = cfgs.pcm_cfg;

    // Memory System Creation
    std::unique_ptr<MemObject> DRAM_PCM(createHybridSystem(dram_cfg, pcm_cfg));

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

        std::unique_ptr<MemObject> L2(createMemObject(pcm_cfg, 
                                                      Memories::L2_CACHE, 
                                                      isNonLLC));

        std::unique_ptr<MemObject> L3(createMemObject(pcm_cfg, 
                                                      Memories::L3_CACHE,
                                                      isLLC));

        L1_D->setId(i);
        L2->setId(i);
        L3->setId(i);

        L3->setBoundaryMemObject(); // Boundary memory object.

        L1_D->setNextLevel(L2.get());
        L2->setNextLevel(L3.get());
        L3->setNextLevel(DRAM_PCM.get());

        L2->setPrevLevel(L1_D.get());
        L3->setPrevLevel(L2.get());

        L1Ds.push_back(std::move(L1_D));
        L2s.push_back(std::move(L2));
        L3s.push_back(std::move(L3));
    }

    // Create MMU. We support an ML MMU. Intelligent MMU is the major focus of this
    // simulator.
    std::unique_ptr<System::MMU> mmu(createMMU(num_of_cores, dram_cfg, pcm_cfg));
    mmu->setPrefPatterns(pref_patterns, pattern_selection);
    std::vector<MemObject*> core_caches;
    for (int i = 0; i < num_of_cores; i++)
    {
        core_caches.push_back(L1Ds[i].get());
    }
    mmu->setCoreCaches(core_caches);
    mmu->setPrefNum(pref_num);
    DRAM_PCM->setMMU(mmu.get());
    
    // Create Processor
    std::unique_ptr<Processor> processor(new Processor(pcm_cfg.on_chip_frequency,
                                                       pcm_cfg.off_chip_frequency,
                                                       trace_lists, DRAM_PCM.get()));
    processor->setMMU(mmu.get());

    for (int i = 0; i < num_of_cores; i++) 
    {
        processor->setDCache(i, L1Ds[i].get());
    }

    std::cout << "\nSimulation Stage (Hybrid DRAM-PCM system)...\n\n";
    runCPUTrace(processor.get());

    // Collecting Stats
    Stats stats;

    stats.registerStats("Execution Time (CPU cycles) = " + 
                        std::to_string(processor->exeTime()));

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
    DRAM_PCM->registerStats(stats);
    
    stats.outputStats(stats_output_file);
}
