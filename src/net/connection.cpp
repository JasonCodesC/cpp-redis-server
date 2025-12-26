#include "connection.hpp"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../protocol/resp.hpp"

namespace net {

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
  close();
}

void Connection::close() {
  if (fd_ != -1) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Connection::read_from_socket() {
  char buf[4096];
  while (true) {
    ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
    if (n > 0) {
      if (read_buf.size() + static_cast<std::size_t>(n) > kMaxReadBuffer) {
        return false;  // too large
      }
      read_buf.append(buf, static_cast<std::size_t>(n));
      continue;
    }
    if (n == 0) {
      return false;  // peer closed
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

bool Connection::flush_write() { // send replies
  while (pending_write_bytes() > 0) {
    const char* data = write_buf.data() + write_offset;
    const std::size_t len = pending_write_bytes();
    ssize_t n = ::send(fd_, data, len, 0);
    if (n > 0) {
      write_offset += static_cast<std::size_t>(n);
      if (write_offset == write_buf.size()) {
        write_buf.clear();
        write_offset = 0;
      }
      continue;
    }
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return true;
    }
    if (n == -1 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

bool Connection::on_read(const std::function<void(const std::vector<std::string_view>&, std::string&)>& dispatch) {
  if (!read_from_socket()) {
    return false;
  }

  while (parser.parse(read_buf)) {
    maybe_compact_write_buf();
    dispatch(parser.argv(), write_buf);
    parser.consume(read_buf);
    if (pending_write_bytes() > kMaxWriteBuffer) {
      return false;  // backpressure failure
    }
  }

  if (parser.error()) {
    resp::append_error(write_buf, "protocol error");
    return false;
  }

  return true;
}

bool Connection::on_write() {
  return flush_write();
}

void Connection::maybe_compact_write_buf() {
  if (write_offset == 0) {
    return;
  }
  if (write_offset >= 4096 && write_offset * 2 >= write_buf.size()) {
    write_buf.erase(0, write_offset);
    write_offset = 0;
  }
}

std::size_t Connection::pending_write_bytes() const {
  return write_buf.size() - write_offset;
}

}  // namespace net
