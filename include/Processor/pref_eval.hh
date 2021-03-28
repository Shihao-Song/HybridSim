#ifndef __PREFETCHER_EVAL_HH__
#define __PREFETCHER_EVAL_HH__

#include "Sim/mem_object.hh"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Simulator
{
// Evaluate spatial patterns among pages
// Format:
// PC (brings in the page) | pageID | touched block (offsets)
class PrefEval : public MemObject
{
  protected:
    typedef uint64_t Addr;
    typedef uint64_t Count;

    static const unsigned block_size = 64;
    static const unsigned block_shift = 6;

    static const unsigned page_size = 4096;
    static const uint64_t va_page_shift = 12;
    static const uint64_t va_page_mask = (uint64_t(1) << va_page_shift) - 1;

  protected:
    struct Page_Info
    {
        Page_Info()
        {
            for (auto &o : offsets) { o = false; }
        }

        Addr page_id;

        // The first-touch instrucion that brings in the page
        Addr first_touch_instruction;

        Count num_of_reads = 0;
        Count num_of_writes = 0;

        // Block offsets
        bool offsets[page_size / block_size];

        bool operator<(const Page_Info& a) 
        {
            return (a.num_of_reads + a.num_of_writes) < 
                   (num_of_reads + num_of_writes);
        }
    };
    std::unordered_map<Addr, Page_Info> page_id_to_page_info;

    struct First_Touch_Instr_Info
    {
        Addr eip;

        Count num_of_reads = 0;
        Count num_of_writes = 0;

        bool operator<(const First_Touch_Instr_Info& a) 
        {
            return (a.num_of_reads + a.num_of_writes) < 
                   (num_of_reads + num_of_writes);
        }
    };
    std::unordered_map<Addr, First_Touch_Instr_Info> fti_to_fti_info;
    std::unordered_map<Addr, std::vector<Addr>> fti_to_page_id;

  public:
    PrefEval() {}

    int pendingRequests() override { return 0; }

    void tick() override {}

    void registerStats(Simulator::Stats &stats)
    {

        std::vector<First_Touch_Instr_Info> fti_sorted;
        for (auto [fti, fti_info] : fti_to_fti_info)
        {
            fti_sorted.push_back(fti_info);
        }
        std::sort(fti_sorted.begin(), fti_sorted.end());

        unsigned print_fti_count = 0;
        for (auto &fti_info : fti_sorted)
        {
            // std::cout << "\n";
            // std::cout << "\n--------------------------------------\n";
            // if (print_fti_count == 10) break;
            // print_fti_count++;
            // std::cout << std::right << std::setw(18) << fti_info.eip << " ";

            auto pages_iter = fti_to_page_id.find(fti_info.eip);
            assert(pages_iter != fti_to_page_id.end());
            auto &page_ids = pages_iter->second;
            
            std::vector<Page_Info> page_info_sorted;
            for (auto id : page_ids)
            {
                auto page_info_iter = page_id_to_page_info.find(id);
                assert(page_info_iter != page_id_to_page_info.end());
                page_info_sorted.push_back(page_info_iter->second);
            }

            std::sort(page_info_sorted.begin(), page_info_sorted.end());

            bool pattern_and[page_size / block_size];
            for (auto &p : pattern_and) { p = true; }
            
            bool pattern_or[page_size / block_size];
            for (auto &p : pattern_or) { p = false; }
 
            bool pattern_max[page_size / block_size];
            for (auto &p : pattern_max) { p = false; }

            Count acc_cnt[page_size / block_size];
            for (auto &acc : acc_cnt) { acc = 0; }

            for (auto &page_info : page_info_sorted)
            {
                for (unsigned i = 0;
                              i < page_size / block_size; i++)
                {
                     pattern_and[i] = pattern_and[i] && page_info.offsets[i];
                     pattern_or[i] = pattern_or[i] || page_info.offsets[i];
                     if (page_info.offsets[i]) { acc_cnt[i]++; }
                }
            }

            for (unsigned i = 0; i < page_size / block_size; i++)
            {
                if (acc_cnt[i] >= ceil((float)(page_info_sorted.size()) / 2.0))
                { pattern_max[i] = true; }
            }

            /*
            // unsigned print_page_count = 0;
            for (auto &page_info : page_info_sorted)
            {
                std::string accessed_blocks = "";
                for (auto acc : page_info.offsets)
                {
                    if (acc) { accessed_blocks += "1"; }
                    else { accessed_blocks += "0"; }
                }

                std::cout << std::right << std::setw(18) 
                          << fti_info.eip << " "

                          << std::right << std::setw(18) 
                          << page_info.page_id << " "

                          << std::right << accessed_blocks << " "

                          << std::right << std::setw(18) 
                          << page_info.num_of_reads << " "

                          << std::right << std::setw(18) 
                          << page_info.num_of_writes << " "
                          << "\n";
                // print_page_count++;
            }
            */
            std::string str_pattern_and = "";
            std::string str_pattern_or = "";
            std::string str_pattern_max = "";

            for (unsigned i = 0; i < page_size / block_size; i++)
            {
                if (pattern_and[i]) { str_pattern_and += "1"; }
                else { str_pattern_and += "0"; }

                if (pattern_or[i]) { str_pattern_or += "1"; }
                else { str_pattern_or += "0"; }

                if (pattern_max[i]) { str_pattern_max += "1"; }
                else { str_pattern_max += "0"; } 
            }

            std::string fti_str = std::to_string(fti_info.eip);
            stats.registerStats(fti_str + " ", false);
            stats.registerStats(str_pattern_and + " ", false);
	    stats.registerStats(str_pattern_or + " ", false);
	    stats.registerStats(str_pattern_max + "\n", false);
//             std::cout << "\nAND Pattern: " << str_pattern_and << "\n";
//             std::cout << " OR Pattern: " << str_pattern_or << "\n";
//             std::cout << "MAX Pattern: " << str_pattern_max << "\n";
/*	    std::string accessed_blocks = "";
            for (auto acc : pattern)
            {
                if (acc) { accessed_blocks += "1"; }
                else { accessed_blocks += "0"; }
            }

            std::cout << std::right << std::setw(18) 
                      << fti_info.eip << " "

                      << std::right << accessed_blocks << "\n";
*/
        }
    }

    bool send(Request &req) override
    {
        Addr va = req.addr;
        Addr eip = req.eip;
        Addr virtual_page_id = va >> va_page_shift;
        Addr virtual_block_offset = (va & va_page_mask) >> block_shift;
        bool read_access = 1;
        if (req.isWrite()) { read_access = 0; }

        if (auto p_iter = page_id_to_page_info.find(virtual_page_id);
                p_iter != page_id_to_page_info.end())
        {
            // Mark accessed block
            p_iter->second.offsets[virtual_block_offset] = true;

            // Increment access
            if (read_access)
            {
                p_iter->second.num_of_reads += 1;
            }
            else
            {
                p_iter->second.num_of_writes += 1;
            }

            // Change the corresponding fit info
            Addr page_fti = p_iter->second.first_touch_instruction;
            auto fti_iter = fti_to_fti_info.find(page_fti);
            assert(fti_iter != fti_to_fti_info.end());

            if (read_access)
            {
                fti_iter->second.num_of_reads += 1;
            }
            else
            {
                fti_iter->second.num_of_writes += 1 ;
            }
        }
        else
        {
            // Insert page
            Page_Info page_info;
            page_info.page_id = virtual_page_id;
            page_info.first_touch_instruction = eip;
            if (read_access)
            {
                page_info.num_of_reads = 1;
                page_info.num_of_writes = 0;
            }
            else
            {
                page_info.num_of_reads = 0;
                page_info.num_of_writes = 1;
            }
            page_info.offsets[virtual_block_offset] = true;

            // Insert new page
            page_id_to_page_info.insert({virtual_page_id, page_info});

            if (auto fti_iter = fti_to_fti_info.find(eip);
                    fti_iter == fti_to_fti_info.end())
            {
                // Insert FTI
                if (read_access)
                {
                    fti_to_fti_info.insert({eip, {eip, 1, 0}});
                }
                else
                {
                    fti_to_fti_info.insert({eip, {eip, 0, 1}});
                }
                // Insert the touched page id
                std::vector<Addr> touched_pages;
                touched_pages.push_back(virtual_page_id);
                fti_to_page_id.insert({eip, touched_pages});
            }
	    else
            {
                // Update FIT info
                if (read_access)
                {
                    fti_iter->second.num_of_reads += 1;
                }
                else
                {
                    fti_iter->second.num_of_writes += 1 ;
                }

                // Insert the touched page id
                auto fti_to_pid_iter = fti_to_page_id.find(eip);
                assert(fti_to_pid_iter != fti_to_page_id.end());
                fti_to_pid_iter->second.push_back(virtual_page_id);
            }
        }

        // std::cout << eip << " " << va << " " << virtual_page_id << " "
        //           << virtual_block_offset << "\n";

        return true;
    }
};
}

#endif
