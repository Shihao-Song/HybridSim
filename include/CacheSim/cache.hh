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

template<class Tag, class Mode, class Position>
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
    auto mshrCallback(auto *mem_obj)
    {
        return [&](Addr addr)
        {
            mem_obj->mshrComplete(addr);
        };
    }
    auto sendMSHRReq(Addr addr)
    {
        std::cout << clk << ": A MSHR request for addr " << addr << " is sent.\n";
        Request req(addr, Request::Request_Type::READ, mshrCallback(this));

        if (next_level->send(req))
        {

            mshr_queue->entryOnBoard(addr);
        }
    }
    auto mshrComplete(Addr addr)
    {
        std::cout << clk << ": A MSHR request for addr " << addr << " is finished.\n";
        if (auto [wb_required, wb_addr] = tags->insertBlock(addr, clk);
            wb_required)
        {
            wb_queue->allocate(wb_addr, clk);
        }
        mshr_queue->deAllocate(addr);

        auto iter = pending_queue_for_non_hit_reqs.begin();
        while (iter != pending_queue_for_non_hit_reqs.end())
        {
            if (iter->addr == addr)
            {
                iter = pending_queue_for_non_hit_reqs.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }
    
    std::unique_ptr<CacheQueue> wb_queue;
    auto wbCallback(auto *mem_obj)
    {
        return [&](Addr addr)
        {
            mem_obj->wbComplete(addr);
        };
    }
    auto sendWBReq(Addr addr)
    {
        Request req(addr, Request::Request_Type::WRITE, wbCallback(this));

        if (next_level->send(req))
        {
            wb_queue->entryOnBoard(addr);
        }
    }
    void wbComplete(Addr addr)
    {
        std::cout << "A write-back request for addr " << addr << " is finished.\n";
        wb_queue->deAllocate(addr);
    }
    
    bool blocked() {return (mshr_queue->isFull() || wb_queue->isFull());}

  protected:
    auto nclksToTickNextLevel(auto &cfg)
    {
        if constexpr (std::is_same<OnChipToOffChip, Position>::value) 
        {
            return cfg.on_chip_frequency / cfg.off_chip_frequency;
        }
        else
        {
            return cfg.on_chip_frequency / cfg.on_chip_frequency;
        }
    }

    const unsigned tag_lookup_latency;
    const unsigned nclks_to_tick_next_level;

  protected:
    std::deque<Request> pending_queue_for_hit_reqs;
    std::list<Request> pending_queue_for_non_hit_reqs;
    auto servePendings()
    {
        // See if any of the hit requests has resolved its lookup latency.
        if (pending_queue_for_hit_reqs.size())
        {
            Request &req = pending_queue_for_hit_reqs[0];
            if (req.end_exe <= clk)
            {
                // TODO, callback
                pending_queue_for_hit_reqs.pop_front();
            }
        }

        // Note, we are not serving non-hit reqs directly but sending out corres. MSHR request
        // to next level.
        auto [write_back_entry_ready, write_back_addr] = wb_queue->getEntry(clk);
        auto [mshr_entry_ready, mshr_req_addr] = mshr_queue->getEntry(clk);

	if (write_back_entry_ready && wb_queue->isFull() ||
            !mshr_entry_ready && wb_queue->numEntries())
        {
            assert(wb_queue->numEntries());
            sendWBReq(write_back_addr);
        }
        else if (mshr_entry_ready)
        {
            assert(mshr_queue->numEntries());
            sendMSHRReq(mshr_req_addr);
        }
    }

  protected:
    Simulator::MemObject *next_level;

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
    {
    }

    int pendingRequests() override
    {
        return pending_queue_for_hit_reqs.size() +
               pending_queue_for_non_hit_reqs.size();
    }

    bool send(Request &req) override
    {
        // Step one, check whether it is a hit or not
        if (auto [hit, aligned_addr] = tags->accessBlock(req.addr, clk);
            hit)
        {
            req.begin_exe = clk;
            req.end_exe = clk + tag_lookup_latency;
            pending_queue_for_hit_reqs.push_back(req);

            ++num_hits;
        }
        else
        {
            // Step two, to ensure data consistency. Check if the current request can be
            // served by a write-back queue.
            // For example, if the request is a LOAD, it can get data directly;
            // if the request is a STORE, it can simply re-write the entry.
            if (wb_queue->isInQueue(aligned_addr))
            {
                req.begin_exe = clk;
                req.end_exe = clk + tag_lookup_latency;
                pending_queue_for_hit_reqs.push_back(req);

                // Erase this entry and bring back to cache (may cause eviction)
                wb_queue->deAllocate(aligned_addr, false);
                if (auto [wb_required, wb_addr] = tags->insertBlock(aligned_addr, clk);
                    wb_required)
                {
                    wb_queue->allocate(wb_addr, clk);
                }
            }

            if constexpr(std::is_same<NormalMode, Mode>::value)
            {
                if (!blocked())
                {
                    assert(!mshr_queue->isFull());
                    assert(!wb_queue->isFull());

                    if (auto hit_in_mshr_queue = mshr_queue->allocate(aligned_addr,
                                                             clk + tag_lookup_latency);
                        !hit_in_mshr_queue)
                    {
                        pending_queue_for_non_hit_reqs.push_back(req);
                    }
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
                        if (auto hit_in_mshr_queue = mshr_queue->allocate(aligned_addr,
                                                                 clk + tag_lookup_latency);
                            !hit_in_mshr_queue)
                        {
                            pending_queue_for_non_hit_reqs.push_back(req);
                        }
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

        servePendings();

        if (clk % nclks_to_tick_next_level == 0)
        {
            std::cout << clk << ": Tick next level.\n";
            next_level->tick();
        }
    }

    void setNextLevel(Simulator::MemObject *_next_level) override
    {
        next_level = _next_level;
    }
};

typedef Cache<LRUFATags,NormalMode,OnChipToOffChip> FA_LRU_LLC;
typedef Cache<LRUFATags,WriteOnly,OnChipToOffChip> FA_LRU_LLC_WRITE_ONLY;
typedef Cache<LRUSetWayAssocTags,NormalMode,OnChipToOffChip> SET_WAY_LRU_LLC;
typedef Cache<LRUSetWayAssocTags,NormalMode,OnChipToOnChip> SET_WAY_LRU_NON_LLC;

class CacheFactory
{
    typedef Simulator::Config Config;
    typedef Simulator::MemObject MemObject;

  private:
    std::unordered_map<std::string,
         std::function<std::unique_ptr<MemObject>(Config::Cache_Level,Config&)>> factories;

  public:
    CacheFactory()
    {
        factories["FA_LRU_LLC"] = [](Config::Cache_Level level, Config &cfg)
                                  {
                                      return std::make_unique<FA_LRU_LLC>(level, cfg);
                                  };

        factories["FA_LRU_LLC_WRITE_ONLY"] = [](Config::Cache_Level level, Config &cfg)
                                  {
                                      return std::make_unique<FA_LRU_LLC_WRITE_ONLY>(level,
                                                                                     cfg);
                                  };

        factories["SET_WAY_LRU_LLC"] = [](Config::Cache_Level level, Config &cfg)
                                  {
                                      return std::make_unique<SET_WAY_LRU_LLC>(level, cfg);
                                  };

        factories["SET_WAY_LRU_NON_LLC"] = [](Config::Cache_Level level, Config &cfg)
                                  {
                                      return std::make_unique<SET_WAY_LRU_NON_LLC>(level, cfg);
                                  };
    }

    auto createCache(Config::Cache_Level level, Config &cfg)
    {
        // TODO, need to make it more flexible in the future.
        if (int(level) < int(Config::Cache_Level::eDRAM))
        {
            return factories["SET_WAY_LRU_NON_LLC"](level, cfg);
        }
        else if (int(level) == int(Config::Cache_Level::eDRAM))
        {
            return factories["FA_LRU_LLC_WRITE_ONLY"](level, cfg);
        }
    }
};

static CacheFactory CacheFactories;
auto createCache(Simulator::Config::Cache_Level level,
                 Simulator::Config &cfg)
{
    return CacheFactories.createCache(level, cfg);
}
}
#endif
