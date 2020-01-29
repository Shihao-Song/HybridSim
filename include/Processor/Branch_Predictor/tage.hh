#ifndef __TAGE_HH__
#define __TAGE_HH__

#include "Processor/Branch_Predictor/branch_predictor.hh"

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
        TageEntry() : ctr(0), tag(0), u(0) { }
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

    // logRatioBiModalHystEntries, "Exploiting Bias in the Hysteresis Bit of 2-bit Saturating Counters in Branch Predictors" allows a hysteresis bit is shared among N prediction bits.
    const unsigned logRatioBiModalHystEntries = 2;
    // nHistoryTables, number of history tables. TODO, confirm this is the tagged tables.
    const unsigned nHistoryTables = 7;
    // tagTableCounterBits. TODO, confirm this is the prediction bits.
    const unsigned tagTableCounterBits = 3;
    const unsigned tagTableUBits = 2; // TODO, should be the useful bits
    const unsigned histBufferSize = 2097152; // TODO, what is this.
    const unsigned minHist = 5;
    const unsigned maxHist = 130;
    const unsigned pathHistBits = 16; // TODO, what is this.

    // Tag size in TAGE tag tables
    std::vector<unsigned> tagTableTagWidths = {0, 9, 9, 10, 10, 11, 11, 12};
    // Log2 of TAGE table sizes
    std::vector<int> logTagTableSizes = {13, 9, 9, 9, 9, 9, 9, 9};

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
    uint64_t logUResetPeriod = 18;
    int64_t initialTCounterValue = 0; // TODO
    unsigned numUseAltOnNa = 1; // TODO
    unsigned useAltOnNaBits = 4; // TODO
    unsigned maxNumAlloc = 1; // Max number of TAGE entries allocted on mispredict

    // Tells which tables are active
    // (for the base TAGE implementation all are active)
    // Some other classes use this for handling associativity
    std::vector<bool> noSkip;

    const bool speculativeHistUpdate = true; // TODO

    bool initialized = 0;

  public:
    TAGE()
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
        initialTCounterValue = (1 << 17);
        tCounter = initialTCounterValue;

        assert(histBufferSize > maxHist * 2);

        useAltPredForNewlyAllocated.resize(numUseAltOnNa, 0);

        threadHistory.resize(1); // TODO, 1 hw thread.
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
    }

    bool predict(Instruction &instr) override
    {
        // Step one, perform TAGE prediction
        unsigned tid = instr.thread_id;
        Addr branch_pc = instr.eip;
        
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
                extraAltCalc(bi);
            }else {
                bi->altTaken = getBimodePred(pc, bi);
            }

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

        return (index & ((ULL(1) << (logTagTableSizes[bank])) - 1));
    }

    // Tag computation
    uint16_t gtag(ThreadID tid, Addr pc, int bank) const
    {
        int tag = (pc >> instShiftAmt) ^
                  threadHistory[tid].computeTags[0][bank].comp ^
                  (threadHistory[tid].computeTags[1][bank].comp << 1);

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
    }

    int bindex(Addr pc_in) const
    {
        return ((pc_in >> instShiftAmt) & ((ULL(1) << (logTagTableSizes[0])) - 1));
    }
};
}

#endif
