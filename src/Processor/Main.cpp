#include "Core/processor.h"

#include <cstring>

void parse_args(int argc, const char *argv[],
                std::vector<const char*> &trace_lists);
void simulate(std::vector<const char*> &trace_lists);

int main(int argc, const char *argv[])
{
    std::vector<const char*> trace_lists;
    parse_args(argc, argv, trace_lists);
    assert(trace_lists.size() != 0);
    for (int i = 0; i < trace_lists.size(); i++)
    {
        std::cout << "Core " << i
                  << " is running trace " << trace_lists[i] << "\n";
    }

    simulate(trace_lists);
}

void simulate(std::vector<const char*> &trace_lists)
{
    Processor::Processor proc(trace_lists);
    while (!proc.done())
    {
        proc.tick();
    }
}

void parse_args(int argc, const char *argv[],
                std::vector<const char*> &trace_lists)
{
    int trace_start = -1;
    int config_start = -1;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--traces") == 0)
        {
            trace_start = i + 1;
        }

        if (strcmp(argv[i], "--config") == 0)
        {
            config_start = i + 1;
        }
    }
    assert(config_start != -1);
    assert(config_start < argc);

    assert(trace_start != -1);
    assert(trace_start < argc);

    int num_traces;
    if (config_start < trace_start)
    {
        num_traces = argc - trace_start;
    }
    else
    {
        num_traces = config_start - 1 - trace_start;
    }

    for (int i = 0; i < num_traces; i++)
    {
        trace_lists.push_back(argv[trace_start + i]);
    }
}

/*
void runMemtraces(eDRAMCache &eDRAM, const char* tracename)
{
    std::cout << "Running trace: " << tracename << "\n\n";

    // Initialize memory trace
    Trace trace(tracename);

    // Run simulation
    bool stall = false;
    bool end = false; // Indicate end of the trace file

    // Initialize Request
    Addr addr = 0;
    Request::Request_Type req_type = PCMSim::Request::Request_Type::READ;

    Tick clks = 0;
    
    while(!end || eDRAM.numOutstanding())
    {
        if (!end && !stall)
        {
            end = !trace.getMemtraceRequest(addr, req_type);
        }

        if(!end)
        {
            Request req(addr, req_type);

	    req.addr = addr;
            req.req_type = req_type;

            stall = !eDRAM.access(req);
        }

        eDRAM.tick();
        clks++;
    }
}
*/
