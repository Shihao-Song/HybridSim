#include "System/mmu.hh"

namespace System
{
void MFUPageToNearRows::va2pa(Request &req)
{
    Addr pc = req.eip;
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    req.addr = pa;

    // Hardware-guided Profiling
    if (profiling_stage)
    {
        profiling(req);
    }

    if (inference_stage && !near_region_full)
    {
        inference(req);
    }
}

void MFUPageToNearRows::profiling(Request& req)
{
    // PC
    Addr pc = req.eip;
    // Get page ID
    Addr page_id = req.addr >> Mapper::va_page_shift;
    // Is the page has already been touched?
    if (auto iter = pages.find(page_id);
             iter != pages.end())
    {
        if (auto iter = first_touch_instructions.find(pc);
                 iter != first_touch_instructions.end())
        {
            // Call-back on this eip
            req.setMMUCommuFunct(profilingCallBack());
        }
    }
    else
    {
        // Record the first touch instruction.
        // Records and callback on this eip
        first_touch_instructions.insert({pc, {pc,0,0}});
        pages.insert({page_id, true});
        req.setMMUCommuFunct(profilingCallBack());
    }
}

void MFUPageToNearRows::inference(Request &req)
{
    Addr pc = req.eip;
    Addr pa = req.addr; // Already translated.
    Addr page_id = pa >> Mapper::va_page_shift;

    // Is the page one of the MFU pages?
    if (auto iter = re_alloc_pages.find(page_id);
             iter != re_alloc_pages.end())
    {
        Addr new_page_id = iter->second;
        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        return;
    }

    // Not found, should we allocate to near rows?
    if (auto iter = first_touch_instructions.find(pc);
             iter != first_touch_instructions.end())
    {
        // Allocate at near row, naive implementations;
        int new_part_id = cur_near_page.row_id / num_of_rows_per_partition;
        int new_row_id = cur_near_page.row_id % num_of_rows_per_partition;
        int new_col_id = cur_near_page.col_id;
        int new_rank_id = cur_near_page.dep_id;

        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(pa, mem_addr_decoding_bits, dec_addr);

        dec_addr[int(Config::Decoding::Partition)] = new_part_id;
        dec_addr[int(Config::Decoding::Row)] = new_row_id;
        dec_addr[int(Config::Decoding::Col)] = new_col_id;
        dec_addr[int(Config::Decoding::Rank)] = new_rank_id;

        Addr new_pa = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);

        req.addr = new_pa; // Replace with the new PA

        Addr new_page_id = new_pa >> Mapper::va_page_shift;
        re_alloc_pages.insert({page_id, new_page_id});

        nextNearPage();
    }
}

void MFUPageToNearRows::nextNearPage()
{
    if (cur_near_page.col_id + 2 >= max_near_page_col_id)
    {
        cur_near_page.col_id = 0;
        if (cur_near_page.row_id + 1 >= max_near_page_row_id)
        {
            cur_near_page.row_id = 0;
            if (cur_near_page.dep_id + 1 >= max_near_page_dep_id)
            {
                near_region_full = true;
            }
            else
            {
                cur_near_page.dep_id = cur_near_page.dep_id + 1;
            }
        }
        else
        {
            cur_near_page.row_id = cur_near_page.row_id + 1;
        }
    }
    else
    {
        cur_near_page.col_id = cur_near_page.col_id + 2;
    }
}
}
