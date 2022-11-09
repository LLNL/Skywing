#include "skywing_core/basic_manager_config.hpp"

#include "skywing_core/manager.hpp"

// #include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace skywing {
namespace {
template<typename T>
std::optional<T> parse_integer_line(std::istream& in) noexcept
{
  std::string num_line;
  if (!std::getline(in, num_line)) { return {}; }
  // TODO: Some build system stuff to enable this when it's available
  // (Availability is pretty poor still, despite it being C++17)
  // T value;
  // const auto [pointer, ec] = std::from_chars(
  //   num_line.c_str(),
  //   num_line.c_str() + num_line.size(),
  //   to_ret.value
  // );
  // (void)pointer;
  // if (ec != std::errc{}) { return {}; }
  // return value;
  char* last_processed;
  const auto value = std::strtoll(num_line.c_str(), &last_processed, 10);
  constexpr auto lower_end = std::numeric_limits<T>::lowest();
  constexpr auto upper_end = std::numeric_limits<T>::max();
  const auto expected_end = num_line.c_str() + num_line.size();
  if (last_processed != expected_end || value < lower_end || value > upper_end) { return {}; }
  return static_cast<T>(value);
}
} // end anonymous namespace

std::optional<BuildManagerInfo> read_manager_config(std::istream& in) noexcept
{
  BuildManagerInfo to_ret;
  // Machine name
  if (!std::getline(in, to_ret.name)) { return {}; }
  const auto port = parse_integer_line<std::uint16_t>(in);
  if (!port) { return {}; }
  to_ret.port = *port;
  const auto heartbeat = parse_integer_line<std::uint32_t>(in);
  if (!heartbeat) { return {}; }
  to_ret.heartbeat_interval_in_ms = *heartbeat;
  // Machines to connect to
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) { to_ret.to_connect_to.push_back(std::move(line)); }
  }
  return to_ret;
}
} // namespace skywing
