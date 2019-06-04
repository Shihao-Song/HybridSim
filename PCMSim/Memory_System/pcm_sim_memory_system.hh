#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "../request.hh"

#include <iostream>
#include <math.h>
#include <string>
#include <vector>

namespace Configuration
{
    class Config;
}

namespace PCMSim
{
class BaseController;

class PCMSimMemorySystem
{
  private:
    std::vector<BaseController*> controllers;
    std::vector<int> addr_bits;

    std::string mem_controller_family;
    std::string mem_controller_type;

  public:
    typedef Configuration::Config Config;

    PCMSimMemorySystem(Config &cfgs);

    ~PCMSimMemorySystem();

    int pendingRequests();

    bool send(Request &req);

    void tick();

    unsigned blkSize;
    
    const float nclks_per_ns;

  private:
    void init(Config &cfgs);
    void decode(Addr _addr, std::vector<int> &vec);
    int sliceLowerBits(Addr& addr, int bits);
};
}
#endif
