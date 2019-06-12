#ifndef __PCMSIM_PLP_CONTROLLER__
#define __PCMSIM_PLP_CONTROLLER__

#include "../Controller/pcm_sim_controller.hh"
#include "pairing_queue.hh"

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

namespace PCMSim
{
class PLPController : public BaseController
{
  public:
    PLPController(Config &cfgs, Array *_channel)
        : BaseController(_channel), scheduled(0)
    {
        r_w_q.no_pairing = 1;

        // Configure pairing_queue
        if (cfgs.mem_controller_type == "PALP")
        {
//            r_w_q.OoO = 1;
            rr_enable = true;
            getHead = std::bind(&PLPController::OoO, this, std::placeholders::_1);
        }
        else if (cfgs.mem_controller_type == "PALP-R")
        {
//            r_w_q.FCFS = 1;
            rr_enable = false;
            getHead = std::bind(&PLPController::OoO, this, std::placeholders::_1);
        }
        else if (cfgs.mem_controller_type == "Base")
        {
//            r_w_q.no_pairing = 1;
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
        int status = mkdir(run_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
       
        unsigned eDRAM_size = cfgs.caches[int(Config::Cache_Level::eDRAM)].size / 1024;
        run_path += "/" + std::to_string(eDRAM_size) + "_MB";
        mkdir(run_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        std::string size_path = run_path + "/" +
                                std::to_string(cfgs.sizeInGB()) + "_GB";
        status = mkdir(size_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
        std::string type_path = size_path + "/" + cfgs.mem_controller_type;
        status = mkdir(type_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        std::string file = type_path + "/" + cfgs.workload + ".req_info";
        out.open(file);

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
    double power = 0.0;
    uint64_t finished_request = 0;

    void update_power_read();
    void update_power_write();
    double power_rr();
    double power_rw();

  private:
    // r_w_q: requests waiting to get served
    Pairing_Queue r_w_q;
    // r_w_pending_queue: requests already get issued
    std::deque<Request> r_w_pending_queue;

  private:
    void servePendingReqs();
    bool issueAccess();
    void channelAccess();

  public:
    void setThresholds(double _RAPL, int _THB)
    {
        RAPL = _RAPL;
        THB = _THB;
    }

  // Scheduler section
  private:
    bool rr_enable;

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
    std::list<Request>::iterator FCFS(bool &retry);
    std::list<Request>::iterator OoO(bool &retry);

    bool OoOPair(std::list<Request>::iterator &req);
    void powerLimit(std::list<Request>::iterator &req);
    void pairForRR(std::list<Request>::iterator &master,
                   std::list<Request>::iterator &slave);
    void pairForRW(std::list<Request>::iterator &master,
                   std::list<Request>::iterator &slave);
    
    // High-priority issue (the current request must be issued)
    void HPIssue(std::list<Request>::iterator &req);
    // Break up master-slave chain
    void breakup(std::list<Request>::iterator &req);

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
