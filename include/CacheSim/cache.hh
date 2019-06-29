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
    Tick clk;

    std::unique_ptr<Tag> tags;

    std::unique_ptr<CacheQueue> mshr;
    std::unique_ptr<CacheQueue> wb_queue;

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
        : Simulator::MemObject()
    {
    
    }

    void setNextLevel(Simulator::MemObject *_next_level)
    {
        next_level = _next_level;
    }
};
}
#endif
