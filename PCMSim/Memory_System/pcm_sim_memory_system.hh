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
class Controller;

class PCMSimMemorySystem
{
  private:
    std::vector<Controller*> controllers;
    std::vector<int> addr_bits;

  public:
    typedef Configuration::Config Config;

    PCMSimMemorySystem(Config &cfgs);

    ~PCMSimMemorySystem();

    int pendingRequests();

    bool send(Request &req);

    void tick();

    unsigned blkSize;
    
    static constexpr float nclks_per_ns = 0.2; // Assume 200MHz clock frequency

  private:
    void init(Config &cfgs);
    void decode(Addr _addr, std::vector<int> &vec);
    int sliceLowerBits(Addr& addr, int bits);
};
}
#endif
