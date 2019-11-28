#include "Sim/config.hh"

namespace Simulator
{
Config::Config(const std::string &cfg_file)
{
    parse(cfg_file);

    if (mmu_type == "Hybrid")
    {
        // TODO, memory controller should not only be Hybrid
        assert(mem_controller_type == "Hybrid");
    }

    // Generate memory address decoding bits
    genMemAddrDecodingBits();
}

void Config::genMemAddrDecodingBits()
{
    mem_addr_decoding_bits.resize(int(Decoding::MAX));

    mem_addr_decoding_bits[int(Decoding::Rank)] = int(log2(num_of_ranks));
//    std::cout << "Number of Ranks: " << num_of_ranks << "\n";
//    std::cout << "Rank bits: " << mem_addr_decoding_bits[int(Decoding::Rank)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Partition)] = int(log2(num_of_parts));
//    std::cout << "Number of Partitions: " << num_of_parts << "\n";
//    std::cout << "Partition bits: " 
//              << mem_addr_decoding_bits[int(Decoding::Partition)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Tile)] = int(log2(num_of_tiles));
//    std::cout << "Number of Tiles: " << num_of_tiles << "\n";
//    std::cout << "Tile bits: " << mem_addr_decoding_bits[int(Decoding::Tile)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Row)] = int(log2(num_of_word_lines_per_tile));
//    std::cout << "Number of Rows: " << num_of_word_lines_per_tile << "\n";
//    std::cout << "Row bits: " << mem_addr_decoding_bits[int(Decoding::Row)] << "\n";
    
    mem_addr_decoding_bits[int(Decoding::Col)] = 
        int(log2(num_of_bit_lines_per_tile / 8 / block_size));
//    std::cout << "Number of cache lines per Tile: " 
//              << num_of_bit_lines_per_tile / 8 / block_size << "\n";
//    std::cout << "Col bits: " << mem_addr_decoding_bits[int(Decoding::Col)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Bank)] = int(log2(num_of_banks));
//    std::cout << "Number of Banks: " << num_of_banks << "\n";
//    std::cout << "Bank bits: " << mem_addr_decoding_bits[int(Decoding::Bank)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Channel)] = int(log2(num_of_channels));
//    std::cout << "Number of Channels: " << num_of_channels << "\n";
//    std::cout << "Channel bits: " << mem_addr_decoding_bits[int(Decoding::Channel)] << "\n";

    mem_addr_decoding_bits[int(Decoding::Cache_Line)] = int(log2(block_size));
//    std::cout << "Block Size: " << block_size << "\n";
//    std::cout << "Cache bits: " << mem_addr_decoding_bits[int(Decoding::Cache_Line)] << "\n";
}

void Config::parse(const std::string &fname)
{
    std::ifstream file(fname);
    assert(file.good());

    std::string line;
    while(getline(file, line))
    {
        char delim[] = " \t=";
        std::vector<std::string> tokens;

        while (true)
        {
            size_t start = line.find_first_not_of(delim);
            if (start == std::string::npos)
            { 
                break;
            }

            size_t end = line.find_first_of(delim, start);
            if (end == std::string::npos)
            {
                tokens.push_back(line.substr(start));
                break;
            }

            tokens.push_back(line.substr(start, end - start));
            line = line.substr(end);
        }

        // empty line
        if (!tokens.size())
        {
            continue;
        }

        // comment line
        if (tokens[0][0] == '#')
        {
            continue;
        }

        // parameter line
        assert(tokens.size() == 2 && "Only allow two tokens in one line");

        // Extract Timing Parameters
        if(tokens[0] == "on_chip_frequency")
        {
            on_chip_frequency = atof(tokens[1].c_str());
        }
        else if(tokens[0] == "off_chip_frequency")
        {
            off_chip_frequency = atof(tokens[1].c_str());
        }
        else if(tokens[0] == "block_size")
        {
            block_size = atoi(tokens[1].c_str());
        }
        else if(tokens[0].find("L1I") != std::string::npos)
        {
            extractCacheInfo(Cache_Level::L1I, tokens);
        }
        else if(tokens[0].find("L1D") != std::string::npos)
        {
            extractCacheInfo(Cache_Level::L1D, tokens);
        }
        else if(tokens[0].find("L2") != std::string::npos)
        {
            extractCacheInfo(Cache_Level::L2, tokens);
        }
        else if(tokens[0].find("L3") != std::string::npos)
        {
            extractCacheInfo(Cache_Level::L3, tokens);
        }
        else if(tokens[0].find("eDRAM") != std::string::npos)
        {
            extractCacheInfo(Cache_Level::eDRAM, tokens);
        }
        else if(tokens[0] == "mmu_type")
        {
            mmu_type = tokens[1];
        }
        else if(tokens[0] == "mem_controller_type")
        {
            mem_controller_type = tokens[1];
        }
	else if(tokens[0] == "num_of_word_lines_per_tile")
        {
            num_of_word_lines_per_tile = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_bit_lines_per_tile")
        {
            num_of_bit_lines_per_tile = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_tiles")
        {
            num_of_tiles = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_parts")
        {
            num_of_parts = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_banks")
        {
            num_of_banks = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_ranks")
        {
            num_of_ranks = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "num_of_channels")
        {
            num_of_channels = atoi(tokens[1].c_str());
        }
    }

    file.close();
}

void Config::extractCacheInfo(Cache_Level level, std::vector<std::string> &tokens)
{
    if(tokens[0].find("assoc") != std::string::npos)
    {
        caches[int(level)].assoc = atoi(tokens[1].c_str());
    }
    else if(tokens[0].find("size") != std::string::npos)
    {
        caches[int(level)].size = atoi(tokens[1].c_str());
    }
    else if(tokens[0].find("write_only") != std::string::npos)
    {
        caches[int(level)].write_only = tokens[1] == "false" ? 0 : 1;
    }
    else if(tokens[0].find("num_mshrs") != std::string::npos)
    {
        caches[int(level)].num_mshrs = atoi(tokens[1].c_str());
    }
    else if(tokens[0].find("num_wb_entries") != std::string::npos)
    {
        caches[int(level)].num_wb_entries = atoi(tokens[1].c_str());
    }
    else if(tokens[0].find("tag_lookup_latency") != std::string::npos)
    {
	caches[int(level)].tag_lookup_latency = atoi(tokens[1].c_str());
    }
}
}
