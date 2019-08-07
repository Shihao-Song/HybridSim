#ifndef __LAS_PCM_CONTROLLER_HH__
#define __LAS_PCM_CONTROLLER_HH__

#include "PCMSim/CP_Aware_Controller/cp_aware_controller.hh"

namespace PCMSim
{
// TODO, limitation, only 1-stage charging is supported so far.
class LASPCM : public CPAwareController 
{
  // We need an organized queue (to organize requests based on bank ID)
  // protected:
    

  public:
    LASPCM(int _id, Config &cfg)
        : CPAwareController(_id, cfg)
    {}
};
}

#endif
