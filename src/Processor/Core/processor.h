#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace Processor
{
typedef uint64_t Addr;
typedef uint64_t Tick;
typedef uint64_t Scalar;

class Window
{
  public:
    Window() : ready_list(depth), addr_list(depth, -1) {}
    bool is_full();
    bool is_empty();
    void insert(bool ready, Addr addr);
    long retire();
    void set_ready(Addr addr, int mask);

    int ipc = 4;
    int depth = 128;

  private:
    int load = 0;
    int head = 0;
    int tail = 0;
    std::vector<bool> ready_list;
    std::vector<Addr> addr_list;
};

class Core
{
  public:
    Core(int core_id, const char* trace_file);

    void tick();
    bool done();

  private:
    Tick cycles;
    int core_id;

    CPUTraceReader trace;
    Window window;

    bool more_insts;
    Instruction cur_inst;

  // Stats
  private:
    Scalar retired;
};

class Processor
{
  public:
    // TODO, better to create a complete configuration file for all the components
    Processor(std::vector<const char*>trace_lists);

    void tick();
    bool done();

  private:
    Tick cycles;

    std::vector<std::unique_ptr<Core>> cores;
};
}

#endif
