#include "epoll.hpp"

#include <cerrno>
#include <unistd.h>

#include "../util/error.hpp"

namespace net {

Epoll::Epoll(int max_events) : events(static_cast<std::size_t>(max_events)) {
  fd = ::epoll_create1(EPOLL_CLOEXEC);
  util::syscall_or_die(fd, "epoll_create1");
}

Epoll::~Epoll() {
  if (fd != -1) {
    ::close(fd);
  }
}

bool Epoll::add(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return ::epoll_ctl(this->fd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoll::mod(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return ::epoll_ctl(this->fd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoll::del(int fd) {
  return ::epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int Epoll::wait(int timeout_ms) {
  int n = ::epoll_wait(fd, events.data(), static_cast<int>(events.size()), timeout_ms);
  if (n < 0 && errno == EINTR) {
    return 0;
  }
  return n;
}

}  // namespace net
