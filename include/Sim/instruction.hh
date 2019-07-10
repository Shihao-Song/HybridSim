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
        LOAD,
        STORE,
        EXE,
        MAX	
    }opr;

    Addr eip;
    Addr target_addr; // Target address to load or store.
    uint64_t size; // Size of data to be loaded or stored, 
                   // we are not utilizing this field currently.

    bool ready_to_commit = false;
};
}

#endif
