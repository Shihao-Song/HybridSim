#ifndef __PCMSIM_PLPQUEUE_HH__
#define __PCMSIM_PLPQUEUE_HH__

#include "Sim/request.hh"
#include "Sim/config.hh"

namespace PCMSim
{
class PLPReqQueue
{
  public:
    typedef Simulator::Config Config; 
    typedef Simulator::PLPRequest PLPRequest;

  public:
    PLPReqQueue() : queue_size(0) {}
    ~PLPReqQueue() {}
    int size() const { return queue_size; }

    void push_back(PLPRequest &req)
    {
        // Assign OrderID
        req.OrderID = queue_size;

        // Update queue size
        queue_size++;

        // Push into queue
        queue.push_back(req);
    }

    void erase(std::list<PLPRequest>::iterator &req)
    {
        int slave = req->slave;

        // pop out of queue
        queue.erase(req);

        // Update queue size
        queue_size--;

        // Update OrderIDs (only when master gets popped out)
        if (slave != 1)
        {
            updateOrderIDs();
        }
    }

    std::list<PLPRequest> queue;

    const int max = 64;

    int queue_size;

    void updateOrderIDs()
    {
        // A negative order ID represents back-logged request
        for(std::list<PLPRequest>::iterator ite = queue.begin(); 
            ite != queue.end(); ++ite)
        {
            ite->OrderID = ite->OrderID - 1;
        }
    }
};
}
#endif
