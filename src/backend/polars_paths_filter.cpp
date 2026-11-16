#include "minera/polars_paths_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_sort.hpp"
#include "polars_list_literal.hpp"
#include "polars_lit_model.hpp"
#include "polars_float_text.hpp"
#include "polars_column_hint.hpp"
#include "polars_key_selection.hpp"
#include "polars_group_keys.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numeric>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

using Cell = std::optional<EventAttribute>;
using EnumRanks = std::unordered_map<std::string, std::size_t>;
enum class Dtype { kString, kEnum, kInteger, kFloat, kBoolean, kNull, kDate };

bool categorical_dtype(const std::string& value) {
  return value == "Categorical" || (value.starts_with("Categorical(") && value.ends_with(')'));
}
bool enum_dtype(const std::string& value) {
  return value.starts_with("Enum(") && value.ends_with(')');
}

[[noreturn]] void fail(std::string type, std::string phase, std::string message) {
  throw PolarsPathsFilterError(std::move(type), std::move(phase), std::move(message));
}

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }
bool collection(const EventAttribute& value) { return value.container != AttributeContainerKind::kScalar; }
bool mapping(const EventAttribute& value) { return value.container == AttributeContainerKind::kContainer; }
bool boolean(const EventAttribute& value) {
  return value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value);
}
bool integer(const EventAttribute& value) {
  return value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
         std::holds_alternative<std::uint64_t>(value.typed_value);
}
bool real(const EventAttribute& value) {
  return value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value);
}
bool date(const EventAttribute& value) {
  return value.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}
bool text(const EventAttribute& value) {
  return !collection(value) && !null(value) && !boolean(value) && !integer(value) && !real(value) && !date(value);
}
TokenInteger exact(const EventAttribute& value) {
  if (const auto* v = std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*v);
  if (const auto* v = std::get_if<bool>(&value.typed_value)) return TokenInteger(*v ? 1 : 0);
  if (boolean(value)) return TokenInteger(value.value == "true" || value.value == "1" ? 1 : 0);
  return TokenInteger(value.value);
}
double number(const EventAttribute& value) {
  return integer(value) || boolean(value) ? static_cast<double>(exact(value)) : native_attribute_number(value);
}
std::string real_text(double value, bool cast_string = false) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (result.ec != std::errc{}) throw std::invalid_argument("Unsupported native Polars real value");
  std::string output(buffer, result.ptr);
  if (cast_string && output.find_first_of(".eE") == std::string::npos) output += ".0";
  return output;
}
std::string quote(const std::string& value) { return '"' + value + '"'; }
std::string debug_quote(const std::string& value) {
  std::string output = "\"";
  for (const auto c : value) {
    switch (c) {
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      case '\0': output += "\\0"; break;
      default: output += c;
    }
  }
  return output + '"';
}
std::string python_type(const EventAttribute& value) {
  if (collection(value)) {
    switch (value.container) {
      case AttributeContainerKind::kList: return "list";
      case AttributeContainerKind::kTuple: return "tuple";
      case AttributeContainerKind::kSet: return "set";
      case AttributeContainerKind::kFrozenSet: return "frozenset";
      default: return "dict";
    }
  }
  if (null(value)) return "NoneType";
  if (boolean(value)) return "bool";
  if (integer(value)) return "int";
  if (real(value)) return "float";
  if (date(value)) return "datetime";
  return "str";
}
// The type name CPython's own TypeError and AttributeError messages print.
std::string core_type(const EventAttribute& value) {
  if (!collection(value) && !null(value) && date(value)) {
    const auto* native = std::get_if<NativeDate>(&value.typed_value);
    return (native ? native->value : value.value).size() == 10 ? "datetime.date" : "datetime.datetime";
  }
  return python_type(value);
}


Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32" || value == "UInt16" || value == "UInt8" || value == "Int32" || value == "Int16" || value == "Int8") return Dtype::kInteger;
  if (value == "Float64" || value == "Float32") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (polars_datetime::temporal(value)) return Dtype::kDate;
  throw std::invalid_argument("Unsupported native Polars paths dtype: " + value);
}
std::string frame_plan(const EventTable& source) {
  std::string output = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) {
    if (i) output += ", ";
    output += quote(source.columns[i]);
  }
  if (source.columns.size() > 4) output += ", ...";
  return output + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}

int compare(const Cell& left, const Cell& right, Dtype kind,
            const EnumRanks* dictionary = nullptr) {
  const auto absent = [kind](const Cell& value) { return !value || null(*value) || kind == Dtype::kNull; };
  if (absent(left) || absent(right)) return absent(left) == absent(right) ? 0 : absent(left) ? -1 : 1;
  if (kind == Dtype::kString) return left->value < right->value ? -1 : right->value < left->value ? 1 : 0;
  if (kind == Dtype::kEnum) {
    if (!dictionary) throw std::invalid_argument("Native Polars between Enum ordering requires its category dictionary");
    const auto position = [&](const std::string& value) {
      const auto found = dictionary->find(value);
      if (found == dictionary->end())
        throw std::invalid_argument("Native Polars between Enum cell is absent from its category dictionary");
      return found->second;
    };
    const auto a = position(left->value), b = position(right->value);
    return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kInteger || kind == Dtype::kBoolean) {
    const auto a = exact(*left), b = exact(*right); return a < b ? -1 : a > b ? 1 : 0;
  }
  if (kind == Dtype::kFloat) {
    const auto a = number(*left), b = number(*right);
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) == std::isnan(b) ? 0 : std::isnan(a) ? 1 : -1;
    return a < b ? -1 : a > b ? 1 : 0;
  }
  const auto instant = [](const EventAttribute& value) {
    const auto* stored = std::get_if<NativeDate>(&value.typed_value);
    const auto parsed = parse_timestamp_exact(stored ? stored->value : value.value);
    if (!parsed) throw std::invalid_argument("Unsupported native Polars paths temporal cell");
    return *parsed;
  };
  const auto a = instant(*left), b = instant(*right);
  if (a.seconds != b.seconds) return a.seconds < b.seconds ? -1 : 1;
  return a.nanoseconds < b.nanoseconds ? -1 : a.nanoseconds > b.nanoseconds ? 1 : 0;
}

std::string string_cast(const EventAttribute& value, const std::string& declared = "") {
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return value.value;
  if (text(value)) return value.value;
  if (boolean(value)) return static_cast<bool>(exact(value)) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  // Round CH: Polars' Float to String cast (1e-5 as "0.00001", a Float32
  // column by its float digits).
  if (real(value)) return detail::polars_cast_float(number(value), declared == "Float32");
  if (date(value)) {
    auto output = std::get_if<NativeDate>(&value.typed_value) ? std::get<NativeDate>(value.typed_value).value : value.value;
    // A Datetime column casts with its unit's fraction digits (round BX).
    if (polars_datetime::datetime(declared)) {
      const auto cast = polars_datetime::cast_text(output, declared);
      if (!cast) throw std::invalid_argument("Native Polars paths temporal cast requires valid declared datetime");
      return *cast;
    }
    if (declared == "Date") return output.substr(0, 10);
    return output;
  }
  if (value.container == AttributeContainerKind::kContainer) {
    std::string output = "{";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) output += ',';
      output += string_cast(value.children[i]);
    }
    return output + '}';
  }
  throw std::invalid_argument("Unsupported native Polars paths string cast");
}

bool truth(const EventAttribute& value) {
  if (null(value)) return false;
  if (collection(value)) return !value.children.empty();
  if (integer(value) || boolean(value)) return static_cast<bool>(exact(value));
  if (real(value)) return number(value) != 0;
  return !value.value.empty();
}
std::vector<EventAttribute> iter(const EventAttribute& value) {
  if (value.container == AttributeContainerKind::kContainer) {
    std::vector<EventAttribute> keys;
    for (const auto& child : value.children) keys.push_back({"", child.key});
    return keys;
  }
  if (collection(value)) return value.children;
  if (text(value)) {
    std::vector<EventAttribute> chars;
    for (std::size_t i = 0; i < value.value.size();) {
      const auto c = static_cast<unsigned char>(value.value[i]);
      const auto count = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
      chars.push_back({"", value.value.substr(i, count)}); i += count;
    }
    return chars;
  }
  fail("TypeError", "apply", "'" + core_type(value) + "' object is not iterable");
}
EventAttribute item(const EventAttribute& value, std::size_t at) {
  if (value.container == AttributeContainerKind::kContainer) fail("KeyError", "apply", std::to_string(at));
  if (text(value) || value.container == AttributeContainerKind::kList || value.container == AttributeContainerKind::kTuple) {
    const auto values = iter(value);
    if (at < values.size()) return values[at];
    fail("IndexError", "apply", (text(value) ? "string" : core_type(value)) + " index out of range");
  }
  fail("TypeError", "apply", "'" + core_type(value) + "' object is not subscriptable");
}
std::string path_text(const EventAttribute& value) {
  // path[0] + "," + path[1]: the first item meets the str before the second
  // is read (round CG: [65536] raises the TypeError, not an IndexError).
  const auto left = item(value, 0);
  // Round CE: a list or tuple refuses the str by its own message.
  if (left.container == AttributeContainerKind::kList || left.container == AttributeContainerKind::kTuple)
    fail("TypeError", "apply", "can only concatenate " + core_type(left) + " (not \"str\") to " + core_type(left));
  if (!text(left)) fail("TypeError", "apply", "unsupported operand type(s) for +: '" + core_type(left) + "' and 'str'");
  const auto right = item(value, 1);
  if (!text(right)) fail("TypeError", "apply", "can only concatenate str (not \"" + core_type(right) + "\") to str");
  return left.value + ',' + right.value;
}
std::vector<std::string> paths(const EventAttribute& value, bool performance) {
  if (performance) return {path_text(value)};
  std::vector<std::string> result;
  for (const auto& candidate : iter(value)) result.push_back(path_text(candidate));
  return result;
}
namespace keys = detail::polars_keys;
// Round CM: pl.col of a list reads every item as a name and pl.col(dict)
// selects the dict's names; keyed() resolves both.
void validate_column(const EventAttribute& value) {
  if (text(value) || mapping(value)) return;
  if (value.container == AttributeContainerKind::kList || value.container == AttributeContainerKind::kTuple) {
    if (!value.children.empty() && !text(value.children.front()))
      fail("TypeError", "apply", "invalid input for `col`\n\nExpected iterable of type `str` or `DataType`, got iterable of type '" + python_type(value.children.front()) + "'.");
    for (const auto& item : value.children) if (!text(item)) fail("TypeError", "apply", null(item) ? "'None' is not an instance of 'str'" : "'" + python_type(item) + "' object is not an instance of 'str'");
    return;
  }
  if (collection(value)) throw std::invalid_argument("Native Polars path selector outside observed empty-list domain");
  fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(value) + "'.");
}
// Round CO: the plain filter reads a list timestamp key holding one item
// that is not a str only in its sort, as a one-value Series literal (case
// 134 of the fresh CK bank).
bool literal_time_list(const EventAttribute& key, bool performance) {
  return !performance && (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple) &&
         key.children.size() == 1 && !text(key.children.front());
}
// Round CU (probe u2): a non-empty list or tuple case key of str names
// sorts as its list literal (one value: any frame but a one-row one fails the
// sort), then is pl.col of its names: a missing name fails first, then the
// attribute, then two distinct names alias "case_next" twice.
bool case_list(const EventAttribute& key) {
  return (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple) && !key.children.empty() &&
         std::all_of(key.children.begin(), key.children.end(), [](const EventAttribute& item) { return keys::plain_text(item); });
}
std::vector<std::string> case_names(const EventAttribute& key) {
  std::vector<std::string> names;
  for (const auto& item : key.children)
    if (std::find(names.begin(), names.end(), item.value) == names.end()) names.push_back(item.value);
  return names;
}
std::vector<std::string> validate_apply(const EventAttribute& value, bool performance, const PolarsPathsFilterOptions& options) {
  auto result = paths(value, performance);
  // Round CN: a list or tuple case or timestamp key is a Series literal
  // (built strictly, as the sort lists them) before any selector is read.
  for (const auto* key : {&options.case_id_key, &options.timestamp_key})
    if (key->container == AttributeContainerKind::kList || key->container == AttributeContainerKind::kTuple)
      if (const auto error = detail::polars_lit::argument_literal_error(*key)) fail("TypeError", "apply", *error);
  // Round CM: a dict case key fails its group_by after the attribute
  // selector is built and before the timestamp one.
  if (mapping(options.case_id_key)) {
    validate_column(options.attribute_key);
    fail("TypeError", "apply", keys::dict_argument_message());
  }
  validate_column(options.case_id_key);
  if (!literal_time_list(options.timestamp_key, performance)) validate_column(options.timestamp_key);
  validate_column(options.attribute_key);
  if (keys::dict_key(options.timestamp_key))
    if (const auto error = detail::polars_lit::dict_literal_error(options.timestamp_key)) fail("TypeError", "apply", *error);
  if (!performance && options.target_attribute_key) validate_column(*options.target_attribute_key);
  // A list bound becomes a Series literal when the performance filter is
  // built; round CJ: a dict bound a struct literal (its own TypeError).
  if (performance)
    for (const auto* bound : {&options.min_performance, &options.max_performance}) {
      if (bound->container == AttributeContainerKind::kList || bound->container == AttributeContainerKind::kTuple) {
        if (const auto error = detail::polars_lit::argument_literal_error(*bound)) fail("TypeError", "apply", *error);
      } else if (mapping(*bound)) {
        if (const auto error = detail::polars_lit::dict_literal_error(*bound)) fail("TypeError", "apply", *error);
      }
    }
  return result;
}
std::string contexts(const std::vector<std::string>& nodes) {
  std::string out = "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < nodes.size(); ++i) out += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'\n";
  return out;
}
// Round CN: a dict timestamp key sorts as its struct literal (one value, so
// the sort fails on any frame but a one-row one).
using SortLiteral = std::optional<std::string>;
std::string sort_time(const PolarsPathsFilterOptions& options, const SortLiteral& literal, const std::string& empty) {
  if (literal) return *literal;
  return collection(options.timestamp_key) ? empty : "col(" + quote(options.timestamp_key.value) + ")";
}
std::string case_sort(const PolarsPathsFilterOptions& options) {
  if (case_list(options.case_id_key)) return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(options.case_id_key));
  return "col(" + quote(options.case_id_key.value) + ")";
}
std::string sort_plan(const EventTable& source, const PolarsPathsFilterOptions& options, const SortLiteral& literal, const std::string& padding = "") {
  return padding + "SORT BY [" + case_sort(options) + ", " + sort_time(options, literal, "[]") + "]\n" + padding + "  " + frame_plan(source);
}
std::string missing_prefix(const std::string& key, const std::vector<std::string>& columns) {
  std::string out = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < columns.size(); ++i) { if (i) out += ", "; out += debug_quote(columns[i]); }
  out += ']';
  if (const auto hint = detail::polars_column_hint(key, columns)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';
  if (keys::by_name(key)) out = keys::by_name_message(key);
  return out + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n";
}
[[noreturn]] void missing(const EventTable& source, const std::string& key, bool after_sort, const PolarsPathsFilterOptions& options, const SortLiteral& literal, const std::string& phase) {
  const auto nodes = after_sort ? std::vector<std::string>{"with_columns", "with_columns", "join left", "join", "select"} : std::vector<std::string>{"sort", "with_columns", "with_columns", "join left", "join", "select"};
  fail("ColumnNotFoundError", phase, missing_prefix(key, source.columns) + (after_sort ? sort_plan(source, options, literal) : frame_plan(source)) + contexts(nodes));
}
[[noreturn]] void empty_selector(const EventTable& source, bool performance, const PolarsPathsFilterOptions& options, const SortLiteral& literal, const std::string& phase) {
  const auto key = quote(options.case_id_key.value), activity = quote(options.attribute_key.value), time = quote(options.timestamp_key.value);
  const auto& target = performance ? options.attribute_key : options.target_attribute_key ? *options.target_attribute_key : options.attribute_key;
  auto columns = source.columns;
  std::string name, plan;
  std::vector<std::string> nodes;
  if (collection(options.case_id_key) && !case_list(options.case_id_key)) {
    name = "@@path"; columns.push_back("next_activity"); if (performance) columns.push_back("next_timestamp");
    plan = " WITH_COLUMNS:\n [] \n   WITH_COLUMNS:\n   [col(" + activity + ").shift([dyn int: -1]).over([]).alias(\"next_activity\")";
    if (performance) plan += ", col(" + time + ").shift([dyn int: -1]).over([]).alias(\"next_timestamp\")";
    plan += "] \n    SORT BY [[], " + sort_time(options, literal, "[]") + "]\n      " + frame_plan(source);
    nodes = {"select", "unique", "join right", "join", "select"};
  } else if (collection(target)) {
    name = "next_activity"; columns.push_back("case_next"); if (performance) columns.push_back("next_timestamp");
    plan = " WITH_COLUMNS:\n [col(" + key + ").alias(\"case_next\")";
    if (performance) plan += ", col(" + time + ").shift([dyn int: -1]).over([col(" + key + ")]).alias(\"next_timestamp\")";
    plan += "] \n" + sort_plan(source, options, literal, "  ");
    nodes = {"with_columns", "join left", "join", "select"};
  } else {
    name = "@@duration"; columns.push_back("case_next"); columns.push_back("next_activity"); columns.push_back("@@path");
    // Round CO: a non-String activity casts to String for the concat, a
    // Null one plainly, any other strictly (probe p21).
    const auto at = std::find(source.columns.begin(), source.columns.end(), options.attribute_key.value);
    const auto dtype = at == source.columns.end() || source.dtypes.size() != source.columns.size() ? std::string("String") : source.dtypes[static_cast<std::size_t>(at - source.columns.begin())];
    const std::string cast = dtype == "String" ? "" : dtype == "Null" ? ".cast(String)" : ".strict_cast(String)";
    plan = " WITH_COLUMNS:\n [when(col(\"case_next\") == col(" + key + ")).then(col(" + activity + ")" + cast + ".str.concat_horizontal([\",\", col(\"next_activity\")" + cast + "])).otherwise(null.cast(String)).alias(\"@@path\")] \n   WITH_COLUMNS:\n   [col(" + key + ").alias(\"case_next\"), col(" + activity + ").shift([dyn int: -1]).over([col(" + key + ")]).alias(\"next_activity\")] \n    SORT BY [" + case_sort(options) + ", " + sort_time(options, literal, "[]") + "]\n      " + frame_plan(source);
    nodes = {"select", "unique", "join right", "join", "select"};
  }
  fail("ColumnNotFoundError", phase, missing_prefix(name, columns) + plan + contexts(nodes));
}
// A datetime bound compares with the duration as epoch microseconds (naive
// read as UTC).
std::int64_t date_micros(const EventAttribute& value) {
  const auto* native = std::get_if<NativeDate>(&value.typed_value);
  const auto parsed = parse_timestamp_microseconds(native ? native->value : value.value);
  if (!parsed) throw std::invalid_argument("Native Polars path bound requires a valid datetime");
  return *parsed;
}
std::string literal(const EventAttribute& value) {
  if (null(value)) return "null";
  // Round CJ: a dict bound as the struct literal (fields cast as the Int64
  // duration meets them).
  if (mapping(value)) return detail::polars_lit::struct_display(detail::polars_lit::struct_model(value, "Int64"));
  // Round CF: a list bound as pl.lit built it.
  if (collection(value)) return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(value));
  if (text(value)) return quote(value.value);
  if (integer(value)) return exact(value).decimal();
  if (boolean(value)) return truth(value) ? "true" : "false";
  return real_text(number(value));
}
// Round CJ: a dict bound fails as the case performance bounds do (the
// Int64 duration cast to the struct fails on a present duration, else the
// struct comparison; a list field leaves the column alone, a SchemaError);
// the failing node is the cast when the cast fails.
void validate_bound(const EventAttribute& bound, bool upper, const PolarsPathsFilterOptions& options, const std::string& path, const std::string& phase, bool present) {
  if (!text(bound) && !collection(bound)) return;
  namespace lit = detail::polars_lit;
  const std::string cmp = upper ? "<=" : ">=";
  std::string type = text(bound) ? "ComputeError" : "SchemaError", node, message;
  if (text(bound)) message = "cannot compare string with numeric type (i64)";
  else if (mapping(bound)) {
    const auto failure = lit::struct_failure("@@duration", "Int64", lit::struct_model(bound, "Int64"), present, upper ? lit::CompareOp::kLe : lit::CompareOp::kGe);
    if (!failure) throw std::invalid_argument("Native Polars path dict bound outside observed domain");
    type = failure->type;
    message = failure->message;
    if (!failure->larger.empty()) node = failure->expression;
  } else {
    message = "could not evaluate '" + cmp + "' comparison between series '@@duration' of dtype: Int64 and series 'literal' of dtype: " + lit::debug_name(lit::list_of(lit::argument_literal(bound).type));
  }
  // The duration is Int64: a float bound casts the column.
  const auto expression = [](const EventAttribute& v, const std::string& comparison) {
    if (mapping(v)) {
      const auto operands = lit::struct_operands("@@duration", "Int64", lit::struct_model(v, "Int64"));
      return "[(" + operands.lhs + ") " + comparison + " (" + operands.rhs + ")]";
    }
    if (!null(v) && !collection(v) && !text(v)) {
      // Round CF: an int past the Int64 range is a UInt64 literal, which
      // meets the Int64 duration at Float64.
      if (const auto u = detail::polars_lit::wide_cell(v))
        return "[(col(\"@@duration\").cast(Float64)) " + comparison + " (" + detail::polars_plan_float(static_cast<double>(*u)) + ")]";
      if (date(v)) return "[(col(\"@@duration\")) " + comparison + " (" + std::to_string(date_micros(v)) + ")]";
      if (boolean(v)) return "[(col(\"@@duration\")) " + comparison + " (" + exact(v).decimal() + ")]";
      if (real(v)) return "[(col(\"@@duration\").cast(Float64)) " + comparison + " (" + detail::polars_plan_float(number(v)) + ")]";
    }
    return "[(col(\"@@duration\")) " + comparison + " (" + literal(v) + ")]";
  };
  // Round CF: equal str or list bounds merge into one == comparison; round
  // CJ: equal dicts too, unless the duration is cast to the struct.
  const auto mergeable = [](const EventAttribute& v) {
    return text(v) || (collection(v) && !mapping(v)) || (mapping(v) && !lit::struct_model(v, "Int64").cast_column);
  };
  const auto key = [](const EventAttribute& v) {
    if (mapping(v)) return "d" + lit::struct_key(lit::struct_model(v, "Int64"));
    return text(v) ? "s" + v.value : "l" + lit::literal_list_key(lit::argument_literal(v));
  };
  const auto& lo = options.min_performance;
  const auto& hi = options.max_performance;
  if (mergeable(lo) && mergeable(hi) && key(lo) == key(hi)) {
    const auto eq = "[(col(\"@@duration\")) == (" + literal(bound) + ")]";
    if (!text(bound)) message.replace(message.find("'" + cmp + "' "), cmp.size() + 3, "");
    message += "\n\nThis error occurred in the following expression:\n\t" + eq + "\nwhile evaluating this larger expression:\n\t[(" + eq +
               ") & ([(col(\"@@path\")) == (" + quote(path) + ")])]\n";
    fail(type, phase, message);
  }
  // A null bound folds away: after the failing bound it drops with the AND,
  // before it the failing bound keeps a null mask and the path test folds.
  const auto path_test = "[(col(\"@@path\")) == (" + quote(path) + ")]";
  std::string larger;
  if (null(options.max_performance)) larger = "[(" + expression(bound, cmp) + ") & (" + path_test + ")]";
  else if (null(options.min_performance)) larger = "[(" + expression(bound, cmp) + ") & (null.repeat([col(\"@@duration\").len()]))]";
  else larger = "[([(" + expression(options.min_performance, ">=") + ") & (" + expression(options.max_performance, "<=") + ")]) & (" + path_test + ")]";
  message += "\n\nThis error occurred in the following expression:\n\t" + (node.empty() ? expression(bound, cmp) : node) + "\nwhile evaluating this larger expression:\n\t" + larger + "\n";
  fail(type, phase, message);
}
void validate_time(const std::string& dtype, const PolarsPathsFilterOptions& options, const std::string& phase) {
  if (polars_datetime::temporal(dtype)) return;
  const auto expression = "[(col(\"next_timestamp\")) - (col(" + quote(options.timestamp_key.value) + "))]";
  const auto condition = phase == "apply" ? "[(col(\"case_next\")) == (col(" + quote(options.case_id_key.value) + "))]" : "col(\"__POLARS_CSER_0x100000003\")";
  const std::string larger = "while evaluating this larger expression:\n\t.when(" + condition + ").then(" + expression + ".dt.total_seconds()).otherwise(null.cast(Int64))\n";
  if (dtype == "String" || dtype == "Boolean") {
    fail("InvalidOperationError", phase, std::string(dtype == "String" ? "sub operation not supported for dtypes `str` and `str`" : "`subtract` operation not supported for dtype `bool`") + "\n\nThis error occurred in the following expression:\n\t" + expression + '\n' + larger);
  }
  // Round CS: every numeric dtype subtracts to itself (fresh CK bank
  // 2026111691 case 29: a Float32 case column selected by a dict key).
  static const std::vector<std::pair<std::string, std::string>> physical = {
      {"Int8", "i8"}, {"Int16", "i16"}, {"Int32", "i32"}, {"Int64", "i64"}, {"UInt8", "u8"}, {"UInt16", "u16"},
      {"UInt32", "u32"}, {"UInt64", "u64"}, {"Float32", "f32"}, {"Float64", "f64"}, {"Null", "null"}};
  const char* name = "";
  for (const auto& [polars, short_name] : physical) if (dtype == polars) name = short_name.c_str();
  if (!*name) throw std::invalid_argument("Unsupported native Polars path timestamp dtype: " + dtype);
  fail("SchemaError", phase, "invalid series dtype: expected `Duration`, got `" + std::string(name) + "` for series with name `next_timestamp`\n\nThis error occurred in the following expression:\n\t" + expression + ".dt.total_seconds()\n" + larger);
}
bool bound_matches(std::int64_t value, const EventAttribute& bound, bool upper) {
  if (null(bound)) return false;
  if (date(bound)) return upper ? value <= date_micros(bound) : value >= date_micros(bound);
  if (integer(bound) || boolean(bound)) return upper ? TokenInteger(value) <= exact(bound) : TokenInteger(value) >= exact(bound);
  const auto limit = number(bound); if (std::isnan(limit)) return upper;
  return upper ? static_cast<double>(value) <= limit : static_cast<double>(value) >= limit;
}
// Round DA (probe d1): a list case key naming one column several times
// resolves every filter and fails at the select of the case ids
// (pl.col of the names, twice the same output name). The plan shows the
// path filter (is_in of the path strings, or == and the two duration
// bounds as separate filters) over both with_columns, the over() listing
// every item of the key.
std::string bound_filter(const EventAttribute& bound, bool upper) {
  namespace lit = detail::polars_lit;
  const std::string column = "col(\"@@duration\")", op = upper ? " <= " : " >= ";
  if (null(bound)) return "null.repeat([col(\"@@duration\").len()])";
  // A dict bound casts the duration to its struct (fields typed as with
  // the Int64 duration), a list bound is its list literal (probe d2).
  if (mapping(bound)) {
    const auto m = lit::struct_model(bound, "Int64");
    return (m.cast_column ? lit::struct_cast_text("@@duration", m) : column) + op + lit::struct_display(m);
  }
  if (collection(bound)) return column + op + lit::literal_list_display(lit::argument_literal(bound));
  if (date(bound)) return column + op + std::to_string(date_micros(bound));
  // An int past the UInt64 range (or below Int64's) is an Int128 literal the
  // simplifier folds against the Int64 duration.
  if (integer(bound) && !boolean(bound)) {
    const TokenInteger x(std::string_view(bound.value));
    const TokenInteger low(std::numeric_limits<std::int64_t>::min()), high(std::string_view("18446744073709551615"));
    if (x < low || high < x) {
      const bool above = high < x;
      return std::string("when(col(\"@@duration\").is_not_null()).then(") + ((upper ? above : !above) ? "true" : "false") + ").otherwise(null)";
    }
  }
  return lit::comparison_plan("@@duration", std::string("Int64"), lit::value_of(bound), upper ? lit::CompareOp::kLe : lit::CompareOp::kGe);
}
[[noreturn]] void repeated_case_name(const EventTable& source, const std::vector<std::string>& paths, bool performance, const PolarsPathsFilterOptions& options, const std::string& name, const std::string& phase) {
  namespace lit = detail::polars_lit;
  const auto key = quote(name), activity = quote(options.attribute_key.value), time = quote(options.timestamp_key.value);
  std::string over;
  for (std::size_t i = 0; i < options.case_id_key.children.size(); ++i) over += std::string(i ? ", " : "") + "col(" + key + ")";
  const auto at = std::find(source.columns.begin(), source.columns.end(), options.attribute_key.value);
  const auto dtype = at == source.columns.end() || source.dtypes.size() != source.columns.size() ? std::string("String") : source.dtypes[static_cast<std::size_t>(at - source.columns.begin())];
  const std::string cast = dtype == "String" ? "" : dtype == "Null" ? ".cast(String)" : ".strict_cast(String)";
  std::string path_column = "when(col(\"case_next\") == col(" + key + ")).then(col(" + activity + ")" + cast + ".str.concat_horizontal([\",\", col(\"next_activity\")" + cast + "])).otherwise(null.cast(String)).alias(\"@@path\")";
  std::string shifted = "col(" + key + ").alias(\"case_next\"), col(" + activity + ").shift([dyn int: -1]).over([" + over + "]).alias(\"next_activity\")";
  if (performance) {
    path_column += ", when(col(\"case_next\") == col(" + key + ")).then(((col(\"next_timestamp\") - col(" + time + "))).dt.total_seconds()).otherwise(null.cast(Int64)).alias(\"@@duration\")";
    shifted += ", col(" + time + ").shift([dyn int: -1]).over([" + over + "]).alias(\"next_timestamp\")";
  }
  const std::string body = " WITH_COLUMNS:\n [" + path_column + "] \n   WITH_COLUMNS:\n   [" + shifted + "] \n    SORT BY [" + case_sort(options) + ", col(" + time + ")]\n      " + frame_plan(source);
  std::vector<std::string> filters;
  if (performance) {
    filters = {"col(\"@@path\") == " + quote(paths.front()), bound_filter(options.min_performance, false), bound_filter(options.max_performance, true)};
  } else if (paths.empty()) {
    filters = {"false.alias(\"@@path\")"};
  } else {
    EventAttribute list{"", "", AttributeKind::kString};
    list.container = AttributeContainerKind::kList;
    for (const auto& path : paths) list.children.push_back(EventAttribute{"", path, AttributeKind::kString});
    filters = {"col(\"@@path\").is_in([" + lit::literal_list_display(lit::argument_literal(list)) + "])"};
  }
  // The filters nest from the last (outermost) to the first, each one's
  // input indented two more spaces.
  std::string plan = body;
  for (const auto& filter : filters) {
    std::string indented;
    for (std::size_t i = 0, start = 0; start <= plan.size(); ++i) {
      const auto end = plan.find('\n', start);
      indented += "  " + plan.substr(start, end == std::string::npos ? std::string::npos : end - start);
      if (end == std::string::npos) break;
      indented += '\n';
      start = end + 1;
    }
    plan = "FILTER " + filter + "\nFROM\n" + indented;
  }
  fail("DuplicateError", phase, detail::polars_group_keys::duplicate_message(name, "projections") + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n" + plan + contexts({"select", "unique", "join right", "join", "select"}));
}
EventTable collect(const EventTable& source, const std::vector<std::string>& paths, bool performance, const PolarsPathsFilterOptions& options, const SortLiteral& literal, const std::string& phase) {
  source.validate();
  if (source.index || source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars paths require declared dtypes and no dataframe index");
  const auto position = [&](const std::string& name, bool sorted) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), name);
    if (found == source.columns.end()) missing(source, name, sorted, options, literal, phase);
    return static_cast<std::size_t>(found - source.columns.begin());
  };
  const bool listed = case_list(options.case_id_key);
  if (collection(options.case_id_key) && !listed) empty_selector(source, performance, options, literal, phase);
  std::size_t case_at = listed ? 0 : position(options.case_id_key.value, false);
  std::optional<std::size_t> time_at;
  if (!literal && !collection(options.timestamp_key)) time_at = position(options.timestamp_key.value, false);
  // The list case key's names resolve after the sort, before the attribute.
  if (listed && keys::by_name(options.case_id_key.value)) missing(source, options.case_id_key.value, true, options, literal, phase);
  // The plain filter reads a dict timestamp only in its sort; the
  // performance one selects it again (pl.col of the dict) for the next
  // timestamp, beside the case and before the next activity.
  if (literal && performance && !collection(options.timestamp_key)) time_at = position(options.timestamp_key.value, true);
  if (literal && !performance) time_at = case_at;
  const auto& target = performance ? options.attribute_key : options.target_attribute_key ? *options.target_attribute_key : options.attribute_key;
  if (collection(target) || collection(options.attribute_key)) empty_selector(source, performance, options, literal, phase);
  const auto target_at = position(target.value, true), activity_at = position(options.attribute_key.value, true);
  if (listed) {
    const auto names = case_names(options.case_id_key);
    if (names.size() > 1)
      fail("ComputeError", phase,
           "the name 'case_next' passed to `LazyFrame.with_columns` is duplicate\n\nIt's possible that multiple expressions are returning the same "
           "default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\n"
           "Resolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n" +
               sort_plan(source, options, literal) + contexts({"with_columns", "with_columns", "join left", "join", "select"}));
    // A repeated name selects its column once in the with_columns and twice
    // in the final select: round DA models the DuplicateError and its plan
    // (probe d1; the CU leftover), on str timestamp keys.
    if (names.size() != options.case_id_key.children.size()) {
      if (literal) throw std::invalid_argument("Native Polars path selector outside observed domain: a repeated name beside a literal sort key");
      repeated_case_name(source, paths, performance, options, names.front(), phase);
    }
    case_at = position(names.front(), true);
    if (literal && !performance) time_at = case_at;
  }
  if (!time_at) empty_selector(source, performance, options, literal, phase);
  if (literal || listed) {
    // No admitted path prunes the sort: the frame as it is or no rows.
    if (!performance && paths.empty()) { if (truth(options.positive)) { auto none = source; none.rows.clear(); return none; } return source; }
    if (source.rows.size() != 1)
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(source.rows.size()) + " and Series length: 1");
  }
  const auto case_kind = dtype(source.dtypes[case_at]), time_kind = dtype(source.dtypes[*time_at]);
  const auto enum_dictionary = [&](std::size_t at, Dtype kind) -> std::optional<EnumRanks> {
    if (kind != Dtype::kEnum) return std::nullopt;
    if (source.categorical_dtypes.empty() || !source.categorical_dtypes[at]) throw std::invalid_argument("Native Polars paths require an Enum dictionary for selected sort column: " + source.columns[at]);
    EnumRanks ranks;
    const auto& categories = source.categorical_dtypes[at]->categories; ranks.reserve(categories.size());
    for (std::size_t i = 0; i < categories.size(); ++i) ranks.emplace(categories[i].value, i);
    for (const auto& row : source.rows) if (row[at] && !null(*row[at]) && !ranks.contains(row[at]->value)) throw std::invalid_argument("Native Polars paths Enum cell absent from selected dictionary: " + source.columns[at]);
    return ranks;
  };
  const auto case_ranks = enum_dictionary(case_at, case_kind);
  const auto time_ranks = *time_at == case_at ? std::optional<EnumRanks>{} : enum_dictionary(*time_at, time_kind);
  const auto* case_dictionary = case_ranks ? &*case_ranks : nullptr;
  const auto* time_dictionary = *time_at == case_at ? case_dictionary : time_ranks ? &*time_ranks : nullptr;
  // Round CG: str bounds with the lower above the upper (byte order) fold the
  // duration interval to an empty one: nothing raises and no path passes.
  const bool folded = performance && text(options.min_performance) && text(options.max_performance) && options.max_performance.value < options.min_performance.value;
  // Whether the duration has a present cell: two rows of one case with
  // timestamps (a dict bound's cast fails only then).
  const auto duration_present = [&] {
    if (!mapping(options.min_performance) && !mapping(options.max_performance)) return false;
    std::vector<std::size_t> timed;
    for (std::size_t i = 0; i < source.rows.size(); ++i) {
      const auto& row = source.rows[i];
      if (row[case_at] && !null(*row[case_at]) && row[*time_at] && !null(*row[*time_at])) timed.push_back(i);
    }
    if (case_kind == Dtype::kNull) return false;
    std::sort(timed.begin(), timed.end(), [&](auto a, auto b) { return compare(source.rows[a][case_at], source.rows[b][case_at], case_kind, case_dictionary) < 0; });
    for (std::size_t i = 1; i < timed.size(); ++i)
      if (compare(source.rows[timed[i - 1]][case_at], source.rows[timed[i]][case_at], case_kind, case_dictionary) == 0) return true;
    return false;
  };
  if (performance) {
    validate_time(source.dtypes[*time_at], options, phase);
    if (!folded) {
      const bool present = duration_present();
      validate_bound(options.min_performance, false, options, paths.front(), phase, present);
      validate_bound(options.max_performance, true, options, paths.front(), phase, present);
    }
  }
  // No admitted path keeps no case: a negative filter returns the frame as it is.
  if (!performance && paths.empty() && !truth(options.positive)) return source;
  std::vector<std::size_t> order(source.rows.size()); std::iota(order.begin(), order.end(), 0);
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  polars_sort::sort_by_keys(order, [&](auto a, auto b) {
    const auto by_case = compare(source.rows[a][case_at], source.rows[b][case_at], case_kind, case_dictionary);
    return by_case ? by_case < 0 : compare(source.rows[a][*time_at], source.rows[b][*time_at], time_kind, time_dictionary) < 0;
  }, source.dtypes[case_at], source.dtypes[*time_at]);
  auto result = source; result.rows.clear();
  for (std::size_t begin = 0; begin < order.size();) {
    auto end = begin + 1;
    while (end < order.size() && compare(source.rows[order[begin]][case_at], source.rows[order[end]][case_at], case_kind, case_dictionary) == 0) ++end;
    bool matched = false;
    const auto& id = source.rows[order[begin]][case_at];
    if (id && !null(*id) && case_kind != Dtype::kNull) for (auto i = begin; i + 1 < end && !matched; ++i) {
      const auto& row = source.rows[order[i]]; const auto& next = source.rows[order[i + 1]];
      if (!row[activity_at] || !next[target_at] || null(*row[activity_at]) || null(*next[target_at])) continue;
      const auto label = string_cast(*row[activity_at], source.dtypes[activity_at]) + ',' + string_cast(*next[target_at], source.dtypes[target_at]);
      if (std::find(paths.begin(), paths.end(), label) == paths.end()) continue;
      if (performance) {
        if (!row[*time_at] || !next[*time_at] || null(*row[*time_at]) || null(*next[*time_at])) continue;
        // Durations count whole seconds of the column unit (round BW).
        const auto unit = polars_datetime::column_unit(source.dtypes[*time_at]);
        const auto instant = [unit](const EventAttribute& value) { const auto* stored = std::get_if<NativeDate>(&value.typed_value); const auto parsed = polars_datetime::ticks(stored ? stored->value : value.value, unit); if (!parsed) throw std::invalid_argument("Native Polars path performance requires valid temporal cells"); return *parsed; };
        const auto duration = (instant(*next[*time_at]) - instant(*row[*time_at])) / polars_datetime::per_second(unit);
        if (folded || !bound_matches(duration, options.min_performance, false) || !bound_matches(duration, options.max_performance, true)) continue;
      }
      matched = true;
    }
    if (matched == truth(options.positive)) for (auto i = begin; i < end; ++i) result.rows.push_back(source.rows[order[i]]);
    begin = end;
  }
  return result;
}
// The column keys as pl.col reads them (a refused multi-name selection stays
// a list, outside the modelled domain).
PolarsPathsFilterOptions keyed(const PolarsPathsFilterOptions& raw, const std::vector<std::string>& columns, bool performance) {
  auto options = raw;
  for (auto* key : {&options.case_id_key, &options.timestamp_key, &options.attribute_key}) {
    if (key == &options.timestamp_key && literal_time_list(raw.timestamp_key, performance)) continue;
    // Round CU: a list case key stays a list (its sort literal) and carries
    // the name pl.col resolves (a first missing name behind the marker).
    if (key == &options.case_id_key && case_list(raw.case_id_key)) {
      const auto resolved = keys::col_key(raw.case_id_key, columns);
      key->value = collection(resolved) ? std::string() : resolved.value;
      continue;
    }
    *key = keys::col_key(*key, columns);
    if (collection(*key) && !key->children.empty()) throw std::invalid_argument("Native Polars path selector outside observed empty-list domain");
  }
  if (options.target_attribute_key) {
    *options.target_attribute_key = keys::col_key(*options.target_attribute_key, columns);
    if (collection(*options.target_attribute_key) && !options.target_attribute_key->children.empty())
      throw std::invalid_argument("Native Polars path selector outside observed empty-list domain");
  }
  return options;
}
SortLiteral sort_literal(const PolarsPathsFilterOptions& raw, bool performance) {
  if (literal_time_list(raw.timestamp_key, performance)) return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(raw.timestamp_key));
  if (!keys::dict_key(raw.timestamp_key)) return std::nullopt;
  return detail::polars_lit::dict_literal_display(raw.timestamp_key);
}
EventTable eager(const EventTable& source, const EventAttribute& path, bool performance, const PolarsPathsFilterOptions& raw) {
  auto parsed = validate_apply(path, performance, raw);
  return collect(source, parsed, performance, keyed(raw, source.columns, performance), sort_literal(raw, performance), "apply");
}
LazyEventTable lazy(const LazyEventTable& source, const EventAttribute& path, bool performance, const PolarsPathsFilterOptions& raw) {
  auto parsed = validate_apply(path, performance, raw);
  const auto options = keyed(raw, source.columns(), performance);
  const auto literal = sort_literal(raw, performance);
  return LazyEventTable(source.columns(), [source, parsed, performance, options, literal](const auto& consume) {
    const auto result = collect(source.materialize(), parsed, performance, options, literal, "collect"); for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, source.dtypes(), source.index(), source.categorical_dtypes());
}
std::vector<PolarsPathsFilterParameter> parameters(const EventAttribute& value) {
  if (null(value)) return {};
  if (value.container == AttributeContainerKind::kContainer) { std::vector<PolarsPathsFilterParameter> result; for (const auto& child : value.children) result.push_back({child.key, child, false}); return result; }
  detail::check_parameter_carrier(value, [](const std::string& type, const std::string& message) { fail(type, "apply", message); });
  return {};
}
EventAttribute dictionary_keys(const PolarsPathsFilterDictionary& dictionary) {
  EventAttribute result; result.container = AttributeContainerKind::kList;
  for (const auto& pair : dictionary.items) result.children.push_back(pair.first);
  return result;
}
}  // namespace

EventTable filter_polars_paths(const EventTable& s, const EventAttribute& p, const PolarsPathsFilterOptions& o) { return eager(s, p, false, o); }
LazyEventTable filter_polars_paths(const LazyEventTable& s, const EventAttribute& p, const PolarsPathsFilterOptions& o) { return lazy(s, p, false, o); }
EventTable filter_polars_path_performance(const EventTable& s, const EventAttribute& p, const PolarsPathsFilterOptions& o) { return eager(s, p, true, o); }
LazyEventTable filter_polars_path_performance(const LazyEventTable& s, const EventAttribute& p, const PolarsPathsFilterOptions& o) { return lazy(s, p, true, o); }
PolarsPathsFilterOptions resolve_polars_paths_filter_parameters(const std::vector<PolarsPathsFilterParameter>& values) {
  PolarsPathsFilterOptions options;
  for (const bool enum_key : {false, true}) for (const auto& value : values) {
    if (value.enum_key != enum_key) continue;
    if (value.key == "pm4py:param:case_id_key") options.case_id_key = value.value;
    else if (value.key == "pm4py:param:attribute_key") options.attribute_key = value.value;
    else if (value.key == "pm4py:param:timestamp_key") options.timestamp_key = value.value;
    else if (value.key == "target_attribute_key") options.target_attribute_key = value.value;
    else if (value.key == "positive") options.positive = value.value;
    else if (value.key == "min_performance") options.min_performance = value.value;
    else if (value.key == "max_performance") options.max_performance = value.value;
  }
  return options;
}
EventTable filter_polars_paths_parameters(const EventTable& s, const EventAttribute& p, const std::vector<PolarsPathsFilterParameter>& v) { return filter_polars_paths(s, p, resolve_polars_paths_filter_parameters(v)); }
LazyEventTable filter_polars_paths_parameters(const LazyEventTable& s, const EventAttribute& p, const std::vector<PolarsPathsFilterParameter>& v) { return filter_polars_paths(s, p, resolve_polars_paths_filter_parameters(v)); }
EventTable filter_polars_path_performance_parameters(const EventTable& s, const EventAttribute& p, const std::vector<PolarsPathsFilterParameter>& v) { return filter_polars_path_performance(s, p, resolve_polars_paths_filter_parameters(v)); }
LazyEventTable filter_polars_path_performance_parameters(const LazyEventTable& s, const EventAttribute& p, const std::vector<PolarsPathsFilterParameter>& v) { return filter_polars_path_performance(s, p, resolve_polars_paths_filter_parameters(v)); }
EventTable filter_polars_paths_parameter_value(const EventTable& s, const EventAttribute& p, const EventAttribute& v) { return filter_polars_paths_parameters(s, p, parameters(v)); }
LazyEventTable filter_polars_paths_parameter_value(const LazyEventTable& s, const EventAttribute& p, const EventAttribute& v) { return filter_polars_paths_parameters(s, p, parameters(v)); }
EventTable filter_polars_path_performance_parameter_value(const EventTable& s, const EventAttribute& p, const EventAttribute& v) { return filter_polars_path_performance_parameters(s, p, parameters(v)); }
LazyEventTable filter_polars_path_performance_parameter_value(const LazyEventTable& s, const EventAttribute& p, const EventAttribute& v) { return filter_polars_path_performance_parameters(s, p, parameters(v)); }
EventTable filter_polars_paths(const EventTable& s, const PolarsPathsFilterDictionary& p, const PolarsPathsFilterOptions& o) { return filter_polars_paths(s, dictionary_keys(p), o); }
LazyEventTable filter_polars_paths(const LazyEventTable& s, const PolarsPathsFilterDictionary& p, const PolarsPathsFilterOptions& o) { return filter_polars_paths(s, dictionary_keys(p), o); }
}  // namespace minera
