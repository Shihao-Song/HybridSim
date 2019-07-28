#include "PCMSim/PLP_Controller/pcm_sim_plp_controller.hh"

namespace PCMSim
{
void PLPController::servePendingAccesses()
{
    if (!r_w_pending_queue.size())
    {
        return;
    }

    PLPRequest &req = r_w_pending_queue[0];
    if (req.end_exe <= clk)
    {
        if (req.callback)
        {
            if (req.callback(req.addr))
            {
                r_w_pending_queue.pop_front();
            }
        }
        else
        {
            r_w_pending_queue.pop_front();
        }
    }
}

void PLPController::channelAccess(std::list<PLPRequest>::iterator& scheduled_req)
{
    auto [req_latency, bank_latency, channel_latency] = getLatency(scheduled_req);

    // Post access
    postAccess(scheduled_req,
               channel_latency,
               req_latency, // This is rank latency for other ranks.
                            // Since there is no rank-level parall,
                            // other ranks must wait until the current rank
                            // to be fully de-coupled.
               bank_latency);
}

std::pair<bool,std::list<PLPRequest>::iterator> PLPController::getHead()
{
    std::list<PLPRequest>::iterator req = r_w_q.queue.begin();
    if (starv_free_enabled && req->OrderID <= THB)
    {
        if (issueable(req))
        {
            // Have to be served anyway
            OoOPair(req);
            powerUpdate(req);
            return std::make_pair(true, req);
        }
        return std::make_pair(false, req);
    }
    assert(req->OrderID > THB);

    bool first_ready_found = false;
    std::list<PLPRequest>::iterator first_ready;
    bool first_ready_pair_found = false;
    std::list<PLPRequest>::iterator first_ready_pair;

    for (std::list<PLPRequest>::iterator ite = r_w_q.queue.begin();
         ite != r_w_q.queue.end(); ++ite)
    {
        if (issueable(ite))
        {
            assert(ite->master != 1);
            assert(ite->slave != 1);
            assert(ite->pair_type == PLPRequest::Pairing_Type::MAX);

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
        powerUpdate(first_ready_pair);
        return std::make_pair(true, first_ready_pair);
    }

    // Can't find any pairs, schedule the first ready request.
    if (first_ready_found)
    {
        powerUpdate(first_ready);
        return std::make_pair(true, first_ready);
    }

    // PCM is busy anyway, we don't need to schedule anything
    return std::make_pair(false, req);
}

bool PLPController::OoOPair(std::list<PLPRequest>::iterator &req)
{
    // Should iterator the element behind it.
    // Because we have known that the requests before it are not ready or
    // this request is the oldest one (exceeding THB).
    auto ite = req;
    ++ite;

    // Step one, find a candidate read or write
    std::list<PLPRequest>::iterator r;
    std::list<PLPRequest>::iterator w;

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

    // Take into power consumption when pairing. If power exceeds
    // we should also return true.
    if (req->req_type == Request::Request_Type::READ)
    {
        // We can pair with either a read or write request
        if (rr_enabled && r_found)
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

void PLPController::powerUpdate(std::list<PLPRequest>::iterator &req)
{
    if (req->pair_type == PLPRequest::Pairing_Type::RR)
    {
        power = power_rr();
    }
    else if (req->pair_type == PLPRequest::Pairing_Type::RW)
    {
        power = power_rw();
    }
    else if (req->pair_type == PLPRequest::Pairing_Type::MAX)
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
void PLPController::pairForRR(std::list<PLPRequest>::iterator &master,
                              std::list<PLPRequest>::iterator &slave)
{
    master->master = 1;
    slave->slave = 1;

    master->pair_type = PLPRequest::Pairing_Type::RR;
    slave->pair_type = PLPRequest::Pairing_Type::RR;

    // Assign pointers to each other
    master->slave_req = slave;
    slave->master_req = master;
}

void PLPController::pairForRW(std::list<PLPRequest>::iterator &master,
                              std::list<PLPRequest>::iterator &slave)
{
    master->master = 1;
    slave->slave = 1;

    master->pair_type = PLPRequest::Pairing_Type::RW;
    slave->pair_type = PLPRequest::Pairing_Type::RW;

    // Assign pointers to each other
    master->slave_req = slave;
    slave->master_req = master;
}
}
