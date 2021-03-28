#ifndef __SIM_STATS_HH__
#define __SIM_STATS_HH__

#include <fstream>
#include <string>
#include <vector>

namespace Simulator
{
class Stats
{
  protected:
    std::vector<std::string> printables;

  public:
    Stats(){}

    void registerStats(std::string printable, bool new_line = true)
    {
        if (new_line)
            printables.push_back(printable + "\n");
        else
            printables.push_back(printable);
    }

    void outputStats(std::string output)
    {
        std::ofstream fd(output);
        for (auto entry : printables)
        {
            fd << entry;
        }
        fd.close();
    }
};
}

#endif
