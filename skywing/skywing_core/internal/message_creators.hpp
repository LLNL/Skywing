#ifndef SKYNET_INTERNAL_MESSAGE_CREATORS_HPP
#define SKYNET_INTERNAL_MESSAGE_CREATORS_HPP

#include "skywing_core/types.hpp"

#include "gsl/span"

#include <cstddef>
#include <vector>

namespace skywing::internal {
/** \brief Create data for a publish
 */
std::vector<std::byte>
  make_publish(const VersionID version, const TagID& tag_id, gsl::span<const PublishValueVariant> value) noexcept;

/** \brief Create data for a greeting
 */
std::vector<std::byte>
  make_greeting(const MachineID& from, const std::vector<MachineID>& neighbors, std::uint16_t port) noexcept;

/** \brief Create data for a goodbyte
 */
std::vector<std::byte> make_goodbye() noexcept;

/** \brief Create data for a new neighbor notification
 */
std::vector<std::byte> make_new_neighbor(const MachineID& neighbor) noexcept;

/** \brief Create data for a removed neighbor notification
 */
std::vector<std::byte> make_remove_neighbor(const MachineID& neighbor) noexcept;

/** \brief Create data for a heartbeat
 */
std::vector<std::byte> make_heartbeat() noexcept;

/** \brief Create data for returning information on tag publishers
 *
 * TODO: This can be made more efficient by directly iterating over the map
 * and just grabbing the information from there; not sure how to make it
 * not horribly ugly though, so put it off for now.
 */
std::vector<std::byte> make_report_publishers(
  const std::vector<TagID>& tags,
  const std::vector<std::vector<std::string>>& addresses,
  const std::vector<std::vector<MachineID>>& machines,
  const std::vector<TagID>& locally_produced_tags) noexcept;

/** \brief Create data for a request for producers of a tag
 */
std::vector<std::byte> make_get_publishers(
  const std::vector<TagID>& tags, const std::vector<std::uint8_t>& publishers_needed, bool ignore_cache) noexcept;

/** \brief Create a message to join a reduce group
 */
std::vector<std::byte> make_join_reduce_group(const TagID& reduce_tag, const TagID& tag_produced) noexcept;

/** \brief Create a message to submit a value for reduction, these are sent
 * to the parents
 */
std::vector<std::byte> make_submit_reduce_value(
  const TagID& reduce_tag,
  const VersionID version,
  const TagID& tag_id,
  gsl::span<const PublishValueVariant> value) noexcept;

/** \brief Create a message for sending a disconnection notification
 */
std::vector<std::byte> make_report_reduce_disconnection(
  const TagID& reduce_tag, const MachineID& initiating_machine, ReductionDisconnectID disconnection_id) noexcept;

/** \brief Create a message for subscribing/unsubscribing
 */
std::vector<std::byte> make_subscription_notice(const std::vector<TagID>& tags, bool is_unsubscribe) noexcept;
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_MESSAGE_CREATORS_HPP
