#ifndef __DEFERRED_QUEUE_HH__
#define __DEFERRED_QUEUE_HH__

#include <assert.h>
#include <set>
#include <unordered_map>

namespace eDRAMSimulator
{

template<class T>
class Deferred_Queue
{
  public:
    Deferred_Queue(int _max) : max(_max) {}
    virtual ~Deferred_Queue() {}

    bool isFull() { return all_entries.size() == max; }
    int numEntries() { return all_entries.size(); }
    
    bool getEntry(Addr &entry)
    {
        // This entry should not be on flight
        for (auto ite = all_entries.begin(); ite != all_entries.end(); ite++)
        {
            if (entries_on_flight.find(*ite) == entries_on_flight.end())
            {
                // Good, we haven't sent out this entry
                entry = *ite;
                return true;
            }
        }
        
        return false;
    }

    virtual void entryOnBoard(Addr addr) {}
    virtual void allocate(Addr addr, Tick when)
    {
        when_ready.insert({addr, when});
    }
    virtual void deAllocate(Addr addr)
    {
        assert(when_ready.erase(addr));
    }
    virtual bool isReady(Addr addr, Tick cur_clk)
    {
        auto iter = when_ready.find(addr);
        assert(iter != when_ready.end());
        return (iter->second <= cur_clk);
    }

    typedef std::unordered_map<Addr, Tick> TickHash;
    TickHash when_ready;

  protected:
    int max;
    T all_entries;
    T entries_on_flight;
};

class Deferred_Set : public Deferred_Queue<std::set<Addr>>
{
public:
    Deferred_Set(int max)
        : Deferred_Queue<std::set<Addr>>(max) {}

    void entryOnBoard(Addr addr) override
    {
        auto ret = entries_on_flight.insert(addr);
        assert(ret.first != entries_on_flight.end());
    }

    void allocate(Addr addr, Tick when) override
    {
        auto ret = all_entries.insert(addr);
        assert(ret.first != all_entries.end());
	
        if (ret.second == true)
        {
            Deferred_Queue::allocate(addr, when);
        }
        // Make sure the entry is there
        assert(when_ready.find(addr) != when_ready.end());
    }

    void deAllocate(Addr addr) override
    {
        assert(all_entries.erase(addr));
        assert(entries_on_flight.erase(addr));

        Deferred_Queue::deAllocate(addr);
        assert(when_ready.find(addr) == when_ready.end());
    }
};
}

#endif
