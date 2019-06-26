#ifndef __CLOCKED_OBJECT_HH__
#define __CLOCKED_OBJECT_HH__

#include "Sim/request.hh"

// This class should be the base class for all the memory component
namespace Simulator
{
class Clocked_Object
{
  public:
    Clocked_Object(){}

    virtual bool send(Request &req) = 0;
    virtual void tick() {}
};
}

#endif
