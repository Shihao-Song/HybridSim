#ifndef __PARAMS_HH__
#define __PARAMS_HH__

#include <cstdint>
#include <vector>

namespace CoreSystem
{
struct Params
{
    unsigned numHWThreads = 1;
};

struct TAGEParams : public Params
{
    unsigned nHistoryTables = 7;
    unsigned minHist = 5;
    unsigned maxHist = 130;

    std::vector<unsigned> tagTableTagWidths = {0, 9, 9, 10, 10, 11, 11, 12};
    std::vector<int> logTagTableSizes = {13, 9, 9, 9, 9, 9, 9, 9};
    unsigned logRatioBiModalHystEntries = 2;

    unsigned tagTableCounterBits = 3;
    unsigned tagTableUBits = 2;

    unsigned histBufferSize = 2097152;

    unsigned pathHistBits = 16;
    unsigned logUResetPeriod = 18;
    unsigned numUseAltOnNa = 1;
    unsigned initialTCounterValue = 1 << 17;
    unsigned useAltOnNaBits = 4;

    unsigned maxNumAlloc = 1;

    std::vector<bool> noSkip;

    bool speculativeHistUpdate = true;
};

struct TAGE_SC_L_TAGEParams : public TAGEParams
{
    unsigned logTagTableSize;

    unsigned shortTagsTageFactor;

    unsigned longTagsTageFactor;

    unsigned shortTagsSize = 8;

    unsigned longTagsSize;

    unsigned firstLongTagTable;

    bool truncatePathHist = true;

    TAGE_SC_L_TAGEParams()
    {
    tagTableTagWidths = {0};
    numUseAltOnNa = 16;
    pathHistBits = 27;
    maxNumAlloc = 2;
    logUResetPeriod = 10;
    initialTCounterValue = 1 << 9;
    useAltOnNaBits = 5;
    speculativeHistUpdate = false;
    }
};

struct TAGE_SC_L_TAGE_64KBParams : public TAGE_SC_L_TAGEParams
{
    TAGE_SC_L_TAGE_64KBParams()
    {
    nHistoryTables = 36;

    minHist = 6;
    maxHist = 3000;

    tagTableUBits = 1;

    logTagTableSizes = {13};

    noSkip = {0,0,1,0,0,0,1,0,0,1,1,1,1,1,1,1,1,1,1,
                1,1,1,1,0,1,0,1,0,1,0,0,0,1,0,0,0,1};

    logTagTableSize = 10;
    shortTagsTageFactor = 10;
    longTagsTageFactor = 20;

    longTagsSize = 12;

    firstLongTagTable = 13;
    }
};

struct LPParams : public Params
{
    unsigned logSizeLoopPred = 8;
    unsigned withLoopBits = 7;
    unsigned loopTableAgeBits = 8;
    unsigned loopTableConfidenceBits = 2;
    unsigned loopTableTagBits = 14;
    unsigned loopTableIterBits = 14;
    unsigned logLoopTableAssoc = 2;

    bool useSpeculation = false;

    bool useHashing = false;

    bool useDirectionBit = false;

    bool restrictAllocation = false;

    unsigned initialLoopIter = 1;
    unsigned initialLoopAge = 255;
    bool optionalAgeReset = true;
};

struct TAGE_SC_L_LoopPredictorParams : public LPParams
{
    TAGE_SC_L_LoopPredictorParams()
    {
    loopTableAgeBits = 4;
    loopTableConfidenceBits = 4;
    loopTableTagBits = 10;
    loopTableIterBits = 10;
    useSpeculation = false;
    useHashing = true;
    useDirectionBit = true;
    restrictAllocation = true;
    initialLoopIter = 0;
    initialLoopAge = 7;
    optionalAgeReset = false;
    }
};

struct TAGE_SC_L_64KB_LoopPredictorParams : public TAGE_SC_L_LoopPredictorParams
{
    TAGE_SC_L_64KB_LoopPredictorParams()
    {
    logSizeLoopPred = 5;
    }
};

struct LTAGEParams
{
    LPParams *lp;
    TAGEParams *tage;
};

struct StatisticalCorrectorParams
{
    unsigned numEntriesFirstLocalHistories;

    unsigned bwnb;
    std::vector<int> bwm;
    unsigned logBwnb;
    int bwWeightInitValue;

    unsigned lnb;
    std::vector<int> lm;
    unsigned logLnb;
    int lWeightInitValue;

    unsigned inb = 1;
    std::vector<int> im = {8};
    unsigned logInb;
    int iWeightInitValue;

    unsigned logBias;

    unsigned logSizeUp = 6;

    unsigned chooserConfWidth = 7;

    unsigned updateThresholdWidth = 12;

    unsigned pUpdateThresholdWidth = 8;

    unsigned extraWeightsWidth = 6;

    unsigned scCountersWidth = 6;

    int initialUpdateThresholdValue = 0;
};

struct TAGE_SC_L_64KB_StatisticalCorrectorParams : public StatisticalCorrectorParams
{
    unsigned pnb = 3;
    std::vector<int> pm = {25, 16, 9};
    unsigned logPnb = 9;

    unsigned snb = 3;
    std::vector<int> sm = {16, 11, 6};
    unsigned logSnb = 9;

    unsigned tnb = 2;
    std::vector<int> tm = {9, 4};
    unsigned logTnb = 10;

    unsigned imnb = 2;
    std::vector<int> imm = {10, 4};
    unsigned logImnb = 9;

    unsigned numEntriesSecondLocalHistories = 16;
    unsigned numEntriesThirdLocalHistories = 16;

    TAGE_SC_L_64KB_StatisticalCorrectorParams()
    {
        numEntriesFirstLocalHistories = 256;

        logBias = 8;

        bwnb = 3;
        bwm = {40, 24, 10};
        logBwnb = 10;
        bwWeightInitValue = 7;

        lnb = 3;
        lm = {11, 6, 3};
        logLnb = 10;
        lWeightInitValue = 7;

        logInb = 8;
        iWeightInitValue = 7;
    }
};

struct MultiperspectivePerceptronParams
{
    int numThreads = 1;
    int num_filter_entries;
    int num_local_histories;
    int local_history_length = 11;
    int block_size = 21;
    int pcshift = -10;
    int threshold = 1;
    int bias0 = -5;
    int bias1 = 5;
    int biasmostly0 = -1;
    int biasmostly1 = 1;
    int nbest = 20;
    int tunebits = 24;
    int hshift = -6;
    uint64_t imli_mask1;
    uint64_t imli_mask4;
    uint64_t recencypos_mask;
    float fudge = 0.245;
    int n_sign_bits = 2;
    int pcbit = 2;
    int decay = 0;
    int record_mask = 191;
    bool hash_taken = false;
    bool tuneonly = true;
    int extra_rounds = 1;
    int speed = 9;
    int initial_theta = 10;
    int budgetbits;
    bool speculative_update = false;
    int initial_ghist_length = 1;
    bool ignore_path_size = false;
};

struct MultiperspectivePerceptron64KBParams : public MultiperspectivePerceptronParams
{
    MultiperspectivePerceptron64KBParams()
    {
        budgetbits = 65536 * 8 + 2048;
        num_local_histories = 510;
        num_filter_entries = 18025;
        imli_mask1 = 0xc1000;
        imli_mask4 = 0x80008000;
        recencypos_mask = 0x100000090;
    }
};

}

#endif
