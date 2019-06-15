#ifndef __PCMSIM_PLP_CONTROLLER__
#define __PCMSIM_PLP_CONTROLLER__

#include "../Controller/pcm_sim_controller.hh"
#include "pcm_sim_rw_queue.hh"

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

namespace PCMSim
{
/*
 * Memory controller for PLP (Partition-level Parallelism) family.
 * */
class PLPController : public BaseController
{
  public:
    PLPController(Config &cfgs, Array *_channel)
        : BaseController(_channel), scheduled(0)
    {
        // Step One, assign controller the scheduling policy based on type
        if (cfgs.mem_controller_type == "PALP")
        {
            // Enable controller to exploit R-R Parallelism.
            rr_enable = true;
            getHead = std::bind(&PLPController::OoO, this, std::placeholders::_1);
        }
        else if (cfgs.mem_controller_type == "PALP-R")
        {
            // PALP-R(educed) only exploits R-W Parallelism
            rr_enable = false;
            getHead = std::bind(&PLPController::OoO, this, std::placeholders::_1);
        }
        else if (cfgs.mem_controller_type == "Base")
        {
            // Base is simply a FCFS scheduler
            getHead = std::bind(&PLPController::Base, this, std::placeholders::_1);
        }
        else
        {
            std::cerr << "Unknow Memory Controller Type. \n";
            exit(0);
        }

        // Initialize timings and power specific to PLP operations
        init();

        // Other configurations specific to PLP operations
        power_limit_enabled = cfgs.power_limit_enabled;
        starv_free_enabled = cfgs.starv_free_enabled;
        RAPL = cfgs.RAPL;
        THB = cfgs.THB;

        // Output for off-line analysis
        std::string run_path = "plp_out/";
        int status = mkdir(run_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // Top directory indicates RAPL Limit and Back-logging Threshold
        // Note, we have upgraded the data transfer rate after submission (cfg files),
        // so you may see different RAPL number with the current configurations.
        if (!power_limit_enabled && !starv_free_enabled)
        {
            run_path += "inf_inf";
        }
	else if (!power_limit_enabled && starv_free_enabled)
        {
            run_path += "inf_" + std::to_string(-THB);
        }
        else
        {
            run_path += std::to_string(RAPL) + "_" + std::to_string(-THB);
        }
        status = mkdir(run_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // Secondary directory indicates the size of eDRAM in MB. In our CASES submission,
        // our eDRAM is configured to be writes-only which means it only caches write requests
        // coming from higher memory hierarchy. All read requests directly go to PCM.
        unsigned eDRAM_size = cfgs.caches[int(Config::Cache_Level::eDRAM)].size / 1024;
        run_path += "/" + std::to_string(eDRAM_size) + "_MB";
        mkdir(run_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // Third level directory indicates the size of PCM in GB.
        std::string size_path = run_path + "/" +
                                std::to_string(cfgs.sizeInGB()) + "_GB";
        status = mkdir(size_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // Fourth level directory indicates the controller's type.
        std::string type_path = size_path + "/" + cfgs.mem_controller_type;
        status = mkdir(type_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // This is the request info output file. It contains all the important information
        // of each request serviced by PCM. (See Format*)
        std::string file = type_path + "/" + cfgs.workload + ".req_info";
        out.open(file);
        // Format*
        out << "Channel,Rank,Bank,"
            << "Type,"
            << "Queue Arrival,Begin Execution,End Execution,"
            << "RAPL,OrderID\n";
    }

    int getQueueSize() override
    {
        return r_w_q.size() + r_w_pending_queue.size();
    }

    bool enqueue(Request& req) override
    {
        if (r_w_q.max == r_w_q.size())
        {
            return false;
        }

        req.queue_arrival = clk;

        r_w_q.push_back(req);

        return true;
    }

    void tick() override;

  // Special timing and power info
  private:
    void init();

    // See init() implementation for more informations.
    int time_for_single_read;
    int time_single_read_work;
    double power_per_bit_read;

    int time_for_single_write;
    int time_single_write_work;
    double power_per_bit_write;

    int time_for_rr;

    int time_for_rw;

    int tRCD;
    int tData;

  // (run-time) power calculations
  private:
    double power = 0.0; // Running average power of the channel

    void update_power_read();
    void update_power_write();
    double power_rr();
    double power_rw();

  private:
    // r_w_q: requests waiting to get served
    Read_Write_Queue r_w_q;
    // r_w_pending_queue: requests already get issued
    std::deque<Request> r_w_pending_queue;

  private:
    void servePendingReqs();
    bool issueAccess();
    void channelAccess();
    
  // Scheduler section
  private:
    bool rr_enable = false; // Exploit R-R parallelism?

    // Running average power should always below RAPL? (Default no)
    bool power_limit_enabled = false;
    // OrderID should never exceed back-logging threshold? (Default no)
    bool starv_free_enabled = false;

    double RAPL; // running average power limit
    int THB; // back-logging threshold

    bool scheduled;
    std::list<Request>::iterator scheduled_req;

    std::function<std::list<Request>::iterator(bool &retry)>getHead;
    std::list<Request>::iterator Base(bool &retry);
    std::list<Request>::iterator OoO(bool &retry);

    bool OoOPair(std::list<Request>::iterator &req);
    void powerUpdate(std::list<Request>::iterator &req);
    void pairForRR(std::list<Request>::iterator &master,
                   std::list<Request>::iterator &slave);
    void pairForRW(std::list<Request>::iterator &master,
                   std::list<Request>::iterator &slave);

  // Data collection for off-line analysis
  public:
    Tick getEndExe() { return clk; }
    double getPower() { return power; }

  private:
    std::ofstream out;
    void printReqInfo(Request &req);
};
}

#endif
