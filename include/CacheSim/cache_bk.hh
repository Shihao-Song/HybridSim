
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

#endif
