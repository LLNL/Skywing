#include "socket_wrappers.hpp"

namespace skywing::internal {
namespace {
/** \brief Sets a socket to a non-blocking mode, returning the socket handle
 *
 * Does nothing if the handle is invalid
 */
int set_non_blocking(const int sockfd) noexcept
{
  if (sockfd == -1) { return sockfd; }
  const auto flags = fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK;
  fcntl(sockfd, F_SETFL, flags);
  return sockfd;
}
} // namespace

int create_non_blocking() noexcept { return set_non_blocking(socket(AF_INET, SOCK_STREAM, 0)); }

int accept_make_non_blocking(const int sockfd, sockaddr* addr, socklen_t* addrlen) noexcept
{
  return set_non_blocking(accept(sockfd, addr, addrlen));
}
} // namespace skywing::internal
