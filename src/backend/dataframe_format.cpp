#include "minera/dataframe_format.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/timestamp.hpp"
#include "full_double.hpp"

namespace minera {
namespace {

constexpr std::string_view kCaseColumn = "case:concept:name";
constexpr std::string_view kActivityColumn = "concept:name";
constexpr std::string_view kTimestampColumn = "time:timestamp";
constexpr std::string_view kStartTimestampColumn = "start_timestamp";

struct ExactTimestamp {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
};

bool timestamp_less(const ExactTimestamp& left, const ExactTimestamp& right) {
  if (left.seconds != right.seconds) return left.seconds < right.seconds;
  return left.nanoseconds < right.nanoseconds;
}

// None, pd.NA, NaN and NaT are missing; a present empty string is a value.
bool cell_missing(const std::optional<EventAttribute>& cell) {
  if (!cell || cell->is_null || is_native_na_attribute(*cell) ||
      cell->first_class_state == FirstClassAttributeState::kNull)
    return true;
  if (const auto* number = std::get_if<double>(&cell->typed_value))
    return std::isnan(*number);
  if (cell->kind == AttributeKind::kFloat &&
      (cell->value == "nan" || cell->value == "NaN" ||
       cell->value == "NAN"))
    return true;
  return false;
}

std::string attribute_text(const EventAttribute& cell) {
  if (cell.is_null ||
      cell.first_class_state == FirstClassAttributeState::kNull)
    return {};
  if (const auto* value = std::get_if<NativeDate>(&cell.typed_value))
    return value->value;
  if (const auto* value = std::get_if<bool>(&cell.typed_value))
    return *value ? "True" : "False";
  if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value)) {
    return std::to_string(*value);
  }
  if (const auto* value = std::get_if<std::uint64_t>(&cell.typed_value)) {
    return std::to_string(*value);
  }
  if (const auto* value = std::get_if<double>(&cell.typed_value)) {
    char buffer[64];
    const auto converted = std::to_chars(
        buffer, buffer + sizeof(buffer), *value, std::chars_format::general);
    auto text = converted.ec == std::errc{}
                    ? std::string(buffer, converted.ptr) : std::to_string(*value);
    if (std::isfinite(*value) && text.find_first_of(".eE") == std::string::npos)
      text += ".0";
    return text;
  }
  return cell.value;
}

EventAttribute null_cell(std::string key,
                         AttributeKind kind = AttributeKind::kString) {
  EventAttribute result{std::move(key), "", kind, true};
  result.first_class_state = FirstClassAttributeState::kNull;
  result.typed_value = std::monostate{};
  return result;
}

EventAttribute copy_cell(const std::optional<EventAttribute>& source,
                         std::string key) {
  if (!source) return null_cell(std::move(key));
  auto result = *source;
  result.key = std::move(key);
  return result;
}

EventAttribute string_cell(std::string key,
                           const std::optional<EventAttribute>& source) {
  if (cell_missing(source)) return null_cell(std::move(key));
  EventAttribute result{std::move(key), attribute_text(*source),
                        AttributeKind::kString};
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.typed_value = std::monostate{};
  result.container = AttributeContainerKind::kScalar;
  result.children.clear();
  result.scalar_wrapper = false;
  result.scalar_children_list = false;
  return result;
}

std::string trim_ascii(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::int64_t days_from_civil(std::int64_t year, unsigned month,
                             unsigned day) {
  year -= month <= 2;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const auto year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 -
                              year_of_era / 100 + day_of_year;
  return era * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

void civil_from_days(std::int64_t days, std::int64_t& year, unsigned& month,
                     unsigned& day) {
  days += 719468;
  const std::int64_t era =
      (days >= 0 ? days : days - 146096) / 146097;
  const unsigned day_of_era = static_cast<unsigned>(days - era * 146097);
  const unsigned year_of_era =
      (day_of_era - day_of_era / 1460 + day_of_era / 36524 -
       day_of_era / 146096) /
      365;
  year = static_cast<std::int64_t>(year_of_era) + era * 400;
  const unsigned day_of_year =
      day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const unsigned month_prime = (5 * day_of_year + 2) / 153;
  day = day_of_year - (153 * month_prime + 2) / 5 + 1;
  month = month_prime < 10 ? month_prime + 3 : month_prime - 9;
  year += month <= 2;
}

// The compact shapes YYYY, YYYYMMDD and YYYYMMDD HHMMSS, which the public
// mixed-format parser also accepts, in ISO spelling.
std::string expand_compact_timestamp(std::string text) {
  auto digits = [&](std::size_t from, std::size_t count) {
    for (std::size_t k = from; k < from + count; ++k)
      if (k >= text.size() || text[k] < '0' || text[k] > '9') return false;
    return true;
  };
  if (text.size() == 4 && digits(0, 4)) return text + "-01-01";
  if (text.size() != 8 && text.size() != 15) return text;
  if (!digits(0, 8)) return text;
  if (text.size() == 15 && (text[8] != ' ' || !digits(9, 6))) return text;
  std::string out = text.substr(0, 4) + "-" + text.substr(4, 2) + "-" + text.substr(6, 2);
  if (text.size() == 15)
    out += " " + text.substr(9, 2) + ":" + text.substr(11, 2) + ":" + text.substr(13, 2);
  return out;
}

std::optional<ExactTimestamp> parse_iso_timestamp(std::string text) {
  text = expand_compact_timestamp(trim_ascii(std::move(text)));
  if (text.empty() || !parse_timestamp(text)) return std::nullopt;

  std::int64_t nanoseconds = 0;
  std::size_t digits = 0;
  if (const auto dot = text.find('.'); dot != std::string::npos) {
    std::size_t end = dot + 1;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
      if (digits < 9) nanoseconds = nanoseconds * 10 + text[end] - '0';
      ++digits;
      ++end;
    }
    while (digits < 9) {
      nanoseconds *= 10;
      ++digits;
    }
    text.erase(dot, end - dot);
  }
  const auto whole_seconds = parse_timestamp(text);
  if (!whole_seconds || !std::isfinite(*whole_seconds)) return std::nullopt;
  return ExactTimestamp{static_cast<std::int64_t>(*whole_seconds),
                        static_cast<std::uint32_t>(nanoseconds)};
}

std::optional<ExactTimestamp> parse_strptime_timestamp(std::string text,
                                                        std::string_view format) {
  text = trim_ascii(std::move(text));
  // The public parser accepts empty fractions and hour-only offsets for
  // ISO-shaped formats, but requires digits/minutes for other formats.
  bool iso_shape = false;
  for (const auto separator : {"-", "/", ".", " "}) {
    for (const auto clock_separator : {" ", "T"}) {
      const auto prefix = std::string("%Y") + separator + "%m" + separator +
                          "%d" + clock_separator + "%H:%M:%S";
      if (format == prefix || format == prefix + ".%f" ||
          format == prefix + "%z" || format == prefix + ".%f%z")
        iso_shape = true;
    }
  }
  std::tm calendar{};
  std::size_t position = 0;
  std::uint32_t nanoseconds = 0;
  int offset_minutes = 0;
  auto digit = [&]() {
    return position < text.size() && text[position] >= '0' &&
           text[position] <= '9';
  };
  auto two_digits = [&]() -> std::optional<int> {
    if (position + 2 > text.size() || !digit() ||
        text[position + 1] < '0' || text[position + 1] > '9')
      return std::nullopt;
    const int value = (text[position] - '0') * 10 + text[position + 1] - '0';
    position += 2;
    return value;
  };
  auto standard = [&](std::string_view fragment) {
    if (fragment.empty()) return true;
    std::istringstream stream(text.substr(position));
    stream.imbue(std::locale::classic());
    stream >> std::get_time(&calendar, std::string(fragment).c_str());
    if (stream.fail()) return false;
    if (stream.eof()) position = text.size();
    else position += static_cast<std::size_t>(stream.tellg());
    return true;
  };
  std::size_t segment = 0;
  for (std::size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%' || i + 1 == format.size()) continue;
    const char directive = format[++i];
    if (directive != 'f' && directive != 'z') continue;
    if (!standard(format.substr(segment, i - 1 - segment)))
      return std::nullopt;
    if (directive == 'f') {
      std::size_t digits = 0;
      nanoseconds = 0;
      while (digit()) {
        if (digits < 9) nanoseconds = nanoseconds * 10 + text[position] - '0';
        ++position;
        ++digits;
      }
      if (!digits && !iso_shape) return std::nullopt;
      while (digits++ < 9) nanoseconds *= 10;
    } else {
      if (position == text.size()) return std::nullopt;
      if (text[position] == 'Z') {
        ++position;
        offset_minutes = 0;
      } else {
        const char sign = text[position++];
        if (sign != '+' && sign != '-') return std::nullopt;
        const auto hours = two_digits();
        if (!hours || *hours > 23) return std::nullopt;
        const bool colon = position < text.size() && text[position] == ':';
        if (colon) ++position;
        std::optional<int> minutes;
        if (digit()) minutes = two_digits();
        else if (iso_shape && !colon) minutes = 0;
        if (!minutes || *minutes > 59) return std::nullopt;
        if ((position < text.size() && text[position] == ':') || digit()) {
          if (colon) {
            if (text[position++] != ':') return std::nullopt;
          } else if (!digit()) return std::nullopt;
          const auto seconds = two_digits();
          if (!seconds) return std::nullopt;
          // Reference UTC normalization discards offset seconds/fractions.
          if (position < text.size() && text[position] == '.') {
            ++position;
            while (digit()) ++position;
          }
        }
        offset_minutes = (*hours * 60 + *minutes) * (sign == '-' ? -1 : 1);
      }
    }
    segment = i + 1;
  }
  if (!standard(format.substr(segment)) ||
      !trim_ascii(text.substr(position)).empty()) return std::nullopt;
  const auto year = static_cast<std::int64_t>(calendar.tm_year) + 1900;
  const auto month = static_cast<unsigned>(calendar.tm_mon + 1);
  const auto day = static_cast<unsigned>(calendar.tm_mday);
  const auto hour = static_cast<unsigned>(calendar.tm_hour);
  const auto minute = static_cast<unsigned>(calendar.tm_min);
  const auto second = static_cast<unsigned>(calendar.tm_sec);
  if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour > 23 || minute > 59 || second > 59)
    return std::nullopt;
  constexpr unsigned month_lengths[] = {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
  const bool leap_year = year % 4 == 0 &&
                         (year % 100 != 0 || year % 400 == 0);
  const auto maximum_day = month_lengths[month - 1] +
                           (month == 2 && leap_year ? 1u : 0u);
  if (day > maximum_day) return std::nullopt;
  return ExactTimestamp{days_from_civil(year, month, day) * 86400 +
                             static_cast<std::int64_t>(hour * 3600 +
                                                       minute * 60 + second) -
                             static_cast<std::int64_t>(offset_minutes) * 60,
                         nanoseconds};
}

std::string format_exact_timestamp(const ExactTimestamp& timestamp,
                                   std::string_view precision) {
  std::int64_t days = timestamp.seconds / 86400;
  std::int64_t second_of_day = timestamp.seconds % 86400;
  if (second_of_day < 0) {
    second_of_day += 86400;
    --days;
  }
  std::int64_t year = 0;
  unsigned month = 0;
  unsigned day = 0;
  civil_from_days(days, year, month, day);
  std::ostringstream output;
  output << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2)
         << month << '-' << std::setw(2) << day << 'T' << std::setw(2)
         << second_of_day / 3600 << ':' << std::setw(2)
         << (second_of_day / 60) % 60 << ':' << std::setw(2)
         << second_of_day % 60;
  const std::uint32_t nanos = timestamp.nanoseconds;
  if (precision == "ns") {
    if (nanos % 1000 != 0) output << '.' << std::setw(9) << nanos;
    else if (nanos != 0) output << '.' << std::setw(6) << nanos / 1000;
  } else if (precision == "us") {
    const auto micros = nanos / 1000;
    if (micros != 0) output << '.' << std::setw(6) << micros;
  }
  output << "+00:00";
  return output.str();
}

std::optional<ExactTimestamp> parse_timestamp_cell(
    const EventAttribute& cell, const std::optional<std::string>& format) {
  if (cell.is_null ||
      cell.first_class_state == FirstClassAttributeState::kNull)
    return std::nullopt;
  if (const auto* date = std::get_if<NativeDate>(&cell.typed_value)) {
    return parse_iso_timestamp(date->value);
  }
  const auto text = attribute_text(cell);
  if (!format || *format == "mixed" || *format == "ISO8601")
    return parse_iso_timestamp(text);
  return parse_strptime_timestamp(text, *format);
}

bool is_numeric_dtype(std::string_view dtype) {
  if (dtype == "bool" || dtype == "boolean") return true;
  if (dtype.find("int") != std::string_view::npos ||
      dtype.find("float") != std::string_view::npos ||
      dtype.find("UInt") != std::string_view::npos ||
      dtype.find("Int") != std::string_view::npos ||
      dtype.find("Float") != std::string_view::npos)
    return true;
  return false;
}

bool numeric_kind(const EventAttribute& cell) {
  return cell.kind == AttributeKind::kInt ||
         cell.kind == AttributeKind::kFloat ||
         cell.kind == AttributeKind::kBoolean ||
         std::holds_alternative<std::int64_t>(cell.typed_value) ||
         std::holds_alternative<std::uint64_t>(cell.typed_value) ||
         std::holds_alternative<double>(cell.typed_value) ||
         std::holds_alternative<bool>(cell.typed_value);
}

std::string source_dtype(const EventTable& table, std::size_t column) {
  if (!table.dtypes.empty()) return table.dtypes[column];
  bool has_value = false;
  bool all_bool = true;
  bool all_int = true;
  bool all_float = true;
  bool all_date = true;
  for (const auto& row : table.rows) {
    if (column >= row.size() || cell_missing(row[column])) continue;
    has_value = true;
    const auto& cell = *row[column];
    all_bool = all_bool && cell.kind == AttributeKind::kBoolean;
    all_int = all_int && cell.kind == AttributeKind::kInt;
    all_float = all_float && cell.kind == AttributeKind::kFloat;
    all_date = all_date && cell.kind == AttributeKind::kDate;
  }
  if (all_bool && has_value) return "bool";
  if (all_int && has_value) return "int64";
  if (all_float && has_value) return "float64";
  if (all_date && has_value) {
    // An object-free table can still carry the source timestamp unit on each
    // native date cell. Preserve that unit when inferring the dataframe dtype;
    // whole-second nanosecond timestamps otherwise look indistinguishable
    // from microsecond timestamps after their lexical value is parsed.
    for (const auto& row : table.rows) {
      if (column >= row.size() || cell_missing(row[column])) continue;
      if (const auto* date = std::get_if<NativeDate>(&row[column]->typed_value))
        if (date->resolution == NativeDateResolution::kNanosecond)
          return "datetime64[ns]";
    }
    return "datetime64[us]";
  }
  return "str";
}

std::string date_precision(const EventTable& table, std::size_t column,
                           const std::vector<std::optional<ExactTimestamp>>& parsed) {
  std::string dtype = source_dtype(table, column);
  if (dtype.find("[ns") != std::string::npos) return "ns";
  if (dtype.find("[us") != std::string::npos) return "us";
  if (dtype.find("[ms") != std::string::npos) return "us";
  if (dtype.find("[s") != std::string::npos) return "s";
  if (table.rows.empty()) return "s";
  // Object columns retain the native unit carried by typed date cells. This
  // matters for whole-second nanosecond timestamps, whose parsed fraction is
  // zero and therefore cannot establish nanosecond precision on its own.
  for (const auto& row : table.rows) {
    if (column >= row.size() || cell_missing(row[column])) continue;
    if (const auto* date = std::get_if<NativeDate>(&row[column]->typed_value))
      if (date->resolution == NativeDateResolution::kNanosecond) return "ns";
  }
  for (const auto& row : table.rows)
    if (!cell_missing(row[column]) && numeric_kind(*row[column])) return "ns";
  if (std::none_of(parsed.begin(), parsed.end(),
                   [](const auto& value) { return value.has_value(); })) return "s";
  for (const auto& value : parsed) {
    if (!value || value->nanoseconds == 0) continue;
    if (value->nanoseconds % 1000 != 0) return "ns";
  }
  return "us";
}

bool all_date_eligible(const EventTable& table, std::size_t column,
                       const std::optional<std::string>& format,
                       std::vector<std::optional<ExactTimestamp>>& parsed) {
  parsed.clear();
  parsed.reserve(table.rows.size());
  const auto dtype = source_dtype(table, column);
  if (is_numeric_dtype(dtype)) return false;
  for (const auto& row : table.rows) {
    if (column >= row.size()) return false;
    if (cell_missing(row[column])) {
      parsed.push_back(std::nullopt);
      continue;
    }
    if (numeric_kind(*row[column])) {
      // Public object columns interpret non-boolean numeric cells as epoch
      // nanoseconds. Numeric dataframe dtypes bypass this conversion above.
      const auto& cell = *row[column];
      std::int64_t nanos = 0;
      if (const auto* signed_number = std::get_if<std::int64_t>(&cell.typed_value)) nanos = *signed_number;
      else if (const auto* unsigned_number = std::get_if<std::uint64_t>(&cell.typed_value)) {
        if (*unsigned_number > static_cast<std::uint64_t>(INT64_MAX)) return false;
        nanos = static_cast<std::int64_t>(*unsigned_number);
      } else if (const auto* floating_number = std::get_if<double>(&cell.typed_value)) {
        if (!std::isfinite(*floating_number) || *floating_number < -9223372036854775808.0 || *floating_number >= 9223372036854775808.0) return false;
        nanos = static_cast<std::int64_t>(*floating_number);
      } else return false;
      if (nanos == INT64_MIN) {
        parsed.push_back(std::nullopt);
        continue;
      }
      auto seconds = nanos / 1000000000;
      auto fraction = nanos % 1000000000;
      if (fraction < 0) { --seconds; fraction += 1000000000; }
      parsed.push_back(ExactTimestamp{seconds, static_cast<std::uint32_t>(fraction)});
      continue;
    }
    const auto text = attribute_text(*row[column]);
    // Public date parsing recognizes only these exact textual missing tokens;
    // whitespace and other capitalization still invalidate column conversion.
    if (text.empty() || text == "NaT" || text == "nat" || text == "NAT" ||
        text == "NaN" || text == "nan" || text == "NAN") {
      parsed.push_back(std::nullopt);
      continue;
    }
    const auto value = parse_timestamp_cell(*row[column], format);
    if (!value) return false;
    parsed.push_back(value);
  }
  return true;
}

EventAttribute date_cell(std::string key, const std::optional<EventAttribute>& source,
                         const std::optional<ExactTimestamp>& parsed,
                         std::string_view precision) {
  if (cell_missing(source)) return null_cell(std::move(key), AttributeKind::kDate);
  if (!parsed) return null_cell(std::move(key), AttributeKind::kDate);
  const auto value = format_exact_timestamp(*parsed, precision);
  EventAttribute result{std::move(key), value, AttributeKind::kDate};
  result.typed_value = NativeDate{value, precision == "ns"
                                           ? NativeDateResolution::kNanosecond
                                           : NativeDateResolution::kMicrosecond, "UTC"};
  result.first_class_state = FirstClassAttributeState::kPresent;
  result.container = AttributeContainerKind::kScalar;
  result.children.clear();
  result.scalar_wrapper = false;
  result.scalar_children_list = false;
  return result;
}

std::optional<std::size_t> find_column(const EventTable& table,
                                       std::string_view name);

void append_column(EventTable& table, std::string name, std::size_t source_column,
                   std::string dtype) {
  if (table.dtypes.empty()) {
    std::vector<std::string> inferred;
    inferred.reserve(table.columns.size());
    for (std::size_t column = 0; column < table.columns.size(); ++column)
      inferred.push_back(source_dtype(table, column));
    table.dtypes = std::move(inferred);
  }
  table.columns.push_back(name);
  table.dtypes.push_back(std::move(dtype));
  if (!table.categorical_dtypes.empty()) {
    const auto category = table.categorical_dtypes[source_column];
    table.categorical_dtypes.push_back(category);
  }
  for (auto& row : table.rows) {
    row.push_back(copy_cell(row[source_column], table.columns.back()));
  }
}

void remove_column(EventTable& table, std::string_view name) {
  const auto found = find_column(table, name);
  if (!found) return;
  const auto column = *found;
  table.columns.erase(table.columns.begin() + static_cast<std::ptrdiff_t>(column));
  if (!table.dtypes.empty())
    table.dtypes.erase(table.dtypes.begin() + static_cast<std::ptrdiff_t>(column));
  if (!table.categorical_dtypes.empty())
    table.categorical_dtypes.erase(
        table.categorical_dtypes.begin() + static_cast<std::ptrdiff_t>(column));
  for (auto& row : table.rows)
    row.erase(row.begin() + static_cast<std::ptrdiff_t>(column));
}

void ensure_aligned_categories(EventTable& table) {
  if (table.categorical_dtypes.empty()) return;
  table.categorical_dtypes.resize(table.columns.size());
}

std::optional<std::size_t> find_column(const EventTable& table,
                                       std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::size_t require_column(const EventTable& table, const std::string& name,
                           std::string_view role) {
  if (const auto column = find_column(table, name)) return *column;
  throw std::invalid_argument(name + " column (" + std::string(role) +
                              ") is not in the dataframe!");
}

void convert_column_to_dates(EventTable& table, std::size_t column,
                             const std::optional<std::string>& format) {
  // Categorical columns keep their categories; the reference never converts
  // them to dates, even when every category is a date or all cells are null.
  // The declared dtype decides: "category" is categorical even when the
  // optional dictionary details are absent.
  if (!table.categorical_dtypes.empty() && column < table.categorical_dtypes.size() &&
      table.categorical_dtypes[column])
    return;
  if (column < table.dtypes.size() && table.dtypes[column] == "category") return;
  std::vector<std::optional<ExactTimestamp>> parsed;
  if (!all_date_eligible(table, column, format, parsed)) return;
  const auto precision = date_precision(table, column, parsed);
  for (std::size_t row = 0; row < table.rows.size(); ++row)
    table.rows[row][column] =
        date_cell(table.columns[column], table.rows[row][column], parsed[row],
                  precision);
  if (table.dtypes.empty()) table.dtypes.resize(table.columns.size());
  table.dtypes[column] = "datetime64[" + precision + ", UTC]";
  if (!table.categorical_dtypes.empty()) table.categorical_dtypes[column] = std::nullopt;
}

void convert_column_to_strings(EventTable& table, std::size_t column) {
  for (auto& row : table.rows)
    row[column] = string_cell(table.columns[column], row[column]);
  if (table.dtypes.empty()) table.dtypes.resize(table.columns.size());
  table.dtypes[column] = "string";
  if (!table.categorical_dtypes.empty()) table.categorical_dtypes[column] = std::nullopt;
}

std::optional<ExactTimestamp> sort_timestamp(const std::optional<EventAttribute>& cell) {
  if (cell_missing(cell)) return std::nullopt;
  if (cell->kind != AttributeKind::kDate) return std::nullopt;
  return parse_timestamp_cell(*cell, std::nullopt);
}

// Compare integer/floating values without rounding an integer through double.
int compare_unsigned_float(std::uint64_t integer, double value) {
  if (value < 0) return 1;
  if (value >= 18446744073709551616.0) return -1;
  const auto whole = static_cast<std::uint64_t>(value);
  if (integer != whole) return integer < whole ? -1 : 1;
  return value > std::floor(value) ? -1 : 0;
}
struct FormatNumber {
  bool negative = false;
  std::uint64_t magnitude = 0;
  std::optional<double> floating;
};
FormatNumber format_number(const EventAttribute& cell) {
  if (const auto* value = std::get_if<double>(&cell.typed_value)) return {false, 0, *value};
  if (const auto* value = std::get_if<bool>(&cell.typed_value)) return {false, *value ? 1U : 0U, {}};
  if (const auto* value = std::get_if<std::uint64_t>(&cell.typed_value)) return {false, *value, {}};
  if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value))
    return {*value < 0, *value < 0 ? static_cast<std::uint64_t>(-(*value + 1)) + 1 : static_cast<std::uint64_t>(*value), {}};
  if (cell.kind == AttributeKind::kFloat) return {false, 0, ::minera::detail::stod(cell.value)};
  if (cell.kind == AttributeKind::kBoolean) return {false, cell.value == "true" || cell.value == "True" || cell.value == "1" ? 1U : 0U, {}};
  const bool negative = cell.value.starts_with('-');
  return {negative, std::stoull(negative ? cell.value.substr(1) : cell.value), {}};
}
int compare_format_numbers(const FormatNumber& a, const FormatNumber& b) {
  if (a.floating && b.floating) return *a.floating < *b.floating ? -1 : *a.floating > *b.floating ? 1 : 0;
  if (a.floating) return -compare_format_numbers(b, a);
  if (b.floating) {
    if (a.negative) return -compare_unsigned_float(a.magnitude, -*b.floating);
    return compare_unsigned_float(a.magnitude, *b.floating);
  }
  if (a.negative != b.negative) return a.negative ? -1 : 1;
  const int comparison = a.magnitude < b.magnitude ? -1 : a.magnitude > b.magnitude ? 1 : 0;
  return a.negative ? -comparison : comparison;
}
bool numeric_less(const EventAttribute& left, const EventAttribute& right) {
  return compare_format_numbers(format_number(left), format_number(right)) < 0;
}

void validate_numeric_sort_categories(const EventTable& table, std::size_t column) {
  struct Category {
    FormatNumber number;
    bool boolean;
  };
  std::vector<Category> categories;
  categories.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    const auto& cell = row[column];
    if (cell_missing(cell) || !numeric_kind(*cell)) return;
    categories.push_back({format_number(*cell),
                          cell->kind == AttributeKind::kBoolean ||
                              std::holds_alternative<bool>(cell->typed_value)});
  }
  // Public sorting first forms exact unique values. The first representative
  // decides the type when equal boolean/integer/float values occur together.
  std::stable_sort(categories.begin(), categories.end(), [](const auto& a, const auto& b) {
    return compare_format_numbers(a.number, b.number) < 0;
  });
  categories.erase(std::unique(categories.begin(), categories.end(), [](const auto& a, const auto& b) {
    return compare_format_numbers(a.number, b.number) == 0;
  }), categories.end());
  if (std::any_of(categories.begin(), categories.end(), [](const auto& value) { return value.boolean; }) ||
      std::none_of(categories.begin(), categories.end(), [](const auto& value) { return value.number.floating.has_value(); }))
    return;
  // Float category inference can collapse distinct exact integers. Reproduce
  // the observed rejection globally, even when the rows have different cases.
  std::set<double> projected;
  for (const auto& category : categories) {
    const auto& number = category.number;
    const auto value = number.floating ? *number.floating :
        (number.negative ? -static_cast<double>(number.magnitude) : static_cast<double>(number.magnitude));
    if (!projected.insert(value).second)
      throw std::invalid_argument("Categorical categories must be unique");
  }
}

EventAttribute index_cell(std::string key, std::int64_t value) {
  EventAttribute result{std::move(key), std::to_string(value), AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

void append_property(EventDataMetadata& metadata, std::string key,
                     std::string value) {
  for (auto& property : metadata.properties) {
    if (property.key == key) {
      property = EventAttribute{std::move(key), std::move(value), AttributeKind::kString};
      return;
    }
  }
  metadata.properties.emplace_back(std::move(key), std::move(value),
                                   AttributeKind::kString);
}

}  // namespace

EventTable format_dataframe(EventTable& input,
                            const DataFrameFormatOptions& options) {
  input.validate();

  // Resolve all selectors before touching the caller-owned frame. This also
  // keeps missing custom selectors from causing a partial alias mutation.
  require_column(input, options.case_id, "case ID");
  require_column(input, options.activity_key, "activity");
  require_column(input, options.timestamp_key, "timestamp");

  // PM4Py makes canonical aliases on the input frame before taking the
  // formatting copy. Keep those aliases raw so callers can observe the same
  // mutation while the returned table receives normalized cells.
  const bool custom_case = options.case_id != kCaseColumn;
  const bool custom_activity = options.activity_key != kActivityColumn;
  // Aliases are replaced in public selector order. Later selectors can
  // observe a canonical alias written by an earlier selector.
  auto alias = [&](const std::string& source, std::string_view target, std::string_view role) {
    if (source == target) return;
    remove_column(input, target);
    const auto column = require_column(input, source, role);
    append_column(input, std::string(target), column, source_dtype(input, column));
  };
  alias(options.case_id, kCaseColumn, "case ID");
  alias(options.activity_key, kActivityColumn, "activity");
  alias(options.timestamp_key, kTimestampColumn, "timestamp");
  ensure_aligned_categories(input);

  const auto start_source =
      options.start_timestamp_key
          ? find_column(input, *options.start_timestamp_key)
          : std::nullopt;

  EventTable working = input;
  working.validate();

  // Resolve indexes again after the alias columns have been appended. The
  // original source indexes remain valid because aliases are appended.
  const auto case_column = require_column(working, options.case_id, "case ID");
  const auto activity_column =
      require_column(working, options.activity_key, "activity");
  const auto timestamp_column =
      require_column(working, options.timestamp_key, "timestamp");
  const auto canonical_case = require_column(working, std::string(kCaseColumn),
                                             "case ID");
  const auto canonical_activity =
      require_column(working, std::string(kActivityColumn), "activity");
  const auto canonical_timestamp =
      require_column(working, std::string(kTimestampColumn), "timestamp");

  if (working.dtypes.empty()) {
    std::vector<std::string> inferred;
    inferred.reserve(working.columns.size());
    for (std::size_t column = 0; column < working.columns.size(); ++column)
      inferred.push_back(source_dtype(working, column));
    working.dtypes = std::move(inferred);
  }

  // Dates are converted on the whole table (a row dropped below still decides
  // whether its column converts and at which unit); rows missing any required
  // value are omitted afterwards.
  // Canonical identifiers are always pandas string dtype. Convert source
  // timestamp/start columns and the canonical timestamp projection only when
  // every non-null value is parseable; invalid text remains text.
  // A canonical identifier column whose values all parse as dates (a column
  // of four-digit years, say) is converted first and then printed as pandas
  // prints a timestamp.
  // This holds for custom selectors too: a custom object case column of 1
  // gives the canonical case '1970-01-01 00:00:00.000000001+00:00'.
  auto identifier_strings = [&](std::size_t column) {
    convert_column_to_dates(working, column, options.timestamp_format);
    if (working.dtypes[column].rfind("datetime64", 0) == 0) {
      for (auto& row : working.rows) {
        if (cell_missing(row[column])) continue;
        auto& cell = *row[column];
        if (cell.value.size() > 10 && cell.value[10] == 'T') cell.value[10] = ' ';
        cell.typed_value = std::monostate{};
        cell.kind = AttributeKind::kString;
      }
    }
    convert_column_to_strings(working, column);
  };
  identifier_strings(canonical_case);
  identifier_strings(canonical_activity);

  // Source selector columns are eligible date columns. Empty custom source
  // schemas are intentionally included: pandas converts an empty object/string
  // series to datetime64[s, UTC]. Other object/string columns are also
  // attempted, matching pandas' observed conversion of extra date-like
  // columns while leaving non-date text untouched.
  std::vector<std::size_t> date_candidates;
  auto add_candidate = [&](std::size_t column) {
    if (std::find(date_candidates.begin(), date_candidates.end(), column) ==
        date_candidates.end())
      date_candidates.push_back(column);
  };
  if (custom_case) add_candidate(case_column);
  if (custom_activity) add_candidate(activity_column);
  add_candidate(timestamp_column);
  if (start_source) add_candidate(*start_source);
  for (std::size_t column = 0; column < working.columns.size(); ++column) {
    if (column == canonical_case || column == canonical_activity) continue;
    add_candidate(column);
  }
  for (const auto column : date_candidates) {
    if (column < working.columns.size())
      convert_column_to_dates(working, column, options.timestamp_format);
  }
  // A custom timestamp selector may have been copied to the canonical column;
  // both columns carry the same normalized date values and dtype.
  if (canonical_timestamp < working.columns.size()) {
    if (canonical_timestamp != timestamp_column)
      convert_column_to_dates(working, canonical_timestamp,
                              options.timestamp_format);
  }

  std::vector<std::size_t> kept;
  kept.reserve(working.rows.size());
  for (std::size_t row = 0; row < working.rows.size(); ++row) {
    if (!cell_missing(working.rows[row][case_column]) &&
        !cell_missing(working.rows[row][activity_column]) &&
        !cell_missing(working.rows[row][timestamp_column]))
      kept.push_back(row);
  }
  EventTable result = working;
  result.rows.clear();
  for (const auto row : kept) result.rows.push_back(working.rows[row]);

  // A successful object-to-date conversion can introduce NaT (the reserved
  // minimum signed nanosecond value). Required timestamp NaTs drop their rows.
  std::erase_if(result.rows, [&](const auto& row) {
    return cell_missing(row[canonical_timestamp]);
  });
  validate_numeric_sort_categories(result, canonical_timestamp);

  // Sort normalized dates exactly, numeric values numerically, and columns
  // that failed date conversion lexically; preserve input order for ties.
  const auto result_case = require_column(result, std::string(kCaseColumn), "case ID");
  const auto result_timestamp =
      require_column(result, std::string(kTimestampColumn), "timestamp");
  std::stable_sort(result.rows.begin(), result.rows.end(), [&](const auto& left,
                                                              const auto& right) {
    const auto left_case = attribute_text(*left[result_case]);
    const auto right_case = attribute_text(*right[result_case]);
    if (left_case != right_case) return left_case < right_case;
    const bool left_numeric = numeric_kind(*left[result_timestamp]);
    const bool right_numeric = numeric_kind(*right[result_timestamp]);
    if (left_numeric != right_numeric) return left_numeric;
    if (left_numeric)
      return numeric_less(*left[result_timestamp], *right[result_timestamp]);
    const auto left_time = sort_timestamp(left[result_timestamp]);
    const auto right_time = sort_timestamp(right[result_timestamp]);
    if (left_time && right_time) return timestamp_less(*left_time, *right_time);
    if (left_time.has_value() != right_time.has_value()) return left_time.has_value();
    if (!cell_missing(left[result_timestamp]) &&
        !cell_missing(right[result_timestamp])) {
      const auto left_text = attribute_text(*left[result_timestamp]);
      const auto right_text = attribute_text(*right[result_timestamp]);
      if (left_text != right_text) return left_text < right_text;
    }
    return false;
  });

  // Existing generated columns retain their positions when overwritten.
  auto generated_column = [&](const std::string& name) {
    if (const auto found = find_column(result, name)) {
      result.dtypes[*found] = "int64";
      if (!result.categorical_dtypes.empty()) result.categorical_dtypes[*found] = std::nullopt;
      return *found;
    }
    const auto column = result.columns.size();
    result.columns.push_back(name);
    result.dtypes.emplace_back("int64");
    if (!result.categorical_dtypes.empty()) result.categorical_dtypes.push_back(std::nullopt);
    for (auto& row : result.rows) row.push_back(std::nullopt);
    return column;
  };
  const auto row_index_column = generated_column("@@index");
  const auto case_index_column = generated_column("@@case_index");
  std::string previous_case;
  bool first_case = true;
  std::int64_t case_index = -1;
  const auto result_case_for_index =
      require_column(result, std::string(kCaseColumn), "case ID");
  for (std::size_t row = 0; row < result.rows.size(); ++row) {
    const auto current_case =
        attribute_text(*result.rows[row][result_case_for_index]);
    if (first_case || current_case != previous_case) {
      ++case_index;
      previous_case = current_case;
      first_case = false;
    }
    result.rows[row][row_index_column] = index_cell("@@index", static_cast<std::int64_t>(row));
    result.rows[row][case_index_column] = index_cell("@@case_index", case_index);
  }

  // A configured start selector is projected under PM4Py's canonical name at
  // the end of the output schema. It is not added to the mutable input frame.
  const bool has_start_projection =
      options.start_timestamp_key && start_source.has_value();
  if (has_start_projection) {
    if (const auto existing_start = find_column(result, kStartTimestampColumn)) {
      for (std::size_t row = 0; row < result.rows.size(); ++row)
        result.rows[row][*existing_start] = copy_cell(
            result.rows[row][*start_source], std::string(kStartTimestampColumn));
      result.dtypes[*existing_start] = source_dtype(result, *start_source);
      if (!result.categorical_dtypes.empty())
        result.categorical_dtypes[*existing_start] = result.categorical_dtypes[*start_source];
      convert_column_to_dates(result, *existing_start, options.timestamp_format);
    } else {
      result.columns.emplace_back(kStartTimestampColumn);
      result.dtypes.emplace_back(source_dtype(result, *start_source));
      // The projection is a copy, so a categorical source stays categorical.
      if (!result.categorical_dtypes.empty())
        result.categorical_dtypes.push_back(result.categorical_dtypes[*start_source]);
      for (std::size_t row = 0; row < result.rows.size(); ++row)
        result.rows[row].push_back(copy_cell(
            result.rows[row][*start_source], std::string(kStartTimestampColumn)));
      const auto start_column = result.columns.size() - 1;
      convert_column_to_dates(result, start_column, options.timestamp_format);
    }
  }

  result.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  result.index->labels.reserve(result.rows.size());
  for (std::size_t row = 0; row < result.rows.size(); ++row)
    result.index->labels.push_back(
        {index_cell("", static_cast<std::int64_t>(row))});

  if (has_start_projection)
    append_property(result.metadata, "pm4py:param:start_timestamp_key",
                    std::string(kStartTimestampColumn));
  append_property(result.metadata, "pm4py:param:activity_key",
                  std::string(kActivityColumn));
  append_property(result.metadata, "pm4py:param:timestamp_key",
                  std::string(kTimestampColumn));
  append_property(result.metadata, "pm4py:param:group_key", "org:group");
  append_property(result.metadata, "pm4py:param:transition_key",
                  "lifecycle:transition");
  append_property(result.metadata, "pm4py:param:resource_key", "org:resource");
  append_property(result.metadata, "pm4py:param:case_id_key",
                  std::string(kCaseColumn));
  result.validate();
  return result;
}

}  // namespace minera
