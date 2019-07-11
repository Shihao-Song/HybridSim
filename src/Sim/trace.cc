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

    const CPUTrace::MicroOp &micro_op = trace_file.micro_ops(instruction_index);

    inst.ready_to_commit = false;
    inst.eip = micro_op.eip();

    if (micro_op.opr() == CPUTrace::MicroOp::EXE)
    {
        inst.opr = Instruction::Operation::EXE;
    }
    else
    {
        if (micro_op.opr() == CPUTrace::MicroOp::LOAD) 
        {
            inst.opr = Instruction::Operation::LOAD;
        }

        if (micro_op.opr() == CPUTrace::MicroOp::STORE) 
        {
            inst.opr = Instruction::Operation::STORE;
        }
        inst.target_addr = micro_op.load_or_store_addr();
        inst.size = micro_op.size();
    }

    ++instruction_index;
    return true;
}

// TODO, support mem-trace feature as well.
bool Trace::getMemtraceRequest(Request &req)
{
    return true;
}
}
