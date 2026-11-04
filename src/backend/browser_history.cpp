#include "minera/browser_history.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef MINERA_HAS_SQLITE
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#include "sqlite_untrusted.hpp"
#endif

namespace minera {
namespace {

constexpr std::size_t kMaxEvents = 10'000'000;

#ifdef MINERA_HAS_SQLITE
class Database {
 public:
  explicit Database(const std::filesystem::path& path) {
    // SQLite takes UTF-8 names on every platform; path.string() is the ANSI
    // code page on Windows and garbles (or throws on) non-ASCII profiles.
    const auto name = path.u8string();
    const auto status = sqlite3_open_v2(
        reinterpret_cast<const char*>(name.c_str()), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (status != SQLITE_OK) {
      const std::string message = db_ ? sqlite3_errmsg(db_) : "unknown error";
      if (db_) sqlite3_close(db_);
      db_ = nullptr;
      throw std::runtime_error("browser history: cannot open database: " +
                               message);
    }
    detail::harden_untrusted_sqlite(db_);
    sqlite3_busy_timeout(db_, 1000);
  }
  ~Database() { if (db_) sqlite3_close(db_); }
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  sqlite3* get() const { return db_; }
 private:
  sqlite3* db_ = nullptr;
};

class Statement {
 public:
  Statement(sqlite3* db, std::string_view sql) : db_(db) {
    const std::string text(sql);
    if (sqlite3_prepare_v2(db_, text.c_str(), -1, &statement_, nullptr) !=
        SQLITE_OK)
      throw std::runtime_error("browser history: invalid database schema: " +
                               std::string(sqlite3_errmsg(db_)));
  }
  ~Statement() { if (statement_) sqlite3_finalize(statement_); }
  sqlite3_stmt* get() const { return statement_; }
 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* statement_ = nullptr;
};

std::string text(sqlite3_stmt* statement, int column) {
  const auto* value = sqlite3_column_text(statement, column);
  return value ? reinterpret_cast<const char*>(value) : std::string{};
}

bool has_column(sqlite3* db, std::string_view table,
                std::string_view column) {
  Statement statement(db, "PRAGMA table_info(" + std::string(table) + ")");
  while (sqlite3_step(statement.get()) == SQLITE_ROW)
    if (text(statement.get(), 1) == column) return true;
  return false;
}
#endif

std::string json_escape(std::string_view value) {
  std::string out;
  for (const char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

std::string activity_from_url(std::string_view url) {
  auto start = url.find("://");
  start = start == std::string_view::npos ? 0 : start + 3;
  auto end = url.find('?', start);
  std::string result(url.substr(start, end == std::string_view::npos
                                           ? url.size() - start
                                           : end - start));
  result.erase(std::remove(result.begin(), result.end(), ','), result.end());
  return result;
}

std::string domain_from_url(std::string_view url) {
  auto value = activity_from_url(url);
  const auto slash = value.find('/');
  if (slash != std::string::npos) value.resize(slash);
  return value;
}

std::string iso_utc(std::int64_t unix_microseconds) {
  using namespace std::chrono;
  const sys_time<microseconds> point{microseconds{unix_microseconds}};
  const auto day = floor<days>(point);
  const year_month_day date{day};
  if (!date.ok())
    throw std::invalid_argument("browser history: timestamp is out of range");
  const hh_mm_ss clock{point - day};
  std::ostringstream out;
  out << static_cast<int>(date.year()) << '-';
  out.width(2); out.fill('0'); out << static_cast<unsigned>(date.month()) << '-';
  out.width(2); out << static_cast<unsigned>(date.day()) << 'T';
  out.width(2); out << clock.hours().count() << ':';
  out.width(2); out << clock.minutes().count() << ':';
  out.width(2); out << clock.seconds().count();
  const auto micros = duration_cast<microseconds>(clock.subseconds()).count();
  if (micros != 0) { out << '.'; out.width(6); out << micros; }
  out << 'Z';
  return out.str();
}

EventAttribute attr(std::string key, std::string value,
                    AttributeKind kind = AttributeKind::kString) {
  return {std::move(key), std::move(value), kind, false};
}

std::string chrome_transition(std::int64_t value) {
  static constexpr const char* names[] = {
      "LINK", "TYPED", "AUTO_BOOKMARK", "AUTO_SUBFRAME",
      "MANUAL_SUBFRAME", "GENERATED", "START_PAGE", "FORM_SUBMIT",
      "RELOAD", "KEYWORD", "KEYWORD_GENERATED"};
  const auto core = static_cast<unsigned>(value) & 0xffU;
  return core < std::size(names) ? names[core]
                                 : "UNKNOWN_" + std::to_string(core);
}

std::string firefox_transition(std::int64_t value) {
  static constexpr const char* names[] = {
      "LINK", "TYPED", "BOOKMARK", "EMBED", "REDIRECT_PERM",
      "REDIRECT_TEMP", "DOWNLOAD", "FRAMED_LINK", "RELOAD", "USER_TYPED"};
  return value >= 0 && static_cast<std::size_t>(value) < std::size(names)
             ? names[value]
             : "UNKNOWN";
}

std::vector<std::pair<std::filesystem::path, std::string>> databases(
    const BrowserHistoryOptions& options) {
  if (options.history_path.empty())
    throw std::invalid_argument("browser history: history_path is required");
  std::error_code error;
  if (std::filesystem::is_regular_file(options.history_path, error))
    return {{options.history_path, "DEFAULT"}};
  if (error || !std::filesystem::is_directory(options.history_path, error))
    throw std::invalid_argument("browser history: history_path does not exist");
  std::vector<std::pair<std::filesystem::path, std::string>> result;
  std::filesystem::directory_iterator iterator(options.history_path, error);
  const std::filesystem::directory_iterator end;
  while (!error && iterator != end) {
    const auto entry = *iterator;
    if (entry.is_directory(error)) {
      const auto candidate = entry.path() /
          (options.variant == BrowserHistoryVariant::kChrome ? "History"
                                                             : "places.sqlite");
      if (std::filesystem::is_regular_file(candidate, error))
        result.emplace_back(candidate, entry.path().filename().string());
    }
    iterator.increment(error);
  }
  if (error)
    throw std::runtime_error("browser history: cannot enumerate profile root");
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return left.second < right.second;
  });
  return result;
}

void append_event(EventLog& log, std::map<std::string, std::size_t>& traces,
                  Event event, std::size_t max_events) {
  std::size_t event_count = 0;
  for (const auto& trace : log.traces) event_count += trace.events.size();
  if (event_count >= max_events)
    throw std::invalid_argument("browser history: max_events exceeded");
  const auto case_id = event.case_id;
  const auto [it, inserted] = traces.emplace(case_id, log.traces.size());
  if (inserted) log.traces.push_back({case_id, {}});
  log.traces[it->second].events.push_back(std::move(event));
}

#ifdef MINERA_HAS_SQLITE
void read_chrome(const std::filesystem::path& path, const std::string& profile,
                 EventLog& log, std::map<std::string, std::size_t>& traces,
                 std::size_t max_events, std::size_t& ordinal) {
  Database database(path);
  const bool duration = has_column(database.get(), "visits", "visit_duration");
  const std::string query =
      "SELECT u.url,u.title,u.typed_count,u.visit_count,v.visit_time," +
      std::string(duration ? "v.visit_duration" : "0") +
      ",v.transition,v.from_visit,COALESCE(r.url,'') "
      "FROM visits v JOIN urls u ON v.url=u.id "
      "LEFT JOIN visits fv ON fv.id=v.from_visit "
      "LEFT JOIN urls r ON r.id=fv.url ORDER BY v.visit_time,v.id";
  Statement statement(database.get(), query);
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto url = text(statement.get(), 0);
    Event event;
    event.case_id = profile;
    event.activity = activity_from_url(url);
    constexpr std::int64_t epoch_delta_us = 11644473600LL * 1000000LL;
    event.timestamp = iso_utc(sqlite3_column_int64(statement.get(), 4) -
                              epoch_delta_us);
    event.input_order = ordinal++;
    event.attributes = {
        attr("complete_url", url), attr("domain", domain_from_url(url)),
        attr("url_wo_parameters", activity_from_url(url)),
        attr("title", text(statement.get(), 1)),
        attr("visit_duration", std::to_string(
             static_cast<double>(sqlite3_column_int64(statement.get(), 5)) /
             1000000.0), AttributeKind::kFloat),
        attr("transition", chrome_transition(
             sqlite3_column_int64(statement.get(), 6))),
        attr("typed_count", std::to_string(
             sqlite3_column_int64(statement.get(), 2)), AttributeKind::kInt),
        attr("visit_count", std::to_string(
             sqlite3_column_int64(statement.get(), 3)), AttributeKind::kInt),
        attr("referrer", text(statement.get(), 8))};
    if (!event.case_id.empty() && !event.activity.empty())
      append_event(log, traces, std::move(event), max_events);
  }
}

void read_firefox(const std::filesystem::path& path, const std::string& profile,
                  EventLog& log, std::map<std::string, std::size_t>& traces,
                  std::size_t max_events, std::size_t& ordinal) {
  Database database(path);
  Statement statement(database.get(),
      "SELECT p.url,v.visit_date,p.title,v.visit_type,v.session,v.from_visit,"
      "p.visit_count,p.typed,p.frecency,p.guid FROM moz_historyvisits v "
      "JOIN moz_places p ON v.place_id=p.id ORDER BY v.visit_date,v.id");
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto url = text(statement.get(), 0);
    Event event;
    event.case_id = profile;
    event.activity = activity_from_url(url);
    event.timestamp = iso_utc(sqlite3_column_int64(statement.get(), 1));
    event.input_order = ordinal++;
    event.attributes = {
        attr("complete_url", url), attr("domain", domain_from_url(url)),
        attr("url_wo_parameters", activity_from_url(url)),
        attr("title", text(statement.get(), 2)),
        attr("visit_type", std::to_string(
             sqlite3_column_int64(statement.get(), 3)), AttributeKind::kInt),
        attr("visit_type_desc", firefox_transition(
             sqlite3_column_int64(statement.get(), 3))),
        attr("session_id", std::to_string(
             sqlite3_column_int64(statement.get(), 4)), AttributeKind::kInt),
        attr("from_visit", std::to_string(
             sqlite3_column_int64(statement.get(), 5)), AttributeKind::kInt),
        attr("visit_count_place", std::to_string(
             sqlite3_column_int64(statement.get(), 6)), AttributeKind::kInt),
        attr("typed", sqlite3_column_int64(statement.get(), 7) ? "true" : "false",
             AttributeKind::kBoolean),
        attr("frecency", std::to_string(
             sqlite3_column_int64(statement.get(), 8)), AttributeKind::kInt),
        attr("guid", text(statement.get(), 9))};
    if (!event.case_id.empty() && !event.activity.empty())
      append_event(log, traces, std::move(event), max_events);
  }
}
#endif

}  // namespace

EventLog import_browser_history(const BrowserHistoryOptions& options) {
  if (options.variant != BrowserHistoryVariant::kChrome &&
      options.variant != BrowserHistoryVariant::kFirefox)
    throw std::invalid_argument("browser history: unsupported variant");
  if (options.max_events == 0 || options.max_events > kMaxEvents)
    throw std::invalid_argument("browser history: max_events must be in [1,10000000]");
#ifndef MINERA_HAS_SQLITE
  (void)options;
  throw std::runtime_error("browser history: SQLite support is unavailable");
#else
  EventLog log;
  log.attributes.push_back(attr("source", "browser_history"));
  std::map<std::string, std::size_t> traces;
  std::size_t ordinal = 0;
  for (const auto& [path, profile] : databases(options)) {
    if (options.variant == BrowserHistoryVariant::kChrome)
      read_chrome(path, profile, log, traces, options.max_events, ordinal);
    else
      read_firefox(path, profile, log, traces, options.max_events, ordinal);
  }
  for (auto& trace : log.traces) order_events_by_time(trace.events);
  return log;
#endif
}

std::string browser_history_json(const EventLog& log,
                                 BrowserHistoryVariant variant) {
  std::ostringstream out;
  out << "{\"type\":\"BROWSER_HISTORY\",\"variant\":\""
      << (variant == BrowserHistoryVariant::kChrome ? "chrome_history"
                                                    : "firefox_history")
      << "\",\"trace_count\":" << log.trace_count()
      << ",\"event_count\":" << log.event_count() << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":\"" << json_escape(trace.case_id)
        << "\",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j) out << ',';
      const auto& event = trace.events[j];
      out << "{\"activity\":\"" << json_escape(event.activity)
          << "\",\"timestamp\":\"" << json_escape(event.timestamp)
          << "\",\"attributes\":{";
      for (std::size_t k = 0; k < event.attributes.size(); ++k) {
        if (k) out << ',';
        out << '"' << json_escape(event.attributes[k].key) << "\":\""
            << json_escape(event.attributes[k].value) << '"';
      }
      out << "}}";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
