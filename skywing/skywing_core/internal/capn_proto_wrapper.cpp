#include "skywing_core/internal/capn_proto_wrapper.hpp"

#include "publish_value_handler.hpp"
#include "skywing_core/internal/utility/logging.hpp"

namespace skywing::internal {
namespace detail {
/** \brief Class that supresses Cap'n Proto's exceptions so that they
 * can be used in a non-exception friendly environment
 *
 * Note that all that has to be done to supress exceptions is to create
 * one of these on the stack.
 */
class ExceptionSuppressor : public kj::ExceptionCallback {
public:
  bool failed() const noexcept { return failed_; }

private:
  void onRecoverableException(kj::Exception&&) override
  {
    // Mark it as failed
    failed_ = true;
  }

  void onFatalException(kj::Exception&&) override
  {
    // just return - nothing can be done here
    return;
  }

  bool failed_ = false;
};

// Short name entirely for the one function below
// (type alias templates can't be local)
template<typename T>
using pvh = PublishValueHandler<T>;
template<typename T>
using pvh_v = PublishValueHandler<std::vector<T>>;
} // namespace detail

/////////////////////////////////////////////////////
// PublishData
/////////////////////////////////////////////////////

std::optional<std::vector<PublishValueVariant>> PublishData::value() const noexcept
{
  using namespace detail;
  const auto decode_value = [](cpnpro::PublishValue::Reader reader) -> std::optional<PublishValueVariant> {
    // This is gross and I hate it, but...
    using vals = cpnpro::PublishValue::Which;
    switch (reader.which()) {
    case vals::D:
      return pvh<double>::get(reader);
    case vals::R_D:
      return pvh_v<double>::get(reader);
    case vals::F:
      return pvh<float>::get(reader);
    case vals::R_F:
      return pvh_v<float>::get(reader);
    case vals::STR:
      return pvh<std::string>::get(reader);
    case vals::R_STR:
      return pvh_v<std::string>::get(reader);
    case vals::I8:
      return pvh<std::int8_t>::get(reader);
    case vals::I16:
      return pvh<std::int16_t>::get(reader);
    case vals::I32:
      return pvh<std::int32_t>::get(reader);
    case vals::I64:
      return pvh<std::int64_t>::get(reader);
    case vals::U8:
      return pvh<std::uint8_t>::get(reader);
    case vals::U16:
      return pvh<std::uint16_t>::get(reader);
    case vals::U32:
      return pvh<std::uint32_t>::get(reader);
    case vals::U64:
      return pvh<std::uint64_t>::get(reader);
    case vals::R_I8:
      return pvh_v<std::int8_t>::get(reader);
    case vals::R_I16:
      return pvh_v<std::int16_t>::get(reader);
    case vals::R_I32:
      return pvh_v<std::int32_t>::get(reader);
    case vals::R_I64:
      return pvh_v<std::int64_t>::get(reader);
    case vals::R_U8:
      return pvh_v<std::uint8_t>::get(reader);
    case vals::R_U16:
      return pvh_v<std::uint16_t>::get(reader);
    case vals::R_U32:
      return pvh_v<std::uint32_t>::get(reader);
    case vals::R_U64:
      return pvh_v<std::uint64_t>::get(reader);
    case vals::BYTES:
      return pvh_v<std::byte>::get(reader);
    case vals::BOOL:
      return pvh<bool>::get(reader);
    case vals::R_BOOL:
      return pvh_v<bool>::get(reader);
    }
    return std::nullopt;
  };
  const auto& value = r.getValue();
  std::vector<PublishValueVariant> to_ret(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (const auto add = decode_value(value[i])) { to_ret[i] = *add; }
    else {
      return std::nullopt;
    }
  }
  return to_ret;
}

VersionID PublishData::version() const noexcept { return r.getVersion(); }
TagID PublishData::tag_id() const noexcept { return r.getTagID(); }
PublishData::PublishData(cpnpro::PublishData::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// Greeting
/////////////////////////////////////////////////////

MachineID Greeting::from() const noexcept { return r.getFrom(); }
std::vector<MachineID> Greeting::neighbors() const noexcept
{
  return detail::list_to_vector<MachineID>(r.getNeighbors());
}
std::uint16_t Greeting::port() const noexcept { return r.getPort(); }
Greeting::Greeting(cpnpro::Greeting::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// NewNeighbor
/////////////////////////////////////////////////////

MachineID NewNeighbor::neighbor_id() const noexcept { return r.getNeighborID(); }
NewNeighbor::NewNeighbor(cpnpro::NewNeighbor::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// RemoveNeighbor
/////////////////////////////////////////////////////

MachineID RemoveNeighbor::neighbor_id() const noexcept { return r.getNeighborID(); }
RemoveNeighbor::RemoveNeighbor(cpnpro::RemoveNeighbor::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// ReportPublishers
/////////////////////////////////////////////////////

std::vector<TagID> ReportPublishers::tags() const noexcept { return detail::list_to_vector<TagID>(r.getTags()); }
std::vector<std::vector<std::string>> ReportPublishers::addresses() const noexcept
{
  return detail::list_to_vector<std::vector<std::string>>(r.getAddresses());
}
std::vector<std::vector<MachineID>> ReportPublishers::machines() const noexcept
{
  return detail::list_to_vector<std::vector<MachineID>>(r.getMachines());
}
std::vector<TagID> ReportPublishers::locally_produced_tags() const noexcept
{
  return detail::list_to_vector<TagID>(r.getLocallyProducedTags());
}

ReportPublishers::ReportPublishers(cpnpro::ReportPublishers::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// GetPublishers
/////////////////////////////////////////////////////

std::vector<std::string> GetPublishers::tags() const noexcept { return detail::list_to_vector<TagID>(r.getTags()); }
std::vector<std::uint8_t> GetPublishers::publishers_needed() const noexcept
{
  return detail::list_to_vector<std::uint8_t>(r.getPublishersNeeded());
}
bool GetPublishers::ignore_cache() const noexcept { return r.getIgnoreCache(); }
GetPublishers::GetPublishers(cpnpro::GetPublishers::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// JoinReduceGroup
/////////////////////////////////////////////////////

TagID JoinReduceGroup::reduce_tag() const noexcept { return r.getReduceTag(); }
TagID JoinReduceGroup::tag_produced() const noexcept { return r.getTagProduced(); }
JoinReduceGroup::JoinReduceGroup(cpnpro::JoinReduceGroup::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// SubmitReduceValue
/////////////////////////////////////////////////////

TagID SubmitReduceValue::reduce_tag() const noexcept { return r.getReduceTag(); }
PublishData SubmitReduceValue::data() const noexcept { return PublishData{r.getData()}; }
SubmitReduceValue::SubmitReduceValue(cpnpro::SubmitReduceValue::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// ReportReduceDisconnection
/////////////////////////////////////////////////////

TagID ReportReduceDisconnection::reduce_tag() const noexcept { return r.getReduceTag(); }
MachineID ReportReduceDisconnection::initiating_machine() const noexcept { return r.getInitiatingMachine(); }
ReductionDisconnectID ReportReduceDisconnection::id() const noexcept { return r.getId(); }

ReportReduceDisconnection::ReportReduceDisconnection(cpnpro::ReportReduceDisconnection::Reader reader) noexcept
  : r{std::move(reader)}
{}

/////////////////////////////////////////////////////
// SubscriptionNotice
/////////////////////////////////////////////////////

std::vector<TagID> SubscriptionNotice::tags() const noexcept { return detail::list_to_vector<TagID>(r.getTags()); }
bool SubscriptionNotice::is_unsubscribe() const noexcept { return r.getIsUnsubscribe(); }

SubscriptionNotice::SubscriptionNotice(cpnpro::SubscriptionNotice::Reader reader) noexcept : r{std::move(reader)} {}

/////////////////////////////////////////////////////
// MessageHandler
/////////////////////////////////////////////////////

MessageHandler::MessageHandler() noexcept : impl_{std::make_unique<Impl>()} {}

MessageHandler::MessageHandler(MessageHandler&&) noexcept = default;
MessageHandler& MessageHandler::operator=(MessageHandler&&) noexcept = default;

std::optional<MessageHandler> MessageHandler::try_to_create(const std::vector<std::byte>& data) noexcept
{
  detail::ExceptionSuppressor suppressor;
  // Read the message from the passed bytes
  MessageHandler to_ret;
  kj::Array<const kj::byte> buffer{
    reinterpret_cast<const kj::byte*>(data.data()), data.size(), to_ret.impl_->null_disposer};
  kj::ArrayInputStream in_s{buffer};
  capnp::readMessageCopy(in_s, to_ret.impl_->message);
  to_ret.impl_->root = to_ret.impl_->message.getRoot<cpnpro::StatusMessage>();
  if (suppressor.failed()) {
    SKYNET_WARN_LOG("Failed to decode message in MessageHandler::try_to_create.");
    return {};
  }
  else {
    return std::optional<MessageHandler>{std::move(to_ret)};
  }
}

auto MessageHandler::extract_message() const noexcept -> std::optional<MessageVariant>
{
  using vals = cpnpro::StatusMessage::Which;
  detail::ExceptionSuppressor suppressor;
  // This is kind of messy, but need to make sure that there's a way
  // to signify that there's no data due to, e.g., malformed input
  const std::optional<MessageVariant> to_ret = [&]() -> std::optional<MessageVariant> {
    switch (impl_->root.which()) {
    case vals::GREETING:
      return Greeting{impl_->root.getGreeting()};
    case vals::GOODBYE:
      return Goodbye{/* impl_->root.getGoodbye() */};
    case vals::NEW_NEIGHBOR:
      return NewNeighbor{impl_->root.getNewNeighbor()};
    case vals::REMOVE_NEIGHBOR:
      return RemoveNeighbor{impl_->root.getRemoveNeighbor()};
    case vals::HEARTBEAT:
      return Heartbeat{/* impl_->root.getHeartbeat() */};
    case vals::REPORT_PUBLISHERS:
      return ReportPublishers{impl_->root.getReportPublishers()};
    case vals::GET_PUBLISHERS:
      return GetPublishers{impl_->root.getGetPublishers()};
    case vals::JOIN_REDUCE_GROUP:
      return JoinReduceGroup{impl_->root.getJoinReduceGroup()};
    case vals::SUBMIT_REDUCE_VALUE:
      return SubmitReduceValue{impl_->root.getSubmitReduceValue()};
    case vals::REPORT_REDUCE_DISCONNECTION:
      return ReportReduceDisconnection{impl_->root.getReportReduceDisconnection()};
    case vals::PUBLISH_DATA:
      return PublishData{impl_->root.getPublishData()};
    case vals::SUBSCRIPTION_NOTICE:
      return SubscriptionNotice{impl_->root.getSubscriptionNotice()};
    }
    return {};
  }();
  if (suppressor.failed()) {
    SKYNET_WARN_LOG("Failed to decode message in MessageHandler::extract_message.");
    return {};
  }
  else {
    return to_ret;
  }
}
} // namespace skywing::internal
