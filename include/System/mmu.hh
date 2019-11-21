#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <set>
#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/decoder.hh"
#include "Sim/mapper.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/stats.hh"
#include "Sim/trace.hh"


namespace System
{
class MMU
{
  protected:
    typedef Simulator::MemObject MemObject;

    typedef Simulator::Mapper Mapper;
    std::vector<Mapper> mappers;

    typedef Simulator::Request Request;

    unsigned num_of_cores;

  public:
    typedef uint64_t Addr;

    MMU(int num_cores) : num_of_cores(num_cores)
    {
        for (int i = 0; i < num_cores; i++)
        {
            mappers.emplace_back(i);
        }
    }

    virtual void va2pa(Request &req)
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;
    }
};

class TrainedMMU : public MMU
{
  protected:
    MemObject *mem_system;

  public:
    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Instruction Instruction;
    typedef Simulator::Trace Trace;

    TrainedMMU(int num_of_cores, Config &cfg)
        : MMU(num_of_cores)
    {}

    ~TrainedMMU()
    {
        if (mmu_profiling_data_output_file != "N/A")
        {
            mmu_profiling_data_out.close();
        }
    }

    virtual bool pageMig() {}

    virtual void printProfiling() {}

    virtual void setProfilingStage() { profiling_stage = true; inference_stage = false; }
    virtual void setInferenceStage() { inference_stage = true; profiling_stage = false; }

    virtual void phaseDone() {}
    virtual void setSizes(std::vector<int> sizes) {}

    void setMemSystem(MemObject *_sys) { mem_system = _sys; }

    virtual void registerStats(Simulator::Stats &stats) {}

  protected:
    bool profiling_stage = false;
    bool inference_stage = false;

    std::string mmu_profiling_data_output_file = "N/A";
    std::ofstream mmu_profiling_data_out;

  public:
    void profilingDataOutput(std::string file)
    {
        mmu_profiling_data_output_file = file;
        //mmu_profiling_data_out.open(file);
        //assert(mmu_profiling_data_out.good());
    }

  // All caches, for page migrations, write-backs must be completed before any pages to be
  // migrated.
  // public:
  //   std::vector<MemObject *>L1;
  //   MemObject *L2;
};

// TODO, Limitations
// (1) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank, 1 GB bank for each memory
// (2) Decoding is fixed: Rank, Partition, Tile, Row, Col, Bank, Channel, Cache_Line
// (3) DRAM and PCM share the same memory space. Implementation-wise, number of ranks
// are doubled, ranks 0~3 are for PCM; ranks 4~7 are for DRAM.
class Hybrid : public TrainedMMU
{
  protected:
    unsigned base_rank_id_pcm;
    unsigned base_rank_id_dram;

    // mem_addr_decoding_bits is used to determine the physical location of the page.
    const std::vector<int> mem_addr_decoding_bits;

  protected:
    struct Page_Info
    {
        Addr page_id;
        Addr first_touch_instruction; // The first-touch instruction that brings in this page

        bool in_dram = false; // Initially, all the pages are in PCM instead of DRAM.

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;

        // Number of phases the page hasn't been touched.
        unsigned num_of_phases_silent = 0;
    };
    std::unordered_map<Addr,Page_Info> pages; // All the touched (allocated) pages
    std::unordered_map<Addr,Addr> re_alloc_pages;

    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;
        bool in_dram = false;

        uint64_t num_hits = 0;
        uint64_t num_pages = 0;

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    // For every FTI in first_touch_instructions, if in_dram is true, allocate the page into DRAM,
    // if not true, allocate to the fast region of PCM.
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> first_touch_instructions;
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> fti_candidates;

    int num_ftis_per_phase = 8;

    struct Mig_Page
    {
        Addr page_id;

        bool in_dram = false;
        bool done = false;

        unsigned num_reads_left; // Number of reads from PCM
        unsigned num_writes_left; // Number of writes to DRAM
    };
    std::vector<Mig_Page> pages_to_migrate;

    // std::ofstream output;

  public: 
    Hybrid(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg)
        , base_rank_id_pcm(0)
        , base_rank_id_dram(cfg.num_of_ranks / 2)
        , mem_addr_decoding_bits(cfg.mem_addr_decoding_bits)
        , num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 / cfg.block_size)
        , num_of_rows(cfg.num_of_word_lines_per_tile)
        , num_of_tiles(cfg.num_of_tiles)
        , num_of_partitions(cfg.num_of_parts)
        , num_of_ranks(cfg.num_of_ranks)
    {
        first_touch_instructions.resize(num_of_cores);
        fti_candidates.resize(num_of_cores);
        // std::cout << base_rank_id_pcm << "\n";
        // std::cout << base_rank_id_dram << "\n";
        // output.open("analysis.txt");
        srand(time(NULL));
    }

    void registerStats(Simulator::Stats &stats) override
    {
        for (int i = 0; i < num_of_mig_pages_to_dram.size(); i++)
        {
            std::string mig = "Total_Mig_Pages_To_DRAM_" + std::to_string(i) + " = " + 
                              std::to_string(num_of_mig_pages_to_dram[i]);
            stats.registerStats(mig);
        }
        for (int i = 0; i < num_of_mig_pages_to_pcm.size(); i++)
        {
            std::string mig = "Total_Mig_Pages_To_PCM_" + std::to_string(i) + " = " + 
                              std::to_string(num_of_mig_pages_to_pcm[i]);
            stats.registerStats(mig);
        }

        /*
        std::string mig_reads = "Total_Mig_Reads = " +
                                 std::to_string(mig_num_pcm_reads);
        std::string mig_writes = "Total_Mig_Writes = " +
                                 std::to_string(mig_num_dram_writes);
        stats.registerStats(mig_reads);
        stats.registerStats(mig_writes);
        */
    }

    /*
    void va2pa(Request &req) override
    {
    Addr pa = mappers[req.core_id].va2pa(req.addr);
    Addr page_id = pa >> Mapper::va_page_shift;

    // Has the page already allocated?
    if (auto iter = re_alloc_pages.find(page_id);
             iter != re_alloc_pages.end())
    {
        Addr new_page_id = iter->second;
        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA
    }
    else
    {
        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(page_id << Mapper::va_page_shift,
                        mem_addr_decoding_bits,
                        dec_addr);

        int random_rank_id = rand() % 5;
        dec_addr[int(Config::Decoding::Rank)] = random_rank_id;
        
        Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
        Addr tmp = pa;
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp >> mem_addr_decoding_bits[i];
        }
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp << mem_addr_decoding_bits[i];
        }
        Addr new_page_id = (page_recon | tmp)  >> Mapper::va_page_shift;

        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        re_alloc_pages.insert({page_id, new_page_id});
    }
    }
    */
    void va2pa(Request &req) override
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;

        Addr pc = req.eip;
	Addr page_id = pa >> Mapper::va_page_shift;

        // Step One, get the page_id.
        // Initially, all the pages should be inside PCM
        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        // std::cout << "Page ID: " << page_id << "\n";
        Decoder::decode(page_id << Mapper::va_page_shift,
                        mem_addr_decoding_bits,
                        dec_addr);

        // TODO, this is not good for page-intensive applications, there can be 
        // some conflicts.
        if (dec_addr[int(Config::Decoding::Rank)] >= base_rank_id_dram)
        {
            dec_addr[int(Config::Decoding::Rank)] -= base_rank_id_dram;
        }

        Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
        Addr tmp = pa;
	for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp >> mem_addr_decoding_bits[i];
        }
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp << mem_addr_decoding_bits[i];
        }
        Addr new_page_id = (page_recon | tmp)  >> Mapper::va_page_shift;
        // std::cout << "New Page ID: " << new_page_id << "\n";
        // exit(0);

        Addr new_pa = (new_page_id << Mapper::va_page_shift) |
                      (pa & Mapper::va_page_mask);
        req.addr = new_pa; // Replace with the new PA

        // At this point, all the pages are inside PCM, we will record all the page accesses.
        // new_page_id equals to (old) page_id if rank_id is below base_rank_id_dram
        // Step two, track page access information
        if (auto p_iter = pages.find(new_page_id);
                p_iter == pages.end())
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

            pages.insert({new_page_id, {new_page_id,
                                        pc,
                                        false,
                                        num_of_reads,
                                        num_of_writes,
                                        0}});
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

            // The page has remained silient for some phases, reset it to zero.
            if ((p_iter->second).num_of_phases_silent > 0)
            {
                (p_iter->second).num_of_phases_silent = 0;
            }

            // Step three, is this page now in DRAM?
            if (p_iter->second.in_dram)
            {
                // exit(0);
                // It has been migrated to DRAM.
                dec_addr[int(Config::Decoding::Rank)] += base_rank_id_dram;

                Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
                Addr tmp = pa;
                for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
                {
                    tmp = tmp >> mem_addr_decoding_bits[i];
                }
                for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
                {
                    tmp = tmp << mem_addr_decoding_bits[i];
                }
                Addr new_page_id = (page_recon | tmp)  >> Mapper::va_page_shift;

                Addr new_pa = new_page_id << Mapper::va_page_shift |
                              pa & Mapper::va_page_mask;

                req.addr = new_pa; // Replace with the new PA
            }
        }
    }

    virtual bool pageMig()
    {
        if (!mig_ready) { prepMig(); return false; }

        // First step, finish all the write-backs (In the future).

        // Second step, finish all the page migration
        for (int i = 0; i < pages_to_migrate.size(); i++)
        {
            if (!pages_to_migrate[i].done)
            {
                if (pages_to_migrate[i].num_reads_left > 0)
                {
                    // One cache-line at one time
                    uint64_t granuality = 64;
                    uint64_t offset = granuality * uint64_t(pages_to_migrate[i].num_reads_left - 1);

                    Addr target_addr = (pages_to_migrate[i].page_id << Mapper::va_page_shift) + offset;
                    Request req(target_addr, Request::Request_Type::READ);
                    req.mig = true;
                    if (!(pages_to_migrate[i].in_dram))
                    {
                        req.req_type = Request::Request_Type::READ;
                    }
                    else
                    {
                        req.req_type = Request::Request_Type::WRITE;
                    }
                    // std::cout << pages_to_migrate[i].page_id << "\n";
                    // req.display = true;
                    if (mem_system->send(req))
                    {
                        --pages_to_migrate[i].num_reads_left;
                        // ++mig_num_pcm_reads;
                    }

                    return false;
                }

                if (pages_to_migrate[i].num_writes_left > 0)
                {
                    // One cache-line at one time
                    uint64_t granuality = 64;
                    uint64_t offset = granuality * uint64_t(pages_to_migrate[i].num_writes_left - 1);

                    Addr old_addr = pages_to_migrate[i].page_id << Mapper::va_page_shift; // Base
                    std::vector<int> dec_addr;
                    dec_addr.resize(mem_addr_decoding_bits.size());
                    Decoder::decode(old_addr,
                                    mem_addr_decoding_bits,
                                    dec_addr);

                    dec_addr[int(Config::Decoding::Rank)] += base_rank_id_dram;
                    Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
                    Addr tmp = old_addr;
                    for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
                    {
                        tmp = tmp >> mem_addr_decoding_bits[i];
                    }
                    for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
                    {
                        tmp = tmp << mem_addr_decoding_bits[i];
                    }
                    Addr target_addr = (page_recon | tmp) + offset;

                    // std::cout << target_addr << "\n";
                    Request req(target_addr, Request::Request_Type::WRITE);
		    req.mig = true;
                    if (!(pages_to_migrate[i].in_dram))
                    {
                        req.req_type = Request::Request_Type::WRITE;
                    }
                    else
                    {
                        req.req_type = Request::Request_Type::READ;
                    }
                    // req.display = true;
                    if (mem_system->send(req))
                    {
                        --pages_to_migrate[i].num_writes_left;
                        // ++mig_num_dram_writes;
                    }

                    return false;
                }
                
                // exit(0);
                pages_to_migrate[i].done = true;
            }
        }

        unsigned num_pages_from_pcm_to_dram = 0;
        unsigned num_pages_from_dram_to_pcm = 0;
        for (int i = 0; i < pages_to_migrate.size(); i++)
        {
            if (pages_to_migrate[i].in_dram)
            {
                ++num_pages_from_dram_to_pcm;
            }
            else
            {
                ++num_pages_from_pcm_to_dram;
            }
        }

        num_of_mig_pages_to_dram.push_back(num_pages_from_pcm_to_dram);
        num_of_mig_pages_to_pcm.push_back(num_pages_from_dram_to_pcm);
        // TODO, (1) reset mig_ready to false; (2) clear pages_to_migrate
        mig_ready = false;
        pages_to_migrate.clear();

        for (auto iter = pages.begin(); iter != pages.end(); iter++)
        {
            if ((iter->second).num_of_reads + (iter->second).num_of_writes == 0)
            {
                ++(iter->second).num_of_phases_silent;
            }

            (iter->second).num_of_reads = 0;
            (iter->second).num_of_writes = 0;
        }

        return true;
    }

    /* FTI Section */
    /*
    void va2pa(Request &req) override
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        // std::cout << pa << "\n";
        Addr old_page_id = pa >> Mapper::va_page_shift;
        // Initially, all the pages should be inside PCM
        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        // std::cout << "Page ID: " << page_id << "\n";
        Decoder::decode(old_page_id << Mapper::va_page_shift,
                        mem_addr_decoding_bits,
                        dec_addr);

        // TODO, this is not good for page-intensive applications, there can be 
        // some conflicts.
        if (dec_addr[int(Config::Decoding::Rank)] >= base_rank_id_dram)
        {
            dec_addr[int(Config::Decoding::Rank)] -= base_rank_id_dram;
        }

        Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
        Addr tmp = pa;
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp >> mem_addr_decoding_bits[i];
        }
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp << mem_addr_decoding_bits[i];
        }
        Addr page_id = (page_recon | tmp)  >> Mapper::va_page_shift;
        // std::cout << "New Page ID: " << new_page_id << "\n";
        // exit(0);
        Addr new_pa = (page_id << Mapper::va_page_shift) |
                      (pa & Mapper::va_page_mask);
        req.addr = new_pa; // Replace with the new PA
        // std::cout << req.addr << "\n";
        // exit(0);

        // Run-time profiling
        runtimeProfiling(req);

        // Run-time reallocation
        runtimeReAlloc(req);
    }


    void runtimeProfiling(Request& req)
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
        if (auto f_instr = first_touch_instructions[req.core_id].find(pc);
                f_instr != first_touch_instructions[req.core_id].end())
        {
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
                ++f_instr->second.num_hits;
                ++f_instr->second.num_pages;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
                ++f_instr->second.num_hits;
                ++f_instr->second.num_pages;
            }
        }
        // Step two (2), cached in fti_candidates?
        else if (auto f_instr = fti_candidates[req.core_id].find(pc);
                f_instr != fti_candidates[req.core_id].end())
        {
            // Yes, cached!
            if (req.req_type == Request::Request_Type::READ)
            {
                ++f_instr->second.num_of_reads;
                ++f_instr->second.num_hits;
                ++f_instr->second.num_pages;
            }
            else if (req.req_type == Request::Request_Type::WRITE)
            {
                ++f_instr->second.num_of_writes;
                ++f_instr->second.num_hits;
                ++f_instr->second.num_pages;
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

            fti_candidates[req.core_id].insert({pc,
                                  {pc,
                                   false,
                                   0, // no hits initially
                                   1, // brought in one page
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
                                false,
                                num_of_reads,
                                num_of_writes,
				0}});
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

        Addr corres_fti = p_iter->second.first_touch_instruction;
        if (auto f_instr = first_touch_instructions[req.core_id].find(corres_fti);
                f_instr != first_touch_instructions[req.core_id].end())
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
        else if (auto f_instr = fti_candidates[req.core_id].find(corres_fti);
                f_instr != fti_candidates[req.core_id].end())
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

    void runtimeReAlloc(Request& req)
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

    // Should we allocate in DRAM?
    if (auto iter = first_touch_instructions[req.core_id].find(pc);
             iter != first_touch_instructions[req.core_id].end())
    {
        std::vector<int> dec_addr;
        dec_addr.resize(mem_addr_decoding_bits.size());
        Decoder::decode(page_id << Mapper::va_page_shift,
                        mem_addr_decoding_bits,
                        dec_addr);

        // For FTI in DRAM, do this
        if (iter->second.in_dram)
	{
            dec_addr[int(Config::Decoding::Rank)] += base_rank_id_dram;
        }
        // For FTI in PCM, do this
        else
        {
            dec_addr[int(Config::Decoding::Rank)] = cur_re_alloc_page.rank_id;
            dec_addr[int(Config::Decoding::Partition)] = cur_re_alloc_page.part_id;
            dec_addr[int(Config::Decoding::Tile)] = cur_re_alloc_page.tile_id;
            dec_addr[int(Config::Decoding::Row)] = cur_re_alloc_page.row_id;
            dec_addr[int(Config::Decoding::Col)] = cur_re_alloc_page.col_id;
            nextReAllocPage(cur_re_alloc_page, int(INCREMENT_LEVEL::RANK));
        }

        Addr page_recon = Decoder::reConstruct(dec_addr, mem_addr_decoding_bits);
        Addr tmp = pa;
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp >> mem_addr_decoding_bits[i];
        }
        for (int i = 0; i < mem_addr_decoding_bits.size(); i++)
        {
            tmp = tmp << mem_addr_decoding_bits[i];
        }
        Addr new_page_id = (page_recon | tmp)  >> Mapper::va_page_shift;
        pages.find(page_id)->second.in_dram = true;

        Addr new_pa = new_page_id << Mapper::va_page_shift |
                      pa & Mapper::va_page_mask;

        req.addr = new_pa; // Replace with the new PA

        re_alloc_pages.insert({page_id, new_page_id});
    }
    }
    */
    void phaseDone()
    {
    // for (int core = 0; core < num_of_cores; core++)
    // {
    /*
    std::cout << "Core: " << core << "\n";
    for (auto [key, value] : first_touch_instructions[core])
    {
        std::cout << value.eip << " : "
                  << value.num_of_reads << " : "
                  << value.num_of_writes << "\n";
    }
    std::cout << "\n";
    */
//    output << "FTI Table: \n";
//    for (auto [key, value] : first_touch_instructions)
//    {
//        output << "PC: " << value.eip << "\n";
//        output << "Allocation Hits: " << value.num_hits << "\n";
//        output << "Number of New Pages It Allocated: " << value.num_pages << "\n";
//        output << "Number of Accesses to All Pages It Allocated: "
//               << (value.num_of_reads + value.num_of_writes) << "\n\n";
//    }
//    output << "************************\n";

//    output << "AIR Table: \n";
    /*
    std::vector<First_Touch_Instr_Info> ordered_by_ref;
    for (auto [key, value] : fti_candidates[core])
    {
//        output << "PC: " << value.eip << "\n";
//        output << "Allocation Hits: " << value.num_hits << "\n";
//        output << "Number of New Pages It Allocated: " << value.num_pages << "\n";
//        output << "Number of Accesses to All Pages It Allocated: "
//               << (value.num_of_reads + value.num_of_writes) << "\n\n";

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
        // TODO, should have a threshold to determine to whether it is a write-intensive 
        // page or not. Urgent! 
        if (ordered_by_ref[i].num_of_writes >= 0)
        {
            ordered_by_ref[i].in_dram = true;
        }
        else
        {
            ordered_by_ref[i].in_dram = false;
        }
        first_touch_instructions[core].insert({ordered_by_ref[i].eip, ordered_by_ref[i]});
    }

    fti_candidates[core].clear();
    }
    */
    /*
    for (auto iter = first_touch_instructions.begin(); 
              iter != first_touch_instructions.end();
              iter++)
    {
        iter->second.num_hits = 0;
        iter->second.num_pages = 0;
        iter->second.num_of_reads = 0;
        iter->second.num_of_writes = 0;
    }
    */

    // static unsigned phase = 0;
    // std::ofstream output("pages/" + std::to_string(phase) + ".csv");
    // std::vector<Page_Info> MFU_pages_profiling;
    // for (auto [key, value] : pages)
    // {
    //     MFU_pages_profiling.push_back(value);
    // }
    // std::sort(MFU_pages_profiling.begin(), MFU_pages_profiling.end(),
    //           [](const Page_Info &a, const Page_Info &b)
    //           {
    //               return (a.num_of_reads + a.num_of_writes) >
    //                      (b.num_of_reads + b.num_of_writes);
    //           });
    // for (int i = 0; i < MFU_pages_profiling.size(); i++)
    // {
    //     output << MFU_pages_profiling[i].page_id << ","
    //            << MFU_pages_profiling[i].first_touch_instruction << ","
    //            << (MFU_pages_profiling[i].num_of_reads + 
    //                MFU_pages_profiling[i].num_of_writes) << ","
    //            << MFU_pages_profiling[i].in_dram << "\n";
    // }

    /*
    for (auto iter = pages.begin(); iter != pages.end(); iter++)
    {
        if ((iter->second).num_of_reads + (iter->second).num_of_writes == 0)
        {
            ++(iter->second).num_of_phases_silent;
        }

        (iter->second).num_of_reads = 0;
        (iter->second).num_of_writes = 0;
    }
    */
    // output.close();
    // phase++;
    // 
//    output << "**************************************************\n\n";
//    output << std::flush;
    }

  protected:
    bool mig_ready = false;

    // uint64_t mig_num_pcm_reads = 0;
    // uint64_t mig_num_dram_writes = 0;
    std::vector<unsigned> num_of_mig_pages_to_dram;
    std::vector<unsigned> num_of_mig_pages_to_pcm;

    void prepMig()
    {
        assert(pages_to_migrate.size() == 0);

        uint64_t total_accesses = 0;
        std::vector<Page_Info> MFU_pages_profiling;

        for (auto [key, value] : pages)
        {
            if (!value.in_dram)
            {
                MFU_pages_profiling.push_back(value);
                total_accesses += value.num_of_reads;
                total_accesses += value.num_of_writes;
            }
            else
            {
                // The page is already in DRAM
                if (value.num_of_phases_silent)
                {
                    // This page needs to migrated to PCM
                    // The page is no longer in DRAM
                    pages.find(value.page_id)->second.in_dram = false;

                    // Push to pages_to_migrate to wait for migration.
                    unsigned num_cache_lines_per_page = 4096 / 64;
                    pages_to_migrate.push_back({value.page_id,
                                                true,
                                                false,
                                                num_cache_lines_per_page,
                                                num_cache_lines_per_page});
                }
            }
	    // else
	    // {
	    //     std::cout << value.page_id << " : "
            //               << (value.num_of_reads + value.num_of_writes) << "\n";
	    // }
        }
        // std::cout << "\n";

        std::sort(MFU_pages_profiling.begin(), MFU_pages_profiling.end(),
                  [](const Page_Info &a, const Page_Info &b)
                  {
                      return (a.num_of_reads + a.num_of_writes) >
                             (b.num_of_reads + b.num_of_writes);
                  });

        uint64_t cur_accesses = 0;
        for (int i = 0; i < MFU_pages_profiling.size(); i++)
        {
            // std::cout << MFU_pages_profiling[i].page_id << " : "
            //           << (MFU_pages_profiling[i].num_of_reads + 
            //               MFU_pages_profiling[i].num_of_writes) << "\n";

            // Update pages information.
            pages.find(MFU_pages_profiling[i].page_id)->second.in_dram = true;

            // Push to pages_to_migrate to wait for migration.
            unsigned num_cache_lines_per_page = 4096 / 64;
            pages_to_migrate.push_back({MFU_pages_profiling[i].page_id,
                                        false,
                                        false,	
                                        num_cache_lines_per_page,
                                        num_cache_lines_per_page});

            cur_accesses += MFU_pages_profiling[i].num_of_reads;
            cur_accesses += MFU_pages_profiling[i].num_of_writes;

            if (cur_accesses >= total_accesses * 0.6) { break; }
        }
        mig_ready = true;
    }

    // Our technique
    const unsigned num_of_cache_lines_per_row;
    const unsigned num_of_rows;
    const unsigned num_of_tiles;
    const unsigned num_of_partitions;
    const unsigned num_of_ranks;
    const unsigned num_of_near_rows = 512;

    struct PageLoc // Physical location
    {
        unsigned rank_id = 0;
        unsigned part_id = 0;
        unsigned tile_id = 0;
        unsigned row_id = 0;
        unsigned col_id = 0;

        PageLoc& operator=(PageLoc other)
        {
            rank_id = other.rank_id;
            part_id = other.part_id;
            tile_id = other.tile_id;
            row_id = other.row_id;
            col_id = other.col_id;
            return *this;
        }

        bool operator==(const PageLoc &other) const
        {
            return rank_id == other.rank_id &&
                   part_id == other.part_id &&
                   tile_id == other.tile_id &&
                   row_id == other.row_id &&
                   col_id == other.col_id;
        }
    };
    bool near_region_full = false;
    PageLoc cur_re_alloc_page; // For pages to be allocated in the near segment (PCM)

    enum INCREMENT_LEVEL: int
    {
        COL,ROW,TILE,PARTITION,RANK
    };
    bool nextReAllocPage(PageLoc &cur_re_alloc_page, int incre_level)
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
                cur_re_alloc_page.rank_id == base_rank_id_dram)
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
};

// TODO, Limitations
// (1) I assume there are 4 channels, 4 ranks/channel, 8 banks/rank, 1 GB bank;
// (2) Decoding is fixed: Rank, Partition, Tile, Row, Col, Bank, Channel, Cache_Line
class NearRegionAware : public TrainedMMU
{
  protected:
    const unsigned num_of_cache_lines_per_row;
    const unsigned num_of_rows;
    const unsigned num_of_tiles;
    const unsigned num_of_partitions;
    const unsigned num_of_ranks;
    const unsigned num_of_near_rows = 512;

    // mem_addr_decoding_bits is used to determine the physical location of the page.
    const std::vector<int> mem_addr_decoding_bits;

  public:
    NearRegionAware(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg),
          num_of_cache_lines_per_row(cfg.num_of_bit_lines_per_tile / 8 / cfg.block_size),
          num_of_rows(cfg.num_of_word_lines_per_tile),
          num_of_tiles(cfg.num_of_tiles),
          num_of_partitions(cfg.num_of_parts),
          num_of_ranks(cfg.num_of_ranks),
//          num_of_near_rows(cfg.num_of_word_lines_per_tile /cfg.num_stages),
          mem_addr_decoding_bits(cfg.mem_addr_decoding_bits)
    {
        cur_re_alloc_page_far_seg.row_id = 512;
/*
        std::cout << "num_of_cache_lines_per_row: " << num_of_cache_lines_per_row << "\n";
        std::cout << "num_of_tiles: " << num_of_tiles << "\n";
        std::cout << "num_of_partitions: " << num_of_partitions << "\n";
        std::cout << "num_of_ranks: " << num_of_ranks << "\n";
        std::cout << "num_of_near_rows: " << num_of_near_rows << "\n";
        exit(0);
*/
    }

  // Define data structures
  protected:
    struct PageLoc // Physical location
    {
        unsigned rank_id = 0;
        unsigned part_id = 0;
        unsigned tile_id = 0;
        unsigned row_id = 0;
        unsigned col_id = 0;

        PageLoc& operator=(PageLoc other)
        {
            rank_id = other.rank_id;
            part_id = other.part_id;
            tile_id = other.tile_id;
            row_id = other.row_id;
            col_id = other.col_id;
            return *this;
        }

	bool operator==(const PageLoc &other) const
        {
            return rank_id == other.rank_id &&
                   part_id == other.part_id &&
                   tile_id == other.tile_id &&
                   row_id == other.row_id && 
                   col_id == other.col_id;
        }
    };

    // TODO, this hash structure is not used currently. I forgot why
    // I coded it.
    struct PageLocHashKey
    {
        template<typename T = PageLoc>
        std::size_t operator()(T &p) const
        {
            return std::hash<unsigned>()(p.rank_id) ^
                   std::hash<unsigned>()(p.part_id) ^
                   std::hash<unsigned>()(p.tile_id) ^
                   std::hash<unsigned>()(p.row_id) ^ 
                   std::hash<unsigned>()(p.col_id); 
        }
    };
    typedef std::unordered_map<PageLoc, bool, PageLocHashKey> PageLocHash;

    bool near_region_full = false;
    PageLoc cur_re_alloc_page; // For pages to be allocated in the near segment
    PageLoc cur_re_alloc_page_far_seg; // For pages to be allocated in the far segment

    enum INCREMENT_LEVEL: int
    {
        COL,ROW,TILE,PARTITION,RANK
    };
    virtual bool nextReAllocPage(PageLoc&, int);
    virtual bool nextReAllocPageFarSeg(PageLoc&, int);
};

// Strategy 1, bring MFU pages to the near rows.
class MFUPageToNearRows : public NearRegionAware
{
  public:
    MFUPageToNearRows(int num_of_cores, Config &cfg)
        : NearRegionAware(num_of_cores, cfg)
    { 
        srand(time(0));
        first_touch_instructions.resize(num_of_cores);
        fti_candidates.resize(num_of_cores);
    }

    void va2pa(Request &req) override;

    void phaseDone() override;

    void setInferenceStage() override 
    {
        inference_stage = true; 
        profiling_stage = false;

        std::vector<Page_Info> MFU_pages_profiling;

        for (auto [key, value] : pages)
        {
            MFU_pages_profiling.push_back(value);
        }

        std::sort(MFU_pages_profiling.begin(), MFU_pages_profiling.end(),
                  [](const Page_Info &a, const Page_Info &b)
                  {
                      return (a.num_of_reads + a.num_of_writes) >
                             (b.num_of_reads + b.num_of_writes);
                  });

        for (int i = 0; i < MFU_pages_profiling.size() * 0.7; i++)
        {
            // Re-alloc the pages to the fast-access region
            Addr page_id = MFU_pages_profiling[i].page_id;

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

            re_alloc_pages.insert({page_id, new_page_id});
        }

        pages.clear();
    }

  protected:
    void runtimeProfiling(Request&);
    void reAllocate(Request&);
    void randomMapping(Request&);

    // TODO, tmp hack, delete it soon.
    void halfWayMapping(Request&);
    void oraclePageProfiling(Request&);
    void oraclePageInference(Request&);

//    void profiling_new(Request&);

//    void inference(Request&);
//    void profiling(Request&);
    
    struct Page_Info
    {
        Addr page_id;
        Addr first_touch_instruction; // The first-touch instruction that brings in this page

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    std::unordered_map<Addr,Page_Info> pages; // All the touched (allocated) pages

    std::unordered_map<Addr,Addr> re_alloc_pages; // All the re-allocated MFU pages, used in
                                                  // inference stage.

    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> first_touch_instructions;
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> fti_candidates;

    int num_ftis_per_phase = 8;
};


class TrainedMMUFactory
{
    typedef Simulator::Config Config;

  private:
    std::unordered_map<std::string,
                       std::function<std::unique_ptr<TrainedMMU>(int,Config&)>> factories;

  public:
    TrainedMMUFactory()
    {
        factories["MFUPageToNearRows"] = [](int num_of_cores, Config &cfg)
                 {
                     return std::make_unique<MFUPageToNearRows>(num_of_cores, cfg);
                 };

        factories["Hybrid"] = [](int num_of_cores, Config &cfg)
                 {
                     return std::make_unique<Hybrid>(num_of_cores, cfg);
                 };
    }

    auto createMMU(int num_of_cores, Config &cfg)
    {
        std::string mmu_type = cfg.mmu_type;
        if (auto iter = factories.find(mmu_type);
            iter != factories.end())
        {
            return iter->second(num_of_cores, cfg);
        }
        else
        {
            std::cerr << "Unsupported MMU type. \n";
            exit(0);
        }
    }
};

static TrainedMMUFactory TrainedMMUFactories;
static auto createTrainedMMU(int num_of_cores, Simulator::Config &cfg)
{
    return TrainedMMUFactories.createMMU(num_of_cores, cfg);
}
}

#endif
