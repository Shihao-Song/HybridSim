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
struct ProtobufMode {}; // We will use google protofbuf in this mode
struct TXTMode {}; // Simply to parse a text file 
template<typename T>
class Trace
{
    typedef uint64_t Addr;

  public:
    Trace(const std::string trace_fname)
    {
        trace_name = trace_fname;
        if constexpr (std::is_same<ProtobufMode, T>::value)
        {
            GOOGLE_PROTOBUF_VERIFY_VERSION;

            std::ifstream input(trace_fname);
            if (!trace_file.ParseFromIstream(&input))
            {
                std::cerr << "Failed to parse trace file. \n";
                exit(0);
            }
        }

        if constexpr (std::is_same<TXTMode, T>::value)
        {
            trace_file_expr.open(trace_fname);
            assert(trace_file_expr.good());
        }
    }

    bool getInstruction(Instruction &inst)
    {
        if constexpr (std::is_same<ProtobufMode, T>::value)
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
                inst.target_vaddr = micro_op.load_or_store_addr();
                inst.size = micro_op.size();
            }

            ++instruction_index;
            return true;
        }

        if constexpr (std::is_same<TXTMode, T>::value)
        {
            if (profiling_stage && instruction_index == profiling_limit)
            {
                profiling_stage = false;
                trace_file_expr.clear();
                trace_file_expr.seekg(0, std::ios::beg);
                return false;
            }

            std::string line;
            getline(trace_file_expr, line);
            if (trace_file_expr.eof()) 
            {
                ++runs;
                if (runs == REPEAT)
                {
                    trace_file_expr.close();
                    return false;
                }
                trace_file_expr.clear();
                trace_file_expr.seekg(0, std::ios::beg);
                getline(trace_file_expr, line);
            }

            std::stringstream line_stream(line);
            std::vector<std::string> tokens;
            std::string intermidiate;
            while (getline(line_stream, intermidiate, ' '))
            {
                tokens.push_back(intermidiate);
            }
            assert(tokens.size());

            inst.ready_to_commit = false;
            inst.eip = std::stoull(tokens[0]);
            
            if (tokens[1] == "E")
            {
                inst.opr = Instruction::Operation::EXE;
            }
            else
            {
                if (tokens[1] == "L")
                {
                    inst.opr = Instruction::Operation::LOAD;
                }
                else if (tokens[1] == "S")
                {
                    inst.opr = Instruction::Operation::STORE;
                }
                else
                {
                    std::cerr << "Unsupported Instruction Type \n";
                    exit(0);
		}
                inst.target_vaddr = std::stoull(tokens[2]);
                inst.size = std::stoull(tokens[3]);
            }
            ++instruction_index;
            return true;
        }
    }

    bool getMemtraceRequest(Request &req)
    {
        std::string line;
        getline(trace_file_expr, line);
        if (trace_file_expr.eof())
        {
            ++runs;
            if (runs == REPEAT)
            {
                trace_file_expr.close();
                return false;
            }
            trace_file_expr.clear();
            trace_file_expr.seekg(0, std::ios::beg);
        }

        std::stringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string intermidiate;
        while (getline(line_stream, intermidiate, ' '))
        {
            tokens.push_back(intermidiate);
        }
        assert(tokens.size());

        req.addr = std::stoull(tokens[0]);
        if (tokens[1] == "R")
        {
            req.req_type = Request::Request_Type::READ;
        }
        else if (tokens[1] == "W")
        {
            req.req_type = Request::Request_Type::WRITE;
        }

        return true;
    }

    void profiling(uint64_t limit)
    {
        profiling_stage = true;
        profiling_limit = limit;
    }

    void disableProfiling()
    {
        profiling_stage = false;
    }

  private:
    CPUTrace::TraceFile trace_file;
    uint64_t instruction_index = 0;

    std::string trace_name;
    std::ifstream trace_file_expr;

    // Are we in profiling stage?
    bool profiling_stage = false;
    uint64_t profiling_limit = 0;

    const unsigned REPEAT = 2;
    unsigned runs = 0;
};

typedef Trace<ProtobufMode> ProtobufTrace;
typedef Trace<TXTMode> TXTTrace;
}
#endif
