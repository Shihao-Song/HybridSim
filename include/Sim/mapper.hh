#ifndef __MAPPER_HH__
#define __MAPPER_HH__

#include "Sim/random.hh"

namespace Simulator
{
class Mapper
{
  protected:
    int core_id;

    uint8_t m_address_randomization_table[256];

  public:
    static const uint64_t pa_core_shift = 48;
    static const uint64_t pa_core_size = 16;
    static const uint64_t pa_va_mask = ~(((uint64_t(1) << pa_core_size) - 1) << pa_core_shift);

    static const unsigned page_size = 4096; // 4kB    
    static const uint64_t va_page_shift = 12;
    static const uint64_t va_page_mask = (uint64_t(1) << va_page_shift) - 1;

  public:
    Mapper(int _core_id) : core_id(_core_id)
    {
        // Using core_id as a seed
        uint64_t state = rng_seed(core_id);
        m_address_randomization_table[0] = 0;
        for(unsigned int i = 1; i < 256; ++i)
        {
            uint8_t j = rng_next(state) % (i + 1);
            m_address_randomization_table[i] = m_address_randomization_table[j];
            m_address_randomization_table[j] = i;
        }
    }

    uint64_t va2pa(uint64_t va)
    {
        uint64_t va_page = va >> va_page_shift;

        // Randomly remapping the lower 32 bits of va_page.
        uint8_t *array = (uint8_t *)&va_page;
        array[0] = m_address_randomization_table[array[0]];
        array[1] = m_address_randomization_table[array[1]];
        array[2] = m_address_randomization_table[array[2]];
        array[3] = m_address_randomization_table[array[3]];

        // Re-organize
        uint64_t pa = (va_page << va_page_shift) |
                      (va & va_page_mask);

        return pa;
    }
};
}

#endif
