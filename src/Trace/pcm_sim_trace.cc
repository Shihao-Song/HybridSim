#include "pcm_sim_trace.hh"

namespace PCMSim
{
Trace::Trace(const std::string trace_fname) : file(trace_fname),
                                              trace_name(trace_fname)
{
    assert(file.good());
}

bool Trace::getMemtraceRequest(Addr &req_addr, Request::Request_Type &req_type)
{
    std::string line;
    getline(file, line);
    if (file.eof()) 
    {
        file.close();
        return false;
    }

    size_t pos;

    try
    {
        req_addr = stoull(line, &pos, 0);
    }
    catch (std::out_of_range e)
    {
        req_addr = 0;
        req_type = Request::Request_Type::READ;

        return true;	
    }

    // Extract request type and data (for write request only)
    pos = line.find_first_not_of(' ', pos+1);

    if (pos == std::string::npos || line.substr(pos)[0] == 'R')
    {
	req_type = Request::Request_Type::READ;
    }
    else if (line.substr(pos)[0] == 'W')
    {
        req_type = Request::Request_Type::WRITE;
    }
    else 
    {
        file.close();
        return false;
        // assert(false);
    }

    return true;
}
}
