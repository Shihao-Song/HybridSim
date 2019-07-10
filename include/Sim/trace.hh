#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "protobuf/cpu_trace.pb.h"
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
    CPUTrace::TraceFile trace_file;
    uint64_t instruction_index = 0;
};
}
#endif
