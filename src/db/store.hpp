#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "../util/time.hpp"

namespace db {

class Store {
 public:
  std::optional<std::string> get(const std::string& key);
  void set(const std::string& key, const std::string& value);
  bool del(const std::string& key);
  bool exists(const std::string& key);

  // Expire in milliseconds, returns true if expiration set, false if key missing.
  bool expire(const std::string& key, long long ttl_ms);
  // Time left to live in milliseconds, -1 if no expiration, -2 if key missing/expired.
  long long ttl(const std::string& key);

  // Optional periodic sweep to remove expired entries.
  void sweep_expired();

 private:
  bool is_expired(const std::string& key, util::TimePoint now);
  void remove_expiration(const std::string& key);

  std::unordered_map<std::string, std::string> kv;
  std::unordered_map<std::string, util::TimePoint> expires;
};

}  // namespace db
