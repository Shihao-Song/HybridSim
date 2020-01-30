#ifndef __LTAGE_HH__
#define __LTAGE_HH__

#include "Processor/Branch_Predictor/loop_predictor.hh"
#include "Processor/Branch_Predictor/tage.hh"

#include <vector>

namespace CoreSystem
{
class LTAGE : public TAGE
{
  protected:
   LoopPredictor *loopPredictor;

  public:
    LTAGE(LTAGEParams *p) : TAGE(p->tage)
    {
        loopPredictor = new LoopPredictor(p->lp);
    }

    // more provider types
    enum {
        LOOP = TAGE::LAST_TAGE_PROVIDER_TYPE + 1,
        LAST_LTAGE_PROVIDER_TYPE = LOOP
    };

    bool predict(Instruction &instr) override
    {
        // Step one, predict
        ThreadID tid = 0;
        Addr branch_pc = instr.eip;
        BranchInfo *tage_info = new BranchInfo(nHistoryTables);
        LoopPredictor::BranchInfo *loop_info = new LoopPredictor::BranchInfo();

        bool pred = tagePredict(tid, branch_pc, tage_info);
        pred = loopPredictor->loopPredict(tid, branch_pc, true,
                                          loop_info, pred,
                                          instShiftAmt);

        if (loop_info->loopPredUsed)
        {
            tage_info->provider = LOOP;
        }

        loop_info->predTaken = pred; // Record final prediction

        bool final_pred = pred;

        // Step two, update
        int nrand = random_mt.random<int>() & 3;
        
        loopPredictor->condBranchUpdate(tid, branch_pc, instr.taken, tage_info->tagePred,
                                        loop_info, instShiftAmt);
        condBranchUpdate(tid, branch_pc, instr.taken, tage_info, nrand, loop_info->predTaken);
        
        updateHistories(tid, branch_pc, instr.taken, tage_info, false);

        delete tage_info;
        delete loop_info;

        if (instr.taken == final_pred)
        {
            correct_preds++;
            return true; // Indicate a correct prediction.
        }
        else
        {
            incorrect_preds++;
            return false; // Indicate an in-correct prediction.
        }
    }
};
}

#endif
