#include "minera/polars_sequence_filters.hpp"
#include "polars_column_hint.hpp"
#include "polars_sort.hpp"
#include "polars_lit_model.hpp"
#include "python_scalar.hpp"
#include "zone_database.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
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
// ------------------------------------------------------------------ typed activities and list literals

// The typed-activity and pl.lit rules live in polars_lit_model.hpp (shared
// with the Polars table filters, round CD).
using detail::polars_lit::float_dtype;
using detail::polars_lit::datetime_dtype;
using detail::polars_lit::categorical_dtype;
using detail::polars_lit::dtype_field;
using detail::polars_lit::unit_nanoseconds;
using detail::polars_lit::timestamp_of;
using detail::polars_lit::floor_div;
using detail::polars_lit::physical_of;
using detail::polars_lit::python_value;
using detail::polars_lit::same_key;
using detail::polars_lit::string_cast;
using detail::polars_lit::LitType;
using detail::polars_lit::lit_type;
using detail::polars_lit::list_of;
using detail::polars_lit::primitive;
using detail::polars_lit::short_name;
using detail::polars_lit::debug_name;
using detail::polars_lit::Literal;
using detail::polars_lit::LitValue;
using detail::polars_lit::micros_of;
using detail::polars_lit::literal_datetime;
using detail::polars_lit::list_literal;
using detail::polars_lit::lit_value;
using detail::polars_lit::polars_float;
using detail::polars_lit::literal_item;
using detail::polars_lit::lit_display;
using detail::polars_lit::lit_supertype;
using detail::polars_lit::cast_lit;
using detail::polars_lit::build_error;
using detail::polars_lit::build_strict;
using detail::polars_lit::nested_null;
using detail::polars_lit::from_values_strict;
using detail::polars_lit::literal_datetime_text;
using detail::polars_lit::literal_text;
using detail::polars_lit::literal_dtype_name;
using detail::polars_lit::lit_kind;
using detail::polars_lit::is_in_dtype_name;
using detail::polars_lit::coerced_is_in;
using detail::polars_lit::literal_has;
using detail::polars_lit::cast_to;
using detail::polars_lit::rust_float;
using detail::polars_lit::polars_frame_float;
using detail::polars_lit::wall_text;
using detail::polars_lit::column_value_text;
using detail::polars_lit::comparable;
using detail::polars_lit::cell_equals;
using detail::polars_lit::operator==;
void enum_is_in(const EventTable& df, std::size_t column, const std::vector<Value>& values, const std::string& node,
                const std::string& plan, const std::vector<std::string>& stack) {
  if (!enum_dtype(dtype_of(df, column))) return;
  detail::polars_lit::enum_is_in(enum_category_list(df, column), values, resolved(node, plan, stack));
}
void check_is_in(const EventTable& df, std::size_t column, const Literal& literal, const std::string& node,
                 const std::string& plan, const std::vector<std::string>& stack) {
  const auto dtype = dtype_of(df, column);
  detail::polars_lit::check_is_in(dtype, enum_dtype(dtype) ? enum_category_list(df, column) : std::vector<std::string>{},
                                  literal, resolved(node, plan, stack));
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
    // Python's messages print the C type name (round CC).
    case Kind::kDatetime: return "datetime.datetime";
    case Kind::kDate: return "datetime.date";
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
  else if (v.kind == Kind::kDict) {
    // A repeated key is one key of the Python dict (round CJ).
    for (const auto& [key, item] : v.entries)
      if (std::none_of(out.begin(), out.end(), [&](const Value& k) { return k.kind == key.kind && k.text == key.text; })) out.push_back(key);
  }
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
EventTable lazy_variant_filter(const EventTable& df, const PolarsFilterParameters& parameters, Predicate&& matches,
                               const std::function<void(const std::vector<std::vector<Cell>>&)>& evaluate = {}) {
  const auto v = variants_of(df, case_key(parameters), activity_key(parameters), kLazySortPlan, kLazyAggPlan);
  if (evaluate) evaluate(v.activities);
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
  // Round CJ: Python builds every item's literal first (a list's or a
  // dict's TypeError), the String casts fail after.
  if (!joined)
    for (const auto& item : admitted_suffixes.items) {
      if (item.kind == Kind::kList || item.kind == Kind::kTuple) (void)list_literal(item.items);
      else if (item.kind == Kind::kDict)
        if (const auto error = detail::polars_lit::dict_literal_error(item)) fail("TypeError", *error);
    }
  std::vector<std::optional<std::string>> suffixes;
  for (const auto& item : admitted_suffixes.items) {
    if (joined) {
      suffixes.emplace_back(python_join(item));
    } else if (item.kind == Kind::kList || item.kind == Kind::kTuple) {
      fail("InvalidOperationError",
           "cannot cast List type (inner: '" + debug_name(list_literal(item.items).type) + "', to: 'String')");
    } else if (item.kind == Kind::kNone) {
      suffixes.emplace_back();
    } else if (item.kind == Kind::kDict) {
      // Round CJ: a dict is a struct literal (its TypeError when a list
      // field fails to build) cast to String: "}" without fields, null with
      // a None field, else as it displays ({1,"x"}).
      if (const auto error = detail::polars_lit::dict_literal_error(item)) fail("TypeError", *error);
      suffixes.push_back(detail::polars_lit::struct_text(detail::polars_lit::StructModel{detail::polars_lit::struct_fields(item), false}));
    } else {
      if (item.kind == Kind::kDatetime || item.kind == Kind::kDate) {
        const auto v = lit_value(item);
        suffixes.emplace_back(cast_lit(v.value, v.type, lit_type(LitType::K::kString))->text);
      } else {
        // Round CH: a float casts as Polars casts a Float64 to String
        // (1e-5 as "0.00001", the form a Float activity's variant holds).
        suffixes.emplace_back(item.kind == Kind::kString  ? item.text
                              : item.kind == Kind::kFloat ? detail::polars_cast_float(item.real)
                                                          : literal_item(item));
      }
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
  // Every pattern is iterated in Python and built into its literal in turn
  // (rounds CB, CC).
  std::vector<Literal> literals;
  for (const auto& item : patterns.items) {
    if (!iterable(item)) fail("TypeError", "'" + py_type(item) + "' object is not iterable");
    literals.push_back(list_literal(elements(item)));
  }
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
  // Round CC: a pattern compares with the activities when its element type
  // and the column's have a supertype; it is then cast to the column dtype
  // when the plan is built (a list pattern on a String column, a str one on
  // a Boolean column cannot be cast), else the comparison fails when the
  // filter runs, naming every pattern (the cast ones as cast).
  // Round CJ: a pattern of structs (dict items) compares with a String
  // column only. On an integer or float column whose type every leaf meets
  // (a list field never does) it is cast to the column dtype, field by
  // field: one plain field casts to that field's value, which compares, any
  // other struct stays a struct of the column dtype and fails with it; on
  // any other column it fails with its own type (probe/cj/s1.py).
  std::vector<std::vector<std::optional<Value>>> lists;
  std::vector<bool> castable(literals.size(), true);
  std::vector<std::optional<std::pair<std::string, std::vector<std::string>>>> struct_shown(literals.size());
  std::vector<bool> struct_fails(literals.size(), false);  // a struct of the column dtype that cannot compare
  std::optional<std::size_t> failed;
  std::optional<std::pair<std::size_t, std::string>> cast_failure;
  const auto leaves_meet = [&](const auto& self, const LitType& t) -> bool {
    for (const auto& f : t.inner) {
      if (f.kind == LitType::K::kStruct ? !self(self, f) : !detail::polars_lit::struct_supertype(dtype, f)) return false;
    }
    return true;
  };
  const auto cast_name = [&](const auto& self, const LitType& t) -> std::string {
    std::string out = "Struct({";
    for (std::size_t i = 0; i < t.inner.size(); ++i)
      out += (i ? ", '" : "'") + t.names[i] + "': " + (t.inner[i].kind == LitType::K::kStruct ? self(self, t.inner[i]) : dtype);
    return out + "})";
  };
  const auto cast_text = [&](const auto& self, const LitType& t, const Value& v) -> std::string {
    if (v.kind == Kind::kNone) return "null";
    std::string out = "{";
    for (std::size_t i = 0; i < t.inner.size() && i < v.entries.size(); ++i) {
      const auto& x = v.entries[i].second;
      out += (i ? "," : "") + (t.inner[i].kind == LitType::K::kStruct ? self(self, t.inner[i], x) : column_value_text(cast_to(x, dtype, nullptr), dtype));
    }
    return out + "}";
  };
  for (std::size_t p = 0; p < literals.size(); ++p) {
    const auto& literal = literals[p];
    auto& list = lists.emplace_back();
    if (act && literal.type.kind == LitType::K::kStruct && dtype != "String") {
      const bool cast = detail::polars_lit::struct_cast_column(dtype) && leaves_meet(leaves_meet, literal.type);
      if (cast) {
        // The plan shows the struct with its leaves cast (Int64: {0.1} as {0}).
        std::vector<std::string> parts;
        for (const auto& v : literal.values) parts.push_back(cast_text(cast_text, literal.type, v));
        struct_shown[p] = std::make_pair(cast_name(cast_name, literal.type), std::move(parts));
      }
      if (cast && literal.type.inner.size() == 1 && literal.type.inner[0].kind != LitType::K::kStruct) {
        for (const auto& v : literal.values)
          list.push_back(v.kind == Kind::kNone || v.entries.empty() ? std::nullopt : cast_to(v.entries[0].second, dtype, nullptr));
        continue;
      }
      castable[p] = false;
      if (cast) struct_fails[p] = true;
      else if (!failed) failed = p;
      continue;
    }
    if (act && !comparable(dtype, literal.type)) {
      castable[p] = false;
      if (!failed) failed = p;
      continue;
    }
    // A list pattern on a String column, a str one on a Boolean column fails
    // its cast when the plan is resolved, after the sort and group_by found
    // their columns (round CJ, probe/cj/s10.py).
    if (act && literal.type.kind == LitType::K::kList) {
      cast_failure.emplace(p, "cannot cast List type (inner: '" + debug_name(literal.type.inner[0]) + "', to: 'String')");
      break;
    }
    if (act && dtype == "Boolean" && literal.type.kind == LitType::K::kString) {
      cast_failure.emplace(p, "casting from Utf8View to Boolean not supported");
      break;
    }
    for (const auto& v : literal.values) list.push_back(cast_to(v, dtype, as_enum ? &categories : nullptr));
  }
  // An empty pattern matches every case, and the plan keeps no comparison.
  if (std::any_of(literals.begin(), literals.end(), [](const Literal& l) { return l.values.empty(); })) failed.reset();
  // A pattern of structs cast to the column dtype fails only when a case's
  // slice holds a present activity (an all-null slice compares).
  const bool any_empty = std::any_of(literals.begin(), literals.end(), [](const Literal& l) { return l.values.empty(); });
  // A pattern as the plan shows it once cast to the column dtype.
  const auto cast_shown = [&](std::size_t p) {
    if (!castable[p] && !struct_shown[p]) return lit_display(list_of(literals[p].type), Value::list(literals[p].values));
    std::vector<std::string> parts;
    if (struct_shown[p]) parts = struct_shown[p]->second;
    else for (const auto& v : lists[p]) parts.push_back(column_value_text(v, dtype));
    std::string shown = "[";
    if (parts.size() <= 3) {
      for (std::size_t i = 0; i < parts.size(); ++i) shown += (i ? ", " : "") + parts[i];
    } else {
      shown += parts[0] + ", " + parts[1] + ", \xe2\x80\xa6 " + parts.back();
    }
    return shown + "]";
  };
  // Round CJ: the plan resolved until the failing cast. The patterns before
  // it show their casts and coerced slices, the failing one its cast, the
  // ones after it as built; an empty pattern is `true`; leading equal
  // patterns fold into one (probe/cj/s11.py).
  const auto cast_failure_plan = [&]() {
    const auto f = cast_failure->first;
    std::vector<std::string> leaves, keys;
    const auto leaf = [&](std::size_t p, bool single) {
      const auto& l = literals[p];
      const auto n = std::to_string(l.values.size());
      const std::string start = !tail ? "dyn int: 0"
                                : single ? "-" + n
                                : p <= f ? "(0 - " + n + ")"
                                         : "(dyn int: 0 - dyn int: " + n + ".cast(Int64))";
      const std::string shown = p < f ? cast_shown(p)
                                      : lit_display(list_of(l.type), Value::list(l.values)) +
                                            (p == f ? ".cast(List(" + std::string(dtype == "Boolean" ? "Boolean" : "String") + "))" : "");
      return "(col(\"activities\").list.slice([" + start + ", dyn int: " + n + "]) == " + shown + ")";
    };
    for (std::size_t p = 0; p < literals.size(); ++p) {
      if (literals[p].values.empty()) {
        leaves.push_back("true");
        keys.push_back("true");
        continue;
      }
      leaves.push_back(leaf(p, false));
      keys.push_back(std::to_string(literals[p].values.size()) + " " + detail::polars_lit::literal_list_key(literals[p]));
    }
    std::string tree = leaves.front();
    bool single = true;
    for (std::size_t p = 1; p < leaves.size(); ++p)
      if (!single || keys[p] != keys[0]) {
        tree = "(" + tree + " | " + leaves[p] + ")";
        single = false;
      }
    if (single) tree = leaf(0, true);
    else if (leaves.front() == "true") tree = tree.substr(1, tree.size() - 2);
    const auto case_glue = case_key(parameters);
    fail("InvalidOperationError",
         cast_failure->second +
             resolved("join",
                      "FILTER " + tree + "\nFROM\n  AGGREGATE[maintain_order: true]\n    [col(" + quote(activity_key(parameters)) +
                          ").implode().alias(\"activities\")] BY [col(" + quote(case_glue) + ")]\n    FROM\n    SORT BY [col(" +
                          quote(case_glue) + "), col(" + quote(kTimestamp) + ")]\n      " + frame_plan(df.columns),
                      {"!filter", "select", "join right", "join"}));
  };
  const auto evaluate = [&](const std::vector<std::vector<Cell>>& variants) {
    if (cast_failure) cast_failure_plan();
    for (std::size_t p = 0; p < literals.size() && !any_empty && (!failed || p < *failed); ++p) {
      if (!struct_fails[p]) continue;
      const auto n = literals[p].values.size();
      const bool present = std::any_of(variants.begin(), variants.end(), [&](const std::vector<Cell>& a) {
        const auto take = std::min(n, a.size());
        const auto begin = tail ? a.size() - take : 0;
        for (auto i = begin; i < begin + take; ++i)
          if (!null_cell(a[i])) return true;
        return false;
      });
      if (present) {
        failed = p;
        break;
      }
    }
    if (!failed) return;
    std::vector<std::string> comparisons;
    for (std::size_t p = 0; p < literals.size(); ++p) {
      const auto n = std::to_string(literals[p].values.size());
      const auto shown = cast_shown(p);
      comparisons.push_back("[(col(\"activities\").list.slice([" + (tail ? "-" + n : "dyn int: 0") + ", dyn int: " + n +
                            "])) == (" + shown + ")]");
    }
    // The plan simplifies `a | a` to `a` (round CC), comparing the patterns
    // as built, before the cast (round CJ: ["ab"] and ["a"] both cast to
    // [null] on a numeric column and stay apart).
    const auto key = [&](std::size_t p) { return comparisons[p].substr(0, comparisons[p].find(")) == (")) + " " + detail::polars_lit::literal_list_key(literals[p]); };
    std::string tree = comparisons.front();
    bool single = true;
    for (std::size_t p = 1; p < comparisons.size(); ++p)
      if (!single || key(p) != key(0)) {
        tree = "[(" + tree + ") | (" + comparisons[p] + ")]";
        single = false;
      }
    fail("SchemaError", "could not evaluate comparison between series 'activities' of dtype: List(" +
                            is_in_dtype_name(dtype) + ") and series 'literal' of dtype: List(" +
                            (struct_shown[*failed] ? struct_shown[*failed]->first : debug_name(literals[*failed].type)) +
                            ")\n\nThis error occurred in the following expression:\n\t" + comparisons[*failed] + "\n" +
                            (tree != comparisons[*failed] ? "while evaluating this larger expression:\n\t" + tree + "\n" : ""));
  };
  return lazy_variant_filter(df, parameters, [&](const std::vector<Cell>& activities) {
    for (std::size_t k = 0; k < lists.size(); ++k) {
      const auto& p = lists[k];
      // A pattern of structs that compared matches no case.
      if (struct_fails[k]) continue;
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
  }, evaluate);
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
  // A list or tuple holds the activities; any other value is one (round CB).
  // Round CJ: is_in takes a dict as the list of its keys (Python's list()).
  std::vector<Value> activities;
  if (activity.kind == Kind::kList || activity.kind == Kind::kTuple) activities = activity.items;
  else if (activity.kind == Kind::kDict) {
    for (const auto& e : activity.entries)
      if (std::none_of(activities.begin(), activities.end(), [&](const Value& k) { return k.text == e.first.text; })) activities.push_back(e.first);
  } else activities = {activity};
  const auto literal = list_literal(activities);
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
  // A value that is no list, tuple or str reaches is_in as itself, which
  // Polars implodes (round CC).
  const bool scalar = activity.kind != Kind::kList && activity.kind != Kind::kTuple && activity.kind != Kind::kString && activity.kind != Kind::kDict;
  std::string shown = literal_text(literal);
  if (scalar) {
    const auto v = literal.values.empty() ? Value::none() : literal.values.front();
    shown = "[" + (v.kind == Kind::kInt ? "dyn int: " + literal_item(v) : v.kind == Kind::kFloat ? "dyn float: " + literal_item(v) : literal_item(v)) + ".implode()]";
  }
  auto enum_stack = act_plan.stack;
  enum_stack.front() = "!with_columns";
  check_is_in(df, act, literal, "select",
             " WITH_COLUMNS:\n [col(" + quote(act_key) + ").is_in(" + shown +
                 ").alias(\"_is_split_activity\")] \n   WITH_COLUMNS:\n   [col(" + quote(case_glue) +
                 ").cum_count().over([col(" + quote(case_glue) + ")]).alias(\"_row_within_case\")] \n    " +
                 frame_plan(df.columns),
             enum_stack);
  // Round CH: a scalar int or float is a dynamic literal that takes a
  // Float32 column's dtype (0.1 matches 0.1f, 16777217 matches 16777216f);
  // a list is a Float64 Series, compared with the column widened.
  const auto split_match = [&](const Cell& a) {
    const auto dtype = dtype_of(df, act);
    if (scalar && dtype == "Float32" && !literal.values.empty() && !null_cell(a)) {
      const auto& v = literal.values.front();
      if (v.kind == Kind::kFloat || v.kind == Kind::kInt) {
        const double x = python_value(a, dtype).real;
        const double y = v.kind == Kind::kInt ? detail::polars_lit::float32_of_int(v)
                                              : static_cast<double>(static_cast<float>(v.real));
        return x == y || (std::isnan(x) && std::isnan(y));
      }
    }
    return literal_has(literal, a, dtype);
  };
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
      running[key] += split_match(a) ? 1 : 0;
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
