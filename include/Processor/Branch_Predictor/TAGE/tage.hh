/*
 * Copyright (c) 2014 The University of Wisconsin
 *
 * Copyright (c) 2006 INRIA (Institut National de Recherche en
 * Informatique et en Automatique  / French National Research Institute
 * for Computer Science and Applied Mathematics)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Vignyan Reddy, Dibakar Gope and Arthur Perais,
 * from André Seznec's code.
 */

/* @file
 * Implementation of a TAGE branch predictor. TAGE is a global-history based
 * branch predictor. It features a PC-indexed bimodal predictor and N
 * partially tagged tables, indexed with a hash of the PC and the global
 * branch history. The different lengths of global branch history used to
 * index the partially tagged tables grow geometrically. A small path history
 * is also used in the hash.
 *
 * All TAGE tables are accessed in parallel, and the one using the longest
 * history that matches provides the prediction (some exceptions apply).
 * Entries are allocated in components using a longer history than the
 * one that predicted when the prediction is incorrect.
 */

#ifndef __CPU_PRED_TAGE
#define __CPU_PRED_TAGE

#include <vector>

#include "Processor/Branch_Predictor/TAGE/tage_base.hh"

namespace CoreSystem
{
class TAGEParams;
class TAGE: public Branch_Predictor
{
  protected:
    Random random_mt;

    TAGEBase *tage;

    struct TageBranchInfo {
        TAGEBase::BranchInfo *tageBranchInfo;

        TageBranchInfo(TAGEBase &tage) : tageBranchInfo(tage.makeBranchInfo())
        {}

        virtual ~TageBranchInfo()
        {
            delete tageBranchInfo;
        }
    };

    
  public:

    TAGE(const TAGEParams *params);
    // ~TAGE() { delete tage; }

    bool predict(Instruction &instr) override;

  protected:

    bool predict(ThreadID tid, Addr branch_pc, bool cond_branch,
                         void* &b);

    // Base class methods.
    bool lookup(ThreadID tid, Addr branch_addr, void* &bp_history);
    void update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
                bool squashed, Addr corrTarget = MaxAddr);
};
}
#endif // __CPU_PRED_TAGE
