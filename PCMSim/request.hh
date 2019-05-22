#ifndef __PCMSIM_REQUEST_H__
#define __PCMSIM_REQUEST_H__

#include <cstdint> // Addr
#include <functional>
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

    // Request type
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

    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type)
    {}

    Request(Addr _addr, Request_Type _type,
            std::function<void(Request&)> _callback) :
        addr(_addr),
        req_type(_type),
        callback(_callback)
    {}
};
}
#endif
