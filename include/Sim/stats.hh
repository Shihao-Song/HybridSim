#ifndef __SIM_STATS_HH__
#define __SIM_STATS_HH__

#include "Sim/config.hh"

namespace Simulator
{
// This class is used to collect information from different objects.
// This class is used for workloads' evaluations and only core 0 is considered 
// at this point.
class Stats
{
  private:
    uint64_t num_loads_by_core;
    uint64_t num_stores_by_core;

    struct CachePerformance
    {
        uint64_t num_hits;
        uint64_t num_misses;
        double hit_ratio;

        uint64_t num_loads;
        uint64_t num_evictions;
    };
    std::array<CachePerformance, int(Config::Cache_Level::MAX)> cache_perf;

  public:
    Stats() {} 

    void setNumLoadsByCore(uint64_t _num_loads)
    {
        num_loads_by_core = _num_loads;
    }

    void setNumStoresByCore(uint64_t _num_stores)
    {
        num_stores_by_core = _num_stores;
    }

    void setNumCacheHits(Config::Cache_Level level, uint64_t _num_hits)
    {
        cache_perf[int(level)].num_hits = _num_hits;
    }

    void setNumCacheMisses(Config::Cache_Level level, uint64_t _num_misses)
    {
        cache_perf[int(level)].num_misses = _num_misses;
    }

    void setCacheHitRatio(Config::Cache_Level level, double _hit_ratio)
    {
        cache_perf[int(level)].hit_ratio = _hit_ratio;
    }

    void setNumLoadsByCache(Config::Cache_Level level, uint64_t _num_loads)
    {
        cache_perf[int(level)].num_loads = _num_loads;
    }

    void setNumEvictsByCache(Config::Cache_Level level, uint64_t _num_evicts)
    {
        cache_perf[int(level)].num_evictions = _num_evicts;
    }

    void outputCache(const char* fn, const char* eval_idx, Config::Cache_Level level)
    {
        if (!std::ifstream(fn) || strcmp(eval_idx, "-1") == 0)
        {
            std::ofstream fd(fn);
            fd << "Eval,# LLC Loads,# LLC Evicts\n";
            fd << eval_idx << ","
               << cache_perf[int(level)].num_loads << ","
               << cache_perf[int(level)].num_evictions << "\n";
            fd.close();
        }
        else
        {
            std::ofstream fd(fn, std::ios_base::app);
            fd << eval_idx << ","
               << cache_perf[int(level)].num_loads << ","
               << cache_perf[int(level)].num_evictions << "\n";
            fd.close();
        }
    }
};
}

#endif
