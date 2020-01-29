#ifndef __TOURNAMENT_HH__
#define __TOURNAMENT_HH__

#include "Processor/Branch_Predictor/2bit_local.hh"

#include <unordered_map>

namespace CoreSystem
{
class Tournament : public Two_Bit_Local
{
  protected:
    const unsigned localHistoryTableSize = 2048;

    const unsigned globalPredictorSize = 8192;
    const unsigned globalCounterBits = 2;
    const unsigned choicePredictorSize = 8192; // Is choicePredictorSize always equal to 
                                               // globalPredictorSize?
    const unsigned choiceCounterBits = 2;

    std::vector<unsigned> local_history_table;

    std::vector<Sat_Counter> global_counters;

    std::vector<Sat_Counter> choice_counters;

    unsigned history_register_mask; // The number of bits used in one global history register
    std::vector<unsigned> history_registers;
 
    // Thresholds
    const unsigned local_threshold;
    const unsigned global_threshold;
    const unsigned choice_threshold;

  public:
    Tournament()
        : global_counters(globalPredictorSize, Sat_Counter(globalCounterBits))
        , choice_counters(choicePredictorSize, Sat_Counter(choiceCounterBits))
        , local_threshold((unsigned(1) << (localCounterBits - 1)) - 1)
        , global_threshold((unsigned(1) << (globalCounterBits - 1)) - 1)
        , choice_threshold((unsigned(1) << (choiceCounterBits - 1)) - 1)
    {
        assert(globalPredictorSize == choicePredictorSize);

        // Initialize local history table
        local_history_table.resize(localHistoryTableSize);
        for (int i = 0; i < localHistoryTableSize; i++)
        {
            local_history_table[i] = 0;
        }

        // Initialize history register mask
        history_register_mask = globalPredictorSize - 1;

        history_registers.resize(1, 0);
    }

    bool predict(Instruction &instr) override
    {
        // Step one, we maintain the content of the history register for each thread,
        // make sure the thread's history register is present.
        unsigned t_id = 0;

        // Step two, get local prediction.
        unsigned local_hist_tab_idx = calcLocHistIdx(instr.eip);
        // std::cout << "Local history table index: " << local_hist_tab_idx << "\n";
        unsigned local_counters_idx = local_history_table[local_hist_tab_idx] & 
                                  (localPredictorSize - 1);
        // std::cout << "Local history: "  << local_history_table[local_hist_tab_idx] << "\n";
        // std::cout << "Local counters index: " << local_counters_idx << "\n";
        bool local_prediction = local_counters[local_counters_idx] > local_threshold;
        // std::cout << "Local counter value: " 
        //           << int(local_counters[local_counters_idx]) << "\n";
        // std::cout << "Local prediction: " << local_prediction << "\n";

        // Step three, get global prediction.
        unsigned global_counters_idx = history_registers[t_id] & 
                                       (globalPredictorSize - 1);
        // std::cout << "Global history: " << history_registers[t_id] << "\n";
        // std::cout << "Global counters index: " << global_counters_idx << "\n";
        bool global_prediction = global_counters[global_counters_idx] > global_threshold;
        // std::cout << "Global counter value: " 
        //           << int(global_counters[global_counters_idx]) << "\n";
        // std::cout << "Global prediction: " << global_prediction << "\n";

        // Step four, get choice prediction.
        // 1 -> pick global; 0 -> pick local.
        unsigned choice_counters_idx = history_registers[t_id] &
                                       (choicePredictorSize - 1);
        assert(global_counters_idx == choice_counters_idx);
        bool choice_prediction = choice_counters[choice_counters_idx] > choice_threshold;
        // std::cout << "Choice counter value: " 
        //           << int(choice_counters[choice_counters_idx]) << "\n";
        // std::cout << "Choice prediction: " << choice_prediction << "\n";

        // Step five, get final prediction
        bool final_prediction;
        if (choice_prediction)
        {
            final_prediction = global_prediction;
            // std::cout << "Global is selected. \n";
        }
        else
        {
            final_prediction = local_prediction;
            // std::cout << "Local is selected. \n";
        }

        // Step six, update choice counter
        if (local_prediction != global_prediction)
        {
            if (local_prediction == instr.taken)
            {
                // More towards local predictor.
                choice_counters[choice_counters_idx]--;
            }
            else if (global_prediction == instr.taken)
            {
                // More towards global predictor.
                choice_counters[choice_counters_idx]++;
            }
        }
        // std::cout << "Updated choice counter value: " 
        //           << int(choice_counters[choice_counters_idx]) << "\n";

        // Step seven, update local and global counters
        if (instr.taken)
        {
            local_counters[local_counters_idx]++;
            global_counters[global_counters_idx]++;

            updateHistRegTaken(t_id);
            updateLocalHistTaken(local_hist_tab_idx);
        }
        else
        {
            local_counters[local_counters_idx]--;
            global_counters[global_counters_idx]--;

            updateHistRegNotTaken(t_id);
            updateLocalHistNotTaken(local_hist_tab_idx);
        }
        // std::cout << "Updated local counter value: "
        //           << int(local_counters[local_counters_idx]) << "\n";
        // std::cout << "Updated global counter value: "
        //           << int(global_counters[global_counters_idx]) << "\n";
        // std::cout << "Updated local history: " 
        //           << local_history_table[local_hist_tab_idx] << "\n";
        // std::cout << "Updated global history: " << history_registers[t_id] << "\n";

        // Step eight, determine prediction correctness.
        if (final_prediction == instr.taken)
        {
            correct_preds++;
            return true;
        }
        else
        {
            incorrect_preds++;
            return false;
        }
    }

    inline unsigned calcLocHistIdx(Addr &branch_addr)
    {
        return (branch_addr >> instShiftAmt) & (localHistoryTableSize - 1);
    }

    inline void updateHistRegTaken(unsigned t_id)
    {
        history_registers[t_id] = (history_registers[t_id] << 1) | 1;
    }
    
    inline void updateHistRegNotTaken(unsigned t_id)
    {
        history_registers[t_id] = (history_registers[t_id] << 1);
    }

    inline void updateLocalHistTaken(unsigned local_history_idx)
    {
        local_history_table[local_history_idx] = 
            (local_history_table[local_history_idx] << 1) | 1;
    }

    inline void updateLocalHistNotTaken(unsigned local_history_idx)
    {
        local_history_table[local_history_idx] = 
            (local_history_table[local_history_idx] << 1);
    }

};
}

#endif
