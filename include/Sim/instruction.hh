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
    Addr target_addr; // For load and store

    bool ready_to_commit = true;
};
}

#endif
