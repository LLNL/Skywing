#include "skywing_core/internal/message_creators.hpp"

#include "publish_value_handler.hpp"
#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include "skywing_core/internal/utility/network_conv.hpp"
#include "skywing_core/types.hpp"

#include "message_format.capnp.h"

#include <capnp/serialize.h>

#include <cassert>
#include <cstring>
#include <string>
#include <type_traits>

namespace skywing::internal {
namespace {
// Creates a vector of bytes with a size pre-prended ready to be sent over the network
std::vector<std::byte> finalize_message(capnp::MallocMessageBuilder& builder) noexcept
{
  // Calculate the sizes
  constexpr auto net_size = sizeof(NetworkSizeType);
  const std::size_t msg_size = capnp::computeSerializedSizeInWords(builder) * sizeof(std::size_t);
  const std::size_t buf_size = net_size + msg_size;

  // Write the message to a buffer
  std::vector<std::byte> buffer_data(buf_size);
  kj::NullArrayDisposer null_disposer{};
  kj::Array<kj::byte> buffer{
    reinterpret_cast<kj::byte*>(buffer_data.data()) + net_size, buffer_data.size() - net_size, null_disposer};
  kj::ArrayOutputStream out_s{buffer};
  capnp::writeMessage(out_s, builder);

  // Write the size
  const auto size_bytes = to_network_bytes(msg_size);
  std::memcpy(buffer_data.data(), &size_bytes, sizeof(size_bytes));
  return buffer_data;
}

void set_publish_data(
  cpnpro::PublishData::Builder to_set,
  const VersionID version,
  const TagID& tag_id,
  gsl::span<const PublishValueVariant> value) noexcept
{
  to_set.setVersion(version);
  to_set.setTagID(tag_id);
  auto publish_value = to_set.initValue(value.size());
  for (int i = 0; i < value.size(); ++i) {
    std::visit(
      [&](const auto& data) {
        using ValueType = std::remove_cv_t<std::remove_reference_t<decltype(data)>>;
        auto to_build = publish_value[i];
        detail::PublishValueHandler<ValueType>::set(to_build, data);
      },
      value[i]);
  }
}

template<typename InitFunc, typename MessageType, typename VecValueType>
void set_vector(const InitFunc& init_func, MessageType msg, const std::vector<VecValueType>& values) noexcept
{
  const std::size_t size = values.size();
  auto to_set = (msg.*init_func)(size);
  for (std::size_t i = 0; i < size; ++i) {
    to_set.set(i, values[i]);
  }
}
} // namespace

std::vector<std::byte>
  make_publish(const VersionID version, const TagID& tag_id, gsl::span<const PublishValueVariant> value) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initPublishData();
  set_publish_data(message, version, tag_id, value);
  return finalize_message(builder);
}

std::vector<std::byte>
  make_greeting(const MachineID& from, const std::vector<MachineID>& neighbors, const std::uint16_t port) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initGreeting();
  message.setFrom(from);
  set_vector(&decltype(message)::initNeighbors, message, neighbors);
  message.setPort(port);
  return finalize_message(builder);
}

std::vector<std::byte> make_goodbye() noexcept
{
  capnp::MallocMessageBuilder builder;
  builder.initRoot<cpnpro::StatusMessage>().setGoodbye();
  return finalize_message(builder);
}

std::vector<std::byte> make_new_neighbor(const MachineID& neighbor) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initNewNeighbor();
  message.setNeighborID(neighbor);
  return finalize_message(builder);
}

std::vector<std::byte> make_remove_neighbor(const MachineID& neighbor) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initRemoveNeighbor();
  message.setNeighborID(neighbor);
  return finalize_message(builder);
}

std::vector<std::byte> make_heartbeat() noexcept
{
  capnp::MallocMessageBuilder builder;
  builder.initRoot<cpnpro::StatusMessage>().setHeartbeat();
  return finalize_message(builder);
}

std::vector<std::byte> make_report_publishers(
  const std::vector<TagID>& tags,
  const std::vector<std::vector<std::string>>& addresses,
  const std::vector<std::vector<MachineID>>& machines,
  const std::vector<TagID>& locally_produced_tags) noexcept
{
  const auto set_nested_vector = [&](auto builder, const auto& set_to) noexcept {
    for (std::size_t i = 0; i < tags.size(); ++i) {
      auto msg = builder.init(i, set_to[i].size());
      for (std::size_t j = 0; j < set_to[i].size(); ++j) {
        msg.set(j, set_to[i][j]);
      }
    }
  };
  assert(tags.size() == addresses.size() && tags.size() == machines.size());
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initReportPublishers();
  using MType = decltype(message);
  set_vector(&MType::initTags, message, tags);
  set_nested_vector(message.initAddresses(tags.size()), addresses);
  set_nested_vector(message.initMachines(tags.size()), machines);
  auto msg_local_tags = message.initLocallyProducedTags(locally_produced_tags.size());
  for (std::size_t i = 0; i < locally_produced_tags.size(); ++i) {
    msg_local_tags.set(i, locally_produced_tags[i]);
  }
  set_vector(&MType::initLocallyProducedTags, message, locally_produced_tags);
  return finalize_message(builder);
}

std::vector<std::byte> make_get_publishers(
  const std::vector<TagID>& tags, const std::vector<std::uint8_t>& publishers_needed, const bool ignore_cache) noexcept
{
  assert(tags.size() == publishers_needed.size());
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initGetPublishers();
  set_vector(&decltype(message)::initTags, message, tags);
  set_vector(&decltype(message)::initPublishersNeeded, message, publishers_needed);
  message.setIgnoreCache(ignore_cache);
  return finalize_message(builder);
}

std::vector<std::byte> make_join_reduce_group(const TagID& reduce_tag, const TagID& tag_produced) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initJoinReduceGroup();
  message.setReduceTag(reduce_tag);
  message.setTagProduced(tag_produced);
  return finalize_message(builder);
}

std::vector<std::byte> make_submit_reduce_value(
  const TagID& reduce_tag,
  const VersionID version,
  const TagID& tag_id,
  gsl::span<const PublishValueVariant> value) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initSubmitReduceValue();
  message.setReduceTag(reduce_tag);
  auto publish_data = message.initData();
  set_publish_data(publish_data, version, tag_id, value);
  return finalize_message(builder);
}

std::vector<std::byte> make_report_reduce_disconnection(
  const TagID& reduce_tag, const MachineID& initiating_machine, ReductionDisconnectID disconnection_id) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initReportReduceDisconnection();
  message.setReduceTag(reduce_tag);
  message.setInitiatingMachine(initiating_machine);
  message.setId(disconnection_id);
  return finalize_message(builder);
}

std::vector<std::byte> make_subscription_notice(const std::vector<TagID>& tags, bool is_unsubscribe) noexcept
{
  capnp::MallocMessageBuilder builder;
  auto message = builder.initRoot<cpnpro::StatusMessage>().initSubscriptionNotice();
  set_vector(&decltype(message)::initTags, message, tags);
  message.setIsUnsubscribe(is_unsubscribe);
  return finalize_message(builder);
}
} // namespace skywing::internal
