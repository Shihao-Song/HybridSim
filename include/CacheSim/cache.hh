#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "Sim/config.hh"
#include "Sim/mem_object.hh"
#include "CacheSim/cache_queue.hh"
#include "CacheSim/tags/fa_tags.hh"
#include "CacheSim/tags/set_assoc_tags.hh"

#include <deque>
#include <functional>
#include <string>

namespace CacheSimulator
{
struct OnChipToOffChip{}; // Next level is off-chip
struct OnChipToOnChip{}; // Next level is still on-chip

struct NormalMode{};
struct ReadOnly{}; // The cache is read only
struct WriteOnly{}; // The cache is write only

template<class Block, class Tag, class Mode, class Position>
class Cache : public Simulator::MemObject
{
  public:
    typedef Simulator::Config Config;
    typedef Simulator::Request Request;

    typedef uint64_t Addr;
    typedef uint64_t Tick;

  protected:
    Config::Cache_Level level;

    Tick clk;

    std::unique_ptr<Tag> tags;

    std::unique_ptr<CacheQueue> mshr_queue;
    std::unique_ptr<CacheQueue> wb_queue;

  protected:
    auto nclksToTick(auto &cfg)
    {
        return [&]()
        {
            if constexpr (std::is_same<OnChipToOffChip, Position>::value) 
            {
                return cfg.on_chip_frequency / cfg.off_chip_frequency;
            }
            else
            {
                return cfg.on_chip_frequency / cfg.on_chip_frequency;
            }
        };
    }

    const unsigned tag_lookup_latency;
    const unsigned nclks_to_tick_next_level;

  protected:
    std::deque<Request> pending_queue;

  protected:
    Simulator::MemObject *next_level;

  protected:
    std::function<void(Request&)> mshrCallback;
    std::function<void(Request&)> wbCallback;

  protected:
    uint64_t num_hits;
    uint64_t num_mshr_hits;
    uint64_t num_wb_queue_hits;

  public:
    Cache(Config::Cache_Level _level, Config &cfg)
        : Simulator::MemObject(),
          level(_level),
          clk(0),
          tags(new Tag(int(_level), cfg)),
          mshr_queue(new CacheQueue(cfg.caches[int(_level)].num_mshrs)),
          wb_queue(new CacheQueue(cfg.caches[int(_level)].num_wb_entries)),
          tag_lookup_latency(cfg.caches[int(_level)].tag_lookup_latency),
          nclks_to_tick_next_level(nclksToTick(cfg)),
          num_hits(0),
          num_mshr_hits(0),
          num_wb_queue_hits(0)
    {}

    int pendingRequests() override
    {
        return pending_queue.size(); 
    }

    void setNextLevel(Simulator::MemObject *_next_level)
    {
        next_level = _next_level;
    }

    std::string getLevel()
    {
        if (level == Config::Cache_Level::L1I)
        {
            return "L1I";
        }
        else if (level == Config::Cache_Level::L1D)
        {
            return "L1D";
        }
        else if (level == Config::Cache_Level::L2)
        {
            return "L2";
        }
        else if (level == Config::Cache_Level::L3)
        {
            return "L3";
        }
        else if (level == Config::Cache_Level::eDRAM)
        {
            return "eDRAM";
        }
    }
};
}
#endif
