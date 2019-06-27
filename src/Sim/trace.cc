#include "Sim/trace.hh"

namespace Simulator
{
Trace::Trace(const std::string trace_fname) : file(trace_fname),
                                              trace_name(trace_fname)
{
    assert(file.good());
}

bool Trace::getMemtraceRequest(Request &req)
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
        req.addr = stoull(line, &pos, 0);
    }
    catch (std::out_of_range e)
    {
        req.addr = 0;
        req.req_type = Request::Request_Type::READ;

        return true;	
    }

    pos = line.find_first_not_of(' ', pos+1);

    if (line.substr(pos)[0] == 'R')
    {
	req.req_type = Request::Request_Type::READ;
    }
    else if (line.substr(pos)[0] == 'W')
    {
        req.req_type = Request::Request_Type::WRITE;
    }
    else 
    {
        file.close();
        std::cerr << "Corrupted trace file. \n";
        exit(0);
    }

    return true;
}
}
