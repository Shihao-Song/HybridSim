#ifndef __INSTRUCTION_HH__
#define __INSTRUCTION_HH__

namespace Simulator
{
class Instruction
{
  public:
    typedef uint64_t Addr;

  public:
    Instruction(){}

    enum class Instruction_Type : int
    {
        LOAD,
        STORE,
        EXE,
        MAX	
    }instr_type;
    
    Addr eip;
    Addr target_addr; // For load and store
};
}

#endif
