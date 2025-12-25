#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "../protocol/resp_parser.hpp"

namespace net {

class Connection {
 public:
  explicit Connection(int fd);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  int fd() const { return fd; }
  bool closed() const { return fd == -1; }
  bool wants_write() const { return !write_buf.empty(); }

  // Returns false to indicate the connection should be closed.
  bool on_read(const std::function<void(const std::vector<std::string_view>&, std::string&)>& dispatch);
  bool on_write();

  void close();

 private:
  bool read_from_socket();
  bool flush_write();

  static constexpr std::size_t kMaxReadBuffer = 1 << 20;   // 1MB
  static constexpr std::size_t kMaxWriteBuffer = 1 << 20;  // 1MB

  int fd;
  std::string read_buf;
  std::string write_buf;
  resp::RespParser parser;
};

}  // namespace net
