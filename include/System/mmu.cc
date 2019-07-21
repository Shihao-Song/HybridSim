#include "System/mmu.hh"

namespace System
{
uint64_t MFUPageToNearRows::va2pa(Addr va, int core_id)
{
    Addr pa = mappers[core_id].va2pa(va);
    Addr page_id = pa >> Mapper::va_page_shift;
    if (auto iter = re_alloc_pages.find(page_id);
             iter == re_alloc_pages.end())
    {
        return pa;
    }
    else
    {
        PageEntry &entry = iter->second;
        int new_part_id = entry.new_loc.row_id / num_of_rows_per_partition;
        int new_row_id = entry.new_loc.row_id % num_of_rows_per_partition;
        int new_col_id = entry.new_loc.col_id;
        int new_rank_id = entry.new_loc.dep_id;

        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(pa, mem_addr_decoding_bits, dec_addr);

        dec_addr[int(Config::Decoding::Partition)] = new_part_id;
        dec_addr[int(Config::Decoding::Row)] = new_row_id;
        dec_addr[int(Config::Decoding::Col)] = new_col_id;
        dec_addr[int(Config::Decoding::Rank)] = new_rank_id;

        Addr new_addr = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
        return new_addr;
    }
}

void MFUPageToNearRows::train(std::vector<const char*> &traces)
{
    int core_id = 0;
    for (auto &training_trace : traces)
    {
        TXTTrace trace(training_trace);

        Instruction instr;
        bool more_insts = trace.getInstruction(instr);
        while (more_insts)
        {
            if (instr.opr == Simulator::Instruction::Operation::LOAD ||
                instr.opr == Simulator::Instruction::Operation::STORE)
            {
                Addr addr = mappers[core_id].va2pa(instr.target_addr);
                
                // Get page ID
                Addr page_id = addr >> Mapper::va_page_shift;
                // Is the page has already been touched?
                if (auto iter = pages.find(page_id);
                         iter != pages.end())
                {
                    ++(iter->second).num_refs;
                }
                else
                {
                    // Is this page in the near row region?
                    std::vector<int> dec_addr;
                    dec_addr.resize(mem_addr_decoding_bits.size());

                    Decoder::decode(addr, mem_addr_decoding_bits, dec_addr);
                    int rank_id = dec_addr[int(Config::Decoding::Rank)];
                    int part_id = dec_addr[int(Config::Decoding::Partition)];
                    int row_id = dec_addr[int(Config::Decoding::Row)];
                    int col_id = dec_addr[int(Config::Decoding::Col)];
                    unsigned row_id_plus = part_id * num_of_rows_per_partition + row_id;

                    if (row_id_plus < num_of_near_rows)
                    {
                        pages.insert({page_id, {page_id, true, 1}});
                        touched_near_pages.insert({{row_id_plus,col_id,rank_id}, true});
                    }
		    else
                    {
                        pages.insert({page_id, {page_id, false, 1}});
                    }
                }
            }
            more_insts = trace.getInstruction(instr);
        }

        ++core_id;
    }

    for (auto [key, value] : pages)
    {
        pages_mfu_order.push_back(value);
    }
    std::sort(pages_mfu_order.begin(), pages_mfu_order.end(),
              [](const PageEntry &a, const PageEntry &b)
              {
                  return a.num_refs > b.num_refs;
              });

    // Step Two, re-map the MFU pages.
    uint64_t total_num_pages = pages_mfu_order.size();
    uint64_t pages_to_re_alloc = total_num_pages * perc_re_alloc;
    if (trained_data_output)
    {
        pages_to_re_alloc = total_num_pages;
    }

    for (uint64_t i = 0; i < pages_to_re_alloc; i++)
    {
        PageEntry &page_entry = pages_mfu_order[i];

        bool paired_with_near_page = false;
        while(!paired_with_near_page && !near_region_full)
        {
            if (auto iter = touched_near_pages.find(cur_near_page);
                     iter == touched_near_pages.end())
            {
                paired_with_near_page = true;
                page_entry.new_loc = cur_near_page;
            }
            else
            {
                nextNearPage();
            }
        }
        nextNearPage();
        if (trained_data_output == nullptr)
        {
            re_alloc_pages.insert({page_entry.page_id, page_entry});
	}
        else
        {
            trained_data_out_fd << page_entry.page_id << " "
                                << page_entry.num_refs << " "
                                << page_entry.new_loc.row_id << " "
                                << page_entry.new_loc.col_id << " "
                                << page_entry.new_loc.dep_id << "\n";
        }
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
