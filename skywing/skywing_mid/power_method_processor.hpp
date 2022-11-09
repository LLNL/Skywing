#ifndef POWER_METHOD_HPP
#define POWER_METHOD_HPP

#include "skywing_mid/sum_processor.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/associative_vector.hpp"

#include <cmath>

namespace skywing
{

/** @brief Executes the Power Method (aka Power Iteration) for
 *  computing the maximum-magnitude eigenvalue of a matrix A.
 *
 *  The matrix A is stored in a distributed fashion across the
 *  collective. Each agent stores one column of A. The rows and
 *  columns are indexed associatively rather than numerically; as a
 *  result, there is no inherent ordering to the rows and columns of
 *  the matrix.
 *
 * @tparam index_t The associative indexing type. Most often either
 * std::size_t or std::string.
 * @tparam scalar_t The scalar type.
 */
template<typename index_t, typename scalar_t = double>
class PowerMethodProcessor
{
public:
  // The AssociativeVector type used to store matrix columns as well
  // as input and output vectors. It is a CLOSED AssociativeVector.
  using AssocVec_t = AssociativeVector<index_t, scalar_t, false>;
  using OpenAssocVec_t = AssociativeVector<index_t, scalar_t, true>;
  // Summation processor for computing the squared norm of $Ax$.
  using SqNormSumProc_t =
    SumProcessor<scalar_t, PushFlowProcessor<double>>;
  // Values sent by this processor.
  using ValueType = std::tuple<AssocVec_t,
                               typename SqNormSumProc_t::ValueType>;

  PowerMethodProcessor(AssocVec_t my_column,
                       index_t my_index)
    : my_index_(my_index),
      my_eigvec_element_(1.0),
      eigenvalue_estimate_(1.0),
      my_column_(my_column),
      sqnorm_sum_processor_(my_eigvec_element_ * my_eigvec_element_)
  {}

  ValueType get_init_publish_values()
  {
    return ValueType(my_eigvec_element_ * my_column_,
                     sqnorm_sum_processor_.get_init_publish_values());
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      const IterMethod& iter_method)
  {
    auto sqnorm_data_handler = nbr_data_handler.template get_sub_handler<typename SqNormSumProc_t::ValueType>([](const ValueType& v){return std::get<1>(v);});
    sqnorm_sum_processor_.process_update(sqnorm_data_handler, iter_method);

    // compute estimate of eigenvalue
    scalar_t sqnorm = sqnorm_sum_processor_.get_value();
    if (sqnorm > 0)
      eigenvalue_estimate_ = sqrt(sqnorm_sum_processor_.get_value());

    // get estimate of my portion of Ax
    auto vec_data_handler = nbr_data_handler.template get_sub_handler<AssocVec_t>([](const ValueType& v){return std::get<0>(v);});
    AssocVec_t Ax = vec_data_handler.sum();

    // update estimate of normalized eigenvector element
    scalar_t my_Ax = Ax.at(my_index_);
    if (abs(eigenvalue_estimate_) > 1e-2)
      my_eigvec_element_ = 0.75 * my_eigvec_element_ + 0.25 * (my_Ax / eigenvalue_estimate_);
      // my_eigvec_element_ = my_Ax / eigenvalue_estimate_;

    // update contribution to norm of matvec output
    sqnorm_sum_processor_.set_value(my_Ax * my_Ax);
  }

  ValueType prepare_for_publication(ValueType v)
  {
    return ValueType(my_eigvec_element_ * my_column_,
                     sqnorm_sum_processor_.prepare_for_publication(std::get<1>(v)));
  }

  scalar_t get_eigenvalue() const { return eigenvalue_estimate_; }
  scalar_t get_eigenvector_element() const { return my_eigvec_element_; }
  
private:
  // my index in the eigenvector
  index_t my_index_;
  // my current estimate for my element of the eigenvector $x$.
  scalar_t my_eigvec_element_;
  // current maximum eigenvalue estimate
  scalar_t eigenvalue_estimate_;
  // my column of the matrix A
  AssocVec_t my_column_;

  SqNormSumProc_t sqnorm_sum_processor_;
}; // class PowerMethodProcessor
  
} // namespace skywing

#endif // POWER_METHOD_HPP
