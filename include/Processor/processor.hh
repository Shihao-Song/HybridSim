#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include "Sim/instruction.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"
#include "Sim/config.hh"

#include "System/mmu.hh"

#include <algorithm>
#include <list>
#include <memory>
#include <deque>
#include <unordered_map>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>

namespace CoreSystem
{
typedef uint64_t Addr;
typedef uint64_t Tick;

typedef Simulator::Instruction Instruction;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;
typedef Simulator::Config Config;

typedef System::MMU MMU;
typedef System::MMU MMU;

class Processor
{
  private:
    class Window
    {
      public:
        static const int IPC = 4; // instruction per cycle
        static const int DEPTH = 128; // window size
        // TODO, I currently hard-coded block_mask.
        Addr block_mask = 63;

      private:
        std::deque<Instruction> pending_instructions;
        int num_issues = 0;

      public:
        Window() {}
        bool isFull() { return pending_instructions.size() == DEPTH; }
        bool isEmpty() { return pending_instructions.size() == 0; } 
        void insert(Instruction &instr)
        {
            assert(pending_instructions.size() <= DEPTH);
            assert(num_issues <= DEPTH);
            pending_instructions.push_back(instr);
            ++num_issues;
        }

        int retire()
        {
            assert(pending_instructions.size() <= DEPTH);
            assert(num_issues <= DEPTH);

            if (isEmpty()) { return 0; }

            int retired = 0;
            while (num_issues > 0 && retired < IPC)
            {
                Instruction &instr = pending_instructions[0];

                if (!instr.ready_to_commit)
                {
                    break;
                }

                pending_instructions.pop_front();
                num_issues--;
                retired++;
            }

            return retired;
	}

        auto commit()
        {
            return [this](Request &req)
            {
                Addr addr = req.addr;
                if (probe_stage && 
                    int(req.hitwhere) >= int(Request::Hitwhere::L2_Clean))
                {
                    accessed_sets[(addr >> set_shift) & set_mask] = 1;
                }

                for (int i = 0; i < num_issues; i++)
                {
                    Instruction &inst = pending_instructions[i];
                    if (inst.opr == Instruction::Operation::LOAD &&
                        (inst.target_paddr & ~block_mask) == addr &&
	                !inst.ready_to_commit)
                    {
                        inst.ready_to_commit = true;
                        // std::cout << "  " 
                        //           << inst.target_paddr
                        //           << " resolved. \n";
                    }
                }

                return true;
            };
        }

      protected:
        uint64_t block_size;
        uint64_t num_sets;
        uint64_t set_shift;
        uint64_t set_mask;

        std::vector<bool> accessed_sets;

        bool probe_stage = false;

      public:
        void setPrimeProbeInfo(Config::Cache_Level _level,
                               Config& cfg)
        {
            block_size = cfg.block_size;
            block_mask = block_size - 1;
            auto size = cfg.caches[int(_level)].size * 1024;
            auto assoc = cfg.caches[int(_level)].assoc;
            num_sets = size / (block_size * assoc);
            accessed_sets.resize(num_sets);
            for (auto i = 0; i < num_sets; i++)
            {
                accessed_sets[i] = 0;
            }

            set_shift = log2(block_size);
            set_mask = num_sets - 1;
        }

        void setProbeStage()
        {
            probe_stage = true;
        }

        void resetProbeStage()
        {
            probe_stage = false;

            for (auto i = 0; i < num_sets; i++)
            {
                accessed_sets[i] = 0;
            }
        }
        
        void outputAccessInfo(std::string &_fn)
        {
            std::ofstream fd(_fn);
            for (auto i = 0; i < num_sets; i++)
            {
                fd << accessed_sets[i] << " ";
            }
            fd << "\n";
            fd.close();
        }

        auto getAccessInfo() { return accessed_sets; }
    };

    class Core
    {
      public:
        Core(int _id, std::string trace_file)
            : trace(trace_file),
              cycles(0),
              core_id(_id)
        {
            more_insts = trace.getInstruction(cur_inst);
            assert(more_insts);
        }

        void setDCache(MemObject* _d_cache) {d_cache = _d_cache;}
        void setMMU(MMU *_mmu) {mmu = _mmu;}

        /*
        // Are we in profiling stage?
        void profiling(uint64_t limit)
        {
            trace.profiling(limit);
        }
        */

        void reStartTrace() { trace.reStartTrace(); }

        // Re-initialize
        void reInitialize()
        {
            cycles = 0;
            retired = 0;
            trace.disableProfiling();

            // Re-initialize caches
            d_cache->reInitialize();

            more_insts = trace.getInstruction(cur_inst);
        }

        void tick()
        {
            cycles++;

            if (d_cache != nullptr) { d_cache->tick(); }

            int num_window_done = window.retire();
            retired += num_window_done;

            if (cycles % 1000000 == 0)
            {
                  std::cout << "Core: " << core_id 
                            << " has done " << retired << " instructions. \n";
	    }
            // (1) check if end of a trace
            if (!more_insts) { return; }
            // if (!more_insts) { phase_end = true; return; }

            int inserted = 0;
            while (inserted < window.IPC && !window.isFull() && more_insts)
            {
		if (cur_inst.opr == Instruction::Operation::EXE)
                {
                    // std::cerr << cycles << ": "
                    //           << cur_inst.thread_id << " "
                    //           << cur_inst.eip << " E" << std::endl;

                    cur_inst.ready_to_commit = true;
                    window.insert(cur_inst);
                    inserted++;
                    cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                    more_insts = trace.getInstruction(cur_inst);
                }
                else if ((cur_inst.opr == Instruction::Operation::LOAD || 
                          cur_inst.opr == Instruction::Operation::STORE))
                {
                    assert(d_cache != nullptr);
                    assert(mmu != nullptr);
                    /*
                    std::cerr << cycles << ": "
                              << cur_inst.thread_id << " " 
                              << cur_inst.eip;
                    if (cur_inst.opr == Instruction::Operation::LOAD)
                    {
                        std::cerr << " L ";
                    }
                    else if (cur_inst.opr == Instruction::Operation::STORE)
                    {
                        std::cerr << " S ";
                    }
                    std::cerr << cur_inst.target_vaddr << std::endl;

                    cur_inst.ready_to_commit = true;
                    window.insert(cur_inst);
                    inserted++;
                    cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                    more_insts = trace.getInstruction(cur_inst);
                    continue;
                    */
                    Request req; 

                    if (cur_inst.opr == Instruction::Operation::LOAD)
                    {
                        req.req_type = Request::Request_Type::READ;
                        req.callback = window.commit();
                    }
                    else if (cur_inst.opr == Instruction::Operation::STORE)
                    {
                        req.req_type = Request::Request_Type::WRITE;
                    }

                    req.core_id = core_id;
                    req.eip = cur_inst.eip;
                    // Address translation
                    // if (!cur_inst.already_translated)
                    // {
                        req.addr = cur_inst.target_vaddr; // Assign virtual first
                        cur_inst.target_paddr = req.addr;
                    //     mmu->va2pa(req);
                        // Update the instruction with the translated physical address
                    //     cur_inst.target_paddr = req.addr;
                    // }
                    // else
                    // {
                        // Already translated, no need to pass to mmu.
                    //     req.addr = cur_inst.target_paddr;
                    // }

                    // Align the address before sending to cache.
                    req.addr = req.addr & ~window.block_mask;
                    if (d_cache->send(req))
                    {
                        if (cur_inst.opr == Instruction::Operation::STORE)
                        {
                             ++num_stores;
                             cur_inst.ready_to_commit = true;
			}
                        else
                        {
                            ++num_loads;
                        }
                        window.insert(cur_inst);
                        inserted++;
                        cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                        more_insts = trace.getInstruction(cur_inst);
                    }
                    else
                    {
                        cur_inst.already_translated = true;
                        break;
                    }
                }
                else
                {
                    std::cerr << "Unsupported Instruction Type" << std::endl;
                    exit(0);
                }
            }
	    // (2) check if end of a phase
            
        }

        /*
        void recordPhase()
        {
            if (!phase_enabled) { return; }

            // std::cout << "\nPhase " << num_phases << " is done. "
            //           << "Number of retired instructions in the phase "
            //           << in_phase_tracking << "\n";

            num_phases++;
            // re-Initialize all the trackings
            phase_end = false;
        }
        */

        bool done()
        {
            // return !more_insts && window.isEmpty();
            bool issuing_done = !more_insts && window.isEmpty();

            bool cache_done = false;
            if (d_cache->pendingRequests() == 0)
            {
                cache_done = true;
            }
	    // std::cout << "Core: " << core_id << "\n";
            // std::cout << "issuing_done: " << issuing_done << "\n";
            // std::cout << "cache_done: " << cache_done << "\n";
	    return issuing_done && cache_done;
        }

        uint64_t numLoads() { return num_loads; }
        uint64_t numStores() { return num_stores; }

	bool instrDrained() { return !more_insts; }

        void registerStats(Simulator::Stats &stats)
        {
            std::string registeree_name = "Core-" + std::to_string(core_id);
            stats.registerStats(registeree_name +
                            ": Number of instructions = " + std::to_string(retired));
        }

        void SVFOracle(std::string &_trace_fn)
        {
            d_cache->outputMemContents(_trace_fn);
        }
        auto getOracleAccess() { return d_cache->getAccessInfo(); }

	void SVFAttacker(std::string &_trace_fn)
        {
            window.outputAccessInfo(_trace_fn);
        }
        auto getAttackerAccess() { return window.getAccessInfo(); }

        bool doneDrainDCacheReqs()
        {
            if (d_cache->pendingRequests() == 0) { return true; }
            else { return false; }
        }

	void drainDCacheReqs() { d_cache->tick(); }

        void reInitDCache() { d_cache->reInitialize(); }

      protected:
        bool prime_stage = false;
        std::vector<bool> accessed_sets;

        Instruction prime_probe;
        bool more_prime_probes;

      public:
        void resetVictimExe() 
        {
            more_prime_probes = trace.getPrimeProbeInstruction(prime_probe);

            d_cache->resetVictimExe();
        }

        void setVictimExe()
        {
            d_cache->setVictimExe();
        }

        void enablePP() { window.setProbeStage(); }
        void disablePP() { window.resetProbeStage(); }

        bool primeAndProbe()
        {
            if (d_cache != nullptr) { d_cache->tick(); }
            int num_window_done = window.retire();

            int inserted = 0;

            while (inserted < window.IPC && !window.isFull() && more_prime_probes)
            {
		assert(prime_probe.opr == Instruction::Operation::LOAD);

                Request req; 
                req.req_type = Request::Request_Type::READ;
                req.callback = window.commit();
                req.core_id = core_id;
                req.addr = prime_probe.target_vaddr; // Assign virtual first

                // Align the address before sending to cache.
                req.addr = req.addr & ~window.block_mask;
                if (d_cache->send(req))
                {
                    window.insert(prime_probe);
                    inserted++;
                    prime_probe.opr = Instruction::Operation::MAX; // Re-initialize
                    more_prime_probes = trace.getPrimeProbeInstruction(prime_probe);
                }
                else
                {
                    break;
                }
            }

            return more_prime_probes; 
        }

        auto setPrimeProbeInfo(Config::Cache_Level _level,
                               Config &cfg)
        {
            auto num_sets = trace.setPrimeProbeInfo(_level, cfg);
            window.setPrimeProbeInfo(_level, cfg);

            return num_sets;
        }

      private:
        // When evaluting branch predictors, MMU is allowed to be NULL.
        MMU *mmu = nullptr;

        Trace trace;

        Tick cycles;
        uint64_t num_loads = 0;;
        uint64_t num_stores = 0;;

        int core_id;

        Window window;

        bool more_insts;
        Instruction cur_inst;
        uint64_t retired = 0;

        // When evaluating branch predictors, d/i-cache is allowed to be NULL
        MemObject *d_cache = nullptr;
        MemObject *i_cache = nullptr;

    };

  public:
    Processor(float on_chip_frequency, float off_chip_frequency,
              std::vector<std::string> trace_lists,
              MemObject *_shared_m_obj) : cycles(0), shared_m_obj(_shared_m_obj)
    {
        unsigned num_of_cores = trace_lists.size();
        for (int i = 0; i < num_of_cores; i++)
        {
            std::cout << "Core " << i << " is assigned trace: "
                      << trace_lists[i] << "\n";
            cores.emplace_back(new Core(i, trace_lists[i]));
        }
        
        if (shared_m_obj->isOnChip()) { nclks_to_tick_shared = 1; }
        else { nclks_to_tick_shared = on_chip_frequency / off_chip_frequency; }
    }

    void setDCache(int core_id, MemObject *d_cache)
    {
        cores[core_id]->setDCache(d_cache);
    }

    void setMMU(MMU *_mmu)
    {
        mmu = _mmu;

        for (auto &core : cores)
        {
            core->setMMU(_mmu);
        }
    }

    // Re-initialize
    void reInitialize()
    {
        cycles = 0;

        // Re-initialize each core with its local cache.
        for (auto &core : cores)
        {
            core->reInitialize();
        }

        // Re-initialize the shared memory object
        shared_m_obj->reInitialize();
    }

    // Set number of instructions per execution phase, this helps us to better
    // monitor program behavior.
    void numClksPerPhase(int64_t _num_clks_per_phase)
    {
        if (_num_clks_per_phase <= 0) { phase_enabled = false; return; }

        // Enable phase-phase execution
        phase_enabled = true;
        // Initialize phase_end (end of a phase) to FALSE
        phase_end = false;

        // Set the number of instructions per phase (remain constant)
        num_clks_per_phase = _num_clks_per_phase;
    }

    void reStartTrace()
    {
        for (auto &core : cores)
        {
            core->reStartTrace();
        }
    }

    void tick()
    {
        if (phase_enabled && (cycles %  num_clks_per_phase == 0))
        {
            std::vector<bool> o_trace;
            std::vector<bool> a_trace;
            if (num_phases > 0)
            {
                // First of all, we want to extract the oracle traces
                std::string trace_fn = std::to_string(num_phases)
                                     + ".oracle";
                if (svf_trace_dir.back() == '/') 
                { trace_fn = svf_trace_dir + trace_fn; }
                else { trace_fn = svf_trace_dir + "/" + trace_fn; }
                cores[0]->SVFOracle(trace_fn);
                o_trace = cores[0]->getOracleAccess();

                // Notify cache that we are in prime and probe stage
                // The function also clears the information from previous
                // stages
                cores[0]->resetVictimExe();

                // Now, we probe the stage
                cores[0]->enablePP();
            }
            else
            {
                // We only prime the cache in the first stage
                cores[0]->resetVictimExe();
                cores[0]->disablePP();
            }

            Tick fake_clk = cycles;
            while (true)
            {
                bool cond = cores[0]->primeAndProbe();
                if (fake_clk % nclks_to_tick_shared == 0)
                {
                    // Tick the shared
                    shared_m_obj->tick();
                }
                fake_clk++;

                if (!cond) break;
            }

            if (num_phases > 0)
            {
                // Lastly, we extract the attacker
                std::string trace_fn = std::to_string(num_phases)
                                     + ".attacker";
                if (svf_trace_dir.back() == '/') 
                { trace_fn = svf_trace_dir + trace_fn; }
                else { trace_fn = svf_trace_dir + "/" + trace_fn; }
                cores[0]->SVFAttacker(trace_fn);
                a_trace = cores[0]->getAttackerAccess();

                // Now, we disable probe the stage
                cores[0]->disablePP();
            }

            // Notify cache that we are out of prime and probe stage
            cores[0]->setVictimExe();

            // Calculate similarity
            if (num_phases > 0)
            {
                assert(o_trace.size() == a_trace.size());
                unsigned diff = 0;
                for (auto i = 0; i < o_trace.size(); i++)
                {
                    if (o_trace[i] != a_trace[i]) diff++;
                }

                if (diff > max_diff_sets) max_diff_sets = diff;
                if (diff < min_diff_sets) min_diff_sets = diff;
                total_diff += diff;
            }

            num_phases++;

            if (num_phases > num_phases_limit)
            {
                std::cerr << "\nCache size: " << num_sets << " sets\n";
                std::cerr << "Max. diff between oracle and attacker: " << max_diff_sets << "\n";
                std::cerr << "Min. diff between oracle and attacker: " << min_diff_sets << "\n";
                std::cerr << "Avg. diff between oracle and attacker: " << (total_diff / num_phases_limit) << "\n";
                
                exit(0);
            }
        }

        cycles++;

        // std::cout << cycles << "\n";
        for (auto &core : cores)
        {
            core->tick();
        }
        // if (cycles % 1000000 == 0) { std::cout << "\n"; }

        if (cycles % nclks_to_tick_shared == 0)
        {
            // Tick the shared
            shared_m_obj->tick();
        }
    }

    bool done()
    {
        // (1) All the instructions are run-out
        for (auto &core : cores)
        {
            if (!core->done())
            {
                return false;
            }
        }
        // (2) All the memory requests including mshr requests, evictions 
        //     are finished.
        if (shared_m_obj->pendingRequests() != 0)
	{
            // std::cout << "Shared not done!!.\n";
            return false;
        }
        // std::cout << "Shared is done.\n";
        return true;
    }

    Tick exeTime() const { return cycles; }

    uint64_t numStores() const
    {
        uint64_t num_stores = 0;
        for (auto &core : cores)
        {
            num_stores += core->numStores();
        }
        return num_stores;
    }

    uint64_t numLoads() const
    {
        uint64_t num_loads = 0;
        for (auto &core : cores)
        {
            num_loads += core->numLoads();
        }
        return num_loads;
    }

    void registerStats(Simulator::Stats &stats)
    {
        for (auto &core : cores) { core->registerStats(stats); }
    }

    void setSVFTraceDir(std::string &_svf_trace_dir)
    {
        svf_trace_dir = _svf_trace_dir;

        if (!dirExists(svf_trace_dir.c_str()))
        {
            std::cerr << "Error: trace dir does not exits!\n";
            exit(0);
        }
    }

    void setPrimeProbeInfo(Config::Cache_Level _level,
                           Config &cfg,
                           unsigned _phases)
    {
        num_phases_limit = _phases;

        for (auto &core : cores)
        {
            num_sets = core->setPrimeProbeInfo(_level, cfg);
        }
    }

  protected:
    int dirExists(const char *path)
    {
        struct stat info;

        if(stat( path, &info ) != 0)
            return 0;
        else if(info.st_mode & S_IFDIR)
            return 1;
        else
            return 0;
    }

  private:
    Tick cycles;

    MMU *mmu = nullptr;
    std::vector<std::unique_ptr<Core>> cores;
    MemObject *shared_m_obj;

    unsigned nclks_to_tick_shared;

    // Phase analysis
    std::string svf_trace_dir;

    unsigned num_phases = 0;
    unsigned num_phases_limit = 0;

    unsigned num_sets;
    unsigned max_diff_sets = 0;
    unsigned min_diff_sets = (unsigned) - 1;
    unsigned total_diff = 0;

    uint64_t num_clks_per_phase;
    bool phase_enabled = false;
    bool phase_end = false;
};
}

#endif
