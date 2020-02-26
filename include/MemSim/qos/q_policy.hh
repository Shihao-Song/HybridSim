#ifndef __QOS_BASE_Q_POLICY_HH__
#define __QOS_BASE_Q_POLICY_HH__

#include <deque>
#include <unordered_set>

#include "Sim/request.hh"

namespace QoS
{
class QoSBase;

/*
 * The queue policy is to schedule packets within the SAME QoS priority queue.
 * */
class QueuePolicy
{
  public:
    
};
}

#endif
