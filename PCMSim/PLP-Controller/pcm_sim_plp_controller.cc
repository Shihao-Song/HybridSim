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
        scheduled_req = getHead();
        scheduled = 1;
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
            channel_latency = tData + tRCD + tData;
        }
        else if (scheduled_req->pair_type == Request::Pairing_Type::RW)
        {
            req_latency = time_for_rw;
            bank_latency = time_for_rw;
            channel_latency = time_for_single_read;
        }
    }

    scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

    // Post access
    int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

    channel->postAccess(Config::Level::Channel, rank_id, bank_id, bank_latency);
    channel->postAccess(Config::Level::Bank, rank_id, bank_id, channel_latency);
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
std::list<Request>::iterator PLPController::Base()
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

std::list<Request>::iterator PLPController::FCFS()
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    HPIssue(req);
    assert(req->OrderID >= 0);
    assert(req->pair_type != Request::Pairing_Type::RR);
    return req; 
}

std::list<Request>::iterator PLPController::OoO()
{
    std::list<Request>::iterator req = r_w_q.queue.begin();
    if (starv_free_enabled && req->OrderID <= THB)
    {
        // Have to be served anyway
        HPIssue(req);
        return req;
    }

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

    // At this point, we need split either RR pair or RW pair
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
}
