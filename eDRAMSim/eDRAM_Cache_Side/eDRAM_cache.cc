#include "eDRAM_cache.hh"
#include "tags/eDRAM_cache_fa_tags.hh"

namespace eDRAMSimulator
{
eDRAMCache::eDRAMCache(PCMSimMemorySystem *_pcm)
    : cur_clk(0),
      write_only(Constants::WRITE_ONLY),
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
{}

void eDRAMCache::tick()
{
    cur_clk++; 
    // Step one: serve pending request
    servePendingHits();

    // Step two: sendDeferredReq 
    sendDeferredReq();

    // Step three: tick the PCM system
    // Let's assume the CPU side including eDRAM ticks every ns (1 GHz)
    // so the PCM should be ticked every 1/pcm_ticks_per_ns
    float pcm_ticks_per_ns = pcm->nclks_per_ns;
    unsigned pcm_tick_period = 1 / pcm_ticks_per_ns;
    if (cur_clk % pcm_tick_period == 0)
    {
        pcm->tick();
    }
}

void eDRAMCache::sendDeferredReq()
{
    Addr mshr_entry = MaxAddr;
    bool mshr_entry_valid = mshrs->getEntry(mshr_entry);

    Addr wb_entry = MaxAddr;
    bool wb_entry_valid = wb_queue->getEntry(wb_entry);

    if (wb_entry_valid && (wb_queue->isFull() || !mshr_entry_valid))
    {
        assert(wb_entry != MaxAddr);
        if (wb_queue->isReady(wb_entry, cur_clk))
        {
            sendWBReq(wb_entry);
        }
    }
    else if (mshr_entry_valid)
    {
        assert(mshr_entry != MaxAddr);
        if (mshrs->isReady(mshr_entry, cur_clk))
        {
            sendMSHRReq(mshr_entry);
        }
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
        //std::cout << cur_clk << ": Read miss detected and forwarding to PCM...\n";
        return pcm->send(req);
    }
   
    //std::cout << cur_clk << ": A Write miss detected but eDRAM is busy now... \n"; 
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
