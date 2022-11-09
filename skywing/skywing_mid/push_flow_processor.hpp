#ifndef PUSH_FLOW_HPP
#define PUSH_FLOW_HPP

#include <algorithm>
#include <tuple>
#include <vector>
#include <unordered_map>

namespace skywing
{

enum UpdateType { ORIGINAL, ALL };

/** @brief The Push Flow algorithm to gossip means.
 *
 * Push Flow is an asynchronous gossip method for calculating
 * arithmetic means of a collection of numbers. Unlike Push Sum, which
 * enforces conservation of mass throughout the collective, this
 * method enforces a conservation of FLOW. This method seems to be a
 * bit slower than Push Sum; however, it is more straightforward to
 * alter values and adapt to changes with this method than with Push
 * Sum, and does not require knowing an agent's neighborhood size
 * ahead of time.
 *
 * @tparam data_t The data type of the mean. Must implement addition,
 * divide-by-scalar, and multiply-by-weight_t operators.
 *
 * @tparam weight_t The data type of the weight. Must implement
 * addition and multiply-by-data_t operators.
 *
 * @tparam update_type Enum indicating whether to use the
 * originally-published update method, which selects a neighbor
 * uniformly at random to update, or our variation, which updates all
 * neighbors every time. As far as we can tell our variation is
 * faster.
 */
template<typename data_t = double, typename weight_t = double,
         UpdateType update_type = ALL>
class PushFlowProcessor
{
public:
  using ValueType = std::tuple<std::unordered_map<std::string, data_t>,
                               std::unordered_map<std::string, weight_t>>;

  PushFlowProcessor(data_t my_val)
    : my_val_(my_val), my_weight_(1),
      curr_num_(my_val_), curr_denom_(my_weight_),
      information_count_(0)
  {}

  PushFlowProcessor(data_t my_val, weight_t my_weight)
    : my_val_(my_val), my_weight_(my_weight),
      curr_num_(my_val_), curr_denom_(my_weight_),
      information_count_(0)
  {}

  ValueType get_init_publish_values()
  {
    return {f_ij_num_, f_ij_denom_};
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      const IterMethod& iter_method)
  {
    std::string my_id = iter_method.my_tag().id();
    for (const auto& pTag : nbr_data_handler.get_updated_tags())
    {
      if (*pTag == iter_method.my_tag()) continue;

      const ValueType& nbr_data = nbr_data_handler.get_data_unsafe(*pTag);
      f_ij_num_[pTag->id()]
        = -get_if_present_or_default<0>(nbr_data, my_id);
      f_ij_denom_[pTag->id()]
        = -get_if_present_or_default<1>(nbr_data, my_id);

      ++information_count_;
    }

    curr_num_ = my_weight_ * my_val_;
    for (const auto& nbr_val : f_ij_num_) curr_num_ -= nbr_val.second;
    curr_denom_ = my_weight_;
    for (const auto& nbr_val : f_ij_denom_) curr_denom_ -= nbr_val.second;
  }

  ValueType prepare_for_publication(ValueType)
  {
    update_fij_to_send();
    return {f_ij_num_, f_ij_denom_};
  }

  data_t get_value() const { return curr_num_ / curr_denom_; }
  size_t get_information_count() const { return information_count_; }

  void set_value(data_t new_val) { my_val_ = new_val; }
  void set_weight(data_t new_weight) { my_weight_ = new_weight; }

private:
  template<int index>
  std::conditional_t<index==0, data_t, weight_t>
  get_if_present_or_default(const ValueType& nbr_data,
                            const std::string& my_id)
  {
    if (std::get<index>(nbr_data).count(my_id))
      return std::get<index>(nbr_data).at(my_id);
    else return 0;
  }

  void update_fij_to_send()
  {
    if (f_ij_num_.size() == 0) return;
    if constexpr (update_type == UpdateType::ALL)
    {
      // update all neighbors
      for (auto& iter : f_ij_num_)
        iter.second = iter.second + (curr_num_ / (1 + f_ij_num_.size()));
      for (auto& iter : f_ij_denom_)
        iter.second = iter.second + (curr_denom_ / (1 + f_ij_denom_.size()));
    }
    else
    {
      // original method, select a random neighbor to update
      std::mt19937 gen{std::random_device{}()};
      auto iter = f_ij_num_.begin();
      std::uniform_int_distribution<>
        dis(0, std::distance(iter, f_ij_num_.end()) - 1);
      std::advance(iter, dis(gen));
      f_ij_num_[iter->first] = f_ij_num_[iter->first] + (curr_num_ / 2);
      f_ij_denom_[iter->first] = f_ij_denom_[iter->first] + (curr_denom_ / 2);
    }
  }
  
  data_t my_val_;
  weight_t my_weight_;

  data_t curr_num_;
  weight_t curr_denom_;
  std::unordered_map<std::string, data_t> f_ij_num_;
  std::unordered_map<std::string, weight_t> f_ij_denom_;

  size_t information_count_;
}; // class PushFlowProcessor
} // namespace skywing

#endif // PUSH_FLOW_HPP
