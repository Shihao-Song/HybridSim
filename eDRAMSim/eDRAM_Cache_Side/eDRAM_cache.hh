#ifndef __eDRAMCACHE_HH__
#define __eDRAMCACHE_HH__

#include "../../PCMSim/Memory_System/pcm_sim_memory_system.hh"
#include "tags/eDRAM_cache_fa_blk.hh"
#include "tags/eDRAM_cache_tags.hh"
#include "util/deferred_queue.hh"

#include <deque>

namespace PCMSim
{
    class Request;
    class PCMSimMemorySystem;
}

namespace eDRAMSimulator
{
typedef PCMSim::Request Request;
typedef PCMSim::PCMSimMemorySystem PCMSimMemorySystem;

class eDRAMCacheTagsWithFABlk;

class eDRAMCache
{
  public:    
    eDRAMCache(PCMSimMemorySystem *_pcm);

    ~eDRAMCache()
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
                wb_queue->numEntries());
    }

  private:
    Tick cur_clk;

  private:
    const bool write_only; // Does this cache only cache writes?

  private:
    std::deque<Request> pending_queue_for_hits; // for read/write hits
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
    void evictBlock(eDRAMCacheFABlk *victim);

  private:
    eDRAMCacheTagsWithFABlk *tags;
    Deferred_Set *mshrs;
    Deferred_Set *wb_queue;

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
