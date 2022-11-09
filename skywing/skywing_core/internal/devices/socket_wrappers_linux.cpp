#include "socket_wrappers.hpp"

namespace skywing::internal {
int create_non_blocking() noexcept { return socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); }

int accept_make_non_blocking(const int sockfd, sockaddr* addr, socklen_t* addrlen) noexcept
{
  return accept4(sockfd, addr, addrlen, SOCK_NONBLOCK);
}
} // namespace skywing::internal
