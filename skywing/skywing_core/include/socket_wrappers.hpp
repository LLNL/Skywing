#ifndef SKYNET_SRC_SOCKET_WRAPPERS_HPP
#define SKYNET_SRC_SOCKET_WRAPPERS_HPP

// OSX has to go through a few more steps to init non-blocking sockets, so
// these wrappers are to help with that

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace skywing::internal {
/** \brief Creates a socket in non-blocking mode
 */
int create_non_blocking() noexcept;

/** \brief Accepts on a socket and puts the connection in non-blocking mode
 */
int accept_make_non_blocking(const int sockfd, sockaddr* addr, socklen_t* addrlen) noexcept;
} // namespace skywing::internal

#endif // SKYNET_SRC_SOCKET_WRAPPERS_HPP
