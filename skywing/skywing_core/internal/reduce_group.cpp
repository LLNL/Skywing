#include "skywing_core/internal/reduce_group.hpp"

#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/manager.hpp"

#include "gsl/span"

namespace skywing::internal {
ReduceGroupBase::ReduceGroupBase(
  const ReduceGroupNeighbors& tag_neighbors,
  Manager& manager,
  const TagID& group_id,
  const TagID& produced_tag,
  const gsl::span<const std::uint8_t> expected_types) noexcept
  : tag_neighbors_{tag_neighbors}
  , manager_{&manager}
  , group_id_{group_id}
  , produced_tag_{produced_tag}
  , expected_types_{expected_types}
{}

// Adds data to the corresponding buffer, returning false if an error occurred
bool ReduceGroupBase::add_data(
  const TagID& tag, gsl::span<const PublishValueVariant> value, const VersionID version) noexcept
{
  const auto comparer
    = [](const PublishValueVariant& lhs, const std::uint8_t rhs) noexcept { return lhs.index() == rhs; };
  if (!std::equal(value.cbegin(), value.cend(), expected_types_.cbegin(), expected_types_.cend(), comparer)) {
    SKYNET_WARN_LOG(
      "\"{}\" rejected data for reduce group \"{}\" for tag \"{}\" version {} due to wrong type",
      manager_->id(),
      group_id_,
      tag,
      version);
    return false;
  }
  for (std::size_t i = 0; i < tag_neighbors_.tags.size(); ++i) {
    if (tag == tag_neighbors_.tags[i]) {
      SKYNET_TRACE_LOG(
        "\"{}\" added data for reduce group \"{}\" for tag \"{}\" version {}", manager_->id(), group_id_, tag, version);
      {
        std::lock_guard<std::mutex> lock{buffer_mutex_};
        add_data_index(i, value, version);
        if (i == 0) { send_value_to_children(value, version); }
        process_pending_reduce_ops();
      }
      future_info_cv_.notify_all();
      return true;
    }
  }
  // There's not good way for the manager to know ahead of time if the tag should
  // be supplied so only output this if it really is unexpected
  if (tag != produced_tag_) {
    SKYNET_WARN_LOG(
      "\"{}\" rejected data for reduce group \"{}\" for tag \"{}\" version {} due to not matching any buffer",
      manager_->id(),
      group_id_,
      tag,
      version);
  }
  return false;
}

void ReduceGroupBase::propagate_disconnection(const MachineID& initiating_machine, ReductionDisconnectID id) noexcept
{
  {
    std::lock_guard g{buffer_mutex_};
    const bool should_act_on = [&]() {
      const auto iter = last_heard_disconnect_.find(initiating_machine);
      if (iter == last_heard_disconnect_.cend()) {
        last_heard_disconnect_.try_emplace(iter, initiating_machine, id);
        return true;
      }
      else if (iter->second == id) {
        return false;
      }
      else {
        iter->second = id;
        return true;
      }
    }();
    if (!should_act_on) { return; }
    // Mark all futures invalid until rebuilding happens
    is_valid = false;
    ++conn_counter;
    send_disconnection(initiating_machine, id);
  }
  future_info_cv_.notify_all();
}

void ReduceGroupBase::report_disconnection() noexcept
{
  {
    std::lock_guard g{buffer_mutex_};
    is_valid = false;
    ++conn_counter;
    send_disconnection(manager_->id(), prng());
  }
  future_info_cv_.notify_all();
}

// Returns true if this handle to the group returns a value on reduce
bool ReduceGroupBase::returns_value_on_reduce() const noexcept { return tag_neighbors_.parent().empty(); }

const ReduceGroupNeighbors& ReduceGroupBase::tag_neighbors() const noexcept { return tag_neighbors_; }

const TagID& ReduceGroupBase::produced_tag() const noexcept { return produced_tag_; }

const TagID& ReduceGroupBase::group_id() const noexcept { return group_id_; }

Waiter<void> ReduceGroupBase::rebuild() noexcept
{
  // Reset the buffers
  {
    std::lock_guard<std::mutex> lock{buffer_mutex_};
    last_sent_version_ = tag_no_data;
    is_valid = true;
    do_reset_buffers();
  }
  return Manager::ReduceGroupAccessor::rebuild_reduce_group(*manager_, group_id_);
}

void ReduceGroupBase::send_value_to_parent(
  gsl::span<const PublishValueVariant> value_to_send, const VersionID version) noexcept
{
  Manager::ReduceGroupAccessor::send_reduce_data_to_parent(*manager_, group_id_, version, produced_tag_, value_to_send);
}

void ReduceGroupBase::send_value_to_children(
  gsl::span<const PublishValueVariant> value_to_send, VersionID version) noexcept
{
  Manager::ReduceGroupAccessor::send_reduce_data_to_children(*manager_, group_id_, version, produced_tag_, value_to_send);
}

void ReduceGroupBase::send_disconnection(const MachineID& initiating_machine, ReductionDisconnectID disconn_id) noexcept
{
  Manager::ReduceGroupAccessor::send_report_disconnection(*manager_, group_id_, initiating_machine, disconn_id);
}
} // namespace skywing::internal
