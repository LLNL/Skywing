#include "skywing_core/internal/utility/network_conv.hpp"

#include "generated/endian.hpp"

#include <cstring>

namespace skywing::internal {
/// Convert from an array of bytes from the network to a local value
NetworkSizeType from_network_bytes(const std::array<std::byte, sizeof(NetworkSizeType)>& data) noexcept
{
  // Grab the bytes
  NetworkSizeType size;
  std::memcpy(&size, data.data(), data.size());
  // convert them if needed
  return machine_is_little_endian ? size : byte_swap(size);
}

/// Convert a value to an array of bytes from local data
std::array<std::byte, sizeof(NetworkSizeType)> to_network_bytes(const NetworkSizeType value) noexcept
{
  // get it into little endian if it isn't
  const auto adj_value = machine_is_little_endian ? value : byte_swap(value);
  // Copy it to an array
  std::array<std::byte, sizeof(NetworkSizeType)> to_ret;
  std::memcpy(to_ret.data(), &adj_value, to_ret.size());
  return to_ret;
}
} // namespace skywing::internal
