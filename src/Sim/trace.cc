#include "Sim/trace.hh"

namespace Simulator
{
Trace::Trace(const std::string trace_fname) : file(trace_fname),
                                              trace_name(trace_fname)
{
    assert(file.good());
}

bool Trace::getInstruction(Instruction &inst)
{
    std::string line;
    getline(file, line);
    if (file.eof())
    {
        // Comment section: in the future, we may need to run the same
        // trace file for multiple times.
        // file.clear();
        // file.seekg(0, file.beg);
        // getline(file, line);
        file.close();
        return false; // tmp implementation, only run once
    }

    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter(tokens));

    assert(tokens.size() != 0);

    if (tokens.size() == 2)
    {
        inst.opr = Instruction::Operation::EXE;
        inst.eip = std::stoull(tokens[1], NULL, 10);
        inst.target_addr = Addr(0) - 1;
    }
    else
    {
        if (tokens[0] == "Load")
        {
            inst.opr = Instruction::Operation::LOAD;
        }
        else if (tokens[0] == "Store")
        {
            inst.opr = Instruction::Operation::STORE;
        }
        else
        {
            std::cerr << "Unsupported operation. \n";
        }
        inst.eip = std::stoull(tokens[1], NULL, 10);
        inst.target_addr = std::stoull(tokens[2], NULL, 10);
    }

    return true;
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
