#ifndef __PCMSIM_ARRAY_HH__
#define __PCMSIM_ARRAY_HH__

#include <cstdint>
#include <list>
#include <memory>
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
          Config &cfg) : level(level_val), id(0)
    {
        initArrInfo(cfg);

        cur_clk = 0;
        next_free = 0;

        int child_level = int(level_val) + 1;

        // Stop at bank level
        if (level == Config::Array_Level::Bank)
        {
            return;
        }
        assert(level != Config::Array_Level::Bank);

        int child_max = -1;

        if(level_val == Config::Array_Level::Channel)
        {
            child_max = cfg.num_of_ranks;
        }
        else if(level_val == Config::Array_Level::Rank)
        {
            child_max = cfg.num_of_banks;
        }
        assert(child_max != -1);

        for (int i = 0; i < child_max; ++i)
        {
            std::unique_ptr<Array> child =
            std::make_unique< Array>(typename Config::Array_Level(child_level), cfg);

            child->parent = this;
            child->id = i;
            children.push_back(std::move(child));
        }
    }

    void reInitialize()
    {
        cur_clk = 0;
        next_free = 0;

        for (auto &child : children)
        {
            child->reInitialize();
        }
    }

    struct Info
    {
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
    unsigned singleReadLatency() const
    {
        return arr_info.tRCD + arr_info.tCL + arr_info.tData;
    }
    unsigned bankDelayCausedBySingleRead() const
    {
        return arr_info.tRCD + arr_info.tCL;
    }
    unsigned singleWriteLatency() const
    {
        return arr_info.tRCD + arr_info.tData +
               arr_info.tWL + arr_info.tWR;
    }
    unsigned bankDelayCausedBySingleWrite() const
    {
        return singleWriteLatency();
    }
    unsigned activationLatency() const
    {
        return arr_info.tRCD;
    }
    unsigned dataTransferLatency() const
    {
        return arr_info.tData;
    }
    // For our PLP technique
    unsigned readWithReadLatency() const
    {
        return arr_info.tRCD +
               arr_info.tRCD +
               arr_info.tRCD +
               arr_info.tCL +
               arr_info.tData +
               arr_info.tRCD +
               arr_info.tData;
    }
    unsigned readWhileWriteLatency() const
    {
        return singleWriteLatency() + arr_info.tRCD;
    }
    double powerPerBitRead() const
    {
        return arr_info.pj_bit_rd /
               singleReadLatency();
    }
    double powerPerBitWrite() const
    {
        return (arr_info.pj_bit_set +
                arr_info.pj_bit_reset) /
                2.0 / singleWriteLatency();
    }

    typename Config::Array_Level level;
    int id;
    Array *parent;
    std::vector<std::unique_ptr<Array>>children;

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
    
        for (auto &child : children)
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
        int num_of_ranks = children.size();
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
    void initArrInfo(Config &cfg)
    {
        arr_info.tRCD = cfg.tRCD;
        arr_info.tData = cfg.tData;
        arr_info.tWL = cfg.tWL;

        arr_info.tWR = cfg.tWR;
        arr_info.tCL = cfg.tCL;

        arr_info.pj_bit_rd = cfg.pj_bit_rd;
        arr_info.pj_bit_set = cfg.pj_bit_set;
        arr_info.pj_bit_reset = cfg.pj_bit_reset;
    }
};
}

#endif

