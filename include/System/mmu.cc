#include "System/mmu.hh"

namespace System
{
void MFUPageToNearRows::va2pa(Request &req)
{
    Addr pc = req.eip;
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    req.addr = pa;

    // Hardware-guided Profiling
    // TODO, should have a flag to indicate.
    if (true)
    {
        profiling(req);
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
        // Records and callback on this eip
        first_touch_instructions.insert({pc, 0});
        pages.insert({page_id, true});
        req.setMMUCommuFunct(profilingCallBack());
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

/*
void MFUPageToNearRows::train(std::vector<const char*> &traces)
{
    int core_id = 0;
    std::set<Addr> fs_instrs;
    std::unordered_map<Addr, uint64_t> fs_instr_freq;
    
    std::vector<TXTTrace> trace_gens;
    trace_gens.emplace_back(traces[0]);
    trace_gens.emplace_back(traces[1]);

        int arbitrator = 0;
        Instruction instr;
        bool more_insts = trace_gens[0].getInstruction(instr);

        uint64_t counter = 0;
        while (counter <= 2000000)
        {
            if (arbitrator == 0)
	    {
	        arbitrator = 1;
	    }
	    else if (arbitrator == 1)
	    {
	        arbitrator = 0;
	    }

            if (instr.opr == Simulator::Instruction::Operation::LOAD ||
                instr.opr == Simulator::Instruction::Operation::STORE)
            {
                Addr pc = instr.eip;
                Addr addr = mappers[core_id].va2pa(instr.target_addr);

                bool first_touch = false;
                // Get page ID
                Addr page_id = addr >> Mapper::va_page_shift;
                // Is the page has already been touched?
                if (auto iter = pages.find(page_id);
                         iter != pages.end())
                {
                    ++(iter->second).num_refs;
                    if (auto iter = fs_instrs.find(pc);
                        iter != fs_instrs.end())
                    {
                        ++(fs_instr_freq.find(pc)->second);
                    }
                }
                else
                {
                    first_touch = true;
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

                if (first_touch)
                {
                if (auto [iter, success] = fs_instrs.insert(pc);
                    !success)
                {
                    ++(fs_instr_freq.find(pc)->second);
                }
                else
                {
                    fs_instr_freq.insert({pc,1});
                }
                }
            }
            ++counter;
            more_insts = trace_gens[arbitrator].getInstruction(instr);
        }

    std::vector<uint64_t> test;
    for (auto [key, value] : fs_instr_freq)
    {
        test.push_back(value);
    }
    std::sort(test.begin(), test.end(),
              [](const uint64_t &a, const uint64_t &b)
              {
                  return a > b;
              });
    for (int i = 0; i < test.size(); i++)
    {
        std::cout << test[i] << "\n";
    }
}
*/
/*
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
        trained_data_out_fd << total_num_pages << "\n";
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
*/

/*
void MFUPageToNearRows::inference(Addr &pa)
{
    Addr page_id = pa >> Mapper::va_page_shift;

    if (auto iter = re_alloc_pages.find(page_id);
             iter == re_alloc_pages.end())
    {
        return;
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
        pa = new_addr;
    }
}

void MFUPageToNearRows::preLoadTrainedData(const char* trained_data, double perc_to_re_allo)
{
    std::ifstream page_re_alloc_info(trained_data);

    std::string line;
    getline(page_re_alloc_info, line);
    uint64_t total_num_pages = std::stoull(line);
    uint64_t num_pages_to_re_alloc = total_num_pages * perc_to_re_allo;

    uint64_t count = 0;
    while(!page_re_alloc_info.eof())
    {
        if (count == num_pages_to_re_alloc)
        {
            break;
        }
        getline(page_re_alloc_info, line);

        std::stringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string intermidiate;
        while (getline(line_stream, intermidiate, ' '))
        {
            tokens.push_back(intermidiate);
        }
        assert(tokens.size());

        Addr page_id = std::stoull(tokens[0]);
        uint64_t num_refs = std::stoull(tokens[1]);
        
        unsigned row_id = std::stoull(tokens[2]);
        unsigned col_id = std::stoull(tokens[3]);
        unsigned dep_id = std::stoull(tokens[4]);

        PageEntry entry;
        entry.page_id = page_id;
        entry.num_refs = num_refs;

        entry.new_loc.row_id = row_id;
        entry.new_loc.col_id = col_id;
        entry.new_loc.dep_id = dep_id;

        re_alloc_pages.insert({page_id, entry});
        ++count;
    }
    
    page_re_alloc_info.close();
}
*/
}