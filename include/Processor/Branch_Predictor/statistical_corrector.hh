#ifndef __STATISTICAL_CORRECTOR_HH__
#define __STATISTICAL_CORRECTOR_HH__

#include "Processor/Branch_Predictor/branch_predictor.hh"
#include "Processor/Branch_Predictor/gem5_random.hh"
#include "Processor/Branch_Predictor/params.hh"

#include <vector>

namespace CoreSystem
{
class StatisticalCorrector : public Branch_Predictor
{
  protected:
    template<typename T>
    inline void ctrUpdate(T & ctr, bool taken, int nbits) {
        assert(nbits <= sizeof(T) << 3);
        if (nbits > 0) {
            if (taken) {
                if (ctr < ((1 << (nbits - 1)) - 1))
                    ctr++;
            } else {
                if (ctr > -(1 << (nbits - 1)))
                    ctr--;
            }
        }
    }
    // histories used for the statistical corrector
    struct SCThreadHistory {
        SCThreadHistory() {
            bwHist = 0;
            numOrdinalHistories = 0;
            imliCount = 0;
        }
        int64_t bwHist;  // backward global history
        int64_t imliCount;

        void setNumOrdinalHistories(unsigned num)
        {
            numOrdinalHistories = num;
            assert(num > 0);
            shifts.resize(num);
            localHistories = new std::vector<int64_t> [num];
        }

        void initLocalHistory(int ordinal, int numHistories, int shift)
        {
            assert((ordinal >= 1) && (ordinal <= numOrdinalHistories));
            shifts[ordinal - 1] = shift;
            // assert(isPowerOf2(numHistories));
            localHistories[ordinal - 1].resize(numHistories, 0);
        }

        int64_t getLocalHistory(int ordinal, Addr pc)
        {
            assert((ordinal >= 1) && (ordinal <= numOrdinalHistories));
            unsigned idx = ordinal - 1;
            return localHistories[idx][getEntry(pc, idx)];
        }

        void updateLocalHistory(
            int ordinal, Addr branch_pc, bool taken, Addr extraXor = 0)
        {
            assert((ordinal >= 1) && (ordinal <= numOrdinalHistories));
            unsigned idx = ordinal - 1;

            unsigned entry = getEntry(branch_pc, idx);
            int64_t hist =  (localHistories[idx][entry] << 1) + taken;

            if (extraXor) {
                hist = hist ^ extraXor;
            }

            localHistories[idx][entry] = hist;
        }

      private:
        std::vector<int64_t> * localHistories;
        std::vector<int> shifts;
        unsigned numOrdinalHistories;

        unsigned getEntry(Addr pc, unsigned idx)
        {
            return (pc ^ (pc >> shifts[idx])) & (localHistories[idx].size()-1);
        }
    };

    // For SC history we use global (i.e. not per thread) non speculative
    // histories, as some of the strucures needed are quite big and it is not
    // reasonable to make them per thread and it would be difficult to
    // rollback on miss-predictions
    SCThreadHistory * scHistory;

    const unsigned logBias;

    const unsigned logSizeUp;
    const unsigned logSizeUps;

    const unsigned numEntriesFirstLocalHistories;

    // global backward branch history GEHL
    const unsigned bwnb;
    const unsigned logBwnb;
    std::vector<int> bwm;
    std::vector<int8_t> * bwgehl;
    std::vector<int8_t> wbw;

    // First local history GEHL
    const unsigned lnb;
    const unsigned logLnb;
    std::vector<int> lm;
    std::vector<int8_t> * lgehl;
    std::vector<int8_t> wl;

    // IMLI GEHL
    const unsigned inb;
    const unsigned logInb;
    std::vector<int> im;
    std::vector<int8_t> * igehl;
    std::vector<int8_t> wi;

    std::vector<int8_t> bias;
    std::vector<int8_t> biasSK;
    std::vector<int8_t> biasBank;

    std::vector<int8_t> wb;

    int updateThreshold;
    std::vector<int> pUpdateThreshold;

    // The two counters used to choose between TAGE ang SC on High Conf
    // TAGE/Low Conf SC
    const unsigned chooserConfWidth;

    const unsigned updateThresholdWidth;
    const unsigned pUpdateThresholdWidth;

    const unsigned extraWeightsWidth;

    const unsigned scCountersWidth;

    int8_t firstH;
    int8_t secondH;

  public:
    typedef uint64_t ULL;
    typedef unsigned ThreadID;

    struct BranchInfo
    {
        BranchInfo() : lowConf(false), highConf(false), altConf(false),
              medConf(false), scPred(false), lsum(0), thres(0),
              predBeforeSC(false), usedScPred(false)
        {}

        // confidences calculated on tage and used on the statistical
        // correction
        bool lowConf;
        bool highConf;
        bool altConf;
        bool medConf;

        bool scPred;
        int lsum;
        int thres;
        bool predBeforeSC;
        bool usedScPred;
    };

    StatisticalCorrector(StatisticalCorrectorParams *p) :
    logBias(p->logBias),
    logSizeUp(p->logSizeUp),
    logSizeUps(logSizeUp / 2),
    numEntriesFirstLocalHistories(p->numEntriesFirstLocalHistories),
    bwnb(p->bwnb),
    logBwnb(p->logBwnb),
    bwm(p->bwm),
    lnb(p->lnb),
    logLnb(p->logLnb),
    lm(p->lm),
    inb(p->inb),
    logInb(p->logInb),
    im(p->im),
    chooserConfWidth(p->chooserConfWidth),
    updateThresholdWidth(p->updateThresholdWidth),
    pUpdateThresholdWidth(p->pUpdateThresholdWidth),
    extraWeightsWidth(p->extraWeightsWidth),
    scCountersWidth(p->scCountersWidth),
    firstH(0),
    secondH(0)
    {
    wb.resize(1 << logSizeUps, 4);

    initGEHLTable(lnb, lm, lgehl, logLnb, wl, p->lWeightInitValue);
    initGEHLTable(bwnb, bwm, bwgehl, logBwnb, wbw, p->bwWeightInitValue);
    initGEHLTable(inb, im, igehl, logInb, wi, p->iWeightInitValue);

    updateThreshold = 35 << 3;

    pUpdateThreshold.resize(1 << logSizeUp, p->initialUpdateThresholdValue);

    bias.resize(1 << logBias);
    biasSK.resize(1 << logBias);
    biasBank.resize(1 << logBias);

    init();
    }

    virtual BranchInfo *makeBranchInfo()
    {
    return new BranchInfo();
    }
    virtual SCThreadHistory *makeThreadHistory()
    {
    return new SCThreadHistory();
    }

    virtual void initBias()
    {
    for (int j = 0; j < (1 << logBias); j++) {
        switch (j & 3) {
          case 0:
            bias[j] = -32;
            biasSK[j] = -8;
            biasBank[j] = -32;
            break;
          case 1:
            bias[j] = 31;
            biasSK[j] = 7;
            biasBank[j] = 31;
            break;
          case 2:
            bias[j] = -1;
            biasSK[j] = -32;
            biasBank[j] = -1;
            break;
          case 3:
            bias[j] = 0;
            biasSK[j] = 31;
            biasBank[j] = 0;
            break;
        }
    }
    }

    virtual bool scPredict(
        ThreadID tid, Addr branch_pc, bool cond_branch, BranchInfo* bi,
        bool prev_pred_taken, bool bias_bit, bool use_conf_ctr,
        int8_t conf_ctr, unsigned conf_bits, int hitBank, int altBank,
        int64_t phist, int init_lsum = 0)
    {
    bool pred_taken = prev_pred_taken;
    if (cond_branch) {

        bi->predBeforeSC = prev_pred_taken;

        // first calc/update the confidences from the TAGE prediction
        if (use_conf_ctr) {
            bi->lowConf = (abs(2 * conf_ctr + 1) == 1);
            bi->medConf = (abs(2 * conf_ctr + 1) == 5);
            bi->highConf = (abs(2 * conf_ctr + 1) >= (1<<conf_bits) - 1);
        }

        int lsum = init_lsum;

        int8_t ctr = bias[getIndBias(branch_pc, bi, bias_bit)];
        lsum += (2 * ctr + 1);
        ctr = biasSK[getIndBiasSK(branch_pc, bi)];
        lsum += (2 * ctr + 1);
        ctr = biasBank[getIndBiasBank(branch_pc, bi, hitBank, altBank)];
        lsum += (2 * ctr + 1);

        lsum = (1 + (wb[getIndUpds(branch_pc)] >= 0)) * lsum;

        int thres = gPredictions(tid, branch_pc, bi, lsum, phist);

        // These will be needed at update time
        bi->lsum = lsum;
        bi->thres = thres;

        bool scPred = (lsum >= 0);

        if (pred_taken != scPred) {
            bool useScPred = true;
            //Choser uses TAGE confidence and |LSUM|
            if (bi->highConf) {
                if (abs (lsum) < (thres / 4)) {
                    useScPred = false;
                } else if (abs (lsum) < (thres / 2)) {
                    useScPred = (secondH < 0);
                }
            }

            if (bi->medConf) {
                if (abs (lsum) < (thres / 4)) {
                    useScPred = (firstH < 0);
                }
            }

            bi->usedScPred = useScPred;
            if (useScPred) {
                pred_taken = scPred;
                bi->scPred = scPred;
            }
        }
    }

    return pred_taken;
    }

    virtual unsigned getIndBias(Addr branch_pc, BranchInfo* bi, bool bias) const
    {
    return (((((branch_pc ^(branch_pc >>2))<<1) ^ (bi->lowConf & bias)) <<1)
            +  bi->predBeforeSC) & ((1<<logBias) -1);
    }

    virtual unsigned getIndBiasSK(Addr branch_pc, BranchInfo* bi) const
    {
    return (((((branch_pc ^ (branch_pc >> (logBias-2)))<<1) ^
           (bi->highConf))<<1) + bi->predBeforeSC) & ((1<<logBias) -1);
    }

    virtual unsigned getIndBiasBank( Addr branch_pc, BranchInfo* bi,
        int hitBank, int altBank) const = 0;

    virtual unsigned getIndUpd(Addr branch_pc) const
    {
    return ((branch_pc ^ (branch_pc >>2)) & ((1 << (logSizeUp)) - 1));
    }
    unsigned getIndUpds(Addr branch_pc) const
    {
    return ((branch_pc ^ (branch_pc >>2)) & ((1 << (logSizeUps)) - 1));
    }

    virtual int gPredictions(ThreadID tid, Addr branch_pc, BranchInfo* bi,
        int & lsum, int64_t phist) = 0;

    int64_t gIndex(Addr branch_pc, int64_t bhist, int logs, int nbr, int i)
    {
    return (((int64_t) branch_pc) ^ bhist ^ (bhist >> (8 - i)) ^
            (bhist >> (16 - 2 * i)) ^ (bhist >> (24 - 3 * i)) ^
            (bhist >> (32 - 3 * i)) ^ (bhist >> (40 - 4 * i))) &
           ((1 << (logs - gIndexLogsSubstr(nbr, i))) - 1);
    }

    virtual int gIndexLogsSubstr(int nbr, int i) = 0;

    int gPredict(
        Addr branch_pc, int64_t hist, std::vector<int> & length,
        std::vector<int8_t> * tab, int nbr, int logs,
        std::vector<int8_t> & w)
    {
    int percsum = 0;
    for (int i = 0; i < nbr; i++) {
        int64_t bhist = hist & ((int64_t) ((1 << length[i]) - 1));
        int64_t index = gIndex(branch_pc, bhist, logs, nbr, i);
        int8_t ctr = tab[i][index];
        percsum += (2 * ctr + 1);
    }
    percsum = (1 + (w[getIndUpds(branch_pc)] >= 0)) * percsum;
    return percsum;
    }

    virtual void gUpdate(
        Addr branch_pc, bool taken, int64_t hist, std::vector<int> & length,
        std::vector<int8_t> * tab, int nbr, int logs,
        std::vector<int8_t> & w, BranchInfo* bi)
    {
    int percsum = 0;
    for (int i = 0; i < nbr; i++) {
        int64_t bhist = hist & ((int64_t) ((1 << length[i]) - 1));
        int64_t index = gIndex(branch_pc, bhist, logs, nbr, i);
        percsum += (2 * tab[i][index] + 1);
        ctrUpdate(tab[i][index], taken, scCountersWidth);
    }

    int xsum = bi->lsum - ((w[getIndUpds(branch_pc)] >= 0)) * percsum;
    if ((xsum + percsum >= 0) != (xsum >= 0)) {
        ctrUpdate(w[getIndUpds(branch_pc)], ((percsum >= 0) == taken),
                  extraWeightsWidth);
    }
    }

    void initGEHLTable(
        unsigned numLenghts, std::vector<int> lengths,
        std::vector<int8_t> * & table, unsigned logNumEntries,
        std::vector<int8_t> & w, int8_t wInitValue)
    {
    assert(lengths.size() == numLenghts);
    if (numLenghts == 0) {
        return;
    }
    table = new std::vector<int8_t> [numLenghts];
    for (int i = 0; i < numLenghts; ++i) {
        table[i].resize(1 << logNumEntries, 0);
        for (int j = 0; j < ((1 << logNumEntries) - 1); ++j) {
            if (! (j & 1)) {
                table[i][j] = -1;
            }
        }
    }

    w.resize(1 << logSizeUps, wInitValue);
    }

    virtual void scHistoryUpdate(
        Addr branch_pc, /*const StaticInstPtr &inst, */bool taken,
        BranchInfo * tage_bi, Addr corrTarget)
    {
    // int brtype = inst->isDirectCtrl() ? 0 : 2;
    // if (! inst->isUncondCtrl()) {
    //     ++brtype;
    // }
    // Non speculative SC histories update
    // if (brtype & 1) {
        if (corrTarget < branch_pc) {
            //This branch corresponds to a loop
            if (!taken) {
                //exit of the "loop"
                scHistory->imliCount = 0;
            } else {
                if (scHistory->imliCount < ((1 << im[0]) - 1)) {
                    scHistory->imliCount++;
                }
            }
        }

        scHistory->bwHist = (scHistory->bwHist << 1) +
                                (taken & (corrTarget < branch_pc));
        scHistory->updateLocalHistory(1, branch_pc, taken);
    // }
    }

    virtual void gUpdates( ThreadID tid, Addr pc, bool taken, BranchInfo* bi,
        int64_t phist) = 0;

    void init()
    {
    scHistory = makeThreadHistory();
    initBias();
    }
    // void regStats() override;
    // void updateStats(bool taken, BranchInfo *bi);

    virtual void condBranchUpdate(ThreadID tid, Addr branch_pc, bool taken,
                          BranchInfo *bi, Addr corrTarget, bool b,
                          int hitBank, int altBank, int64_t phist)
    {
    bool scPred = (bi->lsum >= 0);

    if (bi->predBeforeSC != scPred) {
        if (abs(bi->lsum) < bi->thres) {
            if (bi->highConf) {
                if ((abs(bi->lsum) < bi->thres / 2)) {
                    if ((abs(bi->lsum) >= bi->thres / 4)) {
                        ctrUpdate(secondH, (bi->predBeforeSC == taken),
                                  chooserConfWidth);
                    }
                }
            }
        }
        if (bi->medConf) {
            if ((abs(bi->lsum) < bi->thres / 4)) {
                ctrUpdate(firstH, (bi->predBeforeSC == taken),
                          chooserConfWidth);
            }
        }
    }
    if ((scPred != taken) || ((abs(bi->lsum) < bi->thres))) {
        ctrUpdate(updateThreshold, (scPred != taken), updateThresholdWidth);
        ctrUpdate(pUpdateThreshold[getIndUpd(branch_pc)], (scPred != taken),
                  pUpdateThresholdWidth);

        unsigned indUpds = getIndUpds(branch_pc);
        unsigned indBias = getIndBias(branch_pc, bi, b);
        unsigned indBiasSK = getIndBiasSK(branch_pc, bi);
        unsigned indBiasBank = getIndBiasBank(branch_pc, bi, hitBank, altBank);

        int xsum = bi->lsum -
                      ((wb[indUpds] >= 0) * ((2 * bias[indBias] + 1) +
                          (2 * biasSK[indBiasSK] + 1) +
                          (2 * biasBank[indBiasBank] + 1)));
	if ((xsum + ((2 * bias[indBias] + 1) + (2 * biasSK[indBiasSK] + 1) +
            (2 * biasBank[indBiasBank] + 1)) >= 0) != (xsum >= 0))
        {
            ctrUpdate(wb[indUpds],
                      (((2 * bias[indBias] + 1) +
                        (2 * biasSK[indBiasSK] + 1) +
                        (2 * biasBank[indBiasBank] + 1) >= 0) == taken),
                      extraWeightsWidth);
        }

        ctrUpdate(bias[indBias], taken, scCountersWidth);
        ctrUpdate(biasSK[indBiasSK], taken, scCountersWidth);
        ctrUpdate(biasBank[indBiasBank], taken, scCountersWidth);

        gUpdates(tid, branch_pc, taken, bi, phist);
    }
    }
    // virtual size_t getSizeInBits() const;
};

class TAGE_SC_L_64KB_StatisticalCorrector : public StatisticalCorrector
{
    const unsigned numEntriesSecondLocalHistories;
    const unsigned numEntriesThirdLocalHistories;

    // global branch history variation GEHL
    const unsigned pnb;
    const unsigned logPnb;
    std::vector<int> pm;
    std::vector<int8_t> * pgehl;
    std::vector<int8_t> wp;

    // Second local history GEHL
    const unsigned snb;
    const unsigned logSnb;
    std::vector<int> sm;
    std::vector<int8_t> * sgehl;
    std::vector<int8_t> ws;

    // Third local history GEHL
    const unsigned tnb;
    const unsigned logTnb;
    std::vector<int> tm;
    std::vector<int8_t> * tgehl;
    std::vector<int8_t> wt;

    // Second IMLI GEHL
    const unsigned imnb;
    const unsigned logImnb;
    std::vector<int> imm;
    std::vector<int8_t> * imgehl;
    std::vector<int8_t> wim;

    struct SC_64KB_ThreadHistory : public SCThreadHistory
    {
        std::vector<int64_t> imHist;
    };

    SCThreadHistory *makeThreadHistory() override
    {
    SC_64KB_ThreadHistory *sh = new SC_64KB_ThreadHistory();

    sh->setNumOrdinalHistories(3);
    sh->initLocalHistory(1, numEntriesFirstLocalHistories, 2);
    sh->initLocalHistory(2, numEntriesSecondLocalHistories, 5);
    sh->initLocalHistory(3, numEntriesThirdLocalHistories, logTnb);

    sh->imHist.resize(1 << im[0]);
    return sh;
    }

  public:
    TAGE_SC_L_64KB_StatisticalCorrector(TAGE_SC_L_64KB_StatisticalCorrectorParams *p)
    : StatisticalCorrector(p),
    numEntriesSecondLocalHistories(p->numEntriesSecondLocalHistories),
    numEntriesThirdLocalHistories(p->numEntriesThirdLocalHistories),
    pnb(p->pnb),
    logPnb(p->logPnb),
    pm(p->pm),
    snb(p->snb),
    logSnb(p->logSnb),
    sm(p->sm),
    tnb(p->tnb),
    logTnb(p->logTnb),
    tm(p->tm),
    imnb(p->imnb),
    logImnb(p->logImnb),
    imm(p->imm)
    {
    initGEHLTable(pnb, pm, pgehl, logPnb, wp, 7);
    initGEHLTable(snb, sm, sgehl, logSnb, ws, 7);
    initGEHLTable(tnb, tm, tgehl, logTnb, wt, 7);
    initGEHLTable(imnb, imm, imgehl, logImnb, wim, 0);
    }

    unsigned getIndBiasBank(Addr branch_pc, BranchInfo* bi, int hitBank,
        int altBank) const override
    {
    return (bi->predBeforeSC + (((hitBank+1)/4)<<4) + (bi->highConf<<1) +
            (bi->lowConf <<2) + ((altBank!=0)<<3) +
            ((branch_pc^(branch_pc>>2))<<7)) & ((1<<logBias) -1);
    }

    int gPredictions(ThreadID tid, Addr branch_pc, BranchInfo* bi,
                     int & lsum, int64_t pathHist) override
    {
    SC_64KB_ThreadHistory *sh =
        static_cast<SC_64KB_ThreadHistory *>(scHistory);

    lsum += gPredict(
        (branch_pc << 1) + bi->predBeforeSC, sh->bwHist, bwm,
        bwgehl, bwnb, logBwnb, wbw);

    lsum += gPredict(
        branch_pc, pathHist, pm, pgehl, pnb, logPnb, wp);

    lsum += gPredict(
        branch_pc, sh->getLocalHistory(1, branch_pc), lm,
        lgehl, lnb, logLnb, wl);

    lsum += gPredict(
        branch_pc, sh->getLocalHistory(2, branch_pc), sm,
        sgehl, snb, logSnb, ws);

    lsum += gPredict(
        branch_pc, sh->getLocalHistory(3, branch_pc), tm,
        tgehl, tnb, logTnb, wt);

    lsum += gPredict(
        branch_pc, sh->imHist[scHistory->imliCount], imm,
        imgehl, imnb, logImnb, wim);

    lsum += gPredict(
        branch_pc, sh->imliCount, im, igehl, inb, logInb, wi);

    int thres = (updateThreshold>>3) + pUpdateThreshold[getIndUpd(branch_pc)]
      + 12*((wb[getIndUpds(branch_pc)] >= 0) + (wp[getIndUpds(branch_pc)] >= 0)
      + (ws[getIndUpds(branch_pc)] >= 0) + (wt[getIndUpds(branch_pc)] >= 0)
      + (wl[getIndUpds(branch_pc)] >= 0) + (wbw[getIndUpds(branch_pc)] >= 0)
      + (wi[getIndUpds(branch_pc)] >= 0));

    return thres;
    }

    int gIndexLogsSubstr(int nbr, int i) override
    {
    return (i >= (nbr - 2)) ? 1 : 0;
    }

    void scHistoryUpdate(Addr branch_pc, /*const StaticInstPtr &inst, */bool taken,
                         BranchInfo * tage_bi, Addr corrTarget) override
    {
        SC_64KB_ThreadHistory *sh =
            static_cast<SC_64KB_ThreadHistory *>(scHistory);
        int64_t imliCount = sh->imliCount;
        sh->imHist[imliCount] = (sh->imHist[imliCount] << 1)
                                + taken;
        sh->updateLocalHistory(2, branch_pc, taken, branch_pc & 15);
        sh->updateLocalHistory(3, branch_pc, taken);

        StatisticalCorrector::scHistoryUpdate(branch_pc, taken, tage_bi, corrTarget);
    }

    void gUpdates(ThreadID tid, Addr pc, bool taken, BranchInfo* bi,
            int64_t phist) override
    {
    SC_64KB_ThreadHistory *sh =
        static_cast<SC_64KB_ThreadHistory *>(scHistory);

    gUpdate((pc << 1) + bi->predBeforeSC, taken, sh->bwHist, bwm,
            bwgehl, bwnb, logBwnb, wbw, bi);

    gUpdate(pc, taken, phist, pm,
            pgehl, pnb, logPnb, wp, bi);

    gUpdate(pc, taken, sh->getLocalHistory(1, pc), lm,
            lgehl, lnb, logLnb, wl, bi);

    gUpdate(pc, taken, sh->getLocalHistory(2, pc), sm,
            sgehl, snb, logSnb, ws, bi);

    gUpdate(pc, taken, sh->getLocalHistory(3, pc), tm,
            tgehl, tnb, logTnb, wt, bi);

    gUpdate(pc, taken, sh->imHist[scHistory->imliCount], imm,
            imgehl, imnb, logImnb, wim, bi);

    gUpdate(pc, taken, sh->imliCount, im,
            igehl, inb, logInb, wi, bi);
    }
};

}
#endif//__CPU_PRED_STATISTICAL_CORRECTOR_HH
