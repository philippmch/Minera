#include "minera/polars_sequence_filters.hpp"
#include "polars_column_hint.hpp"
#include "polars_sort.hpp"
#include "python_scalar.hpp"
#include "zone_database.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <variant>

namespace minera {

namespace {

using Value = PolarsFilterValue;
using Kind = PolarsFilterValue::Kind;
using Cell = std::optional<EventAttribute>;

[[noreturn]] void fail(const std::string& type, const std::string& message) {
  throw PolarsSequenceFilterError(type, message);
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
bool at_least(const Number& a, const Number& b) {
  if (a.integral && b.integral) return a.integer >= b.integer;
  const double x = a.integral ? static_cast<double>(a.integer) : a.real;
  const double y = b.integral ? static_cast<double>(b.integer) : b.real;
  return x >= y;
}

const Value* parameter(const PolarsFilterParameters& parameters, std::string_view member, std::string_view literal) {
  if (!parameters) return nullptr;
  for (const auto& p : *parameters)
    if (p.enum_member && p.key == member) return &p.value;
  for (const auto& p : *parameters)
    if (!p.enum_member && p.key == literal) return &p.value;
  return nullptr;
}
Value parameter_or(const PolarsFilterParameters& parameters, std::string_view member, std::string_view literal,
                   Value fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? *found : fallback;
}
std::string text_parameter(const PolarsFilterParameters& parameters, std::string_view member,
                           std::string_view literal, std::string_view fallback) {
  const auto* found = parameter(parameters, member, literal);
  return found ? found->text : std::string(fallback);
}
std::string case_key(const PolarsFilterParameters& p) {
  return text_parameter(p, "CASE_ID_KEY", kPolarsFilterCaseIdKeyParameter, kPolarsFilterCaseConceptName);
}
std::string activity_key(const PolarsFilterParameters& p) {
  return text_parameter(p, "ACTIVITY_KEY", kPolarsFilterActivityKeyParameter, kPolarsFilterDefaultNameKey);
}
bool positive_of(const PolarsFilterParameters& p) {
  return truthy(parameter_or(p, "POSITIVE", "positive", Value::of(true)));
}

// ------------------------------------------------------------------ plan errors

std::string quote(const std::string& v) { return '"' + v + '"'; }
std::string debug_quote(const std::string& v) {
  std::string result = "\"";
  for (const auto c : v) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      default: result += c;
    }
  }
  return result + '"';
}
std::string frame_plan(const std::vector<std::string>& columns) {
  std::string out = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, columns.size()); ++i) {
    if (i) out += ", ";
    out += quote(columns[i]);
  }
  if (columns.size() > 4) out += ", ...";
  return out + "]; PROJECT */" + std::to_string(columns.size()) + " COLUMNS";
}
std::string contexts(const std::vector<std::string>& nodes) {
  std::string out = "\n\nThis error occurred with the following context stack:\n";
  // A node written "!name" prints as 'name' failed.
  for (std::size_t i = 0; i < nodes.size(); ++i)
    out += "\t[" + std::to_string(i + 1) + "] '" +
           (nodes[i].starts_with('!') ? nodes[i].substr(1) + "' failed" : nodes[i] + "'") + "\n";
  return out;
}
std::string resolved(const std::string& node, const std::string& plan, const std::vector<std::string>& nodes) {
  return "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + node + "' <---\n" + plan + contexts(nodes);
}

// A plan node over the source frame: the failing node, the text above the
// source DF line, and the context stack.
struct Plan {
  std::string node;
  std::vector<std::string> stack;
  std::string above;  // printed before the indented source DF line
  std::vector<std::string> extra_columns;
  std::string source;  // the DF line when it is not the source frame
};

[[noreturn]] void column_not_found(const EventTable& df, const std::string& key, const Plan& plan) {
  auto valid = df.columns;
  valid.insert(valid.end(), plan.extra_columns.begin(), plan.extra_columns.end());
  std::string message = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < valid.size(); ++i) {
    if (i) message += ", ";
    message += debug_quote(valid[i]);
  }
  message += ']';
  if (const auto hint = detail::polars_column_hint(key, valid)) message += "\n\nDid you mean " + debug_quote(*hint) + '?';
  const auto source = plan.source.empty() ? frame_plan(df.columns) : plan.source;
  fail("ColumnNotFoundError", message + resolved(plan.node, plan.above.empty() ? source : plan.above + "\n  " + source,
                                                 plan.stack));
}

std::optional<std::size_t> find_column(const EventTable& df, const std::string& name) {
  for (std::size_t i = 0; i < df.columns.size(); ++i)
    if (df.columns[i] == name) return i;
  return std::nullopt;
}
std::size_t require(const EventTable& df, const std::string& name, const Plan& plan) {
  const auto found = find_column(df, name);
  if (!found) column_not_found(df, name, plan);
  return *found;
}

std::string dtype_of(const EventTable& df, std::size_t column) {
  return column < df.dtypes.size() ? df.dtypes[column] : std::string("String");
}
// Polars' short dtype names (join SchemaError, list[...] messages).
std::string short_dtype(const std::string& dtype) {
  if (dtype == "String") return "str";
  if (dtype == "Boolean") return "bool";
  if (dtype == "Null") return "null";
  if (dtype == "Date") return "date";
  if (dtype == "Time") return "time";
  if (dtype == "Binary") return "binary";
  if (dtype.rfind("Categorical", 0) == 0) return "cat";
  if (dtype.rfind("Enum", 0) == 0) return "enum";
  if (dtype.rfind("Float", 0) == 0) return "f" + dtype.substr(5);
  if (dtype.rfind("UInt", 0) == 0) return "u" + dtype.substr(4);
  if (dtype.rfind("Int", 0) == 0) return "i" + dtype.substr(3);
  const auto unit = [&](std::string_view head) -> std::string {
    const auto at = dtype.find("time_unit='");
    if (at == std::string::npos) return {};
    const auto u = dtype.substr(at + 11, 2);
    return std::string(head) + "[" + (u == "us" ? std::string("\xce\xbc" "s") : u);
  };
  if (dtype.rfind("Datetime", 0) == 0) {
    auto out = unit("datetime");
    const auto zone = dtype.find("time_zone='");
    if (zone != std::string::npos) out += ", " + dtype.substr(zone + 11, dtype.find('\'', zone + 11) - zone - 11);
    return out + "]";
  }
  if (dtype.rfind("Duration", 0) == 0) return unit("duration") + "]";
  return dtype;
}
bool integer_dtype(const std::string& dtype) {
  return dtype.rfind("Int", 0) == 0 || dtype.rfind("UInt", 0) == 0;
}
[[noreturn]] void join_dtype_error(const std::string& key, const std::string& left, const std::string& right) {
  fail("SchemaError", "datatypes of join keys don't match - `" + key + "`: " + left + " on left does not match `" + key +
                          "`: " + right + " on right (and no other type was available to cast to)" +
                          resolved("join", frame_plan({key}), {"join"}));
}

// ------------------------------------------------------------------ cells

bool null_cell(const Cell& cell) { return !cell || cell->is_null || is_native_na_attribute(*cell); }

// A case id (or other grouping value): null, or its value text.
struct Key {
  bool null = true;
  std::string text;
  bool operator==(const Key&) const = default;
  bool operator<(const Key& o) const { return null != o.null ? null : text < o.text; }
};
Key key_of(const Cell& cell) { return null_cell(cell) ? Key{} : Key{false, cell->value}; }

std::int64_t integer_of(const EventAttribute& cell) {
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return *i;
  if (const auto* u = std::get_if<std::uint64_t>(&cell.typed_value)) return static_cast<std::int64_t>(*u);
  return std::stoll(cell.value);
}

// Polars sort order of one column's cells: nulls first, integers and floats
// by value (NaN last), Enum values by category position, Booleans false
// first, everything else (Categorical included) by its UTF-8 text.
struct CellOrder {
  enum class Kind { kText, kInteger, kReal, kRank, kBoolean } kind = Kind::kText;
  std::map<std::string, std::size_t> ranks;
};
double real_of(const EventAttribute& cell) {
  if (const auto* d = std::get_if<double>(&cell.typed_value)) return *d;
  if (const auto* i = std::get_if<std::int64_t>(&cell.typed_value)) return static_cast<double>(*i);
  const auto& t = cell.value;
  if (t == "nan" || t == "NaN") return std::nan("");
  if (t == "inf" || t == "Infinity") return HUGE_VAL;
  if (t == "-inf" || t == "-Infinity") return -HUGE_VAL;
  return std::strtod(t.c_str(), nullptr);
}
bool true_of(const EventAttribute& cell) {
  if (const auto* b = std::get_if<bool>(&cell.typed_value)) return *b;
  return cell.value == "true" || cell.value == "True" || cell.value == "1";
}
bool cell_less(const Cell& a, const Cell& b, const CellOrder& order) {
  const bool an = null_cell(a), bn = null_cell(b);
  if (an || bn) return an && !bn;
  switch (order.kind) {
    case CellOrder::Kind::kInteger:
      return integer_of(*a) < integer_of(*b);
    case CellOrder::Kind::kReal: {
      const double x = real_of(*a), y = real_of(*b);
      if (std::isnan(x) || std::isnan(y)) return !std::isnan(x) && std::isnan(y);
      return x < y;
    }
    case CellOrder::Kind::kBoolean:
      return !true_of(*a) && true_of(*b);
    case CellOrder::Kind::kRank: {
      const auto x = order.ranks.find(a->value), y = order.ranks.find(b->value);
      const auto rx = x == order.ranks.end() ? order.ranks.size() : x->second;
      const auto ry = y == order.ranks.end() ? order.ranks.size() : y->second;
      return rx < ry;
    }
    case CellOrder::Kind::kText:
      break;
  }
  return a->value < b->value;
}

EventAttribute string_cell(const std::string& column, std::string value) {
  EventAttribute out;
  out.key = column;
  out.value = std::move(value);
  return out;
}
EventAttribute null_cell_of(const std::string& column) {
  EventAttribute out;
  out.key = column;
  out.is_null = true;
  return out;
}
EventAttribute int_cell(const std::string& column, std::int64_t value) {
  EventAttribute out;
  out.key = column;
  out.value = std::to_string(value);
  out.kind = AttributeKind::kInt;
  out.typed_value = value;
  return out;
}

EventTable subset(const EventTable& df, const std::vector<bool>& keep) {
  EventTable out = df;
  out.rows.clear();
  for (std::size_t i = 0; i < df.size(); ++i)
    if (keep[i]) out.rows.push_back(df.rows[i]);
  return out;
}

// df.join(right, how="inner") (positive) or how="anti" with a frame of case
// ids (`right`, in its row order; a null case id never matches). An inner
// join no taller than its right side comes in the right side's order, each
// right row followed by its matching left rows.
EventTable join_cases(const EventTable& df, std::size_t case_column, const std::vector<Key>& right, bool positive) {
  const std::set<Key> cases(right.begin(), right.end());
  if (positive && df.size() <= right.size()) {
    EventTable out = df;
    out.rows.clear();
    for (const auto& key : right) {
      if (key.null) continue;
      for (const auto& row : df.rows)
        if (key_of(row[case_column]) == key) out.rows.push_back(row);
    }
    return out;
  }
  std::vector<bool> keep(df.size());
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto key = key_of(df.rows[i][case_column]);
    const bool in = !key.null && cases.count(key);
    keep[i] = positive ? in : !in;
  }
  return subset(df, keep);
}

// Case groups (the null case id included) in first-appearance order.
struct Groups {
  std::vector<Key> order;
  std::map<Key, std::vector<std::size_t>> rows;
};
Groups groups_of(const EventTable& df, std::size_t case_column, const std::vector<std::size_t>& rows) {
  Groups out;
  for (const auto i : rows) {
    const auto key = key_of(df.rows[i][case_column]);
    auto [it, added] = out.rows.emplace(key, std::vector<std::size_t>{});
    if (added) out.order.push_back(key);
    it->second.push_back(i);
  }
  return out;
}
std::vector<std::size_t> all_rows(const EventTable& df) {
  std::vector<std::size_t> out(df.size());
  for (std::size_t i = 0; i < out.size(); ++i) out[i] = i;
  return out;
}

// ------------------------------------------------------------------ Enum keys

bool enum_dtype(const std::string& dtype) { return dtype.rfind("Enum", 0) == 0; }
// An Enum column's categories in order (its descriptor, else its dtype text).
std::vector<std::string> enum_category_list(const EventTable& df, std::size_t column) {
  std::vector<std::string> out;
  if (column < df.categorical_dtypes.size() && df.categorical_dtypes[column]) {
    for (const auto& c : df.categorical_dtypes[column]->categories) out.push_back(c.value);
    return out;
  }
  const auto dtype = dtype_of(df, column);
  for (std::size_t at = dtype.find('\''); at != std::string::npos;) {
    const auto end = dtype.find('\'', at + 1);
    if (end == std::string::npos) break;
    out.push_back(dtype.substr(at + 1, end - at - 1));
    at = dtype.find('\'', end + 1);
  }
  return out;
}
std::set<std::string> enum_categories(const EventTable& df, std::size_t column) {
  const auto list = enum_category_list(df, column);
  return {list.begin(), list.end()};
}
CellOrder cell_order(const EventTable& df, std::size_t column) {
  CellOrder out;
  const auto dtype = dtype_of(df, column);
  if (integer_dtype(dtype)) out.kind = CellOrder::Kind::kInteger;
  else if (dtype.rfind("Float", 0) == 0) out.kind = CellOrder::Kind::kReal;
  else if (dtype == "Boolean") out.kind = CellOrder::Kind::kBoolean;
  else if (enum_dtype(dtype)) {
    out.kind = CellOrder::Kind::kRank;
    const auto list = enum_category_list(df, column);
    for (std::size_t i = 0; i < list.size(); ++i) out.ranks.emplace(list[i], i);
  }
  return out;
}
// is_in with a list of str on an Enum column casts the list strictly: a
// value outside the categories fails (the first ten failures shown as Polars
// prints a short Series).
void enum_is_in(const EventTable& df, std::size_t column, const std::vector<Value>& values, const std::string& node,
                const std::string& plan, const std::vector<std::string>& stack) {
  if (!enum_dtype(dtype_of(df, column))) return;
  const auto categories = enum_categories(df, column);
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
                                    resolved(node, plan, stack));
}

// ------------------------------------------------------------------ typed activities

// Round CB: integer, float, Boolean, Date and Datetime activity columns.

bool float_dtype(const std::string& dtype) { return dtype.rfind("Float", 0) == 0; }
bool datetime_dtype(const std::string& dtype) { return dtype.rfind("Datetime", 0) == 0; }
bool categorical_dtype(const std::string& dtype) { return dtype.rfind("Categorical", 0) == 0; }
// The quoted field of a Datetime dtype ("time_unit='us'", "time_zone='UTC'").
std::optional<std::string> dtype_field(const std::string& dtype, const std::string& field) {
  const auto at = dtype.find(field + "='");
  if (at == std::string::npos) return std::nullopt;
  const auto from = at + field.size() + 2;
  return dtype.substr(from, dtype.find('\'', from) - from);
}
std::int64_t unit_nanoseconds(const std::string& dtype) {
  const auto unit = dtype_field(dtype, "time_unit").value_or("us");
  return unit == "ns" ? 1 : unit == "ms" ? 1'000'000 : 1'000;
}

// A temporal cell as a parsed timestamp (a Date at its midnight).
std::optional<ParsedExactTimestamp> timestamp_of(const EventAttribute& cell) {
  const auto scalar = python::from_attribute(cell);
  if (scalar.kind != python::Scalar::Kind::kDate) return std::nullopt;
  return scalar.date;
}
std::int64_t floor_div(std::int64_t a, std::int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }
// A Datetime cell's physical value (its UTC instant in the column's unit, a
// naive one's wall clock), a Date cell's days since the epoch.
std::optional<std::int64_t> physical_of(const EventAttribute& cell, const std::string& dtype) {
  const auto t = timestamp_of(cell);
  if (!t) return std::nullopt;
  if (dtype == "Date") return floor_div(t->seconds, 86'400);
  const auto unit = unit_nanoseconds(dtype);
  return t->seconds * (1'000'000'000 / unit) + static_cast<std::int64_t>(t->nanoseconds) / unit;
}

// The Python value Polars hands out for a cell: an int, float, bool, str,
// datetime.date, or datetime.datetime (microseconds, aware in the column's
// zone); a missing cell is None.
Value python_value(const Cell& cell, const std::string& dtype) {
  if (null_cell(cell)) return Value::none();
  if (integer_dtype(dtype)) return Value::of(integer_of(*cell));
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
bool same_key(const Value& a, const Value& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case Kind::kNone: return true;
    case Kind::kBool: return a.boolean == b.boolean;
    case Kind::kInt:
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
std::string string_cast(const EventAttribute& cell, const std::string& dtype) {
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

// A Python list as Polars builds it into a Series (the is_in, prefix and
// suffix literals): the first value's type decides when every value fits it
// (an Int64 list takes bools as 0/1, a Float64 one ints and bools); otherwise
// Polars rebuilds it strictly, as the first value's type, or as the
// supertype of the values (String, Float64, Int64, Boolean) when one is
// None, and raises at the first value of another type.
struct Literal {
  std::string dtype;          // Null, String, Int64, Float64, Boolean, Datetime, Date
  std::vector<Value> values;  // as the Series holds them
};
std::string literal_type(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "Null";
    case Kind::kBool: return "Boolean";
    case Kind::kInt: return "Int64";
    case Kind::kFloat: return "Float64";
    case Kind::kString: return "String";
    case Kind::kDatetime: return "Datetime";
    case Kind::kDate: return "Date";
    default: return "Object";
  }
}
// A datetime literal as Polars prints it: wall clock in its zone with 3 or 6
// fractional digits only when needed, then the zone's abbreviation (round
// CB).
std::string literal_datetime_text(const Value& v) {
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
  if (v.text == "UTC") return out + " UTC";
  try {
    return out + " " + ::minera::zone_database::info_at_utc(v.text, floor_div(v.instant_nanoseconds(), 1'000'000'000)).abbrev;
  } catch (const std::runtime_error&) {
    return out + " " + v.text;
  }
}
std::string literal_item(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "null";
    case Kind::kBool: return v.boolean ? "true" : "false";
    case Kind::kInt: return std::to_string(v.integer);
    case Kind::kFloat: return std::isnan(v.real) ? "NaN" : python::float_repr(v.real);
    case Kind::kString: return quote(v.text);
    case Kind::kDatetime: return literal_datetime_text(v);
    default: return pandas_filter_value_str(v);
  }
}
Literal list_literal(const std::vector<Value>& items) {
  Literal out{"Null", items};
  const auto first = std::find_if(items.begin(), items.end(), [](const Value& v) { return v.kind != Kind::kNone; });
  if (first == items.end()) return out;
  const auto type = literal_type(*first);
  const auto fits = [&](const Value& v) {
    if (v.kind == Kind::kNone) return true;
    if (type == "Int64") return v.kind == Kind::kInt || v.kind == Kind::kBool;
    if (type == "Float64") return v.kind == Kind::kFloat || v.kind == Kind::kInt || v.kind == Kind::kBool;
    return literal_type(v) == type;
  };
  if (std::all_of(items.begin(), items.end(), fits)) {
    out.dtype = type;
    for (auto& v : out.values) {
      if (type == "Int64" && v.kind == Kind::kBool) v = Value::of(std::int64_t{v.boolean});
      if (type == "Float64" && (v.kind == Kind::kBool || v.kind == Kind::kInt))
        v = Value::of(v.kind == Kind::kBool ? (v.boolean ? 1.0 : 0.0) : static_cast<double>(v.integer));
    }
    return out;
  }
  std::string target = literal_type(items.front());
  if (std::any_of(items.begin(), items.end(), [](const Value& v) { return v.kind == Kind::kNone; })) {
    const auto rank = [](const std::string& t) { return t == "String" ? 4 : t == "Float64" ? 3 : t == "Int64" ? 2 : 1; };
    target = type;
    for (const auto& v : items)
      if (v.kind != Kind::kNone && rank(literal_type(v)) > rank(target)) target = literal_type(v);
  }
  for (const auto& v : items)
    if (v.kind != Kind::kNone && literal_type(v) != target)
      fail("TypeError", "unexpected value while building Series of type " + target + "; found value of type " +
                            literal_type(v) + ": " + literal_item(v) +
                            "\n\nThis error occurred with the following context stack:\n\t[1] while constructing "
                            "Series ''\n\n\nHint: Try setting `strict=False` to allow passing data with mixed types.");
  out.dtype = target;
  return out;
}
// The literal in a plan: up to three values, else the first two, an
// ellipsis and the last.
std::string literal_text(const Literal& literal) {
  const auto& values = literal.values;
  std::string out = "[[";
  if (values.size() <= 3) {
    for (std::size_t i = 0; i < values.size(); ++i) out += (i ? ", " : "") + literal_item(values[i]);
  } else {
    out += literal_item(values[0]) + ", " + literal_item(values[1]) + ", \xe2\x80\xa6 " + literal_item(values.back());
  }
  return out + "]]";
}
// The literal's dtype as an error names it: a datetime list in
// microseconds, in the zone of its first value.
std::string literal_dtype_name(const Literal& literal) {
  if (literal.dtype != "Datetime") return literal.dtype;
  const auto first = std::find_if(literal.values.begin(), literal.values.end(),
                                  [](const Value& v) { return v.kind == Kind::kDatetime; });
  std::string out = "Datetime('\xce\xbc" "s'";
  if (first != literal.values.end() && first->offset_minutes) out += ", '" + first->text + "'";
  return out + ")";
}

// A column dtype as the is_in check names it.
std::string is_in_dtype_name(const std::string& dtype) {
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
// is_in with a list literal: a String list checks String, Categorical and
// Enum data (casting to an Enum strictly), a number list integer and float
// data, a Boolean list Boolean data; a list of only nulls any data.
void check_is_in(const EventTable& df, std::size_t column, const Literal& literal, const std::string& node,
                 const std::string& plan, const std::vector<std::string>& stack) {
  const auto dtype = dtype_of(df, column);
  const auto& l = literal.dtype;
  const bool numeric = integer_dtype(dtype) || float_dtype(dtype);
  const bool ok = l == "Null" ||
                  (l == "String" && (dtype == "String" || categorical_dtype(dtype) || enum_dtype(dtype))) ||
                  ((l == "Int64" || l == "Float64") && numeric) || (l == "Boolean" && dtype == "Boolean") ||
                  (l == "Datetime" && datetime_dtype(dtype)) || (l == "Date" && dtype == "Date");
  if (!ok)
    fail("InvalidOperationError", "'is_in' cannot check for List(" + literal_dtype_name(literal) + ") values in " +
                                      is_in_dtype_name(dtype) + " data" + resolved(node, plan, stack));
  // A datetime list holds microseconds: a millisecond column refuses it, a
  // nanosecond one compares the raw numbers (round CB).
  if (l == "Datetime" && unit_nanoseconds(dtype) == 1'000'000)
    fail("InvalidOperationError", "'is_in' cannot check for Microseconds precision values in Milliseconds Datetime "
                                  "data" + resolved(node, plan, stack));
  if (l == "String") enum_is_in(df, column, literal.values, node, plan, stack);
}
// The is_in expression as a resolved plan prints it once the literal and
// the column meet in their supertype: a narrower integer column casts to
// Int64 (UInt64 to Int128), an integer column strictly to Float64 for a
// float list, a Float32 column to Float64, and an Int64 list on float data
// holds floats.
std::string coerced_is_in(const std::string& column, const std::string& dtype, const Literal& literal) {
  std::string cast;
  Literal shown = literal;
  if (literal.dtype == "Int64" && integer_dtype(dtype) && dtype != "Int64")
    cast = dtype == "UInt64" ? ".cast(Int128)" : ".cast(Int64)";
  else if (literal.dtype == "Float64" && integer_dtype(dtype))
    cast = ".strict_cast(Float64)";
  else if ((literal.dtype == "Int64" || literal.dtype == "Float64") && float_dtype(dtype)) {
    if (dtype != "Float64") cast = ".cast(Float64)";
    for (auto& v : shown.values)
      if (v.kind == Kind::kInt) v = Value::of(static_cast<double>(v.integer));
  }
  return "col(" + quote(column) + ")" + cast + ".is_in(" + literal_text(shown) + ")";
}
// A present cell `is_in` the literal: numbers by value (NaN in NaN), str by
// text, temporal values by instant; a null never is.
bool literal_has(const Literal& literal, const Cell& cell, const std::string& dtype) {
  if (null_cell(cell)) return false;
  const auto value = python_value(cell, dtype);
  for (const auto& v : literal.values) {
    if (v.kind == Kind::kNone) continue;
    if (literal.dtype == "String") {
      if (v.kind == Kind::kString && v.text == cell->value) return true;
    } else if (literal.dtype == "Int64" || literal.dtype == "Float64") {
      if (value.kind == Kind::kInt && v.kind == Kind::kInt) {
        if (value.integer == v.integer) return true;
        continue;
      }
      const double x = value.kind == Kind::kInt ? static_cast<double>(value.integer) : value.real;
      const double y = v.kind == Kind::kInt ? static_cast<double>(v.integer) : v.real;
      if (x == y || (std::isnan(x) && std::isnan(y))) return true;
    } else if (literal.dtype == "Datetime") {
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
std::optional<Value> cast_to(const Value& v, const std::string& dtype, const std::set<std::string>* categories) {
  if (v.kind == Kind::kNone) return std::nullopt;
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
    if (v.kind == Kind::kFloat) return v;
    if (v.kind == Kind::kInt) return Value::of(static_cast<double>(v.integer));
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
// A cell equals a cast pattern value.
bool cell_equals(const EventAttribute& cell, const Value& v, const std::string& dtype) {
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

// The Python type name of a value (str.join and iteration errors).
std::string py_type(const Value& v) {
  switch (v.kind) {
    case Kind::kNone: return "NoneType";
    case Kind::kBool: return "bool";
    case Kind::kInt: return "int";
    case Kind::kFloat: return "float";
    case Kind::kString: return "str";
    case Kind::kList: return "list";
    case Kind::kTuple: return "tuple";
    case Kind::kDict: return "dict";
    case Kind::kDatetime: return "datetime";
    case Kind::kDate: return "date";
  }
  return "object";
}
bool iterable(const Value& v) {
  return v.kind == Kind::kString || v.kind == Kind::kList || v.kind == Kind::kTuple || v.kind == Kind::kDict;
}
// ------------------------------------------------------------------ boundaries

const Plan kBoundaryPlan{"join", {"group_by", "filter", "select", "join right", "join"}, {}, {}};

EventTable boundary_filter(const EventTable& df, const Value& values, const std::string& case_glue,
                           const std::string& act_key, bool positive, bool end) {
  const auto literal = list_literal(values.items);
  const auto case_column = require(df, case_glue, kBoundaryPlan);
  const auto act = require(df, act_key, kBoundaryPlan);
  const std::string alias = end ? "end_activity" : "start_activity";
  check_is_in(df, act, literal, "join",
             "FILTER col(" + quote(alias) + ").is_in(" + literal_text(literal) +
                 ")\nFROM\n  AGGREGATE[maintain_order: false]\n    [col(" + quote(act_key) + ")." +
                 (end ? "last" : "first") + "().alias(" + quote(alias) + ")] BY [col(" + quote(case_glue) +
                 ")]\n    FROM\n    " + frame_plan(df.columns),
             {"!filter", "select", "join right", "join"});
  const auto groups = groups_of(df, case_column, all_rows(df));
  std::vector<Key> kept;
  for (const auto& key : groups.order) {
    const auto& rows = groups.rows.at(key);
    if (literal_has(literal, df.rows[end ? rows.back() : rows.front()][act], dtype_of(df, act))) kept.push_back(key);
  }
  return join_cases(df, case_column, kept, positive);
}

// End activity counts: (activity or null, count) by first appearance.
using Counts = std::vector<std::pair<Value, Value>>;
const Plan kEndCountPlan{"group_by", {"group_by", "group_by"}, {}, {}};

Counts end_counts(const EventTable& df, const std::string& case_glue, const std::string& act_key) {
  const auto case_column = require(df, case_glue, kEndCountPlan);
  const auto act = require(df, act_key, kEndCountPlan);
  const auto groups = groups_of(df, case_column, all_rows(df));
  Counts out;
  for (const auto& key : groups.order) {
    const auto activity = python_value(df.rows[groups.rows.at(key).back()][act], dtype_of(df, act));
    bool found = false;
    for (auto& [k, v] : out)
      if (same_key(k, activity)) {
        ++v.integer;
        found = true;
        break;
      }
    if (!found) out.emplace_back(activity, Value::of(std::int64_t{1}));
  }
  return out;
}

// ------------------------------------------------------------------ variants

const std::string kTimestamp(kPolarsFilterDefaultTimestampKey);

// Rows sorted by case and timestamp (Polars' unstable sort, nulls first),
// grouped by case.
struct Variants {
  std::size_t case_column = 0, act = 0;
  std::vector<Key> order;                              // cases in sorted order
  std::vector<Cell> case_cells;                        // one per case
  std::vector<std::vector<Cell>> activities;           // per case, sorted
};
Variants variants_of(const EventTable& df, const std::string& case_glue, const std::string& act_key,
                     const Plan& sort_plan, const Plan& agg_plan) {
  Variants out;
  out.case_column = require(df, case_glue, sort_plan);
  const auto ts = require(df, kTimestamp, sort_plan);
  out.act = find_column(df, act_key).value_or(df.columns.size());
  if (out.act == df.columns.size()) {
    auto plan = agg_plan;
    plan.above = "SORT BY [col(" + quote(case_glue) + "), col(" + quote(kTimestamp) + ")]";
    column_not_found(df, act_key, plan);
  }
  const auto case_order = cell_order(df, out.case_column);
  const CellOrder ts_order;
  auto rows = all_rows(df);
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  polars_sort::sort_by_keys(rows, [&](std::size_t a, std::size_t b) {
    const auto& ca = df.rows[a][out.case_column];
    const auto& cb = df.rows[b][out.case_column];
    if (cell_less(ca, cb, case_order)) return true;
    if (cell_less(cb, ca, case_order)) return false;
    return cell_less(df.rows[a][ts], df.rows[b][ts], ts_order);
  }, dtype_of(df, out.case_column), dtype_of(df, ts));
  for (const auto i : rows) {
    const auto key = key_of(df.rows[i][out.case_column]);
    if (out.order.empty() || !(out.order.back() == key)) {
      out.order.push_back(key);
      out.case_cells.push_back(df.rows[i][out.case_column]);
      out.activities.emplace_back();
    }
    out.activities.back().push_back(df.rows[i][out.act]);
  }
  return out;
}

// The activities cast to String and joined (a null left out).
std::string variant_string(const std::vector<Cell>& activities, const std::string& dtype = "String") {
  std::string out;
  bool first = true;
  for (const auto& cell : activities) {
    if (null_cell(cell)) continue;
    if (!first) out += ",";
    out += string_cast(*cell, dtype);
    first = false;
  }
  return out;
}

// The characters of a Python str (UTF-8 sequences).
std::vector<std::string> characters(const std::string& text) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < text.size();) {
    std::size_t len = 1;
    const auto c = static_cast<unsigned char>(text[i]);
    if (c >= 0xF0) len = 4;
    else if (c >= 0xE0) len = 3;
    else if (c >= 0xC0) len = 2;
    out.push_back(text.substr(i, len));
    i += len;
  }
  return out;
}
// The elements a Python iteration of a value yields (a str its characters,
// a dict its keys).
std::vector<Value> elements(const Value& v) {
  std::vector<Value> out;
  if (v.kind == Kind::kString)
    for (auto& c : characters(v.text)) out.push_back(Value::of(std::move(c)));
  else if (v.kind == Kind::kDict)
    for (const auto& [key, item] : v.entries) out.push_back(key);
  else
    out = v.items;
  return out;
}
// ",".join(item): a str item joins its characters; any other iterable must
// hold only str (Python's TypeError names the first other item), and a
// value that is not iterable cannot be joined.
std::string python_join(const Value& item) {
  if (!iterable(item)) fail("TypeError", "can only join an iterable");
  const auto parts = elements(item);
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (parts[i].kind != Kind::kString)
      fail("TypeError", "sequence item " + std::to_string(i) + ": expected str instance, " + py_type(parts[i]) + " found");
    out += (i ? "," : "") + parts[i].text;
  }
  return out;
}

// The activities of an admitted item: a str iterates over its characters.
std::vector<std::string> pattern_items(const Value& item) {
  if (item.kind == Kind::kString) return characters(item.text);
  std::vector<std::string> out;
  for (const auto& v : item.items) out.push_back(v.text);
  return out;
}
std::string join_pattern(const Value& item) {
  std::string out;
  const auto items = pattern_items(item);
  for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
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

const Plan kTraceSortPlan{"with_columns", {"sort", "group_by", "with_columns"}, {}, {}};
const Plan kTraceAggPlan{"with_columns", {"group_by", "with_columns"}, {}, {}};
const Plan kSuffixSortPlan{"group_by", {"sort", "group_by"}, {}, {}};
const Plan kSuffixAggPlan{"group_by", {"group_by"}, {}, {}};
const Plan kLazySortPlan{"join", {"sort", "group_by", "filter", "select", "join right", "join"}, {}, {}};
const Plan kLazyAggPlan{"join", {"group_by", "filter", "select", "join right", "join"}, {}, {}};

// The variants frame keeps the case column's Categorical/Enum descriptor.
std::vector<std::optional<EventTableCategoricalDtype>> case_descriptor(const EventTable& df, std::size_t case_column,
                                                                       std::size_t width) {
  if (case_column >= df.categorical_dtypes.size() || !df.categorical_dtypes[case_column]) return {};
  std::vector<std::optional<EventTableCategoricalDtype>> out(width);
  out[0] = df.categorical_dtypes[case_column];
  return out;
}

// variant_list joins the activity lists into str: only a String activity
// column can (Categorical, Enum and every other dtype raise).
void require_string_activities(const EventTable& df, const Variants& v, const std::string& case_glue,
                               const std::string& act_key) {
  const auto dtype = dtype_of(df, v.act);
  if (dtype == "String") return;
  fail("InvalidOperationError",
       "attempted list join with non-string dtype: list[" + short_dtype(dtype) + "]" +
           resolved("with_columns",
                    "AGGREGATE[maintain_order: true]\n  [col(" + quote(act_key) +
                        ").implode().alias(\"variant_list\"), len().alias(\"count\")] BY [col(" + quote(case_glue) +
                        ")]\n  FROM\n  SORT BY [col(" + quote(case_glue) + "), col(" + quote(kTimestamp) + ")]\n    " +
                        frame_plan(df.columns),
                    {"with_columns"}));
}

EventTable trace_variants_frame(const EventTable& df, const Variants& v) {
  EventTable out;
  const auto& case_name = df.columns[v.case_column];
  out.columns = {case_name, "variant_list", "count", "variant"};
  out.dtypes = {dtype_of(df, v.case_column), "List(String)", "UInt32", "String"};
  out.categorical_dtypes = case_descriptor(df, v.case_column, 4);
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    EventAttribute list;
    list.key = "variant_list";
    list.container = AttributeContainerKind::kList;
    for (const auto& cell : v.activities[c]) list.children.push_back(null_cell(cell) ? null_cell_of("") : string_cell("", cell->value));
    out.rows.push_back({v.case_cells[c], list, int_cell("count", static_cast<std::int64_t>(v.activities[c].size())),
                        string_cell("variant", variant_string(v.activities[c]))});
  }
  return out;
}

// Matching case ids joined back as a Python list: its dtype comes from the
// list's values, so the join needs the source dtype to agree.
EventTable join_case_list(const EventTable& df, std::size_t case_column, const std::vector<Key>& matched,
                          bool positive) {
  if (matched.empty()) return positive ? subset(df, std::vector<bool>(df.size(), false)) : df;
  const auto dtype = dtype_of(df, case_column);
  const auto left = short_dtype(dtype);
  bool any = false;
  for (const auto& k : matched) any = any || !k.null;
  // The list's dtype from its Python values (Categorical/Enum cases give
  // str); an integer key of any width casts to the i64 side.
  const std::string right = !any                         ? "null"
                            : integer_dtype(dtype)       ? "i64"
                            : dtype.rfind("Float", 0) == 0 ? "f64"
                            : dtype == "Boolean"         ? "bool"
                                                         : "str";
  if (left != right && !(right == "i64" && integer_dtype(dtype))) join_dtype_error(df.columns[case_column], left, right);
  return join_cases(df, case_column, matched, positive);
}

// A lazy variants filter over the sorted, grouped rows.
template <typename Predicate>
EventTable lazy_variant_filter(const EventTable& df, const PolarsFilterParameters& parameters, Predicate&& matches) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kLazySortPlan, kLazyAggPlan);
  std::vector<Key> kept;
  for (std::size_t c = 0; c < v.order.size(); ++c)
    if (matches(v.activities[c])) kept.push_back(v.order[c]);
  return join_cases(df, v.case_column, kept, positive_of(parameters));
}

// ------------------------------------------------------------------ rework

const Plan kReworkPlan{"join", {"group_by", "filter", "select", "unique", "join right", "join"}, {}, {}};

EventTable rework(const EventTable& df, const PolarsFilterParameters& parameters, const Value* activities) {
  const auto case_glue = case_key(parameters);
  const auto act_key = activity_key(parameters);
  auto plan = kReworkPlan;
  std::size_t act = 0;
  const auto literal = list_literal(activities ? activities->items : std::vector<Value>{});
  if (activities) {
    act = require(df, act_key, kReworkPlan);
    plan.above = (activities->items.empty() ? "FILTER false.alias(" + quote(act_key) + ")"
                                            : "FILTER col(" + quote(act_key) + ").is_in(" + literal_text(literal) + ")") +
                 "\nFROM";
    auto stack = kReworkPlan.stack;
    stack.insert(stack.begin(), "!filter");
    if (!activities->items.empty()) {
      check_is_in(df, act, literal, "join", plan.above + "\n  " + frame_plan(df.columns), stack);
      plan.above = "FILTER " + coerced_is_in(act_key, dtype_of(df, act), literal) + "\nFROM";
    }
  }
  const auto case_column = require(df, case_glue, plan);
  if (!activities) act = require(df, act_key, plan);
  const auto minimum = parameter_or(parameters, "MIN_OCCURRENCES", "min_occurrences", Value::of(std::int64_t{2}));
  const auto threshold = number_of(minimum);
  std::vector<std::size_t> rows;
  for (std::size_t i = 0; i < df.size(); ++i)
    if (!activities || literal_has(literal, df.rows[i][act], dtype_of(df, act))) rows.push_back(i);
  std::map<std::pair<Key, Key>, std::int64_t> counts;
  for (const auto i : rows) ++counts[{key_of(df.rows[i][case_column]), key_of(df.rows[i][act])}];
  std::vector<Key> kept;
  if (threshold)
    for (const auto& [key, count] : counts)
      if (at_least(Number{true, count, 0.0}, *threshold) &&
          std::find(kept.begin(), kept.end(), key.first) == kept.end())
        kept.push_back(key.first);
  return join_cases(df, case_column, kept, true);
}

// ------------------------------------------------------------------ grouping

std::string filter_type(const PolarsFilterParameters& parameters) {
  const auto value = parameter_or(parameters, "FILTER_TYPE", "filter_type", Value::of("first"));
  return value.kind == Kind::kString ? value.text : std::string();
}

}  // namespace

// ------------------------------------------------------------------ public

EventTable polars_start_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                         const PolarsFilterParameters& parameters) {
  return boundary_filter(df, values, case_key(parameters), activity_key(parameters), positive_of(parameters), false);
}
EventTable polars_end_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                       const PolarsFilterParameters& parameters) {
  return boundary_filter(df, values, case_key(parameters), activity_key(parameters), positive_of(parameters), true);
}
EventTable polars_filter_df_on_end_activities(const EventTable& df, const PolarsFilterValue& values,
                                              const std::string& case_id_glue, const std::string& activity_key,
                                              const PolarsFilterValue& positive) {
  return boundary_filter(df, values, case_id_glue, activity_key, truthy(positive), true);
}

PolarsFilterValue polars_get_end_activities(const EventTable& df, const std::string& case_id_key,
                                            const std::string& activity_key) {
  return Value::dict(end_counts(df, case_id_key, activity_key));
}

PolarsEndActivitiesNocc polars_filter_df_on_end_activities_nocc(
    const EventTable& df, const PolarsFilterValue& nocc, const std::optional<PolarsFilterValue>& ea_count0,
    const std::string& case_id_glue, const std::string& activity_key, bool return_dict,
    const std::optional<PolarsFilterValue>& most_common_variant) {
  const Counts counts = ea_count0 ? ea_count0->entries : end_counts(df, case_id_glue, activity_key);
  const auto threshold = number_of(nocc);
  if (!threshold) fail("TypeError", "'>=' not supported between instances of 'int' and 'NoneType'");
  const Value* protected_end = most_common_variant && truthy(*most_common_variant) ? &most_common_variant->items.back()
                                                                                    : nullptr;
  Counts kept;
  for (const auto& entry : counts) {
    const auto count = number_of(entry.second);
    const bool ok = count && at_least(*count, *threshold);
    const bool keep_end = protected_end && same_key(entry.first, *protected_end);
    if (ok || keep_end) kept.push_back(entry);
  }
  PolarsEndActivitiesNocc out;
  if (kept.size() == counts.size()) {
    out.table = df;
  } else {
    std::vector<Value> values;
    for (const auto& entry : kept) values.push_back(entry.first);
    out.table = boundary_filter(df, Value::list(values), case_id_glue, activity_key, true, true);
  }
  if (return_dict) out.counts = Value::dict(kept);
  return out;
}

EventTable polars_trace_filter_get_variants_df(const EventTable& df, const std::string& case_id_key,
                                               const std::string& activity_key) {
  const auto v = variants_of(df, case_id_key, activity_key, kTraceSortPlan, kTraceAggPlan);
  require_string_activities(df, v, case_id_key, activity_key);
  return trace_variants_frame(df, v);
}

EventTable polars_ends_with_filter_get_variants_df(const EventTable& df, const std::string& case_id_key,
                                                   const std::string& activity_key) {
  const auto v = variants_of(df, case_id_key, activity_key, kSuffixSortPlan, kSuffixAggPlan);
  EventTable out;
  out.columns = {df.columns[v.case_column], "variant"};
  out.dtypes = {dtype_of(df, v.case_column), "String"};
  out.categorical_dtypes = case_descriptor(df, v.case_column, 2);
  for (std::size_t c = 0; c < v.order.size(); ++c)
    out.rows.push_back({v.case_cells[c], string_cell("variant", variant_string(v.activities[c], dtype_of(df, v.act)))});
  return out;
}

EventTable polars_trace_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_traces,
                                     const PolarsFilterParameters& parameters) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kTraceSortPlan, kTraceAggPlan);
  require_string_activities(df, v, case_key(parameters), activity_key(parameters));
  // Each admitted trace is iterated and its items read as str or None
  // (round CB).
  for (const auto& item : admitted_traces.items) {
    if (!iterable(item)) fail("TypeError", "'" + py_type(item) + "' object is not iterable");
    for (const auto& part : elements(item))
      if (part.kind != Kind::kString && part.kind != Kind::kNone)
        fail("TypeError", "decoding to str: need a bytes-like object, " + py_type(part) + " found");
  }
  std::vector<std::string> patterns;
  for (const auto& item : admitted_traces.items) patterns.push_back(join_pattern(item));
  std::vector<Key> matched;
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    const auto variant = variant_string(v.activities[c]);
    bool ok = patterns.empty();
    for (const auto& p : patterns) ok = ok || trace_match(variant, p);
    if (ok) matched.push_back(v.order[c]);
  }
  return join_case_list(df, v.case_column, matched, positive_of(parameters));
}

EventTable polars_starts_with_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                           const PolarsFilterParameters& parameters) {
  if (prefixes.items.empty()) fail("TypeError", "invalid predicate for `filter`: None");
  std::vector<std::string> patterns;
  for (const auto& item : prefixes.items) patterns.push_back(python_join(item));
  // The variant casts the activities to String (round CB).
  const auto act = find_column(df, activity_key(parameters));
  const auto dtype = act ? dtype_of(df, *act) : std::string("String");
  return lazy_variant_filter(df, parameters, [&](const std::vector<Cell>& activities) {
    const auto variant = variant_string(activities, dtype);
    for (const auto& p : patterns)
      if (variant.starts_with(p)) return true;
    return false;
  });
}

EventTable polars_ends_with_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_suffixes,
                                         const PolarsFilterParameters& parameters) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kSuffixSortPlan, kSuffixAggPlan);
  if (admitted_suffixes.items.empty()) fail("IndexError", "list index out of range");
  // A list or tuple first item joins every item with ","; otherwise each
  // item is a literal cast to String (round CB: an int as its digits, None
  // as null, a list fails with its inner dtype).
  const auto& front = admitted_suffixes.items.front();
  const bool joined = front.kind == Kind::kList || front.kind == Kind::kTuple;
  std::vector<std::optional<std::string>> suffixes;
  for (const auto& item : admitted_suffixes.items) {
    if (joined) {
      suffixes.emplace_back(python_join(item));
    } else if (item.kind == Kind::kList || item.kind == Kind::kTuple) {
      fail("InvalidOperationError",
           "cannot cast List type (inner: '" + list_literal(item.items).dtype + "', to: 'String')");
    } else if (item.kind == Kind::kNone) {
      suffixes.emplace_back();
    } else {
      suffixes.emplace_back(item.kind == Kind::kString ? item.text : literal_item(item));
    }
  }
  std::vector<Key> kept;
  for (std::size_t c = 0; c < v.order.size(); ++c) {
    const auto variant = variant_string(v.activities[c], dtype_of(df, v.act));
    for (const auto& s : suffixes)
      if (s && variant.ends_with(*s)) {
        kept.push_back(v.order[c]);
        break;
      }
  }
  return join_cases(df, v.case_column, kept, positive_of(parameters));
}

namespace {
EventTable list_filter(const EventTable& df, const PolarsFilterValue& patterns,
                       const PolarsFilterParameters& parameters, bool tail) {
  // Every pattern is iterated in Python first (round CB).
  for (const auto& item : patterns.items)
    if (!iterable(item)) fail("TypeError", "'" + py_type(item) + "' object is not iterable");
  if (patterns.items.empty()) {
    const auto case_glue = case_key(parameters);
    const auto column = require(df, case_glue, Plan{"join", {"join"}, {}, {}, frame_plan({case_glue})});
    join_dtype_error(case_glue, short_dtype(dtype_of(df, column)), "null");
  }
  // Each pattern becomes a Series literal cast (strict=False) to the
  // activity dtype: on an Enum column a value outside the categories, on a
  // typed column an out-of-range or unparsable value becomes null (and a
  // null item equals a null activity).
  const auto act = find_column(df, activity_key(parameters));
  const auto dtype = act ? dtype_of(df, *act) : std::string("String");
  const bool as_enum = act && enum_dtype(dtype);
  const auto categories = as_enum ? enum_categories(df, *act) : std::set<std::string>{};
  std::vector<std::vector<std::optional<Value>>> lists;
  for (const auto& item : patterns.items) {
    auto& list = lists.emplace_back();
    for (const auto& v : list_literal(elements(item)).values) list.push_back(cast_to(v, dtype, as_enum ? &categories : nullptr));
  }
  return lazy_variant_filter(df, parameters, [&](const std::vector<Cell>& activities) {
    for (const auto& p : lists) {
      if (p.size() > activities.size()) continue;
      const std::size_t offset = tail ? activities.size() - p.size() : 0;
      bool same = true;
      for (std::size_t i = 0; i < p.size() && same; ++i) {
        const auto& cell = activities[offset + i];
        same = null_cell(cell) ? !p[i] : p[i] && cell_equals(*cell, *p[i], dtype);
      }
      if (same) return true;
    }
    return false;
  });
}
}  // namespace

EventTable polars_prefix_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                      const PolarsFilterParameters& parameters) {
  return list_filter(df, prefixes, parameters, false);
}
EventTable polars_suffix_filter_apply(const EventTable& df, const PolarsFilterValue& suffixes,
                                      const PolarsFilterParameters& parameters) {
  return list_filter(df, suffixes, parameters, true);
}

EventTable polars_rework_filter_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  return rework(df, parameters, nullptr);
}
EventTable polars_rework_filter_apply_activity_set(const EventTable& df, const PolarsFilterValue& activities,
                                                   const PolarsFilterParameters& parameters) {
  return rework(df, parameters, &activities);
}

EventTable polars_consecutive_act_case_grouping_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  const auto type = filter_type(parameters);
  const bool grouped = type == "first" || type == "last";
  const Plan plan{"select",
                  grouped ? std::vector<std::string>{"with_columns", "group_by", "select", "select"}
                          : std::vector<std::string>{"with_columns", "select"},
                  " WITH_COLUMNS:\n [dyn int: 1.cum_sum().alias(\"_row_num\")] ",
                  {"_row_num"}};
  const auto case_column = require(df, case_key(parameters), plan);
  const auto act = require(df, activity_key(parameters), plan);
  if (!grouped) return df;
  EventTable out = df;
  out.rows.clear();
  std::size_t start = 0;
  for (std::size_t i = 1; i <= df.size(); ++i) {
    bool same = false;
    if (i < df.size()) {
      const auto &c0 = df.rows[i - 1][case_column], &c1 = df.rows[i][case_column];
      const auto &a0 = df.rows[i - 1][act], &a1 = df.rows[i][act];
      same = !null_cell(c0) && !null_cell(c1) && c0->value == c1->value && !null_cell(a0) && !null_cell(a1) &&
             a0->value == a1->value;
    }
    if (same) continue;
    out.rows.push_back(df.rows[type == "first" ? start : i - 1]);
    start = i;
  }
  return out;
}

EventTable polars_timestamp_case_grouping_apply(const EventTable& df, const PolarsFilterParameters& parameters) {
  const auto type = filter_type(parameters);
  if (type != "first" && type != "last") return df;
  const Plan plan{"group_by", {"group_by"}, {}, {}};
  const auto case_column = require(df, case_key(parameters), plan);
  const auto ts = require(df, text_parameter(parameters, "TIMESTAMP_KEY", kPolarsFilterTimestampKeyParameter,
                                             kPolarsFilterDefaultTimestampKey),
                          plan);
  if (type != "first" && type != "last") return df;
  std::vector<std::size_t> order{case_column, ts};
  for (std::size_t c = 0; c < df.columns.size(); ++c)
    if (c != case_column && c != ts) order.push_back(c);
  std::vector<std::pair<Key, Key>> groups;
  std::map<std::pair<Key, Key>, std::vector<std::size_t>> rows;
  for (std::size_t i = 0; i < df.size(); ++i) {
    const std::pair<Key, Key> key{key_of(df.rows[i][case_column]), key_of(df.rows[i][ts])};
    auto [it, added] = rows.emplace(key, std::vector<std::size_t>{});
    if (added) groups.push_back(key);
    it->second.push_back(i);
  }
  EventTable out = df;
  out.columns.clear();
  out.dtypes.clear();
  out.categorical_dtypes.clear();
  out.rows.clear();
  for (const auto c : order) {
    out.columns.push_back(df.columns[c]);
    if (!df.dtypes.empty()) out.dtypes.push_back(df.dtypes[c]);
    if (!df.categorical_dtypes.empty()) out.categorical_dtypes.push_back(df.categorical_dtypes[c]);
  }
  for (const auto& key : groups) {
    const auto& members = rows.at(key);
    const auto& source = df.rows[type == "first" ? members.front() : members.back()];
    EventTable::Row row;
    for (const auto c : order) row.push_back(source[c]);
    out.rows.push_back(std::move(row));
  }
  return out;
}

EventTable polars_activity_split_apply(const EventTable& df, const PolarsFilterValue& activity,
                                       const PolarsFilterParameters& parameters) {
  const auto case_glue = case_key(parameters);
  const auto act_key = activity_key(parameters);
  const auto mode = parameter_or(parameters, "CUT_MODE", "cut_mode", Value::of("this"));
  const bool this_mode = mode.kind == Kind::kString && mode.text == "this";
  const auto concat = parameter_or(parameters, "SUBCASE_CONCAT_STR", "subcase_concat_str", Value::of("##@@"));
  const auto case_column = require(
      df, case_glue,
      Plan{"select",
           this_mode ? std::vector<std::string>{"with_columns", "with_columns", "with_columns", "with_columns", "select"}
                     : std::vector<std::string>{"with_columns", "with_columns", "with_columns", "with_columns",
                                                "select", "with_columns", "select"},
           {},
           {}});
  const Plan act_plan{"select",
                      this_mode ? std::vector<std::string>{"with_columns", "with_columns", "with_columns", "select"}
                                : std::vector<std::string>{"with_columns", "with_columns", "with_columns", "select",
                                                           "with_columns", "select"},
                      " WITH_COLUMNS:\n [col(" + quote(case_glue) + ").cum_count().over([col(" + quote(case_glue) +
                          ")]).alias(\"_row_within_case\")] ",
                      {"_row_within_case"}};
  const auto act = require(df, act_key, act_plan);
  // A list or tuple holds the activities; any other value is one (round CB).
  const std::vector<Value> activities =
      activity.kind == Kind::kList || activity.kind == Kind::kTuple ? activity.items : std::vector<Value>{activity};
  const auto literal = list_literal(activities);
  auto enum_stack = act_plan.stack;
  enum_stack.front() = "!with_columns";
  check_is_in(df, act, literal, "select",
             " WITH_COLUMNS:\n [col(" + quote(act_key) + ").is_in(" + literal_text(literal) +
                 ").alias(\"_is_split_activity\")] \n   WITH_COLUMNS:\n   [col(" + quote(case_glue) +
                 ").cum_count().over([col(" + quote(case_glue) + ")]).alias(\"_row_within_case\")] \n    " +
                 frame_plan(df.columns),
             enum_stack);
  // Per case: the running count ("this"), null on a null activity.
  std::map<Key, std::int64_t> running;
  std::map<Key, std::optional<std::int64_t>> previous;  // the previous row's running count
  std::map<Key, bool> seen;
  EventTable out = df;
  if (!out.dtypes.empty()) out.dtypes[case_column] = "String";
  if (!out.categorical_dtypes.empty()) {
    out.categorical_dtypes[case_column] = std::nullopt;
    if (std::all_of(out.categorical_dtypes.begin(), out.categorical_dtypes.end(),
                    [](const auto& d) { return !d.has_value(); }))
      out.categorical_dtypes.clear();
  }
  for (std::size_t i = 0; i < df.size(); ++i) {
    const auto& case_cell = df.rows[i][case_column];
    const auto key = key_of(case_cell);
    const auto& a = df.rows[i][act];
    std::optional<std::int64_t> current;
    if (!null_cell(a)) {
      running[key] += literal_has(literal, a, dtype_of(df, act)) ? 1 : 0;
      current = running[key];
    }
    std::optional<std::int64_t> counter;
    if (this_mode) counter = current;
    else counter = seen[key] ? previous[key] : std::optional<std::int64_t>(0);
    seen[key] = true;
    previous[key] = current;
    auto& cell = out.rows[i][case_column];
    if (key.null || !counter || concat.kind != Kind::kString) cell = null_cell_of(case_glue);
    else cell = string_cell(case_glue, case_cell->value + concat.text + std::to_string(*counter));
  }
  return out;
}

// ------------------------------------------------------------------ lazy

namespace {
template <typename Function>
LazyEventTable lazily(const LazyEventTable& df, Function&& function) {
  return LazyEventTable::from_table(function(df.materialize()));
}
}  // namespace

LazyEventTable polars_start_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                             const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_start_activities_apply(t, values, parameters); });
}
LazyEventTable polars_end_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                           const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_end_activities_apply(t, values, parameters); });
}
LazyEventTable polars_trace_filter_apply(const LazyEventTable& df, const PolarsFilterValue& admitted_traces,
                                         const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_trace_filter_apply(t, admitted_traces, parameters); });
}
LazyEventTable polars_starts_with_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                               const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_starts_with_filter_apply(t, prefixes, parameters); });
}
LazyEventTable polars_ends_with_filter_apply(const LazyEventTable& df, const PolarsFilterValue& admitted_suffixes,
                                             const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_ends_with_filter_apply(t, admitted_suffixes, parameters); });
}
LazyEventTable polars_prefix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_prefix_filter_apply(t, prefixes, parameters); });
}
LazyEventTable polars_suffix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& suffixes,
                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_suffix_filter_apply(t, suffixes, parameters); });
}
LazyEventTable polars_rework_filter_apply(const LazyEventTable& df, const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_rework_filter_apply(t, parameters); });
}
LazyEventTable polars_consecutive_act_case_grouping_apply(const LazyEventTable& df,
                                                          const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_consecutive_act_case_grouping_apply(t, parameters); });
}
LazyEventTable polars_timestamp_case_grouping_apply(const LazyEventTable& df, const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_timestamp_case_grouping_apply(t, parameters); });
}
LazyEventTable polars_activity_split_apply(const LazyEventTable& df, const PolarsFilterValue& activity,
                                           const PolarsFilterParameters& parameters) {
  return lazily(df, [&](const EventTable& t) { return polars_activity_split_apply(t, activity, parameters); });
}

}  // namespace minera
