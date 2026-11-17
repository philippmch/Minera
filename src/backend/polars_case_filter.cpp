#include "minera/polars_case_filter.hpp"
#include "polars_eager_diagnostic.hpp"
#include "polars_datetime.hpp"
#include "polars_column_hint.hpp"
#include "polars_key_selection.hpp"
#include "polars_group_keys.hpp"
#include "polars_float_text.hpp"
#include "polars_list_literal.hpp"
#include "polars_lit_model.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <numeric>
#include <map>
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
EventAttribute none() { EventAttribute v; v.is_null = true; return v; }
bool collection(const EventAttribute& v) { return v.container != AttributeContainerKind::kScalar; }
bool boolean(const EventAttribute& v) { return v.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(v.typed_value); }
bool integer(const EventAttribute& v) { return v.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(v.typed_value) || std::holds_alternative<std::uint64_t>(v.typed_value); }
bool real(const EventAttribute& v) { return v.kind == AttributeKind::kFloat || std::holds_alternative<double>(v.typed_value); }
bool date(const EventAttribute& v) { return v.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(v.typed_value); }
bool text(const EventAttribute& v) { return !collection(v) && !null(v) && !boolean(v) && !integer(v) && !real(v) && !date(v); }
bool mapping(const EventAttribute& v) { return v.container == AttributeContainerKind::kContainer; }
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
// PyO3's downcast errors print the type's short __name__ ('date', 'datetime').
std::string pyo3_type(const EventAttribute& v) {
  const std::string name = core_type(v);
  return name.rfind("datetime.", 0) == 0 ? name.substr(9) : name;
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
namespace keys = detail::polars_keys;
[[noreturn]] void missing(const EventTable& source, const std::string& key, Operation op, const std::string& phase, bool upper = false) {
  std::string message = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) { if (i) message += ", "; message += debug_quote(source.columns[i]); }
  message += ']';
  if (const auto hint = detail::polars_column_hint(key, source.columns)) message += "\n\nDid you mean " + debug_quote(*hint) + '?';
  if (keys::by_name(key)) message = keys::by_name_message(key);
  fail("ColumnNotFoundError", phase, message + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n" + frame_plan(source) + contexts(nodes(op, upper)));
}
// Python builds the bound literals before any Polars step.
// Round CJ: a dict bound converts strictly too (a list field that fails to
// build fails it with the dict TypeError).
void validate_literals(Operation op, const EventAttribute& low, const EventAttribute& high) {
  // Round DG: the case count goes to head(n), not to a literal.
  if (op == Operation::kCount) return;
  for (const auto* value : {&low, &high}) {
    // Round DF: pl.lit refuses an int past the Int128 range (probe df/i1).
    if (detail::polars_lit::scalar_too_large(*value)) fail("InvalidOperationError", "apply", detail::polars_lit::kIntegerTooLarge);
    // Round DB: pl.lit refuses a set bound.
    if (detail::polars_lit::set_argument(*value)) fail("TypeError", "apply", detail::polars_lit::kSetLiteralError);
    if (mapping(*value)) {
      if (const auto error = detail::polars_lit::dict_literal_error(*value)) fail("TypeError", "apply", *error);
    } else if (collection(*value)) {
      // Round DG: a list bound holds ints in the Int128/UInt128 ranges; one
      // past them fails the list's construction (OverflowError).
      try {
        (void)detail::polars_lit::argument_literal(*value, true);
      } catch (const PolarsSequenceFilterError& error) {
        fail(error.type, "apply", error.message);
      }
    }
  }
}
void validate_apply(Operation op, const EventAttribute& bound, const PolarsCaseFilterOptions& options, bool eager = false) {
  if (op == Operation::kPerformance && truth(options.business_hours))
    fail("NotImplementedError", "apply", "Business hours calculation is not yet implemented for Polars filtering");
  // Round CO: group_by refuses a dict case key before anything else reads
  // the bounds or the timestamp key (probe p15).
  if (keys::dict_key(options.case_id_key)) fail("TypeError", "apply", keys::dict_argument_message());
  if (op == Operation::kPerformance && !text(options.timestamp_key) && !collection(options.timestamp_key))
    fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(options.timestamp_key) + "'.");
  // Round CO: pl.col of a list timestamp key fails before the bounds build
  // their literals (case 98 of the CL bank).
  // Round DQ: pl.col reads a dict with a key that is not a str as the list
  // of its keys (probe dq/tq1).
  const auto time = keys::typed_dict_list(options.timestamp_key);
  // Round DB: pl.col reads a set's items like a list's.
  if (op == Operation::kPerformance && (time.container == AttributeContainerKind::kList || time.container == AttributeContainerKind::kTuple ||
                                        detail::polars_lit::set_argument(time))) {
    if (!time.children.empty() && !text(time.children.front()))
      fail("TypeError", "apply", "invalid input for `col`\n\nExpected iterable of type `str` or `DataType`, got iterable of type '" + python_type(time.children.front()) + "'.");
    for (const auto& item : time.children) if (!text(item)) fail("TypeError", "apply", null(item) ? "'None' is not an instance of 'str'" : "'" + pyo3_type(item) + "' object is not an instance of 'str'");
  }
  // Round DL: an eager frame calls DataFrame.head(n) after the group_by
  // (eager_head).
  if (op == Operation::kCount && !null(bound) && !eager) {
    // Round DB: head(n) compares a truthy str or container with 0 first; an
    // empty one fails the integer conversion.
    if ((text(bound) || collection(bound)) && truth(bound)) fail("TypeError", "apply", "'<' not supported between instances of '" + core_type(bound) + "' and 'int'");
    if (!integer(bound) && !boolean(bound)) fail("TypeError", "apply", "'" + core_type(bound) + "' object cannot be interpreted as an integer");
    if (exact(bound) < TokenInteger(0)) fail("ValueError", "apply", "negative slice lengths (" + exact(bound).decimal() + ") are invalid for LazyFrame");
    // Round DG: head(n) converts n to a C long, then to Polars' u32 index
    // (probe dg/h1).
    if (TokenInteger(std::numeric_limits<std::int64_t>::max()) < exact(bound)) fail("OverflowError", "apply", "Python int too large to convert to C long");
    if (TokenInteger(std::numeric_limits<std::uint32_t>::max()) < exact(bound)) fail("OverflowError", "apply", "number too large to fit in target type");
  }
  if (null(options.case_id_key)) fail("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
  if (!(text(options.case_id_key) || collection(options.case_id_key) || detail::polars_group_keys::modelled(options.case_id_key)))
    throw std::invalid_argument("Native Polars case selector outside observed scalar/list domain");
}
// Round CG: list bounds name their pl.lit type (List(Int64), List(String),
// List(UInt64) past the Int64 range) and print as the literal model shows
// them; the other bound prints as Polars coerces it against the UInt32
// count or the Int64 duration (casts, folded constants); filter_on_case_size
// merges equal bounds and reports the larger expression as the performance
// filters do; a None upper bound leaves the failing lower bound alone (no
// larger expression), a None lower bound folds to a null column printed
// after the failing upper bound (probed against Polars 1.44.1).
bool list_bound(const EventAttribute& v) { return collection(v) && v.container != AttributeContainerKind::kContainer; }
// Round CJ: a dict bound is a struct literal. The UInt32 count or the Int64
// duration is cast to a struct of each field's supertype with it (a list
// field leaves the column alone); the bound then always fails: the cast of a
// present cell ("must specify one field in the struct" past one field,
// "Cannot cast from struct to other types" for a nested one), else the
// struct comparison itself, or the SchemaError of the column left alone.
// The failing node is the cast when the cast fails, the larger expression
// the whole predicate as for the other bounds (probe/cj/c1.py-c12.py; with
// four threads the narrow duration frame may race to the comparison error,
// which the bank test accepts).
detail::polars_lit::StructModel bound_struct(const EventAttribute& v, Operation op) {
  return detail::polars_lit::struct_model(v, op == Operation::kSize ? "UInt32" : "Int64");
}
std::string bound_literal(const EventAttribute& v, Operation op) {
  if (mapping(v)) return detail::polars_lit::struct_display(bound_struct(v, op));
  if (list_bound(v)) return detail::polars_lit::literal_list_display(detail::polars_lit::argument_literal(v, true));
  return literal(v);
}
void validate_bound(const EventAttribute& bound, const EventAttribute& other, bool upper, Operation op, const std::string& phase, bool present, bool alone = false) {
  if (null(bound) || (!text(bound) && !collection(bound))) return;
  namespace lit = detail::polars_lit;
  const auto column = op == Operation::kSize ? "count" : "case_duration";
  const std::string dtype = op == Operation::kSize ? "UInt32" : "Int64";
  const std::string cmp = upper ? "<=" : ">=";
  std::string error, type = text(bound) ? "ComputeError" : "SchemaError", node;
  if (text(bound)) error = "cannot compare string with numeric type (" + std::string(op == Operation::kSize ? "u32" : "i64") + ")";
  else if (mapping(bound)) {
    const auto failure = lit::struct_failure(column, dtype, bound_struct(bound, op), present, upper ? lit::CompareOp::kLe : lit::CompareOp::kGe);
    if (!failure) throw std::invalid_argument("Native Polars case dict bound outside observed domain");
    type = failure->type;
    error = failure->message;
    if (!failure->larger.empty()) node = failure->expression;
  } else {
    const auto type = list_bound(bound) ? lit::debug_name(lit::list_of(lit::argument_literal(bound, true).type)) : std::string("List(Null)");
    error = "could not evaluate '" + cmp + "' comparison between series '" + column + "' of dtype: " + dtype + " and series 'literal' of dtype: " + type;
  }
  const auto expression = [&](const EventAttribute& v, bool at_most) -> std::string {
    const auto comparison = at_most ? lit::CompareOp::kLe : lit::CompareOp::kGe;
    if (mapping(v)) {
      const auto operands = lit::struct_operands(column, dtype, bound_struct(v, op));
      return "[(" + operands.lhs + ") " + lit::op_text(comparison) + " (" + operands.rhs + ")]";
    }
    if (text(v) || collection(v)) return "[(col(" + quote(column) + ")) " + lit::op_text(comparison) + " (" + bound_literal(v, op) + ")]";
    // Round DG: an Int128 bound folds against the count or duration.
    const auto operands = lit::coerced_operands(column, dtype, lit::value_of(v, true), comparison);
    if (!operands.whole.empty()) return operands.whole.starts_with("when(") ? "." + operands.whole : operands.whole;
    return "[(" + operands.lhs + ") " + lit::op_text(comparison) + " (" + operands.rhs + ")]";
  };
  // Round CF: equal str or list bounds merge into one ==, the whole
  // predicate (seed 4263 case 400); round CG: on the case size too. Round
  // CJ: equal dicts too, unless the column is cast to the struct.
  const auto mergeable = [&](const EventAttribute& v) {
    return text(v) || list_bound(v) || (mapping(v) && !bound_struct(v, op).cast_column);
  };
  const auto key = [&](const EventAttribute& v) {
    if (mapping(v)) return "d" + lit::struct_key(bound_struct(v, op));
    return text(v) ? "s" + v.value : "l" + lit::literal_list_key(lit::argument_literal(v, true));
  };
  if (!alone && mergeable(other) && mergeable(bound) && key(bound) == key(other)) {
    if (!text(bound)) error.replace(error.find("'" + cmp + "' "), cmp.size() + 3, "");
    error += "\n\nThis error occurred in the following expression:\n\t[(col(" + quote(column) + ")) == (" + bound_literal(bound, op) + ")]\n";
    fail(type, phase, std::move(error));
  }
  const auto failing = node.empty() ? expression(bound, upper) : node;
  std::string root = expression(bound, upper);
  const std::string null_column = "null.repeat([col(" + quote(column) + ").len()])";
  if (alone) {
  } else if (null(other) && phase == "apply") {
    // Round DL: on an eager frame a None bound stays in place as a null
    // column (probe dl/c4).
    root = upper ? "[(" + null_column + ") & (" + expression(bound, true) + ")]" : "[(" + expression(bound, false) + ") & (" + null_column + ")]";
  } else if (null(other)) {
    if (upper) root = "[(" + expression(bound, true) + ") & (null.repeat([col(" + quote(column) + ").len()]))]";
  } else {
    root = "[(" + expression(upper ? other : bound, false) + ") & (" + expression(upper ? bound : other, true) + ")]";
  }
  error += "\n\nThis error occurred in the following expression:\n\t" + failing + '\n';
  if (root != failing) error += "while evaluating this larger expression:\n\t" + root + "\n";
  fail(type, phase, std::move(error));
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
  if (dtype == "Float64" || dtype == "Float32") {
    const auto n = number(v);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (dtype == "Boolean" || dtype == "Int64" || dtype == "UInt64" || dtype == "UInt32" || dtype == "Int32" || dtype == "UInt16" || dtype == "Int16" || dtype == "UInt8" || dtype == "Int8") return "i" + exact(v).decimal();
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
  // Round CO: Categorical and Enum columns refuse the subtraction as a
  // Boolean one does; every integer and float width subtracts and fails the
  // duration read (probe p16).
  const bool categorical = dtype == "Categorical" || dtype.starts_with("Categorical(");
  if (dtype == "String" || dtype == "Boolean" || categorical || dtype.starts_with("Enum(")) {
    const std::string short_name = dtype == "Boolean" ? "bool" : categorical ? "cat" : "enum";
    fail("InvalidOperationError", phase, (dtype == "String" ? "sub operation not supported for dtypes `str` and `str`" : "`subtract` operation not supported for dtype `" + short_name + "`") + std::string("\n\nThis error occurred in the following expression:\n\t") + expression + "\nwhile evaluating this larger expression:\n\t" + expression + ".dt.total_seconds()\n");
  }
  static const std::map<std::string, const char*> names{{"Int8", "i8"}, {"Int16", "i16"}, {"Int32", "i32"}, {"Int64", "i64"}, {"UInt8", "u8"},
    {"UInt16", "u16"}, {"UInt32", "u32"}, {"UInt64", "u64"}, {"Float32", "f32"}, {"Float64", "f64"}, {"Null", "null"}};
  const auto found = names.find(dtype);
  const auto name = found == names.end() ? "" : found->second;
  if (!*name) throw std::invalid_argument("Unsupported native Polars performance timestamp dtype: " + dtype);
  fail("SchemaError", phase, "invalid series dtype: expected `Duration`, got `" + std::string(name) + "` for series with name `end_time`\n\nThis error occurred in the following expression:\n\t" + expression + ".dt.total_seconds()\n");
}
namespace groups = detail::polars_group_keys;
// Round CO: a non-str scalar or a non-empty list as the case key (the
// group_by/join keys of polars_group_keys.hpp).
groups::Keys case_keys(const EventTable& source, Operation op, const EventAttribute& high, const PolarsCaseFilterOptions& options, const std::string& phase) {
  try {
  return groups::resolve(options.case_id_key, source.columns, source.dtypes,
                         [&](const std::string& name) { missing(source, name, op, phase, !null(high)); },
                         [&](const std::string& name) {
                           fail("DuplicateError", phase, groups::duplicate_message(name) + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n" + frame_plan(source) + contexts(nodes(op, !null(high))));
                         });
  } catch (const PolarsSequenceFilterError& error) { fail(error.type, phase, error.message); }
}
// Round DL: on an eager frame filter_on_ncases calls DataFrame.head(n) on
// the sorted case counts, after the group_by resolved: Python compares n
// with 0 first (any non-number fails it, None too), a negative n keeps all
// but the last -n rows (none past the height), a float that stays positive
// fails the integer conversion, and n past the UInt64 range fails it
// (probe dl/c2).
std::size_t eager_head(const EventAttribute& n, std::size_t height) {
  if (!integer(n) && !boolean(n) && !real(n))
    fail("TypeError", "apply", "'<' not supported between instances of '" + (null(n) ? std::string("NoneType") : core_type(n)) + "' and 'int'");
  if (real(n)) {
    const auto value = number(n);
    if (value < 0 && static_cast<double>(height) + value <= 0) return 0;
    fail("TypeError", "apply", "'float' object cannot be interpreted as an integer");
  }
  const auto count = exact(n);
  if (count < TokenInteger(0)) {
    const auto rest = TokenInteger(static_cast<std::int64_t>(height)) + count;
    return rest < TokenInteger(0) ? 0 : static_cast<std::size_t>(std::stoull(rest.decimal()));
  }
  if (TokenInteger(std::numeric_limits<std::uint64_t>::max()) < count) fail("OverflowError", "apply", "int too big to convert");
  return TokenInteger(static_cast<std::int64_t>(height)) < count ? height : static_cast<std::size_t>(std::stoull(count.decimal()));
}
EventTable collect(const EventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& options, const std::string& phase) {
  const bool eager = phase == "apply";
  source.validate();
  if (source.index || source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars case filters require declared dtypes and no dataframe index");
  // Round DN: on an eager frame the group_by over no key, the duration and
  // the filters run as on any key (each raising first); only the join back
  // onto the frame finds no key (probe dn/c2).
  const bool keyless = collection(options.case_id_key) && options.case_id_key.children.empty();
  // Round DN: the join's right side resolves first, so a missing timestamp
  // column fails before the join finds no key (probe dn/c2, lazy).
  if (keyless && !eager && op == Operation::kPerformance && text(options.timestamp_key) &&
      std::find(source.columns.begin(), source.columns.end(), options.timestamp_key.value) == source.columns.end())
    missing(source, options.timestamp_key.value, op, phase, !null(high));
  if (keyless && !eager)
    fail("InvalidOperationError", phase, "expected join keys/predicates\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nDF []; PROJECT */0 COLUMNS" + contexts({"join"}));
  const bool listed = !keyless && groups::modelled(options.case_id_key);
  const auto position = [&](const std::string& key) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), key);
    if (found == source.columns.end()) missing(source, key, op, phase, !null(high));
    return static_cast<std::size_t>(found - source.columns.begin());
  };
  groups::Keys keys;
  if (listed) keys = case_keys(source, op, high, options, phase);
  else if (!keyless) keys.items.push_back({false, position(options.case_id_key.value), options.case_id_key.value, source.dtypes[position(options.case_id_key.value)], {}, false});
  std::size_t time_at = 0;
  if (op == Operation::kPerformance) {
    if (collection(options.timestamp_key)) {
      // Round DE: a key naming several columns repeats start_time in the
      // aggregation once the case keys resolved (probe de/cp1).
      if (!options.timestamp_key.children.empty())
        fail("DuplicateError", phase, "column with name 'start_time' has more than one occurrence\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n" + frame_plan(source) + contexts(nodes(op, !null(high))));
      // Round DE: the aggregation keeps the case keys' output names, a
      // select when no key reads a column (set probes pltblci-6 and
      // pltblcj-6 case 185).
      std::string names;
      for (const auto& item : keys.items) names += (names.empty() ? "" : ", ") + debug_quote(item.name);
      const auto by = groups::keys_text(keys);
      const auto plan = keys.all_literal() ? "SELECT [" + by + "]\n  " + frame_plan(source) : "AGGREGATE[maintain_order: false]\n  [] BY [" + by + "]\n  FROM\n  " + frame_plan(source);
      fail("ColumnNotFoundError", phase, "unable to find column \"end_time\"; valid columns: [" + names + "]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n" + plan + contexts({"with_columns", "filter", "select", "join right", "join"}));
    }
    time_at = position(options.timestamp_key.value);
  }
  // Round CG: str bounds with the lower above the upper (byte order) fold the
  // interval to an empty one: nothing raises and no case passes. Round CS:
  // so do two int literals (bools among them, within Int64) and two
  // datetimes, and the duration is then never computed (a String or
  // numeric timestamp column raises nothing); floats never fold (fresh CK
  // bank 2026111691 case 100).
  const auto int_literal = [](const EventAttribute& v) {
    return (integer(v) || boolean(v)) && exact(v) >= TokenInteger(std::numeric_limits<std::int64_t>::min()) &&
           exact(v) <= TokenInteger(std::numeric_limits<std::int64_t>::max());
  };
  // Round DL: on an eager frame the case size runs its bounds as separate
  // filter calls (probe dl/c1): nothing folds.
  const bool separate = eager && op == Operation::kSize;
  const bool folded = op != Operation::kCount && !separate &&
      ((text(low) && text(high) && high.value < low.value) ||
       (int_literal(low) && int_literal(high) && exact(high) < exact(low)) ||
       (date(low) && date(high) && date_micros(high) < date_micros(low)));
  // Round DL: on an eager frame the group_by and the duration's with_columns
  // run before the filter builds its bound literals, whatever the bounds
  // (probe dl/c3); the case size builds each bound with its own filter.
  if (op == Operation::kPerformance && (!folded || eager)) validate_time_dtype(source.dtypes[time_at], phase);
  if (eager && op != Operation::kCount) validate_literals(op, low, separate ? none() : high);
  if (op != Operation::kCount && !folded) {
    // Whether the count or the duration has a present cell (a dict bound's
    // cast fails only then).
    // Round DO: the bounds filter the group_by's frame, the case keys beside
    // the count or the duration (on an eager frame the duration's start and
    // end times too). With four columns or more Polars filters it in four
    // row partitions and the errors race as in the numeric attribute filters
    // (DN1-4); the cases' order is the group_by's, random in the reference.
    // Over the reference's runs (probes do/r5.py-r8.py, 100 runs per layout
    // of 1-12 cases) the comparison error of the partitions without a
    // present cell wins when the frame holds fewer than four cases or one
    // case with a present cell, the cast error when it holds four or more
    // cases and two or more of them have one.
    const std::size_t width = keys.items.size() + (eager && op == Operation::kPerformance ? 3 : 1);
    const auto cast_wins = [&](bool present) {
      if (!present || width < 4) return present;
      std::unordered_map<std::string, bool> cases; std::size_t timed = 0;
      const auto cell_key = [](const std::optional<EventAttribute>& cell, const std::string& dtype) { return group_key(cell, dtype); };
      for (const auto& row : source.rows) {
        const auto found = cases.emplace(groups::group_of(keys, row, cell_key), false).first;
        const bool cell = op == Operation::kSize || (row[time_at] && !null(*row[time_at]));
        if (cell && !found->second) { found->second = true; ++timed; }
      }
      return cases.size() >= 4 && timed >= 2;
    };
    const bool present = cast_wins(op == Operation::kSize ? !source.rows.empty() : std::any_of(source.rows.begin(), source.rows.end(), [&](const auto& row) { return row[time_at] && !null(*row[time_at]); }));
    if (separate) validate_bound(low, none(), false, op, phase, present, true);
    else {
      validate_bound(low, high, false, op, phase, present);
      if (op == Operation::kPerformance || !null(high)) validate_bound(high, low, true, op, phase, present);
    }
  }
  // Durations count whole seconds of the column unit (round BW).
  const auto time_unit = op == Operation::kPerformance ? polars_datetime::column_unit(source.dtypes[time_at]) : polars_datetime::Unit::kMicroseconds;
  struct Group { std::size_t count = 0; bool absent = false, keep = false; std::optional<std::int64_t> first, last; };
  std::vector<Group> groups; std::unordered_map<std::string, std::size_t> positions; std::vector<std::size_t> row_groups;
  const auto cell_key = [](const std::optional<EventAttribute>& cell, const std::string& dtype) { return group_key(cell, dtype); };
  for (const auto& row : source.rows) {
    const auto key = groups::group_of(keys, row, cell_key);
    auto [found, inserted] = positions.emplace(key, groups.size());
    if (inserted) { groups.push_back({}); groups.back().absent = !groups::row_key(keys, row, cell_key); }
    auto& group = groups[found->second]; ++group.count; row_groups.push_back(found->second);
    if (op == Operation::kPerformance && !folded && row[time_at] && !null(*row[time_at])) {
      const auto& v = *row[time_at]; const auto parsed = polars_datetime::ticks(std::get_if<NativeDate>(&v.typed_value) ? std::get<NativeDate>(v.typed_value).value : v.value, time_unit);
      if (!parsed) throw std::invalid_argument("Native Polars performance requires valid declared temporal cells");
      if (!group.first || *parsed < *group.first) group.first = parsed;
      if (!group.last || *parsed > *group.last) group.last = parsed;
    }
  }
  if (op == Operation::kCount) {
    std::vector<std::size_t> order(groups.size()); std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return groups[a].count > groups[b].count; });
    const auto limit = eager ? std::optional<std::size_t>(eager_head(low, groups.size())) : std::nullopt;
    for (std::size_t i = 0; i < order.size() && (limit ? i < *limit : null(low) || TokenInteger(i) < exact(low)); ++i) groups[order[i]].keep = true;
  } else if (separate) {
    // The lower bound's filter, then the upper bound's on the cases it kept.
    for (auto& group : groups) group.keep = compare_bound(static_cast<std::int64_t>(group.count), low, false);
    if (!null(high)) {
      validate_literals(op, high, none());
      // Round DO: the kept cases' frame races as the first one did.
      const auto kept = static_cast<std::size_t>(std::count_if(groups.begin(), groups.end(), [](const Group& group) { return group.keep; }));
      const bool present = kept && (keys.items.size() + 1 < 4 || kept >= 4);
      validate_bound(high, none(), true, op, phase, present, true);
      for (auto& group : groups) group.keep = group.keep && compare_bound(static_cast<std::int64_t>(group.count), high, true);
    }
  } else for (auto& group : groups) {
    const auto value = op == Operation::kSize ? std::optional<std::int64_t>{static_cast<std::int64_t>(group.count)} : group.first ? std::optional<std::int64_t>{(*group.last - *group.first) / polars_datetime::per_second(time_unit)} : std::nullopt;
    group.keep = !folded && value && compare_bound(*value, low, false) && ((op == Operation::kSize && null(high)) || compare_bound(*value, high, true));
  }
  if (keyless) fail("InvalidOperationError", phase, "expected join keys/predicates");
  // Literal keys only: the selected keys are one row whatever passed.
  const bool every = keys.all_literal();
  auto result = source; result.rows.clear();
  groups::append_schema(result, keys, source);
  // Round CT: the size and performance filters join the frame with the
  // selected keys; a one-row NaN float key joins nothing (probe ct/j1).
  if (op != Operation::kCount && !every) {
    std::size_t right = 0;
    for (const auto& group : groups) right += !group.absent && group.keep;
    if (groups::one_row_nan_join(source, keys, right)) return result;
  }
  for (std::size_t i = 0; i < source.rows.size(); ++i) {
    const auto& group = groups[row_groups[i]];
    if (group.absent || (!every && !group.keep)) continue;
    auto kept = source.rows[i];
    groups::append_cells(kept, keys, source.rows[i]);
    result.rows.push_back(std::move(kept));
  }
  return result;
}
// Round CM: group_by(case) refuses a dict while the plan is built, and the
// performance filter's pl.col(timestamp) selects a dict's names.
PolarsCaseFilterOptions keyed(const PolarsCaseFilterOptions& raw, Operation op, const std::vector<std::string>& columns) {
  if (keys::dict_key(raw.case_id_key)) fail("TypeError", "apply", keys::dict_argument_message());
  auto options = raw;
  const auto& time = raw.timestamp_key;
  if (op == Operation::kPerformance && collection(time)) options.timestamp_key = keys::col_key(time, columns);
  return options;
}
LazyEventTable lazy(const LazyEventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& raw) {
  validate_apply(op, low, raw);
  validate_literals(op, low, high);
  const auto options = keyed(raw, op, source.columns());
  auto columns = source.columns(); auto dtypes = source.dtypes(); auto categories = source.categorical_dtypes();
  if (groups::modelled(options.case_id_key)) {
    // The schema of the keys as collect resolves them (a key that fails to
    // resolve fails collect).
    struct Unresolved {};
    try {
      const auto keys = groups::resolve(options.case_id_key, columns, dtypes, [](const std::string&) { throw Unresolved{}; }, [](const std::string&) { throw Unresolved{}; });
      groups::append_schema(columns, dtypes, categories, keys, source.categorical_dtypes());
    } catch (const Unresolved&) {
    } catch (const PolarsSequenceFilterError& error) {
      // Round DG: pl.lit refuses a key item past the Int128 range while
      // the expressions build (probe big-pltblck).
      fail(error.type, "apply", error.message);
    }
  }
  return LazyEventTable(std::move(columns), [source, op, low, high, options](const auto& consume) {
    const auto result = collect(source.materialize(), op, low, high, options, "collect");
    for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, std::move(dtypes), source.index(), std::move(categories));
}
EventTable eager(const EventTable& source, Operation op, const EventAttribute& low, const EventAttribute& high, const PolarsCaseFilterOptions& raw) {
  try {
    validate_apply(op, low, raw, true);
    const auto options = keyed(raw, op, source.columns);
    return collect(source, op, low, high, options, "apply");
  } catch (const PolarsCaseFilterError& error) {
    // Round DL: an eager frame's diagnostics (polars_eager_diagnostic.hpp).
    throw PolarsCaseFilterError(error.type, error.phase, detail::polars_eager_diagnostic(error.message));
  }
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
