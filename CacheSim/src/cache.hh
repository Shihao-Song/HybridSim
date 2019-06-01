#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../../Configs/config.hh"
#include "../../PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "tags/cache_blk.hh"
#include "tags/cache_tags.hh"
#include "util/deferred_queue.hh"

#include <deque>
#include <string>

namespace PCMSim
{
    class Request;
    class PCMSimMemorySystem;
}

namespace CacheSimulator
{
class Cache
{
    typedef Configuration::Config Config;
    typedef PCMSim::Request Request;
    typedef PCMSim::PCMSimMemorySystem PCMSimMemorySystem;

  private:
    Config::Cache_Level level;

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

    bool write_only;
    unsigned off_chip_tick;
    unsigned tag_lookup_latency;

  public:
    Cache(Config::Cache_Level _level, Config &cfg);

    ~Cache()
    {
        delete mshrs;
        delete wb_queue;
    }

    bool blocked() { return (mshrs->isFull() || wb_queue->isFull()); }

    bool access(Request &req);
    void tick();

    unsigned numOutstanding()
    {
        return (pcm->pendingRequests() + mshrs->numEntries() +
                wb_queue->numEntries() + pending_queue_for_hits.size());
    }

  private:
    Tick cur_clk;

  private:
    std::deque<Request> pending_queue_for_hits; // for read/write hits

    // TODO, should also invoke call-back function as well.
    void servePendingHits()
    {
        if (!pending_queue_for_hits.size())
        {
            return;
        }
        Request &req = pending_queue_for_hits[0];
        if (req.end_exe <= cur_clk)
        {
            pending_queue_for_hits.pop_front();
        }
    }

  private:
    // To send either an MSHR or WB request
    void sendDeferredReq();

  // This section deals with MSHR handling and WB handling
  private:
    std::function<void(Request&)> mshr_cb_func;
    std::function<void(Request&)> wb_cb_func;

    void sendMSHRReq(Addr addr);
    void MSHRComplete(Request& req);

    void sendWBReq(Addr addr);
    void WBComplete(Request& req);

    void allocateBlock(Request& req);
    void evictBlock(FABlk *victim);

  private:
    void *tags;
    Deferred_Set *mshrs;
    Deferred_Set *wb_queue;

  public:
    void setNextLevel(Cache *_next_level);
    void setNextLevel(PCMSimMemorySystem *_pcm)
    {
        pcm = _pcm;
    }

  private:
    PCMSimMemorySystem *pcm;

  private:
    uint64_t num_read_allos;
    uint64_t num_write_allos;
    uint64_t num_evicts;
    uint64_t num_read_hits;
    uint64_t num_write_hits;
  public:
    void printStats()
    {
        std::cout << "Num of read allocations: " << num_read_allos << "\n";
        std::cout << "Num of write allocations: " << num_write_allos << "\n";
	std::cout << "Num of evictions: " << num_evicts << "\n";	
	std::cout << "Num of read hits: " << num_read_hits << "\n";
        std::cout << "Num of write hits: " << num_write_hits << "\n";
    }
};
}
#endif
