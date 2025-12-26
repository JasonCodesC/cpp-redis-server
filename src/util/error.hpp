#pragma once
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <string>

namespace util {

__attribute__((noinline))
[[noreturn]] inline void die(std::string_view msg) {
  std::fprintf(stderr, "%s\n", msg.data());
  std::exit(EXIT_FAILURE);
}

__attribute__((noinline))
[[noreturn]] inline void die_errno(std::string_view msg) {
  std::perror(std::string(msg).c_str());
  std::exit(EXIT_FAILURE);
}

inline void syscall_or_die(int result, std::string_view msg) {
  if (result == -1) {
    die_errno(msg);
  }
}

}
