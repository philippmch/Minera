#include "minera/pandas_sequence_filters.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string_view>
#include <variant>

#include "minera/filtering_common.hpp"
#include "minera/timestamp.hpp"
#include "python_scalar.hpp"
#include "numpy_object_sort.hpp"
#include "pandas_list_key.hpp"
#include "pandas_numpy_view.hpp"
#include "pandas_range_index.hpp"
#include "zone_database.hpp"

namespace minera {

PandasFilterValue PandasFilterValue::of(bool value) {
  PandasFilterValue out;
  out.kind = Kind::kBool;
  out.boolean = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(std::int64_t value) {
  PandasFilterValue out;
  out.kind = Kind::kInt;
  out.integer = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(double value) {
  PandasFilterValue out;
  out.kind = Kind::kFloat;
  out.real = value;
  return out;
}
PandasFilterValue PandasFilterValue::of(std::string value) {
  PandasFilterValue out;
  out.kind = Kind::kString;
  out.text = std::move(value);
  return out;
}
PandasFilterValue PandasFilterValue::list(std::vector<PandasFilterValue> values) {
  PandasFilterValue out;
  out.kind = Kind::kList;
  out.items = std::move(values);
  return out;
}
PandasFilterValue PandasFilterValue::tuple(std::vector<PandasFilterValue> values) {
  PandasFilterValue out;
  out.kind = Kind::kTuple;
  out.items = std::move(values);
  return out;
}
PandasFilterValue PandasFilterValue::set(std::vector<PandasFilterValue> values) {
  PandasFilterValue out;
  out.kind = Kind::kSet;
  out.items = std::move(values);
  return out;
}
PandasFilterValue PandasFilterValue::dict(std::vector<std::pair<PandasFilterValue, PandasFilterValue>> values) {
  PandasFilterValue out;
  out.kind = Kind::kDict;
  out.entries = std::move(values);
  return out;
}

PandasFilterValue PandasFilterValue::datetime(std::int64_t wall_nanoseconds, std::optional<std::int32_t> offset_minutes,
                                              std::string zone) {
  PandasFilterValue out;
  out.kind = Kind::kDatetime;
  out.integer = wall_nanoseconds;
  out.offset_minutes = offset_minutes;
  if (offset_minutes) out.text = std::move(zone);
  return out;
}
PandasFilterValue PandasFilterValue::date(std::int64_t days) {
  PandasFilterValue out;
  out.kind = Kind::kDate;
  out.integer = days;
  return out;
}

namespace {

// Proleptic Gregorian civil date of a day number (days since 1970-01-01).
void civil_from_days(std::int64_t z, std::int64_t& y, unsigned& m, unsigned& d) {
  z += 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const auto doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = static_cast<std::int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

std::string two(unsigned v) { return std::string(1, static_cast<char>('0' + v / 10)) + static_cast<char>('0' + v % 10); }

std::string date_text(std::int64_t days) {
  std::int64_t y = 0;
  unsigned m = 0, d = 0;
  civil_from_days(days, y, m, d);
  std::string year = std::to_string(y);
  while (year.size() < 4) year.insert(year.begin(), '0');
  return year + "-" + two(m) + "-" + two(d);
}

}  // namespace

std::string pandas_filter_value_str(const PandasFilterValue& value) {
  if (value.kind == PandasFilterValue::Kind::kDate) return date_text(value.integer);
  if (value.kind != PandasFilterValue::Kind::kDatetime) return value.text;
  constexpr std::int64_t kDay = 86'400'000'000'000;
  const auto days = floor_div(value.integer, kDay);
  const auto rest = value.integer - days * kDay;
  const auto seconds = rest / 1'000'000'000;
  const auto nanos = rest % 1'000'000'000;
  std::string out = date_text(days) + " " + two(static_cast<unsigned>(seconds / 3600)) + ":" +
                    two(static_cast<unsigned>(seconds / 60 % 60)) + ":" + two(static_cast<unsigned>(seconds % 60));
  if (nanos) {
    auto digits = std::to_string(nanos);
    digits.insert(digits.begin(), 9 - digits.size(), '0');
    out += "." + (nanos % 1000 ? digits : digits.substr(0, 6));
  }
  if (value.offset_minutes) {
    const auto offset = *value.offset_minutes;
    const auto magnitude = static_cast<unsigned>(offset < 0 ? -offset : offset);
    out += (offset < 0 ? "-" : "+") + two(magnitude / 60) + ":" + two(magnitude % 60);
  }
  return out;
}

bool pandas_filter_values_equal(const PandasFilterValue& a, const PandasFilterValue& b) {
  using K = PandasFilterValue::Kind;
  const auto numeric = [](const PandasFilterValue& v) { return v.kind == K::kBool || v.kind == K::kInt || v.kind == K::kFloat; };
  if (numeric(a) && numeric(b)) {
    const auto as_int = [](const PandasFilterValue& v) { return v.kind == K::kBool ? std::int64_t{v.boolean} : v.integer; };
    if (a.kind != K::kFloat && b.kind != K::kFloat) return as_int(a) == as_int(b);
    const double x = a.kind == K::kFloat ? a.real : static_cast<double>(as_int(a));
    const double y = b.kind == K::kFloat ? b.real : static_cast<double>(as_int(b));
    return x == y;
  }
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case K::kNone: return true;
    case K::kString: return a.text == b.text;
    case K::kDate: return a.integer == b.integer;
    case K::kDatetime:
      return a.offset_minutes.has_value() == b.offset_minutes.has_value() &&
             a.instant_nanoseconds() == b.instant_nanoseconds();
    case K::kList:
    case K::kTuple:
      if (a.items.size() != b.items.size()) return false;
      for (std::size_t i = 0; i < a.items.size(); ++i)
        if (!pandas_filter_values_equal(a.items[i], b.items[i])) return false;
      return true;
    case K::kSet:
      // Round DA: sets compare as sets (the items are hashable and distinct).
      if (a.items.size() != b.items.size()) return false;
      for (const auto& x : a.items)
        if (std::none_of(b.items.begin(), b.items.end(), [&](const PandasFilterValue& y) { return pandas_filter_values_equal(x, y); }))
          return false;
      return true;
    case K::kDict:
      if (a.entries.size() != b.entries.size()) return false;
      for (std::size_t i = 0; i < a.entries.size(); ++i)
        if (!pandas_filter_values_equal(a.entries[i].first, b.entries[i].first) ||
            !pandas_filter_values_equal(a.entries[i].second, b.entries[i].second))
          return false;
      return true;
    default: return false;
  }
}

namespace {

using Value = PandasFilterValue;
using Kind = PandasFilterValue::Kind;
using Cell = std::optional<EventAttribute>;

[[noreturn]] void fail(const std::string& type, const std::string& message) {
  throw PandasSequenceFilterError(type, message);
}

// Python repr of a str.
std::string py_repr(const std::string& text) {
  const bool double_quotes = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
  const char quote = double_quotes ? '"' : '\'';
  std::string out(1, quote);
  for (const char c : text) {
    if (c == '\\') out += "\\\\";
    else if (c == quote) out += std::string("\\") + quote;
    else if (c == '\n') out += "\\n";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out + quote;
}

[[noreturn]] void key_error(const std::string& message) { fail("KeyError", py_repr(message)); }

std::string type_name(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "NoneType";
    case Kind::kBool: return "bool";
    case Kind::kInt: return "int";
    case Kind::kFloat: return "float";
    case Kind::kString: return "str";
    case Kind::kList: return "list";
    case Kind::kTuple: return "tuple";
    case Kind::kDict: return "dict";
    case Kind::kSet: return "set";
    // Python's messages print the C type name (round CC).
    case Kind::kDatetime: return "datetime.datetime";
    case Kind::kDate: return "datetime.date";
  }
  return "object";
}

bool truthy(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return false;
    case Kind::kBool: return v.boolean;
    case Kind::kInt: return v.integer != 0;
    case Kind::kFloat: return v.real != 0.0;
    case Kind::kString: return !v.text.empty();
    case Kind::kList:
    case Kind::kTuple:
    case Kind::kSet: return !v.items.empty();
    case Kind::kDict: return !v.entries.empty();
    case Kind::kDatetime:
    case Kind::kDate: return true;
  }
  return false;
}

// A Python number (bool and int as integers).
struct Number {
  bool integral = true;
  std::int64_t integer = 0;
  double real = 0.0;
};
std::optional<Number> number_of(const Value& v) {
  if (v.kind == Kind::kBool) return Number{true, v.boolean ? 1 : 0, 0.0};
  if (v.kind == Kind::kInt) return Number{true, v.integer, 0.0};
  if (v.kind == Kind::kFloat) return Number{false, 0, v.real};
  return std::nullopt;
}
// a >= b for an int a and a Python number b.
bool int_at_least(std::int64_t a, const Number& b) {
  if (b.integral) return a >= b.integer;
  return static_cast<double>(a) >= b.real;  // the small counts here convert exactly
}

const Value* parameter(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal) {
  if (!parameters) return nullptr;
  for (const auto& p : *parameters)
    if (p.enum_member && p.key == member) return &p.value;
  for (const auto& p : *parameters)
    if (!p.enum_member && p.key == literal) return &p.value;
  return nullptr;
}
Value parameter_or(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal,
                   Value fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? *found : fallback;
}
std::string text_parameter(const PandasFilterParameters& parameters, std::string_view member,
                           std::string_view literal, const std::string& fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? found->text : fallback;
}
// Round CL: a column key as passed (a str, or a dict or list that pandas
// refuses or maps where the key is used).
Value key_parameter(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal,
                    std::string_view fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? *found : Value::of(std::string(fallback));
}
Value case_key(const PandasFilterParameters& p) {
  return key_parameter(p, "CASE_ID_KEY", kPandasFilterCaseIdKeyParameter, kPandasFilterCaseConceptName);
}
Value activity_key(const PandasFilterParameters& p) {
  return key_parameter(p, "ACTIVITY_KEY", kPandasFilterActivityKeyParameter, kPandasFilterDefaultNameKey);
}
// Round DA (probed): starts_with, ends_with and traces read a plain case or
// activity key before the Parameters member of the same value.
Value literal_first_key(const PandasFilterParameters& parameters, std::string_view member, std::string_view literal,
                        std::string_view fallback) {
  if (parameters)
    for (const auto& p : *parameters)
      if (!p.enum_member && p.key == literal) return p.value;
  return key_parameter(parameters, member, literal, fallback);
}
// The column name of a str key; any other key reads as the empty name, as
// before round CL.
std::string key_text(const Value& key) { return key.kind == Kind::kString ? key.text : std::string(); }
// A dict key, or a list key holding a dict (round CL).
bool dict_key(const Value& key) {
  if (key.kind == Kind::kDict) return true;
  if (key.kind != Kind::kList) return false;
  return std::any_of(key.items.begin(), key.items.end(), [](const Value& item) { return item.kind == Kind::kDict; });
}
// Round DA: a set key, or a list key holding a set.
bool set_key(const Value& key) {
  if (key.kind == Kind::kSet) return true;
  if (key.kind != Kind::kList) return false;
  return std::any_of(key.items.begin(), key.items.end(), [](const Value& item) { return item.kind == Kind::kSet; });
}
// The type of a dict or set key, or of the first dict or set item of a list
// key: the first part a hash refuses.
std::string unhashable_key_type(const Value& key) {
  if (key.kind == Kind::kDict) return "dict";
  if (key.kind == Kind::kSet) return "set";
  for (const auto& item : key.items)
    if (item.kind == Kind::kDict || item.kind == Kind::kSet) return item.kind == Kind::kDict ? "dict" : "set";
  return "list";
}

// ------------------------------------------------------------------ cells

bool null_cell(const Cell& cell) {
  if (!cell || cell->is_null || is_native_na_attribute(*cell)) return true;
  if (const auto* d = std::get_if<double>(&cell->typed_value)) return std::isnan(*d);
  if (cell->kind == AttributeKind::kFloat) return cell->value == "NaN" || cell->value == "nan";
  if (cell->kind == AttributeKind::kDate) return cell->value == "NaT";
  return false;
}

// A hashable identity of a non-null cell, as Python dict/groupby keys see
// it: equal numbers are one key whatever their type or spelling (1, True,
// 1.0 and uint64 1; 0.125 and .125; uint64 and float 2**63), distinct
// numbers stay distinct (uint64 max and -1), and the string '1' is its own
// key. Numbers are keyed by their exact value: an integral value by its full
// decimal digits, any other by the shortest text that round-trips.
struct Key {
  int tag = 0;  // 0 number, 1 string, 2 other
  std::string text;
  bool operator==(const Key&) const = default;
  bool operator<(const Key& o) const { return tag != o.tag ? tag < o.tag : text < o.text; }
};
Key real_key(double d) {
  char buffer[512];
  if (std::isfinite(d) && std::trunc(d) == d) {
    if (d == 0) return {0, "I0"};
    const auto r = std::to_chars(buffer, buffer + sizeof(buffer), d, std::chars_format::fixed, 0);
    return {0, "I" + std::string(buffer, r.ptr)};
  }
  const auto r = std::to_chars(buffer, buffer + sizeof(buffer), d);
  return {0, "R" + std::string(buffer, r.ptr)};
}
// A lexical integer of any width keeps its exact digits (Python ints are
// unbounded, so 2**64 and 2**64+1 are distinct keys); a lexical float keys
// by its value.
std::optional<Key> lexical_number_key(const std::string& text, bool integer) {
  if (text.empty()) return std::nullopt;
  if (integer) {
    if (const auto parsed = python::integer_literal(text)) return Key{0, python::identity(*parsed)};
    return std::nullopt;
  }
  char* end = nullptr;
  const double d = std::strtod(text.c_str(), &end);
  if (end && *end == '\0' && !std::isnan(d)) return real_key(d);
  return std::nullopt;
}
// A datetime keys by its exact instant (nanoseconds); an aware and a naive
// value never share a key. A date-looking string is still a string.
std::optional<Key> date_key(const EventAttribute& cell) {
  const auto* date = std::get_if<NativeDate>(&cell.typed_value);
  if (!date && cell.kind != AttributeKind::kDate) return std::nullopt;
  const auto exact = parse_timestamp_exact(date ? date->value : cell.value);
  if (!exact) return std::nullopt;
  return Key{2, std::string(exact->timezone_aware ? "DA" : "DN") + std::to_string(exact->seconds) + ":" +
                    std::to_string(exact->nanoseconds)};
}
Key key_of(const EventAttribute& cell) {
  if (const auto key = date_key(cell)) return *key;
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return {0, "I" + std::to_string(*i)};
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return {0, "I" + std::to_string(*u)};
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return {0, *b ? "I1" : "I0"};
  if (const auto* d = std::get_if<double>(&cell.typed_value)) return real_key(*d);
  if (cell.kind == AttributeKind::kBoolean && !cell.typed_value.index())
    if (cell.value == "True" || cell.value == "true" || cell.value == "False" || cell.value == "false")
      return {0, cell.value == "True" || cell.value == "true" ? "I1" : "I0"};
  if ((cell.kind == AttributeKind::kFloat || cell.kind == AttributeKind::kInt) && !cell.typed_value.index())
    if (auto key = lexical_number_key(cell.value, cell.kind == AttributeKind::kInt)) return *key;
  if (cell.kind == AttributeKind::kString) return {1, cell.value};
  return {2, std::to_string(static_cast<int>(cell.kind)) + ":" + cell.value};
}
// The numeric reading of a scalar cell for dtype inference.
struct ScalarNumber {
  enum Kind { kNone, kBool, kInteger, kReal } kind = kNone;
  bool negative = false, above_int64 = false, beyond_uint64 = false;
  std::int64_t integer = 0;
  std::uint64_t unsigned_integer = 0;
  double real = 0;
};
ScalarNumber scalar_number(const EventAttribute& cell) {
  ScalarNumber n;
  const auto set_signed = [&](std::int64_t v) {
    n.kind = ScalarNumber::kInteger;
    n.integer = v;
    n.negative = v < 0;
    n.unsigned_integer = static_cast<std::uint64_t>(v);
    n.real = static_cast<double>(v);
  };
  const auto set_unsigned = [&](std::uint64_t v) {
    n.kind = ScalarNumber::kInteger;
    n.unsigned_integer = v;
    n.above_int64 = v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    n.integer = static_cast<std::int64_t>(v);
    n.real = static_cast<double>(v);
  };
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) {
    n.kind = ScalarNumber::kBool;
    n.real = *b ? 1 : 0;
  } else if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) {
    set_signed(*i);
  } else if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) {
    set_unsigned(*u);
  } else if (const auto* d = std::get_if<double>(&cell.typed_value)) {
    n.kind = ScalarNumber::kReal;
    n.real = *d;
  } else if (!cell.typed_value.index() && cell.kind == AttributeKind::kBoolean) {
    n.kind = ScalarNumber::kBool;
  } else if (!cell.typed_value.index() && cell.kind == AttributeKind::kInt && !cell.value.empty()) {
    const char* first = cell.value.data();
    const char* last = first + cell.value.size();
    if (*first == '+') ++first;
    std::int64_t i = 0;
    std::uint64_t u = 0;
    if (const auto r = std::from_chars(first, last, i); r.ec == std::errc{} && r.ptr == last) set_signed(i);
    else if (const auto r2 = std::from_chars(first, last, u); r2.ec == std::errc{} && r2.ptr == last) set_unsigned(u);
    else {
      n.kind = ScalarNumber::kInteger;
      n.beyond_uint64 = true;
      n.negative = *first == '-';
      n.real = std::strtod(cell.value.c_str(), nullptr);
    }
  } else if (!cell.typed_value.index() && cell.kind == AttributeKind::kFloat) {
    n.kind = ScalarNumber::kReal;
    n.real = std::strtod(cell.value.c_str(), nullptr);
  }
  return n;
}
bool is_text(const Cell& cell) { return cell && !null_cell(cell) && cell->kind == AttributeKind::kString; }

// The dtype pandas infers for datetime cells: all aware or all naive,
// microsecond unless one needs nanoseconds, in their one zone; nullopt
// (Timestamp objects) when awareness or zones differ. A cell's zone is its
// named zone when the transport carries one (round CC: Timestamps of one
// named zone share it across offsets), else its offset ("UTC", "UTC+01:00").
std::optional<std::string> datetime_dtype(const std::vector<Cell>& dates) {
  bool naive = false, aware = false, nanoseconds = false, mixed_zones = false;
  std::optional<std::string> zone;
  for (const auto& cell : dates) {
    const auto* date = std::get_if<NativeDate>(&cell->typed_value);
    const auto exact = *parse_timestamp_exact(date ? date->value : cell->value);
    (exact.timezone_aware ? aware : naive) = true;
    nanoseconds = nanoseconds || exact.nanoseconds % 1000 != 0;
    if (exact.timezone_aware) {
      std::string name;
      if (date && date->timezone) {
        name = *date->timezone;
      } else if (exact.offset_minutes == 0) {
        name = "UTC";
      } else {
        const auto minutes = exact.offset_minutes;
        char text[16];
        std::snprintf(text, sizeof(text), "UTC%c%02d:%02d", minutes < 0 ? '-' : '+', std::abs(minutes) / 60,
                      std::abs(minutes) % 60);
        name = text;
      }
      if (zone && *zone != name) mixed_zones = true;
      zone = std::move(name);
    }
  }
  if (naive == aware || mixed_zones) return std::nullopt;
  std::string dtype = std::string("datetime64[") + (nanoseconds ? "ns" : "us");
  if (aware) dtype += ", " + *zone;
  return dtype + "]";
}

std::string column_dtype(const EventTable& df, std::size_t column) {
  return column < df.dtypes.size() ? df.dtypes[column] : std::string("object");
}

// pandas cannot index by float16: grouping by a float16 column raises before
// any group forms, empty frames included. A first/last aggregation reports
// it as its own failure on the column it aggregates.
void reject_float16_groupby(const EventTable& df, std::size_t key) {
  if (column_dtype(df, key) == "float16") fail("NotImplementedError", "float16 indexes are not supported");
}
// The frame's set_index(case), which a wrapping int64 progression of case
// ids turns into a range of another length (pandas_range_index.hpp).
void require_case_index(const EventTable& df, std::size_t case_column) {
  if (const auto length = detail::wrapped_range_length(df, case_column))
    fail("ValueError", "Length mismatch: Expected axis has " + std::to_string(df.size()) + " elements, new values have " +
                           std::to_string(*length) + " elements");
}
void reject_float16_aggregation(const EventTable& df, std::size_t key, bool first, const std::string& dtype) {
  if (column_dtype(df, key) == "float16")
    fail("NotImplementedError", std::string("agg function failed [how->") + (first ? "first" : "last") + ",dtype->" + dtype + "]");
}

// The dtype a copied object column takes: str cells (nulls aside) infer str,
// datetime cells datetime64; any other mix stays object.
std::string inferred_dtype(const EventTable& df, std::size_t column) {
  const auto dtype = column_dtype(df, column);
  if (dtype != "object") return dtype;
  std::vector<Cell> dates;
  bool strings = false;
  for (const auto& row : df.rows) {
    const auto& cell = row[column];
    if (null_cell(cell)) continue;
    if (is_text(cell)) strings = true;
    else if (date_key(*cell)) dates.push_back(cell);
    else return dtype;
  }
  if (strings) return dates.empty() ? "str" : dtype;
  if (dates.empty()) return dtype;
  return datetime_dtype(dates).value_or(dtype);
}

// Python str() of a non-null case id.
std::string py_str(const EventAttribute& cell) { return cell.value; }

// The Python object a cell holds: None for an absent or null cell, else the
// typed value (python_scalar.hpp).
python::Scalar scalar_of(const Cell& cell) {
  if (!cell) return python::none();
  return python::from_attribute(*cell);
}
// A scalar filter argument as a Python object; containers have none.
std::optional<python::Scalar> scalar_of(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return python::none();
    case Kind::kBool: return python::boolean(v.boolean);
    case Kind::kInt: return python::integer(v.integer);
    case Kind::kFloat: return python::real(v.real);
    case Kind::kString: return python::text(v.text);
    case Kind::kDatetime: {
      python::Scalar s;
      s.kind = python::Scalar::Kind::kDate;
      const auto instant = v.instant_nanoseconds();
      s.date.seconds = instant / 1'000'000'000 - (instant % 1'000'000'000 < 0);
      s.date.nanoseconds = static_cast<std::uint32_t>(instant - s.date.seconds * 1'000'000'000);
      s.date.timezone_aware = v.offset_minutes.has_value();
      s.date.offset_minutes = v.offset_minutes.value_or(0);
      s.text = pandas_filter_value_str(v);
      return s;
    }
    default: return std::nullopt;
  }
}
// A Python object back as a filter value (a key of a returned dict).
Value value_of(const python::Scalar& s) {
  using K = python::Scalar::Kind;
  switch (s.kind) {
    case K::kNone: return Value::none();
    case K::kBool: return Value::of(s.boolean);
    case K::kFloat: return Value::of(s.real);
    case K::kInt: {
      std::int64_t v = 0;
      const auto text = python::str(s);
      const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
      if (r.ec == std::errc{} && r.ptr == text.data() + text.size()) return Value::of(v);
      return Value::of(text);
    }
    default: return Value::of(python::str(s));
  }
}
bool is_nan(const python::Scalar& s) { return s.kind == python::Scalar::Kind::kFloat && std::isnan(s.real); }
// Whether pd.NA == v answers pd.NA (a number, str, bool or datetime) rather
// than False (None and the containers).
bool na_compares(const Value& v) {
  return v.kind != Kind::kNone && v.kind != Kind::kList && v.kind != Kind::kTuple && v.kind != Kind::kDict;
}
// Python's bool(a == b) as `in` and dict lookups use it for two distinct
// objects: numbers by value (1, 1.0 and True), text exactly, None only to
// None, a NaN to nothing; pd.NA's answer has no truth value.
bool py_equal(const python::Scalar& a, const python::Scalar& b) {
  using K = python::Scalar::Kind;
  if (a.kind == K::kNA || b.kind == K::kNA) {
    if (a.kind == K::kNone || b.kind == K::kNone) return false;
    fail("TypeError", "boolean value of NA is ambiguous");
  }
  if (is_nan(a) || is_nan(b)) return false;
  return python::equal(a, b);
}
// pandas' column == scalar on an object column: a missing cell (None, NaN,
// pd.NA) and a None or NaN scalar compare False, anything else like Python.
// IEEE binary16 conversions (round to nearest even), as numpy's float16.
std::uint16_t float_to_half(float value) {
  std::uint32_t f;
  std::memcpy(&f, &value, sizeof f);
  const std::uint16_t sign = static_cast<std::uint16_t>((f >> 16) & 0x8000u);
  const std::uint32_t magnitude = f & 0x7fffffffu;
  if (magnitude >= 0x7f800000u)
    return sign | (magnitude > 0x7f800000u ? 0x7e00u : 0x7c00u);
  if (magnitude >= 0x477ff000u) return sign | 0x7c00u;  // rounds past 65504
  if (magnitude < 0x38800000u) {  // a subnormal half or zero
    if (magnitude < 0x33000000u) return sign;
    const std::uint32_t exponent = magnitude >> 23;
    const std::uint32_t mantissa = (magnitude & 0x7fffffu) | 0x800000u;
    const std::uint32_t shift = 126 - exponent;  // 14 + 23 - 10 - (exponent - 127) + 1
    std::uint32_t half = mantissa >> shift;
    const std::uint32_t rest = mantissa & ((1u << shift) - 1);
    const std::uint32_t midpoint = 1u << (shift - 1);
    if (rest > midpoint || (rest == midpoint && (half & 1u))) ++half;
    return sign | static_cast<std::uint16_t>(half);
  }
  std::uint32_t half = ((magnitude >> 13) - (112u << 10));
  const std::uint32_t rest = magnitude & 0x1fffu;
  if (rest > 0x1000u || (rest == 0x1000u && (half & 1u))) ++half;
  return sign | static_cast<std::uint16_t>(half);
}
float half_to_float(std::uint16_t half) {
  const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
  const std::uint32_t exponent = (half >> 10) & 0x1fu;
  std::uint32_t mantissa = half & 0x3ffu;
  std::uint32_t f;
  if (exponent == 0x1fu) f = sign | 0x7f800000u | (mantissa << 13);
  else if (exponent) f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
  else if (!mantissa) f = sign;
  else {
    int shift = 0;
    while (!(mantissa & 0x400u)) { mantissa <<= 1; ++shift; }
    f = sign | ((113u - shift) << 23) | ((mantissa & 0x3ffu) << 13);
  }
  float value;
  std::memcpy(&value, &f, sizeof value);
  return value;
}

// A double rounded to float16 or float32 (round to nearest even), as numpy
// casts a Python float to a column's width. Rounding through float32 first
// can land on a float16 tie, so the nearest neighbour is checked.
double narrow_float(double value, int bits) {
  if (bits == 32) return static_cast<float>(value);
  if (bits != 16 || std::isnan(value)) return value;
  const auto half = float_to_half(static_cast<float>(value));
  double best = half_to_float(half);
  if (std::isinf(best) || best == value) return best;
  for (const std::uint16_t neighbour : {static_cast<std::uint16_t>(half - 1), static_cast<std::uint16_t>(half + 1)}) {
    if ((neighbour & 0x7fffu) >= 0x7c00u || (neighbour ^ half) & 0x8000u) continue;
    const double candidate = half_to_float(neighbour);
    if (std::fabs(candidate - value) < std::fabs(best - value) ||
        (std::fabs(candidate - value) == std::fabs(best - value) && !(neighbour & 1u)))
      best = candidate;
  }
  return best;
}

// pandas' masked dtypes, which hold pd.NA.
bool masked_dtype(const std::string& dtype) {
  return dtype == "boolean" || dtype.starts_with("Int") || dtype.starts_with("UInt") || dtype.starts_with("Float");
}

// The float width of a numpy or masked float column (0 for other dtypes).
int float_bits(const std::string& dtype) {
  if (dtype == "float16") return 16;
  if (dtype == "float32" || dtype == "Float32") return 32;
  if (dtype == "float64" || dtype == "Float64") return 64;
  return 0;
}

// df[column] == v: a float16 or float32 column compares with a Python
// number in its own width (numpy casts the number to the column's dtype).
bool column_equals(const Cell& cell, const Value& v, const std::string& dtype) {
  if (null_cell(cell)) return false;
  const auto scalar = scalar_of(v);
  if (!scalar || scalar->kind == python::Scalar::Kind::kNone || is_nan(*scalar)) return false;
  const auto value = scalar_of(cell);
  if (const int bits = float_bits(dtype); bits && bits < 64 && python::is_number(*scalar) && value.kind == python::Scalar::Kind::kFloat) {
    const double number = scalar->kind == python::Scalar::Kind::kFloat ? scalar->real
                          : scalar->kind == python::Scalar::Kind::kBool ? (scalar->boolean ? 1.0 : 0.0)
                                                                         : std::strtod(python::str(*scalar).c_str(), nullptr);
    return narrow_float(number, bits) == value.real;
  }
  return py_equal(value, *scalar);
}

// Python's a + b for the values a dataframe holds: str + str concatenates,
// numbers add (bool as int, integers exactly at any width, a float side makes
// the sum a float), anything else raises the TypeError Python (or pandas'
// Timestamp) raises.
python::Scalar python_add(const python::Scalar& a, const python::Scalar& b) {
  using K = python::Scalar::Kind;
  if (a.kind == K::kText && b.kind == K::kText) return python::text(a.text + b.text);
  if (a.kind == K::kText)
    fail("TypeError", "can only concatenate str (not \"" + python::type_name(b) + "\") to str");
  if (python::is_number(a) && python::is_number(b)) {
    const auto as_integer = [](const python::Scalar& s) {
      return s.kind == K::kBool ? python::integer(s.boolean ? 1 : 0) : s;
    };
    if (a.kind != K::kFloat && b.kind != K::kFloat) {
      const auto x = as_integer(a), y = as_integer(b);
      return python::add_integers(x.negative, x.digits, y.negative, y.digits);
    }
    // int + float converts the int to the nearest float, as float(int) does.
    const auto as_double = [&](const python::Scalar& s) {
      if (s.kind == K::kFloat) return s.real;
      const auto i = as_integer(s);
      const double value = std::strtod(python::str(i).c_str(), nullptr);
      if (std::isinf(value)) fail("OverflowError", "int too large to convert to float");
      return value;
    };
    return python::real(as_double(a) + as_double(b));
  }
  // Timestamp + int raises pandas' own message; bool is not an int to it.
  if (a.kind == K::kDate && b.kind == K::kInt)
    fail("TypeError", "Addition/subtraction of integers and integer-arrays with Timestamp is no longer supported.  "
                      "Instead of adding/subtracting `n`, use `n * obj.freq`");
  if (b.kind == K::kDate && a.kind == K::kInt)
    fail("TypeError", "Addition/subtraction of integers and integer-arrays with Timestamp is no longer supported.  "
                      "Instead of adding/subtracting `n`, use `n * obj.freq`");
  fail("TypeError", "unsupported operand type(s) for +: '" + python::type_name(a) + "' and '" + python::type_name(b) + "'");
}

// pandas adds two object columns element by element over every row, missing
// cells included, and only after a TypeError retries the rows where both
// cells are present. A missing cell (None, NaN, pd.NA, NaT) therefore still
// raises OverflowError when its float meets an int too large for a float,
// unless a TypeError comes first. Returns whether the pair raises, and
// which error: 0 none, 1 TypeError, 2 OverflowError.
int missing_add_outcome(const Cell& left, const Cell& right) {
  using K = python::Scalar::Kind;
  const auto nat = [](const Cell& cell) {
    return cell && !cell->is_null && cell->kind == AttributeKind::kDate && cell->value == "NaT";
  };
  const auto a = scalar_of(left), b = scalar_of(right);
  const bool a_nat = nat(left), b_nat = nat(right);
  const auto date = [](const python::Scalar& s, bool is_nat) { return is_nat || s.kind == K::kDate; };
  const auto huge = [](const python::Scalar& s) {
    return s.kind == K::kInt && std::isinf(std::strtod(python::str(s).c_str(), nullptr));
  };
  if (a.kind == K::kNone || b.kind == K::kNone) return 1;
  if (a.kind == K::kNA) return date(b, b_nat) ? 1 : 0;
  if (b.kind == K::kNA) return date(a, a_nat) ? 1 : 0;
  // NaT adds to an int (not a bool) or a timestamp.
  if (a_nat) return (b.kind == K::kInt || date(b, b_nat)) ? 0 : 1;
  if (b_nat) return (a.kind == K::kInt || a.kind == K::kDate) ? 0 : 1;
  // A NaN float adds to numbers, converting an int to a float.
  const auto& other = is_nan(a) ? b : a;
  if (other.kind == K::kText || other.kind == K::kDate) return 1;
  return huge(other) ? 2 : 0;
}

// groupby(case) with sort=True factorizes the case column first: its
// distinct non-null values must order (python::safe_sort_order), or the
// reference raises before grouping, whatever the groups are used for.
// `rows`, when given, limits the check to the rows the reference groups.
void require_sorted_groupby(const EventTable& df, std::size_t case_column,
                            const std::vector<bool>* rows = nullptr) {
  reject_float16_groupby(df, case_column);
  std::vector<python::Scalar> uniques;
  std::set<std::string> seen;
  for (std::size_t i = 0; i < df.rows.size(); ++i) {
    const auto& row = df.rows[i];
    if ((rows && !(*rows)[i]) || null_cell(row[case_column])) continue;
    auto value = scalar_of(row[case_column]);
    if (python::is_missing(value)) continue;
    if (seen.insert(python::identity(value)).second) uniques.push_back(std::move(value));
  }
  try {
    (void)python::safe_sort_order(uniques);
  } catch (const python::TypeError& error) {
    fail("TypeError", error.what());
  }
}

std::optional<std::size_t> find_column(const EventTable& df, const std::string& name) {
  for (std::size_t i = 0; i < df.columns.size(); ++i)
    if (df.columns[i] == name) return i;
  return std::nullopt;
}
std::size_t require_column(const EventTable& df, const std::string& name, bool groupby_selection = false) {
  const auto found = find_column(df, name);
  if (!found) key_error(groupby_selection ? "Column not found: " + name : name);
  return *found;
}

// Round CL (observed black-box): how pandas uses a dict key, or a list key
// holding a dict, at each kind of column access.
// df.groupby(key): a dict maps the index labels (strs, which no row label
// is, so no group forms); a list checks its str items as columns in order.
// std::nullopt when no group forms.
// Round DA: a set (also as a list item, after the str items before it) is
// mapped over the index labels as a function, which it is not; on an empty
// frame nothing is mapped and no group forms.
std::optional<std::size_t> groupby_key(const EventTable& df, const Value& key) {
  if (key.kind == Kind::kSet) {
    if (df.size() != 0) fail("TypeError", "'set' object is not callable");
    return std::nullopt;
  }
  if (set_key(key)) {
    for (const auto& item : key.items) {
      if (item.kind == Kind::kString) require_column(df, item.text);
      if (item.kind == Kind::kSet && df.size() != 0) fail("TypeError", "'set' object is not callable");
    }
    return std::nullopt;
  }
  if (!dict_key(key)) return require_column(df, key_text(key));
  if (key.kind == Kind::kList)
    for (const auto& item : key.items)
      if (item.kind == Kind::kString) require_column(df, item.text);
  return std::nullopt;
}
// df.groupby(...)[key]: a dict (or a list holding one) does not hash.
std::size_t selection_key(const EventTable& df, const Value& key) {
  if (dict_key(key) || set_key(key)) fail("TypeError", "unhashable type: '" + unhashable_key_type(key) + "'");
  return require_column(df, key_text(key), true);
}
// df[key]: a dict is refused as an indexer, a list's dict does not hash.
std::size_t getitem_key(const EventTable& df, const Value& key) {
  if (key.kind == Kind::kDict || key.kind == Kind::kSet)
    fail("TypeError", "Passing a " + type_name(key) + " as an indexer is not supported. Use a list instead.");
  if (dict_key(key) || set_key(key)) fail("TypeError", "unhashable type: '" + unhashable_key_type(key) + "'");
  return require_column(df, key_text(key));
}
// hash(key) of a dict or list key.
void hash_key(const Value& key) {
  if (key.kind == Kind::kDict) fail("TypeError", "unhashable type: 'dict'");
  if (key.kind == Kind::kSet) fail("TypeError", "unhashable type: 'set'");
  if (key.kind == Kind::kList) fail("TypeError", "unhashable type: 'list'");
}
// df.set_index(key) of a dict key or a list key holding a dict.
void set_index_key(const Value& key) {
  if (!dict_key(key) && !set_key(key)) return;
  fail("TypeError", "The parameter \"keys\" may be a column key, one-dimensional array, or a list containing only "
                    "valid column keys and one-dimensional arrays.. Received column of type <class '" +
                        unhashable_key_type(key) + "'>");
}

EventAttribute make_cell(const std::string& column, std::string text, AttributeKind kind) {
  EventAttribute out;
  out.key = column;
  out.value = std::move(text);
  out.kind = kind;
  return out;
}
EventAttribute int_cell(const std::string& column, std::int64_t value) {
  auto out = make_cell(column, std::to_string(value), AttributeKind::kInt);
  out.typed_value = value;
  return out;
}
EventAttribute float_cell(const std::string& column, double value) {
  auto out = make_cell(column, std::isnan(value) ? "NaN" : filtering_float_repr(value), AttributeKind::kFloat);
  out.typed_value = value;
  return out;
}
EventAttribute string_cell(const std::string& column, std::string value) {
  return make_cell(column, std::move(value), AttributeKind::kString);
}

EventTableIndexLabel range_label(std::size_t i) { return {int_cell("", static_cast<std::int64_t>(i))}; }

std::vector<EventTableIndexLabel> labels_of(const EventTable& df) {
  if (df.index) return df.index->labels;
  std::vector<EventTableIndexLabel> out;
  for (std::size_t i = 0; i < df.size(); ++i) out.push_back(range_label(i));
  return out;
}

bool arithmetic(const std::vector<EventTableIndexLabel>& labels) {
  std::vector<std::int64_t> values;
  for (const auto& label : labels) {
    const auto* v = std::get_if<EventAttribute>(&label.value);
    if (!v) return false;
    if (const auto* i = std::get_if<std::int64_t>(&v->typed_value)) values.push_back(*i);
    else if (const auto* u = std::get_if<std::uint64_t>(&v->typed_value)) values.push_back(static_cast<std::int64_t>(*u));
    else return false;
  }
  if (values.size() < 2) return true;
  const auto step = values[1] - values[0];
  if (step == 0) return false;
  for (std::size_t i = 2; i < values.size(); ++i)
    if (values[i] - values[i - 1] != step) return false;
  return true;
}

EventTable subset(const EventTable& df, const std::vector<bool>& keep) {
  EventTable out = df;
  out.rows.clear();
  const auto labels = labels_of(df);
  if (!out.index) out.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  out.index->labels.clear();
  for (std::size_t i = 0; i < df.size(); ++i) {
    if (!keep[i]) continue;
    out.rows.push_back(df.rows[i]);
    out.index->labels.push_back(labels[i]);
  }
  if (out.index->kind == "RangeIndex" && !arithmetic(out.index->labels)) out.index->kind = "Index";
  return out;
}

void reset_index(EventTable& df) {
  df.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  for (std::size_t i = 0; i < df.size(); ++i) df.index->labels.push_back(range_label(i));
}

// Rows whose case id is in `cases` (positive) or not (negative; null case
// ids are never in the set).
EventTable keep_cases(const EventTable& df, std::size_t case_column, const std::set<Key>& cases, bool positive) {
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const bool in = !null_cell(c) && cases.count(key_of(*c));
    keep[i] = positive ? in : !in;
  }
  return subset(df, keep);
}

// ------------------------------------------------------------------ list case keys

// Round DX (probe dw/s1, observed black-box): pm4py hands a list case key to
// pandas as it is. groupby(key) builds one grouper per item (the shared
// model of pandas_list_key.hpp: column labels, arrays as long as the frame,
// dicts mapping the index labels, None and sets called on them) and
// factorizes them when it aggregates; set_index(key) takes list items as
// arrays and other items as column labels. A row belongs to the group of
// its grouper values (a row with a missing one is in no group); the filters
// then keep rows as for a column key, through a synthetic key column.

// A key as the shared model reads it.
EventAttribute key_attribute(const Value& v) {
  EventAttribute out{"", ""};
  switch (v.kind) {
    case Kind::kNone: out.is_null = true; break;
    case Kind::kBool:
      out.kind = AttributeKind::kBoolean;
      out.value = v.boolean ? "True" : "False";
      out.typed_value = v.boolean;
      break;
    case Kind::kInt:
      out.kind = AttributeKind::kInt;
      out.value = std::to_string(v.integer);
      out.typed_value = v.integer;
      break;
    case Kind::kFloat:
      out.kind = AttributeKind::kFloat;
      out.value = std::isnan(v.real) ? "NaN" : python::float_repr(v.real);
      out.typed_value = v.real;
      break;
    case Kind::kString: out.value = v.text; break;
    case Kind::kList:
    case Kind::kTuple:
    case Kind::kSet:
      out.container = v.kind == Kind::kList    ? AttributeContainerKind::kList
                      : v.kind == Kind::kTuple ? AttributeContainerKind::kTuple
                                               : AttributeContainerKind::kSet;
      for (const auto& item : v.items) out.children.push_back(key_attribute(item));
      break;
    case Kind::kDict:
      out.container = AttributeContainerKind::kContainer;
      for (const auto& [k, x] : v.entries) {
        auto child = key_attribute(x);
        child.key = pandas_filter_value_str(k);
        out.children.push_back(std::move(child));
      }
      break;
    case Kind::kDatetime:
    case Kind::kDate:
      out.kind = AttributeKind::kDate;
      out.value = pandas_filter_value_str(v);
      out.typed_value = NativeDate{out.value};
      break;
  }
  return out;
}

// Python's repr of a key item.
std::string attribute_repr(const EventAttribute& v) {
  if (v.is_null) return "None";
  const auto join = [&](const char* open, const char* close, bool tuple) {
    std::string out = open;
    for (std::size_t i = 0; i < v.children.size(); ++i) out += (i ? ", " : "") + attribute_repr(v.children[i]);
    if (tuple && v.children.size() == 1) out += ",";
    return out + close;
  };
  switch (v.container) {
    case AttributeContainerKind::kList: return join("[", "]", false);
    case AttributeContainerKind::kTuple: return join("(", ")", true);
    case AttributeContainerKind::kSet:
    case AttributeContainerKind::kFrozenSet: return v.children.empty() ? "set()" : join("{", "}", false);
    case AttributeContainerKind::kContainer: {
      std::string out = "{";
      for (std::size_t i = 0; i < v.children.size(); ++i)
        out += (i ? ", " : "") + py_repr(v.children[i].key) + ": " + attribute_repr(v.children[i]);
      return out + "}";
    }
    default: break;
  }
  if (const auto* b = std::get_if<bool>(&v.typed_value)) return *b ? "True" : "False";
  if (const auto* i = std::get_if<std::int64_t>(&v.typed_value)) return std::to_string(*i);
  if (const auto* d = std::get_if<double>(&v.typed_value)) return std::isnan(*d) ? "nan" : python::float_repr(*d);
  if (v.kind == AttributeKind::kString) return py_repr(v.value);
  return v.value;
}

// The identity of a present grouper value: equal values (1, 1.0, True) one
// identity, tuples item by item.
std::string value_identity(const EventAttribute& v) {
  if (v.container == AttributeContainerKind::kTuple || v.container == AttributeContainerKind::kFrozenSet) {
    std::string out = "(";
    for (const auto& child : v.children) out += (child.is_null ? std::string("N") : value_identity(child)) + "\x1e";
    return out + ")";
  }
  const auto key = key_of(v);
  return std::to_string(key.tag) + key.text;
}

bool missing_value(const Cell& cell) {
  if (null_cell(cell)) return true;
  return cell->container == AttributeContainerKind::kScalar && detail::pandas_list_nan(*cell);
}

struct ListCaseKey {
  const Value* source = nullptr;
  EventAttribute key;
  std::vector<detail::PandasListGrouper> groupers;
  std::size_t rows = 0;

  explicit ListCaseKey(const Value& value) : source(&value), key(key_attribute(value)) {}

  static bool applies(const Value& value) { return value.kind == Kind::kList; }

  // groupby(key) over `rows` rows of df; an empty key groups only an empty
  // frame.
  void group(const EventTable& df, std::size_t count) {
    rows = count;
    if (key.children.empty()) {
      if (count) fail("ValueError", "No group keys passed!");
      return;
    }
    groupers = detail::pandas_list_groupers(
        key, count, [&](const EventAttribute& item) -> std::optional<std::size_t> {
          if (!detail::pandas_list_text(item)) return std::nullopt;
          return find_column(df, item.value);
        },
        attribute_repr, fail);
  }
  const Value& item_value(const detail::PandasListGrouper& g) const {
    return source->items[static_cast<std::size_t>(g.item - key.children.data())];
  }
  // The value of grouper g at row `row` (the frame's row `source_row`).
  Cell value(const EventTable& df, const detail::PandasListGrouper& g, std::size_t row, std::size_t source_row,
             const std::vector<EventTableIndexLabel>& labels) const {
    using GK = detail::PandasListGrouper::Kind;
    if (g.kind == GK::kColumn) return df.rows[source_row][g.column];
    if (g.kind == GK::kArray) return g.values[row];
    if (g.kind == GK::kMapping) {
      const auto* label = std::get_if<EventAttribute>(&labels[source_row].value);
      if (!label) return std::nullopt;
      for (const auto& [k, x] : item_value(g).entries) {
        const auto* lk = scalar_of(k) ? &k : nullptr;
        if (!lk || null_cell(*label)) continue;
        if (py_equal(scalar_of(*label), *scalar_of(k))) return key_attribute(x);
      }
      return std::nullopt;
    }
    return std::nullopt;
  }
  // Factorizing the groupers (when the groupby aggregates): deep arrays and
  // unhashable values fail, grouper by grouper.
  void factorize(const EventTable& df, const std::vector<std::size_t>& source_rows) const {
    const auto labels = labels_of(df);
    for (const auto& g : groupers)
      detail::pandas_list_factorize(g, source_rows.size(), [&](const detail::PandasListGrouper& grouper, std::size_t i) {
        const auto cell = value(df, grouper, i, source_rows[i], labels);
        return cell ? *cell : EventAttribute{"", ""};
      }, fail);
  }
  // Each row's group identity (std::nullopt: a missing grouper value).
  std::vector<std::optional<std::string>> identities(const EventTable& df, const std::vector<std::size_t>& source_rows) const {
    const auto labels = labels_of(df);
    std::vector<std::optional<std::string>> out(source_rows.size());
    for (std::size_t i = 0; i < source_rows.size(); ++i) {
      std::string id;
      bool present = true;
      for (const auto& g : groupers) {
        const auto cell = value(df, g, i, source_rows[i], labels);
        if (missing_value(cell)) { present = false; break; }
        id += value_identity(*cell) + "\x1f";
      }
      if (present) out[i] = std::move(id);
    }
    return out;
  }
  // The single column a one-item key names.
  std::optional<std::size_t> single_column() const {
    if (groupers.size() == 1 && groupers.front().kind == detail::PandasListGrouper::Kind::kColumn) return groupers.front().column;
    return std::nullopt;
  }
};

std::vector<std::size_t> all_rows(const EventTable& df) {
  std::vector<std::size_t> out(df.size());
  std::iota(out.begin(), out.end(), std::size_t{0});
  return out;
}

// df.set_index(list key): a dict or set item is refused, labels naming no
// column fail together, then an array must be as long as the frame.
void list_set_index(const EventTable& df, const Value& key) {
  if (key.items.empty() && !df.empty())
    fail("ValueError", "Length mismatch: Expected axis has " + std::to_string(df.size()) + " elements, new values have 0 elements");
  set_index_key(key);
  std::vector<std::string> missing;
  for (const auto& item : key.items) {
    if (item.kind == Kind::kList) continue;
    if (item.kind == Kind::kString && find_column(df, item.text)) continue;
    missing.push_back(attribute_repr(key_attribute(item)));
  }
  if (!missing.empty()) {
    std::string list = "[";
    for (std::size_t i = 0; i < missing.size(); ++i) list += (i ? ", " : "") + missing[i];
    key_error("None of " + list + "] are in the columns");
  }
  for (const auto& item : key.items)
    if (item.kind == Kind::kList && item.items.size() != df.size())
      fail("ValueError", "Length mismatch: Expected " + std::to_string(df.size()) + " rows, received array of length " +
                             std::to_string(item.items.size()));
  // Two or more items make a MultiIndex, whose arrays factorize: a list
  // value does not hash. One array alone is an Index of its values as they
  // are (lists stay lists).
  if (key.items.size() > 1)
    for (const auto& item : key.items)
      if (item.kind == Kind::kList)
        for (const auto& value : item.items)
          if (value.kind == Kind::kList) fail("TypeError", "unhashable type: 'list'");
}

// The identities set_index(key).index.isin(groups) compares: the group
// identities, except that a lone array's list values (which groupby read as
// tuples) are lists in the index and equal no group.
std::vector<std::optional<std::string>> index_identities(const Value& key, std::vector<std::optional<std::string>> ids) {
  if (key.items.size() != 1 || key.items.front().kind != Kind::kList) return ids;
  const auto& values = key.items.front().items;
  for (std::size_t i = 0; i < ids.size() && i < values.size(); ++i)
    if (values[i].kind == Kind::kList) ids[i] = "\x02" + std::to_string(i);
  return ids;
}

// The frame with one more (last) column holding each row's group identity.
const std::string kListCaseColumn = "\x01minera:list-case-key";
EventTable with_case_column(const EventTable& df, const std::vector<std::optional<std::string>>& ids) {
  EventTable out = df;
  out.columns.push_back(kListCaseColumn);
  if (!out.dtypes.empty()) out.dtypes.push_back("object");
  if (!out.categorical_dtypes.empty()) out.categorical_dtypes.emplace_back();
  for (std::size_t i = 0; i < out.rows.size(); ++i) {
    auto cell = string_cell(kListCaseColumn, ids[i].value_or(std::string()));
    cell.is_null = !ids[i];
    out.rows[i].push_back(std::move(cell));
  }
  return out;
}
EventTable without_case_column(EventTable df) {
  if (df.columns.empty() || df.columns.back() != kListCaseColumn) return df;
  df.columns.pop_back();
  if (!df.dtypes.empty()) df.dtypes.pop_back();
  if (!df.categorical_dtypes.empty()) df.categorical_dtypes.pop_back();
  for (auto& row : df.rows) row.pop_back();
  return df;
}

// Case ids in first-appearance order with their row positions.
struct Cases {
  std::vector<Key> order;
  std::map<Key, std::vector<std::size_t>> rows;
};
Cases cases_of(const EventTable& df, std::size_t case_column) {
  Cases out;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (null_cell(c)) continue;
    const auto key = key_of(*c);
    auto [it, added] = out.rows.emplace(key, std::vector<std::size_t>{});
    if (added) out.order.push_back(key);
    it->second.push_back(i);
  }
  return out;
}

// An activity value as the boundary filters see it, typed (1, 1.0 and True
// are one activity, '1' another): absent for null.
using Activity = std::optional<python::Scalar>;
bool same_activity(const Activity& a, const Activity& b) {
  if (!a || !b) return !a && !b;
  return python::identity(*a) == python::identity(*b);
}

// The first (or last) non-null activity of each case.
std::map<Key, Activity> boundary_activities(const EventTable& df, const Cases& cases, std::size_t act, bool end) {
  std::map<Key, Activity> out;
  for (const auto& key : cases.order) {
    Activity found;
    const auto& rows = cases.rows.at(key);
    for (std::size_t j = 0; j < rows.size(); ++j) {
      const auto& cell = df.rows[end ? rows[rows.size() - 1 - j] : rows[j]][act];
      if (!null_cell(cell)) {
        found = scalar_of(cell);
        break;
      }
    }
    out[key] = found;
  }
  return out;
}

bool value_is_null(const Value& v) { return v.kind == Kind::kNone || (v.kind == Kind::kFloat && std::isnan(v.real)); }

// isin: a boundary activity equals a listed value like Python's ==. A case
// without any activity has a missing boundary: None in an object column,
// which only None matches; NaN in a numpy float column, which only NaN
// matches; pd.NA in a masked column, which neither matches.
bool matches_value(const Activity& a, const Value& v, const std::string& dtype) {
  if (!a) {
    // A NaT boundary is `isin` no None or NaN (round CB).
    if (masked_dtype(dtype) || dtype.starts_with("datetime64")) return false;
    if (float_bits(dtype)) return v.kind == Kind::kFloat && std::isnan(v.real);
    return v.kind == Kind::kNone;
  }
  const auto scalar = scalar_of(v);
  return scalar && !value_is_null(v) && py_equal(*a, *scalar);
}

// pandas' isin on a datetime64 column (round CC) converts the list to
// datetimes when it holds only datetimes of the column's awareness (aware
// ones of one UTC offset) and None or NaN, at least one datetime: values
// then match by instant (wall clock when naive) and a NaT boundary is `isin`
// a list with None or NaN. Otherwise it compares the column's Timestamps
// with the list's objects as Python does: NaT matches nothing, and an aware
// Timestamp whose wall clock repeats in its zone (a DST fold) equals no
// datetime of another tzinfo.
bool datetime_list_converts(const Value& values, const std::string& dtype) {
  if (!dtype.starts_with("datetime64")) return false;
  const bool aware = dtype.find(", ") != std::string::npos;
  std::optional<std::int32_t> offset;
  bool datetime = false;
  for (const auto& v : values.items) {
    if (v.kind == Kind::kDatetime) {
      if (v.offset_minutes.has_value() != aware) return false;
      if (aware && datetime && offset != v.offset_minutes) return false;
      offset = v.offset_minutes;
      datetime = true;
    } else if (!value_is_null(v)) {
      return false;
    }
  }
  return datetime;
}
// An aware Timestamp of a datetime64 column whose wall clock occurs twice in
// the column's zone.
bool ambiguous_wall(const python::Scalar& s, const std::string& dtype) {
  if (s.kind != python::Scalar::Kind::kDate || !s.date.timezone_aware) return false;
  const auto comma = dtype.find(", ");
  if (comma == std::string::npos) return false;
  const auto zone = dtype.substr(comma + 2, dtype.size() - comma - 3);
  try {
    return zone_database::info_at_local(zone, s.date.seconds + std::int64_t{s.date.offset_minutes} * 60).result ==
           zone_database::LocalResult::kAmbiguous;
  } catch (const std::runtime_error&) {
    return false;
  }
}
// Round CL (observed black-box): pandas' df[column] == v for a list, tuple
// or dict v. The families: object; str and string; masked (boolean, IntN,
// UIntN, FloatN); category; datetime64; and numpy numbers.
enum class EqualityFamily { kObject, kString, kMasked, kCategory, kDatetime, kNumpy };
EqualityFamily equality_family(const std::string& dtype) {
  if (dtype == "object") return EqualityFamily::kObject;
  if (dtype == "str" || dtype == "string") return EqualityFamily::kString;
  if (masked_dtype(dtype)) return EqualityFamily::kMasked;
  if (dtype == "category") return EqualityFamily::kCategory;
  if (dtype.starts_with("datetime64")) return EqualityFamily::kDatetime;
  return EqualityFamily::kNumpy;
}
bool sequence_value(const Value& v) { return v.kind == Kind::kList || v.kind == Kind::kTuple; }
// numpy's array of the items: nested sequences of one length make it 2-D
// (`depth` 2), sequences next to other items or of several lengths fail.
int array_depth(const std::vector<Value>& items) {
  std::size_t sequences = 0;
  std::optional<std::size_t> width;
  bool ragged = false;
  for (const auto& item : items) {
    if (!sequence_value(item)) continue;
    ++sequences;
    if (width && *width != item.items.size()) ragged = true;
    width = item.items.size();
  }
  if (!sequences) return 1;
  if (sequences != items.size() || ragged)
    fail("ValueError", "setting an array element with a sequence. The requested array has an inhomogeneous shape after "
                       "1 dimensions. The detected shape was (" + std::to_string(items.size()) + ",) + inhomogeneous part.");
  return 2;
}
// One row against one item of an array operand: an aware Timestamp whose
// wall clock repeats in its zone equals no datetime of another tzinfo.
bool item_equals(const Cell& cell, const Value& item, const std::string& dtype) {
  if (sequence_value(item) || item.kind == Kind::kDict) return false;
  if (!null_cell(cell) && item.kind == Kind::kDatetime && ambiguous_wall(scalar_of(cell), dtype)) return false;
  return column_equals(cell, item, dtype);
}
std::vector<bool> series_equals(const EventTable& df, std::size_t column, const Value& v) {
  const auto dtype = column_dtype(df, column);
  const auto family = equality_family(dtype);
  const auto n = df.size();
  const auto rows = std::to_string(n);
  std::vector<bool> out(n);
  const auto elementwise = [&](const std::vector<Value>& items) {
    for (std::size_t i = 0; i < n; ++i) out[i] = item_equals(df.rows[i][column], items[i], dtype);
    return out;
  };
  // Round DA: a set compares as a dict of its length does.
  if (v.kind == Kind::kDict || v.kind == Kind::kSet) {
    const auto k = v.kind == Kind::kSet ? v.items.size() : v.entries.size();
    const auto size = std::to_string(k);
    switch (family) {
      case EqualityFamily::kObject:
      case EqualityFamily::kNumpy: return out;
      case EqualityFamily::kMasked: fail("TypeError", "len() of unsized object");
      case EqualityFamily::kString:
        if (k != n) fail("ValueError", "Lengths of operands do not match: " + rows + " != " + size);
        fail("IndexError", "too many indices for array: array is 0-dimensional, but 1 were indexed");
      case EqualityFamily::kCategory:
        if (k != n) fail("ValueError", "Lengths must match.");
        return out;
      case EqualityFamily::kDatetime:
        if (k != n) fail("ValueError", "Lengths must match");
        fail("ValueError", "Buffer has wrong number of dimensions (expected 1, got 0)");
    }
  }
  if (v.kind == Kind::kList) {
    if (v.items.size() != n)
      fail("ValueError", "('Lengths must match to compare', (" + rows + ",), (" + std::to_string(v.items.size()) + ",))");
    return elementwise(v.items);
  }
  if (v.kind == Kind::kTuple) {
    const auto k = v.items.size();
    const auto size = std::to_string(k);
    switch (family) {
      case EqualityFamily::kObject: return out;
      case EqualityFamily::kString:
        if (k != n) fail("ValueError", "Lengths of operands do not match: " + rows + " != " + size);
        array_depth(v.items);
        return elementwise(v.items);
      case EqualityFamily::kMasked:
        if (k != n) fail("ValueError", "Lengths must match to compare");
        array_depth(v.items);
        return elementwise(v.items);
      case EqualityFamily::kCategory:
        if (k != n) fail("ValueError", "Lengths must match.");
        array_depth(v.items);
        return elementwise(v.items);
      case EqualityFamily::kDatetime:
        if (k != n) fail("ValueError", "Lengths must match");
        if (array_depth(v.items) == 2) fail("ValueError", "Buffer has wrong number of dimensions (expected 1, got 2)");
        return elementwise(v.items);
      case EqualityFamily::kNumpy: {
        if (array_depth(v.items) == 2) {
          const auto width = v.items.front().items.size();
          if (width != n && width != 1 && n != 1)
            fail("ValueError", "operands could not be broadcast together with shapes (" + rows + ",) (" + size + "," +
                                   std::to_string(width) + ") ");
          if (k != n) fail("ValueError", "Length of values (" + size + ") does not match length of index (" + rows + ")");
          return out;
        }
        if (k == n) return elementwise(v.items);
        if (k == 1) return elementwise(std::vector<Value>(n, v.items.front()));
        if (n == 1) fail("ValueError", "Length of values (" + size + ") does not match length of index (1)");
        fail("ValueError", "operands could not be broadcast together with shapes (" + rows + ",) (" + size + ",) ");
      }
    }
  }
  for (std::size_t i = 0; i < n; ++i) out[i] = column_equals(df.rows[i][column], v, dtype);
  return out;
}

bool boundary_isin(const Activity& a, const Value& values, const std::string& dtype, bool converts) {
  if (converts) {
    if (!a) return std::any_of(values.items.begin(), values.items.end(), value_is_null);
    return std::any_of(values.items.begin(), values.items.end(), [&](const Value& v) {
      return v.kind == Kind::kDatetime && py_equal(*a, *scalar_of(v));
    });
  }
  if (a && ambiguous_wall(*a, dtype)) return false;
  return std::any_of(values.items.begin(), values.items.end(),
                     [&](const Value& v) { return matches_value(a, v, dtype); });
}

// Python's type name as pandas prints it in its own messages.
std::string short_type_name(const Value& v) {
  if (v.kind == Kind::kDatetime) return "datetime";
  if (v.kind == Kind::kDate) return "date";
  return type_name(v);
}
// The first part of a value that hash() refuses (round CL).
std::optional<std::string> unhashable_type(const Value& v) {
  if (v.kind == Kind::kDict) return std::string("dict");
  if (v.kind == Kind::kSet) return std::string("set");
  if (v.kind == Kind::kList) return std::string("list");
  if (v.kind == Kind::kTuple)
    for (const auto& item : v.items)
      if (auto inner = unhashable_type(item)) return inner;
  return std::nullopt;
}

// The shape np.asarray(values, dtype=object) gives (lists and tuples nest).
std::vector<std::size_t> object_array_shape(std::vector<const Value*> level) {
  std::vector<std::size_t> shape{level.size()};
  while (!level.empty()) {
    std::optional<std::size_t> width;
    for (const auto* v : level) {
      if (!sequence_value(*v)) return shape;
      if (width && *width != v->items.size()) return shape;
      width = v->items.size();
    }
    shape.push_back(*width);
    std::vector<const Value*> next;
    for (const auto* v : level)
      for (const auto& item : v->items) next.push_back(&item);
    level = std::move(next);
  }
  return shape;
}
// Round CL: StringArray.isin (str and string columns) keeps the values that
// are a str or for which isna(value) holds; isna of a list is an array
// shaped like it, whose truth needs exactly one element; the kept values are
// arrayed again and must stay one-dimensional (as the pandas table filters,
// round CK).
void check_string_isin(const std::vector<Value>& values) {
  std::vector<const Value*> kept;
  for (const auto& v : values) {
    if (v.kind == Kind::kString || value_is_null(v)) {
      kept.push_back(&v);
      continue;
    }
    if (v.kind != Kind::kList) continue;
    const auto shape = object_array_shape({&v});
    std::size_t size = 1;
    for (std::size_t i = 1; i < shape.size(); ++i) size *= shape[i];
    if (size == 0)
      fail("ValueError", "The truth value of an empty array is ambiguous. Use `array.size > 0` to check that an array "
                         "is not empty.");
    if (size > 1)
      fail("ValueError", "The truth value of an array with more than one element is ambiguous. Use a.any() or a.all()");
    const Value* inner = &v;
    while (sequence_value(*inner)) inner = &inner->items.front();
    if (value_is_null(*inner)) kept.push_back(&v);
  }
  if (const auto dims = object_array_shape(kept).size(); dims > 1)
    fail("ValueError", "Buffer has wrong number of dimensions (expected 1, got " + std::to_string(dims) + ")");
}

EventTable boundary_filter(const EventTable& df, const Value& raw_values, const Value& case_glue, const Value& act_key,
                           const Value& positive, bool end) {
  // Round CL: a dict case key groups nothing (it maps the index labels); a
  // dict activity key does not hash as the groupby selection; scalar values
  // fail isin; a dict's keys are the values; a categorical isin hashes the
  // values; set_index then refuses a dict case key.
  const auto isin_values = [&](std::size_t act) {
    if (raw_values.kind != Kind::kList && raw_values.kind != Kind::kTuple && raw_values.kind != Kind::kDict &&
        raw_values.kind != Kind::kSet)
      fail("TypeError", "only list-like objects are allowed to be passed to isin(), you passed a `" +
                            short_type_name(raw_values) + "`");
    // Round DA: isin lists a set in its iteration order.
    Value values = raw_values.kind == Kind::kSet ? Value::list(raw_values.items) : raw_values;
    if (raw_values.kind == Kind::kDict) {
      values = Value::list({});
      for (const auto& entry : raw_values.entries) values.items.push_back(entry.first);
    }
    const auto dtype = column_dtype(df, act);
    if (dtype == "category")
      for (const auto& item : values.items)
        if (const auto type = unhashable_type(item)) fail("TypeError", "unhashable type: '" + *type + "'");
    if ((dtype == "str" || dtype == "string") && raw_values.kind != Kind::kDict) check_string_isin(values.items);
    return values;
  };
  const auto kept_cases = [&](const EventTable& table, std::size_t case_column, std::size_t act, const Value& values) {
    const auto dtype = column_dtype(table, act);
    const auto cases = cases_of(table, case_column);
    const auto boundary = boundary_activities(table, cases, act, end);
    std::set<Key> kept;
    const bool converts = datetime_list_converts(values, dtype);
    for (const auto& [key, a] : boundary)
      if (boundary_isin(a, values, dtype, converts)) kept.insert(key);
    return kept;
  };
  // Round DX: a list case key groups by its items' groupers.
  if (ListCaseKey::applies(case_glue)) {
    ListCaseKey list(case_glue);
    list.group(df, df.size());
    const auto act = selection_key(df, act_key);
    const auto rows = all_rows(df);
    list.factorize(df, rows);
    for (const auto& g : list.groupers)
      if (g.kind == detail::PandasListGrouper::Kind::kColumn) reject_float16_aggregation(df, g.column, !end, inferred_dtype(df, act));
    const auto values = isin_values(act);
    list_set_index(df, case_glue);
    const auto ids = list.identities(df, rows);
    const auto keyed = with_case_column(df, ids);
    const auto column = keyed.columns.size() - 1;
    const auto kept = kept_cases(keyed, column, act, values);
    if (const auto single = list.single_column()) require_case_index(df, *single);
    return without_case_column(keep_cases(with_case_column(df, index_identities(case_glue, ids)), column, kept, truthy(positive)));
  }
  const auto case_column = groupby_key(df, case_glue);
  const auto act = selection_key(df, act_key);
  if (case_column) reject_float16_aggregation(df, *case_column, !end, inferred_dtype(df, act));
  const auto values = isin_values(act);
  set_index_key(case_glue);
  if (!case_column) return subset(df, std::vector<bool>(df.size(), !truthy(positive)));
  const auto kept = kept_cases(df, *case_column, act, values);
  require_case_index(df, *case_column);
  return keep_cases(df, *case_column, kept, truthy(positive));
}

// Counted boundary activities: (activity or null, count) in insertion order.
// The missing boundaries of an object or str column count as one key; in a
// float or masked column each is a NaN object of its own, counted once.
using Counts = std::vector<std::pair<Activity, Value>>;
Counts count_boundaries(const std::vector<Key>& order, const std::map<Key, Activity>& boundary, bool shared_missing) {
  Counts out;
  for (const auto& key : order) {
    const auto& a = boundary.at(key);
    if (!a && !shared_missing) {
      out.emplace_back(a, Value::of(std::int64_t{1}));
      continue;
    }
    bool found = false;
    for (auto& [k, v] : out)
      if (same_activity(k, a)) {
        ++v.integer;
        found = true;
        break;
      }
    if (!found) out.emplace_back(a, Value::of(std::int64_t{1}));
  }
  return out;
}
Counts counts_argument(const Value& dict) {
  Counts out;
  for (const auto& [k, v] : dict.entries)
    out.emplace_back(value_is_null(k) ? Activity{} : scalar_of(k), v);
  return out;
}

struct NoccOutcome {
  EventTable table;
  Counts kept;
  // The counted boundaries of a masked integer column hold a missing one:
  // the column's boundaries then count as floats (round CC).
  bool float_keys = false;
  // The dtype of the counted activity column (round CL).
  std::string dtype = "object";
};
// Round CL: Python's count >= nocc for the values a counted dict holds:
// numbers by value, strs by code point, lists and tuples item by item,
// datetimes by instant (naive and aware do not compare); anything else
// raises the TypeError.
bool python_at_least(const Value& a, const Value& b);
int python_order(const Value& a, const Value& b, const char* op) {
  const auto unsupported = [&]() -> int {
    fail("TypeError", std::string("'") + op + "' not supported between instances of '" + type_name(a) + "' and '" +
                          type_name(b) + "'");
  };
  const auto na = number_of(a), nb = number_of(b);
  if (na && nb) {
    if (na->integral && nb->integral) return na->integer < nb->integer ? -1 : na->integer > nb->integer ? 1 : 0;
    const double x = na->integral ? static_cast<double>(na->integer) : na->real;
    const double y = nb->integral ? static_cast<double>(nb->integer) : nb->real;
    if (std::isnan(x) || std::isnan(y)) return 2;  // unordered: every comparison is false
    return x < y ? -1 : x > y ? 1 : 0;
  }
  if (a.kind == Kind::kString && b.kind == Kind::kString) return a.text < b.text ? -1 : a.text > b.text ? 1 : 0;
  if ((a.kind == Kind::kList && b.kind == Kind::kList) || (a.kind == Kind::kTuple && b.kind == Kind::kTuple)) {
    const auto n = std::min(a.items.size(), b.items.size());
    for (std::size_t i = 0; i < n; ++i)
      if (!pandas_filter_values_equal(a.items[i], b.items[i])) return python_order(a.items[i], b.items[i], op);
    return a.items.size() < b.items.size() ? -1 : a.items.size() > b.items.size() ? 1 : 0;
  }
  if (a.kind == Kind::kDatetime && b.kind == Kind::kDatetime) {
    if (a.offset_minutes.has_value() != b.offset_minutes.has_value())
      fail("TypeError", "can't compare offset-naive and offset-aware datetimes");
    const auto x = a.instant_nanoseconds(), y = b.instant_nanoseconds();
    return x < y ? -1 : x > y ? 1 : 0;
  }
  if (a.kind == Kind::kDate && b.kind == Kind::kDate) return a.integer < b.integer ? -1 : a.integer > b.integer ? 1 : 0;
  return unsupported();
}
bool python_at_least(const Value& a, const Value& b) {
  const int order = python_order(a, b, ">=");
  return order == 0 || order == 1;
}

NoccOutcome nocc_filter(const EventTable& df, const Value& nocc, const std::optional<Value>& count0_argument,
                        const Value& case_glue, const Value& act_key, bool end,
                        const std::optional<Value>& most_common_variant = std::nullopt) {
  // A None count dict is no count dict.
  const auto count0 = count0_argument && count0_argument->kind != Kind::kNone ? count0_argument : std::nullopt;
  // Round CL (observed black-box): the end activities of an empty frame
  // return it before any key is read. The case groupby comes first (a dict
  // case key groups nothing); counts are read from count0 (which must have
  // items()) or from the groupby selection of the activity; each count is
  // compared with nocc as Python does; when not every count is kept, the
  // activity column is read as df[key] and the frame set_index by the case
  // key.
  if (end && df.empty()) return {df, {}, false, "object"};
  const auto case_column = groupby_key(df, case_glue);
  std::optional<std::size_t> act;
  std::string dtype = "object";
  Counts counts;
  std::map<Key, Activity> boundary;
  if (count0) {
    if (count0->kind != Kind::kDict)
      fail("AttributeError", "'" + type_name(*count0) + "' object has no attribute 'items'");
    counts = counts_argument(*count0);
  } else {
    act = selection_key(df, act_key);
    dtype = column_dtype(df, *act);
    // The end activities of an empty frame are counted without a groupby.
    if (case_column && (!end || !df.empty())) reject_float16_aggregation(df, *case_column, !end, dtype);
    if (case_column) {
      const auto cases = cases_of(df, *case_column);
      boundary = boundary_activities(df, cases, *act, end);
      counts = count_boundaries(cases.order, boundary, !masked_dtype(dtype) && !float_bits(dtype));
    }
  }
  Counts kept;
  for (const auto& entry : counts) {
    const bool ok = python_at_least(entry.second, nocc);
    // Round DA: a count below nocc reads the variant's last activity, which
    // a set cannot index.
    if (!ok && most_common_variant && most_common_variant->kind == Kind::kSet && !most_common_variant->items.empty())
      fail("TypeError", "'set' object is not subscriptable");
    // The last activity of the most common variant is always kept; a pd.NA
    // boundary (a "boolean" or "string" column) fails that comparison (round
    // CA); a masked number column's missing boundary is NaN (round CB).
    // Round DA: pd.NA == None, a list, tuple, dict or set is False (fresh
    // CK bank 2026112410 case 837).
    if (!ok && !count0 && !entry.first && most_common_variant && !most_common_variant->items.empty() &&
        (dtype == "boolean" || dtype == "string") && na_compares(most_common_variant->items.back()))
      fail("TypeError", "boolean value of NA is ambiguous");
    const bool protected_end = most_common_variant && !most_common_variant->items.empty() && entry.first &&
                               scalar_of(most_common_variant->items.back()) &&
                               same_activity(entry.first, scalar_of(most_common_variant->items.back()));
    if (ok || protected_end) kept.push_back(entry);
  }
  const bool float_keys = !count0 && masked_dtype(dtype) && (dtype.starts_with("Int") || dtype.starts_with("UInt")) &&
                          std::any_of(boundary.begin(), boundary.end(), [](const auto& entry) { return !entry.second; });
  if (kept.size() == counts.size()) return {df, kept, float_keys, dtype};
  act = getitem_key(df, act_key);
  if (count0) dtype = column_dtype(df, *act);
  set_index_key(case_glue);
  if (count0 && case_column) {
    const auto cases = cases_of(df, *case_column);
    boundary = boundary_activities(df, cases, *act, end);
  }
  std::set<Key> keep;
  // The kept activities are the numpy values of the counted dict: a naive
  // datetime64[ns] activity is then the int of its nanoseconds, which no
  // boundary activity is `isin` (round CB).
  const bool int_keys = dtype == "datetime64[ns]";
  // Round CZ (fresh pandas sequence CK 2026112322 case 339): a datetime64
  // column converts the kept values to datetimes only when one of them is
  // a datetime, so a missing boundary (None) alone matches no NaT.
  const bool none_matches = count0 || !dtype.starts_with("datetime64") ||
                            std::any_of(kept.begin(), kept.end(), [](const auto& entry) { return entry.first.has_value(); });
  for (const auto& [key, a] : boundary)
    for (const auto& entry : kept)
      if (!int_keys && (a || none_matches) && same_activity(entry.first, a)) {
        keep.insert(key);
        break;
      }
  require_case_index(df, *case_column);
  return {keep_cases(df, *case_column, keep, true), kept, float_keys, dtype};
}

// A missing boundary is NaN, or None in an object column (round CA: groupby
// last() of an object column without a present value is None).
// A datetime64 boundary comes back as numpy's tolist() gives it (round CB):
// a naive ns column as the int of its nanoseconds, any other naive unit as
// a datetime.datetime, an aware column (and a Timestamp in an object column)
// as a Timestamp.
Value boundary_value(const python::Scalar& s, const std::string& dtype) {
  if (s.kind != python::Scalar::Kind::kDate) return value_of(s);
  const auto& t = s.date;
  const std::int64_t wall = (t.seconds + (t.timezone_aware ? std::int64_t{t.offset_minutes} * 60 : 0)) * 1'000'000'000 +
                            static_cast<std::int64_t>(t.nanoseconds);
  if (dtype == "datetime64[ns]") return Value::of(wall);
  if (!t.timezone_aware) return Value::datetime(wall);
  const auto comma = dtype.find(", ");
  return Value::datetime(wall, t.offset_minutes,
                         comma == std::string::npos ? std::string() : dtype.substr(comma + 2, dtype.size() - comma - 3));
}

// With float_keys (a masked integer column with a missing boundary, round
// CC) the present boundaries are floats.
Value counts_value(const Counts& counts, const std::string& dtype, bool float_keys) {
  std::vector<std::pair<Value, Value>> entries;
  for (const auto& [k, v] : counts) {
    // A missing boundary of a datetime64 column is NaT, which numpy's
    // tolist() gives as None (round CB).
    auto key = k                                                    ? boundary_value(*k, dtype)
               : dtype == "object" || dtype.starts_with("datetime64") ? Value::none()
                                                                      : Value::of(std::nan(""));
    if (float_keys && key.kind == Kind::kInt) key = Value::of(static_cast<double>(key.integer));
    entries.emplace_back(std::move(key), v);
  }
  return Value::dict(std::move(entries));
}

// ------------------------------------------------------------------ variants

// ",".join(item) of an admitted trace, prefix or suffix.
enum class VariantMode { kTrace, kStartsWith, kEndsWith };

// An admitted pattern as a variant string. The trace filter reads str items
// and None (as "None") and raises on any other item; starts_with and
// ends_with join the items with str.join, which takes str items only.
std::string join_pattern(const Value& item, VariantMode mode) {
  std::string out;
  // Round CS: str.join of a scalar that is no str (fresh pandas sequence CK
  // bank 2026111680 case 913).
  if (mode != VariantMode::kTrace &&
      (item.kind == Kind::kNone || item.kind == Kind::kBool || item.kind == Kind::kInt || item.kind == Kind::kFloat ||
       item.kind == Kind::kDatetime || item.kind == Kind::kDate))
    fail("TypeError", "can only join an iterable");
  // Round CX (probed): the trace filter iterates each admitted trace in
  // turn, so a scalar one fails as not iterable (fresh pandas sequence CK
  // bank 2026112110 case 85).
  if (mode == VariantMode::kTrace &&
      (item.kind == Kind::kNone || item.kind == Kind::kBool || item.kind == Kind::kInt || item.kind == Kind::kFloat ||
       item.kind == Kind::kDatetime || item.kind == Kind::kDate))
    fail("TypeError", "'" + type_name(item) + "' object is not iterable");
  if (item.kind == Kind::kString) {
    // A str iterates over its characters (UTF-8 sequences).
    bool first = true;
    for (std::size_t i = 0; i < item.text.size();) {
      std::size_t len = 1;
      const auto c = static_cast<unsigned char>(item.text[i]);
      if (c >= 0xF0) len = 4;
      else if (c >= 0xE0) len = 3;
      else if (c >= 0xC0) len = 2;
      if (!first) out += ",";
      out += item.text.substr(i, len);
      first = false;
      i += len;
    }
    return out;
  }
  // Round CL: a dict iterates over its keys.
  if (item.kind == Kind::kDict) {
    auto keys = Value::list({});
    for (const auto& entry : item.entries) keys.items.push_back(entry.first);
    return join_pattern(keys, mode);
  }
  for (std::size_t i = 0; i < item.items.size(); ++i) {
    const auto& part = item.items[i];
    // Round CL (observed black-box): the trace filter prints a falsy part
    // (None, 0, 0.0, False, an empty dict) with str().
    const bool falsy = mode == VariantMode::kTrace && !truthy(part) &&
                       (part.kind == Kind::kNone || part.kind == Kind::kInt || part.kind == Kind::kBool ||
                        part.kind == Kind::kFloat || part.kind == Kind::kDict);
    if (part.kind != Kind::kString && !falsy)
      fail("TypeError", mode == VariantMode::kTrace
                            ? "decoding to str: need a bytes-like object, " + type_name(part) + " found"
                            : "sequence item " + std::to_string(i) + ": expected str instance, " + type_name(part) + " found");
    if (i) out += ",";
    if (part.kind == Kind::kString) out += part.text;
    else if (part.kind == Kind::kNone) out += "None";
    else if (part.kind == Kind::kInt) out += "0";
    else if (part.kind == Kind::kBool) out += "False";
    else if (part.kind == Kind::kFloat) out += std::signbit(part.real) ? "-0.0" : "0.0";
    else out += "{}";
  }
  return out;
}

// The Python type of an activity as a case's activity list holds it: a
// numpy column's values are Python scalars, a masked column's are numpy
// scalars (or pd.NA). A UInt64 column whose values all lie past the int64
// range holds numpy's ulonglong instead of uint64.
std::string activity_type_name(const Cell& cell, const std::string& dtype, bool ulonglong,
                               bool datetime_categories = false) {
  // A categorical gap is NaN (NaT for datetime categories), round CA.
  if (dtype == "category" && null_cell(cell)) return datetime_categories ? "NaTType" : "float";
  if (dtype.starts_with("datetime64") && null_cell(cell)) return "NaTType";
  if (masked_dtype(dtype)) {
    if (null_cell(cell)) return "NAType";
    if (dtype == "boolean") return "numpy.bool";
    if (ulonglong) return "numpy.ulonglong";
    std::string name = dtype;
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return "numpy." + name;
  }
  return python::type_name(scalar_of(cell));
}

// Each case's variant string, by first appearance of the case.
std::vector<std::pair<Key, std::string>> variants(const EventTable& df, std::size_t case_column, std::size_t act) {
  const auto cases = cases_of(df, case_column);
  const auto dtype = column_dtype(df, act);
  const bool ulonglong = dtype == "UInt64" && std::all_of(df.rows.begin(), df.rows.end(), [&](const auto& row) {
    if (null_cell(row[act])) return false;
    const auto digits = scalar_of(row[act]).digits;
    return digits.size() > 19 || (digits.size() == 19 && digits > "9223372036854775807");
  });
  const bool datetime_categories = act < df.categorical_dtypes.size() && df.categorical_dtypes[act] &&
                                   df.categorical_dtypes[act]->categories_dtype.starts_with("datetime64[");
  std::vector<std::pair<Key, std::string>> out;
  for (const auto& key : cases.order) {
    const auto& rows = cases.rows.at(key);
    std::string joined;
    for (std::size_t j = 0; j < rows.size(); ++j) {
      const auto& cell = df.rows[rows[j]][act];
      // The activities join with str.join, which takes str items only.
      if (!is_text(cell))
        fail("TypeError", "sequence item " + std::to_string(j) + ": expected str instance, " +
                              activity_type_name(cell, dtype, ulonglong, datetime_categories) + " found");
      if (j) joined += ",";
      joined += cell->value;
    }
    out.emplace_back(key, std::move(joined));
  }
  return out;
}

std::vector<std::string> split_commas(const std::string& text) {
  std::vector<std::string> out(1);
  for (const char c : text) {
    if (c == ',') out.emplace_back();
    else out.back() += c;
  }
  return out;
}

// The trace filter's match: tokens of the joined pattern, where "..." spans
// any run of variant tokens (a run between two wildcards must be nonempty).
bool trace_match(const std::string& variant, const std::string& pattern) {
  const auto tokens = split_commas(pattern);
  bool wildcard = false;
  for (const auto& t : tokens) wildcard = wildcard || t == "...";
  if (!wildcard) return variant == pattern;
  const auto words = split_commas(variant);
  std::vector<std::vector<std::string>> segments(1);
  for (const auto& t : tokens) {
    if (t == "...") segments.emplace_back();
    else segments.back().push_back(t);
  }
  const auto& prefix = segments.front();
  const auto& suffix = segments.back();
  if (words.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i)
    if (words[i] != prefix[i]) return false;
  std::size_t pos = prefix.size();
  for (std::size_t s = 1; s + 1 < segments.size(); ++s) {
    const auto& middle = segments[s];
    if (middle.empty()) return false;
    bool found = false;
    for (std::size_t start = pos; start + middle.size() <= words.size(); ++start) {
      bool same = true;
      for (std::size_t k = 0; k < middle.size() && same; ++k) same = words[start + k] == middle[k];
      if (same) {
        pos = start + middle.size();
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  if (words.size() < pos + suffix.size()) return false;
  for (std::size_t i = 0; i < suffix.size(); ++i)
    if (words[words.size() - suffix.size() + i] != suffix[i]) return false;
  return true;
}

EventTable variant_filter(const EventTable& df, const Value& admitted, const PandasFilterParameters& parameters,
                          VariantMode mode) {
  // Round CL: a dict case key groups nothing (no case keeps a row unless
  // negative); a dict activity key does not hash as the groupby selection.
  const auto case_value =
      literal_first_key(parameters, "CASE_ID_KEY", kPandasFilterCaseIdKeyParameter, kPandasFilterCaseConceptName);
  const auto act_value =
      literal_first_key(parameters, "ACTIVITY_KEY", kPandasFilterActivityKeyParameter, kPandasFilterDefaultNameKey);
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  // Round DX: a list case key groups by its items' groupers (through a
  // synthetic key column, dropped again from the result).
  std::optional<ListCaseKey> list;
  EventTable keyed;
  if (ListCaseKey::applies(case_value)) {
    list.emplace(case_value);
    list->group(df, df.size());
  }
  const auto grouped = list ? std::optional<std::size_t>(df.columns.size()) : groupby_key(df, case_value);
  const auto act = selection_key(df, act_value);
  std::vector<std::optional<std::string>> ids;
  if (list) {
    const auto rows = all_rows(df);
    list->factorize(df, rows);
    for (const auto& g : list->groupers)
      if (g.kind == detail::PandasListGrouper::Kind::kColumn) reject_float16_groupby(df, g.column);
    ids = list->identities(df, rows);
    keyed = with_case_column(df, ids);
    // No group at all: nothing is selected or indexed.
    if (std::none_of(ids.begin(), ids.end(), [](const auto& id) { return id.has_value(); }))
      return subset(df, std::vector<bool>(df.size(), !positive));
  }
  const auto& table = list ? keyed : df;
  if (!grouped) return subset(df, std::vector<bool>(df.size(), !positive));
  const auto case_column = *grouped;
  if (!list) reject_float16_groupby(df, case_column);
  // The case variants are joined before the admitted patterns, which are
  // not read at all without a case.
  const auto case_variants = variants(table, case_column, act);
  std::vector<std::string> patterns;
  if (!case_variants.empty())
    for (const auto& item : admitted.items) patterns.push_back(join_pattern(item, mode));
  std::set<Key> kept;
  for (const auto& [key, variant] : case_variants) {
    // An empty list of admitted traces admits every case.
    bool ok = mode == VariantMode::kTrace && patterns.empty();
    for (const auto& p : patterns) {
      if (ok) break;
      if (mode == VariantMode::kTrace) ok = trace_match(variant, p);
      else if (mode == VariantMode::kStartsWith) ok = variant.compare(0, p.size(), p) == 0 && variant.size() >= p.size();
      else ok = variant.size() >= p.size() && variant.compare(variant.size() - p.size(), p.size(), p) == 0;
      if (ok) break;
    }
    if (ok) kept.insert(key);
  }
  if (list) {
    list_set_index(df, case_value);
    if (const auto single = list->single_column()) require_case_index(df, *single);
    return without_case_column(keep_cases(with_case_column(df, index_identities(case_value, ids)), case_column, kept, positive));
  }
  require_case_index(df, case_column);
  return keep_cases(df, case_column, kept, positive);
}

// ------------------------------------------------------------------ positions

std::string index_repr(const std::vector<std::string>& names) {
  std::string out = "Index([";
  for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + py_repr(names[i]);
  return out + "], dtype='str')";
}

// df[[a, b]] column selection failures.
void require_columns(const EventTable& df, std::vector<std::string> names) {
  std::vector<std::string> unique;
  for (const auto& n : names)
    if (std::find(unique.begin(), unique.end(), n) == unique.end()) unique.push_back(n);
  names = unique;
  std::vector<std::string> missing;
  for (const auto& n : names)
    if (!find_column(df, n)) missing.push_back(n);
  if (missing.empty()) return;
  if (missing.size() == names.size()) key_error("None of [" + index_repr(names) + "] are in the [columns]");
  std::string list = "[";
  for (std::size_t i = 0; i < missing.size(); ++i) list += (i ? ", " : "") + py_repr(missing[i]);
  key_error(list + "] not in index");
}

// Position of each row in its case (absent for null case ids).
std::vector<std::optional<std::int64_t>> positions(const EventTable& df, std::size_t case_column) {
  std::vector<std::optional<std::int64_t>> out(df.size());
  std::map<Key, std::int64_t> next;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (null_cell(c)) continue;
    out[i] = next[key_of(*c)]++;
  }
  return out;
}

EventTable position_filter(const EventTable& df, const Value& activity, const PandasFilterParameters& parameters,
                           bool prefix) {
  const auto case_value = case_key(parameters);
  const auto first_or_last = parameter_or(parameters, "FIRST_OR_LAST", "first_or_last", Value::of("first"));
  const bool strict = truthy(parameter_or(parameters, "STRICT", "strict", Value::of(true)));
  // Round CL (observed black-box): the case groupby (a dict case key maps
  // the index labels), df[act], df[act] == activity (a list, tuple or dict
  // activity can fail there), the first/last choice, then a dict case key
  // fails to hash.
  const bool first = first_or_last.kind == Kind::kString && first_or_last.text == "first";
  const bool last = first_or_last.kind == Kind::kString && first_or_last.text == "last";
  // Round DX: a list case key. The positions come from a groupby over the
  // frame (cumcount), the occurrences from a groupby over the matching rows
  // (an array must then be as long as they are); reset_index inserts the
  // levels again (a column named twice already exists), and selecting
  // [key, index] does not hash the list.
  if (ListCaseKey::applies(case_value)) {
    ListCaseKey whole(case_value);
    whole.group(df, df.size());
    whole.factorize(df, all_rows(df));
    const auto act = getitem_key(df, activity_key(parameters));
    const auto matching = series_equals(df, act, activity);
    const auto filtered = subset(df, matching);
    ListCaseKey occurrences(case_value);
    occurrences.group(filtered, filtered.size());
    if (!first && !last) fail("AttributeError", "'DataFrameGroupBy' object has no attribute 'reset_index'");
    occurrences.factorize(filtered, all_rows(filtered));
    std::set<std::string> columns{"@@index_in_trace"};
    std::set<std::size_t> grouping;
    for (const auto& g : occurrences.groupers)
      if (g.kind == detail::PandasListGrouper::Kind::kColumn) grouping.insert(g.column);
    for (std::size_t c = 0; c < df.columns.size(); ++c)
      if (!grouping.count(c)) columns.insert(df.columns[c]);
    const auto levels = occurrences.groupers.size();
    for (std::size_t l = levels; l-- > 0;) {
      const auto& g = occurrences.groupers[l];
      const auto name = g.kind == detail::PandasListGrouper::Kind::kColumn ? df.columns[g.column]
                        : levels > 1                                      ? "level_" + std::to_string(l)
                        : columns.count("index")                          ? std::string("level_0")
                                                                          : std::string("index");
      if (!columns.insert(name).second) fail("ValueError", "cannot insert " + name + ", already exists");
    }
    fail("TypeError", "unhashable type: 'list'");
  }
  const auto grouped = groupby_key(df, case_value);
  const auto act = getitem_key(df, activity_key(parameters));
  const auto matching = series_equals(df, act, activity);
  if (grouped) require_sorted_groupby(df, *grouped);
  if (!first && !last) fail("AttributeError", "'DataFrameGroupBy' object has no attribute 'reset_index'");
  if (!grouped) hash_key(case_value);
  const auto case_column = *grouped;
  const auto index = positions(df, case_column);
  std::map<Key, std::int64_t> occurrence;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (!index[i] || !matching[i]) continue;
    const auto key = key_of(*c);
    if (first) occurrence.emplace(key, *index[i]);
    else occurrence[key] = *index[i];
  }
  // Each row looks its case up in the occurrences. A masked integer case
  // column holding NA is read as float64 for the lookup, so ids one double
  // holds share an entry: the largest id with an occurrence supplies it.
  const auto dtype = column_dtype(df, case_column);
  const bool by_double = masked_dtype(dtype) && (dtype.starts_with("Int") || dtype.starts_with("UInt")) &&
                         std::any_of(df.rows.begin(), df.rows.end(),
                                     [&](const auto& row) { return null_cell(row[case_column]); });
  const auto as_double = [](const python::Scalar& id) {
    const auto magnitude = static_cast<double>(std::strtoull(id.digits.c_str(), nullptr, 10));
    return id.negative ? -magnitude : magnitude;
  };
  std::map<double, std::pair<python::Scalar, std::int64_t>> by_value;
  if (by_double)
    for (std::size_t i = 0; i < df.size(); ++i) {
      if (!index[i]) continue;
      const auto found = occurrence.find(key_of(*df.rows[i][case_column]));
      if (found == occurrence.end()) continue;
      const auto id = scalar_of(df.rows[i][case_column]);
      auto [entry, added] = by_value.emplace(as_double(id), std::make_pair(id, found->second));
      const auto& kept = entry->second.first;
      if (!added && python::compare_integers(id.negative, id.digits, kept.negative, kept.digits) > 0)
        entry->second = {id, found->second};
    }
  std::vector<std::optional<std::int64_t>> temp(df.size());
  bool index_null = false, temp_null = df.empty();
  // A categorical case column maps its categories (round CA, observed
  // black-box): when every category has an occurrence and no two share one,
  // the lookup is itself categorical and comparing the positions with it
  // raises; a category without an occurrence makes it float64, even when no
  // row holds that category.
  if (dtype == "category") {
    const auto* categorical = case_column < df.categorical_dtypes.size() && df.categorical_dtypes[case_column]
                                  ? &*df.categorical_dtypes[case_column] : nullptr;
    bool missing = false, repeated = false;
    std::set<std::int64_t> seen;
    if (categorical)
      for (const auto& category : categorical->categories) {
        const auto found = occurrence.find(key_of(category));
        if (found == occurrence.end()) missing = true;
        else if (!seen.insert(found->second).second) repeated = true;
      }
    if (categorical && !missing && !repeated) {
      if (categorical->ordered) {
        const char* op = prefix ? (strict ? "gt" : "ge") : (strict ? "lt" : "le");
        fail("TypeError", std::string("Cannot compare a Categorical for op __") + op +
                              "__ with type <class 'numpy.ndarray'>.\nIf you want to compare values, use "
                              "'np.asarray(cat) <op> other'.");
      }
      fail("TypeError", "Unordered Categoricals can only compare equality or not");
    }
    if (missing) temp_null = true;
  }
  for (std::size_t i = 0; i < df.size(); ++i) {
    if (!index[i]) {
      index_null = temp_null = true;
      continue;
    }
    if (by_double) {
      const auto found = by_value.find(as_double(scalar_of(df.rows[i][case_column])));
      if (found == by_value.end()) temp_null = true;
      else temp[i] = found->second.second;
      continue;
    }
    const auto found = occurrence.find(key_of(*df.rows[i][case_column]));
    if (found == occurrence.end()) temp_null = true;
    else temp[i] = found->second;
  }
  EventTable out = df;
  const std::string index_column = "@@index_in_trace", temp_column = "@@temp_column";
  out.columns.push_back(index_column);
  out.columns.push_back(temp_column);
  if (!out.dtypes.empty()) {
    out.dtypes.push_back(index_null ? "float64" : "int64");
    out.dtypes.push_back(temp_null ? "float64" : "int64");
  }
  if (!out.categorical_dtypes.empty()) {
    out.categorical_dtypes.emplace_back();
    out.categorical_dtypes.emplace_back();
  }
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    auto& row = out.rows[i];
    if (index_null) row.push_back(float_cell(index_column, index[i] ? static_cast<double>(*index[i]) : std::nan("")));
    else row.push_back(int_cell(index_column, *index[i]));
    if (temp_null) row.push_back(float_cell(temp_column, temp[i] ? static_cast<double>(*temp[i]) : std::nan("")));
    else row.push_back(int_cell(temp_column, *temp[i]));
    if (index[i] && temp[i]) {
      const auto x = *index[i], t = *temp[i];
      keep[i] = prefix ? (strict ? x < t : x <= t) : (strict ? x > t : x >= t);
    }
  }
  return subset(out, keep);
}

// ------------------------------------------------------------------ grouping

EventTable merge_groups(const EventTable& df, const std::vector<std::vector<std::size_t>>& groups, bool first,
                        const std::vector<std::size_t>& column_order) {
  EventTable out = df;
  out.rows.clear();
  out.columns.clear();
  out.dtypes.clear();
  out.categorical_dtypes.clear();
  for (const auto c : column_order) {
    out.columns.push_back(df.columns[c]);
    if (!df.dtypes.empty()) out.dtypes.push_back(df.dtypes[c]);
    if (!df.categorical_dtypes.empty()) out.categorical_dtypes.push_back(df.categorical_dtypes[c]);
  }
  for (const auto& members : groups) {
    EventTable::Row row;
    for (const auto c : column_order) {
      Cell chosen = df.rows[first ? members.front() : members.back()][c];
      for (std::size_t j = 0; j < members.size(); ++j) {
        const auto& cell = df.rows[first ? members[j] : members[members.size() - 1 - j]][c];
        if (!null_cell(cell)) {
          chosen = cell;
          break;
        }
      }
      // first()/last() of an object column with no present value is None,
      // whichever missing values (None, NaN, pd.NA) the group held.
      if (null_cell(chosen) && c < df.dtypes.size() && df.dtypes[c] == "object") {
        EventAttribute none{df.columns[c], "", AttributeKind::kString};
        none.is_null = true;
        chosen = std::move(none);
      }
      row.push_back(chosen);
    }
    out.rows.push_back(std::move(row));
  }
  reset_index(out);
  return out;
}

std::vector<std::size_t> identity_order(std::size_t n) {
  std::vector<std::size_t> out(n);
  for (std::size_t i = 0; i < n; ++i) out[i] = i;
  return out;
}

}  // namespace

// ------------------------------------------------------------------ public API

void pandas_check_filter_parameters_carrier(const PandasFilterValue& parameters) {
  if (parameters.kind == Kind::kNone || parameters.kind == Kind::kDict) return;
  if (parameters.kind == Kind::kList || parameters.kind == Kind::kTuple) {
    const auto name = type_name(parameters);
    const auto size = static_cast<std::int64_t>(parameters.items.size());
    for (const auto& key : parameters.items) {
      if (key.kind != Kind::kInt && key.kind != Kind::kBool)
        fail("TypeError", name + " indices must be integers or slices, not " + type_name(key));
      const std::int64_t index = key.kind == Kind::kBool ? (key.boolean ? 1 : 0) : key.integer;
      if (index < -size || index >= size) fail("IndexError", name + " index out of range");
    }
    return;
  }
  // Round DA: a set iterates its keys but cannot be indexed by them.
  if (parameters.kind == Kind::kSet) {
    if (!parameters.items.empty()) fail("TypeError", "'set' object is not subscriptable");
    return;
  }
  if (parameters.kind == Kind::kString) {
    if (parameters.text.empty()) fail("TypeError", "'in <string>' requires string as left operand, not Parameters");
    fail("TypeError", "string indices must be integers, not 'str'");
  }
  fail("TypeError", "'" + type_name(parameters) + "' object is not iterable");
}

EventTable pandas_filter_df_on_start_activities(const EventTable& df, const PandasFilterValue& values,
                                                const PandasFilterValue& case_id_glue,
                                                const PandasFilterValue& activity_key,
                                                const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, false);
}
EventTable pandas_filter_df_on_start_activities(const EventTable& df, const PandasFilterValue& values,
                                                const std::string& case_id_glue, const std::string& activity_key,
                                                const PandasFilterValue& positive) {
  return boundary_filter(df, values, Value::of(case_id_glue), Value::of(activity_key), positive, false);
}

EventTable pandas_filter_df_on_end_activities(const EventTable& df, const PandasFilterValue& values,
                                              const PandasFilterValue& case_id_glue,
                                              const PandasFilterValue& activity_key,
                                              const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, true);
}
EventTable pandas_filter_df_on_end_activities(const EventTable& df, const PandasFilterValue& values,
                                              const std::string& case_id_glue, const std::string& activity_key,
                                              const PandasFilterValue& positive) {
  return boundary_filter(df, values, Value::of(case_id_glue), Value::of(activity_key), positive, true);
}

EventTable pandas_start_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                         const PandasFilterParameters& parameters) {
  return pandas_filter_df_on_start_activities(df, values, case_key(parameters), activity_key(parameters),
                                              parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
}

EventTable pandas_end_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                       const PandasFilterParameters& parameters) {
  return pandas_filter_df_on_end_activities(df, values, case_key(parameters), activity_key(parameters),
                                            parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
}

EventTable pandas_filter_df_on_start_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                     const std::optional<PandasFilterValue>& sa_count0,
                                                     const PandasFilterValue& case_id_glue,
                                                     const PandasFilterValue& activity_key) {
  return nocc_filter(df, nocc, sa_count0, case_id_glue, activity_key, false).table;
}
EventTable pandas_filter_df_on_start_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                     const std::optional<PandasFilterValue>& sa_count0,
                                                     const std::string& case_id_glue,
                                                     const std::string& activity_key) {
  return nocc_filter(df, nocc, sa_count0, Value::of(case_id_glue), Value::of(activity_key), false).table;
}

PandasEndActivitiesNocc pandas_filter_df_on_end_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                                const std::optional<PandasFilterValue>& ea_count0,
                                                                const PandasFilterValue& case_id_glue,
                                                                const PandasFilterValue& activity_key,
                                                                bool return_dict,
                                                                const std::optional<PandasFilterValue>& most_common_variant) {
  auto outcome = nocc_filter(df, nocc, ea_count0, case_id_glue, activity_key, true, most_common_variant);
  PandasEndActivitiesNocc out{std::move(outcome.table), std::nullopt};
  if (return_dict && !df.empty()) out.counts = counts_value(outcome.kept, outcome.dtype, outcome.float_keys);
  return out;
}
PandasEndActivitiesNocc pandas_filter_df_on_end_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                                const std::optional<PandasFilterValue>& ea_count0,
                                                                const std::string& case_id_glue,
                                                                const std::string& activity_key, bool return_dict,
                                                                const std::optional<PandasFilterValue>& most_common_variant) {
  return pandas_filter_df_on_end_activities_nocc(df, nocc, ea_count0, Value::of(case_id_glue), Value::of(activity_key),
                                                 return_dict, most_common_variant);
}

EventTable pandas_trace_filter_apply(const EventTable& df, const PandasFilterValue& admitted_traces,
                                     const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_traces, parameters, VariantMode::kTrace);
}
EventTable pandas_starts_with_filter_apply(const EventTable& df, const PandasFilterValue& admitted_prefixes,
                                           const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_prefixes, parameters, VariantMode::kStartsWith);
}
EventTable pandas_ends_with_filter_apply(const EventTable& df, const PandasFilterValue& admitted_suffixes,
                                         const PandasFilterParameters& parameters) {
  return variant_filter(df, admitted_suffixes, parameters, VariantMode::kEndsWith);
}

EventTable pandas_rework_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  const auto case_value = case_key(parameters);
  const auto act_value = activity_key(parameters);
  const auto minimum = parameter_or(parameters, "MIN_OCCURRENCES", "min_occurrences", Value::of(std::int64_t{2}));
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  // Round CL: the two keys are hashed (activity first) before the columns
  // are selected.
  if (act_value.kind == Kind::kDict || act_value.kind == Kind::kList || act_value.kind == Kind::kSet) hash_key(act_value);
  if (case_value.kind == Kind::kDict || case_value.kind == Kind::kList || case_value.kind == Kind::kSet) hash_key(case_value);
  const auto case_name = key_text(case_value);
  const auto act_name = key_text(act_value);
  require_columns(df, {act_name, case_name});
  const auto case_column = *find_column(df, case_name);
  const auto act = *find_column(df, act_name);
  // Round CL: df[act] == activity comes first (a list, tuple or dict
  // activity can fail there).
  const auto matching = series_equals(df, act, activity);
  const auto bound = number_of(minimum);
  if (!bound) fail("TypeError", "unsupported operand type(s) for -: '" + type_name(minimum) + "' and 'int'");
  const Number threshold = bound->integral ? Number{true, bound->integer - 1, 0.0} : Number{false, 0, bound->real - 1};
  // The occurrences are counted by a sorted groupby over the matching rows.
  reject_float16_groupby(df, act);
  require_sorted_groupby(df, case_column, &matching);
  std::map<Key, std::int64_t> seen;
  std::set<Key> kept;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    if (null_cell(c) || !matching[i]) continue;
    const auto key = key_of(*c);
    const auto occurrence = seen[key]++;
    if (int_at_least(occurrence, threshold)) kept.insert(key);
  }
  // The reworked cases come back as the numpy values of the groupby index:
  // a naive datetime64[ns] id is then the int of its nanoseconds, which no
  // datetime cell is `isin` (round CB).
  if (column_dtype(df, case_column) == "datetime64[ns]") kept.clear();
  return keep_cases(df, case_column, kept, positive);
}

EventTable pandas_prefix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  return position_filter(df, activity, parameters, true);
}
EventTable pandas_suffix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                      const PandasFilterParameters& parameters) {
  return position_filter(df, activity, parameters, false);
}

// A numeric column dtype: numpy's bool, intN, uintN and floatN, or pandas'
// masked boolean, IntN, UIntN and FloatN (which hold pd.NA).
struct NumericDtype {
  enum class Kind { kBool, kSigned, kUnsigned, kFloat };
  Kind kind = Kind::kBool;
  int bits = 8;
  bool masked = false;
};

std::optional<NumericDtype> numeric_dtype(const std::string& name) {
  using K = NumericDtype::Kind;
  if (name == "bool") return NumericDtype{K::kBool, 8, false};
  if (name == "boolean") return NumericDtype{K::kBool, 8, true};
  const std::string_view text = name;
  K kind;
  std::string_view width;
  bool masked = false;
  if (text.starts_with("uint")) { kind = K::kUnsigned; width = text.substr(4); }
  else if (text.starts_with("int")) { kind = K::kSigned; width = text.substr(3); }
  else if (text.starts_with("float")) { kind = K::kFloat; width = text.substr(5); }
  else if (text.starts_with("UInt")) { kind = K::kUnsigned; width = text.substr(4); masked = true; }
  else if (text.starts_with("Int")) { kind = K::kSigned; width = text.substr(3); masked = true; }
  else if (text.starts_with("Float")) { kind = K::kFloat; width = text.substr(5); masked = true; }
  else return std::nullopt;
  const int bits = width == "8" ? 8 : width == "16" ? 16 : width == "32" ? 32 : width == "64" ? 64 : 0;
  if (!bits || (kind == K::kFloat && bits == 8) || (masked && kind == K::kFloat && bits == 16)) return std::nullopt;
  return NumericDtype{kind, bits, masked};
}

// numpy's result type of a + b (a masked side makes the result masked):
// bool joins the other side (bool + bool stays bool), integers of one
// signedness take the wider width, a signed and an unsigned integer take the
// next signed width that holds both (float64 past 64 bits), and a float side
// takes the narrowest float that holds both (an 8-bit integer fits float16,
// a 16-bit one float32, wider ones float64).
NumericDtype promote(const NumericDtype& a, const NumericDtype& b) {
  using K = NumericDtype::Kind;
  NumericDtype result = a;
  result.masked = a.masked || b.masked;
  if (a.kind == K::kBool) { result.kind = b.kind; result.bits = b.bits; return result; }
  if (b.kind == K::kBool) return result;
  if (a.kind == K::kFloat || b.kind == K::kFloat) {
    result.kind = K::kFloat;
    result.bits = 16;
    for (const auto* side : {&a, &b})
      result.bits = std::max(result.bits, side->kind == K::kFloat ? side->bits
                                          : side->bits <= 8 ? 16 : side->bits <= 16 ? 32 : 64);
  } else if (a.kind == b.kind) {
    result.bits = std::max(a.bits, b.bits);
  } else {
    const auto& signed_side = a.kind == K::kSigned ? a : b;
    const auto& unsigned_side = a.kind == K::kSigned ? b : a;
    result.kind = K::kSigned;
    if (signed_side.bits > unsigned_side.bits) result.bits = signed_side.bits;
    else if (unsigned_side.bits < 64) result.bits = unsigned_side.bits * 2;
    else { result.kind = K::kFloat; result.bits = 64; }
  }
  return result;
}

// One run key of two numeric columns: pd.NA, or the sum in the result dtype
// (integers as two's-complement bits sign- or zero-extended to 64 bits,
// floats and bools as doubles).
struct NumericKey {
  bool na = false;
  std::uint64_t bits = 0;
  double real = 0.0;
};

NumericKey numeric_key(const Cell& left, const NumericDtype& left_dtype, const Cell& right,
                       const NumericDtype& right_dtype, const NumericDtype& result) {
  using K = NumericDtype::Kind;
  NumericKey key;
  // A missing cell of a masked column is pd.NA (pandas reads NaN into a
  // masked column as NA); in a numpy column only a float can be missing.
  const auto value = [&](const Cell& cell, const NumericDtype& dtype, std::uint64_t& bits, double& real) {
    if (null_cell(cell)) {
      if (dtype.masked || dtype.kind != K::kFloat) key.na = true;
      real = std::numeric_limits<double>::quiet_NaN();
      return;
    }
    const auto s = scalar_of(cell);
    if (s.kind == python::Scalar::Kind::kBool) { bits = s.boolean; real = s.boolean; return; }
    if (s.kind == python::Scalar::Kind::kFloat) { real = s.real; return; }
    const std::uint64_t magnitude = std::strtoull(s.digits.c_str(), nullptr, 10);
    bits = s.negative ? std::uint64_t(0) - magnitude : magnitude;
    real = s.negative ? static_cast<double>(static_cast<std::int64_t>(bits)) : static_cast<double>(magnitude);
  };
  std::uint64_t a_bits = 0, b_bits = 0;
  double a_real = 0, b_real = 0;
  value(left, left_dtype, a_bits, a_real);
  value(right, right_dtype, b_bits, b_real);
  if (key.na) return key;
  if (result.kind == K::kBool) {
    key.bits = (a_bits | b_bits) & 1u;
    key.real = static_cast<double>(key.bits);
  } else if (result.kind == K::kFloat) {
    if (result.bits == 64) key.real = a_real + b_real;
    else if (result.bits == 32) key.real = static_cast<float>(a_real) + static_cast<float>(b_real);
    else key.real = half_to_float(float_to_half(half_to_float(float_to_half(static_cast<float>(a_real))) +
                                                half_to_float(float_to_half(static_cast<float>(b_real)))));
    if (result.masked && std::isnan(key.real)) key.na = true;
  } else {
    std::uint64_t sum = a_bits + b_bits;
    if (result.bits < 64) {
      sum &= (std::uint64_t(1) << result.bits) - 1;
      if (result.kind == K::kSigned && (sum >> (result.bits - 1)) & 1u) sum |= ~((std::uint64_t(1) << result.bits) - 1);
    }
    key.bits = sum;
    key.real = result.kind == K::kSigned ? static_cast<double>(static_cast<std::int64_t>(sum)) : static_cast<double>(sum);
  }
  return key;
}

// Whether row i continues row i - 1's run: pandas compares the key column
// with its shifted copy. A numpy integer key shifts to float64, so
// neighbouring keys are equal when their doubles are; float keys compare in
// their own width (NaN never equal); a bool key shifts to object and compares
// exactly. A masked key keeps its dtype and an NA comparison starts no run.
bool same_run(const NumericKey& key, const NumericKey& previous, const NumericDtype& result) {
  using K = NumericDtype::Kind;
  if (result.masked) {
    if (key.na || previous.na) return true;
    return result.kind == K::kFloat ? key.real == previous.real : key.bits == previous.bits;
  }
  if (key.na || previous.na) return false;
  return result.kind == K::kBool ? key.bits == previous.bits : key.real == previous.real;
}

// A datetime64 side of the run key (round CB). DatetimeArray refuses a
// second datetime array, any integer array and a naive float or boolean
// array as a whole; an aware one adds a float or boolean array elementwise,
// so the first row's pair raises. A str/"string" partner adds where both
// cells are present: one such pair raises as the DatetimeArray, more as the
// first Timestamp. An object partner adds one row as the DatetimeArray and
// more rows elementwise, NaT + an int (or a Timestamp) giving NaT. Without a
// failure every key is missing: returns whether neighbouring keys continue
// a run (a "string" key's pd.NA does).
std::optional<bool> datetime_run_key(const EventTable& df, std::size_t left, std::size_t right, const std::string& left_dtype,
                                     const std::string& right_dtype) {
  const auto is_dt = [](const std::string& d) { return d.starts_with("datetime64"); };
  if (!is_dt(left_dtype) && !is_dt(right_dtype)) return std::nullopt;
  const std::string kAddInt =
      " is no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`";
  if (is_dt(left_dtype) && is_dt(right_dtype)) fail("TypeError", "cannot add DatetimeArray and DatetimeArray");
  const bool dt_left = is_dt(left_dtype);
  const auto dt_column = dt_left ? left : right, other_column = dt_left ? right : left;
  const auto& dt_dtype = dt_left ? left_dtype : right_dtype;
  const auto& other = dt_left ? right_dtype : left_dtype;
  const bool aware = dt_dtype.find(", ") != std::string::npos;
  const auto add_int = [&](const std::string& with) {
    fail("TypeError", "Addition/subtraction of integers and integer-arrays with " + with + kAddInt);
  };
  const auto operands = [&](const std::string& dt_name, const std::string& other_name) {
    fail("TypeError", "unsupported operand type(s) for +: '" + (dt_left ? dt_name : other_name) + "' and '" +
                          (dt_left ? other_name : dt_name) + "'");
  };
  const auto element = [&](const Cell& cell) { return null_cell(cell) ? "NaTType" : "Timestamp"; };
  const auto numeric = numeric_dtype(other);
  if (numeric && (numeric->kind == NumericDtype::Kind::kSigned || numeric->kind == NumericDtype::Kind::kUnsigned))
    add_int("DatetimeArray");
  if (numeric && numeric->kind != NumericDtype::Kind::kSigned && numeric->kind != NumericDtype::Kind::kUnsigned &&
      (numeric->masked || numeric->kind == NumericDtype::Kind::kFloat)) {
    const bool real = numeric->kind == NumericDtype::Kind::kFloat;
    if (!aware) {
      if (!real) fail("ValueError", "Could not convert object to NumPy datetime");
      const auto unit = dt_dtype.substr(11, dt_dtype.find(']') - 11);
      const std::string m8 = "dtype('<M8[" + unit + "]')";
      const std::string f = "dtype('float" + std::to_string(numeric->bits) + "')";
      fail("UFuncTypeError", "ufunc 'add' cannot use operands with types " + (dt_left ? m8 : f) + " and " + (dt_left ? f : m8));
    }
    if (df.empty()) return false;
    operands(element(df.rows[0][dt_column]), real ? "float" : "bool");
  }
  if (other == "str" || other == "string") {
    std::size_t present = 0;
    for (const auto& row : df.rows)
      if (!null_cell(row[dt_column]) && !null_cell(row[other_column])) ++present;
    if (present == 1) fail("TypeError", "unsupported operand type(s) for +: 'DatetimeArray' and 'str'");
    if (present > 1) fail("TypeError", "unsupported operand type(s) for +: 'Timestamp' and 'str'");
    return other == "string";
  }
  if (other == "object") {
    const auto name = [](const Cell& cell) { return python::type_name(scalar_of(cell)); };
    if (df.size() == 1) {
      const auto& cell = df.rows[0][other_column];
      const auto type = name(cell);
      if (type == "int") add_int("DatetimeArray");
      if (type == "Timestamp") fail("TypeError", "cannot add DatetimeArray and Timestamp");
      fail("TypeError", "unsupported operand type(s) for +: 'DatetimeArray' and '" + type + "'");
    }
    for (const auto& row : df.rows) {
      const auto type = name(row[other_column]);
      const bool nat = null_cell(row[dt_column]);
      if ((type == "int" || type == "Timestamp") && nat) continue;
      if (type == "int") add_int("Timestamp");
      fail("TypeError", std::string("unsupported operand type(s) for +: '") + element(row[dt_column]) + "' and '" + type + "'");
    }
    return false;
  }
  return std::nullopt;
}

namespace {

// Round DZ (Review DZ1-1): the run key of a list case key. pm4py adds
// df[key] (a DataFrame of the listed columns) and df[act] (a Series), which
// pandas aligns on the frame's columns: the result's columns are the union of
// the listed names and the row labels, the frame gains an all-NaN float64
// column per row label and the Series is reindexed to the union (a listed
// name gets the Series' missing value). Each column of that union then adds
// its value of the Series, column by column in union order, and the first
// one that raises is the error; a result of several columns cannot be
// assigned to the single run key column. Observed black-box on pandas 3 in
// the reference venv (Python model dz/ca/model.py, 0 differences on 23,400
// fuzzed keys of the sequence domains' frames).
namespace consecutive_list_key {

using SK = python::Scalar::Kind;

const std::string kConcatenation =
    "Concatenation operation is not implemented for NumPy arrays, use np.concatenate() instead. Please do not rely "
    "on this error; it may not be given on all Python implementations.";
const std::string kAddInt = " is no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`";

// A Python object of a column or of the reindexed Series: a value (numpy
// marks a numpy scalar of a masked array, whose categorical and datetime
// arithmetic differ from Python's), NaN (a Python float, or numpy's), pd.NA,
// NaT or None.
struct Operand {
  enum class Kind { kValue, kNaN, kNA, kNaT, kNone } kind = Kind::kValue;
  python::Scalar value;
  bool numpy = false;
};
Operand of_kind(Operand::Kind kind, bool numpy = false) {
  Operand o;
  o.kind = kind;
  o.numpy = numpy;
  return o;
}
Operand of_value(python::Scalar value, bool numpy = false) {
  Operand o;
  o.value = std::move(value);
  o.numpy = numpy;
  if (o.value.kind == SK::kNone) o.kind = Operand::Kind::kNone;
  else if (o.value.kind == SK::kNA) o.kind = Operand::Kind::kNA;
  else if (o.value.kind == SK::kFloat && std::isnan(o.value.real)) o.kind = Operand::Kind::kNaN;
  return o;
}
bool missing(const Operand& o) { return o.kind != Operand::Kind::kValue; }
bool is_date(const Operand& o) { return o.kind == Operand::Kind::kValue && o.value.kind == SK::kDate; }
bool is_int(const Operand& o) { return o.kind == Operand::Kind::kValue && o.value.kind == SK::kInt; }
bool is_text_operand(const Operand& o) { return o.kind == Operand::Kind::kValue && o.value.kind == SK::kText; }
std::string type_of(const Operand& o) {
  switch (o.kind) {
    case Operand::Kind::kNaN: return "float";
    case Operand::Kind::kNA: return "NAType";
    case Operand::Kind::kNaT: return "NaTType";
    case Operand::Kind::kNone: return "NoneType";
    case Operand::Kind::kValue: break;
  }
  return python::type_name(o.value);
}
python::Scalar as_python(const Operand& o) {
  switch (o.kind) {
    case Operand::Kind::kNaN: return python::real(std::numeric_limits<double>::quiet_NaN());
    case Operand::Kind::kNone: return python::none();
    case Operand::Kind::kNA: {
      python::Scalar s;
      s.kind = SK::kNA;
      return s;
    }
    default: return o.value;
  }
}
[[noreturn]] void unsupported(const std::string& left, const std::string& right) {
  fail("TypeError", "unsupported operand type(s) for +: '" + left + "' and '" + right + "'");
}

// Python's a + b of two present objects (a Timestamp next to an int raises
// pandas' message, str + anything else names the other type).
void value_add(const Operand& a, const Operand& b) { (void)python_add(as_python(a), as_python(b)); }
// NaT + b: NaT absorbs an int (not a bool), a Timestamp and NaT.
void nat_add(const Operand& b) {
  if (b.kind == Operand::Kind::kNaT || is_date(b) || is_int(b)) return;
  unsupported("NaTType", type_of(b));
}
// a + b for a Timestamp or NaT `a` of a datetime array.
void date_add(const Operand& a, const Operand& b) {
  if (a.kind == Operand::Kind::kNaT) return nat_add(b);
  if (b.kind == Operand::Kind::kNaT) return;
  if (is_int(b)) fail("TypeError", "Addition/subtraction of integers and integer-arrays with Timestamp" + kAddInt);
  unsupported("Timestamp", type_of(b));
}

// The number of code points of a UTF-8 text (numpy's <U width).
std::size_t utf8_code_points(const std::string& text) {
  std::size_t count = 0;
  for (const unsigned char c : text)
    if ((c & 0xC0) != 0x80) ++count;
  return count;
}

// What a column dtype is to the addition.
struct ColumnKind {
  enum class Family { kCategory, kDatetime, kNumeric, kString, kObject, kOther } family = Family::kOther;
  bool aware = false;       // kDatetime
  std::string unit;         // kDatetime
  std::string numpy_name;   // kNumeric: the numpy dtype of the values (Int64 -> int64)
  bool masked = false;      // kNumeric
  NumericDtype::Kind numeric_kind = NumericDtype::Kind::kFloat;
  bool pd_na = false;       // kString: "string" holds pd.NA
};
ColumnKind column_kind(const std::string& dtype) {
  using F = ColumnKind::Family;
  ColumnKind out;
  if (dtype == "category") out.family = F::kCategory;
  else if (dtype.starts_with("datetime64[")) {
    out.family = F::kDatetime;
    const auto comma = dtype.find(", ");
    out.aware = comma != std::string::npos;
    out.unit = dtype.substr(11, (out.aware ? comma : dtype.find(']')) - 11);
  } else if (const auto numeric = numeric_dtype(dtype)) {
    out.family = F::kNumeric;
    out.masked = numeric->masked;
    out.numeric_kind = numeric->kind;
    using K = NumericDtype::Kind;
    out.numpy_name = numeric->kind == K::kBool       ? std::string("bool")
                     : numeric->kind == K::kSigned   ? "int" + std::to_string(numeric->bits)
                     : numeric->kind == K::kUnsigned ? "uint" + std::to_string(numeric->bits)
                                                     : "float" + std::to_string(numeric->bits);
  } else if (dtype == "str" || dtype == "string") {
    out.family = F::kString;
    out.pd_na = dtype == "string";
  } else if (dtype == "object") out.family = F::kObject;
  return out;
}
bool integer_kind(const ColumnKind& c) {
  return c.family == ColumnKind::Family::kNumeric &&
         (c.numeric_kind == NumericDtype::Kind::kSigned || c.numeric_kind == NumericDtype::Kind::kUnsigned);
}

bool nat_cell(const Cell& cell) {
  return cell && !cell->is_null && cell->kind == AttributeKind::kDate && cell->value == "NaT";
}
// The object a cell holds in a column of this kind.
Operand element(const Cell& cell, const ColumnKind& column) {
  using F = ColumnKind::Family;
  if (column.family == F::kObject) {
    if (!cell || cell->is_null) return of_kind(Operand::Kind::kNone);
    if (nat_cell(cell)) return of_kind(Operand::Kind::kNaT);
    return of_value(scalar_of(cell));
  }
  if (null_cell(cell)) {
    if (column.family == F::kDatetime) return of_kind(Operand::Kind::kNaT);
    if (column.family == F::kNumeric) return of_kind(column.masked ? Operand::Kind::kNA : Operand::Kind::kNaN, true);
    if (column.family == F::kString && column.pd_na) return of_kind(Operand::Kind::kNA);
    return of_kind(Operand::Kind::kNaN);
  }
  return of_value(scalar_of(cell), column.family == F::kNumeric && column.masked);
}

// The left column plus one Series value (a Series backed by an extension
// array: str, string, category, a masked dtype or an aware datetime).
void column_add(const ColumnKind& left, const std::vector<Operand>& values, const Operand& y) {
  using F = ColumnKind::Family;
  switch (left.family) {
    case F::kCategory:
      if (y.numpy) fail("TypeError", "Object with dtype category cannot perform the numpy op add");
      unsupported("Categorical", type_of(y));
    case F::kDatetime:
      if (y.kind == Operand::Kind::kNaT) return;
      if (is_date(y)) fail("TypeError", "cannot add DatetimeArray and Timestamp");
      if (is_int(y)) fail("TypeError", "Addition/subtraction of integers and integer-arrays with DatetimeArray" + kAddInt);
      if (y.numpy && (y.kind == Operand::Kind::kValue || y.kind == Operand::Kind::kNaN)) {
        const bool boolean = y.kind == Operand::Kind::kValue && y.value.kind == SK::kBool;
        if (!left.aware) {
          if (boolean) return;
          fail("UFuncTypeError", "ufunc 'add' cannot use operands with types dtype('<M8[" + left.unit +
                                     "]') and dtype('float64')");
        }
        if (values.empty()) return;
        unsupported(values.front().kind == Operand::Kind::kNaT ? "NaTType" : "Timestamp", type_of(y));
      }
      unsupported("DatetimeArray", type_of(y));
    case F::kNumeric: {
      if (is_text_operand(y))
        fail("UFuncTypeError", "ufunc 'add' did not contain a loop with signature matching types (dtype('" +
                                   left.numpy_name + "'), dtype('<U" +
                                   std::to_string(std::max<std::size_t>(1, utf8_code_points(y.value.text))) +
                                   "')) -> None");
      if (y.kind == Operand::Kind::kNaT) fail("TypeError", "Cannot add NaT to ndarray with dtype " + left.numpy_name);
      if (is_date(y)) {
        if (integer_kind(left))
          fail("TypeError", "Addition/subtraction of integers and integer-arrays with Timestamp" + kAddInt);
        fail("TypeError", kConcatenation);
      }
      if (y.kind == Operand::Kind::kNone)
        unsupported(left.numeric_kind == NumericDtype::Kind::kFloat ? "float"
                    : left.numeric_kind == NumericDtype::Kind::kBool ? "bool" : "int",
                    "NoneType");
      return;
    }
    case F::kString:
    case F::kObject:
      if (y.kind == Operand::Kind::kNaT) fail("TypeError", "Cannot add NaT to ndarray with dtype object");
      if (is_date(y)) fail("TypeError", kConcatenation);
      if (y.kind == Operand::Kind::kNA) return;
      if (left.family == F::kString && (missing(y) || is_text_operand(y))) return;
      for (const auto& v : values)
        if (!missing(v)) value_add(v, y);
      return;
    case F::kOther: break;
  }
  fail("NotImplementedError", "adding this column to a list case key's run key is outside the captured native domain");
}

// The left column plus a Series backed by a numpy array (numbers, objects or
// naive datetimes), broadcast to a block of the frame's shape: the block
// operation adds every element, without the one-row shortcuts of a Series.
void block_add(const ColumnKind& left, const std::vector<Operand>& values, const ColumnKind& right, const Operand& y) {
  using F = ColumnKind::Family;
  const bool right_dates = right.family == F::kDatetime;
  const bool right_numbers = right.family == F::kNumeric;
  switch (left.family) {
    case F::kCategory:
      if (right_dates) unsupported("Categorical", "DatetimeArray");
      fail("TypeError", "Object with dtype category cannot perform the numpy op add");
    case F::kDatetime:
      if (right_dates) fail("TypeError", "cannot add DatetimeArray and DatetimeArray");
      if (integer_kind(right))
        fail("TypeError", "Addition/subtraction of integers and integer-arrays with DatetimeArray" + kAddInt);
      if (right_numbers && right.numeric_kind == NumericDtype::Kind::kFloat && !left.aware)
        fail("UFuncTypeError", "ufunc 'add' cannot use operands with types dtype('<M8[" + left.unit +
                                   "]') and dtype('" + right.numpy_name + "')");
      if (right_numbers && right.numeric_kind == NumericDtype::Kind::kBool && !left.aware) return;
      if (right.family == F::kObject && y.kind == Operand::Kind::kNaT) return;
      for (const auto& v : values) date_add(v, y);
      return;
    case F::kNumeric:
      if (right_dates) {
        if (left.numeric_kind == NumericDtype::Kind::kFloat)
          fail("UFuncTypeError", "ufunc 'add' cannot use operands with types dtype('" + left.numpy_name +
                                     "') and dtype('<M8[" + right.unit + "]')");
        if (left.numeric_kind == NumericDtype::Kind::kBool) {
          if (left.masked) fail("ValueError", "Could not convert object to NumPy datetime");
          break;
        }
        fail("TypeError", "Addition/subtraction of integers and integer-arrays with DatetimeArray" + kAddInt);
      }
      if (right_numbers || missing(y)) return;
      for (const auto& v : values)
        if (!missing(v)) value_add(v, y);
      return;
    case F::kString:
      if (right_dates) {
        if (y.kind == Operand::Kind::kNaT ||
            std::none_of(values.begin(), values.end(), [](const Operand& v) { return !missing(v); }))
          return;
        unsupported("DatetimeArray", "str");
      }
      if (missing(y) || is_text_operand(y)) return;
      for (const auto& v : values)
        if (!missing(v)) value_add(v, y);
      return;
    case F::kObject:
      if (right_dates) {
        for (const auto& v : values) date_add(y, v);
        return;
      }
      if (missing(y)) return;
      for (const auto& v : values)
        if (!missing(v)) value_add(v, y);
      return;
    case F::kOther: break;
  }
  fail("NotImplementedError", "adding this column to a list case key's run key is outside the captured native domain");
}

// The run key's addition and assignment; returns normally when the run key
// is one column (an empty frame with one listed column, or one row and an
// empty key), whose single run needs no values.
void check(const EventTable& df, const std::vector<std::size_t>& listed, std::size_t act) {
  const auto n = df.size();
  const auto k = listed.size();
  // The row labels: the RangeIndex, or int64 labels.
  std::vector<std::int64_t> labels;
  labels.reserve(n);
  if (!df.index) {
    for (std::size_t i = 0; i < n; ++i) labels.push_back(static_cast<std::int64_t>(i));
  } else {
    if (df.index->names.size() != 1 || df.index->dtype != "int64" || df.index->labels.size() != n)
      fail("NotImplementedError", "a list case key's run key over this index is outside the captured native domain");
    for (const auto& label : df.index->labels) {
      const auto* v = std::get_if<EventAttribute>(&label.value);
      const auto* i = v ? std::get_if<std::int64_t>(&v->typed_value) : nullptr;
      if (!i) fail("NotImplementedError", "a list case key's run key over this index is outside the captured native domain");
      labels.push_back(*i);
    }
  }
  std::vector<std::string> names;
  for (const auto c : listed) names.push_back(df.columns[c]);
  const auto non_decreasing = [&] {
    for (std::size_t i = 1; i < n; ++i)
      if (labels[i] < labels[i - 1]) return false;
    return true;
  }();
  const auto increasing_names = [&] {
    for (std::size_t i = 1; i < k; ++i)
      if (!(names[i - 1] < names[i])) return false;
    return true;
  }();
  // The union: listed names (column positions into `listed`) and row
  // labels (row positions), in pandas' order.
  struct Slot {
    bool column = false;
    std::size_t at = 0;
  };
  std::vector<Slot> slots;
  std::vector<std::size_t> name_order(k);
  std::iota(name_order.begin(), name_order.end(), std::size_t{0});
  const auto sort_names = [&] {
    std::stable_sort(name_order.begin(), name_order.end(),
                     [&](std::size_t a, std::size_t b) { return names[a] < names[b]; });
  };
  std::vector<std::size_t> row_order(n);
  std::iota(row_order.begin(), row_order.end(), std::size_t{0});
  bool columns_first = true;
  if (n == 0) {
    if (!increasing_names) sort_names();
  } else if (k == 0) {
    // numpy's quicksort argsort of the labels (its non-SIMD introsort; the
    // SIMD sort of AVX-512 hosts breaks ties of equal labels otherwise).
    if (!non_decreasing)
      row_order = detail::numpy_object_quick_argsort(
          n, [&](std::size_t a, std::size_t b) { return labels[a] < labels[b] ? -1 : labels[b] < labels[a] ? 1 : 0; });
  } else if (increasing_names && non_decreasing) {
    std::map<std::int64_t, std::size_t> counts;
    for (const auto label : labels) ++counts[label];
    if (counts.size() != n) {
      std::size_t values = k;
      for (const auto& [label, count] : counts) values += count * count;
      fail("ValueError", "Length of values (" + std::to_string(values) + ") does not match length of index (" +
                             std::to_string(k + n) + ")");
    }
  } else {
    sort_names();
    std::stable_sort(row_order.begin(), row_order.end(),
                     [&](std::size_t a, std::size_t b) { return labels[a] < labels[b]; });
    columns_first = false;
  }
  if (columns_first)
    for (const auto c : name_order) slots.push_back({true, c});
  for (const auto r : row_order) slots.push_back({false, r});
  if (!columns_first)
    for (const auto c : name_order) slots.push_back({true, c});

  // The reindexed Series: its dtype, and which object each slot gets.
  const auto act_dtype = column_dtype(df, act);
  const auto act_kind = column_kind(act_dtype);
  const bool identity = k == 0 && non_decreasing;
  std::string right_dtype = act_dtype;
  bool from_object = false, from_integers = false;
  if (!identity) {
    if (integer_kind(act_kind) && !act_kind.masked) {
      if (k) right_dtype = "float64", from_integers = true;
    } else if (act_kind.family == ColumnKind::Family::kNumeric && act_kind.numeric_kind == NumericDtype::Kind::kBool &&
               !act_kind.masked) {
      if (k) right_dtype = "object";
    } else if (act_dtype == "object") {
      // A reindex infers a column of str (or of Timestamps of one zone).
      bool texts = true, dates = true, any = false;
      std::vector<Cell> date_cells;
      for (const auto& row : df.rows) {
        const auto value = element(row[act], act_kind);
        if (missing(value)) continue;
        any = true;
        if (!is_text_operand(value)) texts = false;
        if (row[act] && date_key(*row[act])) date_cells.push_back(row[act]);
        else dates = false;
      }
      if (any && texts) right_dtype = "str", from_object = true;
      else if (any && dates)
        if (auto inferred = datetime_dtype(date_cells)) right_dtype = std::move(*inferred), from_object = true;
    }
  }
  const auto right = column_kind(right_dtype);
  using F = ColumnKind::Family;
  const bool blockwise = right.family == F::kObject || (right.family == F::kNumeric && !right.masked) ||
                         (right.family == F::kDatetime && !right.aware);
  if (right.family == F::kOther)
    fail("NotImplementedError", "a list case key's run key with this activity dtype is outside the captured native domain");
  Operand right_missing;
  if (right.family == F::kDatetime) right_missing = of_kind(Operand::Kind::kNaT);
  else if ((right.family == F::kNumeric && right.masked) || (right.family == F::kString && right.pd_na))
    right_missing = of_kind(Operand::Kind::kNA);
  else right_missing = of_kind(Operand::Kind::kNaN, right.family == F::kNumeric);
  const auto right_value = [&](std::size_t row) -> Operand {
    const auto& cell = df.rows[row][act];
    if (from_object) {
      const auto value = element(cell, act_kind);
      return missing(value) ? right_missing : value;
    }
    if (from_integers) {
      if (null_cell(cell)) return right_missing;
      const auto s = scalar_of(cell);
      return of_value(python::real(std::strtod(python::str(s).c_str(), nullptr)), true);
    }
    if (right.family == F::kObject) return element(cell, act_kind);
    if (null_cell(cell)) return right_missing;
    const bool numpy = right.family == F::kNumeric;
    return of_value(scalar_of(cell), numpy);
  };
  const ColumnKind nan_column = column_kind("float64");
  const std::vector<Operand> nan_values(n, of_kind(Operand::Kind::kNaN, true));
  for (const auto& slot : slots) {
    std::vector<Operand> values;
    ColumnKind left = nan_column;
    Operand y = right_missing;
    if (slot.column) {
      const auto c = listed[slot.at];
      left = column_kind(column_dtype(df, c));
      values.reserve(n);
      for (const auto& row : df.rows) values.push_back(element(row[c], left));
    } else {
      values = nan_values;
      y = right_value(slot.at);
    }
    if (blockwise) block_add(left, values, right, y);
    else column_add(left, values, y);
  }
  if (slots.size() > 1)
    fail("ValueError", "Cannot set a DataFrame with multiple columns to the single column @@extra1");
  if (slots.empty()) fail("ValueError", "Cannot set a DataFrame without columns to the column @@extra1");
}

}  // namespace consecutive_list_key

}  // namespace

EventTable pandas_consecutive_act_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto filter_type_of = [&] { return parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first")); };
  const auto finish = [&](const std::vector<bool>& continues, const Value& filter_type) {
    const bool first = filter_type.kind == Kind::kString && filter_type.text == "first";
    const bool last = filter_type.kind == Kind::kString && filter_type.text == "last";
    if (!first && !last) {
      EventTable out = df;
      reset_index(out);
      return out;
    }
    std::vector<std::vector<std::size_t>> runs;
    for (std::size_t i = 0; i < df.size(); ++i) {
      if (continues[i]) runs.back().push_back(i);
      else runs.push_back({i});
    }
    return merge_groups(df, runs, first, identity_order(df.columns.size()));
  };
  // Round DX (probe dw/s1): a list case key is read as df[key]
  // (pandas_list_key.hpp), then the activity column. The run key then adds
  // that frame and the activity series, aligned on the frame's columns,
  // which a column named twice refuses to reindex; round DZ models the
  // addition and the assignment (consecutive_list_key above). A run key that
  // is one column has at most one row, so one run.
  if (ListCaseKey::applies(case_key(parameters))) {
    const auto listed = case_key(parameters).items.empty()
                            ? std::vector<std::size_t>{}
                            : detail::pandas_getitem_list(df.columns, key_attribute(case_key(parameters)), df.size(),
                                                          attribute_repr, [](const std::string& m) { return py_repr(m); }, fail);
    const auto act = getitem_key(df, activity_key(parameters));
    std::set<std::size_t> seen;
    for (const auto c : listed)
      if (!seen.insert(c).second) fail("ValueError", "cannot reindex on an axis with duplicate labels");
    consecutive_list_key::check(df, listed, act);
    return finish(std::vector<bool>(df.size(), false), filter_type_of());
  }
  // Round CL: df[case] then df[act] (a dict key is refused as an indexer).
  const auto case_column = getitem_key(df, case_key(parameters));
  const auto act = getitem_key(df, activity_key(parameters));
  const auto filter_type = filter_type_of();
  // The run key is case id + activity. Two numeric columns add as numpy or
  // pandas' masked arrays do; otherwise the cells add as Python objects (two
  // str concatenate, two numbers add) over the rows where both are present,
  // and the first pair that does not add raises Python's TypeError.
  const auto dtype = [&](std::size_t c) { return c < df.dtypes.size() ? df.dtypes[c] : std::string("object"); };
  // continues[i]: row i joins row i - 1's run.
  std::vector<bool> continues(df.size(), false);
  // A categorical side refuses the addition whatever the cells (round CA):
  // two array types name each other, any other partner is a NumPy op.
  if (dtype(case_column) == "category" || dtype(act) == "category") {
    const auto array_name = [](const std::string& name) -> std::string {
      if (name == "category") return "Categorical";
      if (name.starts_with("datetime64")) return "DatetimeArray";
      if (name.starts_with("timedelta64")) return "TimedeltaArray";
      if (name.starts_with("period")) return "PeriodArray";
      if (name.starts_with("interval")) return "IntervalArray";
      return {};
    };
    const auto left = array_name(dtype(case_column)), right = array_name(dtype(act));
    if (!left.empty() && !right.empty())
      fail("TypeError", "unsupported operand type(s) for +: '" + left + "' and '" + right + "'");
    fail("TypeError", "Object with dtype category cannot perform the numpy op add");
  }
  const auto case_dtype = numeric_dtype(dtype(case_column));
  const auto act_dtype = numeric_dtype(dtype(act));
  if (const auto runs = datetime_run_key(df, case_column, act, dtype(case_column), dtype(act))) {
    for (std::size_t i = 1; i < df.size(); ++i) continues[i] = *runs;
  } else if (case_dtype && act_dtype) {
    const auto result = promote(*case_dtype, *act_dtype);
    if (result.masked && result.kind == NumericDtype::Kind::kFloat && result.bits == 16)
      fail("TypeError", "FloatingArray does not support np.float16 dtype.");
    std::vector<NumericKey> keys;
    keys.reserve(df.size());
    for (const auto& row : df.rows)
      keys.push_back(numeric_key(row[case_column], *case_dtype, row[act], *act_dtype, result));
    for (std::size_t i = 1; i < df.size(); ++i) continues[i] = same_run(keys[i], keys[i - 1], result);
  } else {
    // A masked column (the other one is not numeric) adds its stored values
    // elementwise, so an NA cell takes part as the value stored under the
    // mask: an int of an IntN/UIntN column, NaN of a FloatN column, a bool of
    // a boolean column. Its key is NA either way.
    // Round CP: a pandas string array (StringDtype, "str" or "string") on
    // the left adds a masked activity column by its mask, so the NA rows
    // stay missing; a masked case column on the left adds its stored values.
    const auto string_array = [](const std::string& name) { return name == "str" || name == "string"; };
    const bool string_partner = string_array(dtype(case_column));
    const auto stored = [&](const Cell& cell, const std::optional<NumericDtype>& dtype) -> Cell {
      if (!dtype || !dtype->masked || !null_cell(cell) || string_partner) return cell;
      const auto& name = cell ? cell->key : std::string();
      if (dtype->kind == NumericDtype::Kind::kFloat) return float_cell(name, std::numeric_limits<double>::quiet_NaN());
      if (dtype->kind != NumericDtype::Kind::kBool) return int_cell(name, 1);
      auto out = make_cell(name, "False", AttributeKind::kBoolean);
      out.typed_value = false;
      return out;
    };
    for (std::size_t i = 0; i < df.size(); ++i) {
      const auto c = stored(df.rows[i][case_column], case_dtype);
      const auto a = stored(df.rows[i][act], act_dtype);
      if (null_cell(c) || null_cell(a)) {
        const auto outcome = missing_add_outcome(c, a);
        if (outcome == 1) break;
        if (outcome == 2) fail("OverflowError", "int too large to convert to float");
        continue;
      }
      // The first present pair that raises raises the same way below.
      try {
        (void)python_add(scalar_of(c), scalar_of(a));
      } catch (...) {
        break;
      }
    }
    std::vector<std::optional<python::Scalar>> run_keys(df.size());
    for (std::size_t i = 0; i < df.size(); ++i) {
      const auto c = stored(df.rows[i][case_column], case_dtype);
      const auto a = stored(df.rows[i][act], act_dtype);
      if (null_cell(c) || null_cell(a)) continue;
      auto key = python_add(scalar_of(c), scalar_of(a));
      if (!null_cell(df.rows[i][case_column]) && !null_cell(df.rows[i][act])) run_keys[i] = std::move(key);
    }
    // A pandas "string" side makes the key a string array holding pd.NA,
    // whose NA comparisons continue the run like a masked key (round CA).
    const bool string_key = dtype(case_column) == "string" || dtype(act) == "string";
    for (std::size_t i = 1; i < df.size(); ++i)
      continues[i] = string_key && (!run_keys[i] || !run_keys[i - 1])
                         ? true
                         : run_keys[i] && run_keys[i - 1] && py_equal(*run_keys[i], *run_keys[i - 1]);
  }
  return finish(continues, filter_type);
}

// The dtype pandas infers for a key column's distinct values: all bool
// gives bool, all int int64 (or uint64 when none is negative), ints with
// floats float64, all str str, datetimes their datetime64; any other mix
// stays object.
std::string inferred_key_dtype(const std::vector<Cell>& unique_cases) {
  bool booleans = false, integers = false, reals = false, strings = false, other = false;
  bool negative = false, above_int64 = false, beyond_uint64 = false;
  std::vector<Cell> dates;
  for (const auto& cell : unique_cases) {
    const auto n = scalar_number(*cell);
    if (n.kind == ScalarNumber::kBool) booleans = true;
    else if (n.kind == ScalarNumber::kInteger) {
      integers = true;
      negative = negative || n.negative;
      above_int64 = above_int64 || n.above_int64;
      beyond_uint64 = beyond_uint64 || n.beyond_uint64;
    } else if (n.kind == ScalarNumber::kReal) reals = true;
    else if (is_text(cell)) strings = true;
    else if (date_key(*cell)) dates.push_back(cell);
    else other = true;
  }
  const bool numbers = integers || reals;
  const bool exact_integers = !beyond_uint64 && !(negative && above_int64);
  std::string dtype = "object";
  if (other) dtype = "object";
  else if (!dates.empty()) {
    if (!(booleans || numbers || strings))
      if (auto inferred = datetime_dtype(dates)) dtype = std::move(*inferred);
  }
  else if (strings) dtype = booleans || numbers ? "object" : "str";
  else if (booleans) dtype = numbers ? "object" : "bool";
  else if (reals) dtype = exact_integers ? "float64" : "object";
  else if (integers && exact_integers) dtype = above_int64 ? "uint64" : "int64";
  return dtype;
}

// Round DX: groupby(key, sort=False).first() (or last()).reset_index() for
// a list `key` of groupers (see pandas_timestamp_case_grouping_apply).
EventTable list_timestamp_grouping(const EventTable& df, const Value& key, bool first, bool last) {
  ListCaseKey list(key);
  list.group(df, df.size());
  if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
  const auto rows = all_rows(df);
  list.factorize(df, rows);
  using GK = detail::PandasListGrouper::Kind;
  std::set<std::size_t> grouping;
  for (const auto& g : list.groupers)
    if (g.kind == GK::kColumn) grouping.insert(g.column);
  std::vector<std::size_t> others;
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (!grouping.count(c)) others.push_back(c);
  for (const auto& g : list.groupers)
    if (g.kind == GK::kColumn) {
      for (const auto c : others) reject_float16_aggregation(df, g.column, first, column_dtype(df, c));
      reject_float16_groupby(df, g.column);
    }
  const auto ids = list.identities(df, rows);
  std::vector<std::vector<std::size_t>> groups;
  std::map<std::string, std::size_t> group_of;
  for (std::size_t i = 0; i < df.size(); ++i) {
    if (!ids[i]) continue;
    const auto [it, added] = group_of.emplace(*ids[i], groups.size());
    if (added) groups.emplace_back();
    groups[it->second].push_back(i);
  }
  auto out = merge_groups(df, groups, first, others);
  const auto labels = labels_of(df);
  // Each level: its values (the first-seen member of each equal-value class
  // over all rows) and the dtype their distinct values infer.
  for (std::size_t l = list.groupers.size(); l-- > 0;) {
    const auto& g = list.groupers[l];
    const auto name = g.kind == GK::kColumn ? df.columns[g.column] : "level_" + std::to_string(l);
    std::map<std::string, Cell> representative;
    std::vector<Cell> unique;
    for (std::size_t i = 0; i < df.size(); ++i) {
      auto cell = list.value(df, g, i, i, labels);
      if (missing_value(cell)) continue;
      cell->key = name;
      if (representative.emplace(value_identity(*cell), cell).second) unique.push_back(cell);
    }
    std::string dtype = g.kind == GK::kColumn ? column_dtype(df, g.column) : std::string("object");
    // A level with no value: an empty array (or one of float NaNs only) is
    // numpy's float64; a mapping or callable maps no label (int64 on an
    // empty frame, else object).
    if (dtype == "object" && unique.empty()) {
      const bool float_array = g.kind == GK::kArray && std::all_of(g.values.begin(), g.values.end(), [](const auto& v) {
                                 return v.container == AttributeContainerKind::kScalar && detail::pandas_list_nan(v);
                               });
      dtype = float_array ? "float64" : df.empty() ? "int64" : "object";
    } else if (dtype == "object") {
      dtype = inferred_key_dtype(unique);
    }
    std::vector<Cell> cells;
    for (const auto& members : groups) {
      auto cell = list.value(df, g, members.front(), members.front(), labels);
      cell = representative.at(value_identity(*cell));
      const auto n = scalar_number(*cell);
      if (dtype == "float64") cell = float_cell(name, n.real);
      else if (dtype == "int64" && n.kind == ScalarNumber::kInteger) cell = int_cell(name, n.integer);
      else if (dtype == "uint64" && n.kind == ScalarNumber::kInteger) {
        auto u = make_cell(name, std::to_string(n.unsigned_integer), AttributeKind::kInt);
        u.typed_value = n.unsigned_integer;
        cell = std::move(u);
      }
      cells.push_back(std::move(cell));
    }
    out.columns.insert(out.columns.begin(), name);
    if (!df.dtypes.empty()) out.dtypes.insert(out.dtypes.begin(), dtype);
    if (!out.categorical_dtypes.empty() || (g.kind == GK::kColumn && g.column < df.categorical_dtypes.size() &&
                                            df.categorical_dtypes[g.column])) {
      if (out.categorical_dtypes.empty()) out.categorical_dtypes.resize(out.columns.size() - 1);
      out.categorical_dtypes.insert(out.categorical_dtypes.begin(),
                                    g.kind == GK::kColumn && g.column < df.categorical_dtypes.size()
                                        ? df.categorical_dtypes[g.column]
                                        : std::nullopt);
    }
    for (std::size_t r = 0; r < out.rows.size(); ++r) out.rows[r].insert(out.rows[r].begin(), cells[r]);
  }
  if (std::none_of(out.categorical_dtypes.begin(), out.categorical_dtypes.end(), [](const auto& d) { return d.has_value(); }))
    out.categorical_dtypes.clear();
  return out;
}

EventTable pandas_timestamp_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto case_value = case_key(parameters);
  const auto timestamp_value = key_parameter(parameters, "TIMESTAMP_KEY", "pm4py:param:timestamp_key", "time:timestamp");
  const auto case_name = key_text(case_value);
  const auto timestamp_name = key_text(timestamp_value);
  const auto filter_type = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  const bool first = filter_type.kind == Kind::kString && filter_type.text == "first";
  const bool last = filter_type.kind == Kind::kString && filter_type.text == "last";
  // Round DX (probe dw/s1): a list key is one item of groupby([case,
  // timestamp]) (pandas_list_key.hpp: an array as long as the frame). The
  // levels come back first (a column's under its name, any other as
  // level_i), then the columns no level names.
  if (case_value.kind == Kind::kList || timestamp_value.kind == Kind::kList)
    return list_timestamp_grouping(df, Value::list({case_value, timestamp_value}), first, last);
  // Round CL (observed black-box): groupby([case, timestamp]) with a dict
  // key maps the index labels, so no group forms; a list key is an array
  // grouper that needs one item per row (and then does not hash). The
  // result is empty: the key columns (a dict's as level_i, object, or int64
  // on an empty frame; a column's with the dtype its distinct values infer)
  // and then the other columns.
  if (dict_key(case_value) || dict_key(timestamp_value) || set_key(case_value) || set_key(timestamp_value)) {
    // Round DA: two keys for a two-row frame, neither a dict nor a list,
    // are first looked up as columns in order (a set does not hash); a key
    // that is not a column makes the key list itself one array grouper,
    // whose set item does not hash when first() or last() aggregates.
    const auto plain = [](const Value& key) { return key.kind != Kind::kDict && key.kind != Kind::kList && key.kind != Kind::kTuple; };
    if (df.size() == 2 && plain(case_value) && plain(timestamp_value))
      for (const auto* key : {&case_value, &timestamp_value}) {
        if (key->kind == Kind::kSet) fail("TypeError", "unhashable type: 'set'");
        if (!find_column(df, key_text(*key))) {
          if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
          fail("TypeError", "unhashable type: 'set'");
        }
      }
    // Round CN: the groupby checks each key when it is built; a list key's
    // items hash only when first() or last() aggregates. Round DA: a set
    // key is mapped over the index labels as a function.
    std::vector<std::optional<std::size_t>> keys;
    std::optional<std::string> unhashable;
    for (const auto* key : {&case_value, &timestamp_value}) {
      if (key->kind == Kind::kDict) {
        keys.emplace_back(std::nullopt);
        continue;
      }
      if (key->kind == Kind::kSet) {
        if (df.size() != 0) fail("TypeError", "'set' object is not callable");
        keys.emplace_back(std::nullopt);
        continue;
      }
      if (dict_key(*key) || set_key(*key)) {
        if (key->items.size() != df.size()) fail("ValueError", "Grouper and axis must be same length");
        if (!unhashable) unhashable = unhashable_key_type(*key);
        keys.emplace_back(std::nullopt);
        continue;
      }
      keys.emplace_back(require_column(df, key_text(*key)));
    }
    if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
    if (unhashable) fail("TypeError", "unhashable type: '" + *unhashable + "'");
    EventTable out = df;
    out.rows.clear();
    out.columns.clear();
    out.dtypes.clear();
    out.categorical_dtypes.clear();
    std::vector<std::optional<EventTableCategoricalDtype>> categorical;
    const auto add = [&](const std::string& name, const std::string& dtype, std::optional<std::size_t> source) {
      out.columns.push_back(name);
      out.dtypes.push_back(dtype);
      categorical.push_back(source && *source < df.categorical_dtypes.size() ? df.categorical_dtypes[*source]
                                                                               : std::nullopt);
    };
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (!keys[i]) {
        add("level_" + std::to_string(i), df.empty() ? "int64" : "object", std::nullopt);
        continue;
      }
      auto dtype = column_dtype(df, *keys[i]);
      if (dtype == "object" && !df.empty()) {
        std::vector<Cell> unique;
        std::set<Key> seen;
        for (const auto& row : df.rows)
          if (!null_cell(row[*keys[i]]) && seen.insert(key_of(*row[*keys[i]])).second) unique.push_back(row[*keys[i]]);
        if (!unique.empty()) dtype = inferred_key_dtype(unique);
      }
      add(df.columns[*keys[i]], dtype, keys[i]);
    }
    for (std::size_t c = 0; c < df.columns.size(); ++c)
      if (keys[0] != c && keys[1] != c) add(df.columns[c], column_dtype(df, c), c);
    if (std::any_of(categorical.begin(), categorical.end(), [](const auto& d) { return d.has_value(); }))
      out.categorical_dtypes = std::move(categorical);
    if (df.dtypes.empty()) out.dtypes.clear();
    reset_index(out);
    return out;
  }
  // pandas groups a two-row table by the key list itself when a key is not a
  // column: row i joins the group named by key i, and the group labels come
  // back as an "index" column.
  if (df.size() == 2 && (!find_column(df, case_name) || !find_column(df, timestamp_name))) {
    if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
    std::vector<std::vector<std::size_t>> groups{{0}};
    std::vector<std::string> labels{case_name};
    if (timestamp_name == case_name) {
      groups[0].push_back(1);
    } else {
      groups.push_back({1});
      labels.push_back(timestamp_name);
    }
    auto out = merge_groups(df, groups, first, identity_order(df.columns.size()));
    out.columns.insert(out.columns.begin(), "index");
    if (!out.dtypes.empty()) out.dtypes.insert(out.dtypes.begin(), "str");
    if (!out.categorical_dtypes.empty()) out.categorical_dtypes.insert(out.categorical_dtypes.begin(), std::nullopt);
    for (std::size_t g = 0; g < out.rows.size(); ++g)
      out.rows[g].insert(out.rows[g].begin(), string_cell("index", labels[g]));
    return out;
  }
  const auto case_column = require_column(df, case_name);
  const auto timestamp = require_column(df, timestamp_name);
  if (!first && !last) fail("AttributeError", "'NoneType' object has no attribute 'reset_index'");
  // The first other column is the first one aggregated.
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (c != case_column && c != timestamp) reject_float16_aggregation(df, case_column, first, column_dtype(df, c));
  reject_float16_groupby(df, case_column);
  std::vector<std::vector<std::size_t>> groups;
  // Datetime cells group by their exact instant, so equal instants spelled
  // with Z or +00:00, or with different fractional digits, are one group.
  const auto timestamp_key = [](const EventAttribute& cell) -> Key {
    const auto* date = std::get_if<NativeDate>(&cell.typed_value);
    if (date || cell.kind == AttributeKind::kDate)
      if (const auto t = parse_timestamp_exact(date ? date->value : cell.value))
        return {3, std::string(t->timezone_aware ? "A" : "N") + std::to_string(t->seconds) + ":" +
                       std::to_string(t->nanoseconds)};
    return key_of(cell);
  };
  std::map<std::pair<Key, Key>, std::size_t> group_of;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& t = df.rows[i][timestamp];
    if (null_cell(c) || null_cell(t)) continue;
    const auto [it, added] = group_of.emplace(std::make_pair(key_of(*c), timestamp_key(*t)), groups.size());
    if (added) groups.emplace_back();
    groups[it->second].push_back(i);
  }
  std::vector<std::size_t> order{case_column, timestamp};
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (c != case_column && c != timestamp) order.push_back(c);
  auto out = merge_groups(df, groups, first, order);
  // Group keys come back as the first-seen member of each equal-key class
  // over the whole column (rows dropped for a null partner key included):
  // ids 1 and True show as whichever came first.
  std::map<Key, Cell> case_rep, time_rep;
  for (const auto& row : df.rows) {
    if (!null_cell(row[case_column])) case_rep.emplace(key_of(*row[case_column]), row[case_column]);
    if (!null_cell(row[timestamp])) time_rep.emplace(timestamp_key(*row[timestamp]), row[timestamp]);
  }
  std::vector<Cell> unique_cases;
  {
    std::set<Key> seen;
    for (const auto& row : df.rows)
      if (!null_cell(row[case_column]) && seen.insert(key_of(*row[case_column])).second)
        unique_cases.push_back(row[case_column]);
  }
  for (std::size_t g = 0; g < groups.size(); ++g) {
    const auto& member = df.rows[groups[g].front()];
    out.rows[g][0] = case_rep.at(key_of(*member[case_column]));
    out.rows[g][1] = time_rep.at(timestamp_key(*member[timestamp]));
  }
  // The key column's dtype is inferred from those distinct values.
  if (!out.dtypes.empty() && out.dtypes[0] == "object" && !unique_cases.empty()) {
    const auto dtype = inferred_key_dtype(unique_cases);
    out.dtypes[0] = dtype;
    const auto& name = out.columns[0];
    for (auto& row : out.rows) {
      const auto n = scalar_number(*row[0]);
      if (dtype == "float64") row[0] = float_cell(name, n.real);
      else if (dtype == "int64" && n.kind == ScalarNumber::kInteger) row[0] = int_cell(name, n.integer);
      else if (dtype == "uint64" && n.kind == ScalarNumber::kInteger) {
        auto cell = make_cell(name, std::to_string(n.unsigned_integer), AttributeKind::kInt);
        cell.typed_value = n.unsigned_integer;
        row[0] = std::move(cell);
      }
    }
  }
  return out;
}

// numpy's str of a float16, float32 or float64 scalar: the shortest digits
// that read back to the same value in that width, positional from 1e-4 up
// to 1e3 (float16), 1e6 (float32) or 1e16 (float64), else scientific with a
// two-digit exponent at least.
std::string numpy_float_str(double value, int bits) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  const double magnitude = std::fabs(value);
  // digits: the significant digits (no trailing zeros); exponent: the power
  // of ten of the first one.
  std::string digits;
  int exponent = 0;
  if (bits == 16) {
    // The shortest digits that read back to the same half; at a power of
    // two the two neighbours are not equally far, so the digits rounded
    // either way are tried and the closest one that reads back is taken.
    const auto half = float_to_half(static_cast<float>(magnitude));
    for (int count = 1; count <= 5 && digits.empty(); ++count) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%.*e", count - 1, magnitude);
      const auto e = std::strchr(buffer, 'e');
      std::string mantissa(buffer, e);
      mantissa.erase(std::remove(mantissa.begin(), mantissa.end(), '.'), mantissa.end());
      const long long rounded = std::stoll(mantissa);
      const int power = std::atoi(e + 1) - (count - 1);
      double best = 0;
      for (long long candidate = rounded - 1; candidate <= rounded + 1; ++candidate) {
        if (candidate <= 0) continue;
        const auto text = std::to_string(candidate) + "e" + std::to_string(power);
        const double read = std::strtod(text.c_str(), nullptr);
        if (float_to_half(static_cast<float>(read)) != half) continue;
        // A tie goes to the even last digit.
        const double distance = std::fabs(read - magnitude);
        if (!digits.empty() && (distance > best || (distance == best && candidate % 2))) continue;
        best = distance;
        digits = std::to_string(candidate);
        exponent = power + static_cast<int>(digits.size()) - 1;
      }
    }
    if (digits.empty()) digits = "0";
  } else {
    char buffer[64];
    const auto written =
        bits == 64 ? std::to_chars(buffer, buffer + sizeof(buffer), magnitude, std::chars_format::scientific)
                   : std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(magnitude), std::chars_format::scientific);
    const std::string_view text(buffer, static_cast<std::size_t>(written.ptr - buffer));
    const auto e = text.find('e');
    for (const char c : text.substr(0, e))
      if (c != '.') digits += c;
    exponent = std::stoi(std::string(text.substr(e + 1)));
  }
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  const double cutoff = bits == 16 ? 1e3 : bits == 32 ? 1e6 : 1e16;
  std::string out = std::signbit(value) ? "-" : "";
  if (magnitude == 0 || (magnitude >= 1e-4 && magnitude < cutoff)) {
    if (exponent < 0) return out + "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
    const auto whole = static_cast<std::size_t>(exponent) + 1;
    if (digits.size() <= whole) return out + digits + std::string(whole - digits.size(), '0') + ".0";
    return out + digits.substr(0, whole) + "." + digits.substr(whole);
  }
  out += digits.substr(0, 1);
  if (digits.size() > 1) out += "." + digits.substr(1);
  const auto power = std::to_string(std::abs(exponent));
  return out + (exponent < 0 ? "e-" : "e+") + (power.size() < 2 ? "0" : "") + power;
}

// The array np.unique sorts for a numeric case column: the column's own
// dtype, except that a masked IntN/UIntN column holding NA reads as float64
// (NA as NaN), a masked FloatN one keeps its width (NA as NaN), and a masked
// boolean one holding NA is an object array (nullopt here).
struct NumpyCase {
  bool nan = false;
  std::uint64_t bits = 0;
  double real = 0.0;
};
std::optional<NumericDtype> numpy_case_view(const EventTable& df, std::size_t column) {
  using K = NumericDtype::Kind;
  auto dtype = numeric_dtype(column_dtype(df, column));
  if (!dtype) return std::nullopt;
  const bool na = dtype->masked && std::any_of(df.rows.begin(), df.rows.end(),
                                                [&](const auto& row) { return null_cell(row[column]); });
  if (na && dtype->kind == K::kBool) return std::nullopt;
  if (na && dtype->kind != K::kFloat) return NumericDtype{K::kFloat, 64, false};
  dtype->masked = false;
  return dtype;
}
NumpyCase numpy_case(const Cell& cell) {
  NumpyCase out;
  if (null_cell(cell)) {
    out.nan = true;
    out.real = std::numeric_limits<double>::quiet_NaN();
    return out;
  }
  const auto s = scalar_of(cell);
  if (s.kind == python::Scalar::Kind::kBool) out.bits = s.boolean;
  else if (s.kind == python::Scalar::Kind::kFloat) {
    out.real = s.real;
    out.nan = std::isnan(s.real);
    return out;
  } else {
    const std::uint64_t magnitude = std::strtoull(s.digits.c_str(), nullptr, 10);
    out.bits = s.negative ? std::uint64_t(0) - magnitude : magnitude;
    out.real = s.negative ? -static_cast<double>(magnitude) : static_cast<double>(magnitude);
    return out;
  }
  out.real = static_cast<double>(out.bits);
  return out;
}

// numpy_value == item for a numpy scalar and a Python item: a number is
// cast to a float scalar's width, or compared exactly with an integer
// (an integer against a float as float64); NaN equals nothing.
bool numpy_equals(const NumpyCase& value, const NumericDtype& view, const python::Scalar& item) {
  using K = python::Scalar::Kind;
  if (!python::is_number(item) || value.nan) return false;
  const bool integral = item.kind != K::kFloat;
  if (view.kind == NumericDtype::Kind::kFloat || !integral) {
    const double number = item.kind == K::kFloat  ? item.real
                          : item.kind == K::kBool ? (item.boolean ? 1.0 : 0.0)
                                                  : std::strtod(python::str(item).c_str(), nullptr);
    if (view.kind == NumericDtype::Kind::kFloat) return narrow_float(number, view.bits) == value.real;
    const double own = view.kind == NumericDtype::Kind::kSigned ? static_cast<double>(static_cast<std::int64_t>(value.bits))
                                                                : static_cast<double>(value.bits);
    return own == number;
  }
  // Both integers as a sign and a magnitude, which fit in uint64.
  std::uint64_t magnitude = item.kind == K::kBool && item.boolean ? 1 : 0;
  if (item.kind == K::kInt) {
    if (item.digits.size() > 20) return false;
    if (item.digits.size() == 20 && item.digits > "18446744073709551615") return false;
    magnitude = std::strtoull(item.digits.c_str(), nullptr, 10);
  }
  const bool negative = item.kind == K::kInt && item.negative && magnitude != 0;
  const bool own_negative =
      view.kind == NumericDtype::Kind::kSigned && static_cast<std::int64_t>(value.bits) < 0;
  const std::uint64_t own_magnitude = own_negative ? std::uint64_t(0) - value.bits : value.bits;
  return own_negative == negative && own_magnitude == magnitude;
}

namespace {
EventTable activity_split(const EventTable& df, std::size_t case_column, std::size_t act,
                          const PandasFilterValue& activity, const PandasFilterParameters& parameters,
                          std::optional<std::size_t> bound);
}  // namespace

EventTable pandas_activity_split_apply(const EventTable& df, const PandasFilterValue& activity,
                                       const PandasFilterParameters& parameters) {
  const auto case_value = case_key(parameters);
  // Round DX (probe dw/s1): a list case key is read as df[key]
  // (pandas_list_key.hpp), its cases as that frame's array flattened row by
  // row (N rows times k columns). The walk over them is the one-column walk
  // and reads past the frame's rows when k > 1 ("index R is out of
  // bounds"); writing the labels back to the k columns fails unless the
  // frame holds one row and the key one column, which is the one-column
  // split.
  if (ListCaseKey::applies(case_value)) {
    const auto listed = case_value.items.empty()
                            ? std::vector<std::size_t>{}
                            : detail::pandas_getitem_list(df.columns, key_attribute(case_value), df.size(), attribute_repr,
                                                          [](const std::string& m) { return py_repr(m); }, fail);
    const auto act = getitem_key(df, activity_key(parameters));
    // An empty key selects no column: nothing is walked or written.
    if (listed.empty()) return df;
    if (listed.size() == 1 && df.size() == 1) return activity_split(df, listed.front(), act, activity, parameters, std::nullopt);
    std::vector<std::size_t> columns(listed.begin(), listed.end());
    columns.push_back(act);
    const auto view = detail::numpy_view_of(df, columns);
    auto flat = detail::pandas_list_flat_cases(view, listed);
    flat.columns.push_back(df.columns[act]);
    if (!flat.dtypes.empty()) flat.dtypes.push_back(view.dtypes[act]);
    for (std::size_t r = 0; r < flat.rows.size(); ++r) flat.rows[r].push_back(r < df.size() ? view.rows[r][act] : Cell{});
    (void)activity_split(flat, 0, 1, activity, parameters, df.size());
    fail("ValueError", "Columns must be same length as key");
  }
  // Round CL: df[case] then df[act] (a dict key is refused as an indexer).
  const auto case_column = getitem_key(df, case_value);
  const auto act = getitem_key(df, activity_key(parameters));
  return activity_split(df, case_column, act, activity, parameters, std::nullopt);
}

namespace {
EventTable activity_split(const EventTable& df, std::size_t case_column, std::size_t act,
                          const PandasFilterValue& activity, const PandasFilterParameters& parameters,
                          std::optional<std::size_t> bound) {
  const auto mode = parameter_or(parameters, "CUT_MODE", "cut_mode", Value::of("this"));
  const auto separator_value = parameter_or(parameters, "SUBCASE_CONCAT_STR", "subcase_concat_str", Value::of("##@@"));
  const auto separator = separator_value.text;
  const bool this_row = mode.kind == Kind::kString && mode.text == "this";
  EventTable out = df;
  if (!out.dtypes.empty()) out.dtypes[case_column] = df.empty() ? "float64" : "str";
  if (case_column < out.categorical_dtypes.size()) {
    out.categorical_dtypes[case_column] = std::nullopt;
    if (std::none_of(out.categorical_dtypes.begin(), out.categorical_dtypes.end(),
                     [](const auto& descriptor) { return descriptor.has_value(); }))
      out.categorical_dtypes.clear();
  }
  // The case and activity columns are read as NumPy holds them (round CA): a
  // categorical column as its categories' array, a gap as NaN.
  const auto nv = detail::numpy_view(df, {case_column, act});
  // The reference takes np.unique(cases, return_index, return_counts): a
  // stable sort of the case column (which raises on unorderable ids), runs
  // of equal neighbours (a NaN equals nothing), each run's first position
  // and size. For every run in sorted order it then walks `size` rows from
  // that first position, as if each case's rows were contiguous, counts the
  // walked activities that are `in` the argument, and writes the labels in
  // that order over the case column. Rows of interleaved cases are thereby
  // labelled from other rows, exactly as the reference does.
  // runs: (first position, size, label) of each distinct case in sorted order.
  struct Run {
    std::size_t first = 0, size = 0;
    std::string label;
  };
  std::vector<Run> runs;
  const auto case_dtype = column_dtype(df, case_column);
  if (case_dtype.starts_with("datetime64[")) {
    // A datetime64 case column (round CB): a naive one is numpy's datetime64
    // array, sorted by value with NaT last and every NaT one run, labelled
    // as numpy prints a value of its unit ("2021-01-01T00:00:00.000000"); an
    // aware one is an object array of Timestamps, whose NaT orders against
    // nothing and equals nothing, labelled by str().
    const bool aware = case_dtype.find(", ") != std::string::npos;
    const auto unit = case_dtype.substr(11, case_dtype.find_first_of(",]", 11) - 11);
    struct Stamp {
      bool nat = true;
      std::int64_t seconds = 0;
      std::uint32_t nanos = 0;
      python::Scalar scalar;
    };
    std::vector<Stamp> stamps(df.size());
    for (std::size_t i = 0; i < df.size(); ++i) {
      const auto& cell = df.rows[i][case_column];
      if (null_cell(cell)) continue;
      auto scalar = scalar_of(cell);
      if (scalar.kind != python::Scalar::Kind::kDate) continue;
      stamps[i] = {false, scalar.date.seconds, scalar.date.nanoseconds, std::move(scalar)};
    }
    const auto lt = [&](std::size_t a, std::size_t b) {
      const auto &x = stamps[a], &y = stamps[b];
      if (x.nat || y.nat) return !aware && !x.nat;
      return std::pair(x.seconds, x.nanos) < std::pair(y.seconds, y.nanos);
    };
    const auto same = [&](std::size_t a, std::size_t b) {
      const auto &x = stamps[a], &y = stamps[b];
      if (x.nat || y.nat) return !aware && x.nat && y.nat;
      return x.seconds == y.seconds && x.nanos == y.nanos;
    };
    std::vector<std::size_t> order;
    if (aware) {
      order = detail::numpy_object_stable_argsort(df.size(), [&](std::size_t a, std::size_t b) {
        return lt(a, b) ? -1 : lt(b, a) ? 1 : 0;
      });
    } else {
      order.resize(df.size());
      std::iota(order.begin(), order.end(), std::size_t{0});
      std::stable_sort(order.begin(), order.end(), lt);
    }
    const auto label = [&](const Stamp& t) -> std::string {
      if (t.nat) return "NaT";
      if (aware) return python::str(t.scalar);
      const auto wall = PandasFilterValue::datetime(t.seconds * 1'000'000'000 + t.nanos);
      auto text = pandas_filter_value_str(wall).substr(0, 19);
      text[10] = 'T';
      std::string digits = std::to_string(t.nanos);
      digits.insert(digits.begin(), 9 - digits.size(), '0');
      if (unit == "ms") text += "." + digits.substr(0, 3);
      else if (unit == "us") text += "." + digits.substr(0, 6);
      else if (unit == "ns") text += "." + digits;
      return text;
    };
    for (std::size_t start = 0; start < order.size();) {
      std::size_t end = start + 1;
      while (end < order.size() && same(order[end - 1], order[end])) ++end;
      runs.push_back({order[start], end - start, label(stamps[order[start]])});
      start = end;
    }
  } else if (const auto view = numpy_case_view(nv, case_column)) {
    // A numeric array sorts by value with NaN last; equal neighbours form a
    // run and every NaN joins one run (np.unique's equal_nan).
    using K = NumericDtype::Kind;
    std::vector<NumpyCase> cases;
    cases.reserve(df.size());
    for (const auto& row : nv.rows) cases.push_back(numpy_case(row[case_column]));
    const auto less = [&](const NumpyCase& a, const NumpyCase& b) {
      if (view->kind == K::kFloat) return !a.nan && (b.nan || a.real < b.real);
      if (view->kind == K::kSigned) return static_cast<std::int64_t>(a.bits) < static_cast<std::int64_t>(b.bits);
      return a.bits < b.bits;
    };
    std::vector<std::size_t> order(df.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return less(cases[a], cases[b]); });
    for (std::size_t start = 0; start < order.size();) {
      std::size_t end = start + 1;
      while (end < order.size() && !less(cases[order[end - 1]], cases[order[end]]) &&
             !(cases[order[end]].nan != cases[order[end - 1]].nan))
        ++end;
      const auto& value = cases[order[start]];
      std::string label;
      if (view->kind == K::kFloat) label = numpy_float_str(value.real, view->bits);
      else if (view->kind == K::kBool) label = value.bits ? "True" : "False";
      else if (view->kind == K::kSigned) label = std::to_string(static_cast<std::int64_t>(value.bits));
      else label = std::to_string(value.bits);
      runs.push_back({order[start], end - start, std::move(label)});
      start = end;
    }
  } else {
    std::vector<python::Scalar> cases;
    cases.reserve(df.size());
    for (const auto& row : nv.rows) cases.push_back(scalar_of(row[case_column]));
    // Round DX: a flattened key frame's object array holds NaT for a
    // datetime column's gap, which orders against nothing and equals
    // nothing.
    std::vector<bool> nat(df.size(), false);
    if (bound)
      for (std::size_t i = 0; i < df.size(); ++i) {
        const auto& cell = nv.rows[i][case_column];
        nat[i] = cell && cell->is_null && cell->kind == AttributeKind::kDate;
      }
    std::vector<std::size_t> order;
    try {
      order = detail::numpy_object_stable_argsort(cases.size(), [&](std::size_t a, std::size_t b) {
        return nat[a] || nat[b] ? 0 : python::compare(cases[a], cases[b]);
      });
    } catch (const python::TypeError& error) {
      fail("TypeError", error.what());
    }
    for (std::size_t start = 0; start < order.size();) {
      std::size_t end = start + 1;
      while (end < order.size() && !(is_nan(cases[order[end]]) || is_nan(cases[order[end - 1]])) &&
             !nat[order[end]] && !nat[order[end - 1]] && py_equal(cases[order[end]], cases[order[end - 1]]))
        ++end;
      runs.push_back({order[start], end - start, nat[order[start]] ? std::string("NaT") : python::str(cases[order[start]])});
      start = end;
    }
  }
  // A str argument is one activity; a list, tuple or dict holds them; any
  // other value is not a container.
  std::vector<python::Scalar> cuts;
  bool container = true;
  if (activity.kind == Kind::kString) cuts.push_back(python::text(activity.text));
  else if (activity.kind == Kind::kList || activity.kind == Kind::kTuple)
    for (const auto& item : activity.items) {
      if (const auto s = scalar_of(item)) cuts.push_back(*s);
    }
  else if (activity.kind == Kind::kDict)
    for (const auto& [key, value] : activity.entries) {
      if (const auto s = scalar_of(key)) cuts.push_back(*s);
    }
  // Round DA: `in` a set hashes as `in` a dict's keys does.
  else if (activity.kind == Kind::kSet)
    for (const auto& item : activity.items) {
      if (const auto s = scalar_of(item)) cuts.push_back(*s);
    }
  else container = false;
  // A numeric activity column is read as numpy reads the case column.
  const auto activity_view = numpy_case_view(nv, act);
  const auto activity_dtype = column_dtype(df, act);
  const bool activity_datetime = activity_dtype.starts_with("datetime64[");
  // One cut against a cell, as the column's values compare.
  const auto matches = [&](const Cell& cell, const python::Scalar& cut) {
    // A datetime64 activity (numpy's datetime64 or a Timestamp, NaT
    // included) equals no str, int or None argument (round CB). A datetime
    // argument equals a naive s, ms or us activity of its wall clock (numpy
    // datetime64), an aware activity (a Timestamp) of its instant when it is
    // aware too and the activity's wall clock does not repeat in its zone; a
    // naive ns activity (numpy compares its integer) and a date argument
    // equal nothing (round CC).
    if (activity_datetime) {
      if (null_cell(cell) || activity_dtype == "datetime64[ns]") return false;
      const auto value = scalar_of(cell);
      if (ambiguous_wall(value, activity_dtype)) return false;
      return cut.kind == python::Scalar::Kind::kDate && py_equal(value, cut);
    }
    if (activity_view) return numpy_equals(numpy_case(cell), *activity_view, cut);
    const auto value = scalar_of(cell);
    // `in` matches an element that is the same object or ==: None is
    // None, a NaN cell is never the argument's NaN.
    if (value.kind == python::Scalar::Kind::kNone && cut.kind == python::Scalar::Kind::kNone) return true;
    // Round CP: `in` a dict hashes first, so pd.NA meets no key and is never
    // compared.
    if ((activity.kind == Kind::kDict || activity.kind == Kind::kSet) && value.kind == python::Scalar::Kind::kNA) return false;
    return py_equal(value, cut);
  };
  // Round CN: a numpy scalar activity (a numeric or naive datetime64 column,
  // NaT included) compares a list or tuple item element-wise, and `in` takes
  // the truth of that array: an empty or longer one fails, a one-item one
  // compares its item (probe as1). Items are tried in order until one
  // matches; a dict item equals nothing.
  const bool numpy_scalar = activity_view.has_value() || (activity_datetime && activity_dtype.find(", ") == std::string::npos);
  const bool sequence_items = (activity.kind == Kind::kList || activity.kind == Kind::kTuple) &&
                              std::any_of(activity.items.begin(), activity.items.end(), [](const PandasFilterValue& item) {
                                return item.kind == Kind::kList || item.kind == Kind::kTuple;
                              });
  const auto is_cut = [&](const Cell& cell) {
    if (!container)
      fail("TypeError", "argument of type '" + type_name(activity) + "' is not a container or iterable");
    if (numpy_scalar && sequence_items) {
      for (const auto& item : activity.items) {
        const PandasFilterValue* x = &item;
        while (x->kind == Kind::kList || x->kind == Kind::kTuple) {
          if (x->items.empty())
            fail("ValueError", "The truth value of an empty array is ambiguous. Use `array.size > 0` to check that an array is not empty.");
          if (x->items.size() > 1)
            fail("ValueError", "The truth value of an array with more than one element is ambiguous. Use a.any() or a.all()");
          x = &x->items.front();
        }
        if (const auto cut = scalar_of(*x); cut && matches(cell, *cut)) return true;
      }
      return false;
    }
    return std::any_of(cuts.begin(), cuts.end(), [&](const python::Scalar& cut) { return matches(cell, cut); });
  };
  const auto& name = df.columns[case_column];
  std::size_t written = 0;
  for (const auto& run : runs) {
    // Round CL: the label is the case's str plus the separator, built before
    // the run's rows are read.
    if (separator_value.kind != Kind::kString)
      fail("TypeError", "can only concatenate str (not \"" + type_name(separator_value) + "\") to str");
    const auto label = run.label + separator;
    std::int64_t count = 0;
    for (std::size_t j = run.first; j < run.first + run.size; ++j) {
      const auto before = count;
      if (bound && j >= *bound)
        fail("IndexError", "index " + std::to_string(j) + " is out of bounds for axis 0 with size " + std::to_string(*bound));
      if (is_cut(nv.rows[j][act])) ++count;
      out.rows[written++][case_column] = string_cell(name, label + std::to_string(this_row ? count : before));
    }
  }
  return out;
}
}  // namespace

}  // namespace minera
