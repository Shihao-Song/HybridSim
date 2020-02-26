#ifndef __QOS_BASE_HH__
#define __QOS_BASE_HH__

#include <fstream>
#include <list>
#include <string>

/* Base class */
#include "Sim/mem_object.hh"

/* All policies */


namespace QoS
{
class QoSBase : public MemObject
{
  public:
    /* Bus Direction */
    enum BusState { READ, WRITE };

  protected:
    

  public:
    QoSBase()
    {
    }

    ~QoSBase() {}

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
