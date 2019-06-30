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
    bool blocked() {return (mshr_queue->isFull() || wb_queue->isFull());}

  protected:
    auto nclksToTickNextLevel(auto &cfg)
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
    std::deque<Request> pending_queue_for_hit_reqs;
    std::deque<Request> pending_queue_for_non_hit_reqs;

  protected:
    Simulator::MemObject *next_level;

  protected:
    void mshrComplete(Addr addr)
    {
        std::cout << "A MSHR request for addr " << addr << " is finished.\n";
    }
    auto mshrCallback(auto &mem_obj)
    {
        return [&](Addr addr)
        {
           mem_obj.mshrComplete(addr); 
        };
    }

    void wbComplete(Addr addr)
    {
        std::cout << "A write-back request for addr " << addr << " is finished.\n";
    }
    auto wbCallback(auto &mem_obj)
    {
        return [&](Addr addr)
        {
            mem_obj.wbComplete(addr);
        };
    }

  protected:
    uint64_t num_hits;

  public:
    Cache(Config::Cache_Level _level, Config &cfg)
        : Simulator::MemObject(),
          level(_level),
          clk(0),
          tags(new Tag(int(_level), cfg)),
          mshr_queue(new CacheQueue(cfg.caches[int(_level)].num_mshrs)),
          wb_queue(new CacheQueue(cfg.caches[int(_level)].num_wb_entries)),
          tag_lookup_latency(cfg.caches[int(_level)].tag_lookup_latency),
          nclks_to_tick_next_level(nclksToTickNextLevel(cfg)),
          num_hits(0)
    {}

    int pendingRequests() override
    {
        return pending_queue_for_hit_reqs.size() +
               pending_queue_for_non_hit_reqs.size();
    }

    bool send(Request &req) override
    {
        // Step one, check whether it is a hit or not
        if (auto [hit, aligned_addr] = tags.accessBlock(req.addr, clk);
            hit)
        {
            req.begin_exe = clk;
            req.end_exe = clk + tag_lookup_latency;
            pending_queue_for_hit_reqs.push_back(req);

            ++num_hits;
        }
        else
        {
            if constexpr(std::is_same<NormalMode, Mode>::value)
            {
                if (!blocked())
                {
                    assert(!mshr_queue->isFull());
                    assert(!wb_queue->isFull());

                    mshr_queue->allocate(aligned_addr, clk + tag_lookup_latency);
                    pending_queue_for_non_hit_reqs.push_back(req);
                    return true;
                }
                return false;
            }
            else if constexpr(std::is_same<WriteOnly, Mode>::value)
            {
                if (!blocked())
                {
                    assert(!mshr_queue->isFull());
                    assert(!wb_queue->isFull());

                    if (req.req_type == Request::Request_Type::WRITE)
                    {
                        mshr_queue->allocate(aligned_addr, clk + tag_lookup_latency);
                        pending_queue_for_non_hit_reqs.push_back(req);
                        return true;
                    }
                    else
                    {
                        assert(req.req_type == Request::Request_Type::READ);
                        // Forward to next level directly.
                        return next_level->send(req);
                    }
                }
                return false;
            }
        }
    }

    void tick() override
    {
        clk++;

//        servePendings();

        if (clk % nclks_to_tick_next_level == 0)
        {
            next_level->tick();
        }
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
