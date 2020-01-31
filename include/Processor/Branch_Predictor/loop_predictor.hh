#ifndef __LOOP_PREDICTOR_HH__
#define __LOOP_PREDICTOR_HH__

#include "Processor/Branch_Predictor/branch_predictor.hh"
#include "Processor/Branch_Predictor/params.hh"

namespace CoreSystem
{
class LoopPredictor : public Branch_Predictor
{
  public:
    LoopPredictor(LPParams *p)
        : logSizeLoopPred(p->logSizeLoopPred),
          loopTableAgeBits(p->loopTableAgeBits),
          loopTableConfidenceBits(p->loopTableConfidenceBits),
          loopTableTagBits(p->loopTableTagBits),
          loopTableIterBits(p->loopTableIterBits),
          logLoopTableAssoc(p->logLoopTableAssoc),
          confidenceThreshold((1 << loopTableConfidenceBits) - 1),
          loopTagMask((1 << loopTableTagBits) - 1),
          loopNumIterMask((1 << loopTableIterBits) - 1),
          loopSetMask((1 << (logSizeLoopPred - logLoopTableAssoc)) - 1),
          loopUseCounter(-1),
          withLoopBits(p->withLoopBits),
          useDirectionBit(p->useDirectionBit),
          useSpeculation(p->useSpeculation),
          useHashing(p->useHashing),
          restrictAllocation(p->restrictAllocation),
          initialLoopIter(p->initialLoopIter),
          initialLoopAge(p->initialLoopAge),
          optionalAgeReset(p->optionalAgeReset)
    {
    assert(initialLoopAge <= ((1 << loopTableAgeBits) - 1));

    assert(loopTableTagBits <= 16);
    assert(loopTableIterBits <= 16);

    assert(logSizeLoopPred >= logLoopTableAssoc);

    ltable = new LoopEntry[ULL(1) << logSizeLoopPred];

    /*
    std::cout << logSizeLoopPred << "\n";
    std::cout << withLoopBits << "\n";
    std::cout << loopTableAgeBits << "\n";
    std::cout << loopTableConfidenceBits << "\n";
    std::cout << loopTableTagBits << "\n";
    std::cout << loopTableIterBits << "\n";
    std::cout << logLoopTableAssoc << "\n";
    std::cout << useSpeculation << "\n";
    std::cout << useHashing << "\n";
    std::cout << useDirectionBit << "\n";
    std::cout << restrictAllocation << "\n";
    std::cout << initialLoopIter << "\n";
    std::cout << initialLoopAge << "\n";
    std::cout << optionalAgeReset << "\n";
    exit(0);
    */
    }

    bool predict(Instruction &instr) override
    {
        std::cerr << "We don't use LoopPredictor as a standalone predictor.\n";
        exit(0);
    }

  protected:
    Random random_mt;

    typedef unsigned ThreadID;
    typedef uint64_t ULL;

    const unsigned logSizeLoopPred;
    const unsigned loopTableAgeBits;
    const unsigned loopTableConfidenceBits;
    const unsigned loopTableTagBits;
    const unsigned loopTableIterBits;
    const unsigned logLoopTableAssoc;
    const uint8_t confidenceThreshold;
    const uint16_t loopTagMask;
    const uint16_t loopNumIterMask;
    const int loopSetMask;

    // Prediction Structures
    // Loop Predictor Entry
    struct LoopEntry
    {
        uint16_t numIter;
        uint16_t currentIter;
        uint16_t currentIterSpec; // only for useSpeculation
        uint8_t confidence;
        uint16_t tag;
        uint8_t age;
        bool dir; // only for useDirectionBit

        LoopEntry() : numIter(0), currentIter(0), currentIterSpec(0),
                      confidence(0), tag(0), age(0), dir(0) { }
    };

    LoopEntry *ltable;

    int8_t loopUseCounter;
    unsigned withLoopBits;

    const bool useDirectionBit;
    const bool useSpeculation;
    const bool useHashing;
    const bool restrictAllocation;
    const unsigned initialLoopIter;
    const unsigned initialLoopAge;
    const bool optionalAgeReset;

    /**
     * Updates an unsigned counter based on up/down parameter
     * @param ctr Reference to counter to update.
     * @param up Boolean indicating if the counter is incremented/decremented
     * If true it is incremented, if false it is decremented
     * @param nbits Counter width.
     */
    static inline void unsignedCtrUpdate(uint8_t &ctr, bool up, unsigned nbits)
    {
        assert(nbits <= sizeof(uint8_t) << 3);
        if (up) {
            if (ctr < ((1 << nbits) - 1))
                ctr++;
        } else {
            if (ctr)
                ctr--;
        }
    }
    static inline void signedCtrUpdate(int8_t &ctr, bool up, unsigned nbits)
    {
        if (up) {
            if (ctr < ((1 << (nbits - 1)) - 1))
                ctr++;
        } else {
            if (ctr > -(1 << (nbits - 1)))
                ctr--;
        }
    }
  public:
    // Primary branch history entry
    struct BranchInfo
    {
        uint16_t loopTag;
        uint16_t currentIter;

        bool loopPred;
        bool loopPredValid;
        bool loopPredUsed;
        int  loopIndex;
        int  loopIndexB;  // only for useHashing
        int loopHit;
        bool predTaken;

        BranchInfo()
            : loopTag(0), currentIter(0),
              loopPred(false),
              loopPredValid(false), loopIndex(0), loopIndexB(0), loopHit(0),
              predTaken(false)
        {}
    };

    /**
     * Computes the index used to access the
     * loop predictor.
     * @param pc_in The unshifted branch PC.
     * @param instShiftAmt Shift the pc by as many bits
     */
    int lindex(Addr pc_in, unsigned instShiftAmt) const
    {
    // The loop table is implemented as a linear table
    // If associativity is N (N being 1 << logLoopTableAssoc),
    // the first N entries are for set 0, the next N entries are for set 1,
    // and so on.
    // Thus, this function calculates the set and then it gets left shifted
    // by logLoopTableAssoc in order to return the index of the first of the
    // N entries of the set
    Addr pc = pc_in >> instShiftAmt;
    if (useHashing) {
        pc ^= pc_in;
    }
    return ((pc & loopSetMask) << logLoopTableAssoc);
    }

    /**
     * Computes the index used to access the
     * ltable structures.
     * It may take hashing into account
     * @param index Result of lindex function
     * @param lowPcBits PC bits masked with set size
     * @param way Way to be used
     */
    int finallindex(int index, int lowPcBits, int way) const
    {
    return (useHashing ? (index ^ ((lowPcBits >> way) << logLoopTableAssoc)) :
                         (index))
           + way;
    }

    /**
     * Get a branch prediction from the loop
     * predictor.
     * @param pc The unshifted branch PC.
     * @param bi Pointer to information on the
     * prediction.
     * @param speculative Use speculative number of iterations
     * @param instShiftAmt Shift the pc by as many bits (if hashing is not
     * used)
     * @result the result of the prediction, if it could be predicted
     */
    bool getLoop(Addr pc, BranchInfo* bi, bool speculative,
                 unsigned instShiftAmt) const
    {
    bi->loopHit = -1;
    bi->loopPredValid = false;
    bi->loopIndex = lindex(pc, instShiftAmt);

    if (useHashing) {
        unsigned pcShift = logSizeLoopPred - logLoopTableAssoc;
        bi->loopIndexB = (pc >> pcShift) & loopSetMask;
        bi->loopTag = (pc >> pcShift) ^ (pc >> (pcShift + loopTableTagBits));
        bi->loopTag &= loopTagMask;
    } else {
        unsigned pcShift = instShiftAmt + logSizeLoopPred - logLoopTableAssoc;
        bi->loopTag = (pc >> pcShift) & loopTagMask;
        // bi->loopIndexB is not used without hash
    }

    for (int i = 0; i < (1 << logLoopTableAssoc); i++) {
        int idx = finallindex(bi->loopIndex, bi->loopIndexB, i);
        if (ltable[idx].tag == bi->loopTag) {
            bi->loopHit = i;
            bi->loopPredValid = calcConf(idx);

            uint16_t iter = speculative ? ltable[idx].currentIterSpec
                                        : ltable[idx].currentIter;

            if ((iter + 1) == ltable[idx].numIter) {
                return useDirectionBit ? !(ltable[idx].dir) : false;
            } else {
                return useDirectionBit ? (ltable[idx].dir) : true;
            }
        }
    }
    return false;
    }

   /**
    * Updates the loop predictor.
    * @param pc The unshifted branch PC.
    * @param taken The actual branch outcome.
    * @param bi Pointer to information on the
    * prediction recorded at prediction time.
    * @param tage_pred tage prediction of the branch
    */
    void loopUpdate(Addr pc, bool taken, BranchInfo* bi, bool tage_pred)
    {
    int idx = finallindex(bi->loopIndex, bi->loopIndexB, bi->loopHit);
    if (bi->loopHit >= 0) {
        //already a hit
        if (bi->loopPredValid) {
            if (taken != bi->loopPred) {
                // free the entry
                ltable[idx].numIter = 0;
                ltable[idx].age = 0;
                ltable[idx].confidence = 0;
                ltable[idx].currentIter = 0;
                return;
            } else if (bi->loopPred != tage_pred || optionalAgeInc()) {
                unsignedCtrUpdate(ltable[idx].age, true, loopTableAgeBits);
            }
        }

        ltable[idx].currentIter =
            (ltable[idx].currentIter + 1) & loopNumIterMask;
        if (ltable[idx].currentIter > ltable[idx].numIter) {
            ltable[idx].confidence = 0;
            if (ltable[idx].numIter != 0) {
                // free the entry
                ltable[idx].numIter = 0;
                if (optionalAgeReset) {
                    ltable[idx].age = 0;
                }
            }
        }
        
        if (taken != (useDirectionBit ? ltable[idx].dir : true)) {
            if (ltable[idx].currentIter == ltable[idx].numIter) {
                unsignedCtrUpdate(ltable[idx].confidence, true,
                                  loopTableConfidenceBits);
                //just do not predict when the loop count is 1 or 2
                if (ltable[idx].numIter < 3) {
                    // free the entry
                    ltable[idx].dir = taken; // ignored if no useDirectionBit
                    ltable[idx].numIter = 0;
                    ltable[idx].age = 0;
                    ltable[idx].confidence = 0;
                }
            }else {
                if (ltable[idx].numIter == 0) {
                    // first complete nest;
                    ltable[idx].confidence = 0;
                    ltable[idx].numIter = ltable[idx].currentIter;
                } else {
                    //not the same number of iterations as last time: free the
                    //entry
                    ltable[idx].numIter = 0;
                    if (optionalAgeReset) {
                        ltable[idx].age = 0;
                    }
                    ltable[idx].confidence = 0;
                }
            }
            ltable[idx].currentIter = 0;
        }
    } else if (useDirectionBit ? (bi->predTaken != taken) : taken) {
        if ((random_mt.random<int>() & 3) == 0 || !restrictAllocation) {
            //try to allocate an entry on taken branch
            int nrand = random_mt.random<int>();
            for (int i = 0; i < (1 << logLoopTableAssoc); i++) {
                int loop_hit = (nrand + i) & ((1 << logLoopTableAssoc) - 1);
                idx = finallindex(bi->loopIndex, bi->loopIndexB, loop_hit);
                if (ltable[idx].age == 0) {
                    ltable[idx].dir = !taken; // ignored if no useDirectionBit
                    ltable[idx].tag = bi->loopTag;
                    ltable[idx].numIter = 0;
                    ltable[idx].age = initialLoopAge;
                    ltable[idx].confidence = 0;
                    ltable[idx].currentIter = initialLoopIter;
                    break;
                } else {
                    ltable[idx].age--;
                }
                if (restrictAllocation) {
                    break;
                }
            }
        }
    }		    
    }

    /**
     * Speculatively updates the loop predictor
     * iteration count (only for useSpeculation).
     * @param taken The predicted branch outcome.
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     */
    void specLoopUpdate(bool taken, BranchInfo* bi)
    {
    if (bi->loopHit>=0) {
        int index = finallindex(bi->loopIndex, bi->loopIndexB, bi->loopHit);
        if (taken != ltable[index].dir) {
            ltable[index].currentIterSpec = 0;
        } else {
            ltable[index].currentIterSpec =
                (ltable[index].currentIterSpec + 1) & loopNumIterMask;
        }
    }
    }

    /**
     * Update LTAGE for conditional branches.
     * @param branch_pc The unshifted branch PC.
     * @param taken Actual branch outcome.
     * @param tage_pred Prediction from TAGE
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     * @param instShiftAmt Number of bits to shift instructions
     */
    void condBranchUpdate(ThreadID tid, Addr branch_pc, bool taken,
        bool tage_pred, BranchInfo* bi, unsigned instShiftAmt)
    {
    if (useSpeculation) {
        // recalculate loop prediction without speculation
        // It is ok to overwrite the loop prediction fields in bi
        // as the stats have already been updated with the previous
        // values
        bi->loopPred = getLoop(branch_pc, bi, false, instShiftAmt);
    }

    if (bi->loopPredValid) {
        if (bi->predTaken != bi->loopPred) {
            signedCtrUpdate(loopUseCounter,
                      (bi->loopPred == taken),
                      withLoopBits);
        }
    }

    loopUpdate(branch_pc, taken, bi, tage_pred);
    }

    /**
     * Get the loop prediction
     * @param tid The thread ID to select the global
     * histories to use.
     * @param branch_pc The unshifted branch PC.
     * @param cond_branch True if the branch is conditional.
     * @param bi Reference to wrapping pointer to allow storing
     * derived class prediction information in the base class.
     * @param prev_pred_taken Result of the TAGE prediction
     * @param instShiftAmt Shift the pc by as many bits
     * @param instShiftAmt Shift the pc by as many bits
     * @result the prediction, true if taken
     */
    bool loopPredict(
        ThreadID tid, Addr branch_pc, bool cond_branch,
        BranchInfo* bi, bool prev_pred_taken, unsigned instShiftAmt)
    {
    bool pred_taken = prev_pred_taken;
    if (cond_branch) {
        // loop prediction
        bi->loopPred = getLoop(branch_pc, bi, useSpeculation, instShiftAmt);

        if ((loopUseCounter >= 0) && bi->loopPredValid) {
            pred_taken = bi->loopPred;
            bi->loopPredUsed = true;
        }

        if (useSpeculation) {
            specLoopUpdate(pred_taken, bi);
        }
    }

    return pred_taken;
    }
    
    virtual bool calcConf(int index) const
    {
    return ltable[index].confidence == confidenceThreshold;
    }
    
    virtual bool optionalAgeInc() const
    {
        return false;
    }
    /**
     * Update the stats
     * @param taken Actual branch outcome
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     */
    /*
    void updateStats(bool taken, BranchInfo* bi);

    void squashLoop(BranchInfo * bi);

    void squash(ThreadID tid, BranchInfo *bi);



    virtual BranchInfo *makeBranchInfo();

    int8_t getLoopUseCounter() const
    {
        return loopUseCounter;
    }

    void init() override;

    void regStats() override;

    LoopPredictor(LoopPredictorParams *p);

    size_t getSizeInBits() const;
    */
};

}
#endif//__CPU_PRED_LOOP_PREDICTOR_HH__
