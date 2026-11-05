#pragma once

// Shared helpers for the OCEL writers that reproduce what the reference
// writers emit from their pandas frames: the column dtype a frame built from
// the OCEL's typed values would get, Python's float and str reprs, and the
// text pandas prints for a UTC timestamp.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "minera/filtering_common.hpp"
#include "minera/ocel.hpp"
#include "minera/timestamp.hpp"

namespace minera::ocel_frame {

// The dtype pandas gives a column of a frame built from records: an integer
// column with a missing cell becomes float64, a boolean one object.
enum class Dtype { kEmpty, kInt64, kFloat64, kBool, kObject };

struct ColumnStats {
  bool has_bool = false;
  bool has_int = false;
  bool has_float = false;
  bool has_string = false;
  std::size_t present = 0;

  void observe(const OcelValue& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) return;
    ++present;
    if (std::holds_alternative<bool>(value)) has_bool = true;
    else if (std::holds_alternative<std::int64_t>(value)) has_int = true;
    else if (std::holds_alternative<double>(value)) has_float = true;
    else has_string = true;
  }
  void merge(const ColumnStats& other) {
    has_bool = has_bool || other.has_bool;
    has_int = has_int || other.has_int;
    has_float = has_float || other.has_float;
    has_string = has_string || other.has_string;
    present += other.present;
  }

  [[nodiscard]] Dtype dtype(std::size_t rows) const {
    if (present == 0) return Dtype::kEmpty;
    const bool missing = present < rows;
    if (has_string) return Dtype::kObject;
    if (has_bool)
      return has_int || has_float || missing ? Dtype::kObject : Dtype::kBool;
    if (has_float || missing) return Dtype::kFloat64;
    return Dtype::kInt64;
  }
};

// Columns in first-appearance order with their value statistics.
class Columns {
 public:
  void observe(const std::string& key, const OcelValue& value) {
    auto [slot, added] = index_.try_emplace(key, names_.size());
    if (added) {
      names_.push_back(key);
      stats_.emplace_back();
    }
    stats_[slot->second].observe(value);
  }
  void observe_row(const std::vector<OcelAttribute>& attributes) {
    ++rows_;
    for (const auto& attribute : attributes) observe(attribute.key, attribute.value);
  }
  void add_rows(std::size_t rows) { rows_ += rows; }

  [[nodiscard]] const std::vector<std::string>& names() const { return names_; }
  [[nodiscard]] std::size_t rows() const { return rows_; }
  [[nodiscard]] const ColumnStats* stats(const std::string& key) const {
    const auto found = index_.find(key);
    return found == index_.end() ? nullptr : &stats_[found->second];
  }
  [[nodiscard]] Dtype dtype(const std::string& key) const {
    const auto* found = stats(key);
    return found == nullptr ? Dtype::kEmpty : found->dtype(rows_);
  }

 private:
  std::vector<std::string> names_;
  std::vector<ColumnStats> stats_;
  std::unordered_map<std::string, std::size_t> index_;
  std::size_t rows_ = 0;
};

// pandas to_sql's column type: from the dtype, or for an object column from
// the kinds of its non-null values in the written table.
inline std::string sql_type(Dtype dtype, const ColumnStats& table_values) {
  switch (dtype) {
    case Dtype::kInt64:
    case Dtype::kBool:
      return "INTEGER";
    case Dtype::kFloat64:
      return "REAL";
    default:
      break;
  }
  if (table_values.has_string) return "TEXT";
  if (table_values.has_bool)
    return table_values.has_int || table_values.has_float ? "TEXT" : "INTEGER";
  if (table_values.has_float) return "REAL";
  if (table_values.has_int) return "INTEGER";
  return "TEXT";
}

// Python's repr of a float; "nan", "inf" and "-inf" when not finite.
inline std::string float_repr(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
  return filtering_float_repr(value);
}

// Python's str of a scalar: True/False, the int, the float repr, the text.
inline std::string python_str(const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return "None";
  if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value)) return float_repr(*real);
  return std::get<std::string>(value);
}

// Python's repr of a str: single quotes unless the text holds a single quote
// and no double quote; backslash, the quote and control characters escaped.
// Bytes from 0x80 up are kept (non-ASCII text is printable).
inline std::string python_str_repr(std::string_view text) {
  const bool double_quote = text.find('\'') != std::string_view::npos &&
                            text.find('"') == std::string_view::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const char ch : text) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '\\') out += "\\\\";
    else if (ch == quote) { out += '\\'; out += ch; }
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else if (byte < 0x20 || byte == 0x7f) {
      static constexpr char digits[] = "0123456789abcdef";
      out += "\\x";
      out += digits[byte >> 4];
      out += digits[byte & 15];
    } else {
      out += ch;
    }
  }
  out += quote;
  return out;
}

// A parsed timestamp's civil date and time of day (UTC for an aware one,
// the wall clock for a naive one); nullopt outside years 1..9999.
struct CivilTimestamp {
  long long year, month, day, hour, minute, second;
  std::uint32_t nanoseconds;
  bool timezone_aware;
};

inline std::optional<CivilTimestamp> civil_timestamp(const ParsedExactTimestamp& parsed) {
  std::int64_t days = parsed.seconds / 86400;
  std::int64_t rest = parsed.seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = yoe + era * 400;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) ++year;
  if (year < 1 || year > 9999) return std::nullopt;
  return CivilTimestamp{static_cast<long long>(year), static_cast<long long>(month),
                        static_cast<long long>(day), static_cast<long long>(rest / 3600),
                        static_cast<long long>(rest / 60 % 60), static_cast<long long>(rest % 60),
                        parsed.nanoseconds, parsed.timezone_aware};
}

// "Y-MM-DD<separator>HH:MM:SS" plus `digits` fraction digits (0, 3, 6 or 9),
// the year zero-padded to four digits when `pad_year`.
inline std::string civil_timestamp_text(const CivilTimestamp& time, char separator, int digits,
                                        bool pad_year) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, pad_year ? "%04lld-%02lld-%02lld%c%02lld:%02lld:%02lld"
                                                : "%lld-%02lld-%02lld%c%02lld:%02lld:%02lld",
                time.year, time.month, time.day, separator, time.hour, time.minute, time.second);
  std::string out(buffer);
  if (digits > 0) {
    unsigned fraction = time.nanoseconds;
    for (int drop = 9 - digits; drop > 0; --drop) fraction /= 10;
    std::snprintf(buffer, sizeof buffer, ".%0*u", digits, fraction);
    out += buffer;
  }
  return out;
}

// The text pandas prints for a timestamp the reference holds as UTC:
// "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00" (separator ' ', suffix "+00:00"), or
// with separator 'T' and suffix "Z" for the OCEL 1.0 XML date. A naive text
// gets no suffix; text that is no timestamp is returned as is.
inline std::string utc_timestamp_text(std::string_view text, char separator,
                                      std::string_view suffix) {
  const auto parsed = parse_timestamp_exact(text);
  const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
  if (!time) return std::string(text);
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (time->timezone_aware) out += suffix;
  return out;
}

// The texts DataFrame.to_csv writes for a timestamp column holding `texts`.
// A column of naive timestamps is datetime64 and formatted as a whole: dates
// alone ("2024-01-01", the year unpadded) when every value is a midnight,
// otherwise "Y-MM-DD HH:MM:SS" with as many fraction digits as the most
// precise value needs (3, 6 or 9) on every value. A column of aware
// timestamps is written value by value as str() of each: in the offset the
// values share (pm4py.read_ocel_csv keeps a fixed "+02:00"; the other native
// importers store UTC), and in UTC when they do not share one. A column
// mixing kinds or holding text that is no timestamp is written value by value
// too, as utc_timestamp_text with suffix "+00:00" (observed black-box on
// pm4py.write_ocel_csv).
inline std::vector<std::string> pandas_timestamp_column_text(
    const std::vector<std::string_view>& texts) {
  std::vector<CivilTimestamp> times;
  times.reserve(texts.size());
  bool naive_column = true, aware_column = true;
  std::optional<std::int32_t> offset;  // minutes, when every aware value shares it
  bool one_offset = true;
  std::vector<ParsedExactTimestamp> instants;
  instants.reserve(texts.size());
  for (const auto text : texts) {
    const auto parsed = parse_timestamp_exact(text);
    const auto time = parsed ? civil_timestamp(*parsed) : std::nullopt;
    if (!time) {
      naive_column = aware_column = false;
      break;
    }
    (time->timezone_aware ? naive_column : aware_column) = false;
    if (time->timezone_aware) {
      if (offset && *offset != parsed->offset_minutes) one_offset = false;
      offset = parsed->offset_minutes;
    }
    times.push_back(*time);
    instants.push_back(*parsed);
  }
  std::vector<std::string> out;
  out.reserve(texts.size());
  if (aware_column && one_offset && offset && *offset != 0) {
    char suffix[16];
    const auto minutes = *offset < 0 ? -*offset : *offset;
    std::snprintf(suffix, sizeof suffix, "%c%02d:%02d", *offset < 0 ? '-' : '+',
                  static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    for (std::size_t index = 0; index < texts.size(); ++index) {
      auto local = instants[index];
      local.seconds += static_cast<std::int64_t>(*offset) * 60;
      const auto time = civil_timestamp(local);
      if (!time) {
        out.push_back(utc_timestamp_text(texts[index], ' ', "+00:00"));
        continue;
      }
      const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
      out.push_back(civil_timestamp_text(*time, ' ', digits, true) + suffix);
    }
    return out;
  }
  if (!naive_column || texts.empty()) {
    for (const auto text : texts) out.push_back(utc_timestamp_text(text, ' ', "+00:00"));
    return out;
  }
  bool dates_only = true;
  int digits = 0;
  for (const auto& time : times) {
    if (time.hour != 0 || time.minute != 0 || time.second != 0 || time.nanoseconds != 0)
      dates_only = false;
    if (time.nanoseconds % 1000 != 0) digits = 9;
    else if (time.nanoseconds % 1000000 != 0) digits = std::max(digits, 6);
    else if (time.nanoseconds != 0) digits = std::max(digits, 3);
  }
  for (const auto& time : times) {
    if (dates_only) {
      char buffer[32];
      std::snprintf(buffer, sizeof buffer, "%lld-%02lld-%02lld", time.year, time.month, time.day);
      out.emplace_back(buffer);
    } else {
      out.push_back(civil_timestamp_text(time, ' ', digits, false));
    }
  }
  return out;
}

// The offset (minutes) that every value of an aware timestamp column shares,
// when it is not zero. pm4py.read_ocel_csv keeps such a column with its
// fixed offset (datetime64[us, UTC+02:00]; mixed offsets raise there), and
// the reference writers print each value's wall clock in it (observed
// black-box: OCEL 2.0 XML "2024-01-01T10:00:00+02:00", OCEL 1.0 XML
// "2024-01-01T10:00:00Z", SQLite "2024-01-01 10:00:00+02:00"). The other
// native importers store UTC, which gives nullopt.
inline std::optional<std::int32_t> shared_nonzero_offset(
    const std::vector<std::string_view>& texts) {
  std::optional<std::int32_t> offset;
  for (const auto text : texts) {
    const auto parsed = parse_timestamp_exact(text);
    if (!parsed || !parsed->timezone_aware) return std::nullopt;
    if (offset && *offset != parsed->offset_minutes) return std::nullopt;
    offset = parsed->offset_minutes;
  }
  if (!offset || *offset == 0) return std::nullopt;
  return offset;
}

// `text` as utc_timestamp_text prints it, but in the wall clock of
// `offset_minutes` (nullopt: UTC) and followed by `suffix`, or by the offset
// itself ("+02:00") when `suffix` is nullopt.
inline std::string offset_timestamp_text(std::string_view text, char separator,
                                         std::optional<std::int32_t> offset_minutes,
                                         std::optional<std::string_view> suffix) {
  if (!offset_minutes) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"));
  auto parsed = parse_timestamp_exact(text);
  if (!parsed || !parsed->timezone_aware)
    return utc_timestamp_text(text, separator, suffix.value_or("+00:00"));
  parsed->seconds += static_cast<std::int64_t>(*offset_minutes) * 60;
  const auto time = civil_timestamp(*parsed);
  if (!time) return utc_timestamp_text(text, separator, suffix.value_or("+00:00"));
  const int digits = time->nanoseconds == 0 ? 0 : time->nanoseconds % 1000 == 0 ? 6 : 9;
  auto out = civil_timestamp_text(*time, separator, digits, true);
  if (suffix) return out.append(*suffix);
  char buffer[16];
  const auto minutes = *offset_minutes < 0 ? -*offset_minutes : *offset_minutes;
  std::snprintf(buffer, sizeof buffer, "%c%02d:%02d", *offset_minutes < 0 ? '-' : '+',
                static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
  return out.append(buffer);
}

// Whether a catalog declares `key` of `owner` a date ("date", "time",
// "timestamp", ... in any case), as the reference OCEL 2.0 readers convert.
inline bool declared_date(const OcelTypeCatalog& catalog, const std::string& owner,
                          const std::string& key) {
  const auto schema = catalog.find(owner);
  if (schema == catalog.end()) return false;
  const auto found = schema->second.find(key);
  if (found == schema->second.end()) return false;
  std::string lowered = found->second;
  for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos;
}

// A date attribute's string value as a timestamp text, or nullptr when the
// value is no string or the text is no timestamp (kept as written).
inline const std::string* date_value_text(const OcelValue& value) {
  const auto* text = std::get_if<std::string>(&value);
  if (text == nullptr || !parse_timestamp_exact(*text)) return nullptr;
  return text;
}

}  // namespace minera::ocel_frame
