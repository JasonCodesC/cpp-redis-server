#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "commands/dispatcher.hpp"
#include "db/store.hpp"
#include "net/connection.hpp"
#include "net/epoll.hpp"
#include "net/socket.hpp"
#include "util/error.hpp"

int main() {
  std::cout << "Redis Started \n";
  uint16_t port = 9000;

  int listen_fd = net::create_listen_socket(port);
  net::Epoll epoll;
  if (!epoll.add(listen_fd, EPOLLIN)) {
    util::die_errno("epoll add listen_fd");
  }

  db::Store store;
  commands::Dispatcher dispatcher(store);
  std::unordered_map<int, std::unique_ptr<net::Connection>> conns;

  auto dispatch_cb = [&](const std::vector<std::string_view>& args, std::string& out) {
    dispatcher.dispatch(args, out);
  };

  while (true) {
    int n = epoll.wait(-1);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      util::die_errno("epoll_wait");
    }

    epoll_event* events = epoll.events_data();
    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;
      uint32_t ev = events[i].events;

      if (fd == listen_fd) {
        // Accept as many clients as are queued.
        while (true) {
          int client_fd = ::accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
          if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            if (errno == EINTR) {
              continue;
            }
            break;
          }

          net::set_tcp_nodelay(client_fd);
          conns.emplace(client_fd, std::make_unique<net::Connection>(client_fd));
          epoll.add(client_fd, EPOLLIN);
        }
        continue;
      }

      auto it = conns.find(fd);
      if (it == conns.end()) {
        continue;
      }
      net::Connection& conn = *it->second;

      bool alive = true;
      if (ev & (EPOLLERR | EPOLLHUP)) {
        alive = false;
      }
      if (alive && (ev & EPOLLIN)) {
        alive = conn.on_read(dispatch_cb);
      }
      if (alive && (ev & EPOLLOUT)) {
        alive = conn.on_write();
      }

      if (!alive) {
        epoll.del(fd);
        conns.erase(it);
        continue;
      }

      uint32_t new_events = EPOLLIN;
      if (conn.wants_write()) {
        new_events |= EPOLLOUT;
      }
      epoll.mod(fd, new_events);
    }
  }

  return 0;
}
