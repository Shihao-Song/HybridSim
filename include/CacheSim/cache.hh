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
    std::string level_name;
    std::string toString()
    {
        if (level == Config::Cache_Level::L1D)
        {
            return std::string("L1-D");
        }
        else if (level == Config::Cache_Level::L2)
        {
            return std::string("L2");
        }
        else if (level == Config::Cache_Level::L3)
        {
            return std::string("L3");
        }
        else if (level == Config::Cache_Level::eDRAM)
        {
            return std::string("eDRAM");
        }
    }

    Tick clk;

    std::unique_ptr<Tag> tags;

    std::unique_ptr<CacheQueue> mshr_queue;
    auto sendMSHRReq(Addr addr)
    {
    //    std::cout << clk << ": " << level_name << " is sending an MSHR request for "
    //              << "addr " << addr << "\n";

        Request req(addr, Request::Request_Type::READ,
                    [this](Addr _addr){ return this->mshrComplete(_addr); });

        req.core_id = id;
        auto [eip, mmu_commu] = mshr_queue->retriMMUCommu(addr);
        req.eip = eip;
        req.setMMUCommuFunct(mmu_commu);

	if (next_level->send(req))
        {
            ++num_loads;
            mshr_queue->entryOnBoard(addr);
        }
    }

    bool mshrComplete(Addr addr)
    {
    //    std::cout << clk << ": " << level_name << " is receiving an MSHR answer for "
    //              << "addr " << addr << "\n";

        // To insert a new block may cause a eviction, need to make sure the write-back
        // is not full.
        if (wb_queue->isFull()) { return false; }

        bool is_entry_modified = mshr_queue->isEntryModified(addr);
        auto [eip, mmu_commu] = mshr_queue->retriMMUCommu(addr);
        // if (is_entry_modified)
        // { 
        //     std::cout << level_name << " " << addr << " is dirty. \n";
        // }
        // else
        // {
        //     std::cout << level_name << " " << addr << " is clean. \n";
        // }
        mshr_queue->deAllocate(addr, true);
        if (auto [wb_required, wb_addr] = tags->insertBlock(addr, is_entry_modified, clk);
            wb_required)
        {
            wb_queue->allocate(wb_addr, clk);

            auto [wb_eip, wb_mmu_commu] = tags->retriMMUCommu(addr);
            // Record to wb_queue.
            wb_queue->recordMMUCommu(wb_addr, wb_eip, wb_mmu_commu);
        }
        // Need to record new MMU call-back information.
        tags->recordMMUCommu(addr,
                             eip,
                             mmu_commu);

        auto iter = pending_queue_for_non_hit_reqs.begin();
        while (iter != pending_queue_for_non_hit_reqs.end())
        {
            if (iter->addr == addr)
            {
                iter->end_exe = clk;
                pending_commits.push_back(*iter);
                iter = pending_queue_for_non_hit_reqs.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
        return true;
    }
    
    std::unique_ptr<CacheQueue> wb_queue;
    auto sendWBReq(Addr addr)
    {
        if constexpr(std::is_same<OnChipToOffChip, Position>::value)
        {
            Request req(addr, Request::Request_Type::WRITE);

	    req.core_id = id;
            auto [eip, mmu_commu] = wb_queue->retriMMUCommu(addr);
            req.eip = eip;
            req.setMMUCommuFunct(mmu_commu);

	    if (next_level->send(req))
            {
                ++num_evicts;
                wb_queue->deAllocate(addr);
            }
        }
        else
	{    
            Request req(addr, Request::Request_Type::WRITE_BACK);

	    req.core_id = id;
            auto [eip, mmu_commu] = wb_queue->retriMMUCommu(addr);
            req.eip = eip;
            req.setMMUCommuFunct(mmu_commu);

	    if (next_level->send(req))
            {
                ++num_evicts;
                wb_queue->deAllocate(addr);
            }
        }
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
    std::deque<Request> pending_commits;
    std::list<Request> pending_queue_for_non_hit_reqs;

    auto servePendings()
    {
        if (pending_commits.size())
        {
            Request &req = pending_commits[0];
            if (req.end_exe <= clk)
            {
                if (req.callback)
                {
                    if (req.callback(req.addr))
                    {
                        pending_commits.pop_front();
                    }
                }
                else
                {
                    pending_commits.pop_front();
                }
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
    uint64_t num_hits;
    uint64_t num_misses;
    uint64_t num_loads;
    uint64_t num_evicts;

  public:
    Cache(Config::Cache_Level _level, Config &cfg)
        : Simulator::MemObject(),
          level(_level),
          level_name(toString()),
          clk(0),
          tags(new Tag(int(_level), cfg)),
          mshr_queue(new CacheQueue(cfg.caches[int(_level)].num_mshrs)),
          wb_queue(new CacheQueue(cfg.caches[int(_level)].num_wb_entries)),
          tag_lookup_latency(cfg.caches[int(_level)].tag_lookup_latency),
          nclks_to_tick_next_level(nclksToTickNextLevel(cfg)),
          num_hits(0),
          num_misses(0),
          num_loads(0),
          num_evicts(0)
    {
    }

    int pendingRequests() override
    {
        int pendings = 0;
        pendings = pending_commits.size() +
                   pending_queue_for_non_hit_reqs.size() +
                   mshr_queue->numEntries() +
                   wb_queue->numEntries();

        if (!boundary)
        {
            pendings += next_level->pendingRequests();
        }

        return pendings;
    }

    bool send(Request &req) override
    {
        if (arbitrator)
        {
            assert(req.core_id != -1);
            if (req.core_id != selected_client)
	    {
                return false;
            }
        }

        // Step one, check whether it is a hit or not
        if (auto [hit, aligned_addr] = tags->accessBlock(req.addr,
                                       req.req_type != Request::Request_Type::READ ?
                                       true : false,
                                       clk);
            hit)
        {
            req.begin_exe = clk;
            req.end_exe = clk + tag_lookup_latency;
            pending_commits.push_back(req);

            ++num_hits;
            // Notify MMU that there is a hit
            req.commuToMMU();
            return true;
        }
        else
        {
            // Step two, to ensure data consistency. Check if the current request can be
            // served by a write-back queue.
            // For example, if the request is a LOAD, it can get data directly;
            // if the request is a STORE, it can simply re-write the entry.
            if (wb_queue->isInQueue(aligned_addr))
            {
                ++num_hits;
                // Notify MMU that there is a hit
                req.commuToMMU();

                req.begin_exe = clk;
                req.end_exe = clk + tag_lookup_latency;
                pending_commits.push_back(req);

                return true;
            }
            
            // Step three, if there is a write-back (eviction). We should allocate the space
            // directly.
            if (req.req_type == Request::Request_Type::WRITE_BACK)
            {
                // We need to make sure that the write-back queue is not full.
                if (!wb_queue->isFull())
                {
                    // A write-back from higher level must be a dirty block.
                    if (auto [wb_required, wb_addr] = tags->insertBlock(aligned_addr,
                                                                        true,
                                                                        clk);
                        wb_required)
                    {
                        wb_queue->allocate(wb_addr, clk);
                        // Retrive MMU call-back information of evicted.
                        auto [eip, mmu_commu] = tags->retriMMUCommu(aligned_addr);
                        // Record to wb_queue.
                        wb_queue->recordMMUCommu(wb_addr, eip, mmu_commu);
                    }
                    // Need to record new MMU call-back information.
                    tags->recordMMUCommu(aligned_addr,
                                         req.eip,
                                         req.getMMUCommuFunct());

                    return true;
                }
                return false;
            }

            // Step four, accept normal READ or WRITES.
            if constexpr(std::is_same<NormalMode, Mode>::value)
            {
                if (!blocked())
                {
                    assert(!mshr_queue->isFull());
                    assert(!wb_queue->isFull());

                    if (auto hit_in_mshr_queue = mshr_queue->allocate(aligned_addr,
                                                             clk + tag_lookup_latency);
                        hit_in_mshr_queue)
                    {
                    //     std::cout << "Address " << aligned_addr 
                    //               << " is hit in MSRH. \n";
                        ++num_hits;
                        // Notify MMU that there is a hit
                        req.commuToMMU();
                    }
                    else
                    {
                        // Not hit in cache
                        // Not hit in wb
                        // Not in mshr 
                        ++num_misses;
                        mshr_queue->recordMMUCommu(aligned_addr,
                                                   req.eip,
                                                   req.getMMUCommuFunct());
                    }

                    if (req.req_type == Request::Request_Type::WRITE)
                    {
                    //     std::cout << "Address " << aligned_addr
                    //               << " write arequest is detected.\n";
                       
                        mshr_queue->setEntryModified(aligned_addr);
                    }

                    req.begin_exe = clk;
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
                        if (auto hit_in_mshr_queue = mshr_queue->allocate(aligned_addr,
                                                                 clk + tag_lookup_latency);
                            hit_in_mshr_queue)
                        {
                            ++num_hits;
                            // Notify MMU that there is a hit
                            req.commuToMMU();
                        }
                        else
                        {
                            // Not hit in cache
                            // Not hit in wb
                            // Not hit in mshr 
                            ++num_misses;
                            mshr_queue->recordMMUCommu(aligned_addr,
                                                       req.eip,
                                                       req.getMMUCommuFunct());
                        }
                        if (req.req_type == Request::Request_Type::WRITE)
                        {
                            mshr_queue->setEntryModified(aligned_addr);
                        }
                        req.begin_exe = clk;
                        pending_queue_for_non_hit_reqs.push_back(req);

                        return true;
                    }
                    else
                    {
                        assert(req.req_type == Request::Request_Type::READ);
                        ++num_misses;
                        // Forward to next level directly.
                        bool ret = next_level->send(req);
                        if (ret) {num_loads++;}
                        return ret;
                    }
                }
                return false;
            }
        }
    }

    void tick() override
    {
	servePendings();

        if (boundary)
        {
            // TODO, delete assert in the future.
            assert(level == Config::Cache_Level::L1D);
            clk++;
            return;
        }
        // TODO, delete assert in the future.
        assert(int(level) > int(Config::Cache_Level::L1D));
        if (arbitrator)
        {
            // TODO, delete assert in the future.
            assert(level == Config::Cache_Level::L2);
            selected_client = (selected_client + 1) % num_clients;
        }

        if (clk % nclks_to_tick_next_level == 0)
        {
            next_level->tick();
        }
        
	clk++;
    }

    void reInitialize() override
    {
        clk = 0;
        tags->reInitialize();

        num_hits = 0;
        num_misses = 0;
        num_loads = 0;
        num_evicts = 0;

        selected_client = 0;

        if (!boundary)
        {
            next_level->reInitialize();
        }
    }

    void registerStats(Simulator::Stats &stats) override
    {
        std::string registeree_name = level_name;
        if (id != -1)
        {
            registeree_name = "Core-" + std::to_string(id) + "-" + 
                              registeree_name;
        }

        stats.registerStats(registeree_name +
                            ": Number of hits = " + std::to_string(num_hits));
        stats.registerStats(registeree_name +
                            ": Number of misses = " + std::to_string(num_misses));

        double hit_ratio = double(num_hits) / (double(num_hits) + double(num_misses));
        stats.registerStats(registeree_name + 
                            ": Hit ratio = " + std::to_string(hit_ratio));

        stats.registerStats(registeree_name +
                            ": Number of Loads = " + std::to_string(num_loads));        
        stats.registerStats(registeree_name +
                            ": Number of Evictions = " + std::to_string(num_evicts));
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

    // TODO, this function needs to be more flexible in the future
    // We have limit the use of Set-Assoc-LRU here.
    auto createCache(Config::Cache_Level level, Config &cfg, bool LLC = false)
    {
        if (LLC)
        {
            return factories["SET_WAY_LRU_LLC"](level, cfg);
        }
        else
        {
            return factories["SET_WAY_LRU_NON_LLC"](level, cfg);
        }
    }
};

static CacheFactory CacheFactories;
auto createCache(Simulator::Config::Cache_Level level,
                 Simulator::Config &cfg,
                 bool LLC = false)
{
    return CacheFactories.createCache(level, cfg, LLC);
}
}
#endif
