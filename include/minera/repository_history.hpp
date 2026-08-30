#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

struct RepositoryHistoryResponse {
  int status = 0;
  std::string body;
};

using RepositoryHistoryTransport = std::function<RepositoryHistoryResponse(
    std::string_view url, std::string_view bearer_token)>;

struct RepositoryHistoryOptions {
  std::string owner;
  std::string repository;
  std::string api_base_url = "https://api.github.com";
  std::string auth_token;
  std::size_t max_pages = 100;
  std::size_t max_issues = 10'000;
  std::size_t max_events = 1'000'000;
  RepositoryHistoryTransport transport;
};

[[nodiscard]] EventLog import_repository_history(
    const RepositoryHistoryOptions& options);
[[nodiscard]] std::string repository_history_json(
    const EventLog& log, std::string_view owner, std::string_view repository);

}  // namespace minera
