#ifndef SKYNET_MID_ASYNCHRONOUS_ITERATIVE_HPP
#define SKYNET_MID_ASYNCHRONOUS_ITERATIVE_HPP

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/iterative_method.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/iterative_resilience_policies.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <condition_variable>

namespace skywing {
using namespace std::chrono_literals;
  
/**
 * @brief A decentralized, fully-asynchronous iterative method.
 * 
 * This class template implements the framework of an iterative method
 * in which an agent performs a local computation, and possibly sends
 * updates to neighbors, as soon as it receives any update. It does
 * not wait for all neighbors to send updates.
 *
 * Can be constructed directly, but in most cases is easiest to build
 * through the WaiterBuilder class.
 *
 * @tparam Processor The numerical heart of the iterative method. Must
 * define a type @p ValueType of the data type to communicate, as well
 * as the following member functions: 
 * - @p ValueType get_init_publish_values()
 * - @p void template<typename CallerT> process_update(const std::vector<ValueTag>&, const std::vector<ValueType>&, const CallerT&)
 * - @p ValueType prepare_for_publication(ValueType)
 *
 * @tparam PublishPolicy Determines when it is worth sending an
 * update to its neighbors. Must define a member function 
 * @p bool operator()(const ValueType& new_vals, const ValueType& old_vals)
 *
 * @tparam StopPolicy Determines when to stop the
 * iteration. Must define a member function @ bool operator()(constCallerT&)
 *
 * @tparam ResiliencePolicy Determines how this IterativeMethod
 * should respond to problems such as dead neighbors.
 */
template<typename Processor, typename PublishPolicy, typename StopPolicy, typename ResiliencePolicy>
class AsynchronousIterative :
    public IterativeMethod<ResiliencePolicy,
                           TupleOfValueTypes_t<Processor, PublishPolicy, StopPolicy, ResiliencePolicy>>
{  
public:
  using ValueType = TupleOfValueTypes_t<Processor, PublishPolicy, StopPolicy, ResiliencePolicy>;
  using ThisT = AsynchronousIterative<Processor, PublishPolicy, StopPolicy, ResiliencePolicy>;
  using BaseT = IterativeMethod<ResiliencePolicy, ValueType>;
                                
  using TagType = typename BaseT::TagType;
  
  using ProcessorT = Processor;
  using PublishPolicyT = PublishPolicy;
  using StopPolicyT = StopPolicy;
  using ResiliencePolicyT = ResiliencePolicy;

    /**
   * @param job The job running the iteration.
   * @param produced_tag The tag of the data produced by this agent and sent to iteration neighbors.
   * @param tags The set of tags with <em>already finalized subscriptions</em> from neighbors this iteration relies on.
   * @param processor The Processor object used in iteration.
   * @param publish_policy The PublishPolicy object used in iteration.
   * @param stop_policy The StopPolicy object used in iteration.
   * @param resilience_policy The ResiliencePolicy object used in iteration.
   * @param loop_delay_max The maximum amount of time to wait for an update before at least checking the stopping criterion.
   */
  AsynchronousIterative(
    Job& job,
    const TagType& produced_tag,
    const std::vector<TagType>& tags,
    Processor processor,
    PublishPolicy publish_policy,
    StopPolicy stop_policy,
    ResiliencePolicy resilience_policy,
    std::chrono::milliseconds loop_delay_max = 1000ms) noexcept
    : BaseT{job, produced_tag, tags, std::move(resilience_policy)},
      processor_(std::move(processor)),
      publish_values_(gather_initial_publications_()),
      publish_policy_(std::move(publish_policy)),
      stop_policy_(std::move(stop_policy)),
      wait_max_(loop_delay_max)
  { }

  /** @brief Run the iteration until stopping time or forever.
   *  @param callback A callback function to call after each processing iteration.
   */ 
  template<bool has_callback = true>
  void run(std::function<void(const ThisT&)> callback)
  {
    start_time_ = clock_t::now();
    this->submit_values(publish_values_);
    should_iterate_ = true;
    while (should_iterate_)
    {
      while (should_iterate_)
      {
        if (!this->gather_values()) break;

        //        processor_.process_update(get_processor_data_handler(), *this);
        process_all_updates_();
        
        ValueType new_vals = gather_data_for_publication_();
        if (publish_policy_(std::get<0>(new_vals), std::get<0>(publish_values_)))
        {
          publish_values_ = std::move(new_vals);
          this->submit_values(publish_values_);
        }
        
        if constexpr (has_callback) callback(*this);
        should_iterate_ = !stop_policy_(*this);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      if (!should_iterate_) break;
      this->get_job().wait_for_update(wait_max_);
      should_iterate_ = stop_policy_(*this);
    }
    stop_time_ = clock_t::now();
  }

  /** @brief Run the iteration until stopping time or forever without
      a callback.
   */ 
  void run()
  {
    // Call run with has_callback=false so the callback doesn't get
    // called. The actual lambda passed in doesn't matter.
    run<false>([](const ThisT&) { return; } );
  }
  
  /** @brief Get iteration run time, or zero if not yet began.
   */
  std::chrono::milliseconds run_time() const
  {
    if (!start_time_)
      return std::chrono::milliseconds::zero();
    if (!should_iterate_)
      return std::chrono::duration_cast<std::chrono::milliseconds>(*stop_time_ - *start_time_);
    
    auto curr_time = clock_t::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - *start_time_);
  }

  /** @brief Get number of iterations.
   */
  unsigned get_iteration_count() const
  {
    return iteration_count_;
  }

  /** @brief Get if iteration is ongoing. False can mean either that
   *   it has stopped or that it has not yet begun.
   */
  bool return_iterate() const
  {
    return should_iterate_;
  }

  Processor& get_processor() { return processor_; }
  const Processor& get_processor() const { return processor_; }

private:

  /** @brief Process all updates for the main processor and the policies.
   *
   *  If a policy has defined a ValueType, then it is an auxiliary
   *  processor that implements the same interface as Processor, and
   *  so processes updates. If it does not define ValueType, then
   *  nothing is done with that policy.
   */
  void process_all_updates_()
  {
    processor_.process_update(this->template get_policy_data_handler<Processor, ThisT>(), *this);
    this->template process_policy_update_<PublishPolicy, ThisT>
      (publish_policy_, std::bool_constant<has_ValueType_v<PublishPolicy>>{});
    this->template process_policy_update_<StopPolicy, ThisT>
      (stop_policy_, std::bool_constant<has_ValueType_v<StopPolicy>>{});
    this->template process_policy_update_<ResiliencePolicy, ThisT>
      (this->resilience_policy_, std::bool_constant<has_ValueType_v<ResiliencePolicy>>{});
  }

  /** @brief Collect values for initial publication from all policies.
   *
   * Any policy that contributes is asked, any policy that doesn't is not.
   */
  ValueType gather_initial_publications_()
  {
    return std::tuple_cat
      (this->template get_init_tuple_<Processor, ThisT>(processor_),
       this->template get_init_tuple_<PublishPolicy, ThisT>(publish_policy_),
       this->template get_init_tuple_<StopPolicy, ThisT>(stop_policy_),
       this->template get_init_tuple_<ResiliencePolicy, ThisT>(this->resilience_policy_));
  }

  /** @brief Collect values for publication from all policies.
   *
   * Any policy that contributes is asked, any policy that doesn't is not.
   */
  ValueType gather_data_for_publication_()
  {
    return std::tuple_cat
      (this->template get_pub_tuple_<Processor, ThisT>(processor_, publish_values_),
       this->template get_pub_tuple_<PublishPolicy, ThisT>(publish_policy_, publish_values_),
       this->template get_pub_tuple_<StopPolicy, ThisT>(stop_policy_, publish_values_),
       this->template get_pub_tuple_<ResiliencePolicy, ThisT>(this->resilience_policy_, publish_values_));
  }

  Processor processor_;
  ValueType publish_values_;
  PublishPolicy publish_policy_;
  StopPolicy stop_policy_;

  using clock_t = std::chrono::steady_clock;
  std::optional<std::chrono::time_point<clock_t>> start_time_; // only contains a value once the iteration begins
  std::optional<std::chrono::time_point<clock_t>> stop_time_; // only contains a value once the iteration ends

  size_t iteration_count_ = 0;
  bool should_iterate_ = false;
  std::chrono::milliseconds wait_max_;
}; // class AsynchronousIterative




/** @brief A template specialization of WaiterBuilder for AsynchronousIterative methods.
 *
 * To build this WaiterBuilder, do not pass the AsynchronousIterative
 * template parameters directly, but define the specific type of
 * AsynchronousIterative you wish to build and pass that. Then call
 * each of the @p set_* member functions, passing constructor
 * parameters as needed (still call it even if the parameter list is
 * empty), and finally call @p build_waiter to obtain a Waiter to your
 * iterative method. This waiter will wait on any subscriptions or
 * other wait conditions that need to be completed, and will then
 * lazily construct each sub-component and finally the iterative
 * method itself.
 *
 * For example, a typical use might be as follows:
 * @code
 * using IterMethod = AsynchronousIterative<JacobiProcessor<double>, AlwaysUpdateNbrs, StopAfterTime, TrivialResiliencePolicy>;
 * Waiter<IterMethod> iter_waiter =
 *  WaiterBuilder<IterMethod>(manager_handle, job, my_tag, nbr_tags)
 *  .set_processor(A, b, row_inds)
 *  .set_nbr_update_criterion()
 *  .set_stop_policy(std::chrono::seconds(5))
 *  .set_resilience_policy()
 *  .build_waiter();
 * IterMethod sync_jacobi = iter_waiter.get();
 * @endcode
 */  
template<typename Processor, typename PublishPolicy, typename StopPolicy, typename ResiliencePolicy>
class WaiterBuilder<AsynchronousIterative<Processor, PublishPolicy, StopPolicy, ResiliencePolicy>>
{
public:
  using ObjectT = AsynchronousIterative<Processor, PublishPolicy, StopPolicy, ResiliencePolicy>;
  using ThisT = WaiterBuilder<ObjectT>;
  using TagType = typename ObjectT::BaseT::TagType;

    /** @brief WaiterBuilder constructor for AsynchronousIterative methods.
     *
     * Note that the user does not directly pass the tags, but passes
     * the string IDs that are used to construct the tags. This is
     * because the actual tag types, which are templated on the data
     * types being sent, can be quite complex depending on the
     * policies used. So, let Skywing worry about building the correct
     * tag type.
     *
     * @param handle ManagerHandle object running this agent.
     * @param job The job running the iteration.
     * @param produced_tag_id The string ID of the tag of the data produced by this agent.
     * @param sub_tag_ids An iteration-capable container of string IDs of tags of neighboring data from which this agent will collect updates.
     */
  template<typename Range>
  WaiterBuilder(ManagerHandle handle, Job& job,
                const std::string produced_tag_id,
                const Range& sub_tag_ids)
    : handle_(handle), job_(job),
      produced_tag_(produced_tag_id),
      tags_vec_(sub_tag_ids.cbegin(), sub_tag_ids.end())
  {
    job.declare_publication_intent(produced_tag_);
    subscribe_waiter_ =
      std::make_shared<Waiter<void>>(job.subscribe_range(tags_vec_));
  }

  const TagType& produced_tag() const { return produced_tag_; }
  const std::vector<TagType>& subscribed_tags() const { return tags_vec_; }

  /** @brief Build a Waiter<Processor> that will construct the
      Processor for this iterative method.
   */
  template<typename... Args>
  ThisT& set_processor(Args&&... args)
  {
    processor_waiter_ = std::make_shared<Waiter<Processor>>
      (std::move(WaiterBuilder<Processor>(std::forward<Args>(args)...).build_waiter()));
    return *this;
  }

  /** @brief Build a Waiter<PublishPolicy> that will construct the
      PublishPolicy for this iterative method.
   */
  template<typename... Args>
  ThisT& set_publish_policy(Args&&... args)
  {
    publish_policy_waiter_ = std::make_shared<Waiter<PublishPolicy>>
      (WaiterBuilder<PublishPolicy>(std::forward<Args>(args)...).build_waiter());
    return *this;
  }

  /** @brief Build a Waiter<StopPolicy> that will construct the
      StopPolicy for this iterative method.
   */
  template<typename... Args>
  ThisT& set_stop_policy(Args&&... args)
  {
    stop_policy_waiter_ = std::make_shared<Waiter<StopPolicy>>
      (WaiterBuilder<StopPolicy>(std::forward<Args>(args)...).build_waiter());
    return *this;
  }

  /** @brief Build a Waiter<ResiliencePolicy> that will construct the
      ResiliencePolicy for this iterative method.
   */
  template<typename... Args>
  ThisT& set_resilience_policy(Args&&... args)
  {
    resilience_policy_waiter_ = std::make_shared<Waiter<ResiliencePolicy>>
      (WaiterBuilder<ResiliencePolicy>(std::forward<Args>(args)...).build_waiter());
    return *this;
  }

  /* @brief Build a Waiter to the desired synchronous iterative method.
   * @returns A Waiter<AsynchronousIterative<Processor, PublishPolicy, StopPolicy>>
   */
  Waiter<ObjectT> build_waiter()
  {
    if (!(subscribe_waiter_ && processor_waiter_ && publish_policy_waiter_
          && stop_policy_waiter_ && resilience_policy_waiter_))
      throw std::runtime_error("WaiterBuilder<AsynchronousIterative> requires having built all necessary components prior to calling build_waiter().");
    
    // capture by value to ensure liveness of shared ptrs
    auto is_ready = [subscribe_waiter_ = this->subscribe_waiter_,
                     processor_waiter = this->processor_waiter_,
                     publish_policy_waiter = this->publish_policy_waiter_,
                     stop_policy_waiter = this->stop_policy_waiter_,
                     resilience_policy_waiter = this->resilience_policy_waiter_]()
      {
        return (subscribe_waiter_->is_ready()
                && processor_waiter->is_ready()
                && publish_policy_waiter->is_ready()
                && stop_policy_waiter->is_ready()
                && resilience_policy_waiter->is_ready());
      };
    
    auto cons_args = std::make_tuple
      (std::ref(job_), produced_tag_, tags_vec_,
       processor_waiter_->get(),
       publish_policy_waiter_->get(),
       stop_policy_waiter_->get(),
       resilience_policy_waiter_->get());
    auto get_object = [cons_args = std::move(cons_args)]()
        { return std::make_from_tuple<ObjectT>(cons_args); };
    return handle_.waiter_on_subscription_change<ObjectT>(is_ready, std::move(get_object));
  }


private:
  ManagerHandle handle_;
  Job& job_;
  TagType produced_tag_;
  std::vector<TagType> tags_vec_;

  // Using shared_ptrs on these waiters so that they are not destroyed
  // if the WaiterBuilder gets destroyed before the
  // object is retrieved from the Waiter<ThisT>.
  std::shared_ptr<Waiter<void>> subscribe_waiter_;
  std::shared_ptr<Waiter<Processor>> processor_waiter_;
  std::shared_ptr<Waiter<PublishPolicy>> publish_policy_waiter_;
  std::shared_ptr<Waiter<StopPolicy>> stop_policy_waiter_;
  std::shared_ptr<Waiter<ResiliencePolicy>> resilience_policy_waiter_;
}; // class WaiterBuilder<...>


} // namespace skywing

#endif // SKYNET_MID_ASYNCHRONOUS_ITERATIVE_HPP
