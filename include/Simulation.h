#ifndef __SIMULATION_HH__
#define __SIMULATION_HH__

#include "PCMSim/Memory_System/pcm_sim_memory_system.hh"

typedef Simulator::Config Config;
typedef Simulator::MemObject MemObject;

enum class Memories : int
{
    L1_CACHE,
    L2_CACHE,
    L3_CACHE,
    eDRAM,
    PCM
};

auto createMemObject(Config &cfg, Memories mem_type)
{
    if (mem_type == Memories::PCM)
    {
        return PCMSim::createPCMSimMemorySystem(cfg);
    }
}

#endif
