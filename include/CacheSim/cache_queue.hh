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

    bool isFull() { return all_entries.size() == max; }
    int numEntries() { return all_entries.size(); }

    auto getEntry(Tick cur_clk)
    {
        for (auto ite = all_entries.begin(); ite != all_entries.end(); ite++)
        {
            // Have we sent out this address already?
            if (entries_on_flight.find(*ite) == entries_on_flight.end())
            {
                // Good, we haven't sent out this entry
                if (isReady(*ite, cur_clk))
                {
                    // Even better, we're ready to send out.
                    return std::make_pair(true, *ite);
                }
            }
        }
        
        return std::make_pair(false, MAX_ADDR);
    }

    void entryOnBoard(Addr addr)
    {
        entries_on_flight.insert(addr);
    }

    void allocate(Addr addr, Tick when)
    {
        if (auto [iter, success] = all_entries.insert(addr);
            success)
        {
            when_ready.insert({addr, when});
        }
    }

    void deAllocate(Addr addr, bool on_board = true)
    {
	assert(all_entries.erase(addr));
        if (on_board)
        {
            assert(entries_on_flight.erase(addr));
        }
        assert(when_ready.erase(addr));

        assert(when_ready.find(addr) == when_ready.end());
    }

    bool isReady(Addr addr, Tick cur_clk)
    {
        auto iter = when_ready.find(addr);
        assert(iter != when_ready.end());
        return (iter->second <= cur_clk);
    }

    // This functions is used to detect write-back queue hit
    bool isInQueueNotOnBoard(Addr addr)
    {
        // This address must be in queue
        bool in_all = (all_entries.find(addr) != all_entries.end());
        // We haven't sent out this address
        bool not_on_board = (entries_on_flight.find(addr) ==
                             entries_on_flight.end());

        return (in_all && not_on_board);
    }

    // This function is used to detect mshr queue hit
    bool isInQueue(Addr addr)
    {
        bool in_all = (all_entries.find(addr) != all_entries.end());

        return in_all;
    }

    // TODO, this hash will be more complicated in the future.
    typedef std::unordered_map<Addr, Tick> TickHash;
    TickHash when_ready;

  protected:
    int max;
    std::set<Addr> all_entries;
    std::set<Addr> entries_on_flight;
};
}

#endif
