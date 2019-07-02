#ifndef __CACHE_QUEUE_HH__
#define __CACHE_QUEUE_HH__

#include <assert.h>
#include <set>
#include <unordered_map>

namespace CacheSimulator
{
class CacheQueue
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    const Addr MAX_ADDR = (Addr)-1;

  public:
    CacheQueue(int _max) : max(_max) {}

    bool isFull() { return all_entries.size() >= max; }
    int numEntries() { return all_entries.size(); }

    auto getEntry(Tick cur_clk)
    {
        for (auto ite = all_entries.begin(); ite != all_entries.end(); ite++)
        {
            if (isReady(*ite, cur_clk))
            {
                return std::make_pair(true, *ite);
            }
        }
        
        return std::make_pair(false, MAX_ADDR);
    }
    
    auto allocate(Addr addr, Tick when)
    {
        if (auto [iter, success] = all_entries.insert(addr);
            success)
        {
            when_ready.insert({addr, when});
            return false; // Not hit in queue
        }
        return true; // Hit in queue.
    }

    void deAllocate(Addr addr)
    {
	assert(all_entries.erase(addr));
        assert(when_ready.erase(addr));
    }

    bool isReady(Addr addr, Tick cur_clk)
    {
        auto iter = when_ready.find(addr);
        assert(iter != when_ready.end());
        return (iter->second <= cur_clk);
    }
    
    bool isInQueue(Addr addr)
    {
        bool in_all = (all_entries.find(addr) != all_entries.end());

        return in_all;
    }

    typedef std::unordered_map<Addr, Tick> TickHash;
    TickHash when_ready;

  protected:
    int max;
    std::set<Addr> all_entries;
};
}

#endif
