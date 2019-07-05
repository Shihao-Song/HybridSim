#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include "Sim/instruction.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"

#include <memory>
#include <deque>
#include <unordered_map>
#include <vector>

namespace CoreSystem
{
class Processor
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    typedef Simulator::Instruction Instruction;
    typedef Simulator::Request Request;

  private:
    class Window // Instruction window
    {
      public:
        static const int IPC = 4; // instruction per cycle
        static const int DEPTH = 128; // window size
        // TODO, I currently hard-coded block_mask.
        Addr block_mask = 63;

      public:
        Window() {}
        bool isFull() { return num_issues == DEPTH; }
        bool isEmpty() { return num_issues == 0; } 
        void insert(Instruction &instr)
        {
            assert(num_issues <= DEPTH);
            assert(instr.opr == Instruction::Operation::LOAD);
            pending_instructions.push_back(instr);
            ++num_issues;
            head = (head + 1) % DEPTH;
        }

        int retire()
        {
            assert(num_issues <= DEPTH);
            if (isEmpty()) { return 0; }

            int retired = 0;
            while (num_issues > 0 && retired < IPC)
            {
                if (!pending_instructions[tail].ready_to_commit)
                {
                    break;
                }
                tail = (tail + 1) % DEPTH;
                num_issues--;
                retired++;
            }

            return retired;
	}

        auto commit()
        {
            return [&](Addr addr)
            {
                if (this->num_issues == 0) {return true;}

                for (int i = 0; i < this->num_issues; i++)
                {
                    int index = (this->tail + i) % this->DEPTH;
                    Instruction &inst = this->pending_instructions[index];
                    if (inst.opr == Instruction::Operation::LOAD &&
                        inst.target_addr & this->block_mask != addr)
                    {
                        continue;
                    }
                    inst.ready_to_commit = true;
                }

                return true;
            };
        }

      private:
        std::deque<Instruction> pending_instructions;
        int num_issues = 0;
        int head = 0;
        int tail = 0;
    };

    class Core
    {
      public:
        Core(){}
    };

  public:
    Processor(std::vector<const char*> trace_lists);

    void tick();
    bool done();

  private:
    Tick cycles;
    std::vector<std::unique_ptr<Core>> cores;
};
}

#endif
