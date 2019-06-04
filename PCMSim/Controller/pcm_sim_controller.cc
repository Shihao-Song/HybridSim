#include "pcm_sim_controller.hh"
#include "../Array_Architecture/pcm_sim_array.hh"

namespace PCMSim
{
void Controller::tick()
{
    clk++;
    channel->update(clk);

    servePendingAccesses();

    if (scheduled == 0)
    {
        scheduled_req = r_w_q.begin();

        if (r_w_q.size() != 0)
        {
            scheduled = 1;
        }
    }

    bool issued = 0;
    if (scheduled)
    {
        issued = issueAccess();
    }

    if (issued)
    {
        r_w_pending_queue.push_back(std::move(*scheduled_req));
        r_w_q.erase(scheduled_req);

        scheduled = 0;
    }
}

void Controller::servePendingAccesses()
{
    if (!r_w_pending_queue.size())
    {
        return;
    }

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

bool Controller::issueAccess()
{
    bool issued = 0;
    
    int target_rank = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int target_bank = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];
        
    if (channel->children[target_rank]->children[target_bank]->isFree() &&
        channel->isFree())
    {
        channelAccess();
	issued = 1;
    }

    return issued;
}

// TODO, timing is not correct, please refer to PLPController
void Controller::channelAccess()
{
    scheduled_req->begin_exe = clk;

    unsigned latency = 0;

    if (scheduled_req->req_type == Request::Request_Type::READ)
    {	
        latency = channel->read(scheduled_req);
    }
    else if (scheduled_req->req_type == Request::Request_Type::WRITE)
    {
        latency = channel->write(scheduled_req);
    }

    scheduled_req->end_exe = scheduled_req->begin_exe + latency;

    // Post access
    int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

//    channel->postAccess(Config::Level::Channel, rank_id, bank_id, latency);
//    channel->postAccess(Config::Level::Bank, rank_id, bank_id, latency);
}
}
