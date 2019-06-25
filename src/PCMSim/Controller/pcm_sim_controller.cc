#include "PCMSim/Controller/pcm_sim_controller.hh"

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
        channel->children[target_rank]->isFree() && // There should not be
                                                    // rank-level parallelsim.
        channel->isFree())
    {
        channelAccess();
	issued = 1;
    }

    return issued;
}

void Controller::channelAccess()
{
    scheduled_req->begin_exe = clk;

    unsigned req_latency = 0;
    unsigned bank_latency = 0;
    unsigned channel_latency = 0;

    if (scheduled_req->req_type == Request::Request_Type::READ)
    {
        req_latency = read_latency;
        bank_latency = read_bank_latency;
        channel_latency = tData;
    }
    else if (scheduled_req->req_type == Request::Request_Type::WRITE)
    {
        req_latency = write_latency;
        bank_latency = write_latency;
        channel_latency = tData;
    }

    scheduled_req->end_exe = scheduled_req->begin_exe + req_latency;

    // Post access
    int rank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Rank)];
    int bank_id = (scheduled_req->addr_vec)[int(Config::Decoding::Bank)];

    channel->postAccess(Config::Array_Level::Channel, rank_id, bank_id, channel_latency);
    channel->postAccess(Config::Array_Level::Bank, rank_id, bank_id, bank_latency);

    // All other ranks won't be available until this rank is fully de-coupled.
    int num_of_ranks = channel->arr_info.num_of_ranks;
    for (int i = 0; i < num_of_ranks; i++)
    {
        if (i == rank_id)
        {
            continue;
        }

        channel->postAccess(Config::Array_Level::Rank, i, bank_id, req_latency);
    }
}
}
