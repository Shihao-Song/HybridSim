#include "PCMSim/Array_Architecture/pcm_sim_array.hh"

namespace PCMSim
{
Array::Array(typename Config::Array_Level level_val,
             Config &cfgs) : level(level_val), id(0)
{
    initArrInfo(cfgs);

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
        child_max = arr_info.num_of_ranks;
    }
    else if(level_val == Config::Array_Level::Rank)
    {
        child_max = arr_info.num_of_banks;
    }
    assert(child_max != -1);

    for (int i = 0; i < child_max; ++i)
    {
        Array* child = new Array(typename Config::Array_Level(child_level),
                                 cfgs);
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

void Array::initArrInfo(Config &cfgs)
{
    arr_info.block_size = cfgs.block_size;

//    arr_info.num_of_word_lines_per_bank = cfgs.num_of_word_lines_per_tile *
//                                          cfgs.num_of_parts;

    arr_info.num_of_parts_per_bank = cfgs.num_of_parts;
    arr_info.num_of_word_lines_per_part = cfgs.num_of_word_lines_per_tile;

    arr_info.num_of_byte_lines_per_bank = cfgs.num_of_bit_lines_per_tile /
                                          8 *
                                          cfgs.num_of_tiles;
    
    arr_info.num_of_banks = cfgs.num_of_banks; 
    arr_info.num_of_ranks = cfgs.num_of_ranks;
    arr_info.num_of_channels = cfgs.num_of_channels;

    arr_info.tRCD = cfgs.tRCD;
    arr_info.tData = cfgs.tData;
    arr_info.tWL = cfgs.tWL;

    arr_info.tWR = cfgs.tWR;
    arr_info.tCL = cfgs.tCL;

    arr_info.pj_bit_rd = cfgs.pj_bit_rd;
    arr_info.pj_bit_set = cfgs.pj_bit_set;
    arr_info.pj_bit_reset = cfgs.pj_bit_reset;
}
}
