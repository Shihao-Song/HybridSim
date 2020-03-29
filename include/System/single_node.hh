#ifndef __SINGLE_NODE_HH__
#define __SINGLE_NODE_HH__

#include <algorithm>
#include <random>

#include "System/mmu.hh"

namespace System
{
class SingleNode : public MMU
{
  protected:
    // Data structure for page information
    struct Page_Info
    {
        Addr page_id; // virtual page_id
        Addr re_alloc_page_id; // A page may be re-allocated to a different location (see below)
        
        Addr first_touch_instruction; // The first-touch instruction that brings in this page

        bool near_segment = false;
        bool far_segment = false;

        uint64_t num_of_reads = 0; // Number of reads to the page
        uint64_t num_of_writes = 0; // Number of writes to the page

        // Number of phases the page hasn't been touched.
        unsigned num_of_phases_silent = 0;
    };
    // All the touched pages for each core.
    std::vector<std::unordered_map<Addr,Page_Info>> pages_by_cores;

    // Data structure for first-touch instruction (FTI)
    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;

        // A FTI can allocate a page to the following location.
        bool near_segment = false;
        bool far_segment = false;

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    // All the FTIs for each core
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> FTIs_by_cores;

    // PageID helper
    PageIDHelper page_id_helper;

    // A pool of free physical pages
    std::vector<Addr> free_frame_pool;
    
    // A pool of free fast-access physical pages.
    // TODO, only considered PCM here.
    const unsigned NUM_FAST_ACCESS_ROWS = 512;
    std::vector<Addr> free_fast_access_frame_pool;

    // A pool of free slow-access physical pages.
    std::vector<Addr> free_slow_access_frame_pool;

    // A pool of used physical pages.
    std::unordered_map<Addr,bool> used_frame_pool;

  public:
    SingleNode(int num_of_cores, Config &pcm_cfg)
        : MMU(num_of_cores)
        , page_id_helper(pcm_cfg)
    {
        pages_by_cores.resize(num_of_cores);
        FTIs_by_cores.resize(num_of_cores);

        // Construct all available pages
        auto rng = std::default_random_engine {};
        for (int i = 0; i < pcm_cfg.sizeInGB() * 1024 * 1024 / 4; i++)
        {
            // All available pages
            free_frame_pool.push_back(i);

            // All free fast-access and slow-access physical pages
            auto &mem_addr_decoding_bits = page_id_helper.mem_addr_decoding_bits;
            std::vector<int> dec_addr;
            dec_addr.resize(mem_addr_decoding_bits.size());
            Decoder::decode(i << Mapper::va_page_shift,
                            mem_addr_decoding_bits,
                            dec_addr);

            int row_idx = page_id_helper.row_idx;
            if (dec_addr[row_idx] < NUM_FAST_ACCESS_ROWS)
            {
                free_fast_access_frame_pool.push_back(i);
            }
            else
            {
                free_slow_access_frame_pool.push_back(i);
            }
        }
        std::shuffle(std::begin(free_fast_access_frame_pool),
                     std::end(free_fast_access_frame_pool), rng);

        std::shuffle(std::begin(free_slow_access_frame_pool),
                     std::end(free_slow_access_frame_pool), rng);

        /*
        std::shuffle(std::begin(free_frame_pool),
                     std::end(free_frame_pool), rng);
        std::cout << "Number of fast-access pages: "
                  << free_fast_access_frame_pool.size() << "\n";
        std::cout << "Number of slow-access pages: "
                  << free_slow_access_frame_pool.size() << "\n";
        std::cout << "Total number of pages: "
                  << free_frame_pool.size() << "\n\n";
        exit(0);
        */
    }

    void va2pa(Request &req) override
    {
        int core_id = req.core_id;

        Addr va = req.addr;
        Addr virtual_page_id = va >> Mapper::va_page_shift;

        auto &pages = pages_by_cores[core_id];

        if (auto p_iter = pages.find(virtual_page_id);
                p_iter != pages.end())
        {
            Addr page_id = p_iter->second.re_alloc_page_id;
            req.addr = (page_id << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);
        }
        else
        {
            // TODO, randomize page selection from slow-access region and 
            // fast-access region.

            auto &free_frames = free_slow_access_frame_pool;
            if (free_frames.size() == 0)
            {
                free_frames = free_fast_access_frame_pool;
            }

            auto &used_frames = used_frame_pool;
            // std::cout << "Size of free frames: " << free_frames.size() << "\n";
            // std::cout << "Size of used frames: " << used_frames.size() << "\n";

            // Choose a free frame
            Addr free_frame = *(free_frames.begin());
            // for (int i = 0; i < 11; i++)
            // {
            //     std::cout << free_frames[i] << "\n";
            // }

            free_frames.erase(free_frames.begin());
            used_frames.insert({free_frame, true});

            // std::cout << "\nSize of free frames: " << free_frames.size() << "\n";
            // std::cout << "Size of used frames: " << used_frames.size() << "\n";
            // for (int i = 0; i < 10; i++)
            // {
            //     std::cout << free_frames[i] << "\n";
            // }

            req.addr = (free_frame << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);
            // std::cout << "\nPhysical address: " << req.addr << "\n";
            // Insert the page
            pages.insert({virtual_page_id, {virtual_page_id, free_frame}});
            // exit(0);
        }
    }

    void registerStats(Simulator::Stats &stats)
    {
        uint64_t num_pages = 0;
        for (auto &pages : pages_by_cores)
        {
            num_pages += pages.size();
        }

        std::string prin = "MMU_Total_Pages = " + std::to_string(num_pages);
        stats.registerStats(prin);
    }
};
}

#endif
