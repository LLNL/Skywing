#ifndef SKYNET_UPPER_PUSH_SUM_HPP
#define SKYNET_UPPER_PUSH_SUM_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

namespace skywing
{

/**
 * Push Sum is an asynchronous distributed averaging algorithm that
 * converges to the average of initial values as long as information
 * delay is no longer than exponential in time.
 * This algorithm still converges even if there packet loss, it just
 * doesn't necessarily converge to the average in this case, although
 * it still stagnate to SOME convex combination of initial values.
 * 
 * @tparam S The scalar type used in the average, e.g. double.
 *
 * @param[in] x_value initial value for distributed averaging. 
 * @param[out] x_value/y_value obtained from return_solution(), so the solution is a ratio.
 * 
 */
template<typename S = double>
class PushSumProcessor
{
public:
  using scalar_t = S;
  using ValueType = std::tuple<S, S, unsigned>;
  using ValueTag = skywing::PublishTag<ValueType>;

  /**
   * @param number_of_neighbors Number of neighboring agents.
   * @param starting_values This agent's contribution to the average.
   */
  PushSumProcessor(scalar_t starting_value,
                   size_t number_of_neighbors)
    : x_value_(starting_value)
  {
    in_nodes_plus_one_ = number_of_neighbors + 1.0;
    // Local weights -> This is the information passed to neighbors.
    sigma_x_ = sigma_x_ + (x_value_ / in_nodes_plus_one_);
    sigma_y_ = sigma_y_ + (y_value_ / in_nodes_plus_one_);
  }

  ValueType get_init_publish_values()
  { return {sigma_x_, sigma_y_, information_count_}; }

  /** @brief Perform the push-sum update computation.
   *
   * @param nbr_tags The tags of the updated values. Each tag is an element of this->tags_.
   * @param nbr_values The new values from the neighbors.
   * @param caller The iterative wrapper calling this method.
   */
  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler, const IterMethod& iter_method)
  {
    for (const auto& pTag : nbr_data_handler.get_updated_tags())
    {
      if (*pTag == iter_method.my_tag()) continue;

      std::string nbr_tag_id = pTag->id();
      ValueType nbr_value = nbr_data_handler.get_data_unsafe(*pTag);

      if (rho_x_.count(nbr_tag_id) == 0)
      {
        rho_x_[nbr_tag_id] = 0.0;
        rho_y_[nbr_tag_id] = 0.0;
      } 
      rho_x_previous_[nbr_tag_id] = rho_x_[nbr_tag_id];
      rho_y_previous_[nbr_tag_id] = rho_y_[nbr_tag_id];
      rho_x_[nbr_tag_id] = std::get<0>(nbr_value);
      rho_y_[nbr_tag_id] = std::get<1>(nbr_value);

      x_value_ = x_value_ + rho_x_[nbr_tag_id] - rho_x_previous_[nbr_tag_id];
      y_value_ = y_value_ + rho_y_[nbr_tag_id] - rho_y_previous_[nbr_tag_id];

      // This is the 'wake up' portion followed by broadcast (push_sum theory relevant)
      sigma_x_ = sigma_x_ + (x_value_ / in_nodes_plus_one_);
      sigma_y_ = sigma_y_ + (y_value_ / in_nodes_plus_one_);

      x_value_ = x_value_ / in_nodes_plus_one_;
      y_value_ = y_value_ / in_nodes_plus_one_;
      
      ++information_count_;
    }
  }

  /** @brief Prepare values to send to neighbors.
   */
  ValueType prepare_for_publication(ValueType)
  {
    return {sigma_x_, sigma_y_, information_count_};
    // vals_to_publish[0] = sigma_x_;
    // vals_to_publish[1] = sigma_y_;
    // vals_to_publish[2] = information_count_;
    // return vals_to_publish;
  }

  /** @brief Returns the current estimate of the global average.
   */
  scalar_t get_value() const
  {
    scalar_t consensus_value = x_value_/y_value_;
    return consensus_value;
  }

  unsigned get_information_count() const
  {
    return information_count_;
  }

  scalar_t get_x() const {return x_value_;}
  scalar_t get_y() const {return y_value_;}
  
private:
  unsigned information_count_ = 0;
  scalar_t x_value_;
  scalar_t y_value_ = 1.0;
  // this is the number of neighbors plus one needed for the update
  // rule
  scalar_t in_nodes_plus_one_;
   // Local weights.
  scalar_t sigma_x_;
  scalar_t sigma_y_;
  // stores iterate information
  using data_id_map = std::unordered_map<std::string, scalar_t>;
  data_id_map rho_x_;
  data_id_map rho_y_;
  data_id_map rho_x_previous_;
  data_id_map rho_y_previous_;
};  // class PushSumProcessor

} // namespace skywing

#endif
