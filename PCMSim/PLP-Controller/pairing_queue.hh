#ifndef __PCM_SIM_PAIRING_QUEUE_HH__
#define __PCM_SIM_PAIRING_QUEUE_HH__

#include "../request.hh"
#include "../../Configs/config.hh"

namespace PCMSim
{
class Pairing_Queue
{
    typedef Configuration::Config Config; 

  public:
    Pairing_Queue() : queue_size(0) {}
    ~Pairing_Queue() {}
    int size() const { return queue_size; }

    void push_back(Request &req)
    {
        // Assign OrderID
        req.OrderID = queue_size;

        // Update queue size
        queue_size++;

        // Push into queue
        queue.push_back(req);

        // Find pair (PLP is not enabled)
        if (no_pairing == 1)
        {
            return;
        }
        else if (FCFS == 1)
        {
            FCFS_pairing();
        }
        else if (OoO == 1)
        {
            // Get the newly pushed request
            std::list<Request>::iterator new_req = queue.end();
            new_req--;

            for (std::list<Request>::iterator ite = queue.begin();
                ite != new_req; ++ite)
            {
                if (pairing(ite, new_req, 1) == 1)
	        {
                    break;
	        }
            }
        }	
    }

    void erase(std::list<Request>::iterator &req)
    {
        int slave = req->slave;

        // pop out of queue
        queue.erase(req);

        // Update queue size
        queue_size--;

        // Update OrderIDs
        if (slave != 1)
        {
            updateOrderIDs();
        }
    }

    std::list<Request> queue;

    const int max = 64;

    int OoO = 0;

    int FCFS = 0;

    int no_pairing = 0;

    int queue_size;

    void updateOrderIDs()
    {
        // A negative order ID represents back-logged request
        for(std::list<Request>::iterator ite = queue.begin(); 
            ite != queue.end(); ++ite)
        {
            ite->OrderID = ite->OrderID - 1;
        }
    }

    void FCFS_pairing ()
    {
        if (queue.size() < 2)
        {
            return;
        }

        // Get the newly pushed request
        std::list<Request>::iterator slave = queue.end();
        slave--;

        // Get the master
        std::list<Request>::iterator master = queue.end();
        master--;
        master--;

        // FCFS pairing only considers R-W paral.
        pairing(master, slave, 0);
        assert(slave->pair_type != Request::Pairing_Type::RR);
    }

    int pairing(std::list<Request>::iterator &master, 
                std::list<Request>::iterator &slave,
                bool rr_enabled)
    {
        if (
          // Same Channel as the newly pushed request
          slave->addr_vec[int(Config::Decoding::Channel)] ==
          master->addr_vec[int(Config::Decoding::Channel)] &&
          // Same Rank as the newly pushed request
          slave->addr_vec[int(Config::Decoding::Rank)] ==
          master->addr_vec[int(Config::Decoding::Rank)] &&
          // Same Bank as the newly pushed request
          slave->addr_vec[int(Config::Decoding::Bank)] ==
          master->addr_vec[int(Config::Decoding::Bank)] &&
          // Different Partition than the newly pushed request
          slave->addr_vec[int(Config::Decoding::Partition)] !=
          master->addr_vec[int(Config::Decoding::Partition)] &&
          // (master) Not a master request
          master->master != 1 &&
          // (master) Not a slave request
          master->slave != 1)
        {
            if (slave->req_type == Request::Request_Type::WRITE &&
              master->req_type == Request::Request_Type::READ)
            {
                // A write request can only pair with a write request
                pairForWriteReq(master, slave);
            }
            else if (slave->req_type == Request::Request_Type::READ)
            {
                // A read request can pair with either a read or a write request
                pairForReadReq(master, slave, rr_enabled);
            }

	    if (
              slave->pair_type == Request::Pairing_Type::RW ||
              slave->pair_type == Request::Pairing_Type::RR
            )
            {
                return 1;
            }
        }

        return 0;
    }

    // When slave is a read request
    void pairForReadReq(std::list<Request>::iterator &master,
                        std::list<Request>::iterator &slave,
                        bool rr_enabled)
    {
        // Set pairing type
        if (rr_enabled && master->req_type == Request::Request_Type::READ)
        {
            master->master = 1;
            slave->slave = 1;

            master->pair_type = Request::Pairing_Type::RR;
            slave->pair_type = Request::Pairing_Type::RR;

            // Assign pointers to each other
            master->slave_req = slave;
            slave->master_req = master;
        }
        else if (master->req_type == Request::Request_Type::WRITE)
        {
            master->master = 1;
            slave->slave = 1;

            master->pair_type = Request::Pairing_Type::RW;
            slave->pair_type = Request::Pairing_Type::RW;

            // Assign pointers to each other
            master->slave_req = slave;
            slave->master_req = master;
        }
    }

    void pairForWriteReq(std::list<Request>::iterator &master,
                         std::list<Request>::iterator &slave)
    {
        master->master = 1;
        slave->slave = 1;

        master->pair_type = Request::Pairing_Type::RW;
        slave->pair_type = Request::Pairing_Type::RW;

        master->slave_req = slave;
        slave->master_req = master;
    }
};
}
#endif
