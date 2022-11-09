#ifndef SKYNET_SRC_PUBLISH_VALUE_HANDLER_HPP
#define SKYNET_SRC_PUBLISH_VALUE_HANDLER_HPP

#include "message_format.capnp.h"

#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>
namespace skywing::internal::detail {
// For recursing below, I feel like there's a better way of doing this, but I can't think of it.
template<typename T>
struct IsVector : std::false_type {};
template<typename T>
struct IsVector<std::vector<T>> : std::true_type {};

// Changing from Cap'n Proto's list of things to a vector of things is a common
// operation; provide a function to do it
template<typename To, typename From>
std::vector<To> list_to_vector(const From& values) noexcept
{
  std::vector<To> to_ret;
  to_ret.reserve(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    if constexpr (IsVector<To>::value) { to_ret.push_back(list_to_vector<typename To::value_type>(values[i])); }
    else {
      to_ret.push_back(values[i]);
    }
  }
  return to_ret;
}

// Mapping for the publish data to retrieve things from it as a template
template<typename T>
struct PublishValueHandler;

// Create a mapping for a type and a vector of that type
#define SKYNET_MAKE_PUBLISH_VALUE_HANDLER(cpp_type, capn_suffix)                                                     \
  template<>                                                                                                         \
  struct PublishValueHandler<cpp_type> {                                                                             \
    static std::optional<cpp_type> get(const cpnpro::PublishValue::Reader& r) noexcept                               \
    {                                                                                                                \
      if (!r.is##capn_suffix()) { return {}; }                                                                       \
      return r.get##capn_suffix();                                                                                   \
    }                                                                                                                \
    static void set(cpnpro::PublishValue::Builder& b, const cpp_type& value) noexcept { b.set##capn_suffix(value); } \
  };                                                                                                                 \
  template<>                                                                                                         \
  struct PublishValueHandler<std::vector<cpp_type>> {                                                                \
    static std::optional<std::vector<cpp_type>> get(const cpnpro::PublishValue::Reader& r) noexcept                  \
    {                                                                                                                \
      if (!r.isR##capn_suffix()) { return {}; }                                                                      \
      return list_to_vector<cpp_type>(r.getR##capn_suffix());                                                        \
    }                                                                                                                \
    static void set(cpnpro::PublishValue::Builder& b, const std::vector<cpp_type>& values) noexcept                  \
    {                                                                                                                \
      auto serialized_data = b.initR##capn_suffix(values.size());                                                    \
      for (std::size_t i = 0; i < values.size(); ++i) {                                                              \
        serialized_data.set(i, values[i]);                                                                           \
      }                                                                                                              \
    }                                                                                                                \
  }

SKYNET_MAKE_PUBLISH_VALUE_HANDLER(double, D);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(float, F);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int8_t, I8);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int16_t, I16);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int32_t, I32);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::int64_t, I64);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint8_t, U8);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint16_t, U16);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint32_t, U32);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(std::uint64_t, U64);
SKYNET_MAKE_PUBLISH_VALUE_HANDLER(bool, Bool);

#undef SKYNET_MAKE_PUBLISH_VALUE_HANDLER

// String is a little bit different
template<>
struct PublishValueHandler<std::string> {
  static std::optional<std::string> get(const cpnpro::PublishValue::Reader& r) noexcept
  {
    if (!r.isStr()) { return {}; }
    return r.getStr();
  }
  static void set(cpnpro::PublishValue::Builder& b, const std::string& value) noexcept { b.setStr(value); }
};

template<>
struct PublishValueHandler<std::vector<std::string>> {
  static std::optional<std::vector<std::string>> get(const cpnpro::PublishValue::Reader& r) noexcept
  {
    if (!r.isRStr()) { return {}; }
    return list_to_vector<std::string>(r.getRStr());
  }

  static void set(cpnpro::PublishValue::Builder& b, const std::vector<std::string>& values) noexcept
  {
    auto serialized_data = b.initRStr(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
      serialized_data.set(i, values[i]);
    }
  }
};

// Bytes are different as well
template<>
struct PublishValueHandler<std::vector<std::byte>> {
  static std::optional<std::vector<std::byte>> get(const cpnpro::PublishValue::Reader& r) noexcept
  {
    if (!r.isBytes()) { return {}; }
    const auto bytes = r.getBytes();
    return std::vector<std::byte>{
      reinterpret_cast<const std::byte*>(bytes.begin()), reinterpret_cast<const std::byte*>(bytes.end())};
  }

  static void set(cpnpro::PublishValue::Builder& b, const std::vector<std::byte>& values) noexcept
  {
    auto serialized_data = b.initBytes(values.size());
    std::memcpy(serialized_data.begin(), values.data(), values.size());
  }
};
} // namespace skywing::internal::detail

#endif // SKYNET_SRC_PUBLISH_VALUE_HANDLER_HPP
