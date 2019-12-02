#ifndef __SIM_CONFIG_HH__
#define __SIM_CONFIG_HH__

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string>
#include <vector>

namespace Simulator
{
class Config
{
  public:
    // TODO, should be vector since multi-core should be supported as well.
    std::string workload; // Name of the running workload

    // Processor configuration
    float on_chip_frequency;
    float off_chip_frequency;

    // Cache configuration
    unsigned block_size;

    enum class Cache_Level
    {
        L1I, L1D, L2, L3, eDRAM, MAX
    };

    struct Cache_Info
    {
        int assoc;
        unsigned size;
        bool write_only;
        unsigned num_mshrs;
        unsigned num_wb_entries;
        unsigned tag_lookup_latency;
    };
    std::array<Cache_Info, int(Cache_Level::MAX)> caches;
    void extractCacheInfo(Cache_Level level, std::vector<std::string> &tokens);

    // System-level (MMU)
    std::string mmu_type = "N/A";

    // Memory Controller
    std::string mem_controller_type = "N/A";

    // PCM Array Architecture
    // To mimic DRAM Architecture, num_of_tiles can set to 1 and treat partitions as
    // sub-arrays.
    unsigned num_of_word_lines_per_tile;
    unsigned num_of_bit_lines_per_tile;
    unsigned num_of_tiles;
    unsigned num_of_parts;

    unsigned num_of_banks;
    unsigned num_of_ranks;
    unsigned num_of_channels;

//  public:
    Config(const std::string &cfg_file);

    void parse(const std::string &fname);

    // Partition and below it don't have to be objects.
    enum class Array_Level : int
    {
        Channel, Rank, Bank, MAX
    };

    enum class Decoding : int
    {
        // Address mapping: bank-interleaving
        Rank, Partition, Tile, Row, Col, Bank, Channel, Cache_Line, MAX
    };
    std::vector<int> mem_addr_decoding_bits;
    void genMemAddrDecodingBits();

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

    // Supported memory node type
    enum class Memory_Node : int { DRAM, PCM, MAX };
};
}
#endif
