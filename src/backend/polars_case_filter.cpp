#include "minera/polars_case_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_column_hint.hpp"
#include "polars_float_text.hpp"
#include "polars_list_literal.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <numeric>
#include <optional>
#include <unordered_map>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {
enum class Operation { kCount, kSize, kPerformance };
[[noreturn]] void fail(std::string type, const std::string& phase, std::string message) {
  throw PolarsCaseFilterError(std::move(type), phase, std::move(message));
}
bool null(const EventAttribute& v) { return v.is_null || is_native_na_attribute(v); }
bool collection(const EventAttribute& v) { return v.container != AttributeContainerKind::kScalar; }
bool boolean(const EventAttribute& v) { return v.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(v.typed_value); }
bool integer(const EventAttribute& v) { return v.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(v.typed_value) || std::holds_alternative<std::uint64_t>(v.typed_value); }
bool real(const EventAttribute& v) { return v.kind == AttributeKind::kFloat || std::holds_alternative<double>(v.typed_value); }
bool date(const EventAttribute& v) { return v.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(v.typed_value); }
bool text(const EventAttribute& v) { return !collection(v) && !null(v) && !boolean(v) && !integer(v) && !real(v) && !date(v); }
TokenInteger exact(const EventAttribute& v) {
  if (const auto* p = std::get_if<std::int64_t>(&v.typed_value)) return TokenInteger(*p);
  if (const auto* p = std::get_if<std::uint64_t>(&v.typed_value)) return TokenInteger(*p);
  if (const auto* p = std::get_if<bool>(&v.typed_value)) return TokenInteger(*p ? 1 : 0);
  if (boolean(v)) return TokenInteger(v.value == "true" || v.value == "1" ? 1 : 0);
  return TokenInteger(v.value);
}
double number(const EventAttribute& v) { return integer(v) || boolean(v) ? static_cast<double>(exact(v)) : native_attribute_number(v); }
bool truth(const EventAttribute& v) {
  if (null(v)) return false;
  if (collection(v)) return !v.children.empty();
  if (integer(v) || boolean(v)) return static_cast<bool>(exact(v));
  if (real(v)) return number(v) != 0;
  return !v.value.empty();
}
std::string python_type(const EventAttribute& v) {
  if (collection(v)) return v.container == AttributeContainerKind::kList ? "list" : v.container == AttributeContainerKind::kTuple ? "tuple" : v.container == AttributeContainerKind::kContainer ? "dict" : "set";
  if (null(v)) return "NoneType";
  if (boolean(v)) return "bool";
  if (integer(v)) return "int";
  if (real(v)) return "float";
  if (date(v)) return "datetime";
  return "str";
}
// The type name CPython's own TypeError and AttributeError messages print.
std::string core_type(const EventAttribute& v) {
  if (!collection(v) && !null(v) && date(v)) {
    const auto* native = std::get_if<NativeDate>(&v.typed_value);
    return (native ? native->value : v.value).size() == 10 ? "datetime.date" : "datetime.datetime";
  }
  return python_type(v);
}
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
std::string real_text(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (converted.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars numeric text");
  return std::string(buffer, converted.ptr);
}
// A datetime bound compares as its epoch microseconds (a naive one read as
// UTC wall-clock time).
std::int64_t date_micros(const EventAttribute& value) {
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const auto parsed = parse_timestamp_microseconds(native ? native->value : value.value);
  if (!parsed) throw std::invalid_argument("Native Polars case bound requires a valid datetime");
  return *parsed;
}
std::string literal(const EventAttribute& value) {
  if (null(value)) return "null";
  if (collection(value)) {
    if (!value.children.empty()) throw std::invalid_argument("Native Polars case bound collection outside observed domain");
    return "[]";
  }
  if (boolean(value)) return truth(value) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  if (real(value)) return real_text(number(value));
  if (date(value)) return std::to_string(date_micros(value));
  return quote(value.value);
}
std::string frame_plan(const EventTable& source) {
  std::string out = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) { if (i) out += ", "; out += quote(source.columns[i]); }
  if (source.columns.size() > 4) out += ", ...";
  return out + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}
std::string contexts(const std::vector<std::string>& nodes) {
  std::string out = "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < nodes.size(); ++i) out += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'\n";
  return out;
}
std::vector<std::string> nodes(Operation op, bool upper = false) {
  if (op == Operation::kCount) return {"group_by", "select", "slice", "select", "join right", "join"};
  // case size filters the upper bound on its own when one is given.
  if (op == Operation::kSize && upper) return {"group_by", "filter", "filter", "select", "join right", "join"};
  if (op == Operation::kSize) return {"group_by", "filter", "select", "join right", "join"};
  return {"group_by", "with_columns", "filter", "select", "join right", "join"};
}
[[noreturn]] void missing(const EventTable& source, const std::string& key, Operation op, const std::string& phase, bool upper = false) {
  std::string message = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) { if (i) message += ", "; message += debug_quote(source.columns[i]); }
  message += ']';
  if (const auto hint = detail::polars_column_hint(key, source.columns)) message += "\n\nDid you mean " + debug_quote(*hint) + '?';
  fail("ColumnNotFoundError", phase, message + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n" + frame_plan(source) + contexts(nodes(op, upper)));
}
// Python builds the bound literals before any Polars step.
void validate_literals(const EventAttribute& low, const EventAttribute& high) {
  for (const auto* value : {&low, &high})
    if (collection(*value) && value->container != AttributeContainerKind::kContainer)
      if (const auto error = detail::polars_list_literal(*value).error) fail("TypeError", "apply", *error);
}
void validate_apply(Operation op, const EventAttribute& bound, const PolarsCaseFilterOptions& options) {
  if (op == Operation::kPerformance && truth(options.business_hours))
    fail("NotImplementedError", "apply", "Business hours calculation is not yet implemented for Polars filtering");
  if (op == Operation::kPerformance && !text(options.timestamp_key) && !collection(options.timestamp_key))
    fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(options.timestamp_key) + "'.");
  if (op == Operation::kCount && !null(bound)) {
    if (text(bound)) fail("TypeError", "apply", "'<' not supported between instances of 'str' and 'int'");
    if (!integer(bound) && !boolean(bound)) fail("TypeError", "apply", "'" + core_type(bound) + "' object cannot be interpreted as an integer");
    if (exact(bound) < TokenInteger(0)) fail("ValueError", "apply", "negative slice lengths (" + exact(bound).decimal() + ") are invalid for LazyFrame");
  }
  if (null(options.case_id_key)) fail("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
  if (!(text(options.case_id_key) || integer(options.case_id_key) || collection(options.case_id_key)))
    throw std::invalid_argument("Native Polars case selector outside observed scalar/list domain");
}
void validate_bound(const EventAttribute& bound, const EventAttribute& other, bool upper, Operation op, const std::string& phase) {
  if (null(bound) || (!text(bound) && !collection(bound))) return;
  const auto column = op == Operation::kSize ? "count" : "case_duration";
  const std::string cmp = upper ? "<=" : ">=";
  std::string error;
  if (text(bound)) error = "cannot compare string with numeric type (" + std::string(op == Operation::kSize ? "u32" : "i64") + ")";
  else error = "could not evaluate '" + cmp + "' comparison between series '" + column + "' of dtype: " + std::string(op == Operation::kSize ? "UInt32" : "Int64") + " and series 'literal' of dtype: List(Null)";
  const auto expression = [&](const EventAttribute& v, const std::string& comparison) {
    // The case duration is Int64: a float bound casts the column.
    if (op == Operation::kPerformance && !null(v) && !collection(v) && !text(v)) {
      if (real(v)) return "[(col(" + quote(column) + ").cast(Float64)) " + comparison + " (" + detail::polars_plan_float(number(v)) + ")]";
      if (boolean(v)) return "[(col(" + quote(column) + ")) " + comparison + " (" + exact(v).decimal() + ")]";
    }
    return "[(col(" + quote(column) + ")) " + comparison + " (" + literal(v) + ")]";
  };
  error += "\n\nThis error occurred in the following expression:\n\t" + expression(bound, cmp) + '\n';
  if (op == Operation::kPerformance) error += "while evaluating this larger expression:\n\t[(" + expression(upper ? other : bound, ">=") + ") & (" + expression(upper ? bound : other, "<=") + ")]\n";
  fail(text(bound) ? "ComputeError" : "SchemaError", phase, std::move(error));
}
bool compare_bound(std::int64_t value, const EventAttribute& bound, bool upper) {
  if (null(bound)) return false;
  if (integer(bound) || boolean(bound)) return upper ? TokenInteger(value) <= exact(bound) : TokenInteger(value) >= exact(bound);
  if (date(bound)) return upper ? value <= date_micros(bound) : value >= date_micros(bound);
  const auto limit = number(bound);
  if (std::isnan(limit)) return upper;
  return upper ? static_cast<double>(value) <= limit : static_cast<double>(value) >= limit;
}
std::string group_key(const std::optional<EventAttribute>& cell, const std::string& dtype) {
  if (!cell || null(*cell) || dtype == "Null") return "n";
  const auto& v = *cell;
  if (dtype == "String" || dtype == "Categorical" || dtype.starts_with("Categorical(") || dtype.starts_with("Enum(")) return "s" + v.value;
  if (dtype == "Float64") {
    const auto n = number(v);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (dtype == "Boolean" || dtype == "Int64" || dtype == "UInt64" || dtype == "UInt32" || dtype == "Int32") return "i" + exact(v).decimal();
  if (polars_datetime::temporal(dtype)) {
    const auto parsed = polars_datetime::ticks(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value, polars_datetime::column_unit(dtype));
    if (!parsed) throw std::invalid_argument("Native Polars case grouping requires valid declared temporal cells");
    return "t" + std::to_string(*parsed);
  }
  throw std::invalid_argument("Unsupported native Polars case dtype: " + dtype);
}
void validate_time_dtype(const std::string& dtype, const std::string& phase) {
  if (polars_datetime::temporal(dtype)) return;
  const std::string expression = "[(col(\"end_time\")) - (col(\"start_time\"))]";
  if (dtype == "String" || dtype == "Boolean") {
    fail("InvalidOperationError", phase, (dtype == "String" ? "sub operation not supported for dtypes `str` and `str`" : "`subtract` operation not supported for dtype `bool`") + std::string("\n\nThis error occurred in the following expression:\n\t") + expression + "\nwhile evaluating this larger expression:\n\t" + expression + ".dt.total_seconds()\n");
  }
  const auto name = dtype == "Int64" ? "i64" : dtype == "UInt64" ? "u64" : dtype == "Float64" ? "f64" : dtype == "Null" ? "null" : "";
  if (!*name) throw std::invalid_argument("Unsupported native Polars performance timestamp dtype: " + dtype);
  fail("SchemaError", phase, "invalid series dtype: expected `Duration`, got `" + std::string(name) + "` for series with name `end_time`\n\nThis error occurred in the following expression:\n\t" + expression + ".dt.total_seconds()\n");
}
EventTable collect(const EventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& options, const std::string& phase) {
  source.validate();
  if (source.index || source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars case filters require declared dtypes and no dataframe index");
  if (collection(options.case_id_key)) {
    if (!options.case_id_key.children.empty()) throw std::invalid_argument("Native Polars case selector lists outside observed empty-list domain");
    fail("InvalidOperationError", phase, "expected join keys/predicates\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nDF []; PROJECT */0 COLUMNS" + contexts({"join"}));
  }
  const bool constant = integer(options.case_id_key);
  const auto position = [&](const std::string& key) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), key);
    if (found == source.columns.end()) missing(source, key, op, phase, !null(high));
    return static_cast<std::size_t>(found - source.columns.begin());
  };
  const auto case_at = constant ? 0 : position(options.case_id_key.value);
  std::size_t time_at = 0;
  if (op == Operation::kPerformance) {
    if (collection(options.timestamp_key)) {
      if (!options.timestamp_key.children.empty()) throw std::invalid_argument("Native Polars timestamp selector lists outside observed empty-list domain");
      const auto& key = options.case_id_key.value;
      fail("ColumnNotFoundError", phase, "unable to find column \"end_time\"; valid columns: [" + debug_quote(key) + "]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nAGGREGATE[maintain_order: false]\n  [] BY [col(" + quote(key) + ")]\n  FROM\n  " + frame_plan(source) + contexts({"with_columns", "filter", "select", "join right", "join"}));
    }
    time_at = position(options.timestamp_key.value);
    validate_time_dtype(source.dtypes[time_at], phase);
  }
  if (op != Operation::kCount) {
    validate_bound(low, high, false, op, phase);
    if (op == Operation::kPerformance || !null(high)) validate_bound(high, low, true, op, phase);
  }
  // Durations count whole seconds of the column unit (round BW).
  const auto time_unit = op == Operation::kPerformance ? polars_datetime::column_unit(source.dtypes[time_at]) : polars_datetime::Unit::kMicroseconds;
  struct Group { std::size_t count = 0; bool absent = false, keep = false; std::optional<std::int64_t> first, last; };
  std::vector<Group> groups; std::unordered_map<std::string, std::size_t> positions; std::vector<std::size_t> row_groups;
  for (const auto& row : source.rows) {
    const auto key = constant ? "constant" : group_key(row[case_at], source.dtypes[case_at]);
    auto [found, inserted] = positions.emplace(key, groups.size());
    if (inserted) { groups.push_back({}); groups.back().absent = !constant && key == "n"; }
    auto& group = groups[found->second]; ++group.count; row_groups.push_back(found->second);
    if (op == Operation::kPerformance && row[time_at] && !null(*row[time_at])) {
      const auto& v = *row[time_at]; const auto parsed = polars_datetime::ticks(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value, time_unit);
      if (!parsed) throw std::invalid_argument("Native Polars performance requires valid declared temporal cells");
      if (!group.first || *parsed < *group.first) group.first = parsed;
      if (!group.last || *parsed > *group.last) group.last = parsed;
    }
  }
  if (op == Operation::kCount) {
    std::vector<std::size_t> order(groups.size()); std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return groups[a].count > groups[b].count; });
    for (std::size_t i = 0; i < order.size() && (null(low) || TokenInteger(i) < exact(low)); ++i) groups[order[i]].keep = true;
  } else for (auto& group : groups) {
    const auto value = op == Operation::kSize ? std::optional<std::int64_t>{static_cast<std::int64_t>(group.count)} : group.first ? std::optional<std::int64_t>{(*group.last - *group.first) / polars_datetime::per_second(time_unit)} : std::nullopt;
    group.keep = value && compare_bound(*value, low, false) && ((op == Operation::kSize && null(high)) || compare_bound(*value, high, true));
  }
  auto result = source; result.rows.clear();
  if (constant) {
    if (std::find(result.columns.begin(), result.columns.end(), "literal") != result.columns.end()) throw std::invalid_argument("Native Polars constant case selector collides with existing literal column");
    result.columns.push_back("literal"); result.dtypes.push_back("Int32");
    if (!result.categorical_dtypes.empty()) result.categorical_dtypes.push_back(std::nullopt);
  }
  for (std::size_t i = 0; i < source.rows.size(); ++i) {
    const auto& group = groups[row_groups[i]];
    if (!group.keep || group.absent) continue;
    auto kept = source.rows[i];
    if (constant) { auto value = options.case_id_key; value.key = "literal"; kept.push_back(std::move(value)); }
    result.rows.push_back(std::move(kept));
  }
  return result;
}
LazyEventTable lazy(const LazyEventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& options) {
  validate_apply(op, low, options);
  validate_literals(low, high);
  auto columns = source.columns(); auto dtypes = source.dtypes(); auto categories = source.categorical_dtypes();
  if (integer(options.case_id_key)) { columns.push_back("literal"); if (!dtypes.empty()) dtypes.push_back("Int32"); if (!categories.empty()) categories.push_back(std::nullopt); }
  return LazyEventTable(std::move(columns), [source, op, low, high, options](const auto& consume) {
    const auto result = collect(source.materialize(), op, low, high, options, "collect");
    for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, std::move(dtypes), source.index(), std::move(categories));
}
EventTable eager(const EventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& options) {
  validate_apply(op, low, options);
  validate_literals(low, high);
  return collect(source, op, low, high, options, "apply");
}
std::vector<PolarsCaseFilterParameter> parameter_value(const EventAttribute& value) {
  if (null(value)) return {};
  if (value.container == AttributeContainerKind::kContainer) {
    std::vector<PolarsCaseFilterParameter> result;
    for (const auto& child : value.children) result.push_back({child.key, child, false});
    return result;
  }
  detail::check_parameter_carrier(value, [](const std::string& type, const std::string& message) { fail(type, "apply", message); });
  return {};
}
}  // namespace

EventTable filter_polars_ncases(const EventTable& s, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return eager(s, Operation::kCount, maximum, {}, options); }
LazyEventTable filter_polars_ncases(const LazyEventTable& s, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return lazy(s, Operation::kCount, maximum, {}, options); }
EventTable filter_polars_case_size(const EventTable& s, const EventAttribute& minimum, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return eager(s, Operation::kSize, minimum, maximum, options); }
LazyEventTable filter_polars_case_size(const LazyEventTable& s, const EventAttribute& minimum, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return lazy(s, Operation::kSize, minimum, maximum, options); }
EventTable filter_polars_case_performance(const EventTable& s, const EventAttribute& minimum, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return eager(s, Operation::kPerformance, minimum, maximum, options); }
LazyEventTable filter_polars_case_performance(const LazyEventTable& s, const EventAttribute& minimum, const EventAttribute& maximum, const PolarsCaseFilterOptions& options) { return lazy(s, Operation::kPerformance, minimum, maximum, options); }
PolarsCaseFilterOptions resolve_polars_case_filter_parameters(const std::vector<PolarsCaseFilterParameter>& parameters) {
  PolarsCaseFilterOptions options;
  for (const bool enum_key : {false, true}) for (const auto& parameter : parameters) {
    if (parameter.enum_key != enum_key) continue;
    if (parameter.key == "pm4py:param:case_id_key") options.case_id_key = parameter.value;
    else if (parameter.key == "pm4py:param:timestamp_key") options.timestamp_key = parameter.value;
    else if (parameter.key == "business_hours") options.business_hours = parameter.value;
    else if (parameter.key == "business_hour_slots") options.business_hour_slots = parameter.value;
  }
  return options;
}
EventTable filter_polars_case_performance_parameters(const EventTable& source, const EventAttribute& low, const EventAttribute& high, const std::vector<PolarsCaseFilterParameter>& parameters) { return filter_polars_case_performance(source, low, high, resolve_polars_case_filter_parameters(parameters)); }
LazyEventTable filter_polars_case_performance_parameters(const LazyEventTable& source, const EventAttribute& low, const EventAttribute& high, const std::vector<PolarsCaseFilterParameter>& parameters) { return filter_polars_case_performance(source, low, high, resolve_polars_case_filter_parameters(parameters)); }
EventTable filter_polars_case_performance_parameter_value(const EventTable& source, const EventAttribute& low, const EventAttribute& high, const EventAttribute& parameters) { return filter_polars_case_performance_parameters(source, low, high, parameter_value(parameters)); }
LazyEventTable filter_polars_case_performance_parameter_value(const LazyEventTable& source, const EventAttribute& low, const EventAttribute& high, const EventAttribute& parameters) { return filter_polars_case_performance_parameters(source, low, high, parameter_value(parameters)); }
EventTable apply_polars_case_filter(const EventTable&, const EventAttribute&) { fail("NotImplementedError", "apply", "apply method not available for case filter"); }
LazyEventTable apply_polars_case_filter(const LazyEventTable&, const EventAttribute&) { fail("NotImplementedError", "apply", "apply method not available for case filter"); }
EventTable auto_filter_polars_cases(const EventTable&, const EventAttribute&) { fail("Exception", "apply", "apply_auto_filter method not available for case filter"); }
LazyEventTable auto_filter_polars_cases(const LazyEventTable&, const EventAttribute&) { fail("Exception", "apply", "apply_auto_filter method not available for case filter"); }
}  // namespace minera
