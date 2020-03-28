#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include "Processor/Branch_Predictor/branch_predictor_factory.hh"
#include "Sim/instruction.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"

#include "System/mmu.hh"

#include <algorithm>
#include <list>
#include <memory>
#include <deque>
#include <unordered_map>
#include <vector>

namespace CoreSystem
{
typedef uint64_t Addr;
typedef uint64_t Tick;

typedef Simulator::Instruction Instruction;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

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
            return [this](Addr addr)
            {
                for (int i = 0; i < num_issues; i++)
                {
                    Instruction &inst = pending_instructions[i];
                    if (inst.opr == Instruction::Operation::LOAD &&
                       (inst.target_paddr & ~block_mask) == addr)
                    {
                        inst.ready_to_commit = true;
                    }
                }

                return true;
            };
        }

    };

    class Core
    {
      public:
        Core(int _id, std::string trace_file, std::string bp_type)
            : bp(createBP(bp_type)),
              trace(trace_file),
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

            // Absorb all the misprediction penalty
            while (mispred_penalty)
            { mispred_penalty--; return; }

            int num_window_done = window.retire();
            retired += num_window_done;
            if (phase_enabled) { in_phase_tracking += num_window_done; };

            if (cycles % 1000000 == 0)
            {
                  std::cout << "Core: " << core_id 
                            << " has done " << retired << " instructions. \n";
	    }
            // (1) check if end of a trace
            // if (!more_insts) { return; }
            if (!more_insts) { phase_end = true; return; }
            // (2) check if end of a phase
            if (phase_enabled)
            {
                if (in_phase_tracking >= num_instrs_per_phase) { phase_end = true; return; }
            }
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
                else if (cur_inst.opr == Instruction::Operation::BRANCH)
                {
                    // std::cerr << cycles << ": "
                    //           << cur_inst.thread_id << " "
                    //           << cur_inst.eip << " B "
                    //           << cur_inst.taken << " "
                    //           << cur_inst.branch_target << std::endl;

                    cur_inst.ready_to_commit = true;
                    window.insert(cur_inst);
                    inserted++;

                    // If there is a branch misprediction, stall the processor
                    // for 15 clock cycles (a typical misprediction penalty).
                    if (!bp->predict(cur_inst))
                    {
                        cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                        more_insts = trace.getInstruction(cur_inst);
                        mispred_penalty = 15;
                        break; // No new instruction should be issued before penalty
                               // is completely resolved.
                    }
                    cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                    more_insts = trace.getInstruction(cur_inst);
                }
                else if (cur_inst.opr == Instruction::Operation::LOAD || 
                         cur_inst.opr == Instruction::Operation::STORE)
                {
                    assert(d_cache != nullptr);
                    assert(mmu != nullptr);
                    /*
                    std::cerr << cur_inst.thread_id << " " << cur_inst.eip;
                    if (cur_inst.opr == Instruction::Operation::LOAD)
                    {
                        std::cerr << " L ";
                    }
                    else if (cur_inst.opr == Instruction::Operation::STORE)
                    {
                        std::cerr << " S ";
                    }
                    std::cerr << cur_inst.target_vaddr << std::endl;
                    */
                    /*
                    cur_inst.ready_to_commit = true;
                    window.insert(cur_inst);
                    inserted++;
                    cur_inst.opr = Instruction::Operation::MAX; // Re-initialize
                    more_insts = trace.getInstruction(cur_inst);
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
                    if (!cur_inst.already_translated)
                    {
                        req.addr = cur_inst.target_vaddr; // Assign virtual first
                        mmu->va2pa(req);
                        // Update the instruction with the translated physical address
                        cur_inst.target_paddr = req.addr;
                    }
                    else
                    {
                        // Already translated, no need to pass to mmu.
                        req.addr = cur_inst.target_paddr;
                    }

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
        }

        void numInstPerPhase(int64_t _num_instrs_per_phase)
        {
            if (_num_instrs_per_phase <= 0) { phase_enabled = false; return; }

            // Enable phase-phase execution
            phase_enabled = true;
            // Initialize phase_end (end of a phase) to FALSE
            phase_end = false;

            // Set the number of instructions per phase (remain constant)
            num_instrs_per_phase = _num_instrs_per_phase;
            // Track how many instructions have been completed in one phase
            in_phase_tracking = 0;
        }

        void recordPhase()
        {
            if (!phase_enabled) { return; }

            // std::cout << "\nPhase " << num_phases << " is done. "
            //           << "Number of retired instructions in the phase "
            //           << in_phase_tracking << "\n";

            num_phases++;
            // re-Initialize all the trackings
            phase_end = false;
            in_phase_tracking = 0;

            // Signal the MMU for an phase end.
            // Only let core 0 to signal is enough.
            if (core_id == 0) { mmu->phaseDone(); }
        }

        bool endOfPhase()
	{
            return phase_end;
        }

        bool done()
        {
            // return !more_insts && window.isEmpty();
            bool issuing_done = !more_insts && window.isEmpty();

            // When evaluating branch predictors, d/i-cache maybe set to NULL.
            if (d_cache == nullptr) { return issuing_done; }

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

        void BPEvalMode()
        {
            if (cur_inst.opr == Instruction::Operation::LOAD ||
                cur_inst.opr == Instruction::Operation::STORE)
            {
                cur_inst.opr = Instruction::Operation::EXE;
            }
            trace.BPEvalMode();
        }

        void registerStats(Simulator::Stats &stats)
        {
            std::string registeree_name = "Core-" + std::to_string(core_id);
            stats.registerStats(registeree_name +
                            ": Number of instructions = " + std::to_string(retired));

            stats.registerStats(registeree_name +
                            "-BP: Number of correct predictions = " + 
                            std::to_string(bp->getCorPreds()));
            stats.registerStats(registeree_name +
                            "-BP: Number of in-correct predictions = " +
                            std::to_string(bp->getInCorPreds()));
        }

      private:
        std::unique_ptr<Branch_Predictor> bp;

        unsigned mispred_penalty = 0;

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

        // Phase analysis
        unsigned num_phases = 0;

        uint64_t num_instrs_per_phase;
        uint64_t in_phase_tracking = 0;
        bool phase_enabled = false;
        bool phase_end = false;

        // When evaluating branch predictors, d/i-cache is allowed to be NULL
        MemObject *d_cache = nullptr;
        MemObject *i_cache = nullptr;
    };

  public:
    Processor(float on_chip_frequency, float off_chip_frequency,
              std::vector<std::string> trace_lists,
              MemObject *_shared_m_obj,
              std::string bp_type = "tournament") : cycles(0), shared_m_obj(_shared_m_obj)
    {
        unsigned num_of_cores = trace_lists.size();
        for (int i = 0; i < num_of_cores; i++)
        {
            std::cout << "Core " << i << " is assigned trace: "
                      << trace_lists[i] << "\n";
            cores.emplace_back(new Core(i, trace_lists[i], bp_type));
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
    void numInstPerPhase(int64_t num_instrs_per_phase)
    {
        for (auto &core : cores)
        {
            core->numInstPerPhase(num_instrs_per_phase);
        }
    }
    /*
    // Are we in profiling stage?
    void profiling(std::vector<uint64_t> profiling_limits)
    {
        assert(cores.size() == profiling_limits.size());
        for (int i = 0; i < cores.size(); i++)
        {
            cores[i]->profiling(profiling_limits[i]);
        }
    }
    */

    void reStartTrace()
    {
        for (auto &core : cores)
        {
            core->reStartTrace();
        }
    }

    void tick()
    {
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

        // If all the instructions are drained, there is no need to proceed.
        bool all_drained = true;
        for (auto &core : cores)
        {
            if (core->instrDrained() == false) { all_drained = false; }
        }
        if (all_drained) { return; }

        // Check if the end of an execution phase
        for (auto &core : cores)
        {
            if (!core->endOfPhase()) { return; }
        }

        assert(mmu != nullptr);
        if (!mmu->pageMig()) { return; } // Only proceed when the 
                                            // page migration is done.

        // All cores reach the end of a execution phase
        for (auto &core : cores)
	{
            core->recordPhase();
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

    void BPEvalMode()
    {
        for (auto &core : cores)
        {
            core->BPEvalMode();
        }
    }

    void registerStats(Simulator::Stats &stats)
    {
        for (auto &core : cores) { core->registerStats(stats); }
    }

  private:
    Tick cycles;

    MMU *mmu = nullptr;
    std::vector<std::unique_ptr<Core>> cores;
    MemObject *shared_m_obj;

    unsigned nclks_to_tick_shared;
};
}

#endif
