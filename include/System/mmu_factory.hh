#ifndef __MMU_FACTORY_HH__
#define __MMU_FACTORY_HH__

#include "System/hybrid.hh"

namespace System
{
class TrainedMMUFactory
{
    typedef Simulator::Config Config;

  private:
    std::unordered_map<std::string,
                       std::function<std::unique_ptr<TrainedMMU>(int,Config&)>> factories;

  public:
    TrainedMMUFactory()
    {
        factories["Hybrid"] = [](int num_of_cores, Config &cfg)
                 {
                     return std::make_unique<Hybrid>(num_of_cores, cfg);
                 };
    }

    auto createMMU(int num_of_cores, Config &cfg)
    {
        std::string mmu_type = cfg.mmu_type;
        if (auto iter = factories.find(mmu_type);
            iter != factories.end())
        {
            return iter->second(num_of_cores, cfg);
        }
        else
        {
            std::cerr << "Unsupported MMU type. \n";
            exit(0);
        }
    }
};
static TrainedMMUFactory TrainedMMUFactories;
static auto createTrainedMMU(int num_of_cores, Simulator::Config &cfg)
{
    return TrainedMMUFactories.createMMU(num_of_cores, cfg);
}
}

#endif
