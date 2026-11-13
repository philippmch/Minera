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
#include "pandas_numpy_view.hpp"
#include "pandas_range_index.hpp"

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
PandasFilterValue PandasFilterValue::dict(std::vector<std::pair<PandasFilterValue, PandasFilterValue>> values) {
  PandasFilterValue out;
  out.kind = Kind::kDict;
  out.entries = std::move(values);
  return out;
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
    case Kind::kTuple: return !v.items.empty();
    case Kind::kDict: return !v.entries.empty();
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
std::string case_key(const PandasFilterParameters& p) {
  return text_parameter(p, "CASE_ID_KEY", kPandasFilterCaseIdKeyParameter, std::string(kPandasFilterCaseConceptName));
}
std::string activity_key(const PandasFilterParameters& p) {
  return text_parameter(p, "ACTIVITY_KEY", kPandasFilterActivityKeyParameter, std::string(kPandasFilterDefaultNameKey));
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
// microsecond unless one needs nanoseconds, in their one offset; nullopt
// (Timestamp objects) when awareness or offsets differ.
std::optional<std::string> datetime_dtype(const std::vector<Cell>& dates) {
  bool naive = false, aware = false, nanoseconds = false, mixed_offsets = false;
  std::optional<std::int32_t> offset;
  for (const auto& cell : dates) {
    const auto* date = std::get_if<NativeDate>(&cell->typed_value);
    const auto exact = *parse_timestamp_exact(date ? date->value : cell->value);
    (exact.timezone_aware ? aware : naive) = true;
    nanoseconds = nanoseconds || exact.nanoseconds % 1000 != 0;
    if (exact.timezone_aware) {
      if (offset && *offset != exact.offset_minutes) mixed_offsets = true;
      offset = exact.offset_minutes;
    }
  }
  if (naive == aware || mixed_offsets) return std::nullopt;
  std::string dtype = std::string("datetime64[") + (nanoseconds ? "ns" : "us");
  if (aware) {
    const auto minutes = *offset;
    if (minutes == 0) dtype += ", UTC";
    else {
      char zone[16];
      std::snprintf(zone, sizeof(zone), ", UTC%c%02d:%02d", minutes < 0 ? '-' : '+', std::abs(minutes) / 60, std::abs(minutes) % 60);
      dtype += zone;
    }
  }
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
    if (masked_dtype(dtype)) return false;
    if (float_bits(dtype)) return v.kind == Kind::kFloat && std::isnan(v.real);
    return v.kind == Kind::kNone;
  }
  const auto scalar = scalar_of(v);
  return scalar && !value_is_null(v) && py_equal(*a, *scalar);
}

EventTable boundary_filter(const EventTable& df, const Value& values, const std::string& case_glue,
                           const std::string& act_key, const Value& positive, bool end) {
  const auto case_column = require_column(df, case_glue);
  const auto act = require_column(df, act_key, true);
  reject_float16_aggregation(df, case_column, !end, inferred_dtype(df, act));
  const auto cases = cases_of(df, case_column);
  const auto boundary = boundary_activities(df, cases, act, end);
  std::set<Key> kept;
  for (const auto& [key, a] : boundary)
    for (const auto& v : values.items)
      if (matches_value(a, v, column_dtype(df, act))) {
        kept.insert(key);
        break;
      }
  require_case_index(df, case_column);
  return keep_cases(df, case_column, kept, truthy(positive));
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
};
NoccOutcome nocc_filter(const EventTable& df, const Value& nocc, const std::optional<Value>& count0,
                        const std::string& case_glue, const std::string& act_key, bool end,
                        const std::optional<Value>& most_common_variant = std::nullopt) {
  const auto case_column = require_column(df, case_glue);
  const auto act = require_column(df, act_key, true);
  // The end activities of an empty frame are counted without a groupby.
  if (!end || !df.empty()) reject_float16_aggregation(df, case_column, !end, column_dtype(df, act));
  const auto cases = cases_of(df, case_column);
  const auto boundary = boundary_activities(df, cases, act, end);
  const auto dtype = column_dtype(df, act);
  const Counts counts =
      count0 ? counts_argument(*count0) : count_boundaries(cases.order, boundary, !masked_dtype(dtype) && !float_bits(dtype));
  const auto threshold = number_of(nocc);
  if (!threshold) fail("TypeError", "'>=' not supported between instances of 'int' and '" + type_name(nocc) + "'");
  Counts kept;
  for (const auto& entry : counts) {
    const auto count = number_of(entry.second);
    const bool ok = count && (count->integral ? int_at_least(count->integer, *threshold)
                                              : (threshold->integral ? count->real >= static_cast<double>(threshold->integer)
                                                                     : count->real >= threshold->real));
    // The last activity of the most common variant is always kept; a pd.NA
    // boundary (masked or "string" column) fails that comparison (round CA).
    if (!ok && !count0 && !entry.first && most_common_variant && !most_common_variant->items.empty() &&
        (masked_dtype(dtype) || dtype == "string"))
      fail("TypeError", "boolean value of NA is ambiguous");
    const bool protected_end = most_common_variant && !most_common_variant->items.empty() && entry.first &&
                               scalar_of(most_common_variant->items.back()) &&
                               same_activity(entry.first, scalar_of(most_common_variant->items.back()));
    if (ok || protected_end) kept.push_back(entry);
  }
  if (kept.size() == counts.size()) return {df, kept};
  std::set<Key> keep;
  for (const auto& [key, a] : boundary)
    for (const auto& entry : kept)
      if (same_activity(entry.first, a)) {
        keep.insert(key);
        break;
      }
  require_case_index(df, case_column);
  return {keep_cases(df, case_column, keep, true), kept};
}

// A missing boundary is NaN, or None in an object column (round CA: groupby
// last() of an object column without a present value is None).
Value counts_value(const Counts& counts, const std::string& dtype) {
  std::vector<std::pair<Value, Value>> entries;
  for (const auto& [k, v] : counts)
    entries.emplace_back(k ? value_of(*k) : dtype == "object" ? Value::none() : Value::of(std::nan("")), v);
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
  for (std::size_t i = 0; i < item.items.size(); ++i) {
    const auto& part = item.items[i];
    if (part.kind != Kind::kString && (mode != VariantMode::kTrace || part.kind != Kind::kNone))
      fail("TypeError", mode == VariantMode::kTrace
                            ? "decoding to str: need a bytes-like object, " + type_name(part) + " found"
                            : "sequence item " + std::to_string(i) + ": expected str instance, " + type_name(part) + " found");
    if (i) out += ",";
    out += part.kind == Kind::kNone ? "None" : part.text;
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
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters), true);
  reject_float16_groupby(df, case_column);
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  // The case variants are joined before the admitted patterns, which are
  // not read at all without a case.
  const auto case_variants = variants(df, case_column, act);
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
  const auto case_name = case_key(parameters);
  const auto act_name = activity_key(parameters);
  const auto first_or_last = parameter_or(parameters, "FIRST_OR_LAST", "first_or_last", Value::of("first"));
  const bool strict = truthy(parameter_or(parameters, "STRICT", "strict", Value::of(true)));
  const auto case_column = require_column(df, case_name);
  const auto act = require_column(df, act_name);
  require_sorted_groupby(df, case_column);
  const auto index = positions(df, case_column);
  const bool first = first_or_last.kind == Kind::kString && first_or_last.text == "first";
  const bool last = first_or_last.kind == Kind::kString && first_or_last.text == "last";
  if (!first && !last) fail("AttributeError", "'DataFrameGroupBy' object has no attribute 'reset_index'");
  std::map<Key, std::int64_t> occurrence;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& c = df.rows[i][case_column];
    const auto& a = df.rows[i][act];
    if (!index[i] || !column_equals(a, activity, column_dtype(df, act))) continue;
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

EventTable pandas_filter_df_on_start_activities(const EventTable& df, const PandasFilterValue& values,
                                                const std::string& case_id_glue, const std::string& activity_key,
                                                const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, false);
}

EventTable pandas_filter_df_on_end_activities(const EventTable& df, const PandasFilterValue& values,
                                              const std::string& case_id_glue, const std::string& activity_key,
                                              const PandasFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, positive, true);
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
                                                     const std::string& case_id_glue,
                                                     const std::string& activity_key) {
  return nocc_filter(df, nocc, sa_count0, case_id_glue, activity_key, false).table;
}

PandasEndActivitiesNocc pandas_filter_df_on_end_activities_nocc(const EventTable& df, const PandasFilterValue& nocc,
                                                                const std::optional<PandasFilterValue>& ea_count0,
                                                                const std::string& case_id_glue,
                                                                const std::string& activity_key, bool return_dict,
                                                                const std::optional<PandasFilterValue>& most_common_variant) {
  auto outcome = nocc_filter(df, nocc, ea_count0, case_id_glue, activity_key, true, most_common_variant);
  PandasEndActivitiesNocc out{std::move(outcome.table), std::nullopt};
  if (return_dict && !df.empty()) out.counts = counts_value(outcome.kept, column_dtype(df, *find_column(df, activity_key)));
  return out;
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
  const auto case_name = case_key(parameters);
  const auto act_name = activity_key(parameters);
  const auto minimum = parameter_or(parameters, "MIN_OCCURRENCES", "min_occurrences", Value::of(std::int64_t{2}));
  const bool positive = truthy(parameter_or(parameters, "POSITIVE", "positive", Value::of(true)));
  require_columns(df, {act_name, case_name});
  const auto bound = number_of(minimum);
  if (!bound) fail("TypeError", "unsupported operand type(s) for -: '" + type_name(minimum) + "' and 'int'");
  const Number threshold = bound->integral ? Number{true, bound->integer - 1, 0.0} : Number{false, 0, bound->real - 1};
  const auto case_column = *find_column(df, case_name);
  const auto act = *find_column(df, act_name);
  // The occurrences are counted by a sorted groupby over the matching rows.
  std::vector<bool> matching(df.size());
  reject_float16_groupby(df, act);
  for (std::size_t i = 0; i < df.size(); ++i) matching[i] = column_equals(df.rows[i][act], activity, column_dtype(df, act));
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

EventTable pandas_consecutive_act_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters));
  const auto filter_type = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
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
  if (case_dtype && act_dtype) {
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
    const auto stored = [&](const Cell& cell, const std::optional<NumericDtype>& dtype) -> Cell {
      if (!dtype || !dtype->masked || !null_cell(cell)) return cell;
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
}

EventTable pandas_timestamp_case_grouping_apply(const EventTable& df, const PandasFilterParameters& parameters) {
  const auto case_name = case_key(parameters);
  const auto timestamp_name =
      text_parameter(parameters, "TIMESTAMP_KEY", "pm4py:param:timestamp_key", "time:timestamp");
  const auto filter_type = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  const bool first = filter_type.kind == Kind::kString && filter_type.text == "first";
  const bool last = filter_type.kind == Kind::kString && filter_type.text == "last";
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
  // The key column's dtype is inferred from those distinct values: all bool
  // gives bool, all int int64 (or uint64 when none is negative), ints with
  // floats float64, all str str; any other mix stays object.
  if (!out.dtypes.empty() && out.dtypes[0] == "object" && !unique_cases.empty()) {
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

EventTable pandas_activity_split_apply(const EventTable& df, const PandasFilterValue& activity,
                                       const PandasFilterParameters& parameters) {
  const auto case_column = require_column(df, case_key(parameters));
  const auto act = require_column(df, activity_key(parameters));
  const auto mode = parameter_or(parameters, "CUT_MODE", "cut_mode", Value::of("this"));
  const auto separator = text_parameter(parameters, "SUBCASE_CONCAT_STR", "subcase_concat_str", "##@@");
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
  if (const auto view = numpy_case_view(nv, case_column)) {
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
    std::vector<std::size_t> order;
    try {
      order = detail::numpy_object_stable_argsort(cases.size(), [&](std::size_t a, std::size_t b) {
        return python::compare(cases[a], cases[b]);
      });
    } catch (const python::TypeError& error) {
      fail("TypeError", error.what());
    }
    for (std::size_t start = 0; start < order.size();) {
      std::size_t end = start + 1;
      while (end < order.size() && !(is_nan(cases[order[end]]) || is_nan(cases[order[end - 1]])) &&
             py_equal(cases[order[end]], cases[order[end - 1]]))
        ++end;
      runs.push_back({order[start], end - start, python::str(cases[order[start]])});
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
  else container = false;
  // A numeric activity column is read as numpy reads the case column.
  const auto activity_view = numpy_case_view(nv, act);
  const auto is_cut = [&](const Cell& cell) {
    if (!container)
      fail("TypeError", "argument of type '" + type_name(activity) + "' is not a container or iterable");
    if (activity_view) {
      const auto value = numpy_case(cell);
      return std::any_of(cuts.begin(), cuts.end(),
                         [&](const python::Scalar& cut) { return numpy_equals(value, *activity_view, cut); });
    }
    const auto value = scalar_of(cell);
    for (const auto& cut : cuts) {
      // `in` matches an element that is the same object or ==: None is
      // None, a NaN cell is never the argument's NaN.
      if (value.kind == python::Scalar::Kind::kNone && cut.kind == python::Scalar::Kind::kNone) return true;
      if (py_equal(value, cut)) return true;
    }
    return false;
  };
  const auto& name = df.columns[case_column];
  std::size_t written = 0;
  for (const auto& run : runs) {
    const auto label = run.label + separator;
    std::int64_t count = 0;
    for (std::size_t j = run.first; j < run.first + run.size; ++j) {
      const auto before = count;
      if (is_cut(nv.rows[j][act])) ++count;
      out.rows[written++][case_column] = string_cell(name, label + std::to_string(this_row ? count : before));
    }
  }
  return out;
}

}  // namespace minera
