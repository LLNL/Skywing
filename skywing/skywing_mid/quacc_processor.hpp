#ifndef QUACC_PROCESSOR_HPP
#define QUACC_PROCESSOR_HPP

#include "skywing_mid/idempotent_processor.hpp"
#include "skywing_mid/push_sum_processor.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/big_float.hpp"
#include <chrono>
#include <random>
#include <cmath>
#include <iostream>

namespace skywing
{
  using std::exp;
  using std::log;

/** @brief QUasi-Arithmetic Collective Counter
 *
 * This processor implements a gossip method for counting the number
 * of agents in a collective. Consider a set of positive real numbers
 * $x_1, ..., x_n$ such that, for any $i \ne j$ either $|x_i / x_j|
 * \gg 1e5$ or $|x_i / x_j| \ll 1e-5$. That is, all numbers differ in
 * size by many orders of magnitude. Then
 * $$
 * Q = n^{-1} \sum_i exp(-x_i) \approx n^{-1} exp(-min_i(x_i)).
 * $$
 *
 * That is, the average of the exp(-x_i) is basically just the
 * negative exponential of the smallest x_i. Then
 * $$
 * n \approx exp(-(min_i(x_i) + log(Q))).
 * $$
 *
 * Thus, we can approximate the size of the collective $n$ by jointly
 * performing a minimum over the $x_i$ and a mean over the
 * $exp(-x_i)$, and using the above formula. As long as we can
 * guarantee that the approximation is within 0.5 of the correct value
 * with high probability, then we can get the exact count with high
 * probability by rounding.
 *
 * Many distributions are possible for this. Here we use an
 * exponential distribution with a very small lambda parameter because
 * it's analytically tractable.
 *
 * Note, this computation requires a lot of EXTREMELY small numbers,
 * and if you're using IEEE double-precision floating point, you
 * probably only have enough exponent bits to reliable estimate
 * collective sizes of a few dozen or less. Larger collectives require
 * "big floats" with many more exponent bits.
 *
 * @tparam real_t The real type to use. Use a big float for large collectives.
 * @tparam MinProc The processor for gossip minima.
 * @tparam MeanProc The processor for gossip means.
 */
template<typename real_t = BigFloat,
         typename MinProc = MinProcessor<real_t>,
         typename MeanProc = PushFlowProcessor<real_t>>
class QUACCProcessor
{
public:
  using ValueType = std::tuple<typename MinProc::ValueType, typename MeanProc::ValueType>;

  /** @param number_of_neighbors The neighbor count for this agent;
   *  this parameter is a quirk of the Push Sum mean algorithm and is
   *  likely not necessary for something else like Push Flow.
   *  @param lambda Parameter for the exponential distribution.
   */
  template<typename... Args>
  QUACCProcessor(Args&&... args)
    : my_val_(get_exponential_dist_value()),
      min_processor_(my_val_),
      mean_processor_(exp(-my_val_), std::forward<Args>(args)...)
  { }
  
  ValueType get_init_publish_values()
  {
    return ValueType(min_processor_.get_init_publish_values(),
                     mean_processor_.get_init_publish_values());
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler, const IterMethod& iter_method)
  {
    auto min_data_handler = nbr_data_handler.template get_sub_handler<typename MinProc::ValueType>([](const ValueType& v){return std::get<0>(v);});
    min_processor_.process_update(min_data_handler, iter_method);

    auto mean_data_handler = nbr_data_handler.template get_sub_handler<typename MeanProc::ValueType>([](const ValueType& v){return std::get<1>(v);});
    mean_processor_.process_update(mean_data_handler, iter_method);
  }

  ValueType prepare_for_publication(ValueType v)
  {
    return ValueType(min_processor_.prepare_for_publication(std::get<0>(v)),
                     mean_processor_.prepare_for_publication(std::get<1>(v)));
  }

  real_t get_raw_count() const
  {
    return exp(-(log(mean_processor_.get_value())
                 + min_processor_.get_value()));
  }
  size_t get_count() const
  {
    size_t count = static_cast<size_t>(round(static_cast<double>(get_raw_count())));
    return count == 0 ? 1 : count;
  }

  real_t get_min() const {return min_processor_.get_value();}
  real_t get_mean() const
  {
    return mean_processor_.get_value();
    // real_t x_val = mean_processor_.get_x();
    // real_t y_val = mean_processor_.get_y();
    // return x_val / y_val;
  }

private:

  // Draw a value at random from the exponential distribution with
  // parameter lambda
  real_t get_exponential_dist_value()
  {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);

    real_t p = distribution(generator);
    // should actually be 1-p but the distribution of p and
    // distribution of 1-p are the same so it's fine.
    return -log(p) / LAMBDA;
  }

  const real_t LAMBDA = 1e-10;
  real_t my_val_;
  MinProc min_processor_;
  MeanProc mean_processor_;

  
}; // class QUACCProcessor

} // namespace skyne

#endif // QUACC_PROCESSOR_HPP
