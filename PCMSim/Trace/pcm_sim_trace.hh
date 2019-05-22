#ifndef __PCMSSIM_TRACE_HH__
#define __PCMSSIM_TRACE_HH__

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include "../request.hh"

namespace PCMSim
{
class Trace
{
  public:
    Trace(const std::string trace_fname);

    bool getMemtraceRequest(Addr &req_addr, Request::Request_Type &req_type);

  private:
    std::ifstream file;
    std::string trace_name;
};
}
#endif
