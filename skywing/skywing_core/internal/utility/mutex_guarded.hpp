#ifndef SKYNET_INTERNAL_UTILITY_MUTEX_GUARDED_HPP
#define SKYNET_INTERNAL_UTILITY_MUTEX_GUARDED_HPP

#include <mutex>
#include <type_traits>
#include <utility>

/** \brief Class that holds data that is guarded by a mutex so that
 * only one class can access it at a time
 */
template<typename T>
class MutexGuarded {
public:
  /** \brief Construct a guarded object with the passed parameters
   */
  template<typename... Args>
  constexpr explicit MutexGuarded(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    : value_{std::forward<Args>(args)...}
  {}

  /** \brief Returns the object, locking if it is not available
   */
  std::pair<T&, std::unique_lock<std::mutex>> get() noexcept { return {value_, std::unique_lock{mutex_}}; }

  std::pair<const T&, std::unique_lock<std::mutex>> get() const noexcept { return {value_, std::unique_lock{mutex_}}; }

  /** \brief Trys to lock the object, returns nullptr for the object
   * if it failed to do so
   */
  std::pair<T*, std::unique_lock<std::mutex>> try_get() noexcept
  {
    if (mutex_.try_lock()) {
      // Lock worked
      return {&value_, {mutex_, std::adopt_lock}};
    }
    else {
      // Lock failed
      return {nullptr, {}};
    }
  }

  std::pair<const T*, std::unique_lock<std::mutex>> try_get() const noexcept
  {
    if (mutex_.try_lock()) { return {&value_, {mutex_, std::adopt_lock}}; }
    else {
      return {nullptr, {}};
    }
  }

  /** \brief Returns a reference to the mutex
   */
  std::mutex& mutex() noexcept { return mutex_; }

  /** \brief Returns a reference to the contained value without using the mutex.
   */
  T& unsafe_get() noexcept { return value_; }
  const T& unsafe_get() const noexcept { return value_; }

private:
  T value_;
  mutable std::mutex mutex_;
};

#endif // SKYNET_INTERNAL_UTILITY_MUTEX_GUARDED_HPP
