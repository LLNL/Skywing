#ifndef SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
#define SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP

// This header exists to allow more convienent and (within the codebase)
// conventional access to the Cap'n Proto messages

#include "message_format.capnp.h"

#include <capnp/serialize.h>

#include "skywing_core/internal/utility/overload_set.hpp"
#include "skywing_core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace skywing::internal {
/** \brief Class representing a publish message
 */
class PublishData {
public:
  VersionID version() const noexcept;
  TagID tag_id() const noexcept;
  std::optional<std::vector<PublishValueVariant>> value() const noexcept;

private:
  cpnpro::PublishData::Reader r;

  friend class MessageHandler;
  friend class SubmitReduceValue;
  friend class Publish;
  explicit PublishData(cpnpro::PublishData::Reader reader) noexcept;
};

/** \brief Class representing a greeting message
 */
class Greeting {
public:
  MachineID from() const noexcept;
  std::vector<MachineID> neighbors() const noexcept;
  std::uint16_t port() const noexcept;

private:
  cpnpro::Greeting::Reader r;

  friend class MessageHandler;
  explicit Greeting(cpnpro::Greeting::Reader reader) noexcept;
};

/** \brief Class representing a goodbye message
 */
class Goodbye {
  // Intentionally empty
};

/** \brief Class representing a new neighbor message
 */
class NewNeighbor {
public:
  MachineID neighbor_id() const noexcept;

private:
  cpnpro::NewNeighbor::Reader r;

  friend class MessageHandler;
  explicit NewNeighbor(cpnpro::NewNeighbor::Reader reader) noexcept;
};

/** \brief Class representing a remove neighbor message
 */
class RemoveNeighbor {
public:
  MachineID neighbor_id() const noexcept;

private:
  cpnpro::RemoveNeighbor::Reader r;

  friend class MessageHandler;
  explicit RemoveNeighbor(cpnpro::RemoveNeighbor::Reader reader) noexcept;
};

/** \brief Class representing a heartbeat
 */
class Heartbeat {
  // Intentionally empty
};

/** \brief Class representing information on which machines produce what tags
 */
class ReportPublishers {
public:
  std::vector<TagID> tags() const noexcept;
  std::vector<std::vector<std::string>> addresses() const noexcept;
  std::vector<std::vector<MachineID>> machines() const noexcept;
  std::vector<TagID> locally_produced_tags() const noexcept;

private:
  cpnpro::ReportPublishers::Reader r;

  friend class MessageHandler;
  explicit ReportPublishers(cpnpro::ReportPublishers::Reader reader) noexcept;
};

/** \brief Request information for which machines produce which tags
 */
class GetPublishers {
public:
  std::vector<TagID> tags() const noexcept;
  std::vector<std::uint8_t> publishers_needed() const noexcept;
  bool ignore_cache() const noexcept;

private:
  cpnpro::GetPublishers::Reader r;

  friend class MessageHandler;
  explicit GetPublishers(cpnpro::GetPublishers::Reader reader) noexcept;
};

/** \brief Message for when a machine join a reduce group
 */
class JoinReduceGroup {
public:
  TagID reduce_tag() const noexcept;
  TagID tag_produced() const noexcept;

private:
  cpnpro::JoinReduceGroup::Reader r;

  friend class MessageHandler;
  explicit JoinReduceGroup(cpnpro::JoinReduceGroup::Reader reader) noexcept;
};

/** \brief Message for submitting a value for reduction to the parent/children
 */
class SubmitReduceValue {
public:
  TagID reduce_tag() const noexcept;
  PublishData data() const noexcept;

private:
  cpnpro::SubmitReduceValue::Reader r;

  friend class MessageHandler;
  explicit SubmitReduceValue(cpnpro::SubmitReduceValue::Reader reader) noexcept;
};

/** \brief Message for reporting that a machine disconnected
 */
class ReportReduceDisconnection {
public:
  TagID reduce_tag() const noexcept;
  MachineID initiating_machine() const noexcept;
  ReductionDisconnectID id() const noexcept;

private:
  cpnpro::ReportReduceDisconnection::Reader r;

  friend class MessageHandler;
  explicit ReportReduceDisconnection(cpnpro::ReportReduceDisconnection::Reader reader) noexcept;
};

/** \brief Message for subscribing/unsubscibing to a tag
 */
class SubscriptionNotice {
public:
  std::vector<TagID> tags() const noexcept;
  bool is_unsubscribe() const noexcept;

private:
  cpnpro::SubscriptionNotice::Reader r;

  friend class MessageHandler;
  explicit SubscriptionNotice(cpnpro::SubscriptionNotice::Reader reader) noexcept;
};

/** \brief Class for converting the raw bytes of a message into a useable format
 */
class MessageHandler {
public:
  /** \brief Construct a message handler from a raw set of bytes
   */
  static std::optional<MessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept;

  // Moveable only
  MessageHandler() noexcept;
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;
  MessageHandler(MessageHandler&&) noexcept;
  MessageHandler& operator=(MessageHandler&&) noexcept;

  /** \brief Perform a callback on the stored message
   *
   * Returns true if the callback was successful, false otherwise
   */
  template<typename... Ts>
  bool do_callback(Ts&&... callbacks) const noexcept
  {
    if (const auto msg = extract_message()) {
      return std::visit(make_overload_set(std::forward<Ts>(callbacks)...), *msg);
    }
    return false;
  }

private:
  // The types of messages that can be produced
  using MessageVariant = std::variant<
    Greeting,
    Goodbye,
    NewNeighbor,
    RemoveNeighbor,
    Heartbeat,
    ReportPublishers,
    GetPublishers,
    JoinReduceGroup,
    SubmitReduceValue,
    ReportReduceDisconnection,
    SubscriptionNotice,
    PublishData>;

  // Process the stored message and return its internal type
  std::optional<MessageVariant> extract_message() const noexcept;

  // capnp::MallocMessageBuilder isn't copyable or movable, but needs to be
  // contained in this structure; use PIMPL to solve this
  // Impl needs to be defined here since this object is being returned as
  // an optional, which requires it to be complete
  struct Impl {
    kj::NullArrayDisposer null_disposer;
    capnp::MallocMessageBuilder message;
    cpnpro::StatusMessage::Reader root;
  };
  std::unique_ptr<Impl> impl_;
};
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
