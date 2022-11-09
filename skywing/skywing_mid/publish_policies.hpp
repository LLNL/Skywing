#ifndef UPDATE_NBRS_CRITERION_HPP
#define UPDATE_NBRS_CRITERION_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include <cmath>
#include <iostream>

/** @brief A PublishPolicy that always publishes.
 */ 
class AlwaysPublish
{
public:
  
  AlwaysPublish()
  {}

  template<typename ValueType>
  bool operator()([[maybe_unused]] const ValueType& new_vals,
                  [[maybe_unused]] const ValueType& old_vals)
  { return true; }
}; // class AlwaysPublish




/** @brief a PublishPolicy that publishes if the Linf norm of the
 *  value differences is at least a threshold.
 */
template<typename scalar_t = double>
class PublishOnLinfShift
{
public:

  PublishOnLinfShift(scalar_t thresh)
    : linf_threshold_(thresh)
  {}

  template<typename ValueType>
  bool operator()(const ValueType& new_vals, const ValueType& old_vals)
  {
    auto new_v_iter = new_vals.cbegin();
    auto old_v_iter = old_vals.cbegin();
    while (new_v_iter != new_vals.cend())
    {
      scalar_t curr_diff = std::abs(*new_v_iter - *old_v_iter);
      if (curr_diff > linf_threshold_)
        return true;
      new_v_iter++;
      old_v_iter++;
    }
    return false;
  }

private:
  scalar_t linf_threshold_;
}; // class PublishOnLinfShift



/** @brief A PublishPolicy that publishes if a ratio of values has
 * shifted by a threshold.
 *
 * @tparam scalar_t The same scalar type as used in the PushSumProcessor.
 */
template<typename scalar_t, size_t ind1, size_t ind2>
class PublishOnRatioShift
{
public:

  PublishOnRatioShift(scalar_t thresh)
    : shift_threshold_(thresh)
  {  }

  template<typename ValueType>
  bool operator()(const ValueType& new_vals, const ValueType& old_vals)
  {
    scalar_t new_ratio = std::get<ind1>(new_vals) / std::get<ind2>(new_vals);
    scalar_t old_ratio = std::get<ind1>(old_vals) / std::get<ind2>(old_vals);
    return (new_ratio - old_ratio) > shift_threshold_ || (old_ratio - new_ratio) > shift_threshold_;
  }

private:
  scalar_t shift_threshold_;
}; // class PublishRatioShift

#endif // UPDATE_NBRS_CRITERION_HPP
