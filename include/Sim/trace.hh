#ifndef __SIM_TRACE_HH__
#define __SIM_TRACE_HH__

#include <algorithm>
#include <cassert>
#include <deque>
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

        if (pending_instrs.size())
        {
            pending_instrs[0]->makeInstr(inst);
            pending_instrs.pop_front();

            return true;
        }

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
            // TODO, to support multiple runs, we'd better have a way to re-assign
            // address space for the application.
        }

        std::stringstream line_stream(line);
        std::vector<std::string> tokens;
        std::string intermidiate;
        while (getline(line_stream, intermidiate, ' '))
        {
            tokens.push_back(intermidiate);
        }
        assert(tokens.size() == 5);

        unsigned thread_id = std::stoul(tokens[0]);
        unsigned num_exes = std::stoul(tokens[1]);
        Addr pc = std::stoull(tokens[2]);
        while (num_exes)
        {
            auto instr = std::make_unique<ExeInstrInfo>();
            instr->thread_id = thread_id;
            instr->eip = pc;
            pending_instrs.push_back(std::move(instr));

            --num_exes;
        }

	if (tokens[3] == "B")
        {
            bool taken = std::stoul(tokens[4]);
            if (pending_instrs.size())
            {
                auto instr = std::make_unique<BranchInstrInfo>();
                instr->thread_id = thread_id;
                instr->eip = pc;
                instr->taken = taken;

                pending_instrs.push_back(std::move(instr));

                inst.thread_id = thread_id;
                inst.opr = Instruction::Operation::EXE;
                inst.eip = pc;
                pending_instrs.pop_front();
            }
            else
            {
                inst.thread_id = thread_id;
                inst.eip = pc;
                inst.opr = Instruction::Operation::BRANCH;
                inst.taken = taken;
            }
        }
        else if (tokens[3] == "S" || tokens[3] == "L")
        {
            Addr v_addr = std::stoull(tokens[4]);

            Instruction::Operation opr;
            if (tokens[3] == "S") { opr = Instruction::Operation::STORE; }
            else if (tokens[3] == "L") { opr = Instruction::Operation::LOAD; }

            if (pending_instrs.size())
            {
                auto instr = std::make_unique<MemInstrInfo>();
                instr->thread_id = thread_id;
                instr->eip = pc;
                instr->opr = opr;
                instr->v_addr = v_addr;

                pending_instrs.push_back(std::move(instr));

                inst.thread_id = thread_id;
                inst.opr = Instruction::Operation::EXE;
                inst.eip = pc;
                pending_instrs.pop_front();
            }
            else
            {
                inst.thread_id = thread_id;
                inst.eip = pc;
                inst.opr = opr;
                inst.target_vaddr = v_addr;
            }
        }
	else
        {
            std::cerr << line << "\n";
            std::cerr << "Unsupported Instruction Type \n";
            exit(0);
        }

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

        req.core_id = std::stoi(tokens[0]);

        req.addr = std::stoull(tokens[1]);
        if (tokens[2] == "R")
        {
            req.req_type = Request::Request_Type::READ;
        }
        else if (tokens[2] == "W")
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
        trace_file_expr.open(trace_name);
        assert(trace_file_expr.good());
    }

  private:
    struct InstrInfo
    {
        unsigned thread_id;

        virtual void makeInstr(Instruction& instr) {}
    };

    struct BranchInstrInfo : public InstrInfo
    {
        const Instruction::Operation opr = Instruction::Operation::BRANCH;

        Addr eip; // PC of the branch
        bool taken; // Real direction of the branch

        void makeInstr(Instruction& instr) override
        {
            instr.thread_id = thread_id;
            instr.opr = opr;
            instr.eip = eip;
            instr.taken = taken;
        }
    };
    struct MemInstrInfo : public InstrInfo
    {
        Instruction::Operation opr = Instruction::Operation::MAX; // To be assigned 

        Addr eip;
        Addr v_addr; // virtual (target) address.
	
        void makeInstr(Instruction& instr) override
        {
            instr.thread_id = thread_id;
            instr.opr = opr;
            instr.eip = eip;
            instr.target_vaddr = v_addr;
        }

    };
    struct ExeInstrInfo : public InstrInfo
    {
        const Instruction::Operation opr = Instruction::Operation::EXE;
        Addr eip;
       
        void makeInstr(Instruction &instr) override
        {
            instr.thread_id = thread_id;
            instr.opr = opr;
            instr.eip = eip;
        }
    };
    std::deque<std::unique_ptr<InstrInfo>> pending_instrs;

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
