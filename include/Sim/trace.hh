#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "Sim/instruction.hh"
#include "Sim/request.hh"

namespace Simulator
{
class Trace
{
    typedef uint64_t Addr;

  public:
    Trace(const std::string trace_fname)
    {
        trace_name = trace_fname;
        
        trace_file_expr.open(trace_fname);
        assert(trace_file_expr.good());
    }

    bool getInstruction(Instruction &inst)
    {
        inst.ready_to_commit = false;
        inst.already_translated = false;

        // Drain all the pending exes and pending mem_ops
        if (pending_exes > 0)
        {
            inst.opr = Instruction::Operation::EXE;
            --pending_exes;

            ++instruction_index;
//            std::cout << "E\n";
            return true;
        }
        if (pending_mem_opr != Instruction::Operation::MAX)
        {
            inst.eip = pending_mem_opr_pc;
            inst.opr = pending_mem_opr;
            inst.target_vaddr = pending_mem_vaddr;

            pending_mem_opr = Instruction::Operation::MAX; // Re-initialize
            ++instruction_index;
//            std::cout << "M\n";
            return true;
        }

        // if (instruction_index >= 10000000) { trace_file_expr.close(); return false; }
        
        // if (profiling_stage && instruction_index >= profiling_limit)
        // {
        //    profiling_stage = false;
        //    return false;
        // }

        std::string line;
        getline(trace_file_expr, line);
        if (trace_file_expr.eof()) 
        {
            ++runs;
//            if (runs == REPEAT)
//            {
                trace_file_expr.close();
                return false;
//            }
//            trace_file_expr.clear();
//            trace_file_expr.seekg(0, std::ios::beg);
//            getline(trace_file_expr, line);
        }

        std::stringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string intermidiate;
        while (getline(line_stream, intermidiate, ' '))
        {
            tokens.push_back(intermidiate);
        }
        assert(tokens.size());
        
        if (tokens.size() == 4)
        {
            pending_exes = std::stoul(tokens[0]);

            pending_mem_opr_pc = std::stoull(tokens[1]);
            if (tokens[2] == "L")
            {
                pending_mem_opr = Instruction::Operation::LOAD;
            }
            else if (tokens[2] == "S")
            {
                pending_mem_opr = Instruction::Operation::STORE;
            }
            pending_mem_vaddr = std::stoul(tokens[3]);

            // Return with an EXE instruction
//            std::cout << "E\n";
            inst.opr = Instruction::Operation::EXE;
            --pending_exes;
        }
        else
        {
            inst.eip = std::stoull(tokens[0]);
            
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
//            std::cout << "M\n";
        }
        ++instruction_index;
        return true;
    }

    bool getMemtraceRequest(Request &req)
    {
        std::string line;
        getline(trace_file_expr, line);
        if (trace_file_expr.eof())
        {
//            ++runs;
//            if (runs == REPEAT)
//            {
                trace_file_expr.close();
                return false;
//            }
//            trace_file_expr.clear();
//            trace_file_expr.seekg(0, std::ios::beg);
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

        // trace_file_expr.open(trace_fname);
        // assert(trace_file_expr.good());
    }

    void reStartTrace()
    {
        instruction_index = 0;

        trace_file_expr.open(trace_name);
        assert(trace_file_expr.good());
    }

  private:
    unsigned pending_exes = 0;

    Addr pending_mem_opr_pc;
    Instruction::Operation pending_mem_opr = Instruction::Operation::MAX;
    Addr pending_mem_vaddr;

    uint64_t instruction_index = 0;

    std::string trace_name;
    std::ifstream trace_file_expr;

    // Are we in profiling stage?
    bool profiling_stage = false;
    uint64_t profiling_limit = 0;

    const unsigned REPEAT = 1;
    unsigned runs = 0;
};
}
#endif
