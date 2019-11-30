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

    virtual void va2pa(Request &req)
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;
    }

    // The MMU should be able to do page migration among different memory nodes
    virtual bool pageMig() {}

    // Our simulator can run the program in multiple phases
    virtual void phaseDone() {}

    // MMU should have acess to the memory system to perform page migration
    void setMemSystem(MemObject *_sys) { mem_system = _sys; }

    // Register MMU statistics
    virtual void registerStats(Simulator::Stats &stats) {}

  protected:
    unsigned num_of_phases = 0;

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
