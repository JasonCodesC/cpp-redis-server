#include "resp_parser.hpp"

#include <cstddef>
#include <limits>

namespace resp {

namespace {
enum class ParseStatus { Ok, Incomplete, Error };

ParseStatus parse_length(const std::string& buffer, std::size_t& cursor, std::size_t& out) {
  std::size_t i = cursor;
  if (i >= buffer.size()) {
    return ParseStatus::Incomplete;
  }
  if (buffer[i] < '0' || buffer[i] > '9') {
    return ParseStatus::Error;
  }

  std::size_t value = 0;
  while (i < buffer.size() && buffer[i] >= '0' && buffer[i] <= '9') {
    std::size_t digit = static_cast<std::size_t>(buffer[i] - '0');
    if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
      return ParseStatus::Error;
    }
    value = value * 10 + digit;
    ++i;
  }

  if (i + 1 >= buffer.size()) {
    return ParseStatus::Incomplete;
  }
  if (buffer[i] != '\r' || buffer[i + 1] != '\n') {
    return ParseStatus::Error;
  }

  cursor = i + 2;
  out = value;
  return ParseStatus::Ok;
}
}  // namespace

bool RespParser::parse(std::string& buffer) {
  args.clear();
  consumed = 0;

  if (has_error || buffer.empty()) { return false; }

  std::size_t cursor = 0;

  // Expect arr header => *<count>\r\n
  if (buffer[cursor] != '*') {
    has_error = true;
    return false;
  }
  ++cursor;

  std::size_t array_len = 0;
  ParseStatus status = parse_length(buffer, cursor, array_len);
  if (status == ParseStatus::Incomplete) {
    return false;
  }
  if (status == ParseStatus::Error) {
    has_error = true;
    return false;
  }

  args.reserve(array_len);

  for (std::size_t i = 0; i < array_len; ++i) {
    if (cursor >= buffer.size()) {
      args.clear();
      return false;  // incomplete
    }

    if (buffer[cursor] != '$') {
      has_error = true;
      args.clear();
      return false;
    }
    ++cursor;

    std::size_t bulk_len = 0;
    status = parse_length(buffer, cursor, bulk_len);
    if (status == ParseStatus::Incomplete) {
      args.clear();
      return false;
    }
    if (status == ParseStatus::Error) {
      has_error = true;
      args.clear();
      return false;
    }

    const std::size_t need = bulk_len + 2;  // data + \r\n
    if (cursor + need > buffer.size()) {
      args.clear();
      return false;  // incomplete
    }

    args.emplace_back(buffer.data() + cursor, bulk_len);
    cursor += bulk_len;

    // Expect trailing \r\n
    if (buffer[cursor] != '\r' || buffer[cursor + 1] != '\n') {
      has_error = true;
      args.clear();
      return false;
    }
    cursor += 2;
  }

  consumed = cursor;
  return true;
}

void RespParser::consume(std::string& buffer) {
  if (consumed == 0 || consumed > buffer.size()) {
    return;
  }
  buffer.erase(0, consumed);
  consumed = 0;
}

void RespParser::reset() {
  args.clear();
  consumed = 0;
  has_error = false;
}

}  // namespace resp
