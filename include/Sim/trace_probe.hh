#ifndef __TRACE_PROBE_HH__
#define __TRACE_PROBE_HH__

#include <fstream>
#include <list>
#include <string>

#include "Sim/request.hh"

namespace Simulator
{
class TraceProbe : public MemObject
{
  public:

    TraceProbe(std::string trace_file)
    {
        trace_name = trace_file;

        trace_out.open(trace_name);
        assert(trace_out.good());
    }

    ~TraceProbe() { trace_out << std::flush; trace_out.close(); }

    int pendingRequests() override
    {
        return q.size();
    }

    bool send(Request &req) override
    {
        // Print out
        trace_out << req.core_id << " "
                  << req.addr << " ";
        if (req.req_type == Request::Request_Type::READ)
        {
            trace_out << "R";
        }
        else if (req.req_type == Request::Request_Type::WRITE)
        {
            trace_out << "W";
        }
        trace_out << std::endl;

        q.push_back(req);
        return true;
    }

    void tick() override
    {
        if (!q.size()) { return; }

        auto req = q.begin();
        if (req->callback)
        {
            if (req->callback(*req))
            {
                q.erase(req);
            }
        }
        else
        {
            q.erase(req);
        }
    }

  protected:
    std::list<Request> q;

  protected:
    std::string trace_name;
    std::ofstream trace_out;
};
}

#endif
