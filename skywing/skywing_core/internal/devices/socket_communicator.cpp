#include "skywing_core/internal/devices/socket_communicator.hpp"

#include "socket_wrappers.hpp"

#include "generated/socket_no_sigpipe.hpp"
#include "skywing_core/internal/utility/logging.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr int invalid_handle = -1;

struct addrinfo_deleter {
  void operator()(addrinfo* info) const noexcept { freeaddrinfo(info); }
};

using addrinfo_ptr = std::unique_ptr<addrinfo, addrinfo_deleter>;

addrinfo_ptr resolve_addr(const char* const address, const std::uint16_t port) noexcept
{
  addrinfo* result;
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;
  const auto port_str = std::to_string(port);
  const auto resaddr = getaddrinfo(address, port_str.c_str(), &hints, &result);
  if (resaddr != 0) {
    std::perror("resolve_addr - resaddr");
    std::exit(4);
  }
  return {result, {}};
}

int init_connection(const int sockfd, const char* const address, const std::uint16_t port) noexcept
{
  // TODO: What is the correct address-acquisition approach?
  
  // This isn't super robust, but I'm not sure how to handle looking up a bunch of different
  // address in an asynchronous context
  //const auto result = resolve_addr(address, port);
  //return connect(sockfd, result->ai_addr, static_cast<int>(result->ai_addrlen));
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, address, &serv_addr.sin_addr) <= 0)
  {
    SKYNET_ERROR_LOG("Invalid address {}", address);
    std::exit(4);
  }
  return connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
}
} // namespace

namespace skywing::internal {
SocketCommunicator::SocketCommunicator() noexcept : handle_{create_non_blocking()}
{
  if (handle_ == invalid_handle) {
    std::perror("SocketCommunicator::SocketCommunicator - socket");
    std::exit(4);
  }
}

SocketCommunicator::SocketCommunicator(SocketCommunicator&& other) noexcept : handle_{other.handle_}
{
  other.handle_ = invalid_handle;
}

SocketCommunicator& SocketCommunicator::operator=(SocketCommunicator&& other) noexcept
{
  // Do this in a roundabout way to handle self-assignment
  const auto new_handle = other.handle_;
  other.handle_ = invalid_handle;
  handle_ = new_handle;
  return *this;
}

SocketCommunicator::~SocketCommunicator()
{
  if (handle_ != invalid_handle) { close(handle_); }
}

std::optional<SocketCommunicator> SocketCommunicator::accept() noexcept
{
  sockaddr_in client_address_struct;
  // len can't be const as accept takes a non-const pointer
  socklen_t len = sizeof(client_address_struct);

  const int raw_handle = accept_make_non_blocking(handle_, reinterpret_cast<sockaddr*>(&client_address_struct), &len);
  if (raw_handle == invalid_handle) {
    // No connection to be made
    if (errno == EAGAIN || errno == EWOULDBLOCK) { return {}; }
    // This should never happen and is a programming bug if it's reached
    // Not 100% sure how to handle it, but forcefully quitting with a message
    // seems to be fine for now
    SKYNET_DEBUG_LOG("accept had handle_ {}, raw_handle {}, threw error: {}", handle_, raw_handle, strerror(errno));
    std::perror("SocketCommunicator::accept - accept");
    std::exit(4);
  }

  // Read the address
  return SocketCommunicator(WithRawHandle{}, raw_handle);
}

ConnectionError SocketCommunicator::set_to_listen(const std::uint16_t port) noexcept
{
  constexpr int listen_queue_size = 10;
  sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);
  if (bind(handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
    // std::perror("SocketCommunicator::set_to_listen - bind");
    // std::exit(-1);
    return ConnectionError::unrecoverable;
  }
  if (listen(handle_, listen_queue_size) < 0) {
    // std::perror("SocketCommunicator::set_to_listen - listen");
    // std::exit(-1);
    return ConnectionError::unrecoverable;
  }
  return ConnectionError::no_error;
}

ConnectionError SocketCommunicator::connect_to_server(const char* const address, const std::uint16_t port) noexcept
{
  if (init_connection(handle_, address, port) == -1) {
    if (errno == EINPROGRESS) {
      // wait for the connection to finish
      pollfd to_poll;
      to_poll.fd = handle_;
      to_poll.events = POLLOUT;
      if (poll(&to_poll, 1, -1) < 0) {
        // perror("SocketCommunicator::connect_to_server - poll");
        // exit(-1);
        return ConnectionError::unrecoverable;
      }
      // Check if any error occured
      constexpr auto err_mask = POLLERR | POLLHUP | POLLNVAL;
      if ((to_poll.revents & err_mask) != 0) { return ConnectionError::unrecoverable; }
    }
    else {
      // std::perror("SocketCommunicator::connect_to_server - connect");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
  }
  return ConnectionError::no_error;
}

ConnectionError SocketCommunicator::connect_to_server(const std::string_view address) noexcept
{
  const auto [address_str, port] = split_address(address);
  if (address_str.empty()) { return ConnectionError::unrecoverable; }
  return connect_to_server(address_str.c_str(), port);
}

ConnectionError SocketCommunicator::connect_non_blocking(const char* address, std::uint16_t port) noexcept
{
  if (init_connection(handle_, address, port) == -1 && errno == EINPROGRESS) {
    return ConnectionError::connection_in_progress;
  }
  return ConnectionError::unrecoverable;
}

ConnectionError SocketCommunicator::connect_non_blocking(const std::string_view address) noexcept
{
  const auto [address_str, port] = split_address(address);
  if (address_str.empty()) { return ConnectionError::unrecoverable; }
  return connect_non_blocking(address_str.c_str(), port);
}

ConnectionError SocketCommunicator::connection_progress_status() noexcept
{
  pollfd to_poll;
  to_poll.fd = handle_;
  to_poll.events = POLLOUT | POLLIN;
  if (poll(&to_poll, 1, 0) < 0) {
    // std::perror("SOCKET POLL ERROR: ");
    // This is also required?
    if (errno == EINPROGRESS || errno == EAGAIN) { return ConnectionError::connection_in_progress; }
    return ConnectionError::unrecoverable;
  }
  constexpr auto err_mask = POLLERR | POLLHUP | POLLNVAL;
  if ((to_poll.revents & err_mask) != 0) {
    // std::printf("SOCKET ERR FLAGS %i - COMP %i %i %i\n", to_poll.revents, POLLERR, POLLHUP, POLLNVAL);
    return ConnectionError::unrecoverable;
  }
  if ((to_poll.revents & (POLLOUT | POLLIN)) != 0) { return ConnectionError::no_error; }
  return ConnectionError::connection_in_progress;
}

ConnectionError SocketCommunicator::send_message(const std::byte* const message, const std::size_t size) noexcept
{
  if (send(handle_, message, size, SKYNET_NO_SIGPIPE) < 0) {
    SKYNET_DEBUG_LOG("send_message threw error: {}", strerror(errno));
    if (errno == EAGAIN || errno == EWOULDBLOCK) { return ConnectionError::would_block; }
    // std::perror("SocketCommunicator::send_message - write");
    // std::exit(-1);
    return ConnectionError::unrecoverable;
  }
  return ConnectionError::no_error;
}

ConnectionError SocketCommunicator::read_message(std::byte* const buffer, const std::size_t size) noexcept
{
  const auto read_bytes = read(handle_, reinterpret_cast<char*>(buffer), size);
  if (read_bytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) { return ConnectionError::would_block; }
    
    SKYNET_DEBUG_LOG("read_message threw error: {}", strerror(errno));
    return ConnectionError::unrecoverable;
  }
  return read_bytes == 0 ? ConnectionError::closed : ConnectionError::no_error;
}

AddrPortPair SocketCommunicator::ip_address_and_port() const noexcept
{
  sockaddr_in client_address;
  socklen_t len = sizeof(client_address);
  int err = getpeername(handle_, (struct sockaddr*)&client_address, &len);
  if (err != 0)
    SKYNET_DEBUG_LOG("ip_address_and_port threw error: {}", strerror(errno));

  return {inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port)};
}

AddrPortPair SocketCommunicator::host_ip_address_and_port() const noexcept
{
  sockaddr_in host_address;
  socklen_t len = sizeof(host_address);
  getsockname(handle_, (struct sockaddr*)&host_address, &len);
  return {inet_ntoa(host_address.sin_addr), ntohs(host_address.sin_port)};  
}

SocketCommunicator::SocketCommunicator(WithRawHandle, const int handle) noexcept : handle_{handle} {}

std::vector<std::byte> read_chunked(SocketCommunicator& conn, const std::size_t num_bytes) noexcept
{
  // Size of memory to allocate/read each step
  constexpr std::size_t read_step_size = 0x0'1000;
  constexpr std::size_t allocate_step_size = read_step_size * 16;
  // How often memory needs to be resized
  constexpr std::size_t resize_every_n_steps = allocate_step_size / read_step_size;
  // Ensure that the allocate size is evenly divisible by the read size
  static_assert(allocate_step_size % read_step_size == 0);
  static_assert(allocate_step_size >= read_step_size);
  // To prevent overallocation of memory, don't allocate a ton of memory to start
  std::vector<std::byte> read_bytes;
  // The final bytes to read in the end
  const int final_read_size = num_bytes % read_step_size;
  // Read memory in 4KiB chunks
  const int num_iters = num_bytes / read_step_size + (final_read_size == 0 ? 0 : 1);
  for (int i = 0; i < num_iters; ++i) {
    if (i % resize_every_n_steps == 0) {
      // Allocate more memory
      const std::size_t mem_left_to_read = num_bytes - read_bytes.size();
      const std::size_t additional_size = mem_left_to_read > allocate_step_size ? allocate_step_size : mem_left_to_read;
      read_bytes.resize(read_bytes.size() + additional_size);
    }
    const std::size_t num_bytes_to_read = (i == num_iters - 1 ? final_read_size : read_step_size);
    // Allocate more memory if needed
    if (conn.read_message(&read_bytes[i * read_step_size], num_bytes_to_read) != ConnectionError::no_error) {
      return {};
    }
  }
  return read_bytes;
}

AddrPortPair split_address(const std::string_view address) noexcept
{
  // Split the address by the colon
  const auto colon_loc = address.find(':');
  if (colon_loc == std::string_view::npos) { return {}; }
  const auto port_str = address.substr(colon_loc + 1);
  // Try to parse the port
  char* end;
  const auto port = strtol(port_str.data(), &end, 10);
  // Check that the entire string was parsed and that the port is valid
  if (end != port_str.data() + port_str.size() || port < 0 || port > 0xFFFF) { return {}; }
  // Try to connect to the publisher
  // Need to make a std::string to ensure that it is null-terminated
  const std::string address_str{address.begin(), address.begin() + colon_loc};
  return {address_str, port};
}

std::variant<NetworkSizeType, ConnectionError> read_network_size(SocketCommunicator& conn) noexcept
{
  std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
  const auto err = conn.read_message(size_buffer.data(), size_buffer.size());
  if (err == ConnectionError::no_error) { return from_network_bytes(size_buffer); }
  return err;
}

std::string to_ip_port(const AddrPortPair& addr) noexcept
{
  const auto& [name, port] = to_canonical(addr);
  return name + ':' + std::to_string(port);
}

AddrPortPair to_canonical(const AddrPortPair& addr) noexcept
{
  const auto result = resolve_addr(addr.first.c_str(), addr.second);
  sockaddr_in* info = reinterpret_cast<sockaddr_in*>(result->ai_addr);
  const std::string to_ret = std::to_string((info->sin_addr.s_addr & 0x000000FF) >> 0) + '.'
                           + std::to_string((info->sin_addr.s_addr & 0x0000FF00) >> 8) + '.'
                           + std::to_string((info->sin_addr.s_addr & 0x00FF0000) >> 16) + '.'
                           + std::to_string((info->sin_addr.s_addr & 0xFF000000) >> 24);
  return {to_ret, addr.second};
}
} // namespace skywing::internal
