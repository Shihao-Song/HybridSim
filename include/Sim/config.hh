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

    Config(const std::string &cfg_file);

    void parse(const std::string &fname);

    // Partition and below it don't have to be objects.
    enum class Array_Level : int
    {
        Channel, Rank, Bank, MAX
    };

    enum class Decoding : int
    {
        // TODO, Partition and Tile also have some effect, please do more research on how to
        // better introduce Partition and Tile into address mapping. Or maybe some architectural
        // techniques.
        // Address mapping: bank-interleaving
        Partition, Tile, Row, Col, Rank, Bank, Channel, Cache_Line, MAX

        // The tile is defined as the smallest array unit comprising of PCM cells.
        // Each tile requires its own decoding structures, an n-well ring, dummy elements at 
        // the array borders, and possibly a local amplifer to enhance the cell signal before 
        // sending it to a global sense amplifier.

        // A partition (a group of tiles) is the portion of the array that will change its bias 
        // when switching operations. For example, a given partition will change its bias when 
        // prepared for the write operation, while all the remaining cells will stay in read 
        // bias.
        // A partition has its own dedicated peripheral circuits, like switches, 
        // voltage regulators, fixed borders between adjacent partitions.

        // Why I don't consider row-buffer hits.
        // In DRAM, you can read the entire row into the row buffer, thus priorizing row buffer 
        // hits makes sense. But in NVM, e.g. PCM, I don't think you want to read the entire
        // row, or even possible to read the entire row. There are couple of reasons I can
        // think of (1): technology wise: reading a single NVM cell requires much energy;
        // (2) technology wise: reading a single NVM cell needs to forward-bias the diode 
        // itself, while all the other diodes corresponding to unselected cells must be 
        // either reverse biased or biased at a voltage that is well below a certain threshold
        // ; this is to guarantee that the currect flowing in the de-selected cell is 
        // not causing any read or program disturb on the de-selected cell itself and 
        // not inducing a wrong sensing of the selected cell because of the excessive
        // leakage. As we can see, in NVM, limiting the read granuality has a potitive effect
        // on energy consumption. (3) In more recent times, multi-core workload 
        // characterizations have shown that the row buffer locality is limited and 
        // the overfetch to row buffers is mostly wasted energy.
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
