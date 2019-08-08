#include "Sim/config.hh"

namespace Simulator
{
Config::Config(const std::string &cfg_file)
{
    parse(cfg_file);

    // Generate memory address decoding bits
    genMemAddrDecodingBits();
}

void Config::genMemAddrDecodingBits()
{
    mem_addr_decoding_bits.resize(int(Decoding::MAX));

    mem_addr_decoding_bits[int(Decoding::Rank)] = int(log2(num_of_ranks));

    // I assume all the tiles lined up horizontally, the number of rows in a partition equals
    // to the number of rows in a tile.
    mem_addr_decoding_bits[int(Decoding::Row)] = int(log2(num_of_word_lines_per_tile));

    // Same as above, I assume all the tiles lined up horizontally.
    unsigned num_of_byte_lines_per_bank = num_of_bit_lines_per_tile /
                                          8 *
                                          num_of_tiles;
    mem_addr_decoding_bits[int(Decoding::Col)] =
        int(log2(num_of_byte_lines_per_bank / block_size));

    mem_addr_decoding_bits[int(Decoding::Partition)] = int(log2(num_of_parts));

    mem_addr_decoding_bits[int(Decoding::Bank)] = int(log2(num_of_banks));

    mem_addr_decoding_bits[int(Decoding::Channel)] = int(log2(num_of_channels));

    mem_addr_decoding_bits[int(Decoding::Cache_Line)] = int(log2(block_size));
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
        else if(tokens[0] == "cache_detailed")
        {
            cache_detailed = tokens[1] == "false" ? 0 : 1;
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
        else if(tokens[0] == "power_limit_enabled")
        {
            power_limit_enabled = tokens[1] == "false" ? 0 : 1;
        }
        else if(tokens[0] == "starv_free_enabled")
        {
            starv_free_enabled = tokens[1] == "false" ? 0 : 1;
        }
        else if(tokens[0] == "RAPL")
        {
            RAPL = atof(tokens[1].c_str());
        }
        else if(tokens[0] == "THB")
        {
            THB = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "THI")
        {
            THI = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "THA")
        {
            THA = atoi(tokens[1].c_str());
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
        else if(tokens[0] == "tRCD")
        {
            tRCD = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "tData")
        {
            tData = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "tWL")
        {
            tWL = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "tWR")
        {
            tWR = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "tCL")
        {
            tCL = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "pj_bit_rd")
        {
            pj_bit_rd = atof(tokens[1].c_str());
        }
        else if(tokens[0] == "pj_bit_set")
        {
            pj_bit_set = atof(tokens[1].c_str());
        }
        else if(tokens[0] == "pj_bit_reset")
        {
            pj_bit_reset = atof(tokens[1].c_str());
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

void Config::parseChargePumpInfo(const std::string &fname)
{
    std::ifstream stage_info(fname);
    assert(stage_info.good());

    std::string line;
    while(getline(stage_info,line))
    {
        std::stringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string intermidiate;
        while (getline(line_stream, intermidiate, ','))
        {
            tokens.push_back(intermidiate);
        }
        assert(tokens.size());

        charging_lookaside_buffer[int(Charge_Pump_Opr::SET)].emplace_back(
                                 Charging_Stage{std::stof(tokens[0]),
                                                std::stoul(tokens[1])});

        charging_lookaside_buffer[int(Charge_Pump_Opr::RESET)].emplace_back(
                                 Charging_Stage{std::stof(tokens[2]),
                                                std::stoul(tokens[3])});

        charging_lookaside_buffer[int(Charge_Pump_Opr::READ)].emplace_back(
                                 Charging_Stage{std::stof(tokens[4]),
                                                std::stoul(tokens[5])});
        ++num_stages;
    }

    stage_info.close();
}

}
