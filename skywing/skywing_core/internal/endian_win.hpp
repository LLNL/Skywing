#ifndef SKYNET_INTERNAL_ENDIAN_WIN_HPP
#define SKYNET_INTERNAL_ENDIAN_WIN_HPP

#include <cstdint>

namespace skywing::internal {
// Intrinsics information pulled from
// https://github.com/boostorg/endian/blob/develop/include/boost/endian/detail/intrinsic.hpp
inline std::uint16_t byte_swap(std::uint16_t val) noexcept { return _byteswap_ushort(val); }
inline std::uint32_t byte_swap(std::uint32_t val) noexcept { return _byteswap_ulong(val); }
inline std::uint64_t byte_swap(std::uint64_t val) noexcept { return _byteswap_uint64(val); }
inline std::int16_t byte_swap(std::int16_t val) noexcept { return _byteswap_ushort(val); }
inline std::int32_t byte_swap(std::int32_t val) noexcept { return _byteswap_ulong(val); }
inline std::int64_t byte_swap(std::int64_t val) noexcept { return _byteswap_uint64(val); }
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_ENDIAN_WIN_HPP
