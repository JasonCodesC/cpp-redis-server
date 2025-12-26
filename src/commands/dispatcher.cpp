#include "dispatcher.hpp"

#include <charconv>

#include "../protocol/resp.hpp"

namespace commands {

namespace {
enum class Command {
  Ping,
  Echo,
  Set,
  Get,
  Del,
  Exists,
  Expire,
  Ttl,
  Unknown
};

Command to_command(std::string_view cmd) {
  if (cmd == "SET") return Command::Set;
  if (cmd == "GET") return Command::Get;
  if (cmd == "DEL") return Command::Del;
  if (cmd == "EXISTS") return Command::Exists;
  if (cmd == "EXPIRE") return Command::Expire;
  if (cmd == "TTL") return Command::Ttl;
  if (cmd == "PING") return Command::Ping;
  if (cmd == "ECHO") return Command::Echo;
  return Command::Unknown;
}

bool parse_ll(std::string_view s, long long& out) {
  const char* begin = s.data();
  const char* end = begin + s.size();
  auto [ptr, ec] = std::from_chars(begin, end, out);
  return ec == std::errc() && ptr == end;
}
}  // namespace

void Dispatcher::dispatch(const std::vector<std::string_view>& args, std::string& out) {
  if (args.empty()) {
    resp::append_error(out, "empty command");
    return;
  }

  switch (to_command(args[0])) {
    case Command::Set:
      handle_set(args, out);
      break;
    case Command::Get:
      handle_get(args, out);
      break;
    case Command::Del:
      handle_del(args, out);
      break;
    case Command::Exists:
      handle_exists(args, out);
      break;
    case Command::Expire:
      handle_expire(args, out);
      break;
    case Command::Ttl:
      handle_ttl(args, out);
      break;
    case Command::Ping:
      handle_ping(args, out);
      break;
    case Command::Echo:
      handle_echo(args, out);
      break;
    case Command::Unknown:
    default:
      resp::append_error(out, "unknown command");
      break;
  }
}

void Dispatcher::handle_ping(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() > 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'ping'");
    return;
  }
  if (args.size() == 1) {
    resp::append_status_string(out, "PONG");
  } 
  else {
    resp::append_string(out, args[1]);
  }
}

void Dispatcher::handle_echo(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() != 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'echo'");
    return;
  }
  resp::append_string(out, args[1]);
}

void Dispatcher::handle_set(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() != 3) {
    resp::append_error(out, "ERR wrong number of arguments for 'set'");
    return;
  }
  store.set(std::string(args[1]), std::string(args[2]));
  resp::append_ok(out);
}

void Dispatcher::handle_get(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() != 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'get'");
    return;
  }
  auto val = store.get(std::string(args[1]));
  resp::append_string(out, val);
}

void Dispatcher::handle_del(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() < 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'del'");
    return;
  }
  long long removed = 0;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (store.del(std::string(args[i]))) {
      ++removed;
    }
  }
  resp::append_integer(out, removed);
}

void Dispatcher::handle_exists(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() < 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'exists'");
    return;
  }
  long long count = 0;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (store.exists(std::string(args[i]))) {
      ++count;
    }
  }
  resp::append_integer(out, count);
}

void Dispatcher::handle_expire(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() != 3) {
    resp::append_error(out, "ERR wrong number of arguments for 'expire'");
    return;
  }
  long long ttl_ms = 0;
  if (!parse_ll(args[2], ttl_ms) || ttl_ms < 0) {
    resp::append_error(out, "ERR invalid expire time");
    return;
  }
  bool ok = store.expire(std::string(args[1]), ttl_ms);
  resp::append_integer(out, ok ? 1 : 0);
}

void Dispatcher::handle_ttl(const std::vector<std::string_view>& args, std::string& out) {
  if (args.size() != 2) {
    resp::append_error(out, "ERR wrong number of arguments for 'ttl'");
    return;
  }
  long long remaining = store.ttl(std::string(args[1]));
  resp::append_integer(out, remaining);
}

}
