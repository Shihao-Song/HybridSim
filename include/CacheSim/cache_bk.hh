#ifndef __CACHE_HH__
#define __CACHE_HH__

namespace CacheSimulator
{
template<class B, class T>
class Cache
{
    void tick()
    {
        cur_clk++;

        // Step one: serve pending request
        servePendingHits();

        // Step two: sendDeferredReq 
        sendDeferredReq();

        // Step three: tick the PCM system (off-chip system)
        // TODO, tick next level cache
        // TODO, next-level may need to be a template argument as well
        if (cur_clk % off_chip_tick == 0)
        {
            pcm->tick();
        }
    }

  private:
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
        // std::cout << "Allocating for addr: " << req.addr << "\n";
        if (victim->isValid())
        {
            evictBlock(victim);
        }
        // std::cout << "\n";
        tags->insertBlock(req.addr, victim, cur_clk);
        assert(victim->isValid());
    }
    void evictBlock(B *victim)
    {
        num_evicts++;
        // Send to write-back queue
        // std::cout << "Evicting: " << tags->regenerateAddr(victim) << "\n";
        wb_queue->allocate(tags->regenerateAddr(victim), cur_clk);

        // Invalidate this block
        tags->invalidate(victim);
        assert(!victim->isValid());
    }
};
}
#endif
