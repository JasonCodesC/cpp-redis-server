#include "resp_parser.hpp"

#include <charconv>
#include <cstddef>

namespace resp {

namespace {
constexpr std::size_t invalid_pos = static_cast<std::size_t>(-1);
}

std::size_t RespParser::find_terminator(const std::string& buffer, std::size_t from) {
  auto pos = buffer.find("\r\n", from);
  return pos == std::string::npos ? invalid_pos : pos;
}

bool RespParser::parse_integer(std::string_view view, long long& out) {
  const char* begin = view.data();
  const char* end = begin + view.size();
  auto [ptr, ec] = std::from_chars(begin, end, out);
  return ec == std::errc() && ptr == end;
}

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

  const auto array_line_end = find_terminator(buffer, cursor);
  if (array_line_end == invalid_pos) {
    return false;  // incomplete
  }

  long long array_len = 0;
  if (!parse_integer(std::string_view(buffer.data() + cursor, array_line_end - cursor), array_len) 
    || array_len < 0) {

    has_error = true;
    return false;
  }

  cursor = array_line_end + 2;  // skip \r\n
  args.reserve(static_cast<std::size_t>(array_len));

  for (long long i{}; i < array_len; ++i) {
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

    const auto bulk_line_end = find_terminator(buffer, cursor);
    if (bulk_line_end == invalid_pos) {
      args.clear();
      return false;  // incomplete
    }

    long long bulk_len = 0;
    if (!parse_integer(std::string_view(buffer.data() + cursor, bulk_line_end - cursor), bulk_len) 
      || bulk_len < 0) {
      has_error = true;
      args.clear();
      return false;
    }

    cursor = bulk_line_end + 2;  // skip \r\n

    const std::size_t need = static_cast<std::size_t>(bulk_len) + 2;  // data + \r\n
    if (cursor + need > buffer.size()) {
      args.clear();
      return false;  // incomplete
    }

    args.emplace_back(buffer.data() + cursor, static_cast<std::size_t>(bulk_len));
    cursor += static_cast<std::size_t>(bulk_len);

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
