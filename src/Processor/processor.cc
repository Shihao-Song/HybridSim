#include "Processor/processor.hh"

namespace CoreSystem
{
Processor::Processor(std::vector<const char*> trace_lists) : cycles(0)
{
    assert(trace_lists.size());
    for (int i = 0; i < trace_lists.size(); i++)
    {
        cores.emplace_back(new Core());
    }
}
}
