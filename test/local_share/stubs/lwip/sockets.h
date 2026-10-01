#pragma once
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
inline unsigned short testPort = 0;
inline int testLoopbackBind(int fd, const sockaddr* address, socklen_t length) {
  if (length != sizeof(sockaddr_in)) {
    errno = EINVAL;
    return -1;
  }
  auto local = *reinterpret_cast<const sockaddr_in*>(address);
  if (local.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
    errno = EACCES;
    return -1;
  }
  local.sin_port = 0;  // No privileged port, AP or external interface in host tests.
  const int result = ::bind(fd, reinterpret_cast<const sockaddr*>(&local), sizeof(local));
  socklen_t size = sizeof(local);
  if (result == 0 && getsockname(fd, reinterpret_cast<sockaddr*>(&local), &size) == 0) testPort = ntohs(local.sin_port);
  return result;
}
#define bind testLoopbackBind
