#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace resp {

// Streaming parser for RESP arrays of bulk strings.
// Typical use:
//   while (parser.parse(buf)) {
//     const auto& args = parser.argv();
//     ... process args ...
//     parser.consume(buf);
//   }
// If error() is true, the buffer contained a protocol violation.

class RespParser {
  std::vector<std::string_view> args;
  std::size_t consumed{};
  bool has_error{false};
public:

  // Attempts to parse one command returns true if valid
  bool parse(std::string& buffer);

  const std::vector<std::string_view>& argv() const { return args; }
  std::size_t consumed_bytes() const { return consumed; }
  bool error() const { return has_error; }

  // Drop parsed bytes from buffer (invalidates argv string_views).
  void consume(std::string& buffer);

  void reset();

};

}  // namespace resp
