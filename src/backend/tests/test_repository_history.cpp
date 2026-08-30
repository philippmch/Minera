#undef NDEBUG

#include <cassert>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/repository_history.hpp"

namespace {
bool rejects(const auto& fn) {
  try { fn(); } catch (const std::invalid_argument&) { return true; }
  return false;
}
}  // namespace

int main() {
  const std::string base = "https://api.example.test";
  const std::string page1 = base + "/repos/acme/engine/issues?state=all&per_page=100&page=1";
  const std::string page2 = base + "/repos/acme/engine/issues?state=all&per_page=100&page=2";
  const std::string timeline = base + "/repos/acme/engine/issues/7/timeline";
  std::map<std::string, minera::RepositoryHistoryResponse> responses = {
      {page1, {200, R"json([{"timeline_url":"https://api.example.test/repos/acme/engine/issues/7/timeline","created_at":"2024-01-01T10:00:00Z","user":{"login":"alice"},"author_association":"MEMBER","title":"Unicode \ud83d\ude80","pull_request":{"url":"https://api.example.test/pulls/7"}}])json"}},
      {page2, {200, "[]"}},
      {timeline, {200, R"json([{"created_at":"2024-01-03T10:00:00Z","event":"closed","actor":{"login":"carol"}},{"created_at":"2024-01-02T10:00:00Z","event":"labeled","actor":{"login":"bob"}},{"event":"ignored","actor":null}])json"}}};
  std::vector<std::string> calls;
  minera::RepositoryHistoryOptions options;
  options.owner = "acme";
  options.repository = "engine";
  options.api_base_url = base;
  options.auth_token = "secret";
  options.transport = [&](std::string_view url, std::string_view token) {
    assert(token == "secret"); calls.emplace_back(url); return responses.at(std::string(url));
  };
  const auto log = minera::import_repository_history(options);
  assert(calls == std::vector<std::string>({page1, timeline, page2}));
  assert(log.trace_count() == 1 && log.event_count() == 3);
  const auto& trace = log.traces.front();
  assert(trace.case_id == timeline);
  assert(*trace.attribute("case:owner") == "acme");
  assert(*trace.attribute("case:title") == "Unicode 🚀");
  assert(*trace.attribute("case:pull_request") == "https://api.example.test/pulls/7");
  assert(trace.events[0].activity == "created" && trace.events[0].resource == "alice");
  assert(trace.events[1].activity == "labeled" && trace.events[1].resource == "bob");
  assert(trace.events[2].activity == "closed" && trace.events[2].resource == "carol");
  const auto json = minera::repository_history_json(log, "acme", "engine");
  assert(json.find("\"type\":\"REPOSITORY_HISTORY\"") != std::string::npos);
  assert(json.find("Unicode 🚀") != std::string::npos);

  auto invalid = options;
  invalid.owner = "bad/name";
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
  invalid = options;
  invalid.max_events = 2;
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
  invalid = options;
  invalid.max_pages = 1;
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
  invalid = options;
  invalid.transport = [](std::string_view, std::string_view) {
    return minera::RepositoryHistoryResponse{500, "{}"};
  };
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
  invalid.transport = [](std::string_view, std::string_view) {
    return minera::RepositoryHistoryResponse{200, "not json"};
  };
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
  invalid.transport = [page1](std::string_view url, std::string_view) {
    if (url == page1)
      return minera::RepositoryHistoryResponse{200, R"([{"timeline_url":"https://evil.test/repos/acme/engine/issues/1","created_at":"2024-01-01Z","user":{"login":"a"},"author_association":"NONE","title":"x"}])"};
    return minera::RepositoryHistoryResponse{200, "[]"};
  };
  assert(rejects([&] { (void)minera::import_repository_history(invalid); }));
}
