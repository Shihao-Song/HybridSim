#include "config.hh"

namespace Configuration
{
Config::Config(const std::string &cfg_file)
{
    parse(cfg_file);
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
            blkSize = atoi(tokens[1].c_str());
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
        else if(tokens[0] == "mem_controller_family")
        {
            mem_controller_family = tokens[1];
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
        else if(tokens[0] == "block_size")
        {
            blkSize = atoi(tokens[1].c_str());
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

}
