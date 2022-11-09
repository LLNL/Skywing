#ifndef SKYNET_NEIGHBOR_DATA_HANDLER_HPP
#define SKYNET_NEIGHBOR_DATA_HANDLER_HPP

#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/pubsub_converter.hpp"

namespace skywing
{
  /** @brief Represents a resilient interface to data of a common type
   *  received from a set of neighbors.
   *
   *  Typically used in an iterative method. Instead of iterative
   *  methods interacting with neighbor data directly, this class
   *  instead provides a set of functions that act as common "building
   *  blocks" in iterative methods. The functionality in this class
   *  can be made resilient to problems in the collective. By using
   *  this class as an interface to neighbor data, iterative algorithm
   *  designers can achieve resilience in a simple and transferrable
   *  manner.
   *
   *  @tparam BaseDataType The type of the underlying data received
   *  from neighbors.
   *
   *  @tparam DataType The sub-type of interest. Often, a user
   *  doesn't want to interact with the \c BaseDataType directly, but
   *  wants to interact with a transformed type. For example, \c
   *  BaseDataType might be an \c std::tuple<T1, T2>, and the user
   *  only needs \c T1.
   */
  template<typename BaseDataType, typename DataType>
  class NeighborDataHandler
  {
    using TagValueType = typename PubSubConverter<BaseDataType>::pubsub_type;
  public:
    using TagType = UnwrapAndApply_t<TagValueType, PublishTag>;

    /** @param transformer A function that converts a \c BaseDataType
     *  object into something of type \c DataType. For example, could
     *   be a function to \c get an element of a \c std::tuple.
     *
     *  @param tags A reference to the tags of neighbor data to which we are subscribed.
     *  @param neighbor_values A reference to the neighbor's values.
     *  @param updated_tags A reference to a vector of tags that have been updated recently.
     *
     *  Note that these final 3 parameters are all *references*, and
     *  we expect the contents to be updated regularly by the
     *  overlaying iterative method manager.
     */
    NeighborDataHandler(std::function<DataType(const BaseDataType&)> transformer,
                        const std::vector<TagType>& tags,
                        const tag_map<TagType, BaseDataType>& neighbor_values,
                        const std::vector<const TagType*>& updated_tags)
      : transformer_(transformer), tags_(tags),
        neighbor_values_(neighbor_values), updated_tags_(updated_tags)
    {}

    /** @brief Get a NeighborDataHandler whose sub-type is a further
     *  transformation of \c DataType.
     */
    template<typename SubDataType>
    NeighborDataHandler<BaseDataType, SubDataType>
    get_sub_handler(std::function<SubDataType(const DataType&)> sub_transformer) const
    {
      return NeighborDataHandler<BaseDataType, SubDataType>
        ([=](const BaseDataType& v){return sub_transformer(transformer_(v));},
         tags_, neighbor_values_, updated_tags_);
    }

    // template<std::size_t index>
    // NeighborDataHandler<BaseDataType, typename std::tuple_element<index, DataType>::type>
    // get_tuple_element_handler() const
    // {
    //   return get_sub_handler<typename std::tuple_element<index, DataType>::type>
    //     ([](DataType& d) { return std::get<index>(d); }
    // }

    /******************************
     * Summation functions
     *****************************/

    /* @brief Compute a sum of neighbor data.
     * @tparam R The return type, \c DataType by default.
     */
    template<typename R = DataType>
    R sum() const { return f_accumulate_<R>(transformer_, std::plus<R>()); }

    /* @brief Compute a weighted sum of neighbor data.
     * @tparam S The coefficient type.
     * @tparam R The return type, \c DataType by default.
     *
     * @param coeffs A map from tag to \c S, representing data coefficients.
     */
    template<typename S = DataType, typename R = DataType>
    R weighted_sum(tag_map<TagType, S>& coeffs) const
    {
      return weighted_f_accumulate_<DataType>
        (transformer_, [&](const TagType& t){return coeffs[t];}, std::plus<R>());
    }

    /* @brief Compute a sum of a function of neighbor data.
     * @tparam R The return type, \c DataType by default.
     *
     * @param f The function to apply to each piece of neighbor data.
     */
    template<typename R>
    R f_sum(std::function<R(const DataType&)> f) const
    {
      return weighted_f_accumulate_<R>([&](const BaseDataType& v){return f(transformer_(v));},
                                      std::plus<R>());
    }

    /******************************
     * Averaging functions
     *****************************/

    /* @brief Compute an average of neighbor data.
     * @tparam R The return type, \c DataType by default.
     */
    template<typename R = DataType>
    R average() const
    {
      R num = sum();
      R denom = f_accumulate_<R>([](const BaseDataType&) { return 1.0; }, std::plus<R>());
      return num / denom;
    }

    /* @brief Compute a weighted average af neighbor data.
     * @tparam S The coefficient type.
     * @tparam R The return type, \c DataType by default.
     *
     * @param coeffs A map from tag to \c S, representing data coefficients/weights.
     */
    template<typename S = DataType, typename R = DataType>
    R weighted_average(tag_map<TagType, S> coeffs) const
    {
      DataType num = weighted_sum<S, R>(std::move(coeffs));
      DataType denom = weighted_f_accumulate_<S>([&](const TagType& t){return coeffs[t];},
                                                 std::plus<S>());
      return num / denom;
    }

    /******************************
     * Other useful functions
     *****************************/

    /* @brief Get the number of neighbors.
     */
    std::size_t num_neighbors() const { return tags_.size(); }

    /* @brief Compute an accumulatation of neighbor data.
     * @tparam R The return type.
     *
     * @param f The function to apply to each piece of neighbor data.
     * @param binary_op The binary operation for accumulting
     * data. Must be associative and commutative. For example, could
     * be a max or set union operator.
     */
    template<typename R>
    R f_accumulate(std::function<R(const DataType&)> f, std::function<R(R, R)> binary_op) const
    {
      return f_accumulate_<R>([&](const BaseDataType& v){return f(transformer_(v));},
                             std::move(binary_op));
    }

    /* @brief Get direct, unsafe access to underlying neighbor data.
     *
     * WARNING: Use of this function circumvents all resilience
     * benefits provided by this interface. Therefore, use of this
     * function means that you are promising to provide your own
     * resilience functionality, and this function should not be used
     * unless you are doing that.
     */
    DataType get_data_unsafe(const TagType& tag) const
    {
      return transformer_(neighbor_values_.at(tag));
    }

    /* @brief Get the tags that have been updated since the last data "get".
     */
    const std::vector<const TagType*>& get_updated_tags() const { return updated_tags_; }
    
  private:

    //TODO: Improve efficiency of these functions to not look up keys
    //in neighbor_values_ twice (currently it's once to check that it
    //exists and once to use it).

    /** @brief Compute an affine accumulation of a function of the data, \f$s + \sum_i c_i f(x_i)\f$.
     *
     * @tparam R The output type of the computation.
     * @tparam S The coefficient type \f$c_i\f$.
     *
     * @param f The function applied to the \c BaseDataType \f$x_i\f$.
     * @param coef The coefficients, represented as a function from a \c TagType to \c S.
     * @param binary_op The binary accumulation function, such as \c std::plus<R>.
     * @param shift The affine shift constant of type \c R.
     */
    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const BaseDataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op,
                             R* shift) const
    {
      auto tag_iter = tags_.cbegin();
      while (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) ++tag_iter;
      R val = binary_op(*shift, coef(*tag_iter) * f(neighbor_values_[*tag_iter]));
      
      ++tag_iter;
      for (; tag_iter != tags_.cend(); ++tag_iter)
      {
        if (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) continue;
        val = binary_op(std::move(val), coef(*tag_iter) * f(neighbor_values_.at(*tag_iter)));
      }
      return val;
    }

    /** @brief Compute a linear accumulation of a function of the data, \f$\sum_i c_i f(x_i)\f$.
     *
     * @tparam R The output type of the computation.
     * @tparam S The coefficient type \f$c_i\f$.
     *
     * @param f The function applied to the \c BaseDataType \f$x_i\f$.
     * @param coef The coefficients, represented as a function from a \c TagType to \c S.
     * @param binary_op The binary accumulation function, such as \c std::plus<R>.
     */
    template<typename R, typename S>
    R weighted_f_accumulate_(std::function<R(const BaseDataType&)> f,
                             std::function<S(const TagType&)> coef,
                             std::function<R(R, R)> binary_op) const
    {
      auto tag_iter = tags_.cbegin();
      while (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) ++tag_iter;
      R val = coef(*tag_iter) * f(neighbor_values_.at(*tag_iter));
      
      ++tag_iter;
      for (; tag_iter != tags_.cend(); ++tag_iter)
      {
        if (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) continue;
        val = binary_op(std::move(val), coef(*tag_iter) * f(neighbor_values_.at(*tag_iter)));
      }
      return val;
    }

    /** @brief Compute an accumulation of a function of the data, \f$\sum_i f(x_i)\f$.
     *
     * @tparam R The output type of the computation.
     *
     * @param f The function applied to the \c BaseDataType \f$x_i\f$.
     * @param binary_op The binary accumulation function, such as \c std::plus<R>.
     */
    template<typename R>
    R f_accumulate_(std::function<R(const BaseDataType&)> f,
                    std::function<R(R, R)> binary_op) const
    {
      // find starting existing value
      auto tag_iter = tags_.cbegin();
      while (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) ++tag_iter;
      R val = f(neighbor_values_.at(*tag_iter));
      
      ++tag_iter;
      for (; tag_iter != tags_.cend(); ++tag_iter)
      {
        if (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) continue;
        val = binary_op(std::move(val), f(neighbor_values_.at(*tag_iter)));
      }
      return val;
    }

    /** @brief Compute an accumulation of tag coefficients.
     *
     * @tparam R The output type of the computation.
     *
     * @param coef The coefficients, represented as a function from a \c TagType to \c R.
     * @param binary_op The binary accumulation function, such as \c std::plus<R>.
     *
     * Note that this function doesn't use the neighbor data, but it
     * does react to changes in which neighbors are active/alive. This
     * function is often useful for computing denominators in weighted
     * sums.
     */
    template<typename R>
    R weighted_f_accumulate_(std::function<R(const TagType&)> coef,
                             std::function<R(R, R)> binary_op) const
    {
      auto tag_iter = tags_.cbegin();
      while (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) ++tag_iter;
      R val = coef(*tag_iter);
      
      ++tag_iter;
      for (; tag_iter != tags_.cend(); ++tag_iter)
      {
        if (neighbor_values_.find(*tag_iter) == neighbor_values_.end()) continue;
        val = binary_op(std::move(val), coef(*tag_iter));
      }
      return val;
    }

    std::function<DataType(const BaseDataType&)> transformer_;
    const std::vector<TagType>& tags_;
    const tag_map<TagType, BaseDataType>& neighbor_values_;
    const std::vector<const TagType*>& updated_tags_;
  }; // class NeighborDataHandler

} // namespace skywing

#endif // SKYNET_NEIGHBOR_DATA_HANDLER_HPP
