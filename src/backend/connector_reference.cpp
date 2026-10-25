#include "minera/connector_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <numeric>

#include "minera/filtering_common.hpp"

#ifdef MINERA_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace minera {

namespace {

// A Python value as the reference sees it.
struct Value {
  enum Kind { kNone, kInt, kFloat, kText, kBool } kind = kNone;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
  bool boolean = false;
  static Value none() { return {}; }
  static Value of(std::int64_t v) { Value out; out.kind = kInt; out.integer = v; return out; }
  static Value of_real(double v) { Value out; out.kind = kFloat; out.real = v; return out; }
  static Value of(std::string v) { Value out; out.kind = kText; out.text = std::move(v); return out; }
  static Value of_bool(bool v) { Value out; out.kind = kBool; out.boolean = v; return out; }
  [[nodiscard]] bool is_none() const { return kind == kNone; }
  // Python truthiness.
  [[nodiscard]] bool truthy() const {
    switch (kind) {
      case kNone: return false;
      case kInt: return integer != 0;
      case kFloat: return real != 0.0;
      case kText: return !text.empty();
      case kBool: return boolean;
    }
    return false;
  }
};

// One column as pandas infers its dtype from Python values.
void add_column(ConnectorReferenceFrame& frame, const std::string& name, const std::vector<Value>& values) {
  frame.columns.push_back(name);
  bool any = false, all_bool = true, all_text = true, all_int = true, all_number = true, has_none = false;
  for (const auto& v : values) {
    if (v.is_none()) {
      has_none = true;
      continue;
    }
    any = true;
    all_bool = all_bool && v.kind == Value::kBool;
    all_text = all_text && v.kind == Value::kText;
    all_int = all_int && v.kind == Value::kInt;
    all_number = all_number && (v.kind == Value::kInt || v.kind == Value::kFloat);
  }
  std::string dtype = "object";
  if (any) {
    if (all_bool && !has_none) dtype = "bool";
    else if (all_text) dtype = "str";
    else if (all_int && !has_none) dtype = "int64";
    else if (all_number) dtype = "float64";
  }
  frame.dtypes.push_back(dtype);
  if (frame.rows.size() < values.size()) frame.rows.resize(values.size());
  for (std::size_t k = 0; k < values.size(); ++k) {
    const auto& v = values[k];
    std::optional<std::string> cell;
    if (!v.is_none()) {
      if (dtype == "float64") cell = filtering_float_repr(v.kind == Value::kInt ? static_cast<double>(v.integer) : v.real);
      else if (v.kind == Value::kInt) cell = std::to_string(v.integer);
      else if (v.kind == Value::kFloat) cell = filtering_float_repr(v.real);
      else if (v.kind == Value::kBool) cell = v.boolean ? "True" : "False";
      else cell = v.text;
    }
    frame.rows[k].push_back(std::move(cell));
  }
}

// Days since 1970-01-01 to a civil date.
void civil(std::int64_t days, std::int64_t& year, unsigned& month, unsigned& day) {
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const auto doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  year = static_cast<std::int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year += month <= 2;
}

constexpr std::int64_t kMicrosPerDay = 86400LL * 1000000LL;
// 0001-01-01 and 10000-01-01 in microseconds since 1970-01-01.
constexpr std::int64_t kMinMicros = -719162LL * kMicrosPerDay;
constexpr std::int64_t kMaxMicros = 2932897LL * kMicrosPerDay;

// str() of a UTC pandas Timestamp at microsecond resolution.
std::string timestamp_text(std::int64_t micros) {
  auto days = micros / kMicrosPerDay;
  auto rest = micros % kMicrosPerDay;
  if (rest < 0) {
    rest += kMicrosPerDay;
    --days;
  }
  std::int64_t year;
  unsigned month, day;
  civil(days, year, month, day);
  const auto seconds = rest / 1000000;
  const auto fraction = rest % 1000000;
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%04lld-%02u-%02u %02lld:%02lld:%02lld", static_cast<long long>(year), month, day,
                static_cast<long long>(seconds / 3600), static_cast<long long>(seconds / 60 % 60),
                static_cast<long long>(seconds % 60));
  std::string out = buffer;
  if (fraction) {
    std::snprintf(buffer, sizeof buffer, ".%06lld", static_cast<long long>(fraction));
    out += buffer;
  }
  return out + "+00:00";
}

std::int64_t year_of(std::int64_t micros) {
  auto days = micros / kMicrosPerDay;
  if (micros % kMicrosPerDay < 0) --days;
  std::int64_t year;
  unsigned month, day;
  civil(days, year, month, day);
  return year;
}

// Python's int / int for an int64 numerator and a positive power of ten
// below 2^53: the correctly rounded quotient.
double true_divide(std::int64_t numerator, std::int64_t denominator) {
  constexpr std::int64_t exact = std::int64_t{1} << 53;
  if (numerator > -exact && numerator < exact) return static_cast<double>(numerator) / static_cast<double>(denominator);
  const double guess = static_cast<double>(static_cast<long double>(numerator) / static_cast<long double>(denominator));
  // Pick, among the guess and its neighbours, the double closest to the exact
  // quotient, ties to an even mantissa: compare |d * denominator - numerator|
  // exactly as d = mantissa * 2^exponent.
  auto error = [&](double d) {
    int exponent;
    const double fraction = std::frexp(d, &exponent);
    const auto mantissa = static_cast<__int128>(std::ldexp(fraction, 53));
    exponent -= 53;
    // d * denominator - numerator, scaled by 2^-exponent (exponent < 0 here).
    const __int128 scaled = static_cast<__int128>(numerator) << (-exponent);
    const __int128 diff = mantissa * denominator - scaled;
    return std::pair<__int128, bool>(diff < 0 ? -diff : diff, (mantissa & 1) == 0);
  };
  double best = guess;
  auto best_error = error(guess);
  for (const double candidate : {std::nextafter(guess, -INFINITY), std::nextafter(guess, INFINITY)}) {
    // Neighbours may change binade; compare their errors on a common scale.
    int e1, e2;
    std::frexp(candidate, &e1);
    std::frexp(best, &e2);
    if (e1 != e2) {
      const long double exact_q = static_cast<long double>(numerator) / static_cast<long double>(denominator);
      if (std::fabs(static_cast<long double>(candidate) - exact_q) < std::fabs(static_cast<long double>(best) - exact_q)) {
        best = candidate;
        best_error = error(candidate);
      }
      continue;
    }
    const auto e = error(candidate);
    if (e.first < best_error.first || (e.first == best_error.first && e.second && !best_error.second)) {
      best = candidate;
      best_error = e;
    }
  }
  return best;
}

double round_half_even(double x) {
  double rounded = std::round(x);
  if (std::fabs(x - rounded) == 0.5) rounded = 2.0 * std::round(x / 2.0);
  return rounded;
}

// datetime.fromtimestamp(t, timezone.utc) as microseconds since the epoch.
std::int64_t from_timestamp(double t) {
  double integral;
  double fraction = std::modf(t, &integral);
  fraction = round_half_even(fraction * 1e6);
  if (fraction >= 1e6) {
    fraction -= 1e6;
    integral += 1.0;
  } else if (fraction < 0) {
    fraction += 1e6;
    integral -= 1.0;
  }
  if (!(integral >= -9.2233720368547758e18 && integral < 9.2233720368547758e18))
    throw ConnectorReferenceError("OverflowError", "timestamp out of range for platform time_t");
  const auto seconds = static_cast<std::int64_t>(integral);
  constexpr std::int64_t kLimit = std::numeric_limits<std::int64_t>::max() / 1000000 - 1;
  if (seconds > kLimit || seconds < -kLimit)
    throw ConnectorReferenceError("ValueError", "year is out of range");
  const auto micros = seconds * 1000000 + static_cast<std::int64_t>(fraction);
  if (micros < kMinMicros || micros >= kMaxMicros)
    throw ConnectorReferenceError("ValueError", "year " + std::to_string(year_of(micros)) + " is out of range");
  return micros;
}

struct UrlParts {
  std::string activity, domain, without_parameters;
};

UrlParts split_url(const Value& url) {
  if (url.kind != Value::kText) throw ConnectorReferenceError("AttributeError", "'NoneType' object has no attribute 'split'");
  const auto& text = url.text;
  // str.split('//')[-1]: the matches are found left to right without overlap.
  std::size_t start = 0;
  for (auto at = text.find("//"); at != std::string::npos; at = text.find("//", at + 2)) start = at + 2;
  const std::string rest = text.substr(start);
  UrlParts out;
  out.without_parameters = rest.substr(0, rest.find('?'));
  for (const char c : out.without_parameters)
    if (c != ',') out.activity += c;
  out.domain = rest.substr(0, rest.find('/'));
  return out;
}

struct Visit {
  UrlParts parts;
  std::string url;
  std::int64_t micros = 0;
  std::vector<Value> extra;
};

ConnectorReferenceFrame browser_frame(const std::vector<Visit>& visits, const std::vector<std::string>& extra) {
  ConnectorReferenceFrame frame;
  if (visits.empty()) return frame;
  std::vector<std::size_t> order(visits.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) { return visits[a].micros < visits[b].micros; });
  frame.index = order;
  auto column = [&](const std::string& name, auto value) {
    std::vector<Value> values;
    for (const auto k : order) values.push_back(value(k));
    add_column(frame, name, values);
  };
  column("case:concept:name", [&](std::size_t) { return Value::of(std::string("DEFAULT")); });
  column("concept:name", [&](std::size_t k) { return Value::of(visits[k].parts.activity); });
  column("complete_url", [&](std::size_t k) { return Value::of(visits[k].url); });
  column("domain", [&](std::size_t k) { return Value::of(visits[k].parts.domain); });
  column("url_wo_parameters", [&](std::size_t k) { return Value::of(visits[k].parts.without_parameters); });
  frame.columns.push_back("time:timestamp");
  frame.dtypes.push_back("datetime64[us, UTC]");
  frame.rows.resize(order.size());
  for (std::size_t r = 0; r < order.size(); ++r) frame.rows[r].push_back(timestamp_text(visits[order[r]].micros));
  for (std::size_t c = 0; c < extra.size(); ++c) column(extra[c], [&](std::size_t k) { return visits[k].extra[c]; });
  column("@@index", [&](std::size_t k) { return Value::of(static_cast<std::int64_t>(k)); });
  column("@@case_index", [&](std::size_t) { return Value::of(std::int64_t{0}); });
  return frame;
}

#ifdef MINERA_HAS_SQLITE
struct Database {
  sqlite3* handle = nullptr;
  Database(const std::string& name, int flags) {
    if (sqlite3_open_v2(name.c_str(), &handle, flags, nullptr) != SQLITE_OK) {
      const std::string message = handle ? sqlite3_errmsg(handle) : "unable to open database file";
      sqlite3_close(handle);
      handle = nullptr;
      throw ConnectorReferenceError("OperationalError", message);
    }
  }
  ~Database() { sqlite3_close(handle); }
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  std::vector<std::vector<Value>> query(const std::string& sql) const {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
      sqlite3_finalize(statement);
      throw ConnectorReferenceError("OperationalError", sqlite3_errmsg(handle));
    }
    std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)> guard(statement, sqlite3_finalize);
    std::vector<std::vector<Value>> rows;
    for (;;) {
      const int status = sqlite3_step(statement);
      if (status == SQLITE_DONE) break;
      if (status != SQLITE_ROW) throw ConnectorReferenceError("OperationalError", sqlite3_errmsg(handle));
      std::vector<Value> row;
      for (int c = 0; c < sqlite3_column_count(statement); ++c) {
        switch (sqlite3_column_type(statement, c)) {
          case SQLITE_INTEGER: row.push_back(Value::of(static_cast<std::int64_t>(sqlite3_column_int64(statement, c)))); break;
          case SQLITE_FLOAT: row.push_back(Value::of_real(sqlite3_column_double(statement, c))); break;
          case SQLITE_NULL: row.push_back(Value::none()); break;
          default: {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, c));
            row.push_back(Value::of(std::string(text, static_cast<std::size_t>(sqlite3_column_bytes(statement, c)))));
          }
        }
      }
      rows.push_back(std::move(row));
    }
    return rows;
  }
};

std::string uri_of(const std::filesystem::path& path) { return "file:" + path.string() + "?mode=ro"; }
#endif

const char* const kChromeTransitions[] = {"LINK",  "TYPED",      "AUTO_BOOKMARK", "AUTO_SUBFRAME", "MANUAL_SUBFRAME", "GENERATED",
                                          "START_PAGE", "FORM_SUBMIT", "RELOAD", "KEYWORD", "KEYWORD_GENERATED"};
const char* const kFirefoxVisitTypes[] = {"LINK",          "TYPED",    "BOOKMARK",    "EMBED",  "REDIRECT_PERM",
                                          "REDIRECT_TEMP", "DOWNLOAD", "FRAMED_LINK", "RELOAD", "USER_TYPED"};

}  // namespace

ConnectorReferenceFrame reference_chrome_history(const std::filesystem::path& database) {
#ifndef MINERA_HAS_SQLITE
  (void)database;
  throw std::runtime_error("chrome history: SQLite support is unavailable");
#else
  Database db(uri_of(database), SQLITE_OPEN_READONLY | SQLITE_OPEN_URI);
  bool duration = false;
  for (const auto& row : db.query("PRAGMA table_info(visits)"))
    duration = duration || (row.size() > 1 && row[1].kind == Value::kText && row[1].text == "visit_duration");
  const auto rows = db.query(std::string(R"(
            SELECT
                urls.url AS full_url,
                urls.title AS title,
                urls.typed_count AS typed_cnt,
                urls.visit_count AS visit_cnt,
                visits.visit_time AS visit_time,
                )") + (duration ? "visits.visit_duration AS duration" : "0 AS duration") + R"(,
                visits.transition AS transition,
                visits.from_visit AS from_visit_id
            FROM visits
            JOIN urls ON visits.url = urls.id
        )");
  std::map<std::int64_t, Value> urls;
  for (auto& row : db.query("SELECT id, url FROM urls"))
    if (row[0].kind == Value::kInt) urls[row[0].integer] = row[1];
  std::vector<Visit> visits;
  constexpr std::int64_t kEpoch1601 = 11644473600LL * 1000000LL;
  for (const auto& row : rows) {
    const auto& time = row[4];
    if (time.kind != Value::kInt)
      throw ConnectorReferenceError("TypeError", "unsupported type for timedelta microseconds component: NoneType");
    if (time.integer < kMinMicros + kEpoch1601 || time.integer - kEpoch1601 >= kMaxMicros)
      throw ConnectorReferenceError("OverflowError", "date value out of range");
    Visit visit;
    visit.micros = time.integer - kEpoch1601;
    visit.parts = split_url(row[0]);
    visit.url = row[0].text;
    if (row[6].kind != Value::kInt)
      throw ConnectorReferenceError("TypeError", "unsupported operand type(s) for &: 'NoneType' and 'int'");
    const auto core = static_cast<std::uint64_t>(row[6].integer) & 0xffU;
    const std::string transition = core < std::size(kChromeTransitions) ? kChromeTransitions[core] : "UNKNOWN_" + std::to_string(core);
    if (visit.parts.activity.empty()) continue;
    const auto& from = row[7];
    Value referrer = Value::of(std::string());
    if (from.kind == Value::kInt) {
      const auto found = urls.find(from.integer);
      if (found != urls.end()) referrer = found->second;
    }
    const double seconds = row[5].truthy() ? true_divide(row[5].integer, 1000000) : 0.0;
    visit.extra = {row[1].truthy() ? row[1] : Value::of(std::string()), Value::of_real(seconds), Value::of(transition), row[2], row[3], referrer};
    visits.push_back(std::move(visit));
  }
  return browser_frame(visits, {"title", "visit_duration", "transition", "typed_count", "visit_count", "referrer"});
#endif
}

ConnectorReferenceFrame reference_firefox_history(const std::filesystem::path& database) {
#ifndef MINERA_HAS_SQLITE
  (void)database;
  throw std::runtime_error("firefox history: SQLite support is unavailable");
#else
  Database db(database.string(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  const auto rows = db.query(
      "SELECT p.url, v.visit_date, p.title, v.visit_type, v.session, v.from_visit, p.visit_count, p.typed, p.frecency, "
      "p.guid FROM moz_historyvisits v JOIN moz_places p ON v.place_id = p.id");
  std::vector<Visit> visits;
  for (const auto& row : rows) {
    const auto& date = row[1];
    if (date.kind != Value::kInt) throw ConnectorReferenceError("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'int'");
    Visit visit;
    visit.micros = from_timestamp(true_divide(date.integer, 1000000));
    visit.parts = split_url(row[0]);
    visit.url = row[0].text;
    if (visit.parts.activity.empty()) continue;
    const auto& type = row[3];
    const std::string description =
        type.kind == Value::kInt && type.integer >= 0 && type.integer < static_cast<std::int64_t>(std::size(kFirefoxVisitTypes))
            ? kFirefoxVisitTypes[type.integer]
            : "UNKNOWN";
    visit.extra = {row[2], type, Value::of(description), row[4], row[5], row[6], Value::of_bool(row[7].truthy()), row[8], row[9]};
    visits.push_back(std::move(visit));
  }
  return browser_frame(visits, {"title", "visit_type", "visit_type_desc", "session_id", "from_visit", "visit_count_place",
                                "typed", "frecency", "guid"});
#endif
}

}  // namespace minera
