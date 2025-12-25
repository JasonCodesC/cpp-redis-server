#pragma once
#include <chrono>

namespace util {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

inline TimePoint now() {
  return Clock::now();
}

}
