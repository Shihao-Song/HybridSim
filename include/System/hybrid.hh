#ifndef __HYBRID_HH__
#define __HYBRID_HH__

#include <algorithm>
#include <random>

#include "System/mmu.hh"

namespace System
{
class Hybrid : public MMU
{
  protected:
    // Data structure for page information
    struct Page_Info
    {
        Addr page_id; // virtual page_id
        Addr re_alloc_page_id; // A page may be re-allocated to a different location

        Addr fti; // PC that triggers the page access

        bool in_dram = false;
        bool in_pcm = false;

        uint64_t num_of_reads = 0; // Number of reads to the page
        uint64_t num_of_writes = 0; // Number of writes to the page
    };
    // All the touched pages for each core.
    std::vector<std::unordered_map<Addr,Page_Info>> pages_by_cores;

    // Memory sizes 
    std::vector<unsigned> mem_size_in_gb;

    // PageID helper, one for DRAM, one for PCM
    std::vector<PageIDHelper> page_id_helpers_by_technology;

    // A pool of free physical pages, one for DRAM, one for PCM
    std::vector<Addr> free_frames;
    
    // A pool of used physical pages, one for DRAM, one for PCM
    std::vector<std::unordered_map<Addr,bool>> used_frame_pool_by_technology;

  public:
    Hybrid(int num_of_cores, Config &dram_cfg, Config &pcm_cfg)
        : MMU(num_of_cores)
    {
        pages_by_cores.resize(num_of_cores);

        mem_size_in_gb.push_back(dram_cfg.sizeInGB());
        mem_size_in_gb.push_back(pcm_cfg.sizeInGB());

        page_id_helpers_by_technology.emplace_back(dram_cfg);
        page_id_helpers_by_technology.emplace_back(pcm_cfg);

        used_frame_pool_by_technology.resize(int(Config::Memory_Node::MAX));

        // Construct all available pages
        auto rng = std::default_random_engine {};
        Addr base = 0;
        for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
        {
            // TODO, we hard-coded page as 4KB
            for (int i = 0; i < mem_size_in_gb[m] * 1024 * 1024 / 4; i++)
            {
                // All available pages
                Addr page_id = i + base; 
                free_frames.push_back(page_id);
            }
            base += mem_size_in_gb[m] * 1024 * 1024 / 4;
        }

        // TODO, enable this line!
        std::shuffle(std::begin(free_frames),
                     std::end(free_frames), rng);
        // std::cout << "Total number of pages: "
        //           << free_frames.size() << "\n\n";

        // exit(0);
    }

    // Default: randomly map a virtual page to DRAM or PCM (segment is not considered)
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

            if (req.req_type == Request::Request_Type::READ) 
            { (p_iter->second.num_of_reads)++; }
            if (req.req_type == Request::Request_Type::WRITE) 
            { (p_iter->second.num_of_writes)++; }
        }
        else
        {
            static int total_mem_size = mem_size_in_gb[int(Config::Memory_Node::DRAM)] + 
                                        mem_size_in_gb[int(Config::Memory_Node::PCM)];

            static std::default_random_engine e{};
            static std::uniform_int_distribution<int> d_tech{1, total_mem_size};

            bool in_dram, in_pcm;
            // Randomly determine which technology to be mapped
            int chosen_technology = 0;
            
            if (int random_num = d_tech(e);
                random_num <= mem_size_in_gb[int(Config::Memory_Node::DRAM)])
            {
                chosen_technology = int(Config::Memory_Node::DRAM);
                // std::cout << "Mapped to DRAM \n";
                in_dram = true; in_pcm = false;
            }
            else
            {
                chosen_technology = int(Config::Memory_Node::PCM);
                // std::cout << "Mapped to PCM \n";
                in_dram = false; in_pcm = true;
            }
            
            // chosen_technology = int(Config::Memory_Node::DRAM);
            // in_dram = true; in_pcm = false;

            auto &used_frames = used_frame_pool_by_technology[chosen_technology];
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
            // std::cout << "\nPhysical address: " << req.addr << "\n\n";
            // Insert the page
            uint64_t num_of_reads = 0;
            uint64_t num_of_writes = 0;
            if (req.req_type == Request::Request_Type::READ) { num_of_reads++; }
            if (req.req_type == Request::Request_Type::WRITE) { num_of_writes++; }

            pages.insert({virtual_page_id, {virtual_page_id, 
                                            free_frame,
                                            req.eip,
                                            in_dram,
                                            in_pcm,
                                            num_of_reads,
                                            num_of_writes}});
        }
    }

    int memoryNode(Request &req) override
    {
        Addr page_id = req.addr >> Mapper::va_page_shift;
        // std::cout << "Page ID: " << page_id << "\n";
        for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
        {
            auto &used_frames = used_frame_pool_by_technology[m];

            if (auto p_iter = used_frames.find(page_id);
                    p_iter != used_frames.end())
            {
                return m;
            }
        }

        std::cerr << "Invalid Page ID.\n";
        exit(0);
    }

    void registerStats(Simulator::Stats &stats)
    {
        uint64_t num_pages = 0;
        uint64_t num_pages_in_pcm = 0;
        uint64_t num_pages_in_dram = 0;

        for (auto &pages : pages_by_cores)
        {
            num_pages += pages.size();

            for (auto [v_page, page_info] : pages)
            {
                if (page_info.in_dram == true) { num_pages_in_dram++; }
                if (page_info.in_pcm == true) { num_pages_in_pcm++; }
            }
        }

	std::string prin = "MMU_Total_Pages = " + std::to_string(num_pages);
        stats.registerStats(prin);

        prin = "MMU_Pages_in_DRAM = " + std::to_string(num_pages_in_dram);
        stats.registerStats(prin);
        prin = "MMU_Pages_in_PCM = " + std::to_string(num_pages_in_pcm);
        stats.registerStats(prin);
    }
};
}

#endif
