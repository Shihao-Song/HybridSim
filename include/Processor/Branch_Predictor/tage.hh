#ifndef __TAGE_HH__
#define __TAGE_HH__

#include "Processor/Branch_Predictor/branch_predictor.hh"
#include "Processor/Branch_Predictor/gem5_random.hh"
#include "Processor/Branch_Predictor/params.hh"

#include <vector>

namespace CoreSystem
{
// Majority of the codes are copied from gem5
class TAGE : public Branch_Predictor
{  
  // All data structures
  protected:
    // Tage Entry
    struct TageEntry
    {
        int8_t ctr;
        uint16_t tag;
        uint8_t u;
        TageEntry() : ctr(0), tag(0), u(0) {}
    };

    // Folded History Table - compressed history
    // to mix with instruction PC to index partially
    // tagged tables.
    struct FoldedHistory
    {
        unsigned comp;
        int compLength;
        int origLength;
        int outpoint;
        int bufferSize;

        FoldedHistory()
        {
            comp = 0;
        }

        void init(int original_length, int compressed_length)
        {
            origLength = original_length;
            compLength = compressed_length;
            outpoint = original_length % compressed_length;
        }

        void update(uint8_t * h)
        {
            comp = (comp << 1) | h[0];
            comp ^= h[origLength] << outpoint;
            comp ^= (comp >> compLength);
            comp &= (ULL(1) << compLength) - 1;
        }
    };

    // Keep per-thread histories to
    // support SMT.
    struct ThreadHistory
    {
        // Speculative path history
        // (LSB of branch address)
        int pathHist;

        // Speculative branch direction
        // history (circular buffer)
        // @TODO Convert to std::vector<bool>
        uint8_t *globalHistory;

        // Pointer to most recent branch outcome
        uint8_t* gHist;

        // Index to most recent branch outcome
        int ptGhist;

        // Speculative folded histories.
        FoldedHistory *computeIndices;
        FoldedHistory *computeTags[2];
    };

    // provider type
    enum {
        BIMODAL_ONLY = 0,
        TAGE_LONGEST_MATCH,
        BIMODAL_ALT_MATCH,
        TAGE_ALT_MATCH,
        LAST_TAGE_PROVIDER_TYPE = TAGE_ALT_MATCH
    };

    // Primary branch history entry
    struct BranchInfo
    {
        int pathHist;
        int ptGhist;
        int hitBank;
        int hitBankIndex;
        int altBank;
        int altBankIndex;
        int bimodalIndex;

        bool tagePred;
        bool altTaken;
        bool condBranch;
        bool longestMatchPred;
        bool pseudoNewAlloc;
        Addr branchPC;

        // Pointer to dynamically allocated storage
        // to save table indices and folded histories.
        // To do one call to new instead of five.
        int *storage;

        // Pointers to actual saved array within the dynamically
        // allocated storage.
        int *tableIndices;
        int *tableTags;
        int *ci;
        int *ct0;
        int *ct1;

        // for stats purposes
        unsigned provider;

        BranchInfo(unsigned nHistoryTables)
            : pathHist(0), ptGhist(0),
              hitBank(0), hitBankIndex(0),
              altBank(0), altBankIndex(0),
              bimodalIndex(0),
              tagePred(false), altTaken(false),
              condBranch(false), longestMatchPred(false),
              pseudoNewAlloc(false), branchPC(0),
              provider(-1)
        {
            int sz = nHistoryTables + 1;
            storage = new int [sz * 5];
            tableIndices = storage;
            tableTags = storage + sz;
            ci = tableTags + sz;
            ct0 = ci + sz;
            ct1 = ct0 + sz;
        }

        virtual ~BranchInfo()
        {
            delete[] storage;
        }
    };

  // All necessary parameters
  protected:
    typedef unsigned ThreadID;
    typedef uint64_t ULL;

    const unsigned numHWThreads;

    // logRatioBiModalHystEntries, "Exploiting Bias in the Hysteresis Bit of 2-bit Saturating Counters in Branch Predictors" allows a hysteresis bit is shared among N prediction bits.
    const unsigned logRatioBiModalHystEntries;
    // nHistoryTables, number of history tables. TODO, confirm this is the tagged tables.
    const unsigned nHistoryTables;
    // tagTableCounterBits. TODO, confirm this is the prediction bits.
    const unsigned tagTableCounterBits;
    const unsigned tagTableUBits; // TODO, should be the useful bits
    const unsigned histBufferSize; // TODO, what is this.
    const unsigned minHist;
    const unsigned maxHist;
    const unsigned pathHistBits; // TODO, what is this.

    // Tag size in TAGE tag tables
    std::vector<unsigned> tagTableTagWidths;
    // Log2 of TAGE table sizes
    std::vector<int> logTagTableSizes;

    std::vector<bool> btablePrediction;
    std::vector<bool> btableHysteresis;
    TageEntry **gtable;
    
    std::vector<ThreadHistory> threadHistory;

    int *histLengths;
    int *tableIndices;
    int *tableTags;

    std::vector<int8_t> useAltPredForNewlyAllocated;
    int64_t tCounter;
    // Log period in number of branches to reset TAGE useful counters
    uint64_t logUResetPeriod;
    int64_t initialTCounterValue; // TODO
    unsigned numUseAltOnNa; // TODO
    unsigned useAltOnNaBits; // TODO
    unsigned maxNumAlloc; // Max number of TAGE entries allocted on mispredict

    // Tells which tables are active
    // (for the base TAGE implementation all are active)
    // Some other classes use this for handling associativity
    std::vector<bool> noSkip;

    const bool speculativeHistUpdate; // TODO

    bool initialized = 0;

  public:
    TAGE(TAGEParams *p):
     numHWThreads(p->numHWThreads),
     logRatioBiModalHystEntries(p->logRatioBiModalHystEntries),
     nHistoryTables(p->nHistoryTables),
     tagTableCounterBits(p->tagTableCounterBits),
     tagTableUBits(p->tagTableUBits),
     histBufferSize(p->histBufferSize),
     minHist(p->minHist),
     maxHist(p->maxHist),
     pathHistBits(p->pathHistBits),
     tagTableTagWidths(p->tagTableTagWidths),
     logTagTableSizes(p->logTagTableSizes),
     logUResetPeriod(p->logUResetPeriod),
     initialTCounterValue(p->initialTCounterValue),
     numUseAltOnNa(p->numUseAltOnNa),
     useAltOnNaBits(p->useAltOnNaBits),
     maxNumAlloc(p->maxNumAlloc),
     noSkip(p->noSkip),
     speculativeHistUpdate(p->speculativeHistUpdate),
     initialized(false)
    {
        // Enable all the tables
        noSkip.resize(nHistoryTables + 1, true);

        // Current method for periodically resetting the u counter bits only
        // works for 1 or 2 bits
        // Also make sure that it is not 0
        assert(tagTableUBits <= 2 && (tagTableUBits > 0));

        // we use int type for the path history, so it cannot be more than
        // its size
        assert(pathHistBits <= (sizeof(int) * 8));

        // initialize the counter to half of the period
        assert(logUResetPeriod != 0);
        // initialTCounterValue = (1 << 17);
        tCounter = initialTCounterValue;

        assert(histBufferSize > maxHist * 2);

        useAltPredForNewlyAllocated.resize(numUseAltOnNa, 0);

        threadHistory.resize(numHWThreads); // TODO, 1 hw thread.
        assert(threadHistory.size() == 1);
        for (auto& history : threadHistory) 
        {
            history.pathHist = 0;
            history.globalHistory = new uint8_t[histBufferSize];
            history.gHist = history.globalHistory;
            memset(history.gHist, 0, histBufferSize);
            history.ptGhist = 0;
        }

        histLengths = new int [nHistoryTables+1];

        calculateParameters();

        assert(tagTableTagWidths.size() == (nHistoryTables+1));
        assert(logTagTableSizes.size() == (nHistoryTables+1));

        // First entry is for the Bimodal table and it is untagged in this
        // implementation
        assert(tagTableTagWidths[0] == 0);

        for (auto& history : threadHistory)
        {
            history.computeIndices = new FoldedHistory[nHistoryTables+1];
            history.computeTags[0] = new FoldedHistory[nHistoryTables+1];
            history.computeTags[1] = new FoldedHistory[nHistoryTables+1];

            initFoldedHistories(history);
        }

        const uint64_t bimodalTableSize = ULL(1) << logTagTableSizes[0];
        btablePrediction.resize(bimodalTableSize, false);
        btableHysteresis.resize(bimodalTableSize >> logRatioBiModalHystEntries,
                                true);

        gtable = new TageEntry*[nHistoryTables + 1];
        buildTageTables();

        tableIndices = new int [nHistoryTables+1];
        tableTags = new int [nHistoryTables+1];
        initialized = true;

        /*
        // Make sure all parameters are correct.
        std::cout << nHistoryTables << "\n";
        std::cout << minHist << "\n";
        std::cout << maxHist << "\n";
        for (auto &item : tagTableTagWidths) { std::cout << item << ", "; }
        std::cout << "\n";
        for (auto &item : logTagTableSizes) { std::cout << item << ", "; }
        std::cout << "\n";
        std::cout << logRatioBiModalHystEntries << "\n";
        std::cout << tagTableCounterBits << "\n";
        std::cout << tagTableUBits << "\n";
        std::cout << histBufferSize << "\n";
        std::cout << pathHistBits << "\n";
        std::cout << logUResetPeriod << "\n";
        std::cout << numUseAltOnNa << "\n";
        std::cout << initialTCounterValue << "\n";
        std::cout << useAltOnNaBits << "\n";
        std::cout << maxNumAlloc << "\n";
        for (auto item : noSkip) { std::cout << item << ", "; }
        std::cout << "\n";
        exit(0);
        */
    }

    Random random_mt;
    bool predict(Instruction &instr) override
    {
        // Step one, perform TAGE prediction
        unsigned tid = 0; // TODO, we only consider one hardware thread for now.
        Addr branch_pc = instr.eip;

        BranchInfo *bi = new BranchInfo(nHistoryTables);
        // std::cout << "TAGE predicting...\n";
        bool final_prediction = tagePredict(tid, branch_pc, bi);

        // Step two, update.
        int nrand = random_mt.random<int>() & 3;
        // updateStats(instr.taken, bi.get());
        condBranchUpdate(tid, branch_pc, instr.taken, bi, nrand, bi->tagePred);

        updateHistories(tid, branch_pc, instr.taken, bi, false);

        delete bi;
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

    // For detailed information, read "A PPM-like, tag-based branch predictor"
    bool tagePredict(unsigned tid,
                     Addr branch_pc,
                     BranchInfo* bi)
    {
        Addr pc = branch_pc;
        bool pred_taken = true;

        calculateIndicesAndTags(tid, pc, bi);

        bi->bimodalIndex = bindex(pc);

        bi->hitBank = 0;
        bi->altBank = 0;

        //Look for the bank with longest matching history
        for (int i = nHistoryTables; i > 0; i--) {
            if (noSkip[i] &&
                gtable[i][tableIndices[i]].tag == tableTags[i]) {
                bi->hitBank = i;
                bi->hitBankIndex = tableIndices[bi->hitBank];
                break;
            }
        }

        //Look for the alternate bank
        for (int i = bi->hitBank - 1; i > 0; i--) {
            if (noSkip[i] &&
                gtable[i][tableIndices[i]].tag == tableTags[i]) {
                bi->altBank = i;
                bi->altBankIndex = tableIndices[bi->altBank];
                break;
            }
        }

        //computes the prediction and the alternate prediction
        if (bi->hitBank > 0) {
            if (bi->altBank > 0) {
                bi->altTaken =
                    gtable[bi->altBank][tableIndices[bi->altBank]].ctr >= 0;
                extraAltCalc(bi); // Not used in Base TAGE implementation.
            }else {
                bi->altTaken = getBimodePred(pc, bi);
            }

            // TODO, understand this flow.
            bi->longestMatchPred =
                gtable[bi->hitBank][tableIndices[bi->hitBank]].ctr >= 0;
            bi->pseudoNewAlloc =
                abs(2 * gtable[bi->hitBank][bi->hitBankIndex].ctr + 1) <= 1;

            //if the entry is recognized as a newly allocated entry and
            //useAltPredForNewlyAllocated is positive use the alternate
            //prediction
            if ((useAltPredForNewlyAllocated[getUseAltIdx(bi, branch_pc)] < 0)
                || ! bi->pseudoNewAlloc) {
                bi->tagePred = bi->longestMatchPred;
                bi->provider = TAGE_LONGEST_MATCH;
            } else {
                bi->tagePred = bi->altTaken;
                bi->provider = bi->altBank ? TAGE_ALT_MATCH
                                           : BIMODAL_ALT_MATCH;
            }
        } else {
            bi->altTaken = getBimodePred(pc, bi);
            bi->tagePred = bi->altTaken;
            bi->longestMatchPred = bi->altTaken;
            bi->provider = BIMODAL_ONLY;
        }

        //end TAGE prediction

        pred_taken = (bi->tagePred);

        bi->branchPC = branch_pc;
        bi->condBranch = true;
        return pred_taken;
    }

  protected:
    void initFoldedHistories(ThreadHistory & history)
    {
        for (int i = 1; i <= nHistoryTables; i++)
        {
            history.computeIndices[i].init(
                histLengths[i], (logTagTableSizes[i]));
            history.computeTags[0][i].init(
                history.computeIndices[i].origLength, tagTableTagWidths[i]);
            history.computeTags[1][i].init(
                history.computeIndices[i].origLength, tagTableTagWidths[i]-1);
        }
    }

    void buildTageTables()
    {
        for (int i = 1; i <= nHistoryTables; i++)
        {
            gtable[i] = new TageEntry[1<<(logTagTableSizes[i])];
        }
    }

    void calculateParameters()
    {
        histLengths[1] = minHist;
        histLengths[nHistoryTables] = maxHist;

        for (int i = 2; i <= nHistoryTables; i++)
        {
            histLengths[i] = (int) (((double) minHist *
                           pow ((double) (maxHist) / (double) minHist,
                               (double) (i - 1) / (double) ((nHistoryTables- 1))))
                           + 0.5);
        }
    }

    int F(int A, int size, int bank) const
    {
        int A1, A2;

        A = A & ((ULL(1) << size) - 1);
        A1 = (A & ((ULL(1) << logTagTableSizes[bank]) - 1));
        A2 = (A >> logTagTableSizes[bank]);
        A2 = ((A2 << bank) & ((ULL(1) << logTagTableSizes[bank]) - 1))
           + (A2 >> (logTagTableSizes[bank] - bank));
        A = A1 ^ A2;
        A = ((A << bank) & ((ULL(1) << logTagTableSizes[bank]) - 1))
          + (A >> (logTagTableSizes[bank] - bank));
        return (A);
    }

    // computes a full has of PC, ghist and pathHist
    int gindex(ThreadID tid, Addr pc, int bank) const
    {
        int index;
        int hlen = (histLengths[bank] > pathHistBits) ? pathHistBits :
                                                        histLengths[bank];
        const unsigned int shiftedPc = pc >> instShiftAmt;

        index =
            shiftedPc ^
            (shiftedPc >> ((int) abs(logTagTableSizes[bank] - bank) + 1)) ^
            threadHistory[tid].computeIndices[bank].comp ^
            F(threadHistory[tid].pathHist, hlen, bank);
        // std::cout << "Index calculation is done.\n";
        return (index & ((ULL(1) << (logTagTableSizes[bank])) - 1));
    }

    // Tag computation
    uint16_t gtag(ThreadID tid, Addr pc, int bank) const
    {
        int tag = (pc >> instShiftAmt) ^
                  threadHistory[tid].computeTags[0][bank].comp ^
                  (threadHistory[tid].computeTags[1][bank].comp << 1);

        // std::cout << "Tage calculation is done.\n";
        return (tag & ((ULL(1) << tagTableTagWidths[bank]) - 1));
    }

    void calculateIndicesAndTags(ThreadID tid, Addr branch_pc,
                                 BranchInfo* bi)
    {
        // computes the table addresses and the partial tags
        for (int i = 1; i <= nHistoryTables; i++) 
        {
            tableIndices[i] = gindex(tid, branch_pc, i);
            bi->tableIndices[i] = tableIndices[i];
            tableTags[i] = gtag(tid, branch_pc, i);
            bi->tableTags[i] = tableTags[i];
        }
        // std::cout << "Indicies and Tags calculations have been finished.\n";
    }

    int bindex(Addr pc_in) const
    {
        return ((pc_in >> instShiftAmt) & ((ULL(1) << (logTagTableSizes[0])) - 1));
    }

    void extraAltCalc(BranchInfo* bi)
    {
        // do nothing. This is only used in some derived classes
        return;
    }

    bool getBimodePred(Addr pc, BranchInfo* bi) const
    {
        return btablePrediction[bi->bimodalIndex];
    }

    unsigned getUseAltIdx(BranchInfo* bi, Addr branch_pc)
    {
        // There is only 1 counter on the base TAGE implementation
        return 0;
    }

    void adjustAlloc(bool & alloc, bool taken, bool pred_taken)
    {
        // Nothing for this base class implementation
        return;
    }

    // Up-down saturating counter
    template<typename T> void ctrUpdate(T & ctr, bool taken, int nbits)
    {
        assert(nbits <= sizeof(T) << 3);
        if (taken) {
            if (ctr < ((1 << (nbits - 1)) - 1))
                ctr++;
        } else {
            if (ctr > -(1 << (nbits - 1)))
                ctr--;
        }
    }

    void resetUctr(uint8_t & u)
    {
        u >>= 1;
    }

    void handleUReset()
    {
        //periodic reset of u: reset is not complete but bit by bit
        if ((tCounter & ((ULL(1) << logUResetPeriod) - 1)) == 0) {
            // reset least significant bit
            // most significant bit becomes least significant bit
            for (int i = 1; i <= nHistoryTables; i++) {
                for (int j = 0; j < (ULL(1) << logTagTableSizes[i]); j++) {
                    resetUctr(gtable[i][j].u);
                }
            }
        }
    }

    void handleAllocAndUReset(bool alloc, bool taken, BranchInfo* bi,
                          int nrand)
    {
        if (alloc) {
            // is there some "unuseful" entry to allocate
            uint8_t min = 1;
            for (int i = nHistoryTables; i > bi->hitBank; i--) {
                if (gtable[i][bi->tableIndices[i]].u < min) {
                    min = gtable[i][bi->tableIndices[i]].u;
                }
            }

            // we allocate an entry with a longer history
            // to  avoid ping-pong, we do not choose systematically the next
            // entry, but among the 3 next entries
            int Y = nrand &
                ((ULL(1) << (nHistoryTables - bi->hitBank - 1)) - 1);
            int X = bi->hitBank + 1;
            if (Y & 1) {
                X++;
                if (Y & 2)
                    X++;
            }
            // No entry available, forces one to be available
            if (min > 0) {
                gtable[X][bi->tableIndices[X]].u = 0;
            }

            //Allocate entries
            unsigned numAllocated = 0;
            for (int i = X; i <= nHistoryTables; i++) {
                if ((gtable[i][bi->tableIndices[i]].u == 0)) {
                    gtable[i][bi->tableIndices[i]].tag = bi->tableTags[i];
                    gtable[i][bi->tableIndices[i]].ctr = (taken) ? 0 : -1;
                    ++numAllocated;
                    if (numAllocated == maxNumAlloc) {
                        break;
                    }
                }
            }
        }

        tCounter++;

        handleUReset();
    }

    void unsignedCtrUpdate(uint8_t & ctr, bool up, unsigned nbits)
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

    // Update the bimodal predictor: a hysteresis bit is shared among N prediction
    // bits (N = 2 ^ logRatioBiModalHystEntries)
    void baseUpdate(Addr pc, bool taken, BranchInfo* bi)
    {
        int inter = (btablePrediction[bi->bimodalIndex] << 1)
            + btableHysteresis[bi->bimodalIndex >> logRatioBiModalHystEntries];
        if (taken) {
            if (inter < 3)
                inter++;
        } else if (inter > 0) {
            inter--;
        }
        const bool pred = inter >> 1;
        const bool hyst = inter & 1;
        btablePrediction[bi->bimodalIndex] = pred;
        btableHysteresis[bi->bimodalIndex >> logRatioBiModalHystEntries] = hyst;
    }

    void handleTAGEUpdate(Addr branch_pc, bool taken, BranchInfo* bi)
    {
        if (bi->hitBank > 0) {
            ctrUpdate(gtable[bi->hitBank][bi->hitBankIndex].ctr, taken,
                      tagTableCounterBits);
            // if the provider entry is not certified to be useful also update
            // the alternate prediction
            if (gtable[bi->hitBank][bi->hitBankIndex].u == 0) {
                if (bi->altBank > 0) {
                    ctrUpdate(gtable[bi->altBank][bi->altBankIndex].ctr, taken,
                              tagTableCounterBits);
                }
                if (bi->altBank == 0) {
                    baseUpdate(branch_pc, taken, bi);
                }
            }

            // update the u counter
            if (bi->tagePred != bi->altTaken) {
                unsignedCtrUpdate(gtable[bi->hitBank][bi->hitBankIndex].u,
                                  bi->tagePred == taken, tagTableUBits);
            }
        } else {
            baseUpdate(branch_pc, taken, bi);
        }
    }

    void condBranchUpdate(ThreadID tid, Addr branch_pc, bool taken,
        BranchInfo* bi, int nrand, bool pred, bool preAdjustAlloc = false)
    {
        // TAGE UPDATE
        // try to allocate a  new entries only if prediction was wrong
        bool alloc = (bi->tagePred != taken) && (bi->hitBank < nHistoryTables);

        if (preAdjustAlloc)
        {
            adjustAlloc(alloc, taken, pred);
        }

        if (bi->hitBank > 0) {
            // Manage the selection between longest matching and alternate
            // matching for "pseudo"-newly allocated longest matching entry
            bool PseudoNewAlloc = bi->pseudoNewAlloc;
            // an entry is considered as newly allocated if its prediction
            // counter is weak
            if (PseudoNewAlloc) {
                if (bi->longestMatchPred == taken) {
                    alloc = false;
                }
                // if it was delivering the correct prediction, no need to
                // allocate new entry even if the overall prediction was false
                if (bi->longestMatchPred != bi->altTaken) {
                    ctrUpdate(
                        useAltPredForNewlyAllocated[getUseAltIdx(bi, branch_pc)],
                        bi->altTaken == taken, useAltOnNaBits);
                }
            }
        }

        if (!preAdjustAlloc) {
            adjustAlloc(alloc, taken, pred);
        }

        handleAllocAndUReset(alloc, taken, bi, nrand);

        handleTAGEUpdate(branch_pc, taken, bi);
    }

    // shifting the global history:  we manage the history in a big table in order
    // to reduce simulation time
    void updateGHist(uint8_t * &h, bool dir, uint8_t * tab, int &pt)
    {
        if (pt == 0) {
             // Copy beginning of globalHistoryBuffer to end, such that
             // the last maxHist outcomes are still reachable
             // through pt[0 .. maxHist - 1].
             for (int i = 0; i < maxHist; i++)
                 tab[histBufferSize - maxHist + i] = tab[i];
             pt =  histBufferSize - maxHist;
             h = &tab[pt];
        }
        pt--;
        h--;
        h[0] = (dir) ? 1 : 0;
    }

    void updateHistories(ThreadID tid, Addr branch_pc, bool taken,
                         BranchInfo* bi, bool speculative)
    {
        if (speculative != speculativeHistUpdate) {
            return;
        }
        ThreadHistory& tHist = threadHistory[tid];
        //  UPDATE HISTORIES
        bool pathbit = ((branch_pc >> instShiftAmt) & 1);
        //on a squash, return pointers to this and recompute indices.
        //update user history
        updateGHist(tHist.gHist, taken, tHist.globalHistory, tHist.ptGhist);
        tHist.pathHist = (tHist.pathHist << 1) + pathbit;
        tHist.pathHist = (tHist.pathHist & ((ULL(1) << pathHistBits) - 1));

        if (speculative) {
            bi->ptGhist = tHist.ptGhist;
            bi->pathHist = tHist.pathHist;
        }

        //prepare next index and tag computations for user branchs
        for (int i = 1; i <= nHistoryTables; i++)
        {
            if (speculative) {
                bi->ci[i]  = tHist.computeIndices[i].comp;
                bi->ct0[i] = tHist.computeTags[0][i].comp;
                bi->ct1[i] = tHist.computeTags[1][i].comp;
            }
            tHist.computeIndices[i].update(tHist.gHist);
            tHist.computeTags[0][i].update(tHist.gHist);
            tHist.computeTags[1][i].update(tHist.gHist);
        }
        assert(threadHistory[tid].gHist ==
                &threadHistory[tid].globalHistory[threadHistory[tid].ptGhist]);
    }

    /*
    uint64_t tageLongestMatchProviderCorrect = 0;
    uint64_t tageAltMatchProviderCorrect = 0;
    uint64_t bimodalAltMatchProviderCorrect = 0;
    uint64_t tageBimodalProviderCorrect = 0;
    uint64_t tageLongestMatchProviderWrong = 0;
    uint64_t tageAltMatchProviderWrong = 0;
    uint64_t bimodalAltMatchProviderWrong = 0;
    uint64_t tageBimodalProviderWrong = 0;
    uint64_t tageAltMatchProviderWouldHaveHit = 0;
    uint64_t tageLongestMatchProviderWouldHaveHit = 0;

    Stats::Vector tageLongestMatchProvider;
    Stats::Vector tageAltMatchProvider;
    void updateStats(bool taken, BranchInfo* bi)
    {
        if (taken == bi->tagePred) {
            // correct prediction
            switch (bi->provider) {
            case BIMODAL_ONLY: tageBimodalProviderCorrect++; break;
            case TAGE_LONGEST_MATCH: tageLongestMatchProviderCorrect++; break;
            case BIMODAL_ALT_MATCH: bimodalAltMatchProviderCorrect++; break;
            case TAGE_ALT_MATCH: tageAltMatchProviderCorrect++; break;
            }
        } else {
            // wrong prediction
            switch (bi->provider) {
              case BIMODAL_ONLY: tageBimodalProviderWrong++; break;
              case TAGE_LONGEST_MATCH:
                tageLongestMatchProviderWrong++;
                if (bi->altTaken == taken) {
                    tageAltMatchProviderWouldHaveHit++;
                }
                break;
              case BIMODAL_ALT_MATCH:
                bimodalAltMatchProviderWrong++;
                break;
              case TAGE_ALT_MATCH:
                tageAltMatchProviderWrong++;
                break;
            }

            switch (bi->provider) {
              case BIMODAL_ALT_MATCH:
              case TAGE_ALT_MATCH:
                if (bi->longestMatchPred == taken) {
                    tageLongestMatchProviderWouldHaveHit++;
                }
            }
        }
        switch (bi->provider) {
          case TAGE_LONGEST_MATCH:
          case TAGE_ALT_MATCH:
            tageLongestMatchProvider[bi->hitBank]++;
            tageAltMatchProvider[bi->altBank]++;
            break;
        }
    }
    */
};
}

#endif
