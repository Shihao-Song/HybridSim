#ifndef __BRANCH_PREDICTOR_HH__
#define __BRANCH_PREDICTOR_HH__

#include "Sim/instruction.hh"

namespace CoreSystem
{
class Branch_Predictor
{
  protected:
    uint64_t correct_preds = 0;
    uint64_t incorrect_preds = 0;

  public:
    Branch_Predictor() {}

    typedef Simulator::Instruction Instruction;
    virtual bool predict(Instruction &instr) = 0;

    uint64_t getCorPreds() { return correct_preds; }
    uint64_t getInCorPreds() { return incorrect_preds; }

  protected:
    typedef uint64_t Addr;

    struct Sat_Counter
    {
        Sat_Counter(unsigned _bits)
            : counter_bits(_bits)
            , max_val((1 << counter_bits) - 1)
            , counter(0)
        {}
       
        // Read counter value
        operator uint8_t() const { return counter; }

        // Pre-increment
        Sat_Counter&
        operator++()
        {
            if (counter < max_val)
            {
                ++counter;
            }
            return *this;
        }

        // Post-increment
        Sat_Counter
        operator++(int)
        {
            Sat_Counter old_counter = *this;
            ++*this;
            return old_counter;
        }

        // Pre-decrement
        Sat_Counter&
        operator--()
        {
            if (counter > 0)
            {
                --counter;
            }
            return *this;
        }

        // Post-decrement
        Sat_Counter
        operator--(int)
        {
            Sat_Counter old_counter = *this;
            --*this;
            return old_counter;
        }

        unsigned counter_bits;
        uint8_t max_val;
        uint8_t counter;
    };

    const unsigned instShiftAmt = 2; // Number of bits to shift a PC by (from GEM5)

};
}

#endif
