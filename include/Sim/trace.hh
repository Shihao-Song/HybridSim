#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include "Sim/request.hh"

namespace Simulator
{
class Trace
{
    typedef uint64_t Addr;

  public:
    Trace(const std::string trace_fname);

    bool getMemtraceRequest(Request &req);

  private:
    std::ifstream file;
    std::string trace_name;
};
}
#endif
