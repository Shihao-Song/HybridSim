#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include <fstream>
#include "Sim/request.hh"


// This class should be the base class for all the memory component
namespace Simulator
{
class MemObject
{
  public:
    MemObject(){}

    virtual int pendingRequests() = 0;

    virtual bool send(Request &req) = 0;
    virtual void tick() {}
    virtual void setNextLevel(MemObject *) {}

    virtual void debugPrint(std::ofstream &out){}
};
}

#endif
