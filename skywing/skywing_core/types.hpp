#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include "skywing_core/internal/utility/type_list.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace skywing {
/// The ID type for machines
using MachineID = std::string;

/// The ID type for jobs
using JobID = std::string;

/// The ID type for message versions.
using VersionID = std::uint32_t;

/// The ID type for tags
using TagID = std::string;

/// The type used for communicating message sizes over the network
using NetworkSizeType = std::uint32_t;

/// The type used for disconnection notifications in reduce groups
using ReductionDisconnectID = std::uint64_t;

/// A typelist of all the types that can be published
using PublishValueTypeList = internal::TypeList<
  float,
  std::vector<float>,
  double,
  std::vector<double>,
  std::int8_t,
  std::vector<std::int8_t>,
  std::int16_t,
  std::vector<std::int16_t>,
  std::int32_t,
  std::vector<std::int32_t>,
  std::int64_t,
  std::vector<std::int64_t>,
  std::uint8_t,
  std::vector<std::uint8_t>,
  std::uint16_t,
  std::vector<std::uint16_t>,
  std::uint32_t,
  std::vector<std::uint32_t>,
  std::uint64_t,
  std::vector<std::uint64_t>,
  std::string,
  std::vector<std::string>,
  std::vector<std::byte>,
  bool,
  // TODO: std::vector<bool> is awful, but don't really want an exception either...
  std::vector<bool>>;

/// Variant version of the above
using PublishValueVariant = internal::ApplyTo<PublishValueTypeList, std::variant>;

namespace internal::detail {
// Can't use std::conditional_t because of At not working for 0 size packs
// but conditional_t requires both types to be well-formed
template<typename... Ts>
struct ValueOrTupleImpl {
  using Type = std::tuple<Ts...>;
};
template<typename T>
struct ValueOrTupleImpl<T> {
  using Type = T;
};
} // namespace internal::detail

/// Takes a parameter pack and either packs it into a tuple or
/// turns it into a single type
template<typename... Ts>
using ValueOrTuple = typename internal::detail::ValueOrTupleImpl<Ts...>::Type;

/// A type indicating that a reduce did not produce a result intentionally
/// (i.e., that it is not the root of the reduce tree)
struct ReduceNoValue {};

/// A type indicating that a reduce failed due to a disconnection
struct ReduceDisconnection {};

/** \brief The return result for a normal reduce operation.
 *
 * As a reduce operation can either not produce a value because it isn't
 * the root or not produce a value because of a connection error, can't
 * just return an optional.
 */
template<typename T>
class ReduceResult {
public:
  constexpr ReduceResult(ReduceNoValue) noexcept : var_{ReduceNoValue{}} {}
  constexpr ReduceResult(ReduceDisconnection) noexcept : var_{ReduceDisconnection{}} {}
  constexpr ReduceResult(T value) noexcept : var_{std::move(value)} {}

  /** \brief Returns true if an error occurred
   */
  constexpr bool error_occurred() const noexcept { return std::holds_alternative<ReduceDisconnection>(var_); }

  /** \brief Returns true if the variant holds a value
   */
  constexpr bool has_value() const noexcept { return std::holds_alternative<T>(var_); }

  /** \brief Returns the value held
   *
   * \pre obj.has_value() == true
   */
  constexpr const T& value() const& noexcept
  {
    assert(has_value());
    return *std::get_if<T>(&var_);
  }
  constexpr T value() && noexcept
  {
    assert(has_value());
    return *std::get_if<T>(&var_);
  }
  constexpr const T& operator*() const& noexcept { return value(); }
  constexpr T operator*() && noexcept { return value(); }

private:
  std::variant<ReduceNoValue, ReduceDisconnection, T> var_;
};

/// Address/port pair
using AddrPortPair = std::pair<std::string, std::uint16_t>;

/** \brief Wrapper for returning void values in various situations
 */
struct VoidWrapper {};

namespace internal {
/// Wraps void values in VoidWrappers or returns the type unmodified
template<typename T>
using WrapVoidValue = std::conditional_t<
  // don't care about cv-qualified void because those shouldn't really be a thing
  std::is_same_v<T, void>,
  VoidWrapper,
  T>;

/// Wraps void returning functions into returning VoidWrapper instead
template<typename Callable, typename... Args>
auto wrap_void_func(Callable&& c, Args&&... args) noexcept
  -> WrapVoidValue<decltype(::std::forward<Callable>(c)(::std::forward<Args>(args)...))>
{
  using RetType = WrapVoidValue<decltype(::std::forward<Callable>(c)(::std::forward<Args>(args)...))>;
  if constexpr (std::is_same_v<RetType, VoidWrapper>) {
    ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
    return VoidWrapper{};
  }
  else {
    return ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
  }
}

/// Structure for reporting reduce group building
struct ReduceGroupNeighbors {
  // Having everything as an array is nice sometimes, but so is having named
  // members
  std::array<TagID, 3> tags;

  const TagID& parent() const noexcept { return tags[0]; }
  TagID& parent() noexcept { return tags[0]; }
  const TagID& left_child() const noexcept { return tags[1]; }
  TagID& left_child() noexcept { return tags[1]; }
  const TagID& right_child() const noexcept { return tags[2]; }
  TagID& right_child() noexcept { return tags[2]; }
};

// Marker prepended to mark tags as publish tags
inline constexpr char publish_tag_marker = 'p';

// Marker prepended to mark tags as begin for reduce groups
inline constexpr char reduce_value_marker = 'r';

// Marker prepended to mark tags as reduce group tags
inline constexpr char reduce_group_marker = 'g';

// Marker prepended to mark tags as private tags
inline constexpr char private_tag_marker = 'x';

/// Structure for testing if a value is any of the supplied values
template<typename T, T... Values>
struct any_of {
  template<typename U>
  friend constexpr bool operator==(const U& comp, any_of) noexcept
  {
    return ((comp == Values) || ...);
  }

  template<typename U>
  friend constexpr bool operator==(any_of, const U& comp) noexcept
  {
    return comp == any_of{};
  }
};

// Checks if a tag name is bad
inline bool tag_name_okay(const std::string& tag) noexcept
{
  return !tag.empty()
      && (tag[0] == any_of<char, publish_tag_marker, reduce_value_marker, reduce_group_marker, private_tag_marker>{});
}
} // namespace internal
} // namespace skywing

// Hashing support
template<>
struct std::hash<skywing::AddrPortPair> {
  std::size_t operator()(const skywing::AddrPortPair& val) const noexcept
  {
    return std::hash<std::string>{}(val.first) ^ std::hash<std::uint16_t>{}(val.second);
  }
};

#endif // SKYNET_TYPES_HPP
