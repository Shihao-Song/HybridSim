#ifndef __HYBRID_HH__
#define __HYBRID_HH__

#include "System/mmu.hh"

namespace System
{
class Hybrid : public TrainedMMU
{
  protected:
    // Data structure for page information
    struct Page_Info
    {
        Addr page_id;
        Addr first_touch_instruction; // The first-touch instruction that brings in this page
        Addr re_alloc_page_id; // A page may be re-allocated to a different location (see below)

        // Record physical location of a page. So far, a page may be in:
        //     (1) The fast-access region of PCM (pcm_near)
        //     (2) The slow-access region of PCM (pcm_far)
        //     (3) The fast-access region of DRAM (dram_near)
        //     (4) The slow-access region of DRAM (dram_far)
        bool in_pcm_near = false;
        bool in_pcm_far = false;
        bool in_dram_near = false;
        bool in_dram_far = false;

        uint64_t num_of_reads = 0; // Number of reads to the page
        uint64_t num_of_writes = 0; // Number of writes to the page

        // Number of phases the page hasn't been touched.
        unsigned num_of_phases_silent = 0;
    };
    // All the touched pages for each core.
    std::vector<std::unordered_map<Addr,Page_Info>> pages;

    // Data structure for first-touch instruction (FTI)
    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;

        // A FTI can allocate a page to the following location.
        bool in_pcm_near = false;
        bool in_pcm_far = false;
        bool in_dram_near = false;
        bool in_dram_far = false;

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    // All the FTIs for each core
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> first_touch_instructions;

    // Data structure for page migration (single page migration)
    struct Mig_Page
    {
        Addr page_id;

        bool done = false; // Is migration process done?

        bool pcm_far_to_pcm_near = false; // From far segment to near segment (PCM)
        bool pcm_near_to_pcm_far = false; // From near segment to far segment (PCM)

        bool pcm_near_to_dram_far = false; // From PCM near segment to DRAM far segment
        bool dram_far_to_pcm_near = false; // From DRAM far segment to PCM near segment

        bool dram_far_to_dram_near = false; // From far segment to near segment (DRAM)
        bool dram_near_to_dram_far = false; // From near segment to far segment (DRAM)

        // The following two migration type should also be supported in case of incorrect
        // intial allocation for top hot pages.
        bool pcm_far_to_dram_near = false; // From PCM far segment to DRAM near segment
        bool pcm_near_to_dram_near = false; // From PCM near segment to DRAM near segment

        // When page migration happens, a page should be read from the original node/segment 
        // then written to the target node/segment
        unsigned num_mig_reads_left;
        unsigned num_mig_writes_left;

        Addr ori_page_id;
        Addr target_page_id;
    };
    std::vector<Mig_Page> pages_to_migrate;

  protected:
    // Keep track of Array Architecture of DRAM and PCM
    struct array
    {
        unsigned num_of_channels;
        unsigned num_of_ranks; // per channel
        unsigned num_of_banks; // per rank
        unsigned num_of_partitions; // per bank
        unsigned num_of_tiles; // per partition
        unsigned num_of_rows; // per tile
        unsigned num_of_blocks; // per row

        const std::vector<int> mem_addr_decoding_bits;
    };

  public:
    Hybrid(int num_of_cores, Config &cfg)
        : TrainedMMU(num_of_cores, cfg)
    {
        pages.resize(num_of_cores);
        first_touch_instructions.resize(num_of_cores);

        srand(time(NULL));
    }
};
}

#endif
