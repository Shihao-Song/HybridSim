#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

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
                    req.v_addr = cur_inst.target_vaddr;
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

            return issuing_done;
            /*
            bool cache_done = false;
            if (d_cache->pendingRequests() == 0)
            {
                cache_done = true;
            }
	    // std::cout << "Core: " << core_id << "\n";
            // std::cout << "issuing_done: " << issuing_done << "\n";
            // std::cout << "cache_done: " << cache_done << "\n";
	    return issuing_done && cache_done;
            */
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

        void SVFGen(std::string &_trace_fn)
        {
            d_cache->outputMemContents(_trace_fn);
        }

        bool doneDrainDCacheReqs()
        {
            if (d_cache->pendingRequests() == 0) { return true; }
            else { return false; }
        }

	void drainDCacheReqs() { d_cache->tick(); }

        void reInitDCache() { d_cache->reInitialize(); }

        uint64_t numInstrsRetired() { return retired; }

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
    }

    bool done()
    {
        // TODO, quit if it reaches 200M instruction (for quick evaluation)
        // if (cores[0]->numInstrsRetired() >= 200000000) return true;

        // (1) All the instructions are run-out
        for (auto &core : cores)
        {
            if (!core->done())
            {
                return false;
            }
        }

        return true;
        /*
        // (2) All the memory requests including mshr requests, evictions 
        //     are finished.
        if (shared_m_obj->pendingRequests() != 0)
	{
            // std::cout << "Shared not done!!.\n";
            return false;
        }
        // std::cout << "Shared is done.\n";
        return true;
        */
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

  private:
    Tick cycles;

    MMU *mmu = nullptr;
    std::vector<std::unique_ptr<Core>> cores;
    MemObject *shared_m_obj;

    unsigned nclks_to_tick_shared;
};
}

#endif
