#include "minera/repository_history.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

namespace minera {
namespace {

struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view input) : input_(input) {
    if (input.size() > 16 * 1024 * 1024)
      fail("response exceeds 16 MiB");
  }
  Json read() {
    auto result = value(0);
    space();
    if (position_ != input_.size()) fail("trailing content");
    return result;
  }

 private:
  [[noreturn]] void fail(std::string_view reason) const {
    throw std::invalid_argument("Repository history: invalid JSON (" +
                                std::string(reason) + ")");
  }
  void space() {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\n' ||
            input_[position_] == '\r' || input_[position_] == '\t'))
      ++position_;
  }
  char take() {
    if (position_ == input_.size()) fail("unexpected end");
    return input_[position_++];
  }
  bool consume(std::string_view token) {
    if (input_.substr(position_, token.size()) != token) return false;
    position_ += token.size();
    return true;
  }
  static void append_utf8(std::string& out, unsigned code) {
    if (code <= 0x7f) out.push_back(static_cast<char>(code));
    else if (code <= 0x7ff) {
      out.push_back(static_cast<char>(0xc0 | code >> 6));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else if (code <= 0xffff) {
      out.push_back(static_cast<char>(0xe0 | code >> 12));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xf0 | code >> 18));
      out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    }
  }
  unsigned hex4() {
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = take();
      result <<= 4;
      if (c >= '0' && c <= '9') result += c - '0';
      else if (c >= 'a' && c <= 'f') result += c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') result += c - 'A' + 10;
      else fail("invalid Unicode escape");
    }
    return result;
  }
  std::string string() {
    if (take() != '"') fail("expected string");
    std::string out;
    while (true) {
      const char c = take();
      if (c == '"') return out;
      if (static_cast<unsigned char>(c) < 0x20) fail("control in string");
      if (c != '\\') { out.push_back(c); continue; }
      const char escape = take();
      if (escape == '"' || escape == '\\' || escape == '/') out.push_back(escape);
      else if (escape == 'b') out.push_back('\b');
      else if (escape == 'f') out.push_back('\f');
      else if (escape == 'n') out.push_back('\n');
      else if (escape == 'r') out.push_back('\r');
      else if (escape == 't') out.push_back('\t');
      else if (escape == 'u') {
        unsigned code = hex4();
        if (code >= 0xd800 && code <= 0xdbff) {
          if (!consume("\\u")) fail("unpaired surrogate");
          const unsigned low = hex4();
          if (low < 0xdc00 || low > 0xdfff) fail("unpaired surrogate");
          code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
        } else if (code >= 0xdc00 && code <= 0xdfff) fail("unpaired surrogate");
        append_utf8(out, code);
      } else fail("invalid escape");
      if (out.size() > 1024 * 1024) fail("string exceeds 1 MiB");
    }
  }
  Json value(std::size_t depth) {
    if (depth > 64) fail("nesting exceeds 64 levels");
    if (++nodes_ > 1'000'000) fail("too many values");
    space();
    if (position_ == input_.size()) fail("unexpected end");
    if (input_[position_] == '"') return Json{string()};
    if (consume("null")) return Json{nullptr};
    if (consume("true")) return Json{true};
    if (consume("false")) return Json{false};
    if (input_[position_] == '[') {
      ++position_; Json::Array array; space();
      if (position_ < input_.size() && input_[position_] == ']') { ++position_; return Json{std::move(array)}; }
      while (true) {
        array.push_back(value(depth + 1)); space();
        const char separator = take();
        if (separator == ']') return Json{std::move(array)};
        if (separator != ',') fail("expected comma");
      }
    }
    if (input_[position_] == '{') {
      ++position_; Json::Object object; space();
      if (position_ < input_.size() && input_[position_] == '}') { ++position_; return Json{std::move(object)}; }
      while (true) {
        space(); if (position_ == input_.size() || input_[position_] != '"') fail("expected object key");
        auto key = string(); space(); if (take() != ':') fail("expected colon");
        if (!object.emplace(std::move(key), value(depth + 1)).second) fail("duplicate object key");
        space(); const char separator = take();
        if (separator == '}') return Json{std::move(object)};
        if (separator != ',') fail("expected comma");
      }
    }
    const auto start = position_;
    if (input_[position_] == '-') ++position_;
    if (position_ == input_.size()) fail("invalid number");
    if (input_[position_] == '0') ++position_;
    else {
      if (input_[position_] < '1' || input_[position_] > '9') fail("invalid value");
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_; const auto digits = position_;
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
      if (digits == position_) fail("invalid number");
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_; if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
      const auto digits = position_;
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
      if (digits == position_) fail("invalid number");
    }
    double number = 0;
    const auto token = input_.substr(start, position_ - start);
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
    if (parsed.ec != std::errc{} || !std::isfinite(number)) fail("invalid number");
    return Json{number};
  }
  std::string_view input_;
  std::size_t position_ = 0;
  std::size_t nodes_ = 0;
};

const Json::Object& object(const Json& value, std::string_view context) {
  const auto* result = std::get_if<Json::Object>(&value.value);
  if (!result) throw std::invalid_argument("Repository history: expected object in " + std::string(context));
  return *result;
}
const Json::Array& array(const Json& value, std::string_view context) {
  const auto* result = std::get_if<Json::Array>(&value.value);
  if (!result) throw std::invalid_argument("Repository history: expected array in " + std::string(context));
  return *result;
}
const Json* member(const Json::Object& value, std::string_view key) {
  const auto it = value.find(key); return it == value.end() ? nullptr : &it->second;
}
std::string text(const Json::Object& value, std::string_view key, bool required = true) {
  const auto* item = member(value, key);
  if (!item) {
    if (!required) return {};
    throw std::invalid_argument("Repository history: missing field " + std::string(key));
  }
  const auto* result = std::get_if<std::string>(&item->value);
  if (!result) throw std::invalid_argument("Repository history: field " + std::string(key) + " must be a string");
  return *result;
}
std::string nested_text(const Json::Object& value, std::string_view parent,
                        std::string_view child, bool required = true) {
  const auto* item = member(value, parent);
  if (!item) {
    if (!required) return {};
    throw std::invalid_argument("Repository history: missing field " + std::string(parent));
  }
  if (std::holds_alternative<std::nullptr_t>(item->value) && !required) return {};
  return text(object(*item, parent), child, required);
}
void validate_name(std::string_view value, std::string_view label) {
  if (value.empty()) throw std::invalid_argument("Repository history: " + std::string(label) + " is required");
  if (value.find_first_of("/?#\\ \t\r\n") != std::string_view::npos)
    throw std::invalid_argument("Repository history: invalid " + std::string(label));
}
RepositoryHistoryResponse fetch(const RepositoryHistoryOptions& options,
                                std::string_view url) {
  const auto response = options.transport(url, options.auth_token);
  if (response.status < 200 || response.status >= 300)
    throw std::invalid_argument("Repository history: endpoint returned status " + std::to_string(response.status));
  return response;
}
EventAttribute attr(std::string key, std::string value) {
  return {std::move(key), std::move(value), AttributeKind::kString, false};
}
std::string escape_json(std::string_view value) {
  std::string out;
  for (const unsigned char c : value) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20) {
      constexpr char hex[] = "0123456789abcdef";
      out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
    } else out += static_cast<char>(c);
  }
  return out;
}

}  // namespace

EventLog import_repository_history(const RepositoryHistoryOptions& options) {
  validate_name(options.owner, "owner");
  validate_name(options.repository, "repository");
  if (options.api_base_url.empty() || options.api_base_url.back() == '/')
    throw std::invalid_argument("Repository history: api_base_url must be absolute and have no trailing slash");
  if (options.api_base_url.rfind("https://", 0) != 0 && options.api_base_url.rfind("http://", 0) != 0)
    throw std::invalid_argument("Repository history: api_base_url must use HTTP or HTTPS");
  if (!options.transport) throw std::invalid_argument("Repository history: transport is required");
  if (options.max_pages == 0 || options.max_pages > 10'000 ||
      options.max_issues == 0 || options.max_issues > 1'000'000 ||
      options.max_events == 0 || options.max_events > 10'000'000)
    throw std::invalid_argument("Repository history: invalid safety limit");

  EventLog log;
  log.attributes.push_back(attr("source", "repository_history"));
  const std::string repo_name = options.owner + "/" + options.repository;
  const std::string issue_prefix = options.api_base_url + "/repos/" + repo_name + "/issues/";
  std::size_t issue_count = 0;
  std::size_t event_count = 0;
  bool reached_end = false;
  for (std::size_t page = 1; page <= options.max_pages; ++page) {
    const auto url = options.api_base_url + "/repos/" + repo_name +
                     "/issues?state=all&per_page=100&page=" + std::to_string(page);
    const auto issues = array(JsonReader(fetch(options, url).body).read(), "issue page");
    if (issues.empty()) { reached_end = true; break; }
    for (const auto& issue_value : issues) {
      if (++issue_count > options.max_issues)
        throw std::invalid_argument("Repository history: max_issues exceeded");
      const auto& issue = object(issue_value, "issue");
      const auto timeline_url = text(issue, "timeline_url", false);
      if (timeline_url.empty()) continue;
      if (timeline_url.rfind(issue_prefix, 0) != 0)
        throw std::invalid_argument("Repository history: timeline URL is outside the configured repository");
      Trace trace;
      trace.case_id = timeline_url;
      trace.attributes = {
          attr("case:owner", options.owner), attr("case:repo", repo_name),
          attr("case:author_association", text(issue, "author_association")),
          attr("case:title", text(issue, "title"))};
      if (const auto* pull = member(issue, "pull_request"); pull && !std::holds_alternative<std::nullptr_t>(pull->value)) {
        const auto link = text(object(*pull, "pull_request"), "url", false);
        if (!link.empty()) trace.attributes.push_back(attr("case:pull_request", link));
      }
      auto append = [&](std::string activity, std::string timestamp,
                        std::string resource) {
        if (++event_count > options.max_events)
          throw std::invalid_argument("Repository history: max_events exceeded");
        Event event;
        event.case_id = trace.case_id;
        event.activity = std::move(activity);
        event.timestamp = std::move(timestamp);
        event.resource = std::move(resource);
        event.input_order = event_count - 1;
        trace.events.push_back(std::move(event));
      };
      append("created", text(issue, "created_at"), nested_text(issue, "user", "login"));
      const auto timeline = array(JsonReader(fetch(options, timeline_url).body).read(), "timeline");
      for (auto it = timeline.rbegin(); it != timeline.rend(); ++it) {
        const auto& entry = object(*it, "timeline event");
        const auto created = text(entry, "created_at", false);
        const auto activity = text(entry, "event", false);
        const auto actor = nested_text(entry, "actor", "login", false);
        if (!created.empty() && !activity.empty() && !actor.empty())
          append(activity, created, actor);
      }
      order_events_by_time(trace.events);
      log.traces.push_back(std::move(trace));
    }
  }
  if (!reached_end)
    throw std::invalid_argument("Repository history: max_pages exceeded before an empty page");
  std::stable_sort(log.traces.begin(), log.traces.end(), [](const Trace& left, const Trace& right) {
    const auto left_time = left.events.empty() ? std::string{} : left.events.front().timestamp;
    const auto right_time = right.events.empty() ? std::string{} : right.events.front().timestamp;
    return left_time == right_time ? left.case_id < right.case_id : left_time < right_time;
  });
  return log;
}

std::string repository_history_json(const EventLog& log, std::string_view owner,
                                    std::string_view repository) {
  std::ostringstream out;
  out << "{\"type\":\"REPOSITORY_HISTORY\",\"variant\":\"issues\",\"owner\":\""
      << escape_json(owner) << "\",\"repository\":\"" << escape_json(repository)
      << "\",\"trace_count\":" << log.trace_count() << ",\"event_count\":"
      << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":\"" << escape_json(trace.case_id) << "\",\"attributes\":{";
    for (std::size_t j = 0; j < trace.attributes.size(); ++j) {
      if (j) out << ',';
      out << '"' << escape_json(trace.attributes[j].key) << "\":\""
          << escape_json(trace.attributes[j].value) << '"';
    }
    out << "},\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j) out << ',';
      const auto& event = trace.events[j];
      out << "{\"activity\":\"" << escape_json(event.activity)
          << "\",\"timestamp\":\"" << escape_json(event.timestamp)
          << "\",\"resource\":\"" << escape_json(event.resource) << "\"}";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
