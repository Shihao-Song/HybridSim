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
}

#endif
