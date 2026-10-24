#include "minera/polars_attribute_repetition_filter.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

using Scalar = PolarsAttributeRepetitionScalar;

[[noreturn]] void error(std::string type, std::string phase, std::string message) {
  throw PolarsAttributeRepetitionError(std::move(type), std::move(phase), std::move(message));
}

// ---------------------------------------------------------------------------
// Declared column dtypes

enum class Family { kSigned, kUnsigned, kFloat, kBoolean, kString, kCategorical, kNull, kDate, kDatetime };

struct Column {
  Family family = Family::kString;
  int bits = 64;
  // Datetime unit as a power of ten per second (3, 6 or 9) and time zone.
  int unit = 6;
  std::optional<std::string> zone;
};

Column column_type(const std::string& declared) {
  Column result;
  const auto width = [&](std::string_view digits) { return std::stoi(std::string(digits)); };
  for (const int bits : {8, 16, 32, 64}) {
    if (declared == "Int" + std::to_string(bits)) { result.family = Family::kSigned; result.bits = bits; return result; }
    if (declared == "UInt" + std::to_string(bits)) { result.family = Family::kUnsigned; result.bits = bits; return result; }
  }
  if (declared == "Float32" || declared == "Float64") {
    result.family = Family::kFloat; result.bits = width(std::string_view(declared).substr(5)); return result;
  }
  if (declared == "Boolean") { result.family = Family::kBoolean; return result; }
  if (declared == "String") return result;
  if (declared == "Categorical" || declared.starts_with("Enum(")) { result.family = Family::kCategorical; return result; }
  if (declared == "Null") { result.family = Family::kNull; return result; }
  if (declared == "Date") { result.family = Family::kDate; return result; }
  constexpr std::string_view prefix = "Datetime(time_unit='";
  if (declared.starts_with(prefix) && declared.ends_with(")")) {
    const auto unit = std::string_view(declared).substr(prefix.size(), 2);
    const auto rest = std::string_view(declared).substr(prefix.size() + 2);
    result.family = Family::kDatetime;
    result.unit = unit == "ms" ? 3 : unit == "us" ? 6 : unit == "ns" ? 9 : 0;
    if (result.unit && rest == "', time_zone=None)") return result;
    if (result.unit && rest.starts_with("', time_zone='") && rest.ends_with("')")) {
      result.zone = std::string(rest.substr(14, rest.size() - 16));
      return result;
    }
  }
  throw std::invalid_argument("Unsupported native Polars repetition dtype: " + declared);
}

std::string unit_text(int unit) { return unit == 3 ? "ms" : unit == 6 ? "\xce\xbcs" : "ns"; }

std::string schema_name(const Column& column) {
  switch (column.family) {
    case Family::kSigned: return "Int" + std::to_string(column.bits);
    case Family::kUnsigned: return "UInt" + std::to_string(column.bits);
    case Family::kFloat: return "Float" + std::to_string(column.bits);
    case Family::kBoolean: return "Boolean";
    case Family::kDate: return "Date";
    case Family::kDatetime:
      return "Datetime('" + unit_text(column.unit) + "'" + (column.zone ? ", '" + *column.zone + "'" : "") + ")";
    default: return "String";
  }
}

std::string numeric_short(const Column& column) {
  return (column.family == Family::kSigned ? "i" : column.family == Family::kUnsigned ? "u" : "f") +
         std::to_string(column.bits);
}

// ---------------------------------------------------------------------------
// Number and datetime display

std::string chars(double value, std::chars_format format, std::optional<int> precision = std::nullopt) {
  char buffer[512];
  const auto result = precision ? std::to_chars(buffer, buffer + sizeof(buffer), value, format, *precision)
                                : std::to_chars(buffer, buffer + sizeof(buffer), value, format);
  if (result.ec != std::errc{}) throw std::invalid_argument("Invalid native Polars numeric text");
  return std::string(buffer, result.ptr);
}

// Shortest round-trip text without an exponent, as Rust prints f64 values.
std::string rust_display(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  return chars(value, std::chars_format::fixed);
}

std::string rust_exponent(double value, std::optional<int> precision = std::nullopt) {
  auto text = chars(value, std::chars_format::scientific, precision);
  const auto e = text.find('e');
  std::string exponent = text.substr(e + 1);
  const bool negative = exponent.front() == '-';
  exponent.erase(0, 1);
  exponent.erase(0, std::min(exponent.find_first_not_of('0'), exponent.size() - 1));
  return text.substr(0, e) + "e" + (negative ? "-" : "") + exponent;
}

// Float literal text in a printed query plan.
std::string plan_float(double value) {
  constexpr double bound = 999999.0;
  const double magnitude = std::fabs(value);
  const bool integral = std::isfinite(value) && std::trunc(value) == value;
  if (integral && magnitude < bound) return chars(value, std::chars_format::fixed, 1);
  if (rust_display(value).size() > 9) {
    if (!(magnitude >= 0.000001 && magnitude <= bound)) return rust_exponent(value, 4);
    auto text = chars(value, std::chars_format::fixed, 6);
    if (text.ends_with('0')) {
      text.erase(text.find_last_not_of('0') + 1);
      if (text.ends_with('.')) text += '0';
    }
    return text;
  }
  if (integral) return rust_exponent(value);
  return rust_display(value);
}

std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
  const auto quotient = value / divisor;
  return quotient * divisor > value ? quotient - 1 : quotient;
}

void civil_from_days(std::int64_t days, std::int64_t& year, unsigned& month, unsigned& day) {
  days += 719468;
  const std::int64_t era = floor_div(days, 146097);
  const auto doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  year = static_cast<std::int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) ++year;
}

// Datetime text with 3, 6 or 9 fractional digits only when needed.
std::string wall_text(std::int64_t nanoseconds) {
  const auto seconds = floor_div(nanoseconds, 1000000000);
  const auto fraction = nanoseconds - seconds * 1000000000;
  const auto days = floor_div(seconds, 86400);
  const auto clock = seconds - days * 86400;
  std::int64_t year; unsigned month, day;
  civil_from_days(days, year, month, day);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02u %02lld:%02lld:%02lld", static_cast<long long>(year), month, day,
                static_cast<long long>(clock / 3600), static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
  std::string result = buffer;
  if (fraction) {
    if (fraction % 1000000 == 0) std::snprintf(buffer, sizeof(buffer), ".%03lld", static_cast<long long>(fraction / 1000000));
    else if (fraction % 1000 == 0) std::snprintf(buffer, sizeof(buffer), ".%06lld", static_cast<long long>(fraction / 1000));
    else std::snprintf(buffer, sizeof(buffer), ".%09lld", static_cast<long long>(fraction));
    result += buffer;
  }
  return result;
}

// Datetime text in a literal's zone (wall time plus abbreviation) or naive.
std::string datetime_text(std::int64_t nanoseconds, const std::optional<std::string>& zone) {
  if (!zone) return wall_text(nanoseconds);
  if (*zone == "UTC") return wall_text(nanoseconds) + " UTC";
  const std::chrono::time_zone* resolved = nullptr;
  try { resolved = std::chrono::locate_zone(*zone); }
  catch (const std::runtime_error&) { throw std::invalid_argument("Polars datetime zones must be IANA names"); }
  const auto info = resolved->get_info(std::chrono::sys_seconds{std::chrono::seconds{floor_div(nanoseconds, 1000000000)}});
  return wall_text(nanoseconds + std::chrono::duration_cast<std::chrono::seconds>(info.offset).count() * 1000000000) +
         " " + info.abbrev;
}

std::string quoted_raw(const std::string& value) { return "\"" + value + "\""; }

std::string quoted_debug(const std::string& value) {
  std::string result = "\"";
  for (const unsigned char c : value) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      default: result += static_cast<char>(c);
    }
  }
  return result + '"';
}

std::string binary_text(const std::string& bytes) {
  std::string result = "b\"";
  for (const unsigned char c : bytes) {
    if (c >= 0x20 && c < 0x7f) result += static_cast<char>(c);
    else {
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "\\x%02x", c);
      result += buffer;
    }
  }
  return result + '"';
}

double correctly_rounded(const TokenInteger& value) { return std::strtod(value.decimal().c_str(), nullptr); }
float correctly_rounded_f32(const TokenInteger& value) { return std::strtof(value.decimal().c_str(), nullptr); }

// ---------------------------------------------------------------------------
// Python literal model of the typed target

enum class Lit { kNull, kBool, kInt, kFloat, kString, kBinary, kDatetime, kList };

struct Literal {
  Lit kind = Lit::kNull;
  bool boolean = false;
  TokenInteger integer{0};
  std::string int_class;  // i32, i64, u64 or i128
  double real = 0;
  std::string text;       // string or raw binary bytes
  std::int64_t micros = 0;
  // Literal zone: none when naive, UTC for fixed offsets, else the named zone.
  std::optional<std::string> zone;
  std::vector<Literal> items;
  std::string inner, inner_short;
};

[[noreturn]] void unsupported(const std::string& message) { throw std::invalid_argument(message); }

std::string schema_display(const Literal& value);
std::string short_display(const Literal& value);

Literal literal(const EventAttribute& target, bool nested = false) {
  Literal result;
  if (target.scalar_wrapper || target.scalar_children_list)
    unsupported("Polars repetition targets must be scalars or lists");
  if (target.container == AttributeContainerKind::kList || target.container == AttributeContainerKind::kTuple) {
    result.kind = Lit::kList;
    std::optional<std::string> inner, inner_short;
    for (const auto& child : target.children) {
      auto item = literal(child, true);
      if (item.kind == Lit::kInt && item.int_class != "i32" && item.int_class != "i64")
        unsupported("Polars list targets must hold 64-bit integers");
      // Python integer lists become List(Int64) literals.
      if (item.kind == Lit::kInt) item.int_class = "i64";
      if (item.kind != Lit::kNull) {
        const auto name = schema_display(item);
        if (inner && *inner != name) unsupported("Polars list targets must be homogeneous");
        inner = name; inner_short = short_display(item);
      }
      result.items.push_back(std::move(item));
    }
    result.inner = inner.value_or("Null");
    result.inner_short = inner_short.value_or("null");
    return result;
  }
  if (target.container != AttributeContainerKind::kScalar)
    unsupported("Polars repetition targets must be scalars or lists");
  if (target.is_null) return result;
  if (is_native_na_attribute(target)) unsupported("Polars repetition targets cannot be pandas NA");
  if (target.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(target.typed_value)) {
    result.kind = Lit::kBool;
    if (const auto* stored = std::get_if<bool>(&target.typed_value)) result.boolean = *stored;
    else result.boolean = target.value == "true" || target.value == "True" || target.value == "1";
    return result;
  }
  if (target.kind == AttributeKind::kInt) {
    result.kind = Lit::kInt;
    if (const auto* stored = std::get_if<std::int64_t>(&target.typed_value)) result.integer = TokenInteger(*stored);
    else if (const auto* stored = std::get_if<std::uint64_t>(&target.typed_value)) result.integer = TokenInteger(*stored);
    else result.integer = TokenInteger(target.value);
    const auto& v = result.integer;
    if (v >= TokenInteger(std::numeric_limits<std::int32_t>::min()) && v <= TokenInteger(std::numeric_limits<std::int32_t>::max())) result.int_class = "i32";
    else if (v >= TokenInteger(std::numeric_limits<std::int64_t>::min()) && v <= TokenInteger(std::numeric_limits<std::int64_t>::max())) result.int_class = "i64";
    else if (v >= TokenInteger(0) && v <= TokenInteger(std::numeric_limits<std::uint64_t>::max())) result.int_class = "u64";
    else {
      TokenInteger limit(1);
      for (int i = 0; i < 127; ++i) limit *= TokenInteger(2);
      if (v >= -limit && v < limit) result.int_class = "i128";
      else if (nested) unsupported("Polars integer targets must fit in 128 bits");
      else error("InvalidOperationError", "apply",
                 "integer too large for Polars: OverflowError: Python int too large to convert to C long");
    }
    return result;
  }
  if (target.kind == AttributeKind::kFloat || std::holds_alternative<double>(target.typed_value)) {
    result.kind = Lit::kFloat;
    result.real = std::holds_alternative<double>(target.typed_value) ? std::get<double>(target.typed_value)
                                                                     : native_attribute_number(target);
    return result;
  }
  if (target.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(target.typed_value)) {
    const auto* stored = std::get_if<NativeDate>(&target.typed_value);
    const auto parsed = parse_timestamp_exact(stored ? stored->value : target.value);
    if (!parsed) unsupported("Polars datetime targets must be ISO-8601 timestamps");
    // Nanosecond timestamps become microsecond literals, floored.
    result.kind = Lit::kDatetime;
    if (stored && stored->timezone) result.zone = *stored->timezone;
    else if (parsed->timezone_aware) result.zone = "UTC";
    result.micros = parsed->seconds * 1000000 + floor_div(parsed->nanoseconds, 1000);
    return result;
  }
  if (target.kind == AttributeKind::kBytes) { result.kind = Lit::kBinary; result.text = decode_bytes_attribute(target); return result; }
  result.kind = Lit::kString;
  result.text = target.value;
  return result;
}

std::string datetime_schema(const std::optional<std::string>& zone) {
  return zone ? "Datetime('\xce\xbcs', '" + *zone + "')" : "Datetime('\xce\xbcs')";
}

std::string schema_display(const Literal& value) {
  switch (value.kind) {
    case Lit::kNull: return "Null";
    case Lit::kBool: return "Boolean";
    case Lit::kInt:
      return value.int_class == "i128" ? "Int128" : value.int_class == "u64" ? "UInt64" : value.int_class == "i64" ? "Int64" : "Int32";
    case Lit::kFloat: return "Float64";
    case Lit::kString: return "String";
    case Lit::kBinary: return "Binary";
    case Lit::kDatetime: return datetime_schema(value.zone);
    case Lit::kList: return "List(" + value.inner + ")";
  }
  return {};
}

std::string short_display(const Literal& value) {
  switch (value.kind) {
    case Lit::kNull: return "null";
    case Lit::kBool: return "bool";
    case Lit::kInt: return value.int_class;
    case Lit::kFloat: return "f64";
    case Lit::kString: return "str";
    case Lit::kBinary: return "binary";
    case Lit::kDatetime: return value.zone ? "datetime[\xce\xbcs, " + *value.zone + "]" : "datetime[\xce\xbcs]";
    case Lit::kList: return "list[" + value.inner_short + "]";
  }
  return {};
}

std::string value_display(const Literal& value, bool dynamic) {
  switch (value.kind) {
    case Lit::kNull: return "null";
    case Lit::kBool: return value.boolean ? "true" : "false";
    case Lit::kInt: return (dynamic ? "dyn int: " : "") + value.integer.decimal();
    case Lit::kFloat: return dynamic ? "dyn float: " + rust_display(value.real) : plan_float(value.real);
    case Lit::kString: return quoted_raw(value.text);
    case Lit::kBinary: return binary_text(value.text);
    case Lit::kDatetime: return datetime_text(value.micros * 1000, value.zone);
    case Lit::kList: {
      std::string result = "[";
      for (std::size_t i = 0; i < value.items.size(); ++i) {
        if (i) result += ", ";
        result += value_display(value.items[i], false);
      }
      return result + "]";
    }
  }
  return {};
}

// The literal text in an expression diagnostic: numbers stay dynamic.
std::string expression_literal(const Literal& value) { return value_display(value, true); }

// ---------------------------------------------------------------------------
// Comparison resolution

bool int_literal(const Literal& value) { return value.kind == Lit::kInt || value.kind == Lit::kBool; }
TokenInteger integer_of(const Literal& value) {
  return value.kind == Lit::kBool ? TokenInteger(value.boolean ? 1 : 0) : value.integer;
}
double double_of(const Literal& value) {
  return value.kind == Lit::kFloat ? value.real : correctly_rounded(integer_of(value));
}

bool fits(const Column& column, const TokenInteger& value) {
  if (column.family == Family::kSigned) {
    TokenInteger high(1);
    for (int i = 1; i < column.bits; ++i) high *= TokenInteger(2);
    return value >= -high && value < high;
  }
  TokenInteger high(1);
  for (int i = 0; i < column.bits; ++i) high *= TokenInteger(2);
  return value >= TokenInteger(0) && value < high;
}

bool datetime_compatible(const Column& column, const Literal& value) {
  return column.zone == value.zone;
}

std::string column_reference(const std::string& key) { return "col(" + quoted_raw(key) + ")"; }

// The equality as printed in a resolved query plan.
std::string plan_filter(const Column& column, const std::string& key, const Literal& value) {
  const auto col = column_reference(key);
  if (value.kind == Lit::kNull) return "null.repeat([" + col + ".len()])";
  const auto plain = col + " == " + value_display(value, false);
  const auto dynamic = col + " == " + expression_literal(value);
  const auto never = "when(" + col + ".is_not_null()).then(false).otherwise(null)";
  switch (column.family) {
    case Family::kNull:
      if (value.kind == Lit::kInt || value.kind == Lit::kFloat) return dynamic;
      return col + ".cast(" + schema_display(value) + ") == " + value_display(value, false);
    case Family::kSigned:
    case Family::kUnsigned:
      if (value.kind == Lit::kBool) return col + " == " + (value.boolean ? "1" : "0");
      if (value.kind == Lit::kInt) {
        if (fits(column, value.integer)) return plain;
        if (column.family == Family::kSigned && value.int_class == "u64")
          return col + ".cast(Float64) == " + plan_float(correctly_rounded(value.integer));
        if (column.family == Family::kUnsigned && column.bits == 64 && value.int_class != "i128" && value.integer < TokenInteger(0))
          return col + ".cast(Int64) == " + value.integer.decimal();
        return never;
      }
      if (value.kind == Lit::kFloat) return col + ".cast(Float64) == " + plan_float(value.real);
      if (value.kind == Lit::kDatetime) {
        if (column.bits < 32) return plain;
        return col + (column.family == Family::kSigned && column.bits == 64 ? "" : ".cast(Int64)") + " == " +
               std::to_string(value.micros);
      }
      return plain;
    case Family::kFloat: {
      const auto cast = [&](double v) { return column.bits == 32 ? static_cast<double>(static_cast<float>(v)) : v; };
      if (value.kind == Lit::kBool) return col + " == " + plan_float(value.boolean ? 1.0 : 0.0);
      if (value.kind == Lit::kInt)
        return col + " == " + plan_float(column.bits == 32 ? static_cast<double>(correctly_rounded_f32(value.integer))
                                                           : correctly_rounded(value.integer));
      if (value.kind == Lit::kFloat) return col + " == " + plan_float(cast(value.real));
      if (value.kind == Lit::kDatetime)
        return col + (column.bits == 32 ? ".cast(Float64)" : "") + " == " + plan_float(static_cast<double>(value.micros));
      return plain;
    }
    case Family::kBoolean:
      if (value.kind == Lit::kInt) {
        const auto cast = value.int_class == "i32" ? "Int32" : value.int_class == "i64" ? "Int64" : value.int_class == "u64" ? "UInt64" : "Int128";
        return col + ".cast(" + cast + ") == " + value.integer.decimal();
      }
      if (value.kind == Lit::kFloat) return dynamic;
      if (value.kind == Lit::kString) return col + ".cast(String) == " + quoted_raw(value.text);
      return plain;
    case Family::kString:
      if (value.kind == Lit::kBool) return col + " == " + quoted_raw(value.boolean ? "true" : "false");
      if (value.kind == Lit::kInt || value.kind == Lit::kFloat) return dynamic;
      if (value.kind == Lit::kBinary) return col + ".cast(Binary) == " + binary_text(value.text);
      if (value.kind == Lit::kList) return plain + ".cast(String)";
      return plain;
    case Family::kCategorical:
      return dynamic;
    case Family::kDate:
      if (value.kind == Lit::kDatetime) return col + ".cast(" + datetime_schema(value.zone) + ") == " + value_display(value, false);
      return dynamic;
    case Family::kDatetime:
      if (value.kind != Lit::kDatetime) return dynamic;
      if (!datetime_compatible(column, value)) return plain;
      if (column.unit == 3) return col + " == " + datetime_text(floor_div(value.micros, 1000) * 1000000, value.zone);
      if (column.unit == 9)
        return col + ".is_between([" + datetime_text(value.micros * 1000, value.zone) + ", " +
               datetime_text(value.micros * 1000 + 999, value.zone) + "])";
      return plain;
  }
  return plain;
}

std::string frame_plan(const EventTable& source) {
  std::string result = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) {
    if (i) result += ", ";
    result += quoted_raw(source.columns[i]);
  }
  if (source.columns.size() > 4) result += ", ...";
  return result + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}

std::string trace(const EventTable& source, const std::optional<std::string>& filter, bool filter_failed) {
  std::string result = "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n";
  if (filter) result += "FILTER " + *filter + "\nFROM\n  ";
  result += frame_plan(source) + "\n\nThis error occurred with the following context stack:\n";
  const std::vector<std::string> nodes = filter_failed
      ? std::vector<std::string>{"filter", "group_by", "filter", "select", "join right", "join"}
      : std::vector<std::string>{"group_by", "filter", "select", "join right", "join"};
  for (std::size_t i = 0; i < nodes.size(); ++i)
    result += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'" + (filter_failed && i == 0 ? " failed\n" : "\n");
  return result;
}

// Levenshtein distance over Unicode scalar values.
std::size_t edit_distance(const std::u32string& a, const std::u32string& b) {
  std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j)
      current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (a[i - 1] != b[j - 1])});
    std::swap(previous, current);
  }
  return previous[b.size()];
}

std::u32string code_points(const std::string& text) {
  std::u32string result;
  for (std::size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    const int extra = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    char32_t value = extra == 0 ? c : extra == 1 ? c & 0x1f : extra == 2 ? c & 0x0f : c & 0x07;
    for (int k = 1; k <= extra && i + k < text.size(); ++k) value = (value << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3f);
    result.push_back(value);
    i += extra + 1;
  }
  return result;
}

[[noreturn]] void missing_column(const EventTable& source, const std::string& name,
                                  const std::optional<std::string>& filter) {
  std::string message = "unable to find column " + quoted_debug(name) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) {
    if (i) message += ", ";
    message += quoted_debug(source.columns[i]);
  }
  message += ']';
  // The suggestion is the closest column by edit distance, offered when the
  // distance is at most a third of the requested name's length.
  const auto wanted = code_points(name);
  std::optional<std::size_t> best;
  std::size_t best_distance = 0;
  for (std::size_t i = 0; i < source.columns.size(); ++i) {
    const auto distance = edit_distance(wanted, code_points(source.columns[i]));
    if (!best || distance < best_distance) { best = i; best_distance = distance; }
  }
  if (best && 3 * best_distance <= wanted.size())
    message += "\n\nDid you mean " + quoted_debug(source.columns[*best]) + '?';
  error("ColumnNotFoundError", "collect", message + trace(source, filter, false));
}

std::string expression(const std::string& key, const Literal& value) {
  return "\n\nThis error occurred in the following expression:\n\t[(" + column_reference(key) + ") == (" +
         expression_literal(value) + ")]\n";
}

// Errors raised while the filter is resolved into a plan.
void plan_errors(const EventTable& source, const Column& column, const std::string& key, const Literal& value) {
  std::optional<std::string> message;
  if ((column.family == Family::kDate || column.family == Family::kDatetime) && value.kind == Lit::kString)
    message = "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)";
  else if (column.family == Family::kString && value.kind == Lit::kDatetime)
    message = "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)";
  else if (column.family == Family::kString && value.kind == Lit::kList)
    message = "cannot cast List type (inner: '" + value.inner + "', to: 'String')";
  if (message) error("InvalidOperationError", "collect", *message + trace(source, plan_filter(column, key, value), true));
}

// Errors raised when the comparison is evaluated, even over zero rows.
void evaluation_errors(const Column& column, const std::string& key, const Literal& value) {
  const auto schema = [&] {
    error("SchemaError", "collect", "could not evaluate comparison between series '" + key + "' of dtype: " +
          schema_name(column) + " and series 'literal' of dtype: " + schema_display(value) + expression(key, value));
  };
  if (value.kind == Lit::kNull) return;
  switch (column.family) {
    case Family::kNull: return;
    case Family::kSigned: case Family::kUnsigned: case Family::kFloat:
      if (value.kind == Lit::kString)
        error("ComputeError", "collect", "cannot compare string with numeric type (" + numeric_short(column) + ")" + expression(key, value));
      if (value.kind == Lit::kBinary || value.kind == Lit::kList) schema();
      if (value.kind == Lit::kDatetime && column.bits < 32) schema();
      return;
    case Family::kBoolean:
      if (value.kind == Lit::kBinary || value.kind == Lit::kDatetime || value.kind == Lit::kList) schema();
      return;
    case Family::kString:
      if (value.kind == Lit::kInt || value.kind == Lit::kFloat)
        error("ComputeError", "collect", "cannot compare string with numeric type (" + short_display(value) + ")" + expression(key, value));
      return;
    case Family::kCategorical:
      if (value.kind != Lit::kString)
        error("ComputeError", "collect", "cannot compare categorical with " + short_display(value) + expression(key, value));
      return;
    case Family::kDate:
      if (value.kind == Lit::kBool || value.kind == Lit::kBinary || value.kind == Lit::kList ||
          (value.kind == Lit::kInt && value.int_class == "i128")) schema();
      return;
    case Family::kDatetime:
      if (value.kind == Lit::kBool || value.kind == Lit::kBinary || value.kind == Lit::kList ||
          (value.kind == Lit::kInt && value.int_class == "i128") ||
          (value.kind == Lit::kDatetime && !datetime_compatible(column, value))) schema();
      return;
  }
}

// ---------------------------------------------------------------------------
// Stored cells

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }

TokenInteger cell_integer(const EventAttribute& value) {
  if (const auto* stored = std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*stored);
  if (const auto* stored = std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*stored);
  if (const auto* stored = std::get_if<bool>(&value.typed_value)) return TokenInteger(*stored ? 1 : 0);
  if (value.kind == AttributeKind::kBoolean) return TokenInteger(value.value == "true" || value.value == "1" ? 1 : 0);
  return TokenInteger(value.value);
}

double cell_double(const EventAttribute& value) {
  if (const auto* stored = std::get_if<double>(&value.typed_value)) return *stored;
  if (value.kind == AttributeKind::kFloat) return native_attribute_number(value);
  return correctly_rounded(cell_integer(value));
}

bool cell_boolean(const EventAttribute& value) {
  if (const auto* stored = std::get_if<bool>(&value.typed_value)) return *stored;
  return value.value == "true" || value.value == "True" || value.value == "1";
}

std::optional<ParsedExactTimestamp> cell_instant(const EventAttribute& value) {
  const auto* stored = std::get_if<NativeDate>(&value.typed_value);
  return parse_timestamp_exact(stored ? stored->value : value.value);
}

// Physical value: days for Date, column units since the epoch for Datetime.
std::optional<std::int64_t> cell_physical(const EventAttribute& value, const Column& column) {
  const auto instant = cell_instant(value);
  if (!instant) return std::nullopt;
  if (column.family == Family::kDate) return floor_div(instant->seconds, 86400);
  const std::int64_t scale = column.unit == 3 ? 1000 : column.unit == 6 ? 1000000 : 1000000000;
  return instant->seconds * scale + instant->nanoseconds / (1000000000 / scale);
}

bool float_equal(double a, double b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

bool matches(const EventAttribute& cell, const Column& column, const Literal& value) {
  if (null(cell) || value.kind == Lit::kNull) return false;
  switch (column.family) {
    case Family::kNull: return false;
    case Family::kSigned: case Family::kUnsigned: {
      if (value.kind == Lit::kFloat) return correctly_rounded(cell_integer(cell)) == value.real;
      if (value.kind == Lit::kDatetime) return cell_integer(cell) == TokenInteger(value.micros);
      if (value.kind == Lit::kInt && column.family == Family::kSigned && value.int_class == "u64")
        return correctly_rounded(cell_integer(cell)) == correctly_rounded(value.integer);
      return int_literal(value) && cell_integer(cell) == integer_of(value);
    }
    case Family::kFloat: {
      const double stored = cell_double(cell);
      if (value.kind == Lit::kDatetime) return stored == static_cast<double>(value.micros);
      if (column.bits == 32) {
        const float wanted = value.kind == Lit::kFloat ? static_cast<float>(value.real)
                                                       : correctly_rounded_f32(integer_of(value));
        return float_equal(stored, static_cast<double>(wanted));
      }
      return float_equal(stored, double_of(value));
    }
    case Family::kBoolean: {
      const bool stored = cell_boolean(cell);
      if (value.kind == Lit::kString) return value.text == (stored ? "true" : "false");
      if (value.kind == Lit::kFloat) return value.real == (stored ? 1.0 : 0.0);
      return int_literal(value) && integer_of(value) == TokenInteger(stored ? 1 : 0);
    }
    case Family::kString:
      if (value.kind == Lit::kBool) return cell.value == (value.boolean ? "true" : "false");
      return (value.kind == Lit::kString || value.kind == Lit::kBinary) && cell.value == value.text;
    case Family::kCategorical:
      return value.kind == Lit::kString && cell.value == value.text;
    case Family::kDate: {
      const auto days = cell_physical(cell, column);
      if (!days) return false;
      if (value.kind == Lit::kInt) return TokenInteger(*days) == value.integer;
      if (value.kind == Lit::kFloat) return static_cast<double>(*days) == value.real;
      if (value.kind == Lit::kDatetime) return *days * 86400000000LL == value.micros;
      return false;
    }
    case Family::kDatetime: {
      const auto physical = cell_physical(cell, column);
      if (!physical) return false;
      if (value.kind == Lit::kInt) return TokenInteger(*physical) == value.integer;
      if (value.kind == Lit::kFloat) return static_cast<double>(*physical) == value.real;
      if (value.kind != Lit::kDatetime) return false;
      // Compare at the coarser unit, flooring the finer side.
      if (column.unit == 3) return *physical == floor_div(value.micros, 1000);
      if (column.unit == 9) return floor_div(*physical, 1000) == value.micros;
      return *physical == value.micros;
    }
  }
  return false;
}

std::optional<std::string> group(const std::optional<EventAttribute>& candidate, const Column& column) {
  if (!candidate || null(*candidate)) return std::nullopt;
  switch (column.family) {
    case Family::kNull: return std::nullopt;
    case Family::kString: case Family::kCategorical: return "s:" + candidate->value;
    case Family::kFloat: {
      const auto value = cell_double(*candidate);
      if (std::isnan(value)) return "f:NaN";
      return "f:" + std::to_string(std::bit_cast<std::uint64_t>(value == 0 ? 0.0 : value));
    }
    case Family::kSigned: case Family::kUnsigned: case Family::kBoolean:
      return "n:" + cell_integer(*candidate).decimal();
    case Family::kDate: case Family::kDatetime: {
      const auto physical = cell_physical(*candidate, column);
      if (!physical) return std::nullopt;
      return "d:" + std::to_string(*physical);
    }
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Bounds

std::string scalar_type(const Scalar& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return "NoneType";
  if (std::holds_alternative<bool>(value)) return "bool";
  if (std::holds_alternative<TokenInteger>(value)) return "int";
  if (std::holds_alternative<double>(value)) return "float";
  return "str";
}

void validate_apply(const PolarsAttributeRepetitionOptions& options, const EventAttribute& target) {
  if (!std::holds_alternative<std::string>(options.attribute_key))
    error("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" +
          scalar_type(options.attribute_key) + "'.");
  literal(target);
  if (std::holds_alternative<std::nullptr_t>(options.case_id_key))
    error("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
  if (!std::holds_alternative<std::string>(options.case_id_key))
    throw std::invalid_argument("Native Polars repetition companion requires a string case selector");
}

std::string bound_literal(const Scalar& value) {
  return std::visit([](const auto& item) -> std::string {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) return "null";
    else if constexpr (std::is_same_v<T, std::string>) return quoted_raw(item);
    else if constexpr (std::is_same_v<T, bool>) return item ? "1" : "0";
    else if constexpr (std::is_same_v<T, TokenInteger>) return item.decimal();
    else {
      auto text = rust_display(item);
      if (std::isfinite(item) && text.find_first_of(".e") == std::string::npos) text += ".0";
      return text;
    }
  }, value);
}

// A u32 count compared with an integer outside its range folds to a constant.
std::optional<TokenInteger> integer_bound(const Scalar& value) {
  if (const auto* whole = std::get_if<TokenInteger>(&value)) return *whole;
  if (const auto* flag = std::get_if<bool>(&value)) return TokenInteger(*flag ? 1 : 0);
  return std::nullopt;
}

bool in_count_range(const TokenInteger& value) {
  return value >= TokenInteger(0) && value <= TokenInteger(std::numeric_limits<std::uint32_t>::max());
}

std::string bound_expression(const Scalar& value, bool lower) {
  const std::string op = lower ? ">=" : "<=";
  if (const auto whole = integer_bound(value)) {
    if (!in_count_range(*whole)) {
      const bool always = (*whole < TokenInteger(0)) == lower;
      return std::string(".when(col(\"count\").is_not_null()).then(") + (always ? "true" : "false") + ").otherwise(null)";
    }
    return "[(col(\"count\")) " + op + " (" + whole->decimal() + ")]";
  }
  if (std::holds_alternative<double>(value))
    return "[(col(\"count\").cast(Float64)) " + op + " (" + bound_literal(value) + ")]";
  return "[(col(\"count\")) " + op + " (" + bound_literal(value) + ")]";
}

void validate_bounds(const PolarsAttributeRepetitionOptions& options) {
  for (const bool lower : {true, false}) {
    const auto& value = lower ? options.minimum : options.maximum;
    if (!std::holds_alternative<std::string>(value)) continue;
    std::string message = "cannot compare string with numeric type (u32)\n\nThis error occurred in the following expression:\n\t" +
                          bound_expression(value, lower) + "\n";
    const bool min_null = std::holds_alternative<std::nullptr_t>(options.minimum);
    const bool max_null = std::holds_alternative<std::nullptr_t>(options.maximum);
    if (min_null)
      message += "while evaluating this larger expression:\n\t[(" + bound_expression(options.maximum, false) +
                 ") & (null.repeat([col(\"count\").len()]))]\n";
    else if (!max_null)
      message += "while evaluating this larger expression:\n\t[(" + bound_expression(options.minimum, true) + ") & (" +
                 bound_expression(options.maximum, false) + ")]\n";
    error("ComputeError", "collect", message);
  }
}

// Integer bounds within the count range with minimum above maximum fold the
// counted side to an empty frame, so the attribute comparison never runs.
bool bounds_fold_empty(const PolarsAttributeRepetitionOptions& options) {
  const auto low = integer_bound(options.minimum), high = integer_bound(options.maximum);
  return low && high && in_count_range(*low) && in_count_range(*high) && *high < *low;
}

bool bound_accepts(std::uint64_t count, const Scalar& bound, bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_same_v<T, std::string>) return false;
    else if constexpr (std::is_same_v<T, TokenInteger>)
      return lower ? value <= TokenInteger(count) : TokenInteger(count) <= value;
    else if constexpr (std::is_same_v<T, bool>) return lower ? count >= std::uint64_t(value) : count <= std::uint64_t(value);
    else {
      // Polars total ordering places NaN after all finite/infinite numbers.
      if (std::isnan(value)) return !lower;
      return lower ? static_cast<double>(count) >= value : static_cast<double>(count) <= value;
    }
  }, bound);
}

EventTable collect(const EventTable& source, const EventAttribute& target,
                    const PolarsAttributeRepetitionOptions& options) {
  source.validate();
  if (source.index) throw std::invalid_argument("Native Polars repetition tables cannot have dataframe index metadata");
  if (source.dtypes.size() != source.columns.size())
    throw std::invalid_argument("Native Polars repetition requires declared column dtypes");
  std::vector<Column> columns;
  for (const auto& declared : source.dtypes) columns.push_back(column_type(declared));
  const auto value = literal(target);
  const auto& attribute = std::get<std::string>(options.attribute_key);
  const auto& case_key = std::get<std::string>(options.case_id_key);
  const auto attr_at = std::find(source.columns.begin(), source.columns.end(), attribute);
  if (attr_at == source.columns.end()) missing_column(source, attribute, std::nullopt);
  const auto attribute_column = static_cast<std::size_t>(attr_at - source.columns.begin());
  const auto& attribute_type = columns[attribute_column];
  plan_errors(source, attribute_type, attribute, value);
  const auto case_at = std::find(source.columns.begin(), source.columns.end(), case_key);
  if (case_at == source.columns.end()) missing_column(source, case_key, plan_filter(attribute_type, attribute, value));
  const auto case_column = static_cast<std::size_t>(case_at - source.columns.begin());
  EventTable result = source;
  result.rows.clear();
  if (bounds_fold_empty(options)) return result;
  evaluation_errors(attribute_type, attribute, value);
  validate_bounds(options);
  std::unordered_map<std::string, std::uint64_t> counts;
  for (const auto& row : source.rows) {
    const auto key = group(row[case_column], columns[case_column]);
    if (key && row[attribute_column] && matches(*row[attribute_column], attribute_type, value)) ++counts[*key];
  }
  for (const auto& row : source.rows) {
    const auto key = group(row[case_column], columns[case_column]);
    if (!key) continue;
    // A one-row frame joins a NaN case key against nothing.
    if (source.rows.size() == 1 && *key == "f:NaN") continue;
    const auto found = counts.find(*key);
    if (found != counts.end() && bound_accepts(found->second, options.minimum, true) &&
        bound_accepts(found->second, options.maximum, false)) result.rows.push_back(row);
  }
  return result;
}

}  // namespace

EventTable filter_polars_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& options) {
  validate_apply(options, target);
  return collect(source, target, options);
}

LazyEventTable filter_polars_attribute_value_repetition(
    const LazyEventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& options) {
  validate_apply(options, target);
  return LazyEventTable(source.columns(), [source, target, options](const auto& consume) {
    const auto result = collect(source.materialize(), target, options);
    for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, source.dtypes(),
     source.index(), source.categorical_dtypes());
}

}  // namespace minera
