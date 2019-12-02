#ifndef __MMU_FACTORY_HH__
#define __MMU_FACTORY_HH__

#include "System/hybrid.hh"
#include "System/single_node.hh"

namespace System
{
class MMUFactory
{
    typedef Simulator::Config Config;

  public:
    MMUFactory()
    {
    }
    auto createMMU(int num_of_cores, Config &dram_cfg, Config &pcm_cfg)
    {
        return std::make_unique<Hybrid>(num_of_cores, dram_cfg, pcm_cfg);
    }
    auto createMMU(int num_of_cores, Config &pcm_cfg)
    {
        return std::make_unique<SingleNode>(num_of_cores, pcm_cfg);
    }
};
static MMUFactory MMUFactories;
static auto createMMU(int num_of_cores, Simulator::Config &dram_cfg,
                                               Simulator::Config &pcm_cfg)
{
    return MMUFactories.createMMU(num_of_cores, dram_cfg, pcm_cfg);
}

static auto createMMU(int num_of_cores, Simulator::Config &pcm_cfg)
{
    return MMUFactories.createMMU(num_of_cores, pcm_cfg);
}

}

#endif
