#ifndef __PCMSIM_MEMORY_SYSTEM_HH__
#define __PCMSIM_MEMORY_SYSTEM_HH__

#include "../request.hh"

#include <iostream>
#include <math.h>
#include <string>
#include <vector>

namespace PCMSim
{
class Config;
class Controller;

class PCMSimMemorySystem
{
  private:
    std::vector<Controller*> controllers;
    std::vector<int> addr_bits;

  public:

    PCMSimMemorySystem(const std::string cfg_file);

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
