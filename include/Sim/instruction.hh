#ifndef __INSTRUCTION_HH__
#define __INSTRUCTION_HH__

#include <string>

namespace Simulator
{
class Instruction
{
  public:
    typedef uint64_t Addr;

  public:
    Instruction(){}

    enum class Operation : int
    {
        BRANCH,
        LOAD,
        STORE,
        EXE,
        MAX	
    }opr = Operation::MAX;

    Addr eip;
    Addr target_vaddr; // Target address to load or store (virtual address)
    Addr target_paddr; // Target address to load or store (physical address)

    bool taken; // If the branch is taken.
    Addr branch_target; // The resolved branch target.

    uint64_t size; // Size of data to be loaded or stored, 
                   // we are not utilizing this field currently.

    bool ready_to_commit = false;
    bool already_translated = false;

    unsigned thread_id; // From the trace file
    unsigned process_id; // TODO, assigned by our simulator.
};
}

#endif
