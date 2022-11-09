#ifndef STOPPING_CRITERION_HPP
#define STOPPING_CRITERION_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include <chrono>

namespace skywing
{
  
/* This file contains a number of common iterative methods StopPolicy
   (stopping criteria) options.
 */

/** @brief StopPolicy that stops after a given amount of time has
    passed.
 */
class StopAfterTime
{
public:

  template<typename Duration>
  StopAfterTime(Duration d)
    : max_run_time_(d)
  {}

  template<typename CallerT>
  bool operator()(const CallerT& caller)
  {
    return caller.run_time() > max_run_time_;
  }

private:
  std::chrono::milliseconds max_run_time_;
  
}; // class StopAfterTime


// template<typename LocalStopPolicy>
// class SynchronousConsensusStop
// {
//  public:
//   using ValueType = bool;

//   SynchronousConsensusStop(size_t collective_graph_diameter_upperbound)
//     : diam_ub_(collective_graph_diameter_upperbound)
//   {}

//   template<typename CallerT>
//   bool operator()(const CallerT& caller)
//   { return iterations_since_all_ready_ > diam_ub_;  }

//   ValueType get_init_publish_values()
//   { return locally_ready_to_stop_; }

//   template<typename NbrDataHandler, typename IterMethod>
//   void process_update(const NbrDataHanlder& nbr_data_handler,
//                       const IterMethod& caller)
//   {
//     locally_ready_to_stop_ = local_stop_policy_(caller);
    
//     bool is_all_ready = locally_ready_to_stop_ &&
//       nbr_data_handler.f_accumulate<bool>([](const bool& b) {return b;},
//                                           std::logical_and<bool>);
//     if (is_all_ready)
//       ++iterations_since_all_ready_;
//     else
//       iterations_since_all_ready = 0;
//   }

//   bool prepare_for_publication(ValueType)
//   {
//     return iterations_since_all_ready > diam_ub_;
//   }

// private:
//   bool locally_ready_to_stop_ = false;
//   size_t iterations_since_all_ready_ = 0;
//   size_t diam_ub_;

//   LocalStopPolicy local_stop_policy_;
// } // class SynchronousConsensusStop

} // namespace skywing

#endif
