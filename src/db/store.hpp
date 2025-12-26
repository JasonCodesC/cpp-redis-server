#pragma once

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../util/time.hpp"

namespace db {

struct TransparentStringHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }
};

struct TransparentStringEq {
  using is_transparent = void;
  bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
    return lhs == rhs;
  }
};

class Store {
 public:
  Store();

  std::optional<std::string_view> get(std::string_view key);
  void set(std::string_view key, std::string_view value);
  bool del(std::string_view key);
  bool exists(std::string_view key);

  // Expire in milliseconds, returns true if expiration set, false if key missing.
  bool expire(std::string_view key, long long ttl_ms);
  // Time left to live in milliseconds, -1 if no expiration, -2 if key missing/expired.
  long long ttl(std::string_view key);

  // Optional periodic sweep to remove expired entries.
 void sweep_expired();

 private:
  void remove_expiration(std::string_view key);

  std::pmr::unsynchronized_pool_resource pool;
  using PmrString = std::pmr::string;
  using KvMap = std::pmr::unordered_map<PmrString, PmrString, TransparentStringHash, TransparentStringEq>;
  using ExpMap = std::pmr::unordered_map<PmrString, util::TimePoint, TransparentStringHash, TransparentStringEq>;
  KvMap kv;
  ExpMap expires;
};

}  // namespace db
