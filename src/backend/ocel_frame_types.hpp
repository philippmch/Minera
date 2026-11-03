#pragma once

// Shared helpers for the OCEL writers that reproduce what the reference
// writers emit from their pandas frames: the column dtype a frame built from
// the OCEL's typed values would get, Python's float and str reprs, and the
// text pandas prints for a UTC timestamp.

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

// The text pandas prints for a timestamp the reference holds as UTC:
// "YYYY-MM-DD HH:MM:SS[.ffffff]+00:00" (separator ' ', suffix "+00:00"), or
// with separator 'T' and suffix "Z" for the OCEL 1.0 XML date. A naive text
// gets no suffix; text that is no timestamp is returned as is.
inline std::string utc_timestamp_text(std::string_view text, char separator,
                                      std::string_view suffix) {
  const auto parsed = parse_timestamp_exact(text);
  if (!parsed) return std::string(text);
  std::int64_t seconds = parsed->seconds;
  std::int64_t days = seconds / 86400;
  std::int64_t rest = seconds % 86400;
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
  if (year < 1 || year > 9999) return std::string(text);
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%04lld-%02lld-%02lld%c%02lld:%02lld:%02lld",
                static_cast<long long>(year), static_cast<long long>(month),
                static_cast<long long>(day), separator,
                static_cast<long long>(rest / 3600),
                static_cast<long long>(rest / 60 % 60),
                static_cast<long long>(rest % 60));
  std::string out(buffer);
  if (parsed->nanoseconds != 0) {
    if (parsed->nanoseconds % 1000 == 0)
      std::snprintf(buffer, sizeof buffer, ".%06u", parsed->nanoseconds / 1000);
    else
      std::snprintf(buffer, sizeof buffer, ".%09u", parsed->nanoseconds);
    out += buffer;
  }
  if (parsed->timezone_aware) out += suffix;
  return out;
}

}  // namespace minera::ocel_frame
