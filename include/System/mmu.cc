#include "System/mmu.hh"

namespace System
{
void NearRegionAware::nextReAllocPage()
{
    /*
    if (cur_re_alloc_page.group_id + 1 > max_near_page_group_id)
    {
        cur_re_alloc_page.group_id = 0;
        if (cur_re_alloc_page.col_id + 2 > max_near_page_col_id)
        {
            cur_re_alloc_page.col_id = 0;
            if (cur_re_alloc_page.row_id + 1 > max_near_page_row_id)
            {
                cur_re_alloc_page.row_id = 0;
                if (cur_re_alloc_page.dep_id + 1 > max_near_page_dep_id)
                {
                    near_region_full = true;
                }
                else
                {
                    cur_re_alloc_page.dep_id = cur_re_alloc_page.dep_id + 1;
                }
            }
            else
            {
                cur_re_alloc_page.row_id = cur_re_alloc_page.row_id + 1;
            }
        }
        else
        {
            cur_re_alloc_page.col_id = cur_re_alloc_page.col_id + 2;
        }
    }
    else
    {
        cur_re_alloc_page.group_id = cur_re_alloc_page.group_id + 1;
    }
    */
}

void MFUPageToNearRows::va2pa(Request &req)
{
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    req.addr = pa;

    // Hardware-guided Profiling
    // TODO, tmp modification
//    if (profiling_stage)
//    {
        profiling_new(req);
//    }

//    if (inference_stage && !near_region_full)
//    {
//        inference(req);
//    }
}

void MFUPageToNearRows::profiling_new(Request& req)
{
    // PC
    Addr pc = req.eip;
    // Get page ID
    Addr page_id = req.addr >> Mapper::va_page_shift;

    // Step One, check if it is a page fault
    if (auto p_iter = pages.find(page_id);
            p_iter == pages.end())
    {
        // Yes, it is a page fault.
        // Step two, is the first-touch instruction already cache?
        if (auto f_instr = first_touch_instructions.find(pc);
                f_instr != first_touch_instructions.end())
        {
            // Yes, cached!

            // Are we in profiling stage?
            if (profiling_stage)
            {
                // Yes, we are in profiling stage
                if (req.req_type == Request::Request_Type::READ)
                {
                    ++f_instr->second.reads_profiling_stage;
                }
                else if (req.req_type == Request::Request_Type::WRITE)
		{
                    ++f_instr->second.writes_profiling_stage;
                }

                ++f_instr->second.touched_pages_profiling_stage; // Touched a new page
            }
            else
            {
                // Yes, we are in inference stage
                if (req.req_type == Request::Request_Type::READ)
                {
                    ++f_instr->second.reads_inference_stage;
                }
                else if (req.req_type == Request::Request_Type::WRITE)
		{
                    ++f_instr->second.writes_inference_stage;
                }

                ++f_instr->second.touched_pages_inference_stage; // Touched a new page
            }
        }
	else
        {
            // No, not cached.
            // Organize first-touch instruction information
            bool captured_in_profiling_stage = false;

            uint64_t reads_profiling_stage = 0;
	    uint64_t writes_profiling_stage = 0;

            uint64_t reads_inference_stage = 0;
            uint64_t writes_inference_stage = 0;

            uint64_t touched_pages_profiling_stage = 0;
            uint64_t touched_pages_inference_stage = 0;

            if (profiling_stage)
            {
                captured_in_profiling_stage = true;

                touched_pages_profiling_stage = 1;
                
                if (req.req_type == Request::Request_Type::READ)
                {
                    reads_profiling_stage = 1;
                }
                else if (req.req_type == Request::Request_Type::WRITE)
                {
                    writes_profiling_stage = 1;
                }
            }
            else if (inference_stage)
            {
                captured_in_profiling_stage = false;

                touched_pages_inference_stage = 1;
                
                if (req.req_type == Request::Request_Type::READ)
                {
                    reads_inference_stage = 1;
                }
                else if (req.req_type == Request::Request_Type::WRITE)
                {
                    writes_inference_stage = 1;
                }
            }
            first_touch_instructions.insert({pc,
                                            {pc,
                                             captured_in_profiling_stage,
                                             reads_profiling_stage,
                                             writes_profiling_stage,
                                             reads_inference_stage,
                                             writes_inference_stage,
                                             touched_pages_profiling_stage,
                                             touched_pages_inference_stage
					     }});
        }
        // Insert new page
        // Organize page information
        bool allocated_in_profiling_stage = false;

        uint64_t reads_by_profiled_instructions = 0;
        uint64_t writes_by_profiled_instructions = 0;

        uint64_t reads_by_non_profiled_instructions = 0;
        uint64_t writes_by_non_profiled_instructions = 0;

        uint64_t reads_in_profiling_stage = 0;
        uint64_t writes_in_profiling_stage = 0;

        uint64_t reads_in_inference_stage = 0;
        uint64_t writes_in_inference_stage = 0;
        
        if (profiling_stage)
        {
            allocated_in_profiling_stage = true;

            if (req.req_type == Request::Request_Type::READ)
            {
                reads_in_profiling_stage = 1;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                writes_in_profiling_stage = 1;
            }
        }
        else if (inference_stage)
        {
            allocated_in_profiling_stage = false;
                
            if (req.req_type == Request::Request_Type::READ)
            {
                reads_in_inference_stage = 1;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                writes_in_inference_stage = 1;
            }
        }
    }
    else
    {
        // Not a page fault.
        // Find the first touch instruction that brings in the page.
        auto f_instr = first_touch_instructions.find(p_iter->second);
        assert(f_instr != first_touch_instructions.end());
        if (req.req_type == Request::Request_Type::READ)
        {
            if (profiling_stage)
            {
                ++f_instr->second.reads_profiling_stage;
            }
            else if(inference_stage)
            {
                ++f_instr->second.reads_inference_stage;
            }
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            if (profiling_stage)
            {
                ++f_instr->second.writes_profiling_stage;
            }
            else if(inference_stage)
            {
                ++f_instr->second.writes_inference_stage;
            }
        }
    }
/*
    // Step one, check if the PC in iTab
    if (auto f_instr = first_touch_instructions.find(pc);
            f_instr != first_touch_instructions.end())
    {
        // The PC has already been cached in iTab.
        // Step two, increment counters
        if (req.req_type == Request::Request_Type::READ)
        {
            ++f_instr->second.reads;
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            ++f_instr->second.writes;
        }

        // Step three, page fault?
        if (auto p_iter = pages.find(page_id);
                p_iter == pages.end())
        {
            // Yes, insert the page ID.
            pages.insert({page_id, true});
            // This instruction touched a new page.
            ++f_instr->second.touched_pages;
        }
    }
    else
    {
        // The PC is not cached in iTab.
        // Step one, determine if it is a first-touch instruction
        if (auto p_iter = pages.find(page_id);
                p_iter == pages.end())
        {
            // Page fault! Yes, it is a first-touch instruction. Need to record.
            // Step two, erase the LFU entry if iTab is full.
            if (num_profiling_entries != -1 && 
                first_touch_instructions.size() == num_profiling_entries)
            {
                std::vector<RWCount> ordered_by_ref;
                for (auto [key, value] : first_touch_instructions)
                {
                    ordered_by_ref.push_back(value);
                }
                std::sort(ordered_by_ref.begin(), ordered_by_ref.end(),
                          [](const RWCount &a, const RWCount &b)
                          {
                              return (a.reads + a.writes) > (b.reads + b.writes);
                          });

                assert(ordered_by_ref.size() == num_profiling_entries);
                // Erase the least referenced instruction
                first_touch_instructions.erase(ordered_by_ref[num_profiling_entries - 1].eip);
            }

            // Step three, insert into the iTab. 
            if (req.req_type == Request::Request_Type::READ)
            {
                first_touch_instructions.insert({pc, {pc,1,0,1}});
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                first_touch_instructions.insert({pc, {pc,0,1,1}});
            }
            pages.insert({page_id, true});
        }
    }
*/
}

// This is the old profiling technique which requires all levels of memory to communicate with
// MMU, very expensive.
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
/*
    // Not found, should we allocate to near rows?
    if (auto iter = first_touch_instructions.find(pc);
             iter != first_touch_instructions.end())
    {          	
        // Allocate at near row, naive implementations;
        int new_part_id = cur_re_alloc_page.group_id;
        int new_row_id = cur_re_alloc_page.row_id;
        int new_col_id = cur_re_alloc_page.col_id;
        int new_rank_id = cur_re_alloc_page.dep_id;

        // std::cout << new_part_id << " "
        //           << new_row_id << " "
        //           << new_col_id << " "
        //           << new_rank_id << "\n";

        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(pa, mem_addr_decoding_bits, dec_addr);
        
	dec_addr[int(Config::Decoding::Partition)] = new_part_id;
        dec_addr[int(Config::Decoding::Row)] = new_row_id;
        dec_addr[int(Config::Decoding::Col)] = new_col_id;
        dec_addr[int(Config::Decoding::Rank)] = new_rank_id;

        Addr new_page_id = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits) 
                           >> Mapper::va_page_shift;

        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        re_alloc_pages.insert({page_id, new_page_id});

        nextReAllocPage();
    }
*/
}
/*
// Strategy 2, give the control of near pages to memory controller. Only pages outside
// near region are accessible.
void HiddenNearRows::nextReAllocPage()
{
    if (cur_re_alloc_page.group_id + 1 > cur_max_group_id)
    {
        cur_re_alloc_page.group_id = cur_min_group_id;
        if (cur_re_alloc_page.col_id + 2 > max_col_id)
        {
            cur_re_alloc_page.col_id = 0;
            if (cur_re_alloc_page.row_id + 1 > max_row_id)
            {
                cur_re_alloc_page.row_id = 0;
                if (cur_re_alloc_page.dep_id + 1 > max_dep_id)
                {
                    // TODO, implement this in the future; if all the pages are occupied.
                    // if (cur_max_group_id == max_group_id)
                    // {
                    // }
                    cur_re_alloc_page.dep_id = 0;

                    cur_min_group_id = cur_max_group_id + 1;
                    cur_max_group_id = cur_min_group_id + num_groups_per_stage - 1;

                    cur_re_alloc_page.group_id = cur_min_group_id;		    
                }
                else
                {
                    cur_re_alloc_page.dep_id = cur_re_alloc_page.dep_id + 1;
                }
            }
            else
            {
                cur_re_alloc_page.row_id = cur_re_alloc_page.row_id + 1;
            }
        }
        else
        {
            cur_re_alloc_page.col_id = cur_re_alloc_page.col_id + 2;
        }
    }
    else
    {
        cur_re_alloc_page.group_id = cur_re_alloc_page.group_id + 1;
    }
}

void HiddenNearRows::va2pa(Request &req)
{
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    Addr page_id = pa >> Mapper::va_page_shift;
    req.addr = pa;

    // Has this page already been re-allocated?
    if (auto iter = re_alloc_pages.find(page_id);
             iter != re_alloc_pages.end())
    {
        Addr new_page_id = iter->second;
        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        return;
    }

    // Is the mapped page in the near region?
    std::vector<int> dec_addr;
    dec_addr.resize(mem_addr_decoding_bits.size());
    Decoder::decode(pa, mem_addr_decoding_bits, dec_addr);

    unsigned row_id = dec_addr[int(Config::Decoding::Partition)] * 
                      num_of_rows_per_partition + 
                      dec_addr[int(Config::Decoding::Row)];
    
    if (row_id < num_of_near_rows)
    {
        // This is a near page, re-allocate it.
        int new_part_id = cur_re_alloc_page.group_id;
        int new_row_id = cur_re_alloc_page.row_id;
        int new_col_id = cur_re_alloc_page.col_id;
        int new_rank_id = cur_re_alloc_page.dep_id;

        dec_addr[int(Config::Decoding::Partition)] = new_part_id;
        dec_addr[int(Config::Decoding::Row)] = new_row_id;
        dec_addr[int(Config::Decoding::Col)] = new_col_id;
        dec_addr[int(Config::Decoding::Rank)] = new_rank_id;

        Addr new_page_id = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits)
                           >> Mapper::va_page_shift;

        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        re_alloc_pages.insert({page_id, new_page_id});

        nextReAllocPage();
    }
}
*/
}
