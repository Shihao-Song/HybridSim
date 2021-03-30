#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <set>
#include <unordered_map>
#include <vector>

#include "Sim/config.hh"
#include "Sim/decoder.hh"
#include "Sim/mapper.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/stats.hh"
#include "Sim/trace.hh"

namespace System
{
class MMU
{
  protected:
    typedef Simulator::Config Config;
    typedef Simulator::Decoder Decoder;
    typedef Simulator::Mapper Mapper;
    typedef Simulator::MemObject MemObject;
    typedef Simulator::Request Request;

    typedef uint64_t Addr;

  protected:
    unsigned num_of_cores; // Number of cores in the system
    // When running the same application on multiple cores, you want to re-map the 
    // same virtual address to a different address for each core.
    std::vector<Mapper> mappers;

    MemObject *mem_system;

  public:
    MMU(int num_cores) : num_of_cores(num_cores)
    {
        for (int i = 0; i < num_cores; i++)
        {
            mappers.emplace_back(i);
        }
    }

    ~MMU(){}

    virtual void setPrefPatterns(std::vector<std::string> &_patterns,
                                 std::string &pattern_selection)
    {
        if (pattern_selection == "NONE")
        {
            std::cerr << "Warning: no prefetcher is installed. \n";
            return;
        }

        assert(_patterns.size() == num_of_cores);

        for (auto core_id = 0; core_id < num_of_cores; core_id++)
        {
            std::ifstream file(_patterns[core_id]);
            assert(file.good());

	    std::string line;
            while(getline(file, line))
            {
                std::stringstream line_stream(line);
                std::vector<std::string> tokens;
                std::string intermediate;
                while (getline(line_stream, intermediate, ' '))
                {
                    tokens.push_back(intermediate);
                }
                assert(tokens.size() > 0);

                Addr fti = std::stoull(tokens[0]);
                std::string pattern_str = "";
                if (pattern_selection == "AND")
                {
                    pattern_str = tokens[1];
                }
                else if (pattern_selection == "OR")
                {
                    pattern_str = tokens[2];
                }
                else if (pattern_selection == "MAX")
                {
                    pattern_str = tokens[3];
                }
                else
                {
                    std::cerr << "Unsupported pattern\n"; 
                    exit(0);
                }

                std::vector<unsigned> pos;
                for (int i = 0; i < pattern_str.length(); i++)
                {
                    if (pattern_str[i] == '1')
                    {
                        pos.push_back(1);
                    }
                    else if (pattern_str[i] == '0')
                    {
                        pos.push_back(0);
                    }
                }

                core_prefetchers.addPattern(core_id, fti, pos);
            }

            file.close();
        }
    }

    virtual void setCoreCaches(std::vector<MemObject*> &_caches)
    {
        core_caches = _caches;
    }

    virtual void setPrefNum(unsigned _num) 
    { 
        pref_num = _num; 
    }

    virtual void va2pa(Request &req)
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;
    }

    // Based on the PA, determine which memory node the page is. (For hybrid MMU)
    virtual int memoryNode(Request &req) { return -1; }

    // Register MMU statistics
    virtual void registerStats(Simulator::Stats &stats) {}

  protected:
    std::vector<MemObject*> core_caches;
    unsigned pref_num = 4;

    struct PrefPatterns
    {
        typedef std::unordered_map<Addr, std::vector<unsigned>> fti_to_pattern;
        std::vector<fti_to_pattern> patterns;

        PrefPatterns() {}

        void addPattern(unsigned core_id,
                        Addr fti,
                        std::vector<unsigned> &pos)
        {
            if (core_id == patterns.size())
            {
                fti_to_pattern pattern; patterns.push_back(pattern);
            }

            auto &core_pattern = patterns[core_id];
            assert(core_pattern.find(fti) == core_pattern.end());
            core_pattern.insert({fti, pos});

            // std::cout << core_id << " ";
            // std::cout << fti << " ";
            // for (auto p : pos)
            // {
            //      std::cout << p << " ";
            // }
            // std::cout << "\n";
        }

        /*
        void invoke(unsigned core_id,
                    Addr fti,
                    Addr page_id)
        {
            if (patterns.size() == 0) return;
            auto pattern_iter = patterns[core_id].find(fti);
            if (pattern_iter == patterns[core_id].end()) return;

            auto &page_pattern = pattern_iter->second;
            for (auto i = 0; i < page_pattern.size(); i++)
            {
                if (i == pref_num) break;

                // TODO, warning hard-coded cacheline size
                Addr prefetch_addr = (page_id << Mapper::va_page_shift)
                                   | (page_pattern[i] * 64);
                // std::cout << prefetch_addr << " ";
                core_caches[core_id]->fetchAddr(prefetch_addr);
            }
	    // std::cout << "done \n";
        }
        */
    };
    PrefPatterns core_prefetchers;
    
  protected:
    auto& getPatterns() { return core_prefetchers.patterns; }

  public:
    virtual void invokePrefetcher(Request &req) {}

  protected:
    // TODO, need to do more testings.
    // We need to know which bits affect page ID.
    struct PageIDHelper
    {
        PageIDHelper(Config &cfg) : mem_addr_decoding_bits(cfg.mem_addr_decoding_bits)
        {
            // Why we care about these?
            // Sometime, you want the page to reside in a specific row/bank/channel...
            channel_idx = int(Config::Decoding::Channel);
            rank_idx = int(Config::Decoding::Rank);
            bank_idx = int(Config::Decoding::Bank);
            part_idx = int(Config::Decoding::Partition);
            tile_idx = int(Config::Decoding::Tile);
            row_idx = int(Config::Decoding::Row);
            col_idx = int(Config::Decoding::Col);
        }

        // Decoding bits
        const std::vector<int> mem_addr_decoding_bits;
        
        unsigned channel_idx; // Index to mem_addr_decoding_bits
        unsigned rank_idx;
        unsigned bank_idx;
        unsigned part_idx;
        unsigned tile_idx;
        unsigned row_idx;
        unsigned col_idx;
    };
};
}

#endif
