#ifndef SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
#define SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP

#include "skywing_core/internal/utility/network_conv.hpp"
#include "skywing_core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace skywing::internal {
/** \brief Enum returned from communication functions for connection status
 */
enum class [[nodiscard]] ConnectionError{/// The call has fully succeeded, no more work needs to be done
                                         no_error,

                                         /// The call would block
                                         would_block,

                                         /// Non-blocking connected has been initiated
                                         connection_in_progress = would_block,

                                         /// An error occurred with communication that has left the connection
                                         /// in an unusable state
                                         unrecoverable,

                                         /// The connection has closed
                                         closed}; // enum class ConnectionError

/** \brief Socket based communicator
 */
class SocketCommunicator {
public:
  /** \brief Create a new socket-based communicator
   */
  SocketCommunicator() noexcept;

  // Can not be copied
  SocketCommunicator(const SocketCommunicator&) = delete;
  SocketCommunicator& operator=(const SocketCommunicator&) = delete;

  // Can be moved
  SocketCommunicator(SocketCommunicator&&) noexcept;
  SocketCommunicator& operator=(SocketCommunicator&&) noexcept;

  // Destructor
  ~SocketCommunicator();

  /** \brief Accepts an incoming connection if one is pending
   */
  std::optional<SocketCommunicator> accept() noexcept;

  /** \brief Listens for requests on the specified port
   *
   * \param port The port to listen for connections on
   */
  ConnectionError set_to_listen(std::uint16_t port) noexcept;

  /** \brief Connects to a server
   *
   * \param address The address to connect to
   * \param port The port to connect on
   */
  ConnectionError connect_to_server(const char* address, std::uint16_t port) noexcept;

  /** \brief Connects to a server given an address:port string
   */
  ConnectionError connect_to_server(std::string_view address) noexcept;

  /** \brief Initiates a non-blocking connection to a server
   */
  ConnectionError connect_non_blocking(const char* address, std::uint16_t port) noexcept;
  ConnectionError connect_non_blocking(std::string_view address) noexcept;

  /** \brief Returns status on a pending connection
   *
   * \pre A connection has been initiated
   */
  ConnectionError connection_progress_status() noexcept;

  /** \brief Sends a message on the socket
   *
   * \param message The message to send
   * \param size The size of the message
   */
  ConnectionError send_message(const std::byte* message, std::size_t size) noexcept;

  /** \brief Recieve a message from the socket if one is available
   *
   * If there is no message to read (ConnectionError::would_block is returned)
   * then the buffer is left in an unspecified state.
   *
   * \param buffer The buffer to write to
   * \param size The size of the buffer / number of bytes to read
   */
  ConnectionError read_message(std::byte* buffer, std::size_t size) noexcept;

  /** \brief Returns the IP address and port of the socket's peer
   */
  AddrPortPair ip_address_and_port() const noexcept;

  /** \brief Returns the IP address and port of the host end of the socket
   */
  AddrPortPair host_ip_address_and_port() const noexcept;

private:
  // Tag for using the raw handle constructor
  struct WithRawHandle {};

  // Construct a socket using a pre-exising handle
  SocketCommunicator(WithRawHandle, const int handle) noexcept;

  // The handle to the raw socket
  int handle_;
}; // class SocketCommunicator

/** \brief Read a message in chunks from a SocketCommunicator.
 */
std::vector<std::byte> read_chunked(SocketCommunicator& conn, std::size_t num_bytes) noexcept;

/** \brief Splits an "ip:port" address into its parts
 * The string is empty if the input was invalid
 */
AddrPortPair split_address(const std::string_view address) noexcept;

/** \brief Attempts to read a network size from a connection
 *
 * Returns either the network size or the error that occurred
 */
std::variant<NetworkSizeType, ConnectionError> read_network_size(SocketCommunicator& conn) noexcept;

/** \brief Returns an "IP:Port" string from a given address
 */
std::string to_ip_port(const AddrPortPair& addr) noexcept;

/** \brief Converts an AddrPortPair to the canonical representation
 */
AddrPortPair to_canonical(const AddrPortPair& addr) noexcept;
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
