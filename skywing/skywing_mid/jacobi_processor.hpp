#ifndef SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

namespace skywing
{

/** 
 * @brief A Processor used in an IterativeMethod for solving square linear systems of equations Ax=b.
 *
 * This processor applies the Jacobi update method. Convergence is
 * guaranteed when the matrix A is square and strictly diagonally
 * dominant. This class stores the full solution vector x_iter at each
 * machine as well as the partition of x_iter corresponding to the
 * partition Ax=b that each machine is responsible for updating.
 * 
 * This processor supports overlapping computations, nonuniform
 * partitioning, and non-sequential partitioning of the linear
 * system. This is why the row_index for each component of x must be
 * sent along with every method for proper processing.
 * 
 * @tparam E The matrix and vector element type to be used. Most often
 * a scalar type such as @p double, but could also be more complex
 * types. For example, if E is a matrix type with proper operator
 * overloading, this processor can be used to implement a block Jacobi
 * method.
*/
template<typename E = double>
class JacobiProcessor
{
public:
  using element_t = E;
  using ValueType = std::vector<element_t>;
  using ValueTag = skywing::PublishTag<ValueType>;

  /**
   * @param A_partition A matrix row partition.
   * @param b_partition b vector row partition, same parition as A_partition.
   * @param row_indices indices which correspond to the row the partition above with respect to the matrix A.
   */
  JacobiProcessor(
    std::vector<std::vector<element_t>> A_partition,
    std::vector<element_t> b_partition,
    std::vector<size_t> row_indices)
    : A_partition_(A_partition),
      b_partition_(b_partition),
      row_indices_(row_indices),
      number_of_updated_components_(row_indices_.size()),
      x_iter_(A_partition_[0].size(), 0.0),
      publish_values_(2 * number_of_updated_components_, 0.0)
  {
    jacobi_computation();
  }

  /** @brief Initialize values to communicate.
   *
   *  Must send an index along with each value, so vector length is
   *  twice the number of components updated.
   */
  ValueType get_init_publish_values()
  { return std::vector<element_t>(2 * number_of_updated_components_, 0.0); }

  /** @brief Process an update with a set of new neighbor values.
   *
   * @param nbr_tags The tags of the updated values. Each tag is an element of this->tags_.
   * @param nbr_values The new values from the neighbors.
   * @param caller The iterative wrapper calling this method.
   */
  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      [[maybe_unused]] const IterMethod&)
  {
    for (const auto& pTag : nbr_data_handler.get_updated_tags())
    {
      ValueType nbr_value = nbr_data_handler.get_data_unsafe(*pTag);
      // This cycles through the received_values in order not to
      // replace a component that each process is updating with
      // another processes update if there's overlapping
      // computations.  Since messages of the form [component index
      // ; component], we have to parse these messages in pairs,
      // which is easier to do without iterators.
      for(size_t nbr_vals_ind = 0; nbr_vals_ind < (nbr_value.size()/2); nbr_vals_ind++)
      {
        bool use_this_value = true;
        // Cycles through individual values in row_index to avoid
        // replacing its own updates if there's overlap in the
        // linear system partition.
        for(size_t row_index_cycle = 0 ; row_index_cycle < number_of_updated_components_; row_index_cycle++)
        {
          if(nbr_value[nbr_vals_ind*2] == row_indices_[row_index_cycle])
            use_this_value = false;
        }
        if(use_this_value)
        {
          size_t updated_index = static_cast<size_t>(nbr_value[nbr_vals_ind * 2]);
          x_iter_[updated_index] = nbr_value[nbr_vals_ind * 2 + 1];
          jacobi_computation();
        }
      }
    }
  }
  // template<typename CallerT>
  // void process_update([[maybe_unused]] const std::vector<ValueTag>& nbr_tags,
  //                     const std::vector<ValueType>& nbr_values,
  //                     [[maybe_unused]] const CallerT& caller)
  // {
  //   for (size_t i = 0; i < nbr_values.size(); i++)
  //   {
  //     const ValueType& nbr_value = nbr_values[i];
  //     // This cycles through the received_values in order not to
  //     // replace a component that each process is updating with
  //     // another processes update if there's overlapping
  //     // computations.  Since messages of the form [component index
  //     // ; component], we have to parse these messages in pairs,
  //     // which is easier to do without iterators.
  //     for(size_t nbr_vals_ind = 0; nbr_vals_ind < (nbr_value.size()/2); nbr_vals_ind++)
  //     {
  //       bool use_this_value = true;
  //       // Cycles through individual values in row_index to avoid
  //       // replacing its own updates if there's overlap in the
  //       // linear system partition.
  //       for(size_t row_index_cycle = 0 ; row_index_cycle < number_of_updated_components_; row_index_cycle++)
  //       {
  //         if(nbr_value[nbr_vals_ind*2] == row_indices_[row_index_cycle])
  //           use_this_value = false;
  //       }
  //       if(use_this_value)
  //       {
  //         size_t updated_index = static_cast<size_t>(nbr_value[nbr_vals_ind * 2]);
  //         x_iter_[updated_index] = nbr_value[nbr_vals_ind * 2 + 1];
  //         jacobi_computation();
  //       }
  //     }
  //   }
  // }

  /** @brief Prepare values to send to neighbors.
   */
  ValueType prepare_for_publication(ValueType vals_to_publish)
  {
    for(size_t i = 0 ; i < number_of_updated_components_; i ++)
    {
      vals_to_publish[i*2] = row_indices_[i]*1.0;
      vals_to_publish[i*2+1] = x_iter_[row_indices_[i]];
    }
    return vals_to_publish;
  }
  
  /** @brief Return only the components for which this process updates. 
   */
  std::vector<element_t> return_partition_solution() const
  {
    std::vector<element_t> return_vector;
    for(size_t i = 0 ; i < number_of_updated_components_; i++)
    {
      return_vector.push_back(x_iter_[row_indices_[i]]);
    }
    return return_vector;
  }

  /** @brief Return the full solution.
   */
  const std::vector<element_t>& return_full_solution() const
  {
    return x_iter_;
  }

private:

  /** @brief Perform the Jacobi update.
   */
  void jacobi_computation()
  {
    for(size_t i = 0 ; i < number_of_updated_components_; i++)
    {
      element_t hold = 0;
      for(size_t j = 0 ; j < A_partition_[0].size(); j++)
      {
        if(j!=row_indices_[i])
          hold += A_partition_[i][j]*x_iter_[j];
      }
      hold = (b_partition_[i] - hold)/A_partition_[i][row_indices_[i]];
      size_t updated_index = row_indices_[i];
      x_iter_[updated_index] = hold;
    }
  }


  std::vector<std::vector<element_t>> A_partition_;
  std::vector<element_t> b_partition_;
  std::vector<size_t> row_indices_;
  size_t number_of_updated_components_;
  size_t iteration_count_ = 0;

  // Variables internal to this class.
  std::vector<element_t> x_iter_;
  std::vector<element_t> publish_values_;
}; // class JacobiProcessor

}

#endif 
