#ifndef SKYNET_MID_INTERNAL_ITERATIVE_BASE_HPP
#define SKYNET_MID_INTERNAL_ITERATIVE_BASE_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/pubsub_converter.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"
#include "skywing_mid/neighbor_data_handler.hpp"

namespace skywing {

/** @brief Base class for iterative methods.
 * 
 * @param ResiliencePolicy Determines how this IterativeMethod
 * should respond to problems such as dead neighbors.
 *
 * @tparam DataType The type of data that this iterative method will
 * send to its neighbors. Not necessarily in pubsub type; that
 * conversion will be handled by IterativeMethod.
 * 
 */
template<typename ResiliencePolicy, typename DataType>
class IterativeMethod {
public:
  using ThisT = IterativeMethod<ResiliencePolicy, DataType>;
  using TagValueType = typename PubSubConverter<DataType>::pubsub_type; // std::tuple<stuff...>
  using TagType = UnwrapAndApply_t<TagValueType, PublishTag>; // PublishTag<stuff...>;
  using DataT = DataType;
  using ValueType = DataType;

  /** @param job The job running this iterative method.
   *  @param produced_tag The tag this agent will publish during iteration.
   *  @param tags The set of tags consumed during iteration, possibly including @p produced_tag
   *  @param resilience_policy The ResiliencePolicy object used in iteration.
   */
  IterativeMethod(
    Job& job,
    const TagType& produced_tag,
    const std::vector<TagType>& tags,
    ResiliencePolicy resilience_policy) noexcept
    : job_{&job}, produced_tag_{produced_tag}, tags_{tags},
      resilience_policy_(resilience_policy)
  {
    for (auto tag_iter = tags_.begin(); tag_iter != tags_.end();) {
      if (!job_->tag_has_active_publisher(*tag_iter)) {
        dead_tags_.push_back(std::move(*tag_iter));
        tag_iter = tags_.erase(tag_iter);
      }
      else {
        ++tag_iter;
      }
    }
  }

  const TagType& my_tag() const { return produced_tag_; }

  /** @brief When a neighbor dies, record and react appropriately.
   *
   * The exact behavior here depends on the ResiliencePolicy.
   */
  template<typename TagIter>
  TagIter handle_dead_neighbor(const TagIter& tag_iter) noexcept
  {
    dead_tags_.push_back(std::move(*tag_iter));
    resilience_policy_.handle_dead_neighbor(*this, tag_iter);
    return tags_.erase(tag_iter);
  }
  
  /** @brief Rebuilds connections for dead tags
   */
  Waiter<void> rebuild_dead_tags() noexcept
  {
    auto to_ret = job_->rebuild_tags(dead_tags_);
    std::move(dead_tags_.begin(), dead_tags_.end(), std::back_inserter(tags_));
    dead_tags_.clear();
    return to_ret;
  }

  /** @brief Rebuilds the specified dead tags, ignoring any tags that aren't dead
   */
  template<typename Range>
  Waiter<void> rebuild_dead_tags_range(const Range& r) noexcept
  {
    std::vector<TagType> search_tags;
    for (const auto& tag : r) {
      auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) {
        search_tags.push_back(std::move(*iter));
        dead_tags_.erase(*iter);
      }
    }
    return job_->rebuild_tags(search_tags);
  }

  /** @brief Rebuilds the specified dead tags
   */
  template<typename... Tags>
  Waiter<void> rebuild_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
    return rebuild_dead_tags_range(tag_array);
  }

  /** @brief Drops tracking for dead tags
   */
  void drop_dead_tags() noexcept
  {
    // TODO: Actually unsubscribe when that's a thing that can happen
    dead_tags_.clear();
  }

  /** @brief Drops tracking for specific tags, does nothing if the tags aren't dead
   */
  template<typename Range>
  void drop_dead_tags(const Range& r) noexcept
  {
    for (const auto& tag : r) {
      const auto iter = std::find(dead_tags_.begin(), dead_tags_.end(), tag.id());
      if (iter != dead_tags_.end()) { dead_tags_.erase(iter); }
    }
  }

  /** @brief Drops the specified dead tags
   */
  template<typename... Tags>
  void drop_dead_tags(const Tags&... tags) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const std::array<internal::PublishTagBase, sizeof...(Tags)> tag_array{
      static_cast<internal::PublishTagBase>(tags)...};
  }

  /** @brief Gather any data that has been published by neighbors.
   *
   * If any neighbors have died, this is where that will be detected
   * and handled. This function does not worry about whether or not
   * all neighbors, or even any neighbors, have ready data. So in the
   * case of a SynchronousIterative method, for example, it is the
   * responsibility of that derived class to ensure it is time to call
   * this function before doing so.
   *
   * @returns true if any new data is received, false otherwise.
   */
  bool gather_values()
  {
    auto tag_iter = tags_.begin();
    size_t num_updated = 0;
    // Go through tags, detect any that have died and detect any that
    // have available data to read.
    while (tag_iter != tags_.cend())
    {
      const auto& tag = *tag_iter;
      if (!job_->tag_has_active_publisher(tag))
      {
        tag_iter = handle_dead_neighbor(tag_iter);
        continue;
      }
      if (job_->has_data(tag)) num_updated++;
      ++tag_iter;
    }
    if (num_updated == 0) return false;

    // For the neighbors that are alive and have ready data, read in
    // the data. Record which tags have been updated.
    updated_tags_.clear();
    updated_tags_.reserve(num_updated);
    for (const auto& tag : tags_)
    {
      if (job_->has_data(tag))
      {
        const auto value_opt = job_->get_waiter(tag).get();
        // an assert...doesn't seem like the right way to handle this
        assert(value_opt);
        // convert the pubsub_type back into the required DataType
        neighbor_values_[tag] = PubSubConverter<DataType>::deconvert(*value_opt);
        updated_tags_.push_back(&tag);
      }
    }
    return true;
  }

  /** @brief Publish this agent's values for its neighbors.
   */
  auto submit_values(DataType value_to_submit) noexcept
  {
    TagValueType to_publish = PubSubConverter<DataType>::convert(std::move(value_to_submit));
    job_->publish_tuple(produced_tag_, to_publish);
  }


  /** @brief Returns all active tags
   */
  const std::vector<TagType>& tags() const noexcept { return tags_; }

  /** @brief Returns all dead tags
   */
  const std::vector<TagType>& dead_tags() const noexcept { return dead_tags_; }

  Job& get_job() const noexcept { return *job_; }

  /** @brief Get a NeighborDataHandler to the recieved data.
   *
   *  The NeighborDataHandler acts as a resilient interface to data,
   *  so that client iterative methods can leverage that resilience
   *  without implementing it themselves.
   *
   *  Often, the raw DataType is a concatenation of values used by
   *  different policies in the IterativeMethod, and so we want to
   *  only present an interface to an element of the DataType. This
   *  function enables getting a NeighborDataHandler that provides an
   *  interface to only the underlying data of interest.
   *
   * @param f A function that converts an object of type DataType into
   * something of type SubDataType. For example, a common function to
   * pass here would be the tuple element-getter function 
   * <tt>[](const DataType& v){return std::get<0>(v);}</tt>
   */ 
  template<typename SubDataType>
  NeighborDataHandler<DataType, SubDataType>
  get_neighbor_data_handler(std::function<SubDataType(const DataType& v)> f)
  {
    return NeighborDataHandler<DataType, SubDataType>
      (std::move(f), tags_, neighbor_values_, updated_tags_);
  }

protected:

  /** @brief Get the NeighborDataHandler for the Policy data.
   *
   * If the Policy does not define a ValueType, then attempting to
   * instantiate this function will induce a compile-time error.
   */
  template<typename Policy, typename IterMethod>
  NeighborDataHandler<ValueType, typename Policy::ValueType>
  get_policy_data_handler()
  {
    using ret_t = typename Policy::ValueType;
    constexpr std::size_t ind = IndexInPublishers<Policy, IterMethod>::index;
    return this->template get_neighbor_data_handler<ret_t>
      ([](const ValueType& v){return std::get<ind>(v);});
  }

  /** @brief Ask this Policy to process an update.
   *
   * This function is to be called only if Policy defines ValueType.
   *
   * @param policy the Policy object to update.
   * @param std::true_type Flag to create overload resolution.
   */
  template<typename Policy, typename IterMethod>
  void process_policy_update_(Policy& policy, std::true_type)
  {
    policy.process_update(get_policy_data_handler<Policy, IterMethod>(), *this);
  }
  /** @brief Ask this Policy to process an update.
   *
   * This function is to be called only if Policy does NOT define ValueType.
   *
   * @param policy the Policy object to update.
   * @param std::false_type Flag to create overload resolution.
   */
  template<typename Policy, typename IterMethod>
  void process_policy_update_(Policy&, std::false_type)
  {}

  /** @brief Ask a policy for its initial value, if it produces any.
   *
   *  Wraps that value in a std::tuple of length 1. If the Policy does
   *  not publish anything, returns a std::tuple<>. The purpose of
   *  this is to be included in a call to std::tuple_cat.
   *
   * For example, a derived IterMethod type might call
   * \code{.cpp}
   * std::tuple_cat
   *   (this->template get_init_tuple_<Processor, ThisT>(processor_),
   *    this->template get_init_tuple_<StopPolicy, ThisT>(stop_policy_),
   *    this->template get_init_tuple_<ResiliencePolicy, ThisT>(this->resilience_policy_));
   * \code
   */
  template<typename Policy, typename IterMethod>
  auto get_init_tuple_(Policy& policy_obj)
  {
    using PubTup = typename IfHasValueType<Policy>::tuple_of_value_type;
    if constexpr (std::tuple_size_v<PubTup> == 0) return std::tuple<>();
    else return PubTup(policy_obj.get_init_publish_values());
  }  

  /** @brief Ask a policy for the value it wants to publish, if it
   * produces any.
   *
   *  Wraps that value in a std::tuple of length 1. If the Policy does
   *  not publish anything, returns a std::tuple<>. The purpose of
   *  this is to be included in a call to std::tuple_cat.
   *
   * For example, a derived IterMethod type might call
   * \code{.cpp}
   * std::tuple_cat
   *   (this->template get_pub_tuple_<Processor, ThisT>(processor_, publish_values_),
   *    this->template get_pub_tuple_<StopPolicy, ThisT>(stop_policy_, publish_values_),
   *    this->template get_pub_tuple_<ResiliencePolicy, ThisT>(this->resilience_policy_, publish_values_));
   * \code
   */
  template<typename Policy, typename IterMethod>
  auto get_pub_tuple_(Policy& policy_obj, const typename IterMethod::ValueType& vals)
  {
    using PubTup = typename IfHasValueType<Policy>::tuple_of_value_type;
    if constexpr (std::tuple_size_v<PubTup> == 0) return std::tuple<>();
    else
    {
      constexpr std::size_t ind = IndexInPublishers<Policy, IterMethod>::index;
      return PubTup(policy_obj.prepare_for_publication(std::get<ind>(vals)));
    }
  }

  
  Job* job_;
  TagType produced_tag_;
  std::vector<TagType> tags_;
  std::vector<TagType> dead_tags_;
  ResiliencePolicy resilience_policy_;

private:
  tag_map<TagType, DataType> neighbor_values_;
  std::vector<const TagType*> updated_tags_;

  template<typename Callable, typename IterMethod>
  friend class NeighborDataHandler;
}; // class IterativeBase

} // namespace skywing

#endif // SKYNET_MID_INTERNAL_ITERATIVE_BASE_HPP
