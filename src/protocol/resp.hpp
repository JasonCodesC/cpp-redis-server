#pragma once
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace resp {

inline constexpr std::string_view line_terminator = "\r\n";
inline constexpr std::string_view success = "+OK\r\n";
inline constexpr std::string_view null_string = "$-1\r\n";
inline constexpr std::string_view null_array = "*-1\r\n";

// simple for ok, queued, etc
inline void append_status_string(std::string& out, std::string_view msg) {
  out.push_back('+');
  out.append(msg);
  out.append(line_terminator);
}

// for actual string data
inline void append_string(std::string& out, std::string_view value) {
  char len_buf[32];
  auto [ptr, ec] = std::to_chars(len_buf, len_buf + sizeof(len_buf), value.size());
  (void)ec;
  out.push_back('$');
  out.append(len_buf, static_cast<std::size_t>(ptr - len_buf));
  out.append(line_terminator);
  out.append(value);
  out.append(line_terminator);
}

inline void append_string(std::string& out, std::optional<std::string_view> value) {
  if (value.has_value()) {
    append_string(out, *value);
  } 
  else {
    out.append(null_string);
  }
}

inline void append_ok(std::string& out) {
  out.append(success);
}

inline void append_error(std::string& out, std::string_view msg) {
  out.append("-ERR ");
  out.append(msg);
  out.append(line_terminator);
}

inline void append_integer(std::string& out, long long value) {
  out.push_back(':');
  out.append(std::to_string(value));
  out.append(line_terminator);
}

inline void append_null_string(std::string& out) {
  out.append(null_string);
}

inline void append_array_header(std::string& out, std::size_t count) {
  out.push_back('*');
  out.append(std::to_string(count));
  out.append(line_terminator);
}

inline void append_null_array(std::string& out) {
  out.append(null_array);
}

}
