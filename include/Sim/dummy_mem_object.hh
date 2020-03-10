#ifndef __DUMMY_MEM_OBJ_HH__
#define __DUMMY_MEM_OBJ_HH__

#include <fstream>
#include <list>
#include <string>

#include "Sim/request.hh"

namespace Simulator
{
class DummyMemObject : public MemObject
{
  public:

    DummyMemObject()
    {
    }

    int pendingRequests() override
    {
        return 0;
    }

    bool send(Request &req) override
    {
        return true;
    }

    void tick() override
    {
    
    }
};
}
#endif
