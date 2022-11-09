#ifndef SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP
#define SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP

#include "skywing_core/types.hpp"

#include <array>
#include <cstddef>

namespace skywing::internal {
/// Convert from an array of bytes from the network to a local value
NetworkSizeType from_network_bytes(const std::array<std::byte, sizeof(NetworkSizeType)>& data) noexcept;

/// Convert a value to an array of bytes from local data
std::array<std::byte, sizeof(NetworkSizeType)> to_network_bytes(const NetworkSizeType value) noexcept;
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP
