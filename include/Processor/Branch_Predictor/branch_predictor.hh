#ifndef __BRANCH_PREDICTOR_HH__
#define __BRANCH_PREDICTOR_HH__

#include "Sim/instruction.hh"

class Branch_Predictor
{
  public:
    Branch_Predictor(unsigned _num_threads)
        : num_threads(_num_threads)
    {}

  protected:
    unsigned num_threads; // Some branch predictors need to maintain information
                          // for each thread.
};

#endif
