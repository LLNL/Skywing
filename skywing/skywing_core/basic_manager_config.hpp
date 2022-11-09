#error This header is not yet supported

#ifndef SKYNET_BASIC_MANAGER_CONFIG_HPP
  #define SKYNET_BASIC_MANAGER_CONFIG_HPP

  #include <iosfwd>
  #include <optional>
  #include <vector>

namespace skywing {
class Manager;
struct BuildManagerInfo;

/** \brief EXTREMELY simple Manager setup config files.
 *
 * The format is very limited:
 * ```
 * machine name
 * machine port
 * heartbeat interval in milliseconds
 * address to connect to 1
 * address to connect to 2
 * ...
 * ```
 * The manager must be passed in by reference as it is non-movable.
 *
 * \returns True if reading is successful, false otherwise.
 */
std::optional<BuildManagerInfo> read_manager_config(std::istream& in) noexcept;

/** \brief Class representing information to build a Manager.
 *
 * This two-step process is required because reading information from the file
 * can fail, but an optional can't be returned due to a Manager not being
 * move-able.
 */
struct BuildManagerInfo {
  std::string name;
  std::vector<std::string> to_connect_to;
  std::uint32_t heartbeat_interval_in_ms;
  std::uint16_t port;

  // Declared as an inline friend so it can only be found via ADL.
  // Simple calls the below function
  friend std::istream& operator>>(std::istream& in, BuildManagerInfo& info)
  {
    if (auto new_info = read_manager_config(in)) { info = std::move(*new_info); }
    return in;
  }
};
} // namespace skywing

#endif // SKYNET_BASIC_MANAGER_CONFIG_HPP
