#ifndef SKYNET_INTERNAL_UTILITY_LOGGING
#define SKYNET_INTERNAL_UTILITY_LOGGING

#include "spdlog/spdlog.h"

#include "skywing_core/types.hpp"

#include "gsl/span"

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

// TODO: Maybe TMP to support basically all iterables, but that's a lot of work,
//       so it might just work as it is

// Macro to wrap logging since I don't know if we're doing runtime or what and this
// can easily be searched for or changed later on
#define SKYNET_TRACE_LOG(...) SPDLOG_TRACE(__VA_ARGS__)
#define SKYNET_DEBUG_LOG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define SKYNET_INFO_LOG(...) SPDLOG_INFO(__VA_ARGS__)
#define SKYNET_WARN_LOG(...) SPDLOG_WARN(__VA_ARGS__)
#define SKYNET_ERROR_LOG(...) SPDLOG_ERROR(__VA_ARGS__)
#define SKYNET_CRITICAL_LOG(...) SPDLOG_CRITICAL(__VA_ARGS__)

// Support for logging of gsl::span
template<typename T>
struct fmt::formatter<gsl::span<T>> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const gsl::span<T>& data, FormatContext& ctx) noexcept
  {
    format_to(ctx.out(), "[");
    bool add_comma = false;
    for (const auto& value : data) {
      if (add_comma) { format_to(ctx.out(), ", "); }
      format_to(ctx.out(), "{}", value);
      add_comma = true;
    }
    return format_to(ctx.out(), "]");
  }
};

// Support for logging of vectors
template<typename T>
struct fmt::formatter<std::vector<T>> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const std::vector<T>& data, FormatContext& ctx) noexcept
  {
    using const_span = gsl::span<const T>;
    return fmt::formatter<const_span>{}.format(const_span{data}, ctx);
  }
};

// Array support
// TODO: Maybe just do support for any containers?  Would require restricting
// the template, however...
template<typename T, std::size_t N>
struct fmt::formatter<std::array<T, N>> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const std::array<T, N>& data, FormatContext& ctx) noexcept
  {
    using const_span = gsl::span<const T>;
    return fmt::formatter<const_span>{}.format(const_span{data}, ctx);
  }
};

// Pair objects
template<typename V1, typename V2>
struct fmt::formatter<std::pair<V1, V2>> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const std::pair<V1, V2>& data, FormatContext& ctx) noexcept
  {
    return format_to(ctx.out(), "{}, {}", data.first, data.second);
  }
};

// Unordered map
template<typename Key, typename Value, typename... Rest>
struct fmt::formatter<std::unordered_map<Key, Value, Rest...>> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const std::unordered_map<Key, Value, Rest...>& data, FormatContext& ctx) noexcept
  {
    using const_span = gsl::span<const typename std::unordered_map<Key, Value, Rest...>::value_type>;
    return fmt::formatter<const_span>{}.format(const_span{data}, ctx);
  }
};

// Support for logging of tag data
template<>
struct fmt::formatter<skywing::PublishValueVariant> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const skywing::PublishValueVariant& data, FormatContext& ctx) noexcept
  {
    return std::visit(
      [&](const auto& value) {
        using Type = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Type, std::vector<bool>>) {
          // dumb std::vector<bool> workaround
          return format_to(ctx.out(), "{}", std::vector<std::uint8_t>{value.cbegin(), value.cend()});
        }
        else {
          return format_to(ctx.out(), "{}", value);
        }
      },
      data);
  }
};

// This is returned as an address, port often enough that just allow direct
// printing of it
// Don't make it a general pair format, since most thing won't want to be
// printed seperated by a colon
template<>
struct fmt::formatter<skywing::AddrPortPair> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) noexcept
  {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const skywing::AddrPortPair& data, FormatContext& ctx) noexcept
  {
    return format_to(ctx.out(), "{}:{}", data.first, data.second);
  }
};

#endif // SKYNET_INTERNAL_UTILITY_LOGGING
