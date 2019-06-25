#ifndef __PCMSIM_ARRAY_HH__
#define __PCMSIM_ARRAY_HH__

#include <list>
#include <cstdint>
#include <vector>

#include "Configs/config.hh"

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
    // TODO, make this function like postAccess()
    bool isFree() { return next_free <= cur_clk; }

    void update(Tick clk)
    { 
        cur_clk = clk; 
    
        for (auto child : children)
        {
            child->update(clk);
        }
    }

    void postAccess(typename Config::Array_Level lev, int rank_id, int bank_id,
                     unsigned latency)
    {
        if (lev == Config::Array_Level::Channel)
        {
            next_free = cur_clk + latency;
        }
        else if (lev == Config::Array_Level::Rank)
        {
            children[rank_id]->next_free = cur_clk + latency;
        }
	else
	{
            children[rank_id]->children[bank_id]->next_free = cur_clk + latency;
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

