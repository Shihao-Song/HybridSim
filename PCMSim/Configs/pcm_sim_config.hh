#ifndef __PCMSIM_CONFIG_HH__
#define __PCMSIM_CONFIG_HH__

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace PCMSim
{
class Config
{
  public:
    unsigned blkSize;

    // Array Architecture
    unsigned num_of_word_lines_per_tile;
    unsigned num_of_bit_lines_per_tile;
    unsigned num_of_tiles;
    unsigned num_of_parts;

    unsigned num_of_banks;
    unsigned num_of_ranks;
    unsigned num_of_channels;

    // Timing and energy parameters
    unsigned tRCD;
    unsigned tData;
    unsigned tWL;

    unsigned ns_bit_rd;
    unsigned ns_bit_set;
    unsigned ns_bit_reset;

    double pj_bit_rd;
    double pj_bit_set;
    double pj_bit_reset;
  
  public:
    Config(const std::string &cfg_file);

    void parse(const std::string &fname);

    // We ignore the effect of Tile and Partition for now.
    enum class Level : int
    {
        Channel, Rank, Bank, MAX
    };

    enum class Decoding : int
    {
        // Address mapping: bank-interleaving
        Channel, Rank, Row, Col, Bank, Cache_Line, MAX
    };

    unsigned sizeInGB()
    {
        unsigned long long num_of_word_lines_per_bank = num_of_word_lines_per_tile *
                                                        num_of_parts;
        
	unsigned long long num_of_byte_lines_per_bank = num_of_bit_lines_per_tile /
                                                        8 *
                                                        num_of_tiles;

        unsigned long long size = num_of_word_lines_per_bank *
                                  num_of_byte_lines_per_bank *
                                  num_of_banks * num_of_ranks * num_of_channels /
                                  1024 / 1024 / 1024;
        return size;
    }
};

}

#endif
