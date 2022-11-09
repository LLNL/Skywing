#ifndef SKYNET_WAITER_HPP
#define SKYNET_WAITER_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>

#include "skywing_core/types.hpp"
#include <iostream>
namespace skywing {
template<typename Waiter, typename... Continuations>
class Continuation : public Continuations... {
public:
  explicit Continuation(Waiter wait_on, Continuations... continuations) noexcept
    : Continuations{std::move(continuations)}..., to_wait_on_{std::move(wait_on)}
  {}

  template<typename... NewContinuations>
  auto then(NewContinuations... continuations) && noexcept
    -> Continuation<Waiter, Continuations..., NewContinuations...>
  {
    return Continuation<Waiter, Continuations..., NewContinuations...>{
      to_wait_on_, std::move(static_cast<Continuations&>(*this))..., std::move(continuations)...};
  }

  decltype(auto) get() noexcept
  {
    return handle_continuations(
      [this]() noexcept -> decltype(auto) { return to_wait_on_.get(); }, static_cast<Continuations&>(*this)...);
  }

  void wait() noexcept { to_wait_on_.wait(); }

  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
  {
    return to_wait_on_.wait_for(wait_time);
  }

  template<class Rep, class Period>
  bool wait_until(const std::chrono::time_point<Rep, Period>& end_time) noexcept
  {
    return to_wait_on_.wait_until(end_time);
  }

private:
  // The continuations are in the reverse order that they need to be called
  // so have to build up a function to return the result
  template<typename BuildUp, typename Next, typename... Rest>
  decltype(auto) handle_continuations(const BuildUp& build_up, Next& next, Rest&... rest) noexcept
  {
    const auto next_call = [&]() noexcept -> decltype(auto) {
      // Tried to make this it's own function to make this tidier but it didn't
      // work for some reason?  So some repetition is unavoidable it seems
      // Can't pass void so have to make a wrapper
      if constexpr (std::is_same_v<decltype(build_up()), void>) {
        return [&]() noexcept -> decltype(auto) {
          build_up();
          return next();
        };
      }
      else {
        if constexpr (std::is_invocable_v<Next, decltype(build_up())>) {
          return [&]() noexcept -> decltype(auto) { return next(build_up()); };
        }
        else {
          return [&]() noexcept -> decltype(auto) {
            build_up();
            return next();
          };
        }
      }
    };
    if constexpr (sizeof...(Rest) == 0) { return next_call()(); }
    else {
      return handle_continuations(next_call(), rest...);
    }
  }

  Waiter to_wait_on_;
}; // class Continuation





/** @brief A container that waits for some condition to become true
 * before returning the object of interest.
 *
 * This is frequently used to wait on operations occurring in other
 * threads, or even on other agents, to complete, such as waiting for
 * a set of subscriptions to finalize. Through Waiters, a user is only
 * able to get an object once conditions required for its use are
 * met. For example, for an iterative method to function,
 * subscriptions to neighboring agents' publications associated with
 * the method must be finalized, and constructing an appropriate
 * Waiter allows a user to guarantee subscription completion before
 * use of the iterative method.
 *
 * Waiters are often (but not always) constructed through various
 * specializations of the BuildWaiter class, which can be used to
 * define subscriptions or other wait conditions needed for the type
 * of interest.
 *
 * There are two common special cases of Waiters:
 * 1. The <em>instant Waiter</em> that is immediately ready. Enables lazy construction of objects.
 * 2. The <em>void Waiter</em> which does not return anything upon @p get(). Enables checking for completion of tasks without needing to build an associated object.
 *
 * @tparam T The object type to be returned upon get().
 */
template<typename T>
class Waiter
{
public:
  /// The return type of get()
  using ValueType = T;

  /** @param mutex_handle A reference to the mutex used with the associated condition variable.
   *  @param cv_handle A reference to the condition variable to wait on.
   *  @param is_ready_callable An @p std::function<bool()> that must return true to declare readiness.
   *  @param get_value_callable An @p std::function<T()> that returns the object of interest.
   */
  Waiter(std::mutex& mutex_handle,
         std::condition_variable& cv_handle,
         std::function<bool()>&& is_ready_callable,
         std::function<ValueType()>&& get_value_callable) noexcept
    : mutex_{&mutex_handle}, cv_{&cv_handle},
      is_ready_callable_(std::move(is_ready_callable)),
      get_value_callable_(std::move(get_value_callable))
  {}

  /** @brief A constructor for an "instant Waiter".
   *
   *  @param get_value_callable An @p std::function<T()> that returns the object of interest.
   */
  Waiter(std::function<ValueType()>&& get_value_callable) noexcept
    : get_value_callable_(std::move(get_value_callable))
  {}

  /** @brief A constructor for a "void Waiter".
   *
   *  @param mutex_handle A reference to the mutex used with the associated condition variable.
   *  @param cv_handle A reference to the condition variable to wait on.
   *  @param is_ready_callable An @p std::function<bool()> that must return true to declare readiness.
   */
  Waiter(std::mutex& mutex_handle,
         std::condition_variable& cv_handle,
         std::function<bool()>&& is_ready_callable) noexcept
    : mutex_{&mutex_handle}, cv_{&cv_handle},
      is_ready_callable_(std::move(is_ready_callable)),
      get_value_callable_([](){ return; })
  {}

  /** @brief Block until ready, then return object.
   */
  ValueType get() noexcept
  {
    if (is_instant())
      return get_value_callable_();
      
    std::unique_lock<std::mutex> lock{**mutex_};
    if (!is_ready_no_lock()) {
      (*cv_)->wait(lock, [this]() noexcept { return is_ready_no_lock(); });
    }
    return get_value_callable_();
  }

  /** @brief Block until ready.
   */
  void wait() noexcept
  {
    if (is_instant()) return;
    std::unique_lock<std::mutex> lock{**mutex_};
    if (is_ready_no_lock()) { return; }
    (*cv_)->wait(lock, [this]() noexcept { return is_ready_no_lock(); });
  }

  /** @brief Block until ready or a given amount of time passes, whichever is first.
   */
  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
  {
    if (is_instant()) return true;
    const auto end_time = std::chrono::steady_clock::now() + wait_time;
    return wait_until(end_time);
  }

  /** @brief Block until ready or a given time is reached, whichever is first.
   */
  template<class Rep, class Period>
  bool wait_until(const std::chrono::time_point<Rep, Period>& end_time) noexcept
  {
    if (is_instant()) return true;
    std::unique_lock<std::mutex> lock{**mutex_};
    if (is_ready_no_lock()) { return true; }
    return (*cv_)->wait_until(lock, end_time, [this]() noexcept { return is_ready_no_lock(); });
  }

  /** @brief Thread-safe check if ready.
   *  Blocks to acquire mutex but does not wait for condition to be ready.
   */
  bool is_ready() noexcept
  {
    if (is_instant()) return true;
    std::lock_guard<std::mutex> lock{**mutex_};
    return is_ready_no_lock();
  }

  // For transforming the get type
  template<typename... Continuations>
  Continuation<Waiter, Continuations...> then(Continuations... continuations) && noexcept
  {
    return Continuation<Waiter, Continuations...>{*this, std::move(continuations)...};
  }

private:

  bool is_ready_no_lock() noexcept
  {
    if (!mutex_) return true;
    return (*is_ready_callable_)();
  }

  bool is_instant() { return !mutex_; }

  std::optional<std::mutex*> mutex_;
  std::optional<std::condition_variable*> cv_;
  std::optional<std::function<bool()>> is_ready_callable_;
  std::function<ValueType()> get_value_callable_;
}; // class Waiter


/** @brief A default WaiterBuilder to build waiters for objects.
 *
 *  This default WaiterBuilder constructs an "instant Waiter" that is
 *  immediately ready. If you wish to build a Waiter for an object
 *  that has nontrivial wait conditions, implement a specialization of
 *  the WaiterBuilder class for the type you wish to build. See, e.g,
 *  the WaiterBuilder for the AsynchronousIterative class template for
 *  an example.
 */
template<typename T>
class WaiterBuilder
{
public:
  using ObjectT = T;

  //TODO: Will this correctly perform lazy construction of an ObjecT with perfect forwarding?
  // template<typename... Args>
  // WaiterBuilder(Args&&... args)
  //   : waiter_([args_tup = std::make_tuple(std::forward<Args>(args)...)]()
  //       {
  //         return std::make_from_tuple<ObjectT>(args_tup);
  //       })
  // { }
  template<typename... Args>
  WaiterBuilder(Args&&... args)
    : waiter_([&]()
        {
          return ObjectT(std::forward<Args>(args)...);
        })
  { }


  Waiter<ObjectT> build_waiter() { return waiter_; }

private:
  Waiter<ObjectT> waiter_;

}; // class WaiterBuilder


struct WaiterGetNoOp {
  constexpr void operator()() const noexcept {}
}; // struct WaiterGetNoOp

  
template<typename T>
inline Waiter<T> make_waiter(std::mutex& mutex,
                             std::condition_variable& cv,
                             std::function<bool()>&& is_ready_callable,
                             std::function<T()>&& get_value_callable) noexcept
{
  return Waiter<T>{mutex, cv, std::move(is_ready_callable), std::move(get_value_callable)};
}

inline Waiter<void> make_waiter(std::mutex& mutex,
                                std::condition_variable& cv,
                                std::function<bool()>&& is_ready_callable) noexcept
{
  return Waiter<void>(mutex, cv, std::move(is_ready_callable), WaiterGetNoOp{});
}




  
/** \brief Class that can be used to wait on many waiters of the same type.
 *
 * Generally not created directly, but through when_all_same instead.
 */
template<typename T>
class WaiterVec {
public:
  using ValueType = std::vector<T>;

  explicit WaiterVec(std::vector<Waiter<T>> waiters) noexcept : waiters_{std::move(waiters)} {}

  ValueType get() noexcept
  {
    ValueType to_ret;
    to_ret.reserve(waiters_.size());
    for (auto& waiter : waiters_) {
      to_ret.push_back(waiter.get());
    }
    return to_ret;
  }

  void wait() noexcept
  {
    for (auto& waiter : waiters_) {
      waiter.wait();
    }
  }

  template<class Rep, class Period>
  bool wait_until(const std::chrono::time_point<Rep, Period>& end_time) noexcept
  {
    for (auto& waiter : waiters_) {
      if (!waiter.wait_until(end_time)) { return false; }
    }
    return true;
  }

  template<class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
  {
    const auto end_time = std::chrono::steady_clock::now() + wait_time;
    return wait_until(end_time);
  }

  bool is_ready() noexcept
  {
    for (auto&& waiter : waiters_) {
      if (!waiter.is_ready()) {
        return false;
      }
    }
    return true;
  }

private:
  std::vector<Waiter<T>> waiters_;
};

/** \brief Returns an AllWaiterSame
 */
template<typename T>
WaiterVec<T> make_waitervec(std::vector<Waiter<T>> waiters) noexcept
{
  return WaiterVec<T>{std::move(waiters)};
}

} // namespace skywing

#endif // SKYNET_WAITER_HPP
