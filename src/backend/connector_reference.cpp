#include "minera/connector_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <regex>

#include "database_history_queries.hpp"
#include "minera/dataframe_format.hpp"
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

// pandas.read_sql of a DB-API result: one column per name, typed from its
// Python values.
EventTable read_sql(const ConnectorReferenceResultSet& result) {
  EventTable table;
  table.columns = result.names;
  table.rows.assign(result.rows.size(), {});
  for (std::size_t c = 0; c < result.names.size(); ++c) {
    bool any = false, text = true, integer = true, number = true, none = false;
    for (const auto& row : result.rows) {
      const auto& cell = row.at(c);
      if (std::holds_alternative<std::monostate>(cell)) {
        none = true;
        continue;
      }
      any = true;
      text = text && std::holds_alternative<std::string>(cell);
      integer = integer && std::holds_alternative<std::int64_t>(cell);
      number = number && !std::holds_alternative<std::string>(cell);
    }
    std::string dtype = "object";
    if (any && text) dtype = "str";
    else if (any && integer && !none) dtype = "int64";
    else if (any && number) dtype = "float64";
    table.dtypes.push_back(dtype);
    const auto& key = result.names[c];
    for (std::size_t r = 0; r < result.rows.size(); ++r) {
      const auto& cell = result.rows[r][c];
      EventAttribute attribute{key, "", AttributeKind::kString};
      if (const auto* v = std::get_if<std::string>(&cell)) {
        attribute.value = *v;
      } else if (const auto* i = std::get_if<std::int64_t>(&cell)) {
        if (dtype == "float64") {
          attribute.kind = AttributeKind::kFloat;
          attribute.value = filtering_float_repr(static_cast<double>(*i));
          attribute.typed_value = static_cast<double>(*i);
        } else {
          attribute.kind = AttributeKind::kInt;
          attribute.value = std::to_string(*i);
          attribute.typed_value = *i;
        }
      } else if (const auto* d = std::get_if<double>(&cell)) {
        attribute.kind = AttributeKind::kFloat;
        attribute.value = filtering_float_repr(*d);
        attribute.typed_value = *d;
      } else if (dtype == "float64") {
        attribute.kind = AttributeKind::kFloat;
        attribute.value = "nan";
        attribute.typed_value = std::numeric_limits<double>::quiet_NaN();
      } else {
        attribute.is_null = true;
        attribute.first_class_state = FirstClassAttributeState::kNull;
      }
      table.rows[r].push_back(std::move(attribute));
    }
  }
  table.categorical_dtypes.assign(table.columns.size(), std::nullopt);
  return table;
}

std::optional<std::string> printed(const std::optional<EventAttribute>& cell, const std::string& dtype) {
  if (!cell || cell->is_null) return std::nullopt;
  if (dtype.rfind("datetime64", 0) == 0) {
    auto text = cell->value;
    if (text.empty() || text == "NaT") return std::nullopt;
    if (text.size() > 10 && text[10] == 'T') text[10] = ' ';
    return text;
  }
  if (dtype == "float64" && (cell->value == "nan" || cell->value == "NaN")) return std::nullopt;
  return cell->value;
}

ConnectorReferenceFrame database_frame(const ConnectorReferenceConnection& connection, const std::string& sql) {
  const auto result = connection.execute(sql);
  auto table = read_sql(result);
  if (connection.close) connection.close();
  const auto formatted = format_dataframe(table);
  ConnectorReferenceFrame frame;
  frame.columns = formatted.columns;
  frame.dtypes = formatted.dtypes;
  for (std::size_t r = 0; r < formatted.rows.size(); ++r) {
    frame.index.push_back(r);
    std::vector<std::optional<std::string>> row;
    for (std::size_t c = 0; c < formatted.columns.size(); ++c)
      row.push_back(printed(c < formatted.rows[r].size() ? formatted.rows[r][c] : std::nullopt, formatted.dtypes[c]));
    frame.rows.push_back(std::move(row));
  }
  return frame;
}


// The extraction stops at the first of these; the rows read so far are kept.
struct GithubStop {};

const ConnectorReferenceJson& member(const ConnectorReferenceJson& object, const std::string& key) {
  const auto* found = object.find(key);
  if (!found) throw GithubStop{};  // KeyError, or TypeError on a value that is no dict
  return *found;
}

Value value_of(const ConnectorReferenceJson& json) {
  switch (json.kind) {
    case ConnectorReferenceJson::kNull: return Value::none();
    case ConnectorReferenceJson::kBool: return Value::of_bool(json.boolean);
    case ConnectorReferenceJson::kInt: return Value::of(json.integer);
    case ConnectorReferenceJson::kFloat: return Value::of_real(json.real);
    case ConnectorReferenceJson::kText: return Value::of(json.text);
    default: throw GithubStop{};
  }
}

std::int64_t days_from_civil(std::int64_t year, unsigned month, unsigned day) {
  year -= month <= 2;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const auto yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// The ISO 8601 forms of a GitHub date, the offset dropped: microseconds of
// the wall clock read as UTC.
std::int64_t github_date(const ConnectorReferenceJson& json) {
  if (json.kind != ConnectorReferenceJson::kText) throw GithubStop{};
  const std::string& s = json.text;
  std::size_t at = 0;
  auto digits = [&](std::size_t count) {
    if (at + count > s.size()) throw GithubStop{};
    int out = 0;
    for (std::size_t k = 0; k < count; ++k, ++at) {
      if (s[at] < '0' || s[at] > '9') throw GithubStop{};
      out = out * 10 + (s[at] - '0');
    }
    return out;
  };
  auto literal = [&](char c) {
    if (at >= s.size() || s[at] != c) throw GithubStop{};
    ++at;
  };
  const int year = digits(4);
  literal('-');
  const int month = digits(2);
  literal('-');
  const int day = digits(2);
  int hour = 0, minute = 0, second = 0, micros = 0;
  if (at < s.size()) {
    if (s[at] != 'T' && s[at] != ' ') throw GithubStop{};
    ++at;
    hour = digits(2);
    literal(':');
    minute = digits(2);
    if (at < s.size() && s[at] == ':') {
      ++at;
      second = digits(2);
      if (at < s.size() && s[at] == '.') {
        ++at;
        const auto start = at;
        while (at < s.size() && s[at] >= '0' && s[at] <= '9') ++at;
        if (at == start) throw GithubStop{};
        std::string fraction = s.substr(start, std::min<std::size_t>(at - start, 6));
        fraction.resize(6, '0');
        micros = std::stoi(fraction);
      }
    }
    if (at < s.size()) {
      if (s[at] == 'Z') {
        ++at;
      } else if (s[at] == '+' || s[at] == '-') {
        ++at;
        digits(2);
        if (at < s.size() && s[at] == ':') ++at;
        digits(2);
      } else {
        throw GithubStop{};
      }
    }
    if (at != s.size()) throw GithubStop{};
  }
  static constexpr unsigned kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (year < 1 || month < 1 || month > 12 || day < 1 ||
      static_cast<unsigned>(day) > kDays[month - 1] + (month == 2 && leap) || hour > 23 || minute > 59 || second > 59)
    throw GithubStop{};
  return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * kMicrosPerDay +
         ((hour * 60LL + minute) * 60LL + second) * 1000000LL + micros;
}

// pm4py.format_dataframe's order: cases by their first row (timestamp, then
// row number), rows by case, timestamp and row number. Returns the row order
// and each row's case rank.
std::pair<std::vector<std::size_t>, std::vector<std::size_t>> format_order(const std::vector<std::string>& cases,
                                                                           const std::vector<std::int64_t>& micros) {
  std::vector<std::size_t> order(cases.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return micros[a] < micros[b]; });
  std::map<std::string, std::size_t> rank;
  for (const auto k : order) rank.emplace(cases[k], rank.size());
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) { return rank.at(cases[a]) < rank.at(cases[b]); });
  std::vector<std::size_t> ranks(cases.size());
  for (std::size_t k = 0; k < cases.size(); ++k) ranks[k] = rank.at(cases[k]);
  return {order, ranks};
}

struct GithubRow {
  std::string case_id;
  std::int64_t micros = 0;
  Value activity, resource, association, title;
  std::optional<Value> pull_request;
};

ConnectorReferenceFrame github_frame(const std::string& owner, const std::string& repository,
                                     const std::vector<GithubRow>& rows) {
  ConnectorReferenceFrame frame;
  if (rows.empty()) return frame;
  std::vector<std::string> cases;
  std::vector<std::int64_t> times;
  for (const auto& row : rows) {
    cases.push_back(row.case_id);
    times.push_back(row.micros);
  }
  const auto [order, case_rank] = format_order(cases, times);
  frame.index = order;
  auto column = [&](const std::string& name, auto value) {
    std::vector<Value> values;
    for (const auto k : order) values.push_back(value(rows[k], k));
    add_column(frame, name, values);
  };
  column("case:owner", [&](const GithubRow&, std::size_t) { return Value::of(owner); });
  column("case:repo", [&](const GithubRow&, std::size_t) { return Value::of(owner + "/" + repository); });
  column("case:concept:name", [](const GithubRow& r, std::size_t) { return Value::of(r.case_id); });
  frame.columns.push_back("time:timestamp");
  frame.dtypes.push_back("datetime64[us, UTC]");
  for (std::size_t r = 0; r < order.size(); ++r) frame.rows[r].push_back(timestamp_text(rows[order[r]].micros));
  column("concept:name", [](const GithubRow& r, std::size_t) { return r.activity; });
  column("org:resource", [](const GithubRow& r, std::size_t) { return r.resource; });
  column("case:author_association", [](const GithubRow& r, std::size_t) { return r.association; });
  column("case:title", [](const GithubRow& r, std::size_t) { return r.title; });
  if (std::any_of(rows.begin(), rows.end(), [](const GithubRow& r) { return r.pull_request.has_value(); }))
    column("case:pull_request",
           [](const GithubRow& r, std::size_t) { return r.pull_request ? *r.pull_request : Value::none(); });
  column("@@index", [](const GithubRow&, std::size_t k) { return Value::of(static_cast<std::int64_t>(k)); });
  column("@@case_index",
         [&](const GithubRow&, std::size_t k) { return Value::of(static_cast<std::int64_t>(case_rank[k])); });
  return frame;
}
// The COM connectors drop an item (or a row) at the first of these.
struct ComSkip {};

const ConnectorReferenceCom& attribute(const ConnectorReferenceCom& object, const std::string& name) {
  const auto* found = object.find(name);
  if (!found) throw ComSkip{};
  return *found;
}

// str() of a COM value.
std::string python_str(const ConnectorReferenceCom& value) {
  switch (value.kind) {
    case ConnectorReferenceCom::kNone: return "None";
    case ConnectorReferenceCom::kBool: return value.boolean ? "True" : "False";
    case ConnectorReferenceCom::kInt: return std::to_string(value.integer);
    case ConnectorReferenceCom::kFloat: return filtering_float_repr(value.real);
    case ConnectorReferenceCom::kText: return value.text;
    case ConnectorReferenceCom::kTime: {
      auto out = timestamp_text(value.micros);
      out.resize(out.size() - 6);
      if (value.offset_minutes) {
        const int offset = *value.offset_minutes, magnitude = offset < 0 ? -offset : offset;
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", offset < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
        out += buffer;
      }
      return out;
    }
    default: throw ComSkip{};
  }
}

// A datetime as UTC microseconds: an aware one converted, a naive one as is.
std::int64_t com_time(const ConnectorReferenceCom& value) {
  if (value.kind != ConnectorReferenceCom::kTime) throw ComSkip{};
  return value.micros - (value.offset_minutes ? *value.offset_minutes * 60000000LL : 0);
}

// datetime plus timedelta(minutes=m), OverflowError outside years 1..9999.
std::int64_t plus_minutes(std::int64_t micros, const ConnectorReferenceCom& minutes, bool int_only) {
  double delta = 0.0;
  if (minutes.kind == ConnectorReferenceCom::kInt && (minutes.integer > 1000000000000LL || minutes.integer < -1000000000000LL))
    throw ComSkip{};  // OverflowError
  if (minutes.kind == ConnectorReferenceCom::kInt) delta = static_cast<double>(minutes.integer) * 60e6;
  else if (minutes.kind == ConnectorReferenceCom::kBool) delta = minutes.boolean ? 60e6 : 0.0;
  else if (minutes.kind == ConnectorReferenceCom::kFloat && !int_only) delta = round_half_even(minutes.real * 60e6);
  else throw ComSkip{};
  const double out = static_cast<double>(micros) + delta;
  if (!(out >= static_cast<double>(kMinMicros) && out < static_cast<double>(kMaxMicros))) throw ComSkip{};
  if (minutes.kind == ConnectorReferenceCom::kInt) return micros + minutes.integer * 60000000LL;
  return micros + static_cast<std::int64_t>(delta);
}

bool truthy(const ConnectorReferenceCom& value) {
  switch (value.kind) {
    case ConnectorReferenceCom::kNone: return false;
    case ConnectorReferenceCom::kBool: return value.boolean;
    case ConnectorReferenceCom::kInt: return value.integer != 0;
    case ConnectorReferenceCom::kFloat: return value.real != 0.0;
    case ConnectorReferenceCom::kText: return !value.text.empty();
    case ConnectorReferenceCom::kList: return !value.items.empty();
    default: return true;
  }
}

bool equals_five(const ConnectorReferenceCom& value) {
  return (value.kind == ConnectorReferenceCom::kInt && value.integer == 5) ||
         (value.kind == ConnectorReferenceCom::kFloat && value.real == 5.0);
}

// One output row of a COM connector: its case, timestamp and text columns.
struct ComRow {
  std::string case_id;
  std::int64_t micros = 0;
  std::vector<std::string> texts;
};

ConnectorReferenceFrame com_frame(const std::vector<ComRow>& rows, const std::vector<std::string>& columns,
                                  std::size_t timestamp_at) {
  if (rows.empty()) throw ConnectorReferenceError("KeyError", "'time:timestamp'");
  std::vector<std::string> cases;
  std::vector<std::int64_t> times;
  for (const auto& row : rows) {
    cases.push_back(row.case_id);
    times.push_back(row.micros);
  }
  const auto [order, ranks] = format_order(cases, times);
  ConnectorReferenceFrame frame;
  frame.index = order;
  frame.rows.resize(order.size());
  for (std::size_t c = 0, text = 0; c <= columns.size(); ++c) {
    if (c == timestamp_at) {
      frame.columns.push_back("time:timestamp");
      frame.dtypes.push_back("datetime64[us, UTC]");
      for (std::size_t r = 0; r < order.size(); ++r) frame.rows[r].push_back(timestamp_text(rows[order[r]].micros));
    }
    if (c == columns.size()) break;
    std::vector<Value> values;
    for (const auto k : order) values.push_back(Value::of(rows[k].texts[text]));
    add_column(frame, columns[c], values);
    ++text;
  }
  std::vector<Value> index, case_index;
  for (const auto k : order) {
    index.push_back(Value::of(static_cast<std::int64_t>(k)));
    case_index.push_back(Value::of(static_cast<std::int64_t>(ranks[k])));
  }
  add_column(frame, "@@index", index);
  add_column(frame, "@@case_index", case_index);
  return frame;
}

// time.strptime(text, "%Y%m%d%H%M%S.%f") as naive microseconds.
std::int64_t wmi_time(const std::string& text) {
  static const std::regex pattern(
      R"((\d\d\d\d)(1[0-2]|0[1-9]|[1-9])(3[01]|[12]\d|0[1-9]|[1-9]| [1-9])(2[0-3]|[0-1]\d|\d)([0-5]\d|\d)(6[0-1]|[0-5]\d|\d)\.(\d{1,6}))");
  std::smatch match;
  if (!std::regex_search(text, match, pattern, std::regex_constants::match_continuous))
    throw ConnectorReferenceError("ValueError", "time data '" + text + "' does not match format '%Y%m%d%H%M%S.%f'");
  if (static_cast<std::size_t>(match.length(0)) != text.size())
    throw ConnectorReferenceError("ValueError", "unconverted data remains: " + text.substr(match.length(0)));
  auto number = [&](int group) { return std::stoi(match.str(group)); };
  const int year = number(1), month = number(2), day = number(3), second = number(6);
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (year < 1 || day > kDays[month - 1] + (month == 2 && leap))
    throw ConnectorReferenceError("ValueError", "day is out of range for month");
  if (second > 59) throw ConnectorReferenceError("ValueError", "second must be in 0..59");
  auto fraction = match.str(7);
  fraction.resize(6, '0');
  return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * kMicrosPerDay +
         ((number(4) * 60LL + number(5)) * 60LL + second) * 1000000LL + std::stoll(fraction);
}

}  // namespace

std::string reference_database_connector_query(const std::string& connector, const std::string& prefix) {
  std::string query;
  if (connector == "camunda_workflow") return database_history_detail::camunda_workflow_query;
  if (connector == "sap_accounting") query = database_history_detail::sap_accounting_query;
  else if (connector == "sap_o2c") query = database_history_detail::sap_o2c_query;
  else throw std::invalid_argument("unknown database connector: " + connector);
  const std::string owned = "OWNED.";
  for (auto at = query.find(owned); at != std::string::npos; at = query.find(owned, at + prefix.size())) query.replace(at, owned.size(), prefix);
  return query;
}

ConnectorReferenceFrame reference_camunda_workflow(const ConnectorReferenceConnection& connection) {
  return database_frame(connection, reference_database_connector_query("camunda_workflow"));
}

ConnectorReferenceFrame reference_sap_accounting(const ConnectorReferenceConnection& connection, const std::string& prefix) {
  return database_frame(connection, reference_database_connector_query("sap_accounting", prefix));
}

ConnectorReferenceFrame reference_sap_o2c(const ConnectorReferenceConnection& connection, const std::string& prefix) {
  return database_frame(connection, reference_database_connector_query("sap_o2c", prefix));
}

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

const ConnectorReferenceJson* ConnectorReferenceJson::find(const std::string& key) const {
  if (kind != kObject) return nullptr;
  for (const auto& [name, value] : members)
    if (name == key) return &value;
  return nullptr;
}

ConnectorReferenceFrame reference_github_repo(const std::string& owner, const std::string& repository,
                                              const std::optional<std::string>& token,
                                              const ConnectorReferenceGet& get) {
  std::map<std::string, std::string> headers;
  if (token) headers["Authorization"] = "Bearer " + *token;
  std::vector<GithubRow> rows;
  try {
    for (std::size_t page = 1;; ++page) {
      const auto issues = get("https://api.github.com/repos/" + owner + "/" + repository +
                                  "/issues?state=all&per_page=100&page=" + std::to_string(page),
                              headers);
      if (issues.kind == ConnectorReferenceJson::kNull) break;
      if (issues.kind != ConnectorReferenceJson::kArray) throw GithubStop{};
      if (issues.items.empty()) break;
      for (const auto& issue : issues.items) {
        if (issue.kind != ConnectorReferenceJson::kObject) throw GithubStop{};
        const auto* timeline_url = issue.find("timeline_url");
        if (!timeline_url) continue;
        GithubRow created;
        if (timeline_url->kind != ConnectorReferenceJson::kText) throw GithubStop{};
        created.case_id = timeline_url->text;
        created.micros = github_date(member(issue, "created_at"));
        created.activity = Value::of(std::string("created"));
        created.resource = value_of(member(member(issue, "user"), "login"));
        created.association = value_of(member(issue, "author_association"));
        created.title = value_of(member(issue, "title"));
        if (issue.find("pull_request")) created.pull_request = value_of(member(member(issue, "pull_request"), "url"));
        rows.push_back(created);
        const auto timeline = get(created.case_id, headers);
        if (timeline.kind != ConnectorReferenceJson::kArray) throw GithubStop{};
        for (auto event = timeline.items.rbegin(); event != timeline.items.rend(); ++event) {
          if (event->kind != ConnectorReferenceJson::kObject) throw GithubStop{};
          if (!event->find("created_at") || !event->find("event") || !event->find("actor")) continue;
          GithubRow row = created;
          row.micros = github_date(member(*event, "created_at"));
          row.activity = value_of(member(*event, "event"));
          row.resource = value_of(member(member(*event, "actor"), "login"));
          rows.push_back(row);
        }
      }
    }
  } catch (const GithubStop&) {
  }
  return github_frame(owner, repository, rows);
}

const ConnectorReferenceCom* ConnectorReferenceCom::find(const std::string& name) const {
  if (kind != kObject) return nullptr;
  for (const auto& [key, value] : members)
    if (key == name) return &value;
  return nullptr;
}

ConnectorReferenceFrame reference_outlook_calendar(const std::vector<ConnectorReferenceCom>& calendar,
                                                  bool include_reminders) {
  std::vector<ComRow> rows;
  for (const auto& item : calendar) {
    try {
      const auto id = python_str(attribute(item, "ConversationID"));
      const auto subject = python_str(attribute(item, "Subject"));
      const auto created = com_time(attribute(item, "CreationTime"));
      const auto changed = com_time(attribute(item, "LastModificationTime"));
      const auto start = com_time(attribute(item, "Start"));
      const auto end = plus_minutes(start, attribute(item, "Duration"), false);
      auto add = [&](std::int64_t micros, const char* activity) { rows.push_back({id, micros, {id, subject, activity}}); };
      add(created, "Meeting Created");
      add(changed, "Last Change of Meeting");
      add(start, "Meeting Started");
      add(end, "Meeting Completed");
      try {
        if (equals_five(attribute(item, "MeetingStatus"))) add(changed, "Meeting Cancelled");
      } catch (const ComSkip&) {
      }
      try {
        if (include_reminders && truthy(attribute(item, "ReminderSet"))) {
          const auto& minutes = attribute(item, "ReminderMinutesBeforeStart");
          ConnectorReferenceCom negated = minutes;
          if (minutes.kind == ConnectorReferenceCom::kInt) negated.integer = -minutes.integer;
          else if (minutes.kind == ConnectorReferenceCom::kBool) negated = {ConnectorReferenceCom::kInt, false, -minutes.boolean};
          add(plus_minutes(start, negated, true), "Meeting Reminder Fired");
        }
      } catch (const ComSkip&) {
      }
    } catch (const ComSkip&) {
    }
  }
  return com_frame(rows, {"case:concept:name", "case:subject", "concept:name"}, 2);
}

ConnectorReferenceFrame reference_outlook_mail(const std::vector<ConnectorReferenceCom>& sent,
                                               const std::vector<ConnectorReferenceCom>& inbox) {
  std::vector<ComRow> rows;
  for (const auto& [items, direction] : {std::pair{&sent, "Sent "}, std::pair{&inbox, "Received "}}) {
    for (const auto& item : *items) {
      try {
        const auto kind = python_str(attribute(item, "Class"));
        if (kind != "43" && kind != "46") continue;
        const auto subject = python_str(attribute(item, "Subject"));
        const auto created = com_time(attribute(item, "CreationTime"));
        std::string sender = "EMPTY", recipients = "EMPTY";
        try {
          sender = python_str(attribute(attribute(item, "Sender"), "Name"));
        } catch (const ComSkip&) {
        }
        try {
          const auto& list = attribute(item, "Recipients");
          if (list.kind == ConnectorReferenceCom::kText && list.text.empty()) {
            recipients.clear();
          } else {
            if (list.kind != ConnectorReferenceCom::kList) throw ComSkip{};
            std::string joined;
            for (std::size_t k = 0; k < list.items.size(); ++k)
              joined += (k ? " AND " : "") + python_str(attribute(list.items[k], "Name"));
            recipients = joined;
          }
        } catch (const ComSkip&) {
        }
        const auto id = python_str(attribute(item, "ConversationID"));
        const auto topic = python_str(attribute(item, "ConversationTopic"));
        const std::string activity = direction + std::string(kind == "43" ? "Mail" : "Delivery Report");
        rows.push_back({id, created, {id, activity, sender, recipients, topic, subject}});
      } catch (const ComSkip&) {
      }
    }
  }
  return com_frame(rows, {"case:concept:name", "concept:name", "org:resource", "recipients", "topic", "subject"}, 2);
}

std::vector<std::string> reference_mail_connect_calls(const std::optional<std::string>& email_user, int mailbox_id) {
  std::vector<std::string> calls = {"CoInitialize()", "Dispatch('Outlook.Application')", "GetNamespace('MAPI')"};
  const auto id = std::to_string(mailbox_id);
  if (!email_user) {
    calls.push_back("GetDefaultFolder(" + id + ")");
    return calls;
  }
  // repr() of the user name.
  const bool double_quotes = email_user->find('\'') != std::string::npos && email_user->find('"') == std::string::npos;
  const char quote = double_quotes ? '"' : '\'';
  std::string text(1, quote);
  for (const char c : *email_user) {
    if (c == quote || c == '\\') text += '\\';
    text += c;
  }
  text += quote;
  const auto recipient = "CreateRecipient(" + text + ")";
  calls.push_back(recipient);
  calls.push_back("Resolve()");
  calls.push_back("GetSharedDefaultFolder(" + recipient + ", " + id + ")");
  return calls;
}

ConnectorReferenceFrame reference_windows_events(const std::vector<ConnectorReferenceCom>& rows) {
  static const char* const kProperties[] = {"Category",   "CategoryString", "ComputerName", "EventCode", "EventIdentifier",
                                            "EventType",  "LogFile",        "Message",      "RecordNumber", "SourceName",
                                            "TimeGenerated", "TimeWritten", "Type",         "User"};
  struct Event {
    std::vector<std::string> texts;
    std::int64_t generated = 0, written = 0;
  };
  std::vector<Event> events;
  for (const auto& row : rows) {
    Event event;
    for (const char* name : kProperties) {
      const auto* value = row.find(name);
      if (!value) throw ConnectorReferenceError("KeyError", std::string("'") + name + "'");
      std::string text;
      try {
        text = python_str(*value);
      } catch (const ComSkip&) {
        throw ConnectorReferenceError("TypeError", std::string("property ") + name + " has no str()");
      }
      const std::string name_text = name;
      if (name_text == "TimeGenerated" || name_text == "TimeWritten") {
        const auto cut = text.substr(0, text.find_first_of("+-"));
        (name_text == "TimeGenerated" ? event.generated : event.written) = wmi_time(cut);
      }
      event.texts.push_back(std::move(text));
    }
    events.push_back(std::move(event));
  }
  if (events.empty()) throw ConnectorReferenceError("KeyError", "'computerName'");
  std::vector<std::string> cases;
  std::vector<std::int64_t> times;
  for (const auto& event : events) {
    cases.push_back(event.texts[2]);
    times.push_back(event.generated);
  }
  const auto [order, ranks] = format_order(cases, times);
  ConnectorReferenceFrame frame;
  frame.index = order;
  auto naive = [](std::int64_t micros) {
    auto text = timestamp_text(micros);
    text.resize(text.size() - 6);
    return text;
  };
  auto text_column = [&](const std::string& name, auto value) {
    std::vector<Value> values;
    for (const auto k : order) values.push_back(Value::of(value(events[k])));
    add_column(frame, name, values);
  };
  auto time_column = [&](const std::string& name, auto value) {
    frame.columns.push_back(name);
    frame.dtypes.push_back("datetime64[us]");
    for (std::size_t r = 0; r < order.size(); ++r) frame.rows[r].push_back(naive(value(events[order[r]])));
  };
  static const char* const kColumns[] = {"category",  "categoryString", "computerName", "eventCode",    "eventIdentifier",
                                         "eventType", "logFile",        "message",      "recordNumber", "sourceName"};
  for (std::size_t c = 0; c < 10; ++c) text_column(kColumns[c], [c](const Event& e) { return e.texts[c]; });
  time_column("timeGenerated", [](const Event& e) { return e.generated; });
  time_column("timeWritten", [](const Event& e) { return e.written; });
  text_column("type", [](const Event& e) { return e.texts[12]; });
  text_column("user", [](const Event& e) { return e.texts[13]; });
  text_column("case:concept:name", [](const Event& e) { return e.texts[2]; });
  time_column("time:timestamp", [](const Event& e) { return e.generated; });
  text_column("concept:name", [](const Event& e) { return e.texts[9] + " " + e.texts[4]; });
  text_column("org:resource", [](const Event& e) { return e.texts[13]; });
  std::vector<Value> index, case_index;
  for (const auto k : order) {
    index.push_back(Value::of(static_cast<std::int64_t>(k)));
    case_index.push_back(Value::of(static_cast<std::int64_t>(ranks[k])));
  }
  add_column(frame, "@@index", index);
  add_column(frame, "@@case_index", case_index);
  return frame;
}

}  // namespace minera
