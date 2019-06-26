#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include "Sim/request.hh"

// This class should be the base class for all the memory component
namespace Simulator
{
class MemObject
{
  public:
    MemObject(){}

    virtual bool send(Request &req) = 0;
    virtual void tick() {}
};
}

#endif
