#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include <fstream>

#include "Sim/request.hh"
#include "Sim/stats.hh"

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
    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void setArbitrator(int _num_clients)
    {
        arbitrator = true;
        num_clients = _num_clients;
    }

    virtual void setBoundaryMemObject()
    {
        boundary = true;
    }

    virtual void registerStats(Stats &stats) {}

  protected:
    MemObject *next_level;

    int id = -1;

    bool arbitrator = false; // Am i the arbitrator.
    int num_clients = -1;
    int selected_client = 0; // Which client is allowed to pass.

    bool boundary; // I'm the boundary of my group.
};
}

#endif
