#include "eDRAM_cache.hh"
#include "tags/eDRAM_cache_fa_tags.hh"

namespace eDRAMSimulator
{
eDRAMCache::eDRAMCache(PCMSimMemorySystem *_pcm)
    : cur_clk(0),
      write_only(Constants::WRITE_ONLY),
      pcm_tick_period(Constants::TICKS_PER_NS / pcm->nclks_per_ns),
      mshr_cb_func(std::bind(&eDRAMCache::MSHRComplete, this,
                             std::placeholders::_1)),
      wb_cb_func(std::bind(&eDRAMCache::WBComplete, this,
                           std::placeholders::_1)),
      tags(new eDRAMCacheFATags()),
      mshrs(new Deferred_Set(Constants::NUM_MSHRS)),
      wb_queue(new Deferred_Set(Constants::NUM_WB_ENTRIES)),
      pcm(_pcm),
      num_read_allos(0),
      num_write_allos(0),
      num_evicts(0),
      num_read_hits(0),
      num_write_hits(0)
{
    std::cout << "eDRAM System (" << Constants::SIZE / 1024 / 1024
              << " MB): \n";
    std::cout << "Write-only mode: " << Constants::WRITE_ONLY << "\n\n";
}

void eDRAMCache::tick()
{
    cur_clk++; 

    // Step one: serve pending request
    servePendingHits();

    // Step two: sendDeferredReq 
    sendDeferredReq();

    // Step three: tick the PCM system
    if (cur_clk % pcm_tick_period == 0)
    {
        pcm->tick();
    }
}

void eDRAMCache::sendDeferredReq()
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

bool eDRAMCache::access(Request &req)
{
    eDRAMCacheFABlk *
    blk = tags->accessBlock(req.addr);

    if (blk && blk->isValid())
    {
        // insert to pending queue
        req.begin_exe = cur_clk;
        req.end_exe = cur_clk + Constants::TAG_LOOKUP_LATENCY;
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

    if (write_only && req.req_type == Request::Request_Type::WRITE && !blocked() ||
        !write_only && !blocked())
    {
        assert(!mshrs->isFull());
        assert(!wb_queue->isFull());

        if (write_only)
        {
            assert(req.req_type == Request::Request_Type::WRITE);
        }

        Addr target = tags->extractTag(req.addr);
        mshrs->allocate(target, cur_clk + Constants::TAG_LOOKUP_LATENCY);
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

void eDRAMCache::sendMSHRReq(Addr addr)
{
    Request req(addr, Request::Request_Type::READ, mshr_cb_func);
   
    if (pcm->send(req))
    {
        mshrs->entryOnBoard(addr);
    }
}

void eDRAMCache::sendWBReq(Addr addr)
{
    Request req(addr, Request::Request_Type::WRITE, wb_cb_func);
    
    if (pcm->send(req))
    {
        wb_queue->entryOnBoard(addr);
    }
}

void eDRAMCache::MSHRComplete(Request& req)
{
    // Step one: Allocate eDRAM block
    allocateBlock(req);

    // Step two: De-allocate MSHR entry
    mshrs->deAllocate(req.addr);
}

void eDRAMCache::WBComplete(Request& req)
{
    // Step one: De-allocate wb entry
    wb_queue->deAllocate(req.addr);
}

void eDRAMCache::allocateBlock(Request &req)
{
    eDRAMCacheFABlk *victim = tags->findVictim(req.addr);
    assert(victim != nullptr);

    if (victim->isValid())
    {
        evictBlock(victim);
    }

    tags->insertBlock(req.addr, victim);
    assert(victim->isValid()); 
}

void eDRAMCache::evictBlock(eDRAMCacheFABlk *victim)
{
    num_evicts++;
    // Send to write-back queue
    wb_queue->allocate(victim->tag, cur_clk);
    
    // Invalidate this block
    tags->invalidate(victim);
    assert(!victim->isValid());
}
}
