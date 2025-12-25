#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../db/store.hpp"

namespace commands {

class Dispatcher {
 public:
  explicit Dispatcher(db::Store& store) : store(store) {}

  void dispatch(const std::vector<std::string_view>& args, std::string& out);

 private:
  void handle_ping(const std::vector<std::string_view>& args, std::string& out);
  void handle_echo(const std::vector<std::string_view>& args, std::string& out);
  void handle_set(const std::vector<std::string_view>& args, std::string& out);
  void handle_get(const std::vector<std::string_view>& args, std::string& out);
  void handle_del(const std::vector<std::string_view>& args, std::string& out);
  void handle_exists(const std::vector<std::string_view>& args, std::string& out);
  void handle_expire(const std::vector<std::string_view>& args, std::string& out);
  void handle_ttl(const std::vector<std::string_view>& args, std::string& out);

  db::Store& store;
};

}  // namespace commands
