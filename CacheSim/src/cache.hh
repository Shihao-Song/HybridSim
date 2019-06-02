#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../../Configs/config.hh"
#include "../../PCMSim/Memory_System/pcm_sim_memory_system.hh"
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
template<class B, class T>
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
    Cache(Config::Cache_Level _level, Config &cfg)
        : level(_level),
          cur_clk(0),
          mshr_cb_func(std::bind(&Cache::MSHRComplete, this,
                                 std::placeholders::_1)),
          wb_cb_func(std::bind(&Cache::WBComplete, this,
                               std::placeholders::_1)),
          num_read_allos(0),
          num_write_allos(0),
          num_evicts(0),
          num_read_hits(0),
          num_write_hits(0)
    {
        // Only cache write requests?
        write_only = cfg.caches[int(level)].write_only;
        // When should we tick off-chip component?
        off_chip_tick = cfg.on_chip_frequency / cfg.off_chip_frequency;
        // Do we care about cache latency?
        if (cfg.cache_detailed)
        {
            tag_lookup_latency = cfg.caches[int(level)].tag_lookup_latency;
        }
        else
        {
            tag_lookup_latency = 0;
        }

        // MSHRs and WB buffer    
        mshrs = new Deferred_Set(cfg.caches[int(level)].num_mshrs);
        wb_queue = new Deferred_Set(cfg.caches[int(level)].num_wb_entries);

        std::cout << getLevel()
                  << ", Write-only mode: " << write_only << "\n";
    }

    ~Cache()
    {
        delete mshrs;
        delete wb_queue;
    }

    bool blocked() { return (mshrs->isFull() || wb_queue->isFull()); }

    bool access(Request &req)
    {
        B *blk = tags->accessBlock(req.addr);

        if (blk && blk->isValid())
        {
            // insert to pending queue
            req.begin_exe = cur_clk;
            req.end_exe = cur_clk + tag_lookup_latency;
            pending_queue_for_hits.push_back(req);

            if (req.req_type == Request::Request_Type::WRITE)
            {
                num_write_hits++;
            }
            else
            {
                num_read_hits++;
            }

            return true;
        }

        // Hit on MSHR queue, return true.
        // (1) A read followed by a write, can get data directly;
        // (2) A write followed by a write, update the data (in buffer) directly;
        if (mshrs->isInQueue(req.addr))
        {
            return true;
        }

        // Hit on wb queue, bring back the entry.
        if (wb_queue->isInQueueNotOnBoard(req.addr))
        {
            wb_queue->deAllocate(req.addr, false);
            allocateBlock(req);
        }

        if (write_only &&
            req.req_type == Request::Request_Type::WRITE && !blocked() ||
            !write_only && !blocked())
        {
            assert(!mshrs->isFull());
            assert(!wb_queue->isFull());

            if (write_only)
            {
                assert(req.req_type == Request::Request_Type::WRITE);
            }

            Addr target = tags->extractTag(req.addr);
            mshrs->allocate(target, cur_clk + tag_lookup_latency);
            if (req.req_type == Request::Request_Type::READ)
            {
                num_read_allos++;
            }
            else
            {
                num_write_allos++;
            }

            return true;
        }

        if (!write_only)
        {
            return false;
        }

        assert(write_only);
        if (req.req_type == Request::Request_Type::READ)
        {
            return pcm->send(req);
        }

        // At this point, we know the request must be a write request.
        // Let's also make sure that the eDRAM must be in BLOCKED state.
        assert(blocked());

        return false;
    }

    void tick()
    {
        cur_clk++;

        // Step one: serve pending request
        servePendingHits();

        // Step two: sendDeferredReq 
        sendDeferredReq();

        // Step three: tick the PCM system (off-chip system)
        // TODO, tick next level cache
        if (cur_clk % off_chip_tick == 0)
        {
            pcm->tick();
        }
    }

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
    void sendDeferredReq()
    {
        Addr mshr_entry = MaxAddr;
        bool mshr_entry_valid = mshrs->getEntry(mshr_entry, cur_clk);

        Addr wb_entry = MaxAddr;
        bool wb_entry_valid = wb_queue->getEntry(wb_entry, cur_clk);

        if (wb_entry_valid && (wb_queue->isFull() || !mshr_entry_valid))
        {
            // Make sure write-back address is correct
            assert(wb_entry != MaxAddr);
            // Make sure the tag lookup latency has been resolved
            assert(wb_queue->isReady(wb_entry, cur_clk));
            sendWBReq(wb_entry);
        }
        else if (mshr_entry_valid)
        {
            // Make sure mshr address is correct
            assert(mshr_entry != MaxAddr);
            // Make sure tag lookup latency has been resolved
            assert(mshrs->isReady(mshr_entry, cur_clk));
            sendMSHRReq(mshr_entry);
        }
    }

  // This section deals with MSHR handling and WB handling
  private:
    std::function<void(Request&)> mshr_cb_func;
    std::function<void(Request&)> wb_cb_func;

    void sendMSHRReq(Addr addr)
    {
        Request req(addr, Request::Request_Type::READ, mshr_cb_func);

        if (pcm->send(req))
        {
            mshrs->entryOnBoard(addr);
        }
    }
    void MSHRComplete(Request& req)
    {
        // Step one: Allocate block
        allocateBlock(req);

        // Step two: De-allocate MSHR entry
        mshrs->deAllocate(req.addr);
    }

    void sendWBReq(Addr addr)
    {
        Request req(addr, Request::Request_Type::WRITE, wb_cb_func);

        if (pcm->send(req))
        {
            wb_queue->entryOnBoard(addr);
        }
    }
    void WBComplete(Request& req)
    {
        // Step one: De-allocate wb entry
        wb_queue->deAllocate(req.addr);
    }

    void allocateBlock(Request& req)
    {
        B *victim = tags->findVictim(req.addr);
        assert(victim != nullptr);

        if (victim->isValid())
        {
            evictBlock(victim);
        }

        tags->insertBlock(req.addr, victim);
        assert(victim->isValid());
    }
    void evictBlock(B *victim)
    {
        num_evicts++;
        // Send to write-back queue
        wb_queue->allocate(victim->tag, cur_clk);

        // Invalidate this block
        tags->invalidate(victim);
        assert(!victim->isValid());
    }

  private:
    T *tags;
    Deferred_Set *mshrs;
    Deferred_Set *wb_queue;

  public:
    void setNextLevel(Cache *_next_level);
    void setNextLevel(PCMSimMemorySystem *_pcm)
    {
        pcm = _pcm;
    }
    void setTags(T *_tags){ tags = _tags; }

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
