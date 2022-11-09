#ifndef SKYNET_INTERNAL_REDUCE_GROUP_HPP
#define SKYNET_INTERNAL_REDUCE_GROUP_HPP

#include "skywing_core/internal/manager_waiter_callables.hpp"
#include "skywing_core/internal/tag_buffer.hpp"
#include "skywing_core/types.hpp"
#include "skywing_core/waiter.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <type_traits>
#include <unordered_map>

namespace skywing {
class Manager;

namespace internal {
// TODO: Can maybe make this deal with a variant of pointers instead of a variant
// of values so there's fewer conversions, etc.?  Would likely be faster, but I don't
// think it's worth pursuing unless this becomes a bottleneck
class ReduceGroupBase {
public:
  virtual ~ReduceGroupBase() = default;
  ReduceGroupBase() = delete;
  ReduceGroupBase(const ReduceGroupBase&) = delete;
  ReduceGroupBase(ReduceGroupBase&&) = delete;
  ReduceGroupBase& operator=(const ReduceGroupBase&) = delete;
  ReduceGroupBase& operator=(ReduceGroupBase&&) = delete;

  struct Accessor {
  private:
    friend skywing::Manager;

    static void report_disconnection(ReduceGroupBase& g) noexcept { return g.report_disconnection(); }
    static const ReduceGroupNeighbors& tag_neighbors(const ReduceGroupBase& g) noexcept { return g.tag_neighbors(); }
    static bool add_data(
      ReduceGroupBase& g, const TagID& tag, gsl::span<const PublishValueVariant> value, VersionID version) noexcept
    {
      return g.add_data(tag, value, version);
    }
    static void propagate_disconnection(
      ReduceGroupBase& g, const MachineID& initiating_machine, ReductionDisconnectID id) noexcept
    {
      g.propagate_disconnection(initiating_machine, id);
    }
    static const TagID& produced_tag(const ReduceGroupBase& g) noexcept { return g.produced_tag(); }
    static const TagID& group_id(const ReduceGroupBase& g) noexcept { return g.group_id(); }
  };

protected:
  ReduceGroupBase(
    const ReduceGroupNeighbors& tag_neighbors,
    Manager& manager,
    const TagID& group_id,
    const TagID& produced_tag,
    gsl::span<const std::uint8_t> expected_types) noexcept;

  /////////////////////////////////
  // Manager accessible functions
  /////////////////////////////////

  // Create a disconnection notice and send it to connected machines
  void report_disconnection() noexcept;

  // Returns the parent/children tags
  const ReduceGroupNeighbors& tag_neighbors() const noexcept;

  // Adds data to the corresponding buffer, returning false if an error occurred
  bool add_data(const TagID& tag, gsl::span<const PublishValueVariant> value, VersionID version) noexcept;

  // Handle and propagate a disconnection notice from another machine
  void propagate_disconnection(const MachineID& initiating_machine, ReductionDisconnectID id) noexcept;

  // Returns the tag this node produces
  const TagID& produced_tag() const noexcept;

  // Returns the group tag
  const TagID& group_id() const noexcept;

  /////////////////////////////////
  // Implementaion for derived
  /////////////////////////////////

  // Returns true if this handle to the group returns a value on reduce
  bool returns_value_on_reduce() const noexcept;

  // Rebuilds a reduce group after it fails due to a disconnection
  Waiter<void> rebuild() noexcept;

  // Process any pending reduce operations, removing them if finished
  void process_pending_reduce_ops() noexcept { do_process_pending_reduce_ops(); }

  // Sends a value to the parent
  void send_value_to_parent(gsl::span<const PublishValueVariant> value_to_send, VersionID version) noexcept;

  // Sends a value to the children
  void send_value_to_children(gsl::span<const PublishValueVariant> value_to_send, VersionID version) noexcept;

  // Sends a disconnection notice to all parents and children
  void send_disconnection(const MachineID& initiating_machine, ReductionDisconnectID disconn_id) noexcept;

  // Adds data without locking and using an index
  void add_data_index(
    std::size_t index, const gsl::span<const PublishValueVariant> value, const VersionID version) noexcept
  {
    assert(index < 3);
    do_add_data_index(index, value, version);
  }

  /////////////////////////////////
  // Virtual functions
  /////////////////////////////////

  virtual void do_process_pending_reduce_ops() noexcept = 0;
  virtual void do_reset_buffers() noexcept = 0;
  virtual void
    do_add_data_index(std::size_t index, gsl::span<const PublishValueVariant> value, VersionID version) noexcept = 0;

  /////////////////////////////////
  // Data members
  /////////////////////////////////

  ReduceGroupNeighbors tag_neighbors_;
  Manager* manager_;
  std::unordered_map<MachineID, ReductionDisconnectID> last_heard_disconnect_;
  TagID group_id_;
  TagID produced_tag_;
  VersionID last_sent_version_ = tag_no_data;
  std::mutex buffer_mutex_;
  std::condition_variable future_info_cv_;
  // PRNG for disconnection ID's
  // This doesn't need to be a very good PRNG since it'll seldom be used and
  // it's only to avoid extremely rare collisions
  // Use minstd_rand just for its small size (8 bytes)
  // This also has the nice property where the next number will always be
  // different from the next number, so the previous number doesn't have
  // to be stored
  std::minstd_rand prng{std::random_device{}()};
  gsl::span<const std::uint8_t> expected_types_;
  bool is_valid = true;
  // Internal counter so that earlier-made futures know to error
  std::uint16_t conn_counter = 0;
}; // class ReduceGroupBase
} // namespace internal

template<typename... Ts>
class ReduceGroup : public internal::ReduceGroupBase {
public:
  using ValueType = ValueOrTuple<Ts...>;

  ReduceGroup(
    const internal::ReduceGroupNeighbors& tag_neighbors,
    Manager& manager,
    const TagID& group_id,
    const TagID& produced_tag) noexcept
    : ReduceGroupBase{tag_neighbors, manager, group_id, produced_tag, internal::expected_type_for<Ts...>}
  {}

  // Make these functions available publicly
  using internal::ReduceGroupBase::rebuild;
  using internal::ReduceGroupBase::returns_value_on_reduce;

  template<typename Callable, typename... ArgTypes>
  auto reduce(Callable reduce_op, ArgTypes&&... values) noexcept
  {
    static_assert((... && std::is_convertible_v<ArgTypes, Ts>), "Reduce called with invalid parameters!");
    return reduce_impl<false>(std::move(reduce_op), ValueType{static_cast<Ts>(std::forward<ArgTypes>(values))...});
  }
  template<typename Callable, typename... ArgTypes>
  auto reduce(Callable reduce_op, const std::tuple<ArgTypes...>& values_tuple) noexcept
  {
    const auto apply_to = [&](const auto&... values) { return reduce(std::move(reduce_op), values...); };
    return std::apply(apply_to, values_tuple);
  }

  template<typename Callable, typename... ArgTypes>
  auto allreduce(Callable reduce_op, ArgTypes&&... values) noexcept
  {
    static_assert((... && std::is_convertible_v<ArgTypes, Ts>), "Allreduce called with invalid parameters!");
    return reduce_impl<true>(std::move(reduce_op), ValueType{static_cast<Ts>(std::forward<ArgTypes>(values))...});
  }

  template<typename Callable, typename... ArgTypes>
  auto allreduce(Callable reduce_op, const std::tuple<ArgTypes...>& values_tuple) noexcept
  {
    const auto apply_to = [&](const auto&... values) { return allreduce(std::move(reduce_op), values...); };
    return std::apply(apply_to, values_tuple);
  }

private:
  // Wraps a reduce operation into a compatible type and handle tuple wrapping and
  // unwrapping if needed
  template<typename Callable>
  static auto make_wrapper(Callable reduce_op) noexcept
  {
    using TupleType = std::tuple<Ts...>;
    if constexpr (sizeof...(Ts) == 1) {
      static_assert(
        std::is_invocable_r_v<ValueType, Callable, ValueType, ValueType>,
        "Invalid Callable used for reduce operation!");
      return [op = std::move(reduce_op)](ValueType lhs, ValueType rhs) mutable noexcept -> ValueType {
        return op(std::move(lhs), std::move(rhs));
      };
    }
    else if constexpr (std::is_invocable_r_v<ValueType, Callable, TupleType, TupleType>) {
      return [op = std::move(reduce_op)](ValueType lhs, ValueType rhs) mutable noexcept -> ValueType {
        return op(std::move(lhs), std::move(rhs));
      };
    }
    else if constexpr (std::is_invocable_r_v<ValueType, Callable, Ts..., Ts...>) {
      return [op = std::move(reduce_op)](ValueType lhs, ValueType rhs) mutable noexcept -> ValueType {
        return std::apply(op, std::tuple_cat(std::move(lhs), std::move(rhs)));
      };
    }
    else {
      static_assert(std::is_same_v<Callable, Callable>, "Invalid Callable used for reduce operation!");
    }
  }

  void do_process_pending_reduce_ops() noexcept override
  {
    const auto reduce_is_ready = [&](const VersionID required_version) noexcept {
      if (tag_neighbors_.left_child().empty()) { return true; }
      else if (tag_neighbors_.right_child().empty()) {
        // Left child only
        return data_buffers_[1].has_data(required_version);
      }
      else {
        // Both children
        return data_buffers_[1].has_data(required_version) && data_buffers_[2].has_data(required_version);
      }
    };
    // Process the reductions in order, until one fails to complete
    for (auto iter = pending_reduces_.begin(); iter != pending_reduces_.end(); iter = pending_reduces_.erase(iter)) {
      if (!is_valid) { continue; }
      if (!reduce_is_ready(iter->required_version)) { return; }
      last_sent_version_ = iter->required_version;
      const auto reduce_result = [&]() -> std::vector<PublishValueVariant> {
        // Three different options - 2 children, left child only, no children
        if (tag_neighbors_.left_child().empty()) {
          // no children, just propagate value to parent
          return make_variant_vector(iter->value, std::index_sequence_for<Ts...>{});
        }
        // Either one or two children, left child is always present
        const auto left_val = data_buffers_[1].get(iter->required_version);
        if (tag_neighbors_.right_child().empty()) {
          // One child, just apply op with value and propagate value to parent
          const auto reduce_value = iter->operation(left_val, iter->value);
          return make_variant_vector(reduce_value, std::index_sequence_for<Ts...>{});
        }
        // Both children
        const auto right_val = data_buffers_[2].get(iter->required_version);
        // Do op(op(left, value), right) so order of evaluation is always the same
        // Also if there are no parents then this will have the final reduce value
        const auto reduce_value = iter->operation(iter->operation(left_val, iter->value), right_val);
        return make_variant_vector(reduce_value, std::index_sequence_for<Ts...>{});
      }();
      const gsl::span<const PublishValueVariant> result_span{reduce_result};
      // Put the result in the buffer so the result can be retrieved if this is the root
      // Otherwise, send the result to the parent
      if (returns_value_on_reduce()) {
        add_data_index(0, result_span, iter->required_version);
        if (iter->is_all_reduce) { send_value_to_children(result_span, iter->required_version); }
      }
      else {
        send_value_to_parent(result_span, iter->required_version);
      }
    }
  }

  template<std::size_t... Is>
  std::vector<PublishValueVariant> make_variant_vector(ValueType val, std::index_sequence<Is...>) noexcept
  {
    if constexpr (sizeof...(Is) == 1) { return std::vector<PublishValueVariant>{std::move(val)}; }
    else {
      return std::vector<PublishValueVariant>{std::move(std::get<Is>(val))...};
    }
  }

  void do_reset_buffers() noexcept override
  {
    for (auto& buf : data_buffers_) {
      buf.reset();
    }
  }

  void do_add_data_index(
    const std::size_t index,
    const gsl::span<const PublishValueVariant> value,
    const VersionID version) noexcept override
  {
    data_buffers_[index].add(value, version);
  }

  // Templated because the return type will be different if it's an allreduce
  template<bool IsAllReduce, typename Callable>
  auto reduce_impl(Callable reduce_op, ValueOrTuple<Ts...> value) noexcept
  {
    std::lock_guard lock{buffer_mutex_};
    const auto required_version = last_sent_version_ + 1;
    pending_reduces_.push_back({required_version, value, this->make_wrapper(std::move(reduce_op)), IsAllReduce});
    process_pending_reduce_ops();
    const auto conn_id = conn_counter;
    using produced_type = std::conditional_t<IsAllReduce, std::optional<ValueType>, ReduceResult<ValueType>>;
    // As the produced type is different,
    return make_waiter<produced_type>(
      buffer_mutex_,
      future_info_cv_,
      [this, required_version, conn_id]() noexcept {
        if (conn_id < conn_counter || !is_valid) { return true; }
        if constexpr (IsAllReduce) { return data_buffers_[0].has_data(required_version); }
        else {
          return last_sent_version_ != internal::tag_no_data && last_sent_version_ >= required_version;
        }
      },
      [this, required_version, conn_id]() noexcept -> produced_type {
        const bool error_occurred = (conn_id < conn_counter || !is_valid);
        const auto make_error = []() {
          if constexpr (IsAllReduce) { return produced_type{}; }
          else {
            return ReduceDisconnection{};
          }
        };
        if (IsAllReduce || returns_value_on_reduce()) {
          // If there's a value return it regardless of if there's an error
          if (data_buffers_[0].has_data(required_version)) {
            // Value is present
            return data_buffers_[0].get(required_version);
          }
          else {
            return make_error();
          }
        }
        else if (error_occurred) {
          return make_error();
        }
        else if constexpr (!IsAllReduce) {
          // Normal reduce - no error occurred, but no value to return
          return ReduceNoValue{};
        }
        return make_error();
      });
  }

  struct PendingReduce {
    VersionID required_version;
    ValueType value;
    std::function<ValueType(ValueType, ValueType)> operation;
    bool is_all_reduce;
  };
  std::vector<PendingReduce> pending_reduces_;
  std::array<internal::FifoTagBuffer<Ts...>, 3> data_buffers_;
}; // class ReduceGroup
} // namespace skywing

#endif // SKYNET_INTERNAL_REDUCE_GROUP_HPP
