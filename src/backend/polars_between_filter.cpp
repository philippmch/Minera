#include "minera/polars_between_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_float_text.hpp"
#include "polars_sort.hpp"
#include "polars_column_hint.hpp"
#include "polars_list_literal.hpp"
#include "polars_lit_model.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <limits>
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
  throw PolarsBetweenError(std::move(type), std::move(phase), std::move(message));
}

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }
bool collection(const EventAttribute& value) { return value.container != AttributeContainerKind::kScalar; }
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

std::vector<EventAttribute> anchors(const EventAttribute& value) {
  if (!collection(value)) return {value};
  if (value.container != AttributeContainerKind::kContainer) return value.children;
  std::vector<EventAttribute> keys;
  for (const auto& child : value.children) keys.push_back(EventAttribute{"", child.key});
  return keys;
}
namespace lit = detail::polars_lit;

// Python builds a list anchor as pl.lit builds a list (round CD: the shared
// literal model, its strict construction and supertype errors).
void validate_anchor(const EventAttribute& value, const std::string& phase = "apply") {
  if (!collection(value)) return;
  try {
    (void)lit::literal_of(lit::value_of(value));
  } catch (const PolarsSequenceFilterError& error) {
    fail(error.type, phase, error.what());
  }
}
std::string activity_key(const EventAttribute& value) {
  if (text(value)) return value.value;
  if (collection(value) && value.children.size() == 1 && text(value.children.front()))
    return value.children.front().value;
  fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(value) + "'.");
}
void validate_apply(const EventAttribute& act1, const EventAttribute& act2, const PolarsBetweenOptions& options) {
  const auto& key = options.case_id_key;
  if (null(key)) fail("InvalidOperationError", "apply", "At least one of `partition_by` and `order_by` must be specified in `over`");
  if (!text(key)) {
    if (collection(key)) fail("TypeError", "apply", "'" + core_type(key) + "' object is not an instance of 'str'");
    fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(key) + "'.");
  }
  (void)activity_key(options.activity_key);
  validate_anchor(act1); validate_anchor(act2);
}

Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32" || value == "UInt16" || value == "UInt8" || value == "Int32" || value == "Int16" || value == "Int8") return Dtype::kInteger;
  if (value == "Float64" || value == "Float32") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (polars_datetime::temporal(value)) return Dtype::kDate;
  throw std::invalid_argument("Unsupported native Polars between dtype: " + value);
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
std::string stack(bool sort_node, bool failed_node = false) {
  const std::vector<std::string> nodes = sort_node
      ? std::vector<std::string>{"sort", "with_columns", "with_columns", "with_columns", "with_columns", "with_columns", "with_columns", "with_columns", "filter", "with_columns", "join left", "join", "with_columns", "with_columns", "select"}
      : std::vector<std::string>{"with_columns", "with_columns", "with_columns", "with_columns", "with_columns", "with_columns", "filter", "with_columns", "join left", "join", "with_columns", "with_columns", "select"};
  std::string output = "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < nodes.size(); ++i)
    output += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'" + (failed_node && i == 0 ? " failed\n" : "\n");
  return output;
}
std::string sort_plan(const EventTable& source, const std::string& key, const std::string& padding) {
  return padding + "SORT BY [col(" + quote(key) + "), col(\"time:timestamp\")]\n" + padding + "  " + frame_plan(source);
}
std::string indexed_plan(const EventTable& source, const std::string& key, const std::string& padding) {
  return padding + " WITH_COLUMNS:\n" + padding + " [0.int_range([len().cast(Int64)]).over([col(" + quote(key) + ")]).alias(\"_event_idx\")] \n" +
         sort_plan(source, key, padding + "  ");
}
[[noreturn]] void missing(const EventTable& source, const std::string& name,
                         const std::string& key, bool indexed, const std::string& phase) {
  auto columns = source.columns;
  if (indexed) columns.push_back("_event_idx");
  std::string message = "unable to find column " + debug_quote(name) + "; valid columns: [";
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (i) message += ", ";
    message += debug_quote(columns[i]);
  }
  message += ']';
  if (const auto hint = detail::polars_column_hint(name, columns)) message += "\n\nDid you mean " + debug_quote(*hint) + '?';
  if (phase == "collect") {
    message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n";
    message += indexed ? indexed_plan(source, key, "") : frame_plan(source);
    message += stack(!indexed);
  }
  fail("ColumnNotFoundError", phase, message);
}

std::string literal(const EventAttribute& value) {
  if (null(value)) return "null";
  if (text(value)) return quote(value.value);
  if (boolean(value)) return static_cast<bool>(exact(value)) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  if (real(value)) return real_text(number(value));
  return quote(value.value);
}
// A scalar anchor is imploded into a list; once resolved, the plan prints the
// bare scalar.
std::string anchor_expression(const EventAttribute& value, bool resolved = false) {
  if (!collection(value) && !text(value) && !null(value))
    return '[' + lit::scalar_display(lit::value_of(value)) + (resolved ? "]" : ".implode()]");
  std::string output = "[[";
  const auto values = anchors(value);
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) output += ", ";
    output += literal(values[i]);
  }
  return output + "]]";
}

// Round CD: an anchor as the reference hands it to is_in. A str is wrapped
// in a list, a list (a dict: its keys) is a list literal, and any other
// scalar is a pl.lit Polars implodes into a one-element list.
struct Anchor {
  bool scalar = false;
  lit::Value value;      // the imploded scalar
  lit::Literal literal;  // the list literal (a scalar's one-element list)
  std::string shown;     // the element type an is_in error names
};
Anchor anchor_of(const EventAttribute& value) {
  Anchor out;
  if (collection(value) || (!null(value) && text(value))) {
    out.literal = collection(value) ? lit::literal_of(lit::value_of(value)) : lit::list_literal({lit::Value::of(value.value)});
    out.shown = lit::literal_dtype_name(out.literal);
    return out;
  }
  out.scalar = true;
  out.value = null(value) ? lit::Value::none() : lit::value_of(value);
  out.literal = lit::list_literal({out.value});
  out.shown = out.value.kind == lit::Kind::kInt ? "Unknown(Int(" + lit::integer_text(out.value) + "))"
              : out.value.kind == lit::Kind::kFloat ? std::string("Unknown(Float)")
                                                    : lit::literal_dtype_name(out.literal);
  return out;
}
// The is_in argument as the plan prints it before it resolves, and while it
// fails (an imploded scalar shows its implode).
std::string unresolved_text(const Anchor& a, bool failing) {
  if (!a.scalar) return lit::literal_text(a.literal);
  return '[' + lit::scalar_display(a.value) + (failing ? ".implode()]" : "]");
}
// The smallest integer dtype a dynamic integer that does not fit `dtype`
// meets it in, and whether the column casts strictly.
std::pair<std::string, bool> integer_supertype(const std::string& dtype, std::int64_t v) {
  const bool is_unsigned = dtype.rfind("UInt", 0) == 0;
  const int bits = std::stoi(dtype.substr(is_unsigned ? 4 : 3));
  const auto signed_bits = [](std::int64_t x) {
    for (const int b : {8, 16, 32}) if (x >= -(std::int64_t{1} << (b - 1)) && x < (std::int64_t{1} << (b - 1))) return b;
    return 64;
  };
  if (is_unsigned && v >= 0) {
    for (const int b : {16, 32}) if (static_cast<std::uint64_t>(v) < (std::uint64_t{1} << b)) return {"UInt" + std::to_string(b), false};
    return {"UInt64", false};
  }
  if (is_unsigned) return {"Int" + std::to_string(std::min(64, std::max(2 * bits, signed_bits(v)))), bits == 64};
  return {"Int" + std::to_string(std::max(bits, signed_bits(v))), false};
}
// A resolved is_in: a list meets the column in their supertype, an imploded
// scalar casts to the column's list type (a number that does not fit to its
// supertype, a float on integer data to Float64).
std::string resolved_text(const Anchor& a, const std::string& column, const std::string& dtype) {
  if (!a.scalar) return lit::coerced_is_in(column, dtype, a.literal);
  const auto c = "col(" + quote(column) + ")";
  const auto cast_list = [](const std::string& item, const std::string& to) {
    return "[[" + item + "].cast(List(" + to + "))]";
  };
  const auto& v = a.value;
  // Round CF: a Null column casts to the scalar's type; None stays. Round
  // CJ (probe/cj/b1.py, b2.py): a dynamic number keeps its unresolved type,
  // Unknown(Int(<value>)) or Unknown(Float).
  if (dtype == "Null") {
    if (v.kind == lit::Kind::kNone) return c + ".is_in([[null]])";
    std::string to, item = lit::scalar_display(v);
    if (v.kind == lit::Kind::kInt) {
      item = lit::integer_text(v);
      to = "Unknown(Int(" + item + "))";
    } else if (v.kind == lit::Kind::kFloat) {
      item = lit::polars_frame_float(v.real);
      to = "Unknown(Float)";
    } else {
      if (v.kind == lit::Kind::kBool) item = v.boolean ? "true" : "false";
      to = lit::debug_name(lit::lit_value(v).type);
    }
    return c + ".cast(" + to + ").is_in([[" + item + "]])";
  }
  if (v.kind == lit::Kind::kNone) return c + ".is_in(" + cast_list("null", lit::is_in_dtype_name(dtype)) + ")";
  if (v.kind == lit::Kind::kInt && (lit::integer_dtype(dtype) || lit::float_dtype(dtype))) {
    // Round CH: an int past the Int64 range shows its digits.
    const auto item = lit::integer_text(v);
    if (lit::float_dtype(dtype)) return c + ".is_in(" + cast_list(item, dtype) + ")";
    if (lit::integer_range_side(v.integer, dtype) == 0) return c + ".is_in(" + cast_list(item, dtype) + ")";
    const auto [to, strict] = integer_supertype(dtype, v.integer);
    return c + (strict ? ".strict_cast(" : ".cast(") + to + ").is_in(" + cast_list(item, to) + ")";
  }
  if (v.kind == lit::Kind::kFloat && (lit::integer_dtype(dtype) || lit::float_dtype(dtype))) {
    const auto item = lit::polars_frame_float(v.real);
    if (lit::float_dtype(dtype)) return c + ".is_in(" + cast_list(item, dtype) + ")";
    return c + ".strict_cast(Float64).is_in(" + cast_list(item, "Float64") + ")";
  }
  return c + ".is_in(" + lit::literal_text(a.literal) + ")";
}
// The literal a cell is looked up in: an imploded number cast to the
// column's type (Float32 rounds it).
lit::Literal lookup_literal(const Anchor& a, const std::string& dtype) {
  if (!a.scalar) return a.literal;
  const auto& v = a.value;
  // Round CH: an int rounds to Float32 straight from its digits (no double
  // in between), an int past the Int64 range as its unsigned value.
  const bool f32 = dtype == "Float32";
  if (lit::float_dtype(dtype) && v.kind == lit::Kind::kInt)
    return {lit::lit_type(lit::LitType::K::kFloat64),
            {lit::Value::of(f32 ? lit::float32_of_int(v) : lit::integer_real(v))}};
  if (lit::float_dtype(dtype) && v.kind == lit::Kind::kFloat)
    return {lit::lit_type(lit::LitType::K::kFloat64),
            {lit::Value::of(f32 ? static_cast<double>(static_cast<float>(v.real)) : v.real)}};
  return a.literal;
}
std::vector<std::string> enum_categories(const EventTable& source, std::size_t at) {
  std::vector<std::string> out;
  if (enum_dtype(source.dtypes[at]) && at < source.categorical_dtypes.size() && source.categorical_dtypes[at])
    for (const auto& c : source.categorical_dtypes[at]->categories) out.push_back(c.value);
  return out;
}
void validate_membership(const EventTable& source, std::size_t at, const std::string& key,
                         const Anchor& act1, const Anchor& act2, const std::string& phase) {
  const auto& column = source.columns[at];
  const auto& dtype = source.dtypes[at];
  const auto categories = enum_categories(source, at);
  const auto is_in = [&](const std::string& argument) { return "col(" + quote(column) + ").is_in(" + argument + ")"; };
  const auto suffix = [&](const std::string& first, const std::string& second) {
    return "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n WITH_COLUMNS:\n [" + first +
           ".alias(\"_is_act1\"), " + second + ".alias(\"_is_act2\")] \n" + indexed_plan(source, key, "  ") +
           stack(false, true);
  };
  // The second anchor resolves first.
  try {
    lit::check_is_in(dtype, categories, act2.literal,
                     suffix(is_in(unresolved_text(act1, false)), is_in(unresolved_text(act2, true))), &act2.shown);
    lit::check_is_in(dtype, categories, act1.literal,
                     suffix(is_in(unresolved_text(act1, true)), resolved_text(act2, column, dtype)), &act1.shown);
  } catch (const PolarsSequenceFilterError& error) {
    fail(error.type, phase, error.what());
  }
}

// Public diagnostic graph only, parameterized by roles, anchors and source schema.
#include "polars_between_diagnostic_template.inc"
std::string interpolate(const std::string& pattern,
                        const std::vector<std::pair<std::string, std::string>>& values) {
  std::string output;
  for (std::size_t at = 0; at < pattern.size();) {
    const auto found = std::find_if(values.begin(), values.end(), [&](const auto& value) {
      return pattern.compare(at, value.first.size(), value.first) == 0;
    });
    if (found == values.end()) output += pattern[at++];
    else {
      output += found->second;
      at += found->first.size();
    }
  }
  return output;
}
void validate_separator(const EventTable& source, const std::string& key, const std::string& activity,
                        const Anchor& act1, const Anchor& act2, const std::string& activity_dtype,
                        const EventAttribute& separator, const std::string& phase) {
  if (!collection(separator) || separator.container == AttributeContainerKind::kContainer) return;
  const auto expression = anchor_expression(separator);
  // Round CE: the plan below the failing select is resolved, so each anchor
  // prints as its resolved is_in (cast to the activity's list type).
  auto pattern = kSeparatorListDiagnostic;
  const auto substitute = [&](const std::string& form, const std::string& token) {
    for (auto at = pattern.find(form); at != std::string::npos; at = pattern.find(form, at + token.size()))
      pattern.replace(at, form.size(), token);
  };
  substitute("col(%ACTIVITY%).is_in(%ANCHOR1%)", "%RESOLVED1%");
  substitute("col(%ACTIVITY%).is_in(%ANCHOR2%)", "%RESOLVED2%");
  substitute("col(\"_first_act\").is_in(%ANCHOR1%)", "%FIRST1%");
  substitute("col(\"_last_act\").is_in(%ANCHOR2%)", "%LAST2%");
  auto message = interpolate(pattern, {
      {"%RESOLVED1%", resolved_text(act1, activity, activity_dtype)},
      {"%RESOLVED2%", resolved_text(act2, activity, activity_dtype)},
      {"%FIRST1%", resolved_text(act1, "_first_act", activity_dtype)},
      {"%LAST2%", resolved_text(act2, "_last_act", activity_dtype)},
      {"(inner: 'String',", "(inner: '" + detail::polars_lit::debug_name(detail::polars_lit::argument_literal(separator).type) + "',"},
      {"%CASE%", quote(key)}, {"%ACTIVITY%", quote(activity)},
      {"%SEPARATOR%", expression.substr(1, expression.size() - 2)}, {"%FRAME%", frame_plan(source)}});
  // Round CE: a case column of any other dtype is cast to String first
  // (probed on integer, Float32, Categorical, Enum, temporal and Boolean).
  const auto column = std::find(source.columns.begin(), source.columns.end(), key);
  if (column != source.columns.end() && source.dtypes.size() == source.columns.size() &&
      source.dtypes[static_cast<std::size_t>(column - source.columns.begin())] != "String") {
    const auto plain = "col(" + quote(key) + ").str.concat_horizontal([";
    if (const auto at = message.find(plain); at != std::string::npos)
      message.replace(at, plain.size(), "col(" + quote(key) + ").strict_cast(String).str.concat_horizontal([");
  }
  fail("InvalidOperationError", phase, message);
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
    if (!parsed) throw std::invalid_argument("Unsupported native Polars between temporal cell");
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
    // cast(pl.String) of a temporal case id: its unit's fractional digits.
    if (declared == "Date" || declared.starts_with("Datetime(")) return lit::string_cast(value, declared);
    // Round CE: a datetime separator literal as Polars casts it to String.
    const auto literal = lit::lit_value(lit::value_of(value));
    if (const auto cast = lit::cast_lit(literal.value, literal.type, lit::lit_type(lit::LitType::K::kString))) return cast->text;
    return std::get_if<NativeDate>(&value.typed_value) ? std::get<NativeDate>(value.typed_value).value : value.value;
  }
  if (value.container == AttributeContainerKind::kContainer) {
    std::string output = "{";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) output += ',';
      output += string_cast(value.children[i]);
    }
    return output + '}';
  }
  throw std::invalid_argument("Unsupported native Polars between string cast");
}
EventTable collect(const EventTable& source, const EventAttribute& act1, const EventAttribute& act2,
                   const PolarsBetweenOptions& options, const std::string& phase) {
  // Python builds the anchor and separator literals before Polars runs.
  validate_anchor(act1, phase); validate_anchor(act2, phase);
  if (collection(options.subcase_concat_str) && options.subcase_concat_str.container != AttributeContainerKind::kContainer)
    if (const auto error = detail::polars_lit::argument_literal_error(options.subcase_concat_str)) fail("TypeError", phase, *error);
  source.validate();
  if (source.index) throw std::invalid_argument("Native Polars between tables cannot have dataframe index metadata");
  if (source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars between requires declared column dtypes");
  const auto key = options.case_id_key.value, activity = activity_key(options.activity_key);
  const auto position = [&](const std::string& name, bool indexed) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), name);
    if (found == source.columns.end()) missing(source, name, key, indexed, phase);
    return static_cast<std::size_t>(found - source.columns.begin());
  };
  const auto case_at = position(key, false), time_at = position("time:timestamp", false), activity_at = position(activity, true);
  const auto case_kind = dtype(source.dtypes[case_at]), time_kind = dtype(source.dtypes[time_at]);
  (void)dtype(source.dtypes[activity_at]);
  const auto enum_dictionary = [&](std::size_t at, Dtype kind) -> std::optional<EnumRanks> {
    if (kind != Dtype::kEnum) return std::nullopt;
    if (source.categorical_dtypes.empty() || !source.categorical_dtypes[at])
      throw std::invalid_argument("Native Polars between requires an Enum category dictionary for selected column: " + source.columns[at]);
    const auto& categories = source.categorical_dtypes[at]->categories;
    EnumRanks ranks; ranks.reserve(categories.size());
    for (std::size_t rank = 0; rank < categories.size(); ++rank) ranks.emplace(categories[rank].value, rank);
    for (const auto& row : source.rows) {
      const auto& cell = row[at];
      if (cell && !null(*cell) && !ranks.contains(cell->value))
        throw std::invalid_argument("Native Polars between Enum cell is absent from its category dictionary: " + source.columns[at]);
    }
    return ranks;
  };
  const auto case_ranks = enum_dictionary(case_at, case_kind);
  const auto time_ranks = time_at == case_at ? std::optional<EnumRanks>{} : enum_dictionary(time_at, time_kind);
  const auto* case_dictionary = case_ranks ? &*case_ranks : nullptr;
  const auto* time_dictionary = time_at == case_at ? case_dictionary : time_ranks ? &*time_ranks : nullptr;
  const auto anchor1 = anchor_of(act1), anchor2 = anchor_of(act2);
  validate_membership(source, activity_at, key, anchor1, anchor2, phase);
  const auto& activity_dtype = source.dtypes[activity_at];
  const auto lookup1 = lookup_literal(anchor1, activity_dtype), lookup2 = lookup_literal(anchor2, activity_dtype);
  validate_separator(source, key, activity, anchor1, anchor2, activity_dtype, options.subcase_concat_str, phase);
  std::vector<std::size_t> order(source.rows.size()); std::iota(order.begin(), order.end(), 0);
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  polars_sort::sort_by_keys(order, [&](auto a, auto b) {
    const auto by_case = compare(source.rows[a][case_at], source.rows[b][case_at], case_kind, case_dictionary);
    return by_case ? by_case < 0 : compare(source.rows[a][time_at], source.rows[b][time_at], time_kind, time_dictionary) < 0;
  }, source.dtypes[case_at], source.dtypes[time_at]);
  auto result = source; result.rows.clear(); result.dtypes[case_at] = "String";
  if (!result.categorical_dtypes.empty()) result.categorical_dtypes[case_at].reset();
  std::size_t begin = 0;
  while (begin < order.size()) {
    auto end = begin + 1;
    while (end < order.size() && compare(source.rows[order[begin]][case_at], source.rows[order[end]][case_at], case_kind, case_dictionary) == 0) ++end;
    if (source.rows[order[begin]][case_at] && !null(*source.rows[order[begin]][case_at])) {
      struct Segment { std::size_t first; std::size_t last; std::string identifier; };
      std::vector<Segment> segments;
      std::optional<std::size_t> start;
      for (auto i = begin; i < end; ++i) {
        const auto& row = source.rows[order[i]];
        if (lit::literal_has(lookup1, row[activity_at], activity_dtype)) start = i;
        if (start && lit::literal_has(lookup2, row[activity_at], activity_dtype)) {
          segments.push_back({*start, i, std::to_string(*start - begin) + '_' + std::to_string(i - begin)});
          start.reset();
        }
      }
      // The public subcase number is a dense lexical rank of the textual
      // per-case start_end identifier, rather than its chronological ordinal.
      std::vector<std::string> identifiers;
      for (const auto& segment : segments) identifiers.push_back(segment.identifier);
      std::sort(identifiers.begin(), identifiers.end());
      identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
      for (const auto& segment : segments) {
        const auto rank = static_cast<std::size_t>(std::lower_bound(identifiers.begin(), identifiers.end(), segment.identifier) - identifiers.begin()) + 1;
        for (auto at = segment.first; at <= segment.last; ++at) {
          auto kept = source.rows[order[at]];
          auto& id = *kept[case_at];
          const auto new_id = null(options.subcase_concat_str) ? std::string{} :
              string_cast(id, source.dtypes[case_at]) + string_cast(options.subcase_concat_str) + std::to_string(rank);
          id.value = new_id; id.kind = AttributeKind::kString; id.typed_value = std::monostate{};
          id.is_null = null(options.subcase_concat_str);
          id.container = AttributeContainerKind::kScalar; id.children.clear();
          id.scalar_wrapper = false; id.scalar_children_list = false;
          if (id.first_class_role != FirstClassAttributeRole::kNone)
            id.first_class_state = id.is_null ? FirstClassAttributeState::kNull : FirstClassAttributeState::kPresent;
          result.rows.push_back(std::move(kept));
        }
      }
    }
    begin = end;
  }
  return result;
}

}  // namespace

PolarsBetweenOptions resolve_polars_between_parameters(const std::vector<PolarsBetweenParameter>& parameters) {
  PolarsBetweenOptions options;
  for (const bool enum_key : {false, true}) for (const auto& parameter : parameters) {
    if (parameter.enum_key != enum_key) continue;
    if (parameter.key == "pm4py:param:case_id_key") options.case_id_key = parameter.value;
    else if (parameter.key == "pm4py:param:activity_key") options.activity_key = parameter.value;
    else if (parameter.key == "subcase_concat_str") options.subcase_concat_str = parameter.value;
  }
  return options;
}

PolarsBetweenOptions resolve_polars_between_parameter_value(const EventAttribute& parameters) {
  if (null(parameters)) return {};
  if (parameters.container == AttributeContainerKind::kContainer) {
    std::vector<PolarsBetweenParameter> items;
    for (const auto& child : parameters.children) items.push_back({child.key, child, false});
    return resolve_polars_between_parameters(items);
  }
  detail::check_parameter_carrier(parameters, [](const std::string& type, const std::string& message) {
    throw PolarsBetweenError(type, "apply", message);
  });
  return {};
}

EventTable filter_polars_between(const EventTable& source, const EventAttribute& act1,
                                const EventAttribute& act2, const PolarsBetweenOptions& options) {
  validate_apply(act1, act2, options);
  return collect(source, act1, act2, options, "apply");
}
LazyEventTable filter_polars_between(const LazyEventTable& source, const EventAttribute& act1,
                                    const EventAttribute& act2, const PolarsBetweenOptions& options) {
  validate_apply(act1, act2, options);
  auto dtypes = source.dtypes(); auto categories = source.categorical_dtypes();
  const auto found = std::find(source.columns().begin(), source.columns().end(), options.case_id_key.value);
  if (found != source.columns().end()) {
    const auto at = static_cast<std::size_t>(found - source.columns().begin());
    if (dtypes.size() == source.columns().size()) dtypes[at] = "String";
    if (!categories.empty()) categories[at].reset();
  }
  return LazyEventTable(source.columns(), [source, act1, act2, options](const auto& consume) {
    const auto result = collect(source.materialize(), act1, act2, options, "collect");
    for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, std::move(dtypes), source.index(), std::move(categories));
}

}  // namespace minera
