#ifndef __PARAMS_HH__
#define __PARAMS_HH__

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

}

#endif
