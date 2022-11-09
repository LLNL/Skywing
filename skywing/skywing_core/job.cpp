#include "skywing_core/job.hpp"

#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/manager.hpp"

#include <iostream>

namespace skywing {
std::thread Job::Accessor::run(Job& j) noexcept
{
  return std::thread{[&j]() {
    j.to_run_(j, ManagerHandle{*j.manager_});
    // Re-use the buffer mutex here
    std::lock_guard lock{j.bufs_.mutex()};
    // Signify that the work is done
    j.to_run_ = nullptr;
  }};
}

Job::Job(
  Accessor::AllowConstruction,
  const std::string& id,
  Manager& manager,
  std::function<void(Job&, ManagerHandle)> to_run) noexcept
  : id_{id}, manager_{&manager}, to_run_{std::move(to_run)}
{
  assert(!id.empty());
}

bool Job::is_finished() const noexcept { return to_run_ == nullptr; }

const std::unordered_map<TagID, gsl::span<const std::uint8_t>>& Job::tags_produced() const noexcept
{
  return tags_produced_;
}

bool Job::process_data(const TagID& tag_id, gsl::span<const PublishValueVariant> data, const VersionID version) noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  const auto loc = buffers.find(tag_id);
  // Not subscribed; don't do anything, but not an error
  if (loc == buffers.cend()) {
    SKYNET_TRACE_LOG(
      "\"{}\", job \"{}\" discarded tag \"{}\", version {}, data {}, due to not being subscribed",
      manager_->id(),
      id_,
      tag_id,
      version,
      data);
    return true;
  }
  // If the types are wrong then something went wrong
  const auto comparer = [](std::uint8_t lhs, const PublishValueVariant& rhs) { return lhs == rhs.index(); };
  const auto& expected_types = loc->second.expected_types;
  if (!std::equal(expected_types.cbegin(), expected_types.cend(), data.cbegin(), data.cend(), comparer)) {
    SKYNET_WARN_LOG(
      "\"{}\", job \"{}\" discarded tag \"{}\", version {}, data {}, due to it having the wrong type index",
      manager_->id(),
      id_,
      tag_id,
      version,
      data);
    loc->second.error_occurred = TagInfo::Error::incorrect_type;
    data_buffer_modified_cv_.notify_all();
    return false;
  }
  SKYNET_TRACE_LOG(
    "\"{}\", job \"{}\" accepted tag \"{}\", version {}, data {}", manager_->id(), id_, tag_id, version, data);
  // Otherwise just make it the current value
  loc->second.buffer->add(data, version);
  data_buffer_modified_cv_.notify_all();
  return true;
}

bool Job::tag_has_subscription(const internal::PublishTagBase& tag) const noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  const auto iter = buffers.find(tag.id());
  return iter != buffers.cend() && iter->second.error_occurred == TagInfo::Error::no_error;
}

bool Job::tags_have_subscriptions_impl(gsl::span<const internal::PublishTagBase> tags) const noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  for (const auto& tag : tags) {
    const auto iter = buffers.find(tag.id());
    if (iter == buffers.cend() || iter->second.error_occurred != TagInfo::Error::no_error) { return false; }
  }
  return true;
}

size_t Job::number_of_subscribers(const internal::PublishTagBase& tag) const noexcept
{
  return ManagerHandle{*manager_}.number_of_subscribers(tag);
}

void Job::mark_tag_as_dead(const TagID& tag_id) noexcept
{
  SKYNET_TRACE_LOG("\"{}\" tag \"{}\" marked as dead.", id_, tag_id);
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  const auto tag_loc = buffers.find(tag_id);
  if (tag_loc == buffers.cend()) { return; }
  auto& tag_info = tag_loc->second;
  tag_info.error_occurred = TagInfo::Error::disconnected;
  ++tag_info.connection_id;
  // TODO: Allow passing multiple tags so the cv is notified a bunch
  // of times if there are many tags?  Errors are expected to be rare
  // so maybe this isn't a problem
  data_buffer_modified_cv_.notify_all();
}

void Job::publish_impl(const internal::PublishTagBase& tag, const gsl::span<PublishValueVariant> to_send) noexcept
{
  assert(
    tags_produced_.find(tag.id()) != tags_produced_.cend()
    && "Attempted to publish on a tag that was not declared for publishing!");
  // assert(tags_produced_.find(tag.id())->second == to_send.index()
  //   && "Attempted to publish the wrong type on a tag!");
  // Find / create the last version and obtain a reference to it
  auto& last_version = last_published_version_.try_emplace(tag.id(), internal::tag_no_data).first->second;
  last_version = last_version + 1;
  Manager::JobAccessor::publish(*manager_, last_version, tag.id(), to_send);
}

// Private implementation of public functions
bool Job::has_data(const internal::PublishTagBase& tag) noexcept
{
  std::lock_guard<std::mutex> lock{bufs_.mutex()};
  return has_data_no_lock(tag);
}

bool Job::has_data_no_lock(const internal::PublishTagBase& tag) noexcept
{
  auto& buffers = bufs_.unsafe_get();
  const auto loc = buffers.find(tag.id());
  if (loc == buffers.cend()) { return false; }
  return loc->second.buffer->has_data();
}

const JobID& Job::id() const noexcept { return id_; }

void Job::init_or_update_subscribe(
  const gsl::span<const internal::PublishTagBase> tags,
  gsl::span<std::unique_ptr<internal::DiscardOldVersionTagBufferBase>> ptrs) noexcept
{
  assert(tags.size() == ptrs.size());
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  // Always subscribe ahead of time, since the gap between the
  // Job::subscribe calls can cause messages to get discarded once the
  // connection is made but before it's marked as subscribed
  for (int i = 0; i < tags.size(); ++i) {
    const auto& tag = tags[i];
    auto& ptr = ptrs[i];
    // Then add the expected type; marking the tag as watched
    const auto [iter, inserted] = buffers.try_emplace(
      tag.id(),
      TagInfo{// Just need a dummy value here
              std::move(ptr),
              tag.expected_types(),
              0,
              TagInfo::Error::no_error});
    // Already exists - update the connection id and reset the buffer / error
    if (!inserted) {
      ++iter->second.connection_id;
      // Reset it to a default constructed buffer
      iter->second.buffer->reset();
      iter->second.error_occurred = TagInfo::Error::no_error;
    }
  }
}

Waiter<void> Job::get_subscribe_future(const gsl::span<const internal::PublishTagBase> tags) noexcept
{
  std::vector<TagID> tag_ids(tags.size());
  std::transform(tags.cbegin(), tags.cend(), tag_ids.begin(), [](const internal::PublishTagBase& t) { return t.id(); });
  return Manager::JobAccessor::subscribe(*manager_, tag_ids);
}

Waiter<bool> Job::get_ip_subscribe_future(
  const std::string& address, const gsl::span<const internal::PublishTagBase> tags) noexcept
{
  std::vector<TagID> tag_ids(tags.size());
  std::transform(tags.cbegin(), tags.cend(), tag_ids.begin(), [](const internal::PublishTagBase& t) { return t.id(); });
  const auto addr_pair = internal::split_address(address);
  if (addr_pair.first.empty()) {
    std::cerr << fmt::format(
      "Invalid address \"{}\" for Job::ip_subscribe!  Note that a port must be specified.\n", address);
    std::exit(1);
  }
  return Manager::JobAccessor::ip_subscribe(*manager_, addr_pair, tag_ids);
}

void Job::declare_publication_intent_impl(gsl::span<const internal::PublishTagBase> tags) noexcept
{
  const std::vector<TagID> tag_ids = [&]() {
    std::lock_guard g{bufs_.mutex()};
    for (const auto& tag : tags) {
      tags_produced_.try_emplace(tag.id(), tag.expected_types());
    }
    std::vector<TagID> tag_ids(tags.size());
    std::transform(
      tags.cbegin(), tags.cend(), tag_ids.begin(), [&](const internal::PublishTagBase& t) { return t.id(); });
    return tag_ids;
  }();
  Manager::JobAccessor::report_new_publish_tags(*manager_, tag_ids);
}

void Job::declare_publication_intent_impl(const gsl::span<const internal::PublishTagBase* const> tags) noexcept
{
  const std::vector<TagID> tag_ids = [&]() {
    std::lock_guard g{bufs_.mutex()};
    for (const auto& tag : tags) {
      tags_produced_.try_emplace(tag->id(), tag->expected_types());
    }
    std::vector<TagID> tag_ids(tags.size());
    std::transform(
      tags.cbegin(), tags.cend(), tag_ids.begin(), [&](const internal::PublishTagBase* t) { return t->id(); });
    return tag_ids;
  }();
  Manager::JobAccessor::report_new_publish_tags(*manager_, tag_ids);
}

// void Job::unsubscribe_impl(const TagID& tag_id) noexcept
// {
//   auto [buffers, lock] = bufs_.get();
//   (void)lock;
//   // Just remove any the expected types and data maps
//   buffers.erase(tag_id);
// }

internal::ReduceGroupNeighbors Job::create_reduce_group_init(
  const TagID& tag_produced,
  const std::vector<TagID>& reduce_over_tags,
  gsl::span<const std::uint8_t> expected_types) noexcept
{
  assert(
    tags_produced_.find(tag_produced) == tags_produced_.cend()
    && "Attempted to create a reduce group with a tag that's published on by this type!");
  tags_produced_.try_emplace(tag_produced, expected_types);
  auto bin_tree = reduce_over_tags;
  // A heap can't be used; can produce different ordering depending on the input order
  std::sort(bin_tree.begin(), bin_tree.end());
  const auto index = std::distance(bin_tree.cbegin(), std::find(bin_tree.cbegin(), bin_tree.cend(), tag_produced));
  const auto parent_index = (index - 1) / 2;
  const auto lchild_index = (2 * index) + 1;
  const auto rchild_index = (2 * index) + 2;
  internal::ReduceGroupNeighbors tags_to_find;
  if (index != 0) { tags_to_find.parent() = bin_tree[parent_index]; }
  for (const auto child_index : {lchild_index, rchild_index}) {
    const auto write_index = (child_index == lchild_index ? 1 : 2);
    if (child_index < static_cast<std::remove_const_t<decltype(child_index)>>(bin_tree.size())) {
      tags_to_find.tags[write_index] = bin_tree[child_index];
    }
  }
  SKYNET_TRACE_LOG(
    "\"{}\", job \"{}\", created a reduce group; produced tag is \"{}\", parent tag is \"{}\", child tags are \"{}\", "
    "\"{}\"",
    manager_->id(),
    id_,
    tag_produced,
    tags_to_find.parent(),
    tags_to_find.left_child(),
    tags_to_find.right_child());
  return tags_to_find;
}

Waiter<internal::ReduceGroupBase&>
Job::create_reduce_group_future(std::unique_ptr<internal::ReduceGroupBase> group_ptr) noexcept
{
  return Manager::JobAccessor::create_reduce_group(*manager_, std::move(group_ptr));
}

bool Job::tag_has_active_publisher_impl(const TagID& tag_id) const noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  const auto iter = buffers.find(tag_id);
  if (iter == buffers.cend()) { return false; }
  return iter->second.error_occurred == TagInfo::Error::no_error;
}
} // namespace skywing
