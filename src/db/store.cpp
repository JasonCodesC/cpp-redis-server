#include "store.hpp"

namespace db {

std::optional<std::string> Store::get(const std::string& key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return std::nullopt;
  }
  if (is_expired(key, util::now())) {
    kv.erase(it);
    expires.erase(key);
    return std::nullopt;
  }
  return it->second;
}

void Store::set(const std::string& key, const std::string& value) {
  kv[key] = value;
  remove_expiration(key);
}

bool Store::del(const std::string& key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  kv.erase(it);
  expires.erase(key);
  return true;
}

bool Store::exists(const std::string& key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  if (is_expired(key, util::now())) {
    kv.erase(it);
    expires.erase(key);
    return false;
  }
  return true;
}

bool Store::expire(const std::string& key, long long ttl_ms) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  auto deadline = util::now() + std::chrono::milliseconds(ttl_ms);
  expires[key] = deadline;
  return true;
}

long long Store::ttl(const std::string& key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return -2;
  }
  auto now = util::now();
  if (is_expired(key, now)) {
    kv.erase(it);
    expires.erase(key);
    return -2;
  }
  auto exp_it = expires.find(key);
  if (exp_it == expires.end()) {
    return -1;
  }
  auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(exp_it->second - now).count();
  return remaining < 0 ? 0 : remaining;
}

void Store::sweep_expired() {
  auto now = util::now();
  for (auto it = expires.begin(); it != expires.end();) {
    if (it->second <= now) {
      kv.erase(it->first);
      it = expires.erase(it);
    } 
    else {
      ++it;
    }
  }
}

bool Store::is_expired(const std::string& key, util::TimePoint now) {
  auto it = expires.find(key);
  if (it == expires.end()) {
    return false;
  }
  if (it->second <= now) {
    kv.erase(key);
    expires.erase(it);
    return true;
  }
  return false;
}

void Store::remove_expiration(const std::string& key) {
  expires.erase(key);
}

}  // namespace db
