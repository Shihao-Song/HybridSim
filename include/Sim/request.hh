#ifndef __REQUEST_HH__
#define __REQUEST_HH__

#include <cstdint> // Addr
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace Simulator
{
class Request;
class Request
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    int core_id; // Which core issues this request.

    Addr eip; // PC that issues this request.

    bool mig = false; // If this is a migration request.

    Addr v_addr;
    Addr addr; // The address we are trying to read or write

    // Size of data to be loaded (read) or stored (written)
    // We are not utilizing this field currently.
    uint64_t size;

    std::vector<int> addr_vec;

    // (Memory) request type
    enum class Request_Type : int
    {
        READ,
        WRITE,
        WRITE_BACK,
        MAX
    }req_type = Request_Type::MAX;

    bool isWrite() { return req_type == Request_Type::WRITE; }
    bool isRead() { return req_type == Request_Type::READ; }

    // Hitwhere, which level of memory it hits.
    enum class Hitwhere : int
    {
        L1_D_Clean,
        L1_D_Dirty,
        L2_Clean,
        L2_Dirty,
        L3_Clean,
        L3_Dirty,
        MAX
    }hitwhere = Hitwhere::MAX;

    // clock cycle that request arrives to the queue (in main memory)
    Addr queue_arrival;

    // time to start execution (in main memory)
    Addr begin_exe;

    // estimated completion time (in main memory)
    Addr end_exe;

    // when OoO is enabled (track back-logging) (in main memory)
    int OrderID;

    // (general) call-back function
    std::function<bool(Request&)> callback;

    /* Constructors */
    Request() : addr(0), req_type(Request_Type::MAX)
    {}

    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type)
    {}

    Request(Addr _addr, Request_Type _type,
            std::function<bool(Request&)> _callback) :
        addr(_addr),
        req_type(_type),
        callback(_callback)
    {}
};
}
#endif
