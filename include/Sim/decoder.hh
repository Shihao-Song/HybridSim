#ifndef __DECODER_HH__
#define __DECODER_HH__

#include <vector>

namespace Simulator
{
class Decoder
{
  public:
    Decoder(){}

    static void decode(uint64_t _addr,
                const std::vector<int> &decoding_bits,
                std::vector<int> &result_addr_vec)
    {
        assert(decoding_bits.size());
        assert(result_addr_vec.size());
        assert(decoding_bits.size() == result_addr_vec.size());

        uint64_t addr = _addr;
        for (int i = decoding_bits.size() - 1; i >= 0; i--)
        {
            result_addr_vec[i] = sliceLowerBits(addr, decoding_bits[i]);
        }
    }

    static int sliceLowerBits(uint64_t& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
};
}

#endif
