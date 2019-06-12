#include "pcm_sim_plp_controller.hh"
#include "../Array_Architecture/pcm_sim_array.hh"

namespace PCMSim
{
void PLPController::init()
{
    time_for_single_read = channel->arr_info.tRCD +
                           channel->arr_info.tData +
			   channel->arr_info.tCL;
    time_single_read_work = channel->arr_info.tCL;
    power_per_bit_read = channel->arr_info.pj_bit_rd /
			 time_single_read_work;
    
    time_for_single_write = channel->arr_info.tRCD +
                            channel->arr_info.tData +
                            channel->arr_info.tWL +
                            channel->arr_info.tWR;
    time_single_write_work = channel->arr_info.tWL +
                             channel->arr_info.tWR;
    power_per_bit_write = (channel->arr_info.pj_bit_set +
                         channel->arr_info.pj_bit_reset) /
                         2.0 / time_single_write_work;

    time_for_rr = channel->arr_info.tRCD +
                  channel->arr_info.tRCD +
                  channel->arr_info.tRCD +
                  channel->arr_info.tCL +
                  channel->arr_info.tData +
                  channel->arr_info.tRCD +
                  channel->arr_info.tData;

    time_for_rw = time_for_single_write + channel->arr_info.tRCD;

    tData = channel->arr_info.tData;
    tRCD = channel->arr_info.tRCD;
}

void PLPController::tick()
{
    clk++;

    // Update state in PCM
    channel->update(clk);

    // Serve pending requests
    servePendingReqs();

    // Find the request to schedule
    if (scheduled == 0 && r_w_q.size())
    {
        bool retry = false;
        scheduled_req = getHead(retry);
        if (!retry)
        {
            scheduled = 1;
        }
    }

    // Issue command
    bool issued = 0;

    if (scheduled == 1)
    {
        issued = issueAccess();
    }

    // If it's issued, push it into pending queue.
    if (issued)
    {
        if (scheduled_req->master == 1)
        {
            scheduled_req->slave_callback = 
            scheduled_req->slave_req->callback;

            scheduled_req->slave_addr = scheduled_req->slave_req->addr;

            scheduled_req->slave_arrival = scheduled_req->slave_req->queue_arrival;
            scheduled_req->slave_type = scheduled_req->slave_req->req_type;
            scheduled_req->slave_order_id = scheduled_req->slave_req->OrderID;

            r_w_pending_queue.push_back(*scheduled_req);

            // Erase slave
            r_w_q.erase(scheduled_req->slave_req);

            // Erase master
            r_w_q.erase(scheduled_req);
        }
        else
        {
            r_w_pending_queue.push_back(*scheduled_req);
            r_w_q.erase(scheduled_req);
        }

        scheduled = 0;
    }
}

void PLPController::servePendingReqs()
{
    if (r_w_pending_queue.size() > 0)
    {
        Request &req = r_w_pending_queue[0];
        if (req.end_exe <= clk)
        {
            if (req.callback)
            {
                req.callback(req);
            }

            // Very important!
            if (req.master == 1)
            {
                if (req.slave_callback)
                {
                    Request tmp_req(req.slave_addr,
                                    Request::Request_Type::READ);
                    req.slave_callback(tmp_req);
                }
            }
            printReqInfo(req);
            r_w_pending_queue.pop_front();
        }
    }
}

bool PLPController::issueAccess()
{
    bool issued = 0;

    int target_rank = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int target_bank = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

    // channel->isFree() mimics the bus utilization
    // ...bank->isFree() mimics the bank utilization
    if (channel->children[target_rank]->children[target_bank]->isFree() &&
        channel->children[target_rank]->isFree() && // There should not be
                                                    // rank-level parallelsim.
        channel->isFree())
    {
        channelAccess();
        issued = 1;
    }

    return issued;
}

void PLPController::channelAccess()
{
    scheduled_req->begin_exe = clk;

    unsigned req_latency = 0; // read/write + data transfer
    unsigned bank_latency = 0;
    unsigned channel_latency = 0;

    if (scheduled_req->master != 1)
    {
        if (scheduled_req->req_type == Request::Request_Type::READ)
        {	
            req_latency = time_for_single_read;
            bank_latency = tRCD + time_single_read_work;
            channel_latency = tData;
        }
        else if (scheduled_req->req_type == Request::Request_Type::WRITE)
        {
            req_latency = time_for_single_write;
            bank_latency = time_for_single_write;
            channel_latency = tData;
        }
    }
    else if (scheduled_req->master == 1)
    {
        if (scheduled_req->pair_type == Request::Pairing_Type::RR)
        {
            req_latency = time_for_rr;
            bank_latency = tRCD * 3 + time_single_read_work;
            channel_latency = tData;
        }
        else if (scheduled_req->pair_type == Request::Pairing_Type::RW)
        {
            req_latency = time_for_rw;
            bank_latency = time_for_rw;
            channel_latency = tData;
        }
    }

    scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

    // Post access
    int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

    channel->postAccess(Config::Level::Channel, rank_id, bank_id, channel_latency);
    channel->postAccess(Config::Level::Bank, rank_id, bank_id, bank_latency);

    // All other ranks won't be available until this rank is fully de-coupled.
    int num_of_ranks = channel->arr_info.num_of_ranks;
    for (int i = 0; i < num_of_ranks; i++)
    {
        if (i == rank_id)
        {
            continue;
        }

        channel->postAccess(Config::Level::Rank, i, bank_id, req_latency);
    }

    /*
    // De-bugging
    std::cout << "My rank: " << rank_id << "\n";
    std::cout << "My bank: " << bank_id << "\n";
    std::cout << "Current CLK: " << clk << "\n";
    std::cout << "My next bank free: "
              << channel->children[rank_id]->children[bank_id]->next_free << "\n";
    for (int i = 0; i < num_of_ranks; i++)
    {
        std::cout << "Rank " << i << ": "
                  << channel->children[i]->next_free << "\n";
    }

    std::cout << "\n";
    */
}

// (run-time) power calculation
void PLPController::update_power_read()
{
    power = ((clk * power) + time_single_read_work * power_per_bit_read) /
            (clk + time_for_single_read);
}

void PLPController::update_power_write()
{
    power = ((clk * power) + time_single_write_work * power_per_bit_write) /
            (clk + time_for_single_write);
}

double PLPController::power_rr()
{
    double tmp_power = ((clk * power) +
                       time_single_read_work * power_per_bit_read * 2.0) /
                       (clk + time_for_rr);
    return tmp_power;
}

double PLPController::power_rw()
{
    double tmp_power = ((clk * power) +
                       time_single_read_work * power_per_bit_read +
                       time_single_write_work * power_per_bit_write) /
                       (clk + time_for_rw);
    return tmp_power;
}

// scheduler
std::list<Request>::iterator PLPController::Base(bool &retry)
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    assert(req->pair_type == Request::Pairing_Type::MAX);
    assert(req->OrderID == 0);

    // Update power
    if (req->req_type == Request::Request_Type::READ)
    {
        update_power_read();
    }
    else if (req->req_type == Request::Request_Type::WRITE)
    {
        update_power_write();
    }

    assert(req->OrderID == 0);
    return req;
}

std::list<Request>::iterator PLPController::FCFS(bool &retry)
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    HPIssue(req);
    assert(req->OrderID >= 0);
    assert(req->pair_type != Request::Pairing_Type::RR);
    return req; 
}

std::list<Request>::iterator PLPController::OoO(bool &retry)
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    if (starv_free_enabled && req->OrderID <= THB)
    {
        // Have to be served anyway
        OoOPair(req);
        powerLimit(req);
        return req;
    }
    assert(req->OrderID > THB);

    bool first_ready_found = false;
    std::list<Request>::iterator first_ready;
    bool first_ready_pair_found = false;
    std::list<Request>::iterator first_ready_pair;

    for (std::list<Request>::iterator ite = r_w_q.queue.begin();
         ite != r_w_q.queue.end(); ++ite)
    {
        int target_rank = (ite->addr_vec)[int(Config::Decoding::Rank)];
        int target_bank = (ite->addr_vec)[int(Config::Decoding::Bank)];

        if (channel->children[target_rank]->children[target_bank]->isFree() &&
            channel->children[target_rank]->isFree() && // There should not be
                                                       // rank-level parallelsim.
            channel->isFree())
        {
            assert(ite->master != 1);
            assert(ite->slave != 1);
            assert(ite->pair_type == Request::Pairing_Type::MAX);

            if (first_ready_found == false)
            {
                first_ready_found = true;
                first_ready = ite;
            }

            if (OoOPair(ite))
            {
                first_ready_pair_found = true;
                first_ready_pair = ite;
                break; // No need to proceed further
            }
        }
    }

    if (first_ready_pair_found)
    {
        powerLimit(first_ready_pair);
        return first_ready_pair;
    }

    // Can't find any pairs, schedule the first ready request.
    if (first_ready_found)
    {
        powerLimit(first_ready);
        return first_ready;
    }

    // PCM is busy anyway, we don't need to schedule anything
    retry = true;
    return req;
}

bool PLPController::OoOPair(std::list<Request>::iterator &req)
{
    // Should iterator the element behind it.
    // Because we have known that the requests before it are not ready or
    // this request is the oldest one (exceeding THB).
    auto ite = req;
    ++ite;

    // Step one, find a candidate read or write
    std::list<Request>::iterator r;
    std::list<Request>::iterator w;

    bool r_found = false;
    bool w_found = false;

    for (ite; ite != r_w_q.queue.end(); ++ite)
    {
        if (ite->addr_vec[int(Config::Decoding::Channel)] ==
            req->addr_vec[int(Config::Decoding::Channel)] &&
            // Same Rank
            ite->addr_vec[int(Config::Decoding::Rank)] ==
            req->addr_vec[int(Config::Decoding::Rank)] &&
            // Same Bank
            ite->addr_vec[int(Config::Decoding::Bank)] ==
            req->addr_vec[int(Config::Decoding::Bank)] &&
            // Different Partition
            ite->addr_vec[int(Config::Decoding::Partition)] !=
            req->addr_vec[int(Config::Decoding::Partition)])\
        {
            if (w_found == false &&
                ite->req_type == Request::Request_Type::WRITE)
            {
                w_found = true;
                w = ite;
            }
            else if (r_found == false &&
                     ite->req_type == Request::Request_Type::READ)
            {
                r_found = true;
                r = ite;;
            }
        }
    }

    // TODO, take into power consumption when pairing. If power exceeds
    // we should also return true.
    if (req->req_type == Request::Request_Type::READ)
    {
        // We can pair with either a read or write request
        if (rr_enable && r_found)
        {
            double est_power = power_rr();

            if (!power_limit_enabled ||
                power_limit_enabled && est_power < RAPL)
            {
                // Always priori. RR pair
                pairForRR(req, r);
            }
            return true;
        }

        // Then try to pair with a write
        if (w_found)
        {
            double est_power = power_rw();

            if (!power_limit_enabled ||
                power_limit_enabled && est_power < RAPL)
            {
                pairForRW(req, w);
            }
            return true;
        }
    }
    else if (req->req_type == Request::Request_Type::WRITE)
    {
        // We can only pair with a read request
        if (r_found)
        {
            double est_power = power_rw();

            if (!power_limit_enabled ||
                power_limit_enabled && est_power < RAPL)
            {
                pairForRW(req, r);
            }
            return true;
        }
    }

    return false; // Cannot find any pair
}

void PLPController::powerLimit(std::list<Request>::iterator &req)
{
    if (req->pair_type == Request::Pairing_Type::RR)
    {
        double est_power = power_rr();

        if (!power_limit_enabled ||
            power_limit_enabled && est_power < RAPL)
        {
            power = est_power;
        }
    }
    else if (req->pair_type == Request::Pairing_Type::RW)
    {
        double est_power = power_rw();

        if (!power_limit_enabled ||
            power_limit_enabled && est_power < RAPL)
        {
            power = est_power;
        }

    }
    else if (req->pair_type == Request::Pairing_Type::MAX)
    {
        if (req->req_type == Request::Request_Type::READ)
        {
            update_power_read();
        }
        else if (req->req_type == Request::Request_Type::WRITE)
        {
            update_power_write();
        }
    }
}

// When slave is a read request
void PLPController::pairForRR(std::list<Request>::iterator &master,
                              std::list<Request>::iterator &slave)
{
    master->master = 1;
    slave->slave = 1;

    master->pair_type = Request::Pairing_Type::RR;
    slave->pair_type = Request::Pairing_Type::RR;

    // Assign pointers to each other
    master->slave_req = slave;
    slave->master_req = master;
}

void PLPController::pairForRW(std::list<Request>::iterator &master,
                              std::list<Request>::iterator &slave)
{
    master->master = 1;
    slave->slave = 1;

    master->pair_type = Request::Pairing_Type::RW;
    slave->pair_type = Request::Pairing_Type::RW;

    // Assign pointers to each other
    master->slave_req = slave;
    slave->master_req = master;
}

/*
std::list<Request>::iterator PLPController::OoO(bool &retry)
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    if (starv_free_enabled && req->OrderID <= THB)
    {
        // Have to be served anyway
        HPIssue(req);
        return req;
    }
    assert(req->OrderID > THB);

    std::list<Request>::iterator r;
    std::list<Request>::iterator rr;
    std::list<Request>::iterator w;
    std::list<Request>::iterator rw;

    bool r_found = 0;
    bool rr_found = 0;
    bool w_found = 0;
    bool rw_found = 0;

    for (std::list<Request>::iterator ite = r_w_q.queue.begin();
         ite != r_w_q.queue.end(); ++ite)
    {
        // Record R
        int target_rank = (ite->addr_vec)[int(Config::Decoding::Rank)];
        int target_bank = (ite->addr_vec)[int(Config::Decoding::Bank)];

        if (channel->children[target_rank]->children[target_bank]->isFree() &&
            channel->children[target_rank]->isFree() && // There should not be
                                                       // rank-level parallelsim.
            channel->isFree())
        {
            if (ite->master != 1 &&
                ite->slave != 1 &&
                ite->req_type == Request::Request_Type::READ &&
                r_found == 0)
            {
                r_found = 1;
                r = ite;
            }

            // Record RR pair
            if (ite->master == 1 && // Must be a master request
                ite->pair_type == Request::Pairing_Type::RR && // RR pair
                rr_found == 0 // Must be the first RR pair found
               )
            {
                rr_found = 1;
                rr = ite;
            }

            // Record W
            if (ite->master != 1 &&
                ite->slave != 1 &&
                ite->req_type == Request::Request_Type::WRITE &&
                w_found == 0)
            {
                w_found = 1;
                w = ite;
            }

            // Record RW pair
            if (ite->master == 1 && // Must be a master request
                ite->pair_type == Request::Pairing_Type::RW && // RW pair
                rw_found == 0 // Must be the first RW pair found
               )
            {
                rw_found = 1;
                rw = ite;
            }
        }
    }

    // Prioritize rr pair
    if (rr_found == 1)
    {
        double est_power = power_rr();

        if (!power_limit_enabled || 
            power_limit_enabled && est_power < RAPL)
        {
            power = est_power;
            return rr;
        }
    }

    if (r_found == 1)
    {
        update_power_read();
        return r;
    }
   
    if (w_found == 1)
    {
        update_power_write();
        return w;
    }

    if (rw_found == 1)
    {
        double est_power = power_rw();

        if (!power_limit_enabled ||
            power_limit_enabled && est_power < RAPL)
        {
            power = est_power;
            return rw;
        }
    }

    if (r_found == 0 &&
        rr_found == 0 &&
        rw_found == 0 &&
        w_found == 0)
    {
        // PCM is busy anyway, we don't need to schedule anything
        retry = true;
        return req;
    }

    // At this point, we need to split either RR pair or RW pair
    if (rw_found == 1)
    {
        breakup(rw);
        assert(rw->pair_type == Request::Pairing_Type::MAX);
        return rw;
    }
    else if (rr_found == 1)
    {
        breakup(rr);
        assert(rr->pair_type == Request::Pairing_Type::MAX);
        return rr;
    }
    else
    {
        std::cerr << "Should never happen. \n";
        exit(0);
    }
}
*/



void PLPController::HPIssue(std::list<Request>::iterator &req)
{
    // If it is an individual request, schedule it
    if (req->master != 1 && req->slave != 1)
    {
        // Update power
        if (req->req_type == Request::Request_Type::READ)
        {
            update_power_read();
        }
        else if (req->req_type == Request::Request_Type::WRITE)
        {
            update_power_write();
        }
    }
    else
    {
        double est_power = 0.0;

        if (req->pair_type == Request::Pairing_Type::RR)
        {
            est_power = power_rr();
        }
        else if (req->pair_type == Request::Pairing_Type::RW)
        {
            est_power = power_rw();
        }

        if (power_limit_enabled && est_power >= RAPL)
        {
            breakup(req);
            assert(req->pair_type == Request::Pairing_Type::MAX);
        }
        else
        {
            // Update power
	    assert(est_power != 0.0);
            power = est_power;
        }
    }
}

void PLPController::breakup(std::list<Request>::iterator &req)
{
    // Break the master-slave connection
    req->master = 0;
    req->slave = 0;
    req->pair_type = Request::Pairing_Type::MAX;

    req->slave_req->slave = 0;
    req->slave_req->master = 0;
    req->slave_req->pair_type = Request::Pairing_Type::MAX;

    // Update power
    if (req->req_type == Request::Request_Type::READ)
    {
	update_power_read();
    }
    else if (req->req_type == Request::Request_Type::WRITE)
    {
        update_power_write();
    }
}

// Data collection for off-line analysis
void PLPController::printReqInfo(Request &req)
{
    out << req.addr_vec[int(Config::Decoding::Channel)] << ",";
    out << req.addr_vec[int(Config::Decoding::Rank)] << ",";
    out << req.addr_vec[int(Config::Decoding::Bank)] << ",";
    
    if(req.req_type == Request::Request_Type::READ)
    {
        out << "R,";
    }
    else
    {
        out << "W,";
    }

    out << req.queue_arrival << ",";
    out << req.begin_exe << ",";
    out << req.end_exe << ",";
    out << power << ",";
    out << req.OrderID << "\n";

    if(req.master != 1)
    {
        return;
    }

    assert(req.master == 1);

    out << req.addr_vec[int(Config::Decoding::Channel)] << ",";
    out << req.addr_vec[int(Config::Decoding::Rank)] << ",";
    out << req.addr_vec[int(Config::Decoding::Bank)] << ",";
    
    if(req.slave_type == Request::Request_Type::READ)
    {
        out << "R,";
    }
    else
    {
        out << "W,";
    }

    out << req.slave_arrival << ",";
    out << req.begin_exe << ",";
    out << req.end_exe << ",";
    out << power << ",";
    out << req.slave_order_id << "\n";
}
}
