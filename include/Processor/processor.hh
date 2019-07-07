#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include "Sim/instruction.hh"
#include "Sim/mapper.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"

#include <memory>
#include <deque>
#include <unordered_map>
#include <vector>

namespace CoreSystem
{
typedef uint64_t Addr;
typedef uint64_t Tick;

typedef Simulator::Instruction Instruction;
typedef Simulator::Mapper Mapper;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

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
                       (inst.target_addr & ~block_mask) == addr)
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
        Core(int _id, const char* trace_file)
            : mapper(_id),
              trace(trace_file),
              cycles(0),
              core_id(_id)
        {
            more_insts = trace.getInstruction(cur_inst);
            assert(more_insts);
        }

        void setDCache(MemObject* _d_cache) {d_cache = _d_cache;}

        void tick()
        {
            cycles++;

            d_cache->tick();
            retired += window.retire();
            if (cycles % 1000000 == 0)
            {
                std::cout << "Core: " << core_id 
                          << " has done " << retired << " instructions. \n";
	    }
            if (!more_insts) { return; }

            int inserted = 0;
            while (inserted < window.IPC && !window.isFull() && more_insts)
            {
                if (cur_inst.opr == Instruction::Operation::EXE)
                {
                    cur_inst.ready_to_commit = true;
                    window.insert(cur_inst);
                    inserted++;
                    more_insts = trace.getInstruction(cur_inst);
                }
                else
                {
                    Request req;
                    req.core_id = core_id;
                    cur_inst.target_addr = mapper.va2pa(cur_inst.target_addr);
                    req.addr = cur_inst.target_addr & ~window.block_mask;

                    if (cur_inst.opr == Instruction::Operation::LOAD)
                    {
                        req.req_type = Request::Request_Type::READ;
                        req.callback = window.commit();
                    }
                    else if (cur_inst.opr == Instruction::Operation::STORE)
                    {
                        req.req_type = Request::Request_Type::WRITE;
                    }

                    if (d_cache->send(req))
                    {
                        if (cur_inst.opr == Instruction::Operation::STORE)
                        {
                             cur_inst.ready_to_commit = true;
			}
                        window.insert(cur_inst);
                        inserted++;
                        more_insts = trace.getInstruction(cur_inst);
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        bool done()
        {
            return !more_insts && window.isEmpty();
        }

      private:
        // TODO, Mapper should be in MMU in the future.
        Mapper mapper;

        Trace trace;

        Tick cycles;
        int core_id;

        Window window;

        bool more_insts;
        Instruction cur_inst;
        uint64_t retired = 0;

        MemObject *d_cache;
        MemObject *i_cache;
    };

  public:
    Processor(std::vector<const char*> trace_lists,
              MemObject *_shared_m_obj) : cycles(0), shared_m_obj(_shared_m_obj)
    {
        unsigned num_of_cores = trace_lists.size();
        for (int i = 0; i < num_of_cores; i++)
        {
            std::cout << "Core " << i << " is assigned trace: "
                      << trace_lists[i] << "\n";
            cores.emplace_back(new Core(i, trace_lists[i]));
        }
    }

    void setDCache(int core_id, MemObject *d_cache)
    {
        cores[core_id]->setDCache(d_cache);
    }

    void tick()
    {
        cycles++;
        for (auto &core : cores)
        {
            core->tick();
        }
        if (cycles % 1000000 == 0) { std::cout << "\n"; }

        // Tick the shared cache
        shared_m_obj->tick();
    }

    bool done()
    {
        for (auto &core : cores)
        {
            if (!core->done())
            {
                return false;
            }
        }
        return true;
    }

  private:
    Tick cycles;
    std::vector<std::unique_ptr<Core>> cores;
    MemObject *shared_m_obj; // L3 and below
};
}

#endif
