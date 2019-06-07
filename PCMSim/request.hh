#ifndef __PCMSIM_REQUEST_H__
#define __PCMSIM_REQUEST_H__

#include <cstdint> // Addr
#include <functional>
#include <list>
#include <memory>
#include <vector>

typedef uint64_t Addr;
typedef uint64_t Tick;

const Addr MaxAddr = (Addr)-1;

namespace PCMSim
{
class Request
{
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
    std::function<void(Request&)> callback;

    /* when PLP (Partition-level Paral.) is enabled */
    int OrderID;

    enum class Pairing_Type : int
    {
        RR, // Two reads scheduled in parallel
        RW, // One read and one write scheduled in parallel
        MAX
    }pair_type;
	
    int master = 0;
    int slave = 0;

    std::list<Request>::iterator slave_req;

    std::list<Request>::iterator master_req;

    // Very important
    std::function<void(Request&)> slave_callback;
    Addr slave_addr;

    /* Constructors */
    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type),
        pair_type(Pairing_Type::MAX)
    {}

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
