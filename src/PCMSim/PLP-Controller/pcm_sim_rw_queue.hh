#ifndef __PCM_SIM_READ_WRITE_QUEUE_HH__
#define __PCM_SIM_READ_WRITE_QUEUE_HH__

#include "../request.hh"
#include "../../Sim/config.hh"

namespace PCMSim
{
class Read_Write_Queue
{
    typedef Configuration::Config Config; 

  public:
    Read_Write_Queue() : queue_size(0) {}
    ~Read_Write_Queue() {}
    int size() const { return queue_size; }

    void push_back(Request &req)
    {
        // Assign OrderID
        req.OrderID = queue_size;

        // Update queue size
        queue_size++;

        // Push into queue
        queue.push_back(req);
    }

    void erase(std::list<Request>::iterator &req)
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

    std::list<Request> queue;

    const int max = 64;

    int queue_size;

    void updateOrderIDs()
    {
        // A negative order ID represents back-logged request
        for(std::list<Request>::iterator ite = queue.begin(); 
            ite != queue.end(); ++ite)
        {
            ite->OrderID = ite->OrderID - 1;
        }
    }
};
}
#endif
