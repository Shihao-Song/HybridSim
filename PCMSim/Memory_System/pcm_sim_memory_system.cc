#include "pcm_sim_memory_system.hh"

#include "../Array_Architecture/pcm_sim_array.hh"
#include "../Configs/pcm_sim_config.hh"
#include "../Controller/pcm_sim_controller.hh"

namespace PCMSim
{
PCMSimMemorySystem::PCMSimMemorySystem(const std::string cfg_file)
{
    Config cfgs(cfg_file);

    init(cfgs);
    blkSize = cfgs.blkSize;
    std::cout << "Number of Channels: " << cfgs.num_of_channels << "\n";
    std::cout << "Number of Ranks: " << cfgs.num_of_ranks << "\n";
    std::cout << "Number of Banks: " << cfgs.num_of_banks << "\n";
    std::cout << "Cache line size: " 
              << (controllers[0]->channel->arr_info).blkSize << "\n\n";
}

PCMSimMemorySystem::~PCMSimMemorySystem()
{
    for (auto &c : controllers)
    {
        delete c;
    }
}

void PCMSimMemorySystem::init(Config &cfgs)
{
    for (int i = 0; i < cfgs.num_of_channels; i++)
    {
        Array *channel = new Array(Config::Level::Channel,
                                   cfgs, nclks_per_ns);
        assert(channel != nullptr);
	channel->id = i;

        Controller *controller = new Controller(channel);
        assert(controller != nullptr);
        controllers.push_back(controller);
    }

    addr_bits.resize(int(Config::Decoding::MAX));
    addr_bits[int(Config::Decoding::Channel)] =
        int(log2((controllers[0]->channel->arr_info).num_of_channels));

    addr_bits[int(Config::Decoding::Rank)] =
        int(log2((controllers[0]->channel->arr_info).num_of_ranks));

    addr_bits[int(Config::Decoding::Row)] =
        int(log2((controllers[0]->channel->arr_info).num_of_word_lines_per_bank));

    addr_bits[int(Config::Decoding::Col)] =
        int(log2((controllers[0]->channel->arr_info).num_of_byte_lines_per_bank /
                  (controllers[0]->channel->arr_info).blkSize));

    addr_bits[int(Config::Decoding::Bank)] =
        int(log2((controllers[0]->channel->arr_info).num_of_banks));

    addr_bits[int(Config::Decoding::Cache_Line)] =
        int(log2((controllers[0]->channel->arr_info).blkSize));
}

void PCMSimMemorySystem::decode(Addr _addr, std::vector<int> &vec)
{
    Addr addr = _addr;
    for (int i = addr_bits.size() - 1; i >= 0; i--)
    {
        vec[i] = sliceLowerBits(addr, addr_bits[i]);
    }
}

bool PCMSimMemorySystem::send(Request &req)
{
    req.addr_vec.resize(int(Config::Decoding::MAX));
    
    decode(req.addr, req.addr_vec);

    int channel_id = req.addr_vec[int(Config::Decoding::Channel)];
    
    if(controllers[channel_id]->enqueue(req))
    {
        return true;
    }

    return false;
}

void PCMSimMemorySystem::tick()
{
    for (auto controller : controllers)
    {
        controller->tick();
    }
}

int PCMSimMemorySystem::pendingRequests()
{
    int outstandings = 0;

    for (auto controller : controllers)
    {
        outstandings += controller->getQueueSize();
    }

    return outstandings;
}

int PCMSimMemorySystem::sliceLowerBits(Addr& addr, int bits)
{
    int lbits = addr & ((1<<bits) - 1);
    addr >>= bits;
    return lbits;
}
}
