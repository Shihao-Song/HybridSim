#include "pcm_sim_array.hh"

namespace PCMSim
{
Array::Array(typename Config::Level level_val,
             Config &cfgs, float nclks_per_ns) : level(level_val), id(0)
{
    initArrInfo(cfgs, nclks_per_ns);

    cur_clk = 0;
    next_free = 0;

    int child_level = int(level_val) + 1;
    
    // Stop at bank level
    if (level == Config::Level::Bank)
    {
        return;
    }
    assert(level != Config::Level::Bank);

    int child_max = -1;

    if(level_val == Config::Level::Channel)
    {
        child_max = arr_info.num_of_ranks;
    }
    else if(level_val == Config::Level::Rank)
    {
        child_max = arr_info.num_of_banks;
    }
    assert(child_max != -1);

    for (int i = 0; i < child_max; ++i)
    {
        Array* child = new Array(typename Config::Level(child_level),
                                 cfgs, nclks_per_ns);
        child->parent = this;
        child->id = i;
        children.push_back(child);
    }
}

Array::~Array()
{
    for (auto child: children)
    {
        delete child;
    }
}

void Array::initArrInfo(Config &cfgs, float nclks_per_ns)
{
    arr_info.blkSize = cfgs.blkSize;

    arr_info.num_of_word_lines_per_bank = cfgs.num_of_word_lines_per_tile *
                                          cfgs.num_of_parts;

    arr_info.num_of_byte_lines_per_bank = cfgs.num_of_bit_lines_per_tile /
                                          8 *
                                          cfgs.num_of_tiles;
    
    arr_info.num_of_banks = cfgs.num_of_banks; 
    arr_info.num_of_ranks = cfgs.num_of_ranks;
    arr_info.num_of_channels = cfgs.num_of_channels;

    arr_info.tRCD = cfgs.tRCD;
    arr_info.tData = cfgs.tData;
    arr_info.tWL = cfgs.tWL;

    arr_info.nclks_bit_rd = cfgs.ns_bit_rd * nclks_per_ns;
    arr_info.nclks_bit_set = cfgs.ns_bit_set * nclks_per_ns;
    arr_info.nclks_bit_reset = cfgs.ns_bit_reset * nclks_per_ns;

    arr_info.pj_bit_rd = cfgs.pj_bit_rd;
    arr_info.pj_bit_set = cfgs.pj_bit_set;
    arr_info.pj_bit_reset = cfgs.pj_bit_reset;
}

// @ECEC-623, please re-code these two functions.
unsigned Array::write(std::list<Request>::iterator &req)
{
    unsigned lat = arr_info.tRCD + arr_info.tData +
                   arr_info.tWL + arr_info.nclks_bit_set +
                   arr_info.nclks_bit_reset;

    return lat;
}

unsigned Array::read(std::list<Request>::iterator &req)
{
    unsigned lat = arr_info.tRCD + arr_info.tData +
                   arr_info.nclks_bit_rd;

    return lat;
}
}
