#ifndef SUM_PROCESSOR_HPP
#define SUM_PROCESSOR_HPP

#include "skywing_mid/push_sum_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include <iostream>

namespace skywing
{

/** @brief Processor for computing gossip sums.
 *
 *  This processor is basically just a mean processor and a collective
 *  count processor together. It computes the mean, then multiplies
 *  the result by the collective count.
 *
 *  The requirement is that the data type must be something that can
 *  be sensibly multiplied by an integer.
 *
 * @tparam data_t The data type to sum up.
 * @tparam MeanProcessor The processor for computing the mean.
 * @tparam CountProcessor The processor for computing the collective count.
 */
template<typename data_t,
         typename MeanProcessor=PushSumProcessor<data_t>,
         typename CountProcessor=QUACCProcessor<>>
class SumProcessor
{
public:
  using ValueType = std::tuple<typename MeanProcessor::ValueType, typename CountProcessor::ValueType>;

  template<typename... Args>
  SumProcessor(data_t my_value,
               Args&&... args)
    : mean_processor_(my_value, std::forward<Args>(args)...),
      count_processor_(std::forward<Args>(args)...)
  {
    std::cout << "Starting value " << my_value << std::endl;
  }

  ValueType get_init_publish_values()
  {
    return ValueType(mean_processor_.get_init_publish_values(),
                     count_processor_.get_init_publish_values());
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler, const IterMethod& iter_method)
  {
    auto mean_data_handler = nbr_data_handler.template get_sub_handler<typename MeanProcessor::ValueType>([](const ValueType& v){return std::get<0>(v);});
    mean_processor_.process_update(mean_data_handler, iter_method);

    auto count_data_handler = nbr_data_handler.template get_sub_handler<typename CountProcessor::ValueType>([](const ValueType& v){return std::get<1>(v);});
    count_processor_.process_update(count_data_handler, iter_method);
  }

  ValueType prepare_for_publication(ValueType v)
  {
    return ValueType(mean_processor_.prepare_for_publication(std::get<0>(v)),
                     count_processor_.prepare_for_publication(std::get<1>(v)));
  }

  data_t get_value() const
  {
    return count_processor_.get_count() * mean_processor_.get_value();
  }
  void set_value(data_t new_val)
  {
    mean_processor_.set_value(std::move(new_val));
  }

  size_t get_information_count() const
  { return mean_processor_.get_information_count(); }

private:
  MeanProcessor mean_processor_;
  CountProcessor count_processor_;
}; // class SumProcessor
  
} // namespace skywing

#endif // SUM_PROCESSOR_HPP
