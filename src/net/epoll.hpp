#pragma once

#include <sys/epoll.h>

#include <vector>

namespace net {

class Epoll {
 public:
  explicit Epoll(int max_events = 128);
  ~Epoll();

  Epoll(const Epoll&) = delete;
  Epoll& operator=(const Epoll&) = delete;

  bool add(int fd, uint32_t events);
  bool mod(int fd, uint32_t events);
  bool del(int fd);

  int wait(int timeout_ms);
  epoll_event* events_data() { return events.data(); }

 private:
  int fd{-1};
  std::vector<epoll_event> events;
};

}  // namespace net
