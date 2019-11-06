#include "System/mmu.hh"

namespace System
{
bool NearRegionAware::nextReAllocPage(PageLoc &cur_re_alloc_page, int incre_level)
{
    if (incre_level == int(INCREMENT_LEVEL::COL))
    {
        // TODO, hard-coded.
        if (cur_re_alloc_page.col_id += 2;
            cur_re_alloc_page.col_id == num_of_cache_lines_per_row)
        {
            cur_re_alloc_page.col_id = 0;
            return true; // Overflow detected.
        }
        else
        {
            return false; // No overflow.
        }
    }

    if (bool overflow = nextReAllocPage(cur_re_alloc_page, incre_level - 1);
        !overflow)
    {
        return false;
    }
    else
    {
        if (incre_level == int(INCREMENT_LEVEL::ROW))
        {
            if (cur_re_alloc_page.row_id += 1;
                cur_re_alloc_page.row_id == num_of_near_rows)
            {
//                std::cerr << "Abnormal. \n";
//                exit(0);
                cur_re_alloc_page.row_id = 0;
                return true; // Overflow detected.
            }
            else
            {
                return false; // No overflow.
            }
        }

        if (incre_level == int(INCREMENT_LEVEL::TILE))
        {
            if (cur_re_alloc_page.tile_id += 1;
                cur_re_alloc_page.tile_id == num_of_tiles)
            {
                cur_re_alloc_page.tile_id = 0;
                return true; // Overflow detected.
            }
            else
            {
                return false; // No overflow.
            }
        }

        if (incre_level == int(INCREMENT_LEVEL::PARTITION))
        {
            if (cur_re_alloc_page.part_id += 1;
                cur_re_alloc_page.part_id == num_of_partitions)
            {
                cur_re_alloc_page.part_id = 0;
                return true; // Overflow detected.
            }
            else
            {
                return false; // No overflow.
            }
        }

        if (incre_level == int(INCREMENT_LEVEL::RANK))
        {
            if (cur_re_alloc_page.rank_id += 1;
                cur_re_alloc_page.rank_id == num_of_ranks)
            {
                cur_re_alloc_page.rank_id = 0;
                near_region_full = true;
//                std::cerr << "Abnormal. \n";
//                exit(0);
                return true; // Overflow detected.
            }
            else
            {
                return false; // No overflow.
            }
        }
    }
}

void MFUPageToNearRows::va2pa(Request &req)
{
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    req.addr = pa;

//    runtimeProfiling(req);
//    reAllocate(req);
/*
    // Hardware-guided Profiling
    if (profiling_stage)
    {
        profiling_new(req);
    }

    if (inference_stage && !near_region_full)
    {
        inference(req);
    }
*/
}

void MFUPageToNearRows::phaseDone()
{
//    std::cout << first_touch_instructions.size() << "\n";
//    std::cout << fti_candidates.size() << "\n\n";

    // Sort all the FTI candidates
    std::vector<First_Touch_Instr_Info> ordered_by_ref;
    for (auto [key, value] : fti_candidates)
    {
        ordered_by_ref.push_back(value);
    }
    std::sort(ordered_by_ref.begin(), ordered_by_ref.end(),
                  [](const First_Touch_Instr_Info &a, const First_Touch_Instr_Info &b)
                  {
                      return (a.num_of_reads + a.num_of_writes) > 
                             (b.num_of_reads + b.num_of_writes);
                  });

    // Capture the top FTIs
    for (int i = 0; i < num_ftis_per_phase && i < ordered_by_ref.size(); i++)
    {
        first_touch_instructions.insert({ordered_by_ref[i].eip, ordered_by_ref[i]});
    }

    fti_candidates.clear();
}

void MFUPageToNearRows::runtimeProfiling(Request& req)
{
    // PC
    Addr pc = req.eip;
    // Get page ID
    Addr page_id = req.addr >> Mapper::va_page_shift;

    // Step One, check if it is a page fault
    if (auto p_iter = pages.find(page_id);
            p_iter == pages.end())
    {
        // Step two, is the first-touch instruction already cached?
        if (auto f_instr = first_touch_instructions.find(pc);
                f_instr != first_touch_instructions.end())
        {
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
            }
        }
        // Step two (2), cached in fti_candidates?
	else if (auto f_instr = fti_candidates.find(pc);
                f_instr != fti_candidates.end())
        {
            // Yes, cached!
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
            }
        }
	else
        {
            uint64_t num_of_reads = 0;
            uint64_t num_of_writes = 0;

            if (req.req_type == Request::Request_Type::READ)
            {
                num_of_reads = 1;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                num_of_writes = 1;
            }

            fti_candidates.insert({pc,
                                  {pc,
                                   num_of_reads,
                                   num_of_writes
                                   }});
        }

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
        
        if (req.req_type == Request::Request_Type::READ)
        {
            num_of_reads = 1;
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            num_of_writes = 1;
        }
        
        pages.insert({page_id, {page_id,
                                pc,
                                num_of_reads,
                                num_of_writes}});
    }
    else
    {
        // Not a page fault.
        // (1) Record page access information
        if (req.req_type == Request::Request_Type::READ)
        {
            ++p_iter->second.num_of_reads;
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            ++p_iter->second.num_of_writes;
        }

        // (2) Recored FTI information
        if (auto f_instr = first_touch_instructions.find(pc);
                f_instr != first_touch_instructions.end())
        {
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
            }
        }
        // Step two (2), cached in fti_candidates?
	else if (auto f_instr = fti_candidates.find(pc);
                f_instr != fti_candidates.end())
        {
            // Yes, cached!
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
            }
        }
    }
}

void MFUPageToNearRows::reAllocate(Request &req)
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
        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(page_id << Mapper::va_page_shift,
                        mem_addr_decoding_bits,
                        dec_addr);

        dec_addr[int(Config::Decoding::Rank)] = cur_re_alloc_page.rank_id;
        dec_addr[int(Config::Decoding::Partition)] = cur_re_alloc_page.part_id;
        dec_addr[int(Config::Decoding::Tile)] = cur_re_alloc_page.tile_id;
        dec_addr[int(Config::Decoding::Row)] = cur_re_alloc_page.row_id;
        dec_addr[int(Config::Decoding::Col)] = cur_re_alloc_page.col_id;

        nextReAllocPage(cur_re_alloc_page, int(INCREMENT_LEVEL::RANK));

	Addr new_page_id = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits)
                           >> Mapper::va_page_shift;

        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        re_alloc_pages.insert({page_id, new_page_id});

//        std::cout << dec_addr[int(Config::Decoding::Rank)] << " : "
//                  << dec_addr[int(Config::Decoding::Partition)] << " : "
//                  << dec_addr[int(Config::Decoding::Tile)] << " : "
//                  << dec_addr[int(Config::Decoding::Row)] << " : "
//                  << dec_addr[int(Config::Decoding::Col)] << "\n";
    }
}

/*
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

        pages.insert({page_id, {page_id,
                                pc,
                                allocated_in_profiling_stage,
                                reads_in_profiling_stage,
                                writes_in_profiling_stage,
                                reads_in_inference_stage,
                                writes_in_inference_stage}});
    }
    else
    {
        // Not a page fault.
        // Find the first touch instruction that brings in the page.
        auto f_instr = first_touch_instructions.find(p_iter->second.first_touch_instruction);
        assert(f_instr != first_touch_instructions.end());
        if (req.req_type == Request::Request_Type::READ)
        {
            if (profiling_stage)
            {
                ++f_instr->second.reads_profiling_stage;
                ++p_iter->second.reads_in_profiling_stage;
            }
            else if(inference_stage)
            {
                ++f_instr->second.reads_inference_stage;
                ++p_iter->second.reads_in_inference_stage;
            }
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            if (profiling_stage)
            {
                ++f_instr->second.writes_profiling_stage;
                ++p_iter->second.writes_in_profiling_stage;
            }
            else if(inference_stage)
            {
                ++f_instr->second.writes_inference_stage;
                ++p_iter->second.writes_in_inference_stage;
            }
        }
    }
}
*/
}
