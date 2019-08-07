#ifndef __REQUEST_HH__
#define __REQUEST_HH__

#include <cstdint> // Addr
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace Simulator
{
// Sometimes, certain memory objects need to communicate with the MMU.
class Request;
class MMUCommuPacket
{
  public:
    MMUCommuPacket(){}

    std::function<void(Request&)> callback = 0;
};

class Request
{
  public:
    typedef uint64_t Addr;
    typedef uint64_t Tick;

    int core_id;
    Addr eip; // Advanced feature, the instruction that caused this memory request;
    MMUCommuPacket mmu_commu; // Advanced feature
    void setMMUCommuFunct(auto func)
    {
        mmu_commu.callback = func;
    }
    auto getMMUCommuFunct()
    {
        return mmu_commu.callback;
    }
    auto commuToMMU()
    {
        if (mmu_commu.callback)
        {
            mmu_commu.callback(*this);
        }
    }

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
    }req_type;

    // clock cycle that request arrives to the queue
    Addr queue_arrival;

    // time to start execution
    Addr begin_exe;

    // estimated completion time
    Addr end_exe;

    // when OoO is enabled (track back-logging)
    int OrderID;

    // (general) call-back function
    std::function<bool(Addr)> callback;

    /* Constructors */
    Request() : addr(0), req_type(Request_Type::MAX)
    {}

    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type)
    {}

    Request(Addr _addr, Request_Type _type,
            std::function<bool(Addr)> _callback) :
        addr(_addr),
        req_type(_type),
        callback(_callback)
    {}
};

// TODO, move this PLP_Controller diectory
class PLPRequest : public Request
{
  public:
    PLPRequest() : Request() {}

    PLPRequest(Request &req) : Request()
    { 
        core_id = req.core_id;

        eip = req.eip;
        setMMUCommuFunct(req.getMMUCommuFunct());

        addr = req.addr;
        addr_vec = req.addr_vec;
        size = req.size;

        req_type = req.req_type;

        callback = req.callback;
    }

    /*  PLP (Partition-level Parallelism) Section */
    enum class Pairing_Type : int
    {
        RR, // Two reads scheduled in parallel
        RW, // One read and one write scheduled in parallel
        MAX
    }pair_type = Pairing_Type::MAX;

    // PLP related, master: request whose OrderID and queue arrival time are smaller
    int master = 0;
    // PLP related, slave: request that is scheduled with the master, this request
    // comes (into the queue) later than master
    int slave = 0;

    // PLP related, master maintains a pointer to slave
    std::list<PLPRequest>::iterator slave_req;
    // PLP related, slave maintains a pointer to master
    std::list<PLPRequest>::iterator master_req;
};

}
#endif
