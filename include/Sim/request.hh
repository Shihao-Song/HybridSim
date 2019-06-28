#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <cstdint> // Addr
#include <functional>
#include <list>
#include <memory>
#include <vector>

/*
const Addr MaxAddr = (Addr)-1;
*/

namespace Simulator
{
class Request
{
    typedef uint64_t Addr;
    typedef uint64_t Tick;

  public:
    Addr addr;

    std::vector<int> addr_vec;

    // (Memory) request type
    enum class Request_Type : int
    {
        READ,
        WRITE,
        MAX
    }req_type;

    // clock cycle that request arrives to the queue
    Addr queue_arrival;

    // time to start execution
    Addr begin_exe;

    // estimated completion time
    Addr end_exe;

    // call-back function
    // TODO, this may need to be updated as a vector of callbacks
    // because of multiple layers of memory (cache).
    std::function<void(Request&)> callback;

    // TODO, should have something like Builder or Factory to create different
    // types of Request object.

    // when OoO is enabled
    int OrderID;

    /*  PLP (Partition-level Parallelism) Section */
    enum class Pairing_Type : int
    {
        RR, // Two reads scheduled in parallel
        RW, // One read and one write scheduled in parallel
        MAX
    }pair_type;

    // PLP related, master: request whose OrderID and queue arrival time are smaller
    int master = 0;
    // PLP related, slave: request that is scheduled with the master, this request
    // comes (into the queue) later than master
    int slave = 0;

    // PLP related, master maintains a pointer to slave
    std::list<Request>::iterator slave_req;
    // PLP related, slave maintains a pointer to master
    std::list<Request>::iterator master_req;

    /* Constructors */
    Request() : addr(0), req_type(Request_Type::MAX)
    { }

    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type),
        pair_type(Pairing_Type::MAX)
    {}

    // TODO, callback should be pushed into the vector
    Request(Addr _addr, Request_Type _type,
            std::function<void(Request&)> _callback) :
        addr(_addr),
        req_type(_type),
        callback(_callback),
        pair_type(Pairing_Type::MAX)
    {}
};
}
#endif
