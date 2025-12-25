#include "socket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

#include "../util/error.hpp"

namespace net {

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  util::syscall_or_die(flags, "fcntl(F_GETFL)");
  util::syscall_or_die(fcntl(fd, F_SETFL, flags | O_NONBLOCK), "fcntl(F_SETFL)");
}

void set_tcp_nodelay(int fd) {
  int flag = 1;
  util::syscall_or_die(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)), "setsockopt(TCP_NODELAY)");
}

void set_reuseaddr(int fd) {
  int flag = 1;
  util::syscall_or_die(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)), "setsockopt(SO_REUSEADDR)");
}

int create_listen_socket(uint16_t port, int backlog) {
  int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  util::syscall_or_die(fd, "socket");

  set_reuseaddr(fd);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  util::syscall_or_die(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), "bind");
  util::syscall_or_die(::listen(fd, backlog), "listen");

  return fd;
}

}  // namespace net
