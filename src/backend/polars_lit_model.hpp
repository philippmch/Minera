#pragma once

// Polars literal and dtype rules shared by the native Polars filters
// (black-box, Polars 1.44.1 as pinned by the reference): Python values as
// pl.lit builds them (rounds CB and CC, first written for the sequence
// filters), the is_in checks and plan casts of a list literal, the casts of
// literal values to a column's dtype and their plan texts, and the Python
// values of typed cells. Failures throw PolarsSequenceFilterError (type and
// message); other modules rethrow it as their own error.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/polars_sequence_filters.hpp"
#include "minera/timestamp.hpp"
#include "polars_float_text.hpp"
#include "python_scalar.hpp"
#include "zone_database.hpp"

namespace minera::detail::polars_lit {

using Value = PolarsFilterValue;
using Kind = PolarsFilterValue::Kind;
using Cell = std::optional<EventAttribute>;

[[noreturn]] inline void fail(const std::string& type, const std::string& message) {
  throw PolarsSequenceFilterError(type, message);
}
inline std::string quote(const std::string& v) { return '"' + v + '"'; }
inline bool null_cell(const Cell& cell) { return !cell || cell->is_null || is_native_na_attribute(*cell); }
inline std::int64_t integer_of(const EventAttribute& cell) {
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return *i;
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return static_cast<std::int64_t>(*u);
  return std::stoll(cell.value);
}
// Round CE: a Python int above the Int64 range (up to 2**64 - 1, as typed
// JSON carries it in UInt64) is a kInt value whose text holds its decimal
// digits; integer then holds its low 64 bits. Polars types it Int128 as a
// value, UInt64 in a list of non-negative ints.
inline Value wide_value(std::uint64_t u) {
  auto v = Value::of(static_cast<std::int64_t>(u));
  v.text = std::to_string(u);
  return v;
}
inline std::optional<std::uint64_t> wide_int(const Value& v) {
  if (v.kind != Kind::kInt || v.text.empty()) return std::nullopt;
  return static_cast<std::uint64_t>(v.integer);
}
inline std::optional<std::uint64_t> wide_cell(const EventAttribute& cell) {
  constexpr auto top = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return *u > top ? std::optional<std::uint64_t>(*u) : std::nullopt;
  if (!std::holds_alternative<std::monostate>(cell.typed_value)) return std::nullopt;
  std::uint64_t u = 0;
  const auto* first = cell.value.data();
  const auto* last = first + cell.value.size();
  const auto r = std::from_chars(first, last, u);
  if (r.ec == std::errc{} && r.ptr == last && u > top) return u;
  return std::nullopt;
}
inline double integer_real(const Value& v) {
  if (const auto u = wide_int(v)) return static_cast<double>(*u);
  return static_cast<double>(v.integer);
}
inline std::string integer_text(const Value& v) { return wide_int(v) ? v.text : std::to_string(v.integer); }
inline double real_of(const EventAttribute& cell) {
  if (const auto* d = std::get_if<double>(&cell.typed_value)) return *d;
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return static_cast<double>(*i);
  const auto& t = cell.value;
  if (t == "nan" || t == "NaN") return std::nan("");
  if (t == "inf" || t == "Infinity") return HUGE_VAL;
  if (t == "-inf" || t == "-Infinity") return -HUGE_VAL;
  return std::strtod(t.c_str(), nullptr);
}
inline bool true_of(const EventAttribute& cell) {
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return *b;
  return cell.value == "true" || cell.value == "True" || cell.value == "1";
}
inline bool integer_dtype(const std::string& dtype) {
  return dtype.rfind("Int", 0) == 0 || dtype.rfind("UInt", 0) == 0;
}
inline bool enum_dtype(const std::string& dtype) { return dtype.rfind("Enum", 0) == 0; }

inline void enum_is_in(const std::vector<std::string>& category_list, const std::vector<Value>& values,
                       const std::string& suffix) {
  const std::set<std::string> categories(category_list.begin(), category_list.end());
  std::vector<std::string> failed;
  for (const auto& v : values)
    if (v.kind == Kind::kString && !categories.count(v.text)) failed.push_back(v.text);
  if (failed.empty()) return;
  const auto shown = std::min<std::size_t>(failed.size(), 10);
  std::string list = "[";
  if (shown <= 3) {
    for (std::size_t i = 0; i < shown; ++i) list += (i ? ", " : "") + quote(failed[i]);
  } else {
    list += quote(failed[0]) + ", " + quote(failed[1]) + ", \xe2\x80\xa6 " + quote(failed[shown - 1]);
  }
  fail("InvalidOperationError", "conversion from `str` to `enum` failed in column '' for " +
                                    std::to_string(failed.size()) + " out of " + std::to_string(values.size()) +
                                    " values: " + list +
                                    "]\n\nEnsure that all values in the input column are present in the categories "
                                    "of the enum datatype." +
                                    suffix);
}


// ------------------------------------------------------------------ typed activities

// Round CB: integer, float, Boolean, Date and Datetime activity columns.

inline bool float_dtype(const std::string& dtype) { return dtype.rfind("Float", 0) == 0; }
inline bool datetime_dtype(const std::string& dtype) { return dtype.rfind("Datetime", 0) == 0; }
inline bool categorical_dtype(const std::string& dtype) { return dtype.rfind("Categorical", 0) == 0; }
// The quoted field of a Datetime dtype ("time_unit='us'", "time_zone='UTC'").
inline std::optional<std::string> dtype_field(const std::string& dtype, const std::string& field) {
  const auto at = dtype.find(field + "='");
  if (at == std::string::npos) return std::nullopt;
  const auto from = at + field.size() + 2;
  return dtype.substr(from, dtype.find('\'', from) - from);
}
inline std::int64_t unit_nanoseconds(const std::string& dtype) {
  const auto unit = dtype_field(dtype, "time_unit").value_or("us");
  return unit == "ns" ? 1 : unit == "ms" ? 1'000'000 : 1'000;
}

// A temporal cell as a parsed timestamp (a Date at its midnight).
inline std::optional<ParsedExactTimestamp> timestamp_of(const EventAttribute& cell) {
  const auto scalar = python::from_attribute(cell);
  if (scalar.kind != python::Scalar::Kind::kDate) return std::nullopt;
  return scalar.date;
}
inline std::int64_t floor_div(std::int64_t a, std::int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }
// A Datetime cell's physical value (its UTC instant in the column's unit, a
// naive one's wall clock), a Date cell's days since the epoch.
inline std::optional<std::int64_t> physical_of(const EventAttribute& cell, const std::string& dtype) {
  const auto t = timestamp_of(cell);
  if (!t) return std::nullopt;
  if (dtype == "Date") return floor_div(t->seconds, 86'400);
  const auto unit = unit_nanoseconds(dtype);
  return t->seconds * (1'000'000'000 / unit) + static_cast<std::int64_t>(t->nanoseconds) / unit;
}

// The Python value Polars hands out for a cell: an int, float, bool, str,
// datetime.date, or datetime.datetime (microseconds, aware in the column's
// zone); a missing cell is None.
inline Value python_value(const Cell& cell, const std::string& dtype) {
  if (null_cell(cell)) return Value::none();
  if (integer_dtype(dtype)) {
    if (const auto u = wide_cell(*cell)) return wide_value(*u);
    return Value::of(integer_of(*cell));
  }
  if (float_dtype(dtype)) return Value::of(real_of(*cell));
  if (dtype == "Boolean") return Value::of(true_of(*cell));
  if (dtype == "Date" || datetime_dtype(dtype)) {
    const auto t = timestamp_of(*cell);
    if (!t) return Value::of(cell->value);
    if (dtype == "Date") return Value::date(floor_div(t->seconds, 86'400));
    const auto zone = dtype_field(dtype, "time_zone");
    const std::int64_t offset = zone ? t->offset_minutes : 0;
    const std::int64_t wall = (t->seconds + offset * 60) * 1'000'000'000 + (t->nanoseconds / 1000) * 1000;
    if (!zone) return Value::datetime(wall);
    return Value::datetime(wall, static_cast<std::int32_t>(offset), *zone);
  }
  return Value::of(cell->value);
}
// Two Python values as one dict key.
inline bool same_key(const Value& a, const Value& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case Kind::kNone: return true;
    case Kind::kBool: return a.boolean == b.boolean;
    case Kind::kInt: return a.integer == b.integer && a.text == b.text;
    case Kind::kDate: return a.integer == b.integer;
    case Kind::kFloat: return a.real == b.real || (std::isnan(a.real) && std::isnan(b.real));
    case Kind::kDatetime: return pandas_filter_values_equal(a, b);
    default: return a.text == b.text;
  }
}

// cast(pl.String) of a cell: integers in decimal, floats as Python prints
// them (NaN as "NaN"), Booleans lower case, a Date as YYYY-MM-DD and a
// Datetime as "YYYY-MM-DD HH:MM:SS" with its unit's digits, an aware one in
// its zone with the offset.
inline std::string string_cast(const EventAttribute& cell, const std::string& dtype) {
  if (float_dtype(dtype)) {
    const double v = real_of(cell);
    return std::isnan(v) ? "NaN" : python::float_repr(v);
  }
  if (dtype == "Boolean") return true_of(cell) ? "true" : "false";
  if (!datetime_dtype(dtype) && dtype != "Date") return cell.value;
  const auto t = timestamp_of(cell);
  if (!t) return cell.value;
  if (dtype == "Date") return pandas_filter_value_str(Value::date(floor_div(t->seconds, 86'400)));
  const auto zone = dtype_field(dtype, "time_zone");
  const std::int64_t offset = zone ? t->offset_minutes : 0;
  const auto wall = Value::datetime((t->seconds + offset * 60) * 1'000'000'000);
  std::string out = pandas_filter_value_str(wall).substr(0, 19);
  const auto unit = unit_nanoseconds(dtype);
  std::string digits = std::to_string(t->nanoseconds);
  digits.insert(digits.begin(), 9 - digits.size(), '0');
  out += "." + digits.substr(0, unit == 1 ? 9 : unit == 1000 ? 6 : 3);
  if (zone) {
    const auto magnitude = offset < 0 ? -offset : offset;
    const auto two = [](std::int64_t v) { return std::string(1, static_cast<char>('0' + v / 10)) + static_cast<char>('0' + v % 10); };
    out += (offset < 0 ? "-" : "+") + two(magnitude / 60) + ":" + two(magnitude % 60);
  }
  return out;
}

// ------------------------------------------------------------------ list literals

// Round CC: a Python list as pl.lit builds it (the is_in, prefix and suffix
// literals; Polars 1.44, black-box). Each item becomes a typed value (an
// aware datetime with a fixed offset in UTC, one with a named zone in that
// zone, a nested list recursively). The items are read up to the first None:
// a list without None whose last item is a bool, int, float or str goes
// through the typed constructor of its first item (an int list takes bools,
// a float list ints and bools; otherwise the first item's type is built
// strictly); any other list folds the supertype of its distinct types in
// order ("failed to determine supertype of A and B") and builds it strictly
// ("unexpected value while building Series of type X; found value of type Y:
// v", a list type without the found part).
struct LitType {
  enum class K { kNull, kBoolean, kInt64, kFloat64, kString, kDate, kDatetime, kList, kUInt64, kInt128 };
  K kind = K::kNull;
  std::string zone;            // kDatetime: empty when naive
  std::vector<LitType> inner;  // kList: the element type
};
inline bool operator==(const LitType& a, const LitType& b) {
  return a.kind == b.kind && a.zone == b.zone && a.inner.size() == b.inner.size() &&
         (a.inner.empty() || a.inner[0] == b.inner[0]);
}
inline LitType lit_type(LitType::K kind) { return LitType{kind, {}, {}}; }
inline LitType list_of(const LitType& inner) { return LitType{LitType::K::kList, {}, {inner}}; }
inline bool primitive(const LitType& t) {
  using K = LitType::K;
  return t.kind == K::kBoolean || t.kind == K::kInt64 || t.kind == K::kFloat64 || t.kind == K::kString ||
         t.kind == K::kInt128 || t.kind == K::kUInt64;
}
inline bool python_int(const LitType& t) {
  return t.kind == LitType::K::kInt64 || t.kind == LitType::K::kInt128 || t.kind == LitType::K::kUInt64;
}
// The dtype as a supertype error prints it (bool, i64, datetime[μs, UTC],
// list[str]) and as a build error does (Boolean, Datetime('μs', 'UTC')).
inline std::string short_name(const LitType& t) {
  using K = LitType::K;
  switch (t.kind) {
    case K::kNull: return "null";
    case K::kBoolean: return "bool";
    case K::kInt64: return "i64";
    case K::kFloat64: return "f64";
    case K::kString: return "str";
    case K::kDate: return "date";
    case K::kDatetime: return t.zone.empty() ? "datetime[\xce\xbcs]" : "datetime[\xce\xbcs, " + t.zone + "]";
    case K::kList: return "list[" + short_name(t.inner[0]) + "]";
    case K::kUInt64: return "u64";
    case K::kInt128: return "i128";
  }
  return "?";
}
inline std::string debug_name(const LitType& t) {
  using K = LitType::K;
  switch (t.kind) {
    case K::kNull: return "Null";
    case K::kBoolean: return "Boolean";
    case K::kInt64: return "Int64";
    case K::kFloat64: return "Float64";
    case K::kString: return "String";
    case K::kDate: return "Date";
    case K::kDatetime: return t.zone.empty() ? "Datetime('\xce\xbcs')" : "Datetime('\xce\xbcs', '" + t.zone + "')";
    case K::kList: return "List(" + debug_name(t.inner[0]) + ")";
    case K::kUInt64: return "UInt64";
    case K::kInt128: return "Int128";
  }
  return "?";
}

struct Literal {
  LitType type;               // the element type
  std::vector<Value> values;  // as the Series holds them (None for null)
};
// A literal value with its type (an AnyValue).
struct LitValue {
  LitType type;
  Value value;
};
inline std::int64_t micros_of(const Value& v) { return floor_div(v.instant_nanoseconds(), 1000); }
// A datetime as its literal: microseconds, a fixed offset in UTC.
inline Value literal_datetime(const Value& v) {
  const auto wall = floor_div(v.integer, 1000) * 1000;
  if (!v.offset_minutes) return Value::datetime(wall);
  if (v.text.empty()) return Value::datetime(wall - std::int64_t{*v.offset_minutes} * 60'000'000'000, 0, "UTC");
  return Value::datetime(wall, *v.offset_minutes, v.text);
}
inline Literal list_literal(const std::vector<Value>& items);
inline LitValue lit_value(const Value& v) {
  using K = LitType::K;
  switch (v.kind) {
    case Kind::kNone: return {lit_type(K::kNull), v};
    case Kind::kBool: return {lit_type(K::kBoolean), v};
    case Kind::kInt: return {lit_type(wide_int(v) ? K::kInt128 : K::kInt64), v};
    case Kind::kFloat: return {lit_type(K::kFloat64), v};
    case Kind::kString: return {lit_type(K::kString), v};
    case Kind::kDate: return {lit_type(K::kDate), v};
    case Kind::kDatetime: {
      auto value = literal_datetime(v);
      return {LitType{K::kDatetime, value.offset_minutes ? value.text : std::string{}, {}}, std::move(value)};
    }
    case Kind::kList:
    case Kind::kTuple: {
      auto inner = list_literal(v.items);
      return {list_of(inner.type), Value::list(std::move(inner.values))};
    }
    case Kind::kDict: break;
  }
  fail("TypeError", "unsupported list literal item");
}

// Polars' float display: integers with one decimal, others as printed.
inline std::string polars_float(double v) {
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
  if (v == std::floor(v) && std::fabs(v) < 999'999.0) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f", v);
    return buffer;
  }
  return python::float_repr(v);
}
inline std::string literal_item(const Value& v);
inline std::string polars_frame_float(double v);
inline std::string scalar_display(const Value& v);
// A literal value as Polars displays it (errors, plans).
inline std::string lit_display(const LitType& type, const Value& v) {
  if (v.kind == Kind::kNone) return "null";
  if (type.kind == LitType::K::kList) {
    std::vector<std::string> parts;
    for (const auto& item : v.items) parts.push_back(lit_display(type.inner[0], item));
    std::string out = "[";
    if (parts.size() <= 3) {
      for (std::size_t i = 0; i < parts.size(); ++i) out += (i ? ", " : "") + parts[i];
    } else {
      out += parts[0] + ", " + parts[1] + ", \xe2\x80\xa6 " + parts.back();
    }
    return out + "]";
  }
  // Round CE: a Float64 value as a frame shows it (1e6, 9.0072e15, 0.3).
  if (v.kind == Kind::kFloat) return polars_frame_float(v.real);
  return literal_item(v);
}

inline std::optional<LitType> lit_supertype(const LitType& l, const LitType& r) {
  using K = LitType::K;
  const auto inner = [](const LitType& a, const LitType& b) -> std::optional<LitType> {
    if (a == b) return a;
    const auto x = a.kind, y = b.kind;
    // Round CE: Int128 takes Int64 and Boolean, UInt64 with Int64 widens to
    // Float64 (probed in pl.lit lists).
    if (x == K::kInt128 && (y == K::kInt64 || y == K::kBoolean || y == K::kUInt64)) return a;
    if (x == K::kUInt64 && y == K::kBoolean) return a;
    if (x == K::kUInt64 && y == K::kInt64) return lit_type(K::kFloat64);
    if ((x == K::kInt128 || x == K::kUInt64) && y == K::kFloat64) return lit_type(K::kFloat64);
    if ((x == K::kInt64 && y == K::kBoolean) || ((x == K::kDate || x == K::kDatetime) && y == K::kInt64))
      return lit_type(K::kInt64);
    if ((x == K::kInt64 || x == K::kBoolean || x == K::kDate || x == K::kDatetime) && y == K::kFloat64)
      return lit_type(K::kFloat64);
    if (x == K::kDate && y == K::kDatetime) return b;
    if (y == K::kString) return lit_type(K::kString);
    if (y == K::kNull) return a;
    if (x == K::kDatetime && y == K::kDatetime && a.zone == b.zone) return b;
    if (x == K::kList && y == K::kList) {
      const auto s = lit_supertype(a.inner[0], b.inner[0]);
      if (!s) return std::nullopt;
      return list_of(*s);
    }
    return std::nullopt;
  };
  if (auto s = inner(l, r)) return s;
  return inner(r, l);
}

// A list value cast (strict=False) to another element type while a list
// Series is built: std::nullopt when the value becomes null.
inline std::optional<Value> cast_lit(const Value& v, const LitType& from, const LitType& to) {
  using K = LitType::K;
  if (v.kind == Kind::kNone || from.kind == K::kNull) return std::nullopt;
  if (from == to) return v;
  if (to.kind == K::kList || from.kind == K::kList) {
    if (to.kind != K::kList || from.kind != K::kList) return std::nullopt;
    std::vector<Value> items;
    for (const auto& item : v.items) items.push_back(cast_lit(item, from.inner[0], to.inner[0]).value_or(Value::none()));
    return Value::list(std::move(items));
  }
  const auto micros = [&] { return micros_of(v); };
  switch (to.kind) {
    case K::kString:
      if (from.kind == K::kDatetime) {
        auto text = pandas_filter_value_str(Value::datetime(floor_div(micros(), 1'000'000) * 1'000'000'000)).substr(0, 19);
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), ".%06lld", static_cast<long long>(micros() - floor_div(micros(), 1'000'000) * 1'000'000));
        return Value::of(text + buffer + (from.zone.empty() ? "" : "+00:00"));
      }
      if (from.kind == K::kDate) return Value::of(pandas_filter_value_str(v));
      if (from.kind == K::kFloat64) {
        if (std::isnan(v.real)) return Value::of("NaN");
        if (std::isinf(v.real)) return Value::of(v.real < 0 ? "-inf" : "inf");
      }
      return Value::of(literal_item(v));
    case K::kFloat64:
      if (python_int(from)) return Value::of(integer_real(v));
      if (from.kind == K::kDate) return Value::of(static_cast<double>(v.integer));
      if (from.kind == K::kBoolean) return Value::of(v.boolean ? 1.0 : 0.0);
      if (from.kind == K::kDatetime) return Value::of(static_cast<double>(micros()));
      if (from.kind == K::kString) {
        char* end = nullptr;
        const double d = std::strtod(v.text.c_str(), &end);
        if (v.text.empty() || *end) return std::nullopt;
        return Value::of(d);
      }
      return std::nullopt;
    case K::kInt64:
      if (python_int(from)) return wide_int(v) ? std::nullopt : std::optional<Value>(v);
      if (from.kind == K::kBoolean) return Value::of(std::int64_t{v.boolean});
      if (from.kind == K::kDate) return Value::of(v.integer);
      if (from.kind == K::kDatetime) return Value::of(micros());
      if (from.kind == K::kFloat64) {
        if (!std::isfinite(v.real) || std::fabs(v.real) >= 9.2e18) return std::nullopt;
        return Value::of(static_cast<std::int64_t>(v.real));
      }
      if (from.kind == K::kString) {
        std::int64_t parsed = 0;
        const auto r = std::from_chars(v.text.data(), v.text.data() + v.text.size(), parsed);
        if (v.text.empty() || r.ec != std::errc{} || r.ptr != v.text.data() + v.text.size()) return std::nullopt;
        return Value::of(parsed);
      }
      return std::nullopt;
    case K::kUInt64:
    case K::kInt128:
      if (python_int(from)) return to.kind == K::kUInt64 && !wide_int(v) && v.integer < 0 ? std::nullopt : std::optional<Value>(v);
      if (from.kind == K::kBoolean) return Value::of(std::int64_t{v.boolean});
      if (from.kind == K::kFloat64) {
        if (!std::isfinite(v.real) || v.real <= -1.0 || v.real >= 9.2e18) return std::nullopt;
        return Value::of(static_cast<std::int64_t>(v.real));
      }
      return std::nullopt;
    case K::kBoolean:
      if (python_int(from)) return Value::of(v.integer != 0);
      if (from.kind == K::kFloat64) return Value::of(v.real != 0.0);
      if (from.kind == K::kString && (v.text == "true" || v.text == "false")) return Value::of(v.text == "true");
      return std::nullopt;
    case K::kDatetime:
      if (from.kind == K::kDate) return Value::datetime(v.integer * 86'400'000'000'000, to.zone.empty() ? std::nullopt : std::optional<std::int32_t>{0}, to.zone.empty() ? "" : "UTC");
      if (from.kind == K::kInt64) return Value::datetime(v.integer * 1000, to.zone.empty() ? std::nullopt : std::optional<std::int32_t>{0}, to.zone.empty() ? "" : "UTC");
      if (from.kind == K::kDatetime) return v;
      return std::nullopt;
    case K::kDate:
      if (from.kind == K::kDatetime) return Value::date(floor_div(micros(), 86'400'000'000));
      if (from.kind == K::kInt64) return Value::date(v.integer);
      return std::nullopt;
    default: return std::nullopt;
  }
}

[[noreturn]] inline void build_error(const std::string& head) {
  fail("TypeError", head +
                        "\n\nThis error occurred with the following context stack:\n\t[1] while constructing Series "
                        "''\n\n\nHint: Try setting `strict=False` to allow passing data with mixed types.");
}
// The values built strictly as `type` (a null type takes anything as null;
// a Datetime type takes any datetime, keeping its microseconds).
inline std::vector<Value> build_strict(const LitType& type, const std::vector<LitValue>& values) {
  using K = LitType::K;
  std::vector<Value> out;
  if (type.kind == K::kNull) return std::vector<Value>(values.size());
  if (type.kind == K::kList) {
    bool valid = true;
    for (const auto& v : values) {
      if (v.type.kind == K::kNull) {
        out.emplace_back();
      } else if (v.type.kind != K::kList) {
        valid = false;
        out.emplace_back();
      } else if (type.inner[0].kind == K::kNull) {
        out.push_back(v.value);
      } else {
        std::vector<Value> items;
        for (const auto& item : v.value.items) {
          auto cast = cast_lit(item, v.type.inner[0], type.inner[0]);
          if (!cast && item.kind != Kind::kNone) valid = false;
          items.push_back(cast.value_or(Value::none()));
        }
        out.push_back(Value::list(std::move(items)));
      }
    }
    if (!valid) build_error("unexpected value while building Series of type " + debug_name(type));
    return out;
  }
  for (const auto& v : values) {
    if (v.type.kind == K::kNull) {
      out.emplace_back();
      continue;
    }
    if (v.type.kind != type.kind && !(type.kind == K::kInt128 && v.type.kind == K::kInt64))
      build_error("unexpected value while building Series of type " + debug_name(type) + "; found value of type " +
                  debug_name(v.type) + ": " + lit_display(v.type, v.value));
    out.push_back(v.value);
  }
  return out;
}
inline bool nested_null(const LitValue& v) {
  if (v.type.kind == LitType::K::kNull) return true;
  if (v.type.kind != LitType::K::kList) return false;
  return std::all_of(v.value.items.begin(), v.value.items.end(), [](const Value& x) { return x.kind == Kind::kNone; });
}
// Series.from_any_values(strict=True): the first value that is not a
// (nested) null decides the type.
inline Literal from_values_strict(const std::vector<LitValue>& values) {
  const auto first = std::find_if(values.begin(), values.end(), [](const LitValue& v) { return !nested_null(v); });
  LitType type;
  if (first != values.end()) {
    type = first->type;
  } else {
    const auto any = std::find_if(values.begin(), values.end(),
                                  [](const LitValue& v) { return v.type.kind != LitType::K::kNull; });
    if (any != values.end()) type = any->type;
  }
  return {type, build_strict(type, values)};
}
inline Literal list_literal(const std::vector<Value>& items) {
  using K = LitType::K;
  if (items.empty()) return {};
  std::vector<LitValue> values;
  std::size_t next = 0;
  while (next < items.size()) {
    values.push_back(lit_value(items[next++]));
    if (values.back().type.kind == K::kNull) break;
  }
  if (values.back().type.kind != K::kNull && primitive(values.back().type)) {
    // pl.Series(items): the first item's constructor, else strictly.
    for (; next < items.size(); ++next) values.push_back(lit_value(items[next]));
    const auto first = values.front().type.kind;
    const auto all = [&](auto&& ok) { return std::all_of(values.begin(), values.end(), ok); };
    if (first == K::kBoolean && all([](const LitValue& v) { return v.type.kind == K::kBoolean; }))
      return {lit_type(K::kBoolean), items};
    const auto int_or_bool = [](const LitValue& v) { return python_int(v.type) || v.type.kind == K::kBoolean; };
    if (python_int(values.front().type) && all(int_or_bool)) {
      const bool wide = std::any_of(values.begin(), values.end(), [](const LitValue& v) { return v.type.kind == K::kInt128; });
      // Round CE: past the Int64 range non-negative ints build UInt64, any
      // other list strictly from its first value.
      const bool unsigned_list = std::all_of(items.begin(), items.end(), [](const Value& v) { return v.kind != Kind::kInt || wide_int(v) || v.integer >= 0; });
      if (wide && !unsigned_list) return from_values_strict(values);
      Literal out{lit_type(wide ? K::kUInt64 : K::kInt64), {}};
      for (const auto& v : items) out.values.push_back(v.kind == Kind::kBool ? Value::of(std::int64_t{v.boolean}) : v);
      return out;
    }
    if (first == K::kFloat64 && all([&](const LitValue& v) { return v.type.kind == K::kFloat64 || int_or_bool(v); })) {
      Literal out{lit_type(K::kFloat64), {}};
      for (const auto& v : items)
        out.values.push_back(v.kind == Kind::kFloat ? v
                                                    : Value::of(v.kind == Kind::kBool ? (v.boolean ? 1.0 : 0.0)
                                                                                      : integer_real(v)));
      return out;
    }
    if (first == K::kString && all([](const LitValue& v) { return v.type.kind == K::kString; }))
      return {lit_type(K::kString), items};
    return from_values_strict(values);
  }
  for (; next < items.size(); ++next) values.push_back(lit_value(items[next]));
  std::vector<LitType> seen;
  for (const auto& v : values)
    if (std::find(seen.begin(), seen.end(), v.type) == seen.end()) seen.push_back(v.type);
  LitType type;
  for (const auto& t : seen) {
    const auto s = lit_supertype(type, t);
    if (!s) fail("TypeError", "failed to determine supertype of " + short_name(type) + " and " + short_name(t));
    type = *s;
  }
  return {type, build_strict(type, values)};
}
// A datetime literal as Polars prints it: wall clock in its zone with 3 or 6
// fractional digits only when needed, then the zone's abbreviation (round
// CB).
inline std::string literal_datetime_text(const Value& v) {
  const auto micros = floor_div(v.integer, 1000);
  const auto seconds = floor_div(micros, 1'000'000);
  const auto fraction = micros - seconds * 1'000'000;
  const auto days = floor_div(seconds, 86'400);
  const auto clock = seconds - days * 86'400;
  // Proleptic Gregorian civil date of a day number.
  const std::int64_t z = days + 719'468;
  const std::int64_t era = floor_div(z, 146'097);
  const auto doe = static_cast<unsigned>(z - era * 146'097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36'524 - doe / 146'096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400 + (month <= 2);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02u %02lld:%02lld:%02lld", static_cast<long long>(year), month,
                day, static_cast<long long>(clock / 3600), static_cast<long long>(clock / 60 % 60),
                static_cast<long long>(clock % 60));
  std::string out = buffer;
  if (fraction) {
    if (fraction % 1000 == 0) std::snprintf(buffer, sizeof(buffer), ".%03lld", static_cast<long long>(fraction / 1000));
    else std::snprintf(buffer, sizeof(buffer), ".%06lld", static_cast<long long>(fraction));
    out += buffer;
  }
  if (!v.offset_minutes) return out;
  if (v.text == "UTC" || v.text.empty()) return out + " UTC";
  try {
    return out + " " + ::minera::zone_database::info_at_utc(v.text, floor_div(v.instant_nanoseconds(), 1'000'000'000)).abbrev;
  } catch (const std::runtime_error&) {
    return out + " " + v.text;
  }
}
inline std::string literal_item(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "null";
    case Kind::kBool: return v.boolean ? "true" : "false";
    case Kind::kInt: return integer_text(v);
    case Kind::kFloat: return polars_float(v.real);
    case Kind::kString: return quote(v.text);
    case Kind::kDatetime: return literal_datetime_text(v);
    case Kind::kList:
    case Kind::kTuple: {
      std::string out = "[";
      for (std::size_t i = 0; i < v.items.size(); ++i) out += (i ? ", " : "") + literal_item(v.items[i]);
      return out + "]";
    }
    default: return pandas_filter_value_str(v);
  }
}
// The literal in a plan: up to three values, else the first two, an
// ellipsis and the last.
inline std::string literal_text(const Literal& literal) {
  return "[" + lit_display(list_of(literal.type), Value::list(literal.values)) + "]";
}
// The literal's dtype as an error names it.
inline std::string literal_dtype_name(const Literal& literal) { return debug_name(literal.type); }

// The literal's element kind as the is_in checks read it.
inline std::string lit_kind(const Literal& literal) {
  using K = LitType::K;
  switch (literal.type.kind) {
    case K::kNull: return "Null";
    case K::kBoolean: return "Boolean";
    case K::kInt64: return "Int64";
    case K::kFloat64: return "Float64";
    case K::kString: return "String";
    case K::kDate: return "Date";
    case K::kDatetime: return "Datetime";
    case K::kList: return "List";
    case K::kUInt64: return "UInt64";
    case K::kInt128: return "Int128";
  }
  return "List";
}

// A column dtype as the is_in check names it.
inline std::string is_in_dtype_name(const std::string& dtype) {
  if (enum_dtype(dtype)) return "Enum([...])";
  if (categorical_dtype(dtype)) return "Categorical";
  if (datetime_dtype(dtype) || dtype.rfind("Duration", 0) == 0) {
    auto unit = dtype_field(dtype, "time_unit").value_or("us");
    if (unit == "us") unit = "\xce\xbc" "s";
    auto out = std::string(datetime_dtype(dtype) ? "Datetime" : "Duration") + "('" + unit + "'";
    if (const auto zone = dtype_field(dtype, "time_zone")) out += ", '" + *zone + "'";
    return out + ")";
  }
  return dtype;
}
// A column dtype's short name, as Polars writes it in join and arithmetic
// errors: str, cat, enum, i16, u8, f32, bool, null, date, datetime[ms, Zone].
inline std::string short_dtype(const std::string& dtype) {
  if (dtype == "String") return "str";
  if (categorical_dtype(dtype)) return "cat";
  if (enum_dtype(dtype)) return "enum";
  if (dtype == "Boolean") return "bool";
  if (dtype == "Null") return "null";
  if (dtype == "Date") return "date";
  if (dtype.rfind("UInt", 0) == 0) return "u" + dtype.substr(4);
  if (dtype.rfind("Int", 0) == 0) return "i" + dtype.substr(3);
  if (dtype.rfind("Float", 0) == 0) return "f" + dtype.substr(5);
  if (datetime_dtype(dtype) || dtype.rfind("Duration", 0) == 0) {
    auto unit = dtype_field(dtype, "time_unit").value_or("us");
    if (unit == "us") unit = "\xce\xbc" "s";
    auto out = std::string(datetime_dtype(dtype) ? "datetime[" : "duration[") + unit;
    if (const auto zone = dtype_field(dtype, "time_zone")) out += ", " + *zone;
    return out + "]";
  }
  return dtype;
}
// is_in with a list literal: a String list checks String, Categorical and
// Enum data (casting to an Enum strictly), a number list integer and float
// data, a Boolean list Boolean data; a list of only nulls any data, and
// Null data any list.
// `categories`: an Enum column's categories in order; `suffix`: the resolved
// plan text appended to a failure; `shown`: the element type the error
// names when it is not the literal's own (an imploded scalar's
// Unknown(Int(1)) or Unknown(Float)).
inline void check_is_in(const std::string& dtype, const std::vector<std::string>& categories, const Literal& literal,
                        const std::string& suffix, const std::string* shown = nullptr) {
  // Round CE: a Null column takes any element type but a list.
  if (dtype == "Null" && literal.type.kind != LitType::K::kList) return;
  const auto l = lit_kind(literal);
  const bool numeric = integer_dtype(dtype) || float_dtype(dtype);
  const bool ok = l == "Null" ||
                  (l == "String" && (dtype == "String" || categorical_dtype(dtype) || enum_dtype(dtype))) ||
                  ((l == "Int64" || l == "Float64" || l == "UInt64" || l == "Int128") && numeric) || (l == "Boolean" && dtype == "Boolean") ||
                  (l == "Datetime" && datetime_dtype(dtype)) || (l == "Date" && dtype == "Date");
  if (!ok)
    fail("InvalidOperationError", "'is_in' cannot check for List(" + (shown ? *shown : literal_dtype_name(literal)) + ") values in " +
                                      is_in_dtype_name(dtype) + " data" + suffix);
  // A datetime list holds microseconds: a millisecond column refuses it, a
  // nanosecond one compares the raw numbers (round CB).
  if (l == "Datetime" && unit_nanoseconds(dtype) == 1'000'000)
    fail("InvalidOperationError", "'is_in' cannot check for Microseconds precision values in Milliseconds Datetime "
                                  "data" + suffix);
  if (l == "String" && enum_dtype(dtype)) enum_is_in(categories, literal.values, suffix);
}
// The is_in expression as a resolved plan prints it once the literal and
// the column meet in their supertype: a narrower integer column casts to
// Int64 (UInt64 to Int128), an integer column strictly to Float64 for a
// float list, a Float32 column to Float64, and an Int64 list on float data
// holds floats.
inline std::string coerced_is_in(const std::string& column, const std::string& dtype, const Literal& literal) {
  std::string cast;
  Literal shown = literal;
  // Round CE: a Null column takes the element type (none for a null list).
  if (dtype == "Null" && literal.type.kind != LitType::K::kNull)
    cast = ".cast(" + debug_name(literal.type) + ")";
  else if (lit_kind(literal) == "Int64" && integer_dtype(dtype) && dtype != "Int64")
    cast = dtype == "UInt64" ? ".cast(Int128)" : ".cast(Int64)";
  // Round CE: a UInt64 list meets signed columns in Int128, narrower
  // unsigned ones in UInt64; an Int128 list meets any integer column there.
  else if (lit_kind(literal) == "UInt64" && integer_dtype(dtype) && dtype != "UInt64")
    cast = dtype.rfind("UInt", 0) == 0 ? ".cast(UInt64)" : ".cast(Int128)";
  else if (lit_kind(literal) == "Int128" && integer_dtype(dtype))
    cast = ".cast(Int128)";
  else if (lit_kind(literal) == "Float64" && integer_dtype(dtype))
    cast = ".strict_cast(Float64)";
  else if ((lit_kind(literal) == "Int64" || lit_kind(literal) == "Float64" || lit_kind(literal) == "UInt64" ||
            lit_kind(literal) == "Int128") && float_dtype(dtype)) {
    if (dtype != "Float64") cast = ".cast(Float64)";
    shown.type = lit_type(LitType::K::kFloat64);
    for (auto& v : shown.values)
      if (v.kind == Kind::kInt) v = Value::of(integer_real(v));
  }
  return "col(" + quote(column) + ")" + cast + ".is_in(" + literal_text(shown) + ")";
}
// Round CE: col(x).is_in(scalar) as a resolved plan prints it: the scalar
// imploded and cast to the supertype of the column and the literal (an int
// widens an integer column to the smallest type holding both, a float casts
// it strictly to Float64), a Null column cast to the literal's type;
// std::nullopt where the form is not modelled.
inline std::optional<std::string> coerced_scalar_is_in(const std::string& column, const std::string& dtype, const Value& v) {
  const auto c = "col(" + quote(column) + ")";
  const auto item = [&] {
    if (v.kind == Kind::kInt) return integer_text(v);
    if (v.kind == Kind::kFloat) return polars_frame_float(v.real);
    if (v.kind == Kind::kBool) return std::string(v.boolean ? "true" : "false");
    return scalar_display(v);
  };
  const auto form = [&](const std::string& cast, const std::string& list_type) {
    return c + cast + ".is_in([[" + item() + "]" + (list_type.empty() ? "" : ".cast(List(" + list_type + "))") + "])";
  };
  if (dtype == "Null") {
    if (v.kind == Kind::kInt) return form(".cast(Unknown(Int(" + integer_text(v) + ")))", "");
    if (v.kind == Kind::kFloat) return form(".cast(Unknown(Float))", "");
    if (v.kind == Kind::kBool || v.kind == Kind::kDatetime) return form(".cast(" + debug_name(lit_value(v).type) + ")", "");
    return std::nullopt;
  }
  if (dtype == "Boolean" && v.kind == Kind::kBool) return form("", "");
  if (float_dtype(dtype) && (v.kind == Kind::kInt || v.kind == Kind::kFloat)) return form("", dtype);
  if (!integer_dtype(dtype)) return std::nullopt;
  if (v.kind == Kind::kFloat) return form(".strict_cast(Float64)", "Float64");
  if (v.kind != Kind::kInt) return std::nullopt;
  const bool is_unsigned = dtype.rfind("UInt", 0) == 0;
  const int bits = std::stoi(dtype.substr(is_unsigned ? 4 : 3));
  if (wide_int(v)) {
    if (!is_unsigned) return form(".strict_cast(Float64)", "Float64");
    return form(bits == 64 ? "" : ".cast(UInt64)", "UInt64");
  }
  const auto i = v.integer;
  const auto signed_bits = [](std::int64_t x) {
    for (int b = 8; b < 64; b *= 2)
      if (x >= -(std::int64_t{1} << (b - 1)) && x < (std::int64_t{1} << (b - 1))) return b;
    return 64;
  };
  const auto unsigned_bits = [](std::int64_t x) {
    for (int b = 8; b < 64; b *= 2)
      if (x < (std::int64_t{1} << b)) return b;
    return 64;
  };
  std::string type;
  int width = bits;
  bool strict = false;
  if (!is_unsigned) {
    width = std::max(bits, signed_bits(i));
    type = "Int" + std::to_string(width);
  } else if (i >= 0) {
    width = std::max(bits, unsigned_bits(i));
    type = "UInt" + std::to_string(width);
  } else {
    strict = bits == 64;
    width = std::max(std::min(bits * 2, 64), signed_bits(i));
    type = "Int" + std::to_string(width);
  }
  if (type == dtype) return form("", type);
  return form((strict ? ".strict_cast(" : ".cast(") + type + ")", type);
}
// A present cell `is_in` the literal: numbers by value (NaN in NaN), str by
// text, temporal values by instant; a null never is.
inline bool literal_has(const Literal& literal, const Cell& cell, const std::string& dtype) {
  if (null_cell(cell)) return false;
  const auto value = python_value(cell, dtype);
  for (const auto& v : literal.values) {
    if (v.kind == Kind::kNone) continue;
    if (lit_kind(literal) == "String") {
      if (v.kind == Kind::kString && v.text == cell->value) return true;
    } else if (lit_kind(literal) == "Int64" || lit_kind(literal) == "Float64" || lit_kind(literal) == "UInt64" ||
               lit_kind(literal) == "Int128") {
      if (value.kind == Kind::kInt && v.kind == Kind::kInt) {
        if (value.integer == v.integer && value.text == v.text) return true;
        continue;
      }
      const double x = value.kind == Kind::kInt ? integer_real(value) : value.real;
      const double y = v.kind == Kind::kInt ? integer_real(v) : v.real;
      if (x == y || (std::isnan(x) && std::isnan(y))) return true;
    } else if (lit_kind(literal) == "Datetime") {
      if (v.kind == Kind::kDatetime && physical_of(*cell, dtype) == floor_div(v.instant_nanoseconds(), 1000))
        return true;
    } else if (same_key(value, v)) {
      return true;
    }
  }
  return false;
}

// A literal value cast (strict=False) to an activity dtype, as the prefix
// and suffix lists are: out of range, unparsable or unsupported is null.
inline std::optional<Value> cast_to(const Value& v, const std::string& dtype, const std::set<std::string>* categories) {
  if (v.kind == Kind::kNone) return std::nullopt;
  const auto in_float = [&](double x) {
    return Value::of(dtype == "Float32" ? static_cast<double>(static_cast<float>(x)) : x);
  };
  // Round CC: a datetime value casts as its microseconds (a Date column
  // takes its day, a Datetime column its microseconds in the column's unit),
  // a date value as its days (a Datetime column its UTC midnight).
  if (v.kind == Kind::kDatetime || v.kind == Kind::kDate) {
    using K = LitType::K;
    const bool date = v.kind == Kind::kDate;
    const std::int64_t micros = date ? v.integer * 86'400'000'000 : micros_of(v);
    if (dtype == "String" || categorical_dtype(dtype) || enum_dtype(dtype)) {
      const LitType from = date ? lit_type(K::kDate) : LitType{K::kDatetime, v.offset_minutes ? v.text : "", {}};
      auto text = cast_lit(v, from, lit_type(K::kString))->text;
      if (categories && !categories->count(text)) return std::nullopt;
      return Value::of(std::move(text));
    }
    if (integer_dtype(dtype)) {
      const auto i = date ? v.integer : micros;
      const bool unsigned_type = dtype.rfind("UInt", 0) == 0;
      const int bits = std::stoi(dtype.substr(unsigned_type ? 4 : 3));
      if (unsigned_type ? (i < 0 || (bits < 64 && i >= (std::int64_t{1} << bits)))
                        : (bits < 64 && (i < -(std::int64_t{1} << (bits - 1)) || i >= (std::int64_t{1} << (bits - 1)))))
        return std::nullopt;
      return Value::of(i);
    }
    if (float_dtype(dtype)) return in_float(static_cast<double>(date ? v.integer : micros));
    if (dtype == "Date") return date ? v : Value::date(floor_div(floor_div(v.integer, 1000), 86'400'000'000));
    if (datetime_dtype(dtype)) {
      const auto unit = unit_nanoseconds(dtype);
      return Value::of(unit == 1 ? micros * 1000 : unit == 1'000'000 ? floor_div(micros, 1000) : micros);
    }
    return std::nullopt;
  }
  const auto as_integer = [&](const Value& x) -> std::optional<std::int64_t> {
    if (x.kind == Kind::kInt) return x.integer;
    if (x.kind == Kind::kBool) return std::int64_t{x.boolean};
    if (x.kind == Kind::kFloat) {
      if (!std::isfinite(x.real) || std::fabs(x.real) >= 9.2e18) return std::nullopt;
      return static_cast<std::int64_t>(x.real);
    }
    if (x.kind == Kind::kString) {
      std::int64_t parsed = 0;
      const auto* begin = x.text.data() + (x.text.starts_with('+') ? 1 : 0);
      const auto r = std::from_chars(begin, x.text.data() + x.text.size(), parsed);
      if (x.text.empty() || r.ec != std::errc{} || r.ptr != x.text.data() + x.text.size()) return std::nullopt;
      return parsed;
    }
    return std::nullopt;
  };
  if (dtype == "String" || categorical_dtype(dtype) || enum_dtype(dtype)) {
    const std::string text = v.kind == Kind::kString ? v.text : literal_item(v);
    if (categories && !categories->count(text)) return std::nullopt;
    return Value::of(text);
  }
  if (integer_dtype(dtype)) {
    const auto i = as_integer(v);
    if (!i) return std::nullopt;
    const bool unsigned_type = dtype.rfind("UInt", 0) == 0;
    const int bits = std::stoi(dtype.substr(unsigned_type ? 4 : 3));
    if (unsigned_type ? (*i < 0 || (bits < 64 && *i >= (std::int64_t{1} << bits)))
                      : (bits < 64 && (*i < -(std::int64_t{1} << (bits - 1)) || *i >= (std::int64_t{1} << (bits - 1)))))
      return std::nullopt;
    return Value::of(*i);
  }
  if (float_dtype(dtype)) {
    if (v.kind == Kind::kFloat) return in_float(v.real);
    if (v.kind == Kind::kInt) return in_float(static_cast<double>(v.integer));
    if (v.kind == Kind::kBool) return Value::of(v.boolean ? 1.0 : 0.0);
    return std::nullopt;
  }
  if (dtype == "Boolean") {
    if (v.kind == Kind::kBool) return v;
    if (v.kind == Kind::kInt) return Value::of(v.integer != 0);
    return std::nullopt;
  }
  if (dtype == "Date") {
    if (v.kind == Kind::kString) {
      const auto& t = v.text;
      if (t.size() != 10 || t[4] != '-' || t[7] != '-') return std::nullopt;
      const auto parsed = parse_timestamp_exact(t);
      if (!parsed) return std::nullopt;
      return Value::date(floor_div(parsed->seconds, 86'400));
    }
    if (v.kind == Kind::kBool) return std::nullopt;
    const auto i = as_integer(v);
    if (!i) return std::nullopt;
    return Value::date(*i);
  }
  if (datetime_dtype(dtype)) {
    if (v.kind == Kind::kString || v.kind == Kind::kBool) return std::nullopt;
    const auto i = as_integer(v);
    if (!i) return std::nullopt;
    return Value::of(*i);  // the physical value in the column's unit
  }
  return std::nullopt;
}
// Rust's Display of a double: the shortest digits that read back, without
// an exponent.
inline std::string rust_float(double v) {
  char buffer[64];
  for (int precision = 1; precision <= 17; ++precision) {
    std::snprintf(buffer, sizeof(buffer), "%.*e", precision - 1, v);
    if (std::strtod(buffer, nullptr) == v) break;
  }
  std::string text = buffer;
  const bool negative = text[0] == '-';
  if (negative) text.erase(0, 1);
  const auto e = text.find('e');
  const int exponent = std::stoi(text.substr(e + 1));
  std::string digits;
  for (const char c : text.substr(0, e))
    if (c != '.') digits += c;
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  const int point = exponent + 1;
  std::string out;
  if (point <= 0) out = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
  else if (point >= static_cast<int>(digits.size())) out = digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0');
  else out = digits.substr(0, static_cast<std::size_t>(point)) + "." + digits.substr(static_cast<std::size_t>(point));
  return (negative ? "-" : "") + out;
}
// Polars' float display in a frame or literal: integers with one decimal,
// long values in scientific notation with four decimals or rounded to six.
inline std::string polars_frame_float(double v) {
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
  char buffer[64];
  const double a = std::fabs(v);
  if (v == std::floor(v) && a < 999'999.0) {
    std::snprintf(buffer, sizeof(buffer), "%.1f", v);
    return buffer;
  }
  const auto scientific = [&](int decimals) {
    std::snprintf(buffer, sizeof(buffer), "%.*e", decimals, v);
    std::string t = buffer;
    const auto e = t.find('e');
    const int exponent = std::stoi(t.substr(e + 1));
    std::string mantissa = t.substr(0, e);
    if (decimals < 0) {  // shortest
      mantissa = rust_float(std::strtod(t.substr(0, e).c_str(), nullptr));
    }
    return mantissa + "e" + std::to_string(exponent);
  };
  const auto display = rust_float(v);
  if (display.size() > 9) {
    if (!(a >= 0.000001 && a <= 999'999.0) || a > 999'999.0) return scientific(4);
    std::snprintf(buffer, sizeof(buffer), "%.6f", v);
    std::string t = buffer;
    if (t.back() == '0') {
      while (t.back() == '0') t.pop_back();
      if (t.back() == '.') t += '0';
    }
    return t;
  }
  if (v == std::floor(v)) {
    // {:e}: the shortest mantissa.
    std::snprintf(buffer, sizeof(buffer), "%.16e", v);
    std::string t = buffer;
    const auto e = t.find('e');
    std::string mantissa = rust_float(std::strtod(t.substr(0, e).c_str(), nullptr));
    if (mantissa.size() > 2 && mantissa.ends_with(".0")) mantissa.resize(mantissa.size() - 2);
    return mantissa + "e" + std::to_string(std::stoi(t.substr(e + 1)));
  }
  return display;
}
// A wall clock (seconds and nanoseconds since the epoch) as Polars prints a
// datetime: the fraction in 3, 6 or 9 digits when present.
inline std::string wall_text(std::int64_t seconds, std::int64_t nanoseconds) {
  const auto days = floor_div(seconds, 86'400);
  const auto clock = seconds - days * 86'400;
  auto text = pandas_filter_value_str(Value::date(days));
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), " %02lld:%02lld:%02lld", static_cast<long long>(clock / 3600),
                static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
  text += buffer;
  if (nanoseconds) {
    if (nanoseconds % 1'000'000 == 0) std::snprintf(buffer, sizeof(buffer), ".%03lld", static_cast<long long>(nanoseconds / 1'000'000));
    else if (nanoseconds % 1000 == 0) std::snprintf(buffer, sizeof(buffer), ".%06lld", static_cast<long long>(nanoseconds / 1000));
    else std::snprintf(buffer, sizeof(buffer), ".%09lld", static_cast<long long>(nanoseconds));
    text += buffer;
  }
  return text;
}
// A cast pattern value as the plan prints it in the column's dtype.
inline std::string column_value_text(const std::optional<Value>& v, const std::string& dtype) {
  if (!v) return "null";
  if (dtype == "String" || categorical_dtype(dtype) || enum_dtype(dtype)) return quote(v->text);
  if (integer_dtype(dtype)) return std::to_string(v->integer);
  if (float_dtype(dtype)) return polars_frame_float(v->real);
  if (dtype == "Boolean") return v->boolean ? "true" : "false";
  if (dtype == "Date") return pandas_filter_value_str(*v);
  if (datetime_dtype(dtype)) {
    const auto unit = unit_nanoseconds(dtype);
    const auto per_second = 1'000'000'000 / unit;
    const auto seconds = floor_div(v->integer, per_second);
    const auto nanoseconds = (v->integer - seconds * per_second) * unit;
    const auto zone = dtype_field(dtype, "time_zone");
    if (!zone) return wall_text(seconds, nanoseconds);
    try {
      const auto info = ::minera::zone_database::info_at_utc(*zone, seconds);
      return wall_text(seconds + info.offset_seconds, nanoseconds) + " " + info.abbrev;
    } catch (const std::runtime_error&) {
      return wall_text(seconds, nanoseconds) + " " + *zone;
    }
  }
  return literal_item(*v);
}
// Round CC: an activity column and a pattern literal compare when their
// element types have a supertype (Polars 1.44): a String column with
// anything, a str literal with anything, Categorical and Enum only with str,
// Boolean with numbers, integers with Date or Datetime only at 32 bits and
// more (UInt32, UInt64, Int32, Int64), temporal types with each other (two
// datetimes when both are naive or share their zone); a nested list never.
inline bool comparable(const std::string& dtype, const LitType& lit) {
  using K = LitType::K;
  const auto k = lit.kind;
  if (k == K::kNull || dtype == "String") return true;
  if (k == K::kList) return false;
  if (categorical_dtype(dtype) || enum_dtype(dtype)) return k == K::kString;
  if (k == K::kString) return true;
  if (dtype == "Boolean") return k == K::kInt64 || k == K::kFloat64 || k == K::kBoolean;
  if (integer_dtype(dtype)) {
    if (k == K::kDate || k == K::kDatetime)
      return dtype == "Int32" || dtype == "Int64" || dtype == "UInt32" || dtype == "UInt64";
    return true;
  }
  if (float_dtype(dtype)) return true;
  if (dtype == "Date") return k != K::kBoolean;
  if (datetime_dtype(dtype)) {
    if (k == K::kBoolean) return false;
    if (k != K::kDatetime) return true;
    const auto zone = dtype_field(dtype, "time_zone");
    return zone ? *zone == lit.zone : lit.zone.empty();
  }
  return false;
}
// A cell equals a cast pattern value.
inline bool cell_equals(const EventAttribute& cell, const Value& v, const std::string& dtype) {
  if (dtype == "Date") return v.kind == Kind::kDate && physical_of(cell, dtype) == v.integer;
  if (datetime_dtype(dtype)) return v.kind == Kind::kInt && physical_of(cell, dtype) == v.integer;
  if (integer_dtype(dtype)) return v.kind == Kind::kInt && integer_of(cell) == v.integer;
  if (float_dtype(dtype)) {
    const double x = real_of(cell);
    return v.kind == Kind::kFloat && (x == v.real || (std::isnan(x) && std::isnan(v.real)));
  }
  if (dtype == "Boolean") return v.kind == Kind::kBool && true_of(cell) == v.boolean;
  return v.kind == Kind::kString && cell.value == v.text;
}


// Round CD: a Python argument of the Polars table filters (an EventAttribute
// as their bank and routes carry it) as a filter value: a datetime with its
// fixed UTC offset (an offset of 0 is datetime.timezone.utc), a ten-character
// date text a date, containers item by item (a dict by its keys' text).
inline Value value_of(const EventAttribute& a) {
  if (a.is_null || is_native_na_attribute(a)) return Value::none();
  if (a.container == AttributeContainerKind::kContainer) {
    std::vector<std::pair<Value, Value>> entries;
    for (const auto& child : a.children) entries.emplace_back(Value::of(child.key), value_of(child));
    return Value::dict(std::move(entries));
  }
  if (a.container != AttributeContainerKind::kScalar) {
    std::vector<Value> items;
    for (const auto& child : a.children) items.push_back(value_of(child));
    return a.container == AttributeContainerKind::kTuple ? Value::tuple(std::move(items)) : Value::list(std::move(items));
  }
  const auto scalar = python::from_attribute(a);
  switch (scalar.kind) {
    case python::Scalar::Kind::kBool: return Value::of(scalar.boolean);
    case python::Scalar::Kind::kInt: {
      if (const auto u = wide_cell(a)) return wide_value(*u);
      return Value::of(integer_of(a));
    }
    case python::Scalar::Kind::kFloat: return Value::of(scalar.real);
    case python::Scalar::Kind::kDate: {
      const auto& t = scalar.date;
      const auto* native = std::get_if<NativeDate>(&a.typed_value);
      if ((native ? native->value : a.value).size() == 10) return Value::date(floor_div(t.seconds, 86'400));
      if (!t.timezone_aware) return Value::datetime(t.seconds * 1'000'000'000 + t.nanoseconds);
      const std::int64_t wall = (t.seconds + std::int64_t{t.offset_minutes} * 60) * 1'000'000'000 + t.nanoseconds;
      return Value::datetime(wall, t.offset_minutes, t.offset_minutes == 0 ? "UTC" : "");
    }
    case python::Scalar::Kind::kNone:
    case python::Scalar::Kind::kNA: return Value::none();
    default: return Value::of(a.value);
  }
}
// The Python values Polars hands out for cells of one column (to_list()).
inline std::vector<Value> python_values(const std::vector<EventAttribute>& cells, const std::string& dtype) {
  std::vector<Value> out;
  for (const auto& c : cells) out.push_back(python_value(Cell(c), dtype));
  return out;
}
// The Literal of a list or tuple argument (a dict: its keys).
inline Literal literal_of(const Value& v) {
  if (v.kind == Kind::kDict) {
    std::vector<Value> keys;
    for (const auto& [k, _] : v.entries) keys.push_back(k);
    return list_literal(keys);
  }
  return list_literal(v.items);
}
// Round CE: a list or tuple argument as pl.lit builds it (its construction
// TypeError, its element type, its plan display).
inline Literal argument_literal(const EventAttribute& a) { return literal_of(value_of(a)); }
inline std::optional<std::string> argument_literal_error(const EventAttribute& a) {
  try {
    (void)argument_literal(a);
  } catch (const PolarsSequenceFilterError& e) {
    return e.message;
  }
  return std::nullopt;
}
inline std::string literal_list_display(const Literal& l) { return lit_display(list_of(l.type), Value::list(l.values)); }

// Round CD: col(x) OP v for a column dtype and a Python scalar v, as Polars
// 1.44 types and evaluates it (probed per dtype, literal kind and operator).
enum class CompareOp { kEq, kGe, kLe, kGt, kLt };
inline std::string op_text(CompareOp op) {
  switch (op) {
    case CompareOp::kEq: return "==";
    case CompareOp::kGe: return ">=";
    case CompareOp::kLe: return "<=";
    case CompareOp::kGt: return ">";
    case CompareOp::kLt: return "<";
  }
  return "==";
}
inline CompareOp compare_op(const std::string& text) {
  if (text == ">=") return CompareOp::kGe;
  if (text == "<=") return CompareOp::kLe;
  if (text == ">") return CompareOp::kGt;
  if (text == "<") return CompareOp::kLt;
  return CompareOp::kEq;
}
// A Python scalar as the uncoerced plan prints it: dyn int: 1, dyn float:
// 0.1, "a", true, null, 2021-01-01, 2021-01-01 01:00:00.001 UTC.
inline std::string scalar_display(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "null";
    case Kind::kBool: return v.boolean ? "true" : "false";
    case Kind::kInt: return "dyn int: " + integer_text(v);
    case Kind::kFloat: return "dyn float: " + polars_dyn_float(v.real);
    case Kind::kString: return quote(v.text);
    case Kind::kDate: return pandas_filter_value_str(v);
    case Kind::kDatetime: return literal_datetime_text(literal_datetime(v));
    default: break;
  }
  throw std::invalid_argument("Unsupported native Polars comparison literal");
}
inline bool scalar_literal(const Value& v) {
  return v.kind != Kind::kList && v.kind != Kind::kTuple && v.kind != Kind::kDict;
}
// The failure of a comparison: kResolve while the plan resolves (str with a
// temporal side), kCompute when the filter runs (with its expression).
struct ScalarCheck {
  enum class Stage { kOk, kResolve, kCompute };
  Stage stage = Stage::kOk;
  std::string type, message;
  [[nodiscard]] bool ok() const { return stage == Stage::kOk; }
};
inline const char* kTemporalString =
    "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or "
    "compare to a temporal column)";
inline std::string comparison_expression(const std::string& column, CompareOp op, const Value& v) {
  return "[(col(" + quote(column) + ")) " + op_text(op) + " (" + scalar_display(v) + ")]";
}
// The ComputeError suffix naming the failing comparison.
inline std::string expression_suffix(const std::string& column, CompareOp op, const Value& v) {
  return "\n\nThis error occurred in the following expression:\n\t" + comparison_expression(column, op, v) + "\n";
}
inline ScalarCheck check_scalar(const std::string& column, const std::string& dtype, const Value& v, CompareOp op,
                                const std::vector<std::string>* categories = nullptr) {
  using Stage = ScalarCheck::Stage;
  if (v.kind == Kind::kNone || dtype == "Null") return {};
  const bool str = v.kind == Kind::kString, temporal = v.kind == Kind::kDate || v.kind == Kind::kDatetime;
  const bool number = v.kind == Kind::kInt || v.kind == Kind::kFloat;
  const auto int_name = [&] {
    return v.integer >= std::numeric_limits<std::int32_t>::min() && v.integer <= std::numeric_limits<std::int32_t>::max() ? "i32" : "i64";
  };
  const auto schema = [&] {
    return ScalarCheck{Stage::kCompute, "SchemaError",
                       "could not evaluate " + (op == CompareOp::kEq ? std::string() : "'" + op_text(op) + "' ") +
                           "comparison between series '" + column + "' of dtype: " + is_in_dtype_name(dtype) +
                           " and series 'literal' of dtype: " + debug_name(lit_value(v).type)};
  };
  const auto numeric_string = [&](const std::string& name) {
    return ScalarCheck{Stage::kCompute, "ComputeError", "cannot compare string with numeric type (" + name + ")"};
  };
  // Round CE: an int past the Int64 range is a UInt64 literal.
  if (wide_int(v)) {
    if (dtype == "String") return numeric_string("u64");
    if (categorical_dtype(dtype) || enum_dtype(dtype)) return {Stage::kCompute, "ComputeError", "cannot compare categorical with u64"};
    return {};
  }
  if (dtype == "String") {
    if (number) return numeric_string(v.kind == Kind::kInt ? int_name() : "f64");
    if (temporal) return {Stage::kResolve, "InvalidOperationError", kTemporalString};
    return {};
  }
  if (categorical_dtype(dtype) || enum_dtype(dtype)) {
    if (str) {
      if (enum_dtype(dtype) && op != CompareOp::kEq && categories &&
          std::find(categories->begin(), categories->end(), v.text) == categories->end())
        return {Stage::kCompute, "InvalidOperationError", "conversion from `str` to `enum` failed for value \"" + v.text + "\""};
      return {};
    }
    const std::string name = v.kind == Kind::kInt ? int_name() : v.kind == Kind::kFloat ? "f64" : short_name(lit_value(v).type);
    return {Stage::kCompute, "ComputeError", "cannot compare categorical with " + name};
  }
  if (dtype == "Boolean") return temporal ? schema() : ScalarCheck{};
  if (integer_dtype(dtype)) {
    if (str) return numeric_string(short_dtype(dtype));
    if (temporal && dtype != "Int32" && dtype != "Int64" && dtype != "UInt32" && dtype != "UInt64") return schema();
    return {};
  }
  if (float_dtype(dtype)) return str ? numeric_string(short_dtype(dtype)) : ScalarCheck{};
  if (dtype == "Date" || datetime_dtype(dtype)) {
    if (str) return {Stage::kResolve, "InvalidOperationError", kTemporalString};
    if (v.kind == Kind::kBool) return schema();
    if (v.kind == Kind::kDatetime && datetime_dtype(dtype)) {
      const auto zone = dtype_field(dtype, "time_zone");
      const auto lit = lit_value(v).type;
      if (zone ? *zone != lit.zone : !lit.zone.empty()) return schema();
    }
    return {};
  }
  throw std::invalid_argument("Unsupported native Polars comparison dtype: " + dtype);
}
// A signed 128-bit integer (two's complement, hi:lo) for the exact integer
// comparisons below: UInt64 cells above the Int64 range and nanoseconds
// from micro- or day-scaled literals. Portable (MSVC has no __int128).
struct Wide {
  std::int64_t hi = 0;
  std::uint64_t lo = 0;
  constexpr Wide() = default;
  template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
  constexpr Wide(T v)  // NOLINT(google-explicit-constructor)
      : hi(std::is_signed_v<T> && v < 0 ? -1 : 0),
        lo(std::is_signed_v<T> ? static_cast<std::uint64_t>(static_cast<std::int64_t>(v)) : static_cast<std::uint64_t>(v)) {}
  friend constexpr bool operator==(const Wide& a, const Wide& b) { return a.hi == b.hi && a.lo == b.lo; }
  friend constexpr bool operator<(const Wide& a, const Wide& b) { return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }
  friend constexpr bool operator>(const Wide& a, const Wide& b) { return b < a; }
  friend constexpr bool operator<=(const Wide& a, const Wide& b) { return !(b < a); }
  friend constexpr bool operator>=(const Wide& a, const Wide& b) { return !(a < b); }
};
// a + b.
constexpr Wide wide_add(const Wide& a, std::int64_t b) {
  const Wide w(b);
  Wide r;
  r.lo = a.lo + w.lo;
  r.hi = static_cast<std::int64_t>(static_cast<std::uint64_t>(a.hi) + static_cast<std::uint64_t>(w.hi) + (r.lo < a.lo ? 1U : 0U));
  return r;
}
// The exact product a * b.
constexpr Wide wide_mul(std::int64_t a, std::int64_t b) {
  const bool negative = (a < 0) != (b < 0);
  const auto magnitude = [](std::int64_t x) { return x < 0 ? 0 - static_cast<std::uint64_t>(x) : static_cast<std::uint64_t>(x); };
  const std::uint64_t x = magnitude(a), y = magnitude(b), mask = 0xffffffffU;
  const std::uint64_t x0 = x & mask, x1 = x >> 32, y0 = y & mask, y1 = y >> 32;
  const std::uint64_t p00 = x0 * y0, p01 = x0 * y1, p10 = x1 * y0, p11 = x1 * y1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & mask) + (p10 & mask);
  std::uint64_t lo = (p00 & mask) | (mid << 32);
  std::uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  if (negative) {
    lo = ~lo + 1;
    hi = ~hi + (lo == 0 ? 1U : 0U);
  }
  Wide r;
  r.hi = static_cast<std::int64_t>(hi);
  r.lo = lo;
  return r;
}
// The nearest double (values in the Int64 and UInt64 ranges).
inline double wide_double(const Wide& x) {
  if (x.hi == 0) return static_cast<double>(x.lo);
  if (x.hi == -1 && (x.lo >> 63) != 0) return static_cast<double>(static_cast<std::int64_t>(x.lo));
  return static_cast<double>(x.hi) * 18446744073709551616.0 + static_cast<double>(x.lo);
}
// Where an integer lies against an integer dtype's range: 0 inside, 1
// above its maximum, -1 below its minimum.
inline int integer_range_side(std::int64_t x, const std::string& dtype) {
  const bool is_unsigned = dtype.rfind("UInt", 0) == 0;
  const int bits = std::stoi(dtype.substr(is_unsigned ? 4 : 3));
  if (is_unsigned) {
    if (x < 0) return -1;
    return bits < 64 && static_cast<std::uint64_t>(x) > (std::uint64_t{1} << bits) - 1 ? 1 : 0;
  }
  if (bits >= 64) return 0;
  const std::int64_t hi = (std::int64_t{1} << (bits - 1)) - 1, lo = -hi - 1;
  return x > hi ? 1 : x < lo ? -1 : 0;
}
// An integer cell as a 128-bit value (UInt64 cells above the Int64 range).
inline Wide wide_of(const EventAttribute& cell) {
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return Wide(*u);
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return Wide(*i);
  if (!cell.value.empty() && cell.value[0] != '-') {
    std::uint64_t u = 0;
    const auto r = std::from_chars(cell.value.data(), cell.value.data() + cell.value.size(), u);
    if (r.ec == std::errc{}) return Wide(u);
  }
  return Wide(integer_of(cell));
}
// Polars' float order: NaN equals NaN and sorts above every number.
inline int total_order(double x, double y) {
  const bool a = std::isnan(x), b = std::isnan(y);
  if (a || b) return a == b ? 0 : a ? 1 : -1;
  return x < y ? -1 : x > y ? 1 : 0;
}
template <class T>
inline int order_of(const T& x, const T& y) { return x < y ? -1 : y < x ? 1 : 0; }
inline int apply_order(int c, CompareOp op) {
  switch (op) {
    case CompareOp::kEq: return c == 0;
    case CompareOp::kGe: return c >= 0;
    case CompareOp::kLe: return c <= 0;
    case CompareOp::kGt: return c > 0;
    case CompareOp::kLt: return c < 0;
  }
  return 0;
}
// A datetime literal's microseconds: its instant when aware, its wall clock
// when naive.
inline std::int64_t literal_micros(const Value& v) { return micros_of(literal_datetime(v)); }
// Kleene truth of col(x) OP v on one cell (check_scalar passed): -1 null,
// 0 false, 1 true.
inline int compare_scalar(const Cell& cell, const std::string& dtype, const Value& v, CompareOp op,
                          const std::vector<std::string>* categories = nullptr) {
  if (null_cell(cell) || v.kind == Kind::kNone || dtype == "Null") return -1;
  const auto& a = *cell;
  // Round CE: an int past the Int64 range (see coerced_operands); a
  // temporal column casts it to null.
  if (const auto u = wide_int(v)) {
    const double d = static_cast<double>(*u);
    if (dtype == "UInt64") return apply_order(order_of(wide_of(a), Wide(*u)), op);
    if (dtype == "Float32") return apply_order(total_order(static_cast<double>(static_cast<float>(real_of(a))), static_cast<double>(static_cast<float>(d))), op);
    if (dtype == "Float64") return apply_order(total_order(real_of(a), d), op);
    if (integer_dtype(dtype) && dtype.rfind("UInt", 0) != 0) return apply_order(total_order(static_cast<double>(integer_of(a)), d), op);
    if (dtype == "Boolean" || integer_dtype(dtype)) return apply_order(-1, op);
    return -1;
  }
  if (dtype == "String" || categorical_dtype(dtype)) {
    const std::string rhs = v.kind == Kind::kBool ? (v.boolean ? "true" : "false") : v.text;
    return apply_order(order_of(a.value, rhs), op);
  }
  if (enum_dtype(dtype)) {
    if (op == CompareOp::kEq || !categories) return a.value == v.text;
    const auto at = [&](const std::string& t) { return std::find(categories->begin(), categories->end(), t) - categories->begin(); };
    return apply_order(order_of(at(a.value), at(v.text)), op);
  }
  if (dtype == "Boolean") {
    const bool b = true_of(a);
    if (v.kind == Kind::kString) return apply_order(order_of(std::string(b ? "true" : "false"), v.text), op);
    if (v.kind == Kind::kFloat) return apply_order(total_order(b ? 1.0 : 0.0, v.real), op);
    if (v.kind == Kind::kBool) return apply_order(order_of(int(b), int(v.boolean)), op);
    return apply_order(order_of(std::int64_t(b), v.integer), op);
  }
  if (integer_dtype(dtype)) {
    const auto x = wide_of(a);
    switch (v.kind) {
      case Kind::kFloat: return apply_order(total_order(wide_double(x), v.real), op);
      case Kind::kBool: return apply_order(order_of(x, Wide(v.boolean ? 1 : 0)), op);
      case Kind::kDatetime: return apply_order(order_of(x, Wide(literal_micros(v))), op);
      default: return apply_order(order_of(x, Wide(v.integer)), op);
    }
  }
  if (float_dtype(dtype)) {
    const double x = real_of(a);
    const bool f32 = dtype == "Float32";
    const auto narrow = [&](double y) { return f32 ? static_cast<double>(static_cast<float>(y)) : y; };
    switch (v.kind) {
      case Kind::kFloat: return apply_order(total_order(narrow(x), narrow(v.real)), op);
      case Kind::kInt: return apply_order(total_order(narrow(x), narrow(static_cast<double>(v.integer))), op);
      case Kind::kBool: return apply_order(total_order(x, v.boolean ? 1.0 : 0.0), op);
      case Kind::kDate: return apply_order(total_order(x, static_cast<double>(v.integer)), op);
      default: return apply_order(total_order(x, static_cast<double>(literal_micros(v))), op);
    }
  }
  const auto physical = physical_of(a, dtype);
  if (!physical) return -1;
  const auto p = *physical;
  if (dtype == "Date") {
    switch (v.kind) {
      case Kind::kFloat: return apply_order(total_order(static_cast<double>(p), v.real), op);
      case Kind::kDatetime: return apply_order(order_of(wide_mul(p, 86'400'000'000LL), Wide(literal_micros(v))), op);
      default: return apply_order(order_of(p, v.integer), op);
    }
  }
  const auto unit = unit_nanoseconds(dtype);
  switch (v.kind) {
    case Kind::kFloat: return apply_order(total_order(static_cast<double>(p), v.real), op);
    case Kind::kInt: return apply_order(order_of(p, v.integer), op);
    case Kind::kDate: return apply_order(order_of(Wide(p), wide_mul(v.integer, 86'400'000'000'000LL / unit)), op);
    default: break;
  }
  const auto m = literal_micros(v);
  if (unit == 1) {
    // A microsecond literal covers a nanosecond range: == is_between it, <=
    // its end, the others its start.
    const Wide lo = wide_mul(m, 1000), hi = wide_add(lo, 999), at(p);
    if (op == CompareOp::kEq) return at >= lo && at <= hi;
    if (op == CompareOp::kLe) return at <= hi;
    return apply_order(order_of(at, lo), op);
  }
  if (unit == 1000) return apply_order(order_of(p, m), op);
  // Round CE: to milliseconds by floor division (before 1970 too).
  return apply_order(order_of(p, floor_div(m, 1000)), op);
}

// The plan text of col(x) OP v once Polars coerced it (check_scalar passed):
// the literal cast to the column's dtype where it fits (col("x") == 1,
// col("x") == 3.0000e9), the column cast to a wider type otherwise
// (col("x").cast(Float64) == 1.5), a constant for an integer out of the
// column's range, a nanosecond range for a microsecond datetime.
struct Operands {
  std::string lhs, rhs, whole;  // whole: a folded form (constant, range)
  [[nodiscard]] std::string text(CompareOp op) const { return whole.empty() ? lhs + " " + op_text(op) + " " + rhs : whole; }
};
inline Operands coerced_operands(const std::string& column, const std::string& dtype, const Value& v, CompareOp op) {
  const auto c = "col(" + quote(column) + ")";
  const auto plain = [&](const std::string& rhs) { return Operands{c, rhs, {}}; };
  const auto cast = [&](const std::string& to, const std::string& rhs) { return Operands{c + ".cast(" + to + ")", rhs, {}}; };
  const auto folded = [&](std::string whole) { return Operands{{}, {}, std::move(whole)}; };
  if (v.kind == Kind::kNone) return folded("null.repeat([" + c + ".len()])");
  // Round CE: an int past the Int64 range compares exactly on UInt64 (a
  // Boolean column cast to it), as a float on float and signed columns,
  // folds on narrower unsigned ones and stays dynamic on the others.
  if (const auto u = wide_int(v)) {
    const double d = static_cast<double>(*u);
    if (dtype == "UInt64") return plain(v.text);
    if (dtype == "Boolean") return cast("UInt64", v.text);
    if (dtype == "Float32") return plain(polars_frame_float(static_cast<double>(static_cast<float>(d))));
    if (dtype == "Float64") return plain(polars_frame_float(d));
    if (dtype.rfind("UInt", 0) == 0)
      return folded("when(" + c + ".is_not_null()).then(" + (apply_order(-1, op) ? "true" : "false") + ").otherwise(null)");
    if (integer_dtype(dtype)) return cast("Float64", polars_frame_float(d));
    return plain(scalar_display(v));
  }
  // Round CE: a Null column takes the type of a typed literal (a dynamic
  // int or float stays as written).
  if (dtype == "Null") {
    if (v.kind == Kind::kInt || v.kind == Kind::kFloat) return plain(scalar_display(v));
    return cast(debug_name(lit_value(v).type), scalar_display(v));
  }
  if (dtype == "String") {
    if (v.kind == Kind::kBool) return plain(quote(v.boolean ? "true" : "false"));
    return plain(scalar_display(v));
  }
  if (dtype == "Boolean") {
    if (v.kind == Kind::kString) return cast("String", quote(v.text));
    // Round CE: a dynamic int materializes as Int32, Int64 past its range.
    if (v.kind == Kind::kInt) {
      const bool narrow = v.integer >= std::numeric_limits<std::int32_t>::min() && v.integer <= std::numeric_limits<std::int32_t>::max();
      return cast(narrow ? "Int32" : "Int64", std::to_string(v.integer));
    }
    return plain(scalar_display(v));
  }
  if (integer_dtype(dtype)) {
    switch (v.kind) {
      case Kind::kBool: return plain(v.boolean ? "1" : "0");
      case Kind::kFloat: return cast("Float64", polars_frame_float(v.real));
      case Kind::kDate:
        return dtype == "Int32" || dtype == "Int64" ? plain(std::to_string(v.integer)) : cast("Int64", std::to_string(v.integer));
      case Kind::kDatetime: {
        const auto m = std::to_string(literal_micros(v));
        return dtype == "Int64" ? plain(m) : cast("Int64", m);
      }
      case Kind::kInt: {
        const int range = integer_range_side(v.integer, dtype);
        if (range == 0) return plain(std::to_string(v.integer));
        if (dtype == "UInt64") return cast("Int64", std::to_string(v.integer));
        // Every non-null cell lies on one side of the literal.
        const int side = range > 0 ? -1 : 1;
        return folded("when(" + c + ".is_not_null()).then(" + (apply_order(side, op) ? "true" : "false") + ").otherwise(null)");
      }
      default: return plain(scalar_display(v));
    }
  }
  if (float_dtype(dtype)) {
    const bool f32 = dtype == "Float32";
    const auto narrow = [&](double y) { return f32 ? static_cast<double>(static_cast<float>(y)) : y; };
    switch (v.kind) {
      case Kind::kBool: return plain(polars_frame_float(v.boolean ? 1.0 : 0.0));
      case Kind::kInt: return plain(polars_frame_float(narrow(static_cast<double>(v.integer))));
      case Kind::kFloat: return plain(polars_frame_float(narrow(v.real)));
      case Kind::kDate: return plain(polars_frame_float(static_cast<double>(v.integer)));
      case Kind::kDatetime: {
        const auto m = polars_frame_float(static_cast<double>(literal_micros(v)));
        return f32 ? cast("Float64", m) : plain(m);
      }
      default: return plain(scalar_display(v));
    }
  }
  if (dtype == "Date") {
    if (v.kind == Kind::kDatetime) return cast(debug_name(lit_value(v).type), scalar_display(v));
    return plain(scalar_display(v));
  }
  if (datetime_dtype(dtype)) {
    const auto unit = unit_nanoseconds(dtype);
    const auto text = [&](std::int64_t physical) { return column_value_text(Value::of(physical), dtype); };
    if (v.kind == Kind::kDate) return plain(text(v.integer * (86'400'000'000'000LL / unit)));
    if (v.kind != Kind::kDatetime) return plain(scalar_display(v));
    const auto m = literal_micros(v);
    if (unit == 1) {
      const auto lo = m * 1000, hi = lo + 999;
      if (op == CompareOp::kEq) return folded(c + ".is_between([" + text(lo) + ", " + text(hi) + "])");
      return plain(text(op == CompareOp::kLe ? hi : lo));
    }
    return plain(text(unit == 1000 ? m : floor_div(m, 1000)));
  }
  return plain(scalar_display(v));
}
inline std::string coerced_comparison(const std::string& column, const std::string& dtype, const Value& v, CompareOp op) {
  return coerced_operands(column, dtype, v, op).text(op);
}
// The plan text of col(x) OP v as a later failure prints it: coerced when
// the comparison types, as written otherwise.
inline std::string comparison_plan(const std::string& column, const std::optional<std::string>& dtype, const Value& v,
                                   CompareOp op, const std::vector<std::string>* categories = nullptr) {
  if (dtype && check_scalar(column, *dtype, v, op, categories).ok()) return coerced_comparison(column, *dtype, v, op);
  return "col(" + quote(column) + ") " + op_text(op) + " " + scalar_display(v);
}

}  // namespace minera::detail::polars_lit
