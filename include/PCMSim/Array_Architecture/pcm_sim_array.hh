#ifndef __PCMSIM_ARRAY_HH__
#define __PCMSIM_ARRAY_HH__

#include <list>
#include <cstdint>
#include <vector>

#include "Sim/config.hh"

namespace PCMSim
{
class Array
{
    typedef uint64_t Tick;
    typedef Simulator::Config Config;

  public:
    Array(typename Config::Array_Level level_val,
          Config &cfgs);
    ~Array();

    struct Info
    {
        // Array Architecture
	/*
        unsigned num_of_word_lines_per_tile;
        unsigned num_of_bit_lines_per_tile;
        unsigned num_of_tiles;
        unsigned num_of_parts;
        */
        unsigned block_size;

        // unsigned long long num_of_word_lines_per_bank;
        unsigned num_of_parts_per_bank;
        unsigned long long num_of_word_lines_per_part;
        unsigned long long num_of_byte_lines_per_bank;
        unsigned num_of_banks;
        unsigned num_of_ranks;
        unsigned num_of_channels;

        // Timing and energy parameters
        unsigned tRCD;
        unsigned tData;
        unsigned tWL;

        unsigned tWR;
        unsigned tCL;

        double pj_bit_rd;
        double pj_bit_set;
        double pj_bit_reset;
    };
    Info arr_info;

    typename Config::Array_Level level;
    int id;
    Array *parent;
    std::vector<Array *>children;

    // State information
    bool isFree(int target_rank, int target_bank)
    {
        if (children[target_rank]->children[target_bank]->next_free <= cur_clk &&
            children[target_rank]->next_free <= cur_clk &&
            next_free <= cur_clk)
        {
            return true;
	}
        else
        {
            return false;
        }
    }

    void update(Tick clk)
    { 
        cur_clk = clk; 
    
        for (auto child : children)
        {
            child->update(clk);
        }
    }

    void postAccess(int rank_id, int bank_id,
                    unsigned channel_latency,
                    unsigned rank_latency,
                    unsigned bank_latency)
    {
        // Add channel latency
        next_free = cur_clk + channel_latency;

        // Add bank latency
        children[rank_id]->children[bank_id]->next_free = cur_clk + bank_latency;

        // All other ranks won't be available until this rank is fully de-coupled.
        int num_of_ranks = arr_info.num_of_ranks;
        for (int i = 0; i < num_of_ranks; i++)
        {
            if (i == rank_id)
            {
                continue;
            }

            children[i]->next_free = cur_clk + rank_latency;
        }
    }

  private:
    Tick cur_clk;
    Tick next_free;

    // Helper functions
    void initArrInfo(Config &cfgs);
};
}

#endif

