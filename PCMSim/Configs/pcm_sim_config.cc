#include "pcm_sim_config.hh"

namespace PCMSim
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
        if(tokens[0] == "num_of_word_lines_per_tile")
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
        else if(tokens[0] == "ns_bit_rd")
        {
            ns_bit_rd = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "ns_bit_set")
        {
            ns_bit_set = atoi(tokens[1].c_str());
        }
        else if(tokens[0] == "ns_bit_reset")
        {
            ns_bit_reset = atoi(tokens[1].c_str());
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

}
