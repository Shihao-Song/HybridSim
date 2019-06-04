#ifndef __PCMSIM_ARRAY_HH__
#define __PCMSIM_ARRAY_HH__

#include <list>
#include <vector>

#include "../request.hh"
#include "../../Configs/config.hh"

namespace PCMSim
{
class Array
{
    typedef Configuration::Config Config;

  public:
    Array(typename Config::Level level_val,
          Config &cfgs, float nclks_per_ns);
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
        unsigned blkSize;

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

    typename Config::Level level;
    int id;
    Array *parent;
    std::vector<Array *>children;

    unsigned write(std::list<Request>::iterator &req);
    unsigned read(std::list<Request>::iterator &req);

    // State information
    bool isFree() { return next_free <= cur_clk; }

    void update(Tick clk)
    { 
        cur_clk = clk; 
    
        for (auto child : children)
        {
            child->update(clk);
        }
    }

    void postAccess(typename Config::Level lev, int rank_id, int bank_id,
                     unsigned latency)
    {
        if (lev == Config::Level::Channel)
        {
            next_free = cur_clk + latency;
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
    void initArrInfo(Config &cfgs, float nclks_per_ns);
};
}

#endif

