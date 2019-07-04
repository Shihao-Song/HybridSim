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
        Addr block_mask = 63; // default

      public:
        Window() {}
        void setBlockMask(unsigned _mask) { block_mask = _mask; }
        bool isFull() { return pending_instructions.size() == DEPTH; }
        bool isEmpty() { return pending_instructions.size() == 0; } 
        void insert(Instruction &instr)
        {
            assert(instr.opr == Instruction::Operation::LOAD);
            Addr block_addr = instr.target_addr & ~block_mask;
             
        }

      private:
        std::deque<Instruction> pending_instructions;
        int load = 0;
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
