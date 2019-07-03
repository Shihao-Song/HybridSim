#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "Sim/instruction.hh"
#include "Sim/request.hh"

namespace Simulator
{
class Trace
{
    typedef uint64_t Addr;

  public:
    Trace(const std::string trace_fname);

    bool getMemtraceRequest(Request &req);
    bool getInstruction(Instruction &inst);

  private:
    std::ifstream file;
    std::string trace_name;
};
}
#endif
