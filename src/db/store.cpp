#include "store.hpp"

#include <utility>

namespace db {

Store::Store() : kv(&pool), expires(&pool) {}

std::optional<std::string_view> Store::get(std::string_view key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return std::nullopt;
  }
  auto exp_it = expires.find(key);
  if (exp_it == expires.end()) {
    return it->second;
  }
  auto now = util::now();
  if (exp_it->second <= now) {
    kv.erase(it);
    expires.erase(exp_it);
    return std::nullopt;
  }
  return it->second;
}

void Store::set(std::string_view key, std::string_view value) {
  std::pmr::string key_pmr(key, &pool);
  std::pmr::string value_pmr(value, &pool);
  kv.insert_or_assign(std::move(key_pmr), std::move(value_pmr));
  remove_expiration(key);
}

bool Store::del(std::string_view key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  kv.erase(it);
  auto exp_it = expires.find(key);
  if (exp_it != expires.end()) {
    expires.erase(exp_it);
  }
  return true;
}

bool Store::exists(std::string_view key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  auto exp_it = expires.find(key);
  if (exp_it == expires.end()) {
    return true;
  }
  auto now = util::now();
  if (exp_it->second <= now) {
    kv.erase(it);
    expires.erase(exp_it);
    return false;
  }
  return true;
}

bool Store::expire(std::string_view key, long long ttl_ms) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return false;
  }
  auto deadline = util::now() + std::chrono::milliseconds(ttl_ms);
  auto exp_it = expires.find(key);
  if (exp_it != expires.end()) {
    exp_it->second = deadline;
  } else {
    expires.emplace(std::pmr::string(key, &pool), deadline);
  }
  return true;
}

long long Store::ttl(std::string_view key) {
  auto it = kv.find(key);
  if (it == kv.end()) {
    return -2;
  }
  auto exp_it = expires.find(key);
  if (exp_it == expires.end()) {
    return -1;
  }
  auto now = util::now();
  if (exp_it->second <= now) {
    kv.erase(it);
    expires.erase(exp_it);
    return -2;
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

void Store::remove_expiration(std::string_view key) {
  auto exp_it = expires.find(key);
  if (exp_it != expires.end()) {
    expires.erase(exp_it);
  }
}

}  // namespace db
