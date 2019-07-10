#include "Sim/trace.hh"

namespace Simulator
{
Trace::Trace(const std::string trace_fname)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::ifstream input(trace_fname);
    if (!trace_file.ParseFromIstream(&input))
    {
        std::cerr << "Failed to parse trace file. \n";
        exit(0);
    }
}

bool Trace::getInstruction(Instruction &inst)
{
    if (instruction_index == trace_file.micro_ops_size())
    {
        google::protobuf::ShutdownProtobufLibrary();
        return false;
    }

/*
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

    inst.ready_to_commit = false;
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
*/
    return true;
}

// TODO, support mem-trace feature as well.
bool Trace::getMemtraceRequest(Request &req)
{
    return true;
}
}
