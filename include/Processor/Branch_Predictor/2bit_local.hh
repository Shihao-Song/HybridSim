#ifndef __TWO_BIT_LOCAL_HH__
#define __TWO_BIT_LOCAL_HH__

#include "Processor/Branch_Predictor/branch_predictor.hh"

#include <vector>

namespace CoreSystem
{
class Two_Bit_Local : public Branch_Predictor
{
  protected:
    // Followings are from GEM5
    // const unsigned localPredictorSize = 2048;
    const unsigned localPredictorSize = 1;
    const unsigned localCounterBits = 2;

    unsigned index_mask;
    std::vector<Sat_Counter> local_counters;

  public:
    Two_Bit_Local()
        : local_counters(localPredictorSize, Sat_Counter(localCounterBits))
        , index_mask(localPredictorSize - 1)
    {}

    bool predict(Instruction &instr) override
    {
        assert(instr.opr == Instruction::Operation::BRANCH);

        // (1) Get prediction
        unsigned local_predictor_idx = getLocalIndex(instr.eip);

        uint8_t counter_val = local_counters[local_predictor_idx];

        bool pred = getPrediction(counter_val);

        // (2) Update
        if (instr.taken)
        {
            local_counters[local_predictor_idx]++;
        }
        else
        {
            local_counters[local_predictor_idx]--;
        }

        std::cout << local_predictor_idx << " -> "
                  << int(counter_val) << " : " 
                  << int(local_counters[local_predictor_idx]) << "\n";
        // (3) Return back the correctness of the prediction
        return instr.taken == pred;
    }

  protected:
    inline unsigned getLocalIndex(Addr &branch_addr)
    {
        return (branch_addr >> instShiftAmt) & index_mask;
    }

    inline bool getPrediction(uint8_t &count)
    {
        return (count >> (localCounterBits - 1));
    }
};
}

#endif
