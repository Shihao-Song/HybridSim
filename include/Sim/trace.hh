#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "protobuf/cpu_trace.pb.h"
#include "Sim/instruction.hh"
#include "Sim/request.hh"

namespace Simulator
{
struct WorkloadEvalMode {}; // We will use google protofbuf in this mode
struct NormalMode {}; // Simply to parse a text file 
template<typename T>
class Trace
{
    typedef uint64_t Addr;

  public:
    Trace(const std::string trace_fname)
    {
        if constexpr (std::is_same<WorkloadEvalMode, T>::value)
        {
            GOOGLE_PROTOBUF_VERIFY_VERSION;

            std::ifstream input(trace_fname);
            if (!trace_file.ParseFromIstream(&input))
            {
                std::cerr << "Failed to parse trace file. \n";
                exit(0);
            }
        }
    }

    bool getInstruction(Instruction &inst)
    {
        if constexpr (std::is_same<WorkloadEvalMode, T>::value)
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
    }

    bool getMemtraceRequest(Request &req)
    {
        return false;
    }

  private:
    CPUTrace::TraceFile trace_file;
    uint64_t instruction_index = 0;
};

typedef Trace<WorkloadEvalMode> TraceEval;
typedef Trace<NormalMode> TraceNormal;
}
#endif
