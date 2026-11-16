#include "minera/polars_ltl_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_sort.hpp"
#include "polars_list_literal.hpp"
#include "polars_column_hint.hpp"
#include "polars_key_selection.hpp"
#include "python_scalar.hpp"
#include "polars_lit_model.hpp"
#include "polars_group_keys.hpp"
#include "polars_ltl_cse_model.hpp"
#include "pandas_parameter_carrier.hpp"

#include <algorithm>
#include <cctype>
#include <bit>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <unordered_set>
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
using Op = PolarsLtlFilterOperation;
#include "polars_ltl_diagnostic_templates.inc"

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
  throw PolarsLtlFilterError(std::move(type), std::move(phase), std::move(message));
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


Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32" || value == "UInt16" || value == "UInt8" || value == "Int32" || value == "Int16" || value == "Int8") return Dtype::kInteger;
  if (value == "Float64" || value == "Float32") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (polars_datetime::temporal(value)) return Dtype::kDate;
  throw std::invalid_argument("Unsupported native Polars LTL dtype: " + value);
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
    if (!dictionary) throw std::invalid_argument("Native Polars LTL Enum ordering requires its category dictionary");
    const auto position = [&](const std::string& value) {
      const auto found = dictionary->find(value);
      if (found == dictionary->end())
        throw std::invalid_argument("Native Polars LTL Enum cell is absent from its category dictionary");
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
    if (!parsed) throw std::invalid_argument("Unsupported native Polars LTL temporal cell");
    return *parsed;
  };
  const auto a = instant(*left), b = instant(*right);
  if (a.seconds != b.seconds) return a.seconds < b.seconds ? -1 : 1;
  return a.nanoseconds < b.nanoseconds ? -1 : a.nanoseconds > b.nanoseconds ? 1 : 0;
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

bool sequence_rule(Op op) { return op == Op::kEventuallyFollows || op == Op::kNext; }
std::string function_name(Op op) {
  return op == Op::kEventuallyFollows ? "eventually_follows" : op == Op::kNext ? "A_next_B_next_C" :
         op == Op::kFourEyes ? "four_eyes_principle" : "attr_value_different_persons";
}
std::string group_key(const Cell& value, const std::string& declared) {
  if (!value || null(*value) || declared == "Null") return "n";
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return "s" + value->value;
  if (declared == "Float64" || declared == "Float32") {
    const auto n = number(*value);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (declared == "Boolean" || declared == "Int64" || declared == "UInt64" || declared == "UInt32" || declared == "Int32" || declared == "UInt16" || declared == "Int16" || declared == "UInt8" || declared == "Int8") return "i" + exact(*value).decimal();
  if (polars_datetime::temporal(declared)) {
    const auto* stored = std::get_if<NativeDate>(&value->typed_value);
    const auto parsed = polars_datetime::ticks(stored ? stored->value : value->value, polars_datetime::column_unit(declared));
    if (!parsed) throw std::invalid_argument("Native Polars LTL declared temporal cell is invalid");
    return "t" + std::to_string(*parsed);
  }
  throw std::invalid_argument("Unsupported native Polars LTL grouping dtype: " + declared);
}
std::string literal(const EventAttribute& value) {
  if (null(value)) return "null";
  if (text(value)) return debug_quote(value.value);
  if (boolean(value)) return truth(value) ? "true" : "false";
  if (integer(value)) return "dyn int: " + exact(value).decimal();
  // Round CI: Rust's float display, as the other filters print it.
  if (real(value)) return "dyn float: " + detail::polars_dyn_float(number(value));
  if (date(value)) {
    auto out = std::get_if<NativeDate>(&value.typed_value) ? std::get<NativeDate>(value.typed_value).value : value.value;
    std::replace(out.begin(), out.end(), 'T', ' ');
    if (out.ends_with("+00:00")) { out.resize(out.size() - 6); out += " UTC"; }
    return out;
  }
  if (collection(value)) {
    std::string out = "[";
    for (std::size_t i = 0; i < value.children.size(); ++i) { if (i) out += ", "; out += literal(value.children[i]); }
    return out + ']';
  }
  throw std::invalid_argument("Unsupported native Polars LTL literal");
}

struct Prepared {
  Op operation;
  PolarsLtlFilterOptions options;
  EventAttribute original;
  std::vector<EventAttribute> values;
};
// Round CO: a case key the joins read as one literal (probe ltl2): a
// non-str number or bool, or a list holding one non-str item (pl.lit of the
// item; the sort and the case select print the list itself). A Null literal
// never joins.
namespace groups = detail::polars_group_keys;
struct LiteralKey { groups::Item item; bool listed = false; };
std::optional<LiteralKey> literal_key(const EventAttribute& key) {
  if (key.container == AttributeContainerKind::kScalar)
    return groups::modelled(key) ? std::optional<LiteralKey>(LiteralKey{groups::literal_item(key), false}) : std::nullopt;
  if (key.container != AttributeContainerKind::kList && key.container != AttributeContainerKind::kTuple) return std::nullopt;
  if (key.children.empty() || (key.children.size() == 1 && groups::text_key(key.children.front()))) return std::nullopt;
  // Round CQ: several items are no one literal (list_key_failure).
  if (key.children.size() != 1) return std::nullopt;
  const auto& item = key.children.front();
  if (item.container != AttributeContainerKind::kContainer && !item.is_null && !is_native_na_attribute(item) && !groups::modelled(item))
    throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: the item type");
  return LiteralKey{groups::literal_item(item), true};
}
// The literal as an alias or over() prints it, and as the case select and
// the sort print the key.
std::string literal_alias(const LiteralKey& key) {
  namespace lit = detail::polars_lit;
  return key.item.value.kind == lit::Kind::kInt ? lit::integer_text(key.item.value) : lit::lit_display(key.item.type, key.item.value);
}
std::string literal_sort_key(const LiteralKey& key) {
  return key.listed ? "[" + literal_alias(key) + "]" : groups::key_text(key.item);
}
std::vector<std::string> category_list(const EventTable& source, std::size_t column) {
  std::vector<std::string> out;
  if (enum_dtype(source.dtypes[column]) && column < source.categorical_dtypes.size() && source.categorical_dtypes[column])
    for (const auto& c : source.categorical_dtypes[column]->categories) out.push_back(c.value);
  return out;
}
// Round CN: the timestamp sort key as the plan prints it: a column, or a
// dict (struct) or list literal.
std::string time_sort(const EventAttribute& key) {
  namespace lit = detail::polars_lit;
  if (key.container == AttributeContainerKind::kContainer) return lit::dict_literal_display(key);
  if (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple) {
    try { return lit::literal_list_display(lit::argument_literal(key)); } catch (const PolarsSequenceFilterError&) {}
  }
  // Round CW: a scalar that is no column name (an int, a float, a bool)
  // sorts as its literal (fresh probe w2: dyn int: 1).
  if (key.container == AttributeContainerKind::kScalar && !text(key) && !null(key)) return lit::scalar_display(lit::value_of(key));
  return "col(" + quote(key.value) + ")";
}
// Round CT: an int case key sorts as its scalar literal and windows over
// itself (probe ct/l1).
std::string list_key_display(const EventAttribute& key) {
  namespace lit = detail::polars_lit;
  if (key.container == AttributeContainerKind::kScalar) return lit::scalar_display(lit::value_of(key));
  if (key.children.empty()) return "[]";
  return lit::literal_list_display(lit::argument_literal(key));
}
std::string over_keys_display(const EventAttribute& key) {
  std::string out;
  for (const auto& item : key.container == AttributeContainerKind::kScalar ? std::vector<EventAttribute>{key} : key.children) {
    if (!out.empty()) out += ", ";
    out += groups::text_key(item) ? "col(" + quote(item.value) + ")" : literal_alias(LiteralKey{groups::literal_item(item), false});
  }
  return out;
}
// Round CQ: four or more four_eyes activities with a literal case key fail
// at the third pair join, the pairs counted in (i, j) order (probe cq/q5).
std::pair<std::size_t, std::size_t> third_pair(std::size_t n) { return n == 3 ? std::pair<std::size_t, std::size_t>{1, 2} : std::pair<std::size_t, std::size_t>{0, 3}; }
std::string render(std::string_view pattern, const EventTable& source, const Prepared& p,
                   const std::string& expression = "") {
  const auto& o = p.options;
  // Round CD: each activity filter as the plan prints it, coerced to the
  // attribute column's dtype.
  std::optional<std::size_t> attribute;
  if (text(o.attribute_key)) {
    const auto found = std::find(source.columns.begin(), source.columns.end(), o.attribute_key.value);
    if (found != source.columns.end()) attribute = static_cast<std::size_t>(found - source.columns.begin());
  }
  const auto value = [&](std::size_t i) {
    namespace lit = detail::polars_lit;
    const auto& key = o.attribute_key.value;
    if (i >= p.values.size()) return "col(" + quote(key) + ") == null";
    const auto v = lit::value_of(p.values[i]);
    // Round CI: a dict as the struct comparison prints it.
    if (v.kind == lit::Kind::kDict && attribute) {
      const auto& declared = source.dtypes[*attribute];
      const auto m = lit::struct_model(p.values[i], declared);
      if (declared == "String") return "col(" + quote(key) + ") == " + lit::struct_literal(m);
      if (m.cast_column) return lit::struct_cast_text(key, m) + " == " + lit::struct_display(m);
      return "col(" + quote(key) + ") == " + lit::struct_display(m);
    }
    if (!lit::scalar_literal(v)) {
      // Round CE: a list as pl.lit built it; a Null column casts to its type.
      std::string lhs = "col(" + quote(key) + ")", shown = literal(p.values[i]);
      if (v.kind == lit::Kind::kList || v.kind == lit::Kind::kTuple) {
        try {
          const auto built = lit::literal_of(v);
          shown = lit::literal_list_display(built);
          if (attribute && source.dtypes[*attribute] == "Null") lhs += ".cast(" + lit::debug_name(lit::list_of(built.type)) + ")";
        } catch (const PolarsSequenceFilterError&) {}
      }
      return lhs + " == " + shown;
    }
    std::optional<std::string> declared;
    std::vector<std::string> categories;
    if (attribute) { declared = source.dtypes[*attribute]; categories = category_list(source, *attribute); }
    return lit::comparison_plan(key, declared, v, lit::CompareOp::kEq, &categories);
  };
  std::optional<LiteralKey> literal;
  try { literal = literal_key(o.case_id_key); } catch (const std::invalid_argument&) {}
  // Round CO: each placeholder is rendered only when the pattern names it
  // (a value the template leaves out may not build: case 111 of the fresh
  // CI bank).
  using Text = std::function<std::string()>;
  const auto item_at = [&](std::size_t i) -> Text { return [&value, i] { return value(i); }; };
  const std::size_t none = static_cast<std::size_t>(-1);
  const std::map<std::string_view, Text> substitutions = {
    {"FRAME", [&] { return frame_plan(source); }}, {"CASE_Q", [&] { return quote(o.case_id_key.value); }},
    {"ATTRIBUTE_Q", [&] { return quote(o.attribute_key.value); }}, {"TIME_SORT", [&] { return time_sort(o.timestamp_key); }},
    {"RESOURCE_Q", [&] { return quote(o.resource_key.value); }}, {"RESOURCE2_Q", [&] { return quote(o.resource_key.value + "_2"); }},
    {"LITERAL", [&] { return literal ? literal_alias(*literal) : std::string("0"); }},
    {"LITERAL_S", [&] { return literal ? groups::key_text(literal->item) : std::string("0"); }},
    {"LITERAL_K", [&] { return literal ? literal_sort_key(*literal) : std::string("0"); }},
    // Round CQ: the filter-failure tail names a literal case key as the
    // window and sort key (case 373 of the fresh CK 11 bank).
    // Round CQ: a list case key as the select, group_by and sort print it
    // (one List literal) and as over() prints its items.
    {"LIST_KEY", [&] { return list_key_display(o.case_id_key); }},
    {"OVER_KEYS", [&] { return over_keys_display(o.case_id_key); }},
    {"TIME_Q", [&] { return quote(o.timestamp_key.value); }},
    {"CASE_OVER", [&] { return literal ? literal_alias(*literal) : "col(" + quote(o.case_id_key.value) + ")"; }},
    {"CASE_SORT", [&] { return literal ? literal_sort_key(*literal) : "col(" + quote(o.case_id_key.value) + ")"; }},
    {"FIRST", item_at(0)}, {"SECOND", item_at(1)}, {"THIRD", item_at(2)}, {"LAST", item_at(p.values.empty() ? none : p.values.size() - 1)},
    {"PAIR_A", item_at(p.values.size() < 3 ? none : third_pair(p.values.size()).first)},
    {"PAIR_B", item_at(p.values.size() < 3 ? none : third_pair(p.values.size()).second)}, {"EXPR", [&] { return expression; }}
  };
  std::string out;
  for (std::size_t at = 0; at < pattern.size();) {
    if (pattern[at] != '%') { out += pattern[at++]; continue; }
    const auto end = pattern.find('%', at + 1);
    if (end == std::string_view::npos) throw std::logic_error("Unclosed native LTL diagnostic placeholder");
    const auto found = substitutions.find(pattern.substr(at + 1, end - at - 1));
    if (found == substitutions.end()) throw std::logic_error("Unknown native LTL diagnostic placeholder");
    out += found->second(); at = end + 1;
  }
  return out;
}
namespace keys = detail::polars_keys;
std::string missing_message(const EventTable& source, const std::string& key, bool index = false) {
  if (keys::by_name(key)) return keys::by_name_message(key);
  std::string out = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) { if (i) out += ", "; out += debug_quote(source.columns[i]); }
  if (index) { if (!source.columns.empty()) out += ", "; out += "\"_index\""; }
  out += ']';
  if (const auto hint = detail::polars_column_hint(key, source.columns)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';
  return out;
}
std::optional<std::size_t> column(const EventTable& source, const EventAttribute& role) {
  if (!text(role)) return std::nullopt;
  const auto found = std::find(source.columns.begin(), source.columns.end(), role.value);
  return found == source.columns.end() ? std::nullopt : std::optional<std::size_t>(found - source.columns.begin());
}
[[noreturn]] void invalid_col(const EventAttribute& value) {
  fail("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" + python_type(value) + "'.");
}
std::vector<PolarsLtlFilterParameter> parameters(const EventAttribute& value) {
  if (null(value)) return {};
  if (value.container == AttributeContainerKind::kContainer) {
    std::vector<PolarsLtlFilterParameter> out;
    for (const auto& child : value.children) out.push_back({child.key, child, false});
    return out;
  }
  detail::check_parameter_carrier(value, [](const std::string& type, const std::string& message) { fail(type, "apply", message); });
  return {};
}
// Round CM: group_by, sort and over refuse a dict key while the plan is built.
void dict_argument(const EventAttribute& key) {
  if (keys::dict_key(key)) fail("TypeError", "apply", keys::dict_argument_message());
}
// Round CN: a list case key becomes a Series literal (built strictly) where
// the reference lists it among its sort, over or join keys, while Python
// builds the expressions.
void list_key_literal(const EventAttribute& key) {
  // Round DB: pl.lit refuses a set key.
  if (!key.is_null && detail::polars_lit::set_argument(key)) fail("TypeError", "apply", detail::polars_lit::kSetLiteralError);
  if (key.is_null || (key.container != AttributeContainerKind::kList && key.container != AttributeContainerKind::kTuple)) return;
  if (const auto error = detail::polars_lit::argument_literal_error(key)) fail("TypeError", "apply", *error);
}
// repr() of a Python datetime (its text is ISO with an optional fraction
// and UTC offset).
std::string datetime_repr(const std::string& text) {
  const auto number = [&](std::size_t at, std::size_t width) { return std::stoi(text.substr(at, width)); };
  std::string result = "datetime.datetime(" + std::to_string(number(0, 4)) + ", " + std::to_string(number(5, 2)) + ", " +
                       std::to_string(number(8, 2)) + ", " + std::to_string(number(11, 2)) + ", " + std::to_string(number(14, 2));
  const int second = number(17, 2);
  std::size_t position = 19;
  int micro = 0;
  if (position < text.size() && text[position] == '.') {
    std::string digits;
    ++position;
    while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) digits += text[position++];
    digits.resize(6, '0');
    micro = std::stoi(digits.substr(0, 6));
  }
  if (second || micro) result += ", " + std::to_string(second);
  if (micro) result += ", " + std::to_string(micro);
  if (position < text.size()) {
    const int sign = text[position] == '-' ? -1 : 1;
    const int minutes = sign * (number(position + 1, 2) * 60 + number(position + 4, 2));
    if (minutes == 0) {
      result += ", tzinfo=datetime.timezone.utc";
    } else {
      const int seconds = minutes * 60;
      result += ", tzinfo=datetime.timezone(datetime.timedelta(" +
                std::string(seconds < 0 ? "days=-1, seconds=" + std::to_string(86400 + seconds) : "seconds=" + std::to_string(seconds)) + "))";
    }
  }
  return result + ")";
}
// Python's repr of a key (the A_next_B_next_C selection message). Round CV:
// a datetime item is datetime.datetime(...).
std::string key_repr(const EventAttribute& value) {
  return detail::pandas_container_repr(value, [](const EventAttribute& v) -> std::string {
    if (null(v)) return "None";
    if (date(v) && !std::holds_alternative<NativeDate>(v.typed_value) && v.value.size() >= 19) return datetime_repr(v.value);
    if (boolean(v)) return truth(v) ? "True" : "False";
    if (integer(v)) return exact(v).decimal();
    if (real(v)) return python::float_repr(native_attribute_number(v));
    std::string out = "'";
    for (const char c : v.value) { if (c == '\\' || c == '\'') out += '\\'; out += c; }
    return out + "'";
  }, [](const std::string& k) { return "'" + k + "'"; });
}
// pl.col of a list: every item a str, or the first item names the type.
void col_list(const EventAttribute& key) {
  // Round DB: a set's items too.
  if (key.container != AttributeContainerKind::kList && key.container != AttributeContainerKind::kTuple && !detail::polars_lit::set_argument(key)) return;
  if (!key.children.empty() && !text(key.children.front()))
    fail("TypeError", "apply", "invalid input for `col`\n\nExpected iterable of type `str` or `DataType`, got iterable of type '" + python_type(key.children.front()) + "'.");
  for (const auto& item : key.children) if (!text(item)) fail("TypeError", "apply", null(item) ? "'None' is not an instance of 'str'" : "'" + python_type(item) + "' object is not an instance of 'str'");
}
Prepared prepare(const PolarsLtlFilterCall& call, const std::vector<std::string>& columns) {
  if (!call.values) fail("TypeError", "apply", function_name(call.operation) + "() missing 1 required positional argument: '" +
      (call.operation == Op::kDifferentPersons ? "attribute_value" : "attribute_values") + "'");
  Prepared out{call.operation, call.options, *call.values, {}};
  const auto entries = call.parameter_value ? parameters(*call.parameter_value) : call.parameters.value_or(std::vector<PolarsLtlFilterParameter>{});
  for (const bool enum_key : {false, true}) for (const auto& entry : entries) {
    if (entry.enum_key != enum_key) continue;
    if (entry.key == "pm4py:param:case_id_key") out.options.case_id_key = entry.value;
    else if (entry.key == "pm4py:param:attribute_key") out.options.attribute_key = entry.value;
    else if (entry.key == "pm4py:param:timestamp_key") out.options.timestamp_key = entry.value;
    else if (entry.key == "pm4py:param:resource_key") out.options.resource_key = entry.value;
    else if (entry.key == "positive") out.options.positive = entry.value;
    else if (entry.key == "enable_timestamp") out.options.enable_timestamp = entry.value;
    else if (entry.key == "timestamp_diff_boundaries") out.options.timestamp_diff_boundaries = entry.value;
  }
  const auto& o = out.options;
  if (sequence_rule(out.operation)) { dict_argument(o.case_id_key); list_key_literal(o.case_id_key); list_key_literal(o.timestamp_key); }
  if (sequence_rule(out.operation) && null(o.case_id_key))
    fail("InvalidOperationError", "apply", "At least one of `partition_by` and `order_by` must be specified in `over`");
  if (out.operation == Op::kDifferentPersons) out.values = {out.original};
  else {
    if (out.operation != Op::kEventuallyFollows && !text(out.original) && !collection(out.original))
      fail("TypeError", "apply", "object of type '" + core_type(out.original) + "' has no len()");
    out.values = iter(out.original);
    // Round DC: eventually_follows builds every activity's filter before it
    // indexes the values (probe dc/ef3); four_eyes indexes first (dc/fe1).
    if (out.operation != Op::kNext && out.operation != Op::kEventuallyFollows && out.values.size() > 1)
      for (std::size_t i = 0; i < out.values.size(); ++i) (void)item(out.original, i);
  }
  if (out.operation != Op::kNext && (out.operation != Op::kFourEyes || out.values.size() > 1)) {
    if (!text(o.attribute_key) && !collection(o.attribute_key)) invalid_col(o.attribute_key);
    col_list(o.attribute_key);
    if (out.operation == Op::kDifferentPersons && (out.original.container == AttributeContainerKind::kSet || out.original.container == AttributeContainerKind::kFrozenSet))
      fail("TypeError", "apply", "cannot create expression literal for value of type set.\n\nHint: Pass `allow_object=True` to accept any value and create a literal of type Object.");
  }
  // A list activity becomes a Series literal when its filter is built.
  if (out.operation != Op::kNext && (out.operation != Op::kFourEyes || out.values.size() > 1))
    for (std::size_t at = 0; at < out.values.size(); ++at) {
      const auto& query = out.values[at];
      // Round CP: four_eyes selects [case, resource] from each activity's
      // filter as it builds it, so a list case key's literal fails after
      // the first activity's (CM bank 2026111550 case 300).
      if (out.operation == Op::kFourEyes && at == 1) list_key_literal(o.case_id_key);
      if (query.container == AttributeContainerKind::kList || query.container == AttributeContainerKind::kTuple)
        if (const auto error = detail::polars_lit::argument_literal_error(query)) fail("TypeError", "apply", *error);
      // Round DB: and a set activity is refused by pl.lit.
      if (detail::polars_lit::set_argument(query)) fail("TypeError", "apply", detail::polars_lit::kSetLiteralError);
      // Round CI: a dict becomes a struct literal then too (a list field
      // that cannot be built fails it, as in the timeframe filter).
      if (query.container == AttributeContainerKind::kContainer) {
        try {
          (void)detail::polars_lit::struct_fields(query);
        } catch (const PolarsSequenceFilterError&) {
          fail("TypeError", "apply", "cannot create expression literal for value of type dict.\n\nHint: Pass `allow_object=True` to accept any value and create a literal of type Object.");
        }
      }
    }
  if (out.operation == Op::kEventuallyFollows && out.values.size() > 1)
    for (std::size_t i = 0; i < out.values.size(); ++i) (void)item(out.original, i);
  // Round CN: the activity filters are built before the case key joins
  // them (None, a dict, a list literal that cannot be built), and four_eyes
  // reads its resource key after that (probe q1).
  if (!sequence_rule(out.operation) && (out.operation != Op::kFourEyes || out.values.size() > 1)) {
    if (null(o.case_id_key)) fail("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
    dict_argument(o.case_id_key);
    list_key_literal(o.case_id_key);
    if (out.operation == Op::kFourEyes) {
      if (!text(o.resource_key) && !collection(o.resource_key)) invalid_col(o.resource_key);
      // Round DB: a set resource key is refused by pl.lit where a list one
      // is concatenated.
      if (detail::polars_lit::set_argument(o.resource_key)) fail("TypeError", "apply", detail::polars_lit::kSetLiteralError);
      if (collection(o.resource_key)) fail("TypeError", "apply", "can only concatenate list (not \"str\") to list");
    }
    // Round DB: attr_value_different_persons groups by [case, resource], so
    // pl.lit refuses a set resource key while the plan is built (probe
    // set-pltblck-2 case 188).
    if (out.operation == Op::kDifferentPersons && detail::polars_lit::set_argument(o.resource_key))
      fail("TypeError", "apply", detail::polars_lit::kSetLiteralError);
  }
  // pl.col(dict) selects the dict's names (round CM).
  if (out.operation != Op::kNext) out.options.attribute_key = keys::col_key(o.attribute_key, columns);
  return out;
}

// Replaces the context stack of a rendered diagnostic.
std::string restack(std::string text, const std::vector<std::string>& nodes) {
  const auto at = text.find("This error occurred with the following context stack:\n");
  if (at == std::string::npos) return text;
  text.resize(at);
  text += "This error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < nodes.size(); ++i)
    text += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + (nodes[i].ends_with("' failed") ? "\n" : "'\n");
  return text;
}
void repeat(std::vector<std::string>& nodes, std::ptrdiff_t times, std::initializer_list<const char*> items) {
  for (std::ptrdiff_t i = 0; i < times; ++i) nodes.insert(nodes.end(), items.begin(), items.end());
}
// eventually_follows chains one join per later activity; a failure at the
// filter of activity `at` skips the joins of the activities before it.
std::vector<std::string> eventually_stack(const Prepared& p, std::size_t at, bool filter, bool empty_join = false) {
  const auto n = static_cast<std::ptrdiff_t>(p.values.size());
  std::vector<std::string> nodes;
  // Round CT: without activities the empty key fails the one case join
  // (probe ct/l1).
  if (empty_join && n == 0) return {"join", "select"};
  if (filter) nodes.push_back("filter' failed");
  if (empty_join) nodes.push_back("join");
  else nodes.insert(nodes.end(), {"select", "unique", "join right", "join"});
  repeat(nodes, n - 1 - static_cast<std::ptrdiff_t>(at), {"join left", "join"});
  nodes.insert(nodes.end(), {"select", "unique"});
  repeat(nodes, n - 1, {"join left", "join"});
  nodes.insert(nodes.end(), {"join right", "join", "select"});
  return nodes;
}
// four_eyes_principle joins every activity pair; a negative filter filters
// its pair joins. Plans resolve left inputs first: the first activity
// filters the left input of pair 0, a later activity `at` first the right
// input of pair (0, at), whose result joins the earlier pairs from the right
// (round CD).
std::vector<std::string> four_eyes_stack(const Prepared& p, bool filter, std::size_t at = 0) {
  std::vector<std::string> nodes;
  if (filter) nodes.push_back("filter' failed");
  const auto k = static_cast<std::ptrdiff_t>(at == 0 ? 0 : at - 1);
  nodes.insert(nodes.end(), {"select", "unique", at == 0 ? "join left" : "join right", "join"});
  if (truth(p.options.positive)) nodes.insert(nodes.end(), {"select", "unique", "join left", "join"});
  else nodes.insert(nodes.end(), {"filter", "select", "unique"});
  const auto pairs = static_cast<std::ptrdiff_t>(p.values.size() * (p.values.size() - 1) / 2);
  if (k > 0) nodes.insert(nodes.end(), {"join right", "join"});
  repeat(nodes, pairs - 1 - k, {"join left", "join"});
  nodes.insert(nodes.end(), {"join right", "join"});
  return nodes;
}
// Round CQ: a list case key of several items or of str items (probes cq/q4,
// q5). The reference selects and groups by [case key, resource] (the list
// one List literal named "literal") and joins and windows over the key's
// items (a str a column, any other value a literal): several literals repeat
// the name "literal", a str item is missing from the select or group_by
// output, and eventually_follows' window resolves its str items against the
// frame. Mixed items fail earlier (list_key_literal). Returns only for
// eventually_follows over present, distinct str items with at most one
// activity, which joins on those columns (row_case_key).
std::vector<std::string> list_names(const EventAttribute& key) {
  std::vector<std::string> out;
  for (const auto& item : key.children) out.push_back(item.value);
  return out;
}
bool str_items(const EventAttribute& key) {
  return !key.children.empty() && std::all_of(key.children.begin(), key.children.end(), [](const EventAttribute& v) { return groups::text_key(v); });
}
std::vector<std::string> four_first_join_stack(const Prepared& p);
void list_key_failure(const EventTable& source, const Prepared& p, const std::string& phase) {
  const auto& o = p.options;
  const auto& items = o.case_id_key.children;
  const bool strs = str_items(o.case_id_key);
  const bool literals = std::all_of(items.begin(), items.end(), [](const EventAttribute& v) {
    return v.container == AttributeContainerKind::kScalar && (null(v) || groups::modelled(v));
  });
  if (!strs && !literals) throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: the item types");
  const auto names = strs ? list_names(o.case_id_key) : std::vector<std::string>{};
  const auto missing = [&](const std::string& name, const std::vector<std::string>& valid) {
    std::string out = "unable to find column " + debug_quote(name) + "; valid columns: [";
    for (std::size_t i = 0; i < valid.size(); ++i) { if (i) out += ", "; out += debug_quote(valid[i]); }
    out += ']';
    if (const auto hint = detail::polars_column_hint(name, valid)) out += "\n\nDid you mean " + debug_quote(*hint) + '?';
    return out;
  };
  const auto literal_repeat = [&](const std::string& node) { return groups::duplicate_message("literal", node); };
  if (p.operation == Op::kFourEyes) {
    if (strs && std::find(names.begin(), names.end(), o.resource_key.value) != names.end())
      throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: the resource as a case item");
    const auto tail = restack(render(kFourListJoin, source, p), four_first_join_stack(p));
    if (strs) fail("ColumnNotFoundError", phase, missing(names.front(), {"literal", o.resource_key.value}) + tail);
    fail("InvalidOperationError", phase, "joining with repeated key names; already joined on literal and literal" + tail);
  }
  if (p.operation == Op::kDifferentPersons) {
    if (strs && std::any_of(names.begin(), names.end(), [&](const std::string& n) { return n == o.resource_key.value || n == "count"; }))
      throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: a grouped column as a case item");
    const auto tail = render(kDifferentListGroup, source, p);
    if (strs) fail("ColumnNotFoundError", phase, missing(names.front(), {"literal", o.resource_key.value, "count"}) + tail);
    fail("DuplicateError", phase, literal_repeat("group_by keys") + tail);
  }
  if (p.operation != Op::kEventuallyFollows) throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: the operation");
  if (!text(o.timestamp_key)) throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: the timestamp key");
  const auto n = p.values.size();
  // The window resolves its columns first.
  for (const auto& name : names)
    if (std::find(source.columns.begin(), source.columns.end(), name) == source.columns.end())
      fail("ColumnNotFoundError", phase, missing(name, source.columns) + render(kEventListSort, source, p));
  // Then each activity's select repeats a name.
  std::optional<std::string> repeated;
  if (literals) repeated = "literal";
  for (std::size_t i = 0; i < names.size() && !repeated; ++i)
    for (std::size_t j = 0; j < i; ++j)
      if (names[i] == names[j]) { repeated = names[i]; break; }
  if (repeated) {
    if (n == 0) fail("DuplicateError", phase, groups::duplicate_message(*repeated, "projections") + restack(render(kEventListOver, source, p), {"select", "unique", "join right", "join", "select"}));
    fail("DuplicateError", phase, groups::duplicate_message(*repeated, "projections") + restack(render(kEventListFilter, source, p), eventually_stack(p, 0, false)));
  }
  if (n <= 1) return;
  if (std::any_of(names.begin(), names.end(), [&](const std::string& c) { return c == "_index" || c == o.timestamp_key.value; }))
    throw std::invalid_argument("Native Polars LTL list case keys outside the observed domain: a selected column as a case item");
  std::vector<std::string> nodes{"join", "filter", "select", "unique", "join right", "join"};
  repeat(nodes, static_cast<std::ptrdiff_t>(n) - 2, {"join left", "join"});
  nodes.insert(nodes.end(), {"join right", "join", "select"});
  fail("ColumnNotFoundError", phase, missing(names.front(), {"literal", "_index", o.timestamp_key.value}) + restack(render(kEventListSelect, source, p), nodes));
}
// Round CQ: four_eyes fails at its first pair join when a list case key
// cannot join (no key, repeated literal keys, a str item the select left out):
// after every pair join when positive, inside the filtered joins otherwise
// (probe cq/q5).
std::vector<std::string> four_first_join_stack(const Prepared& p) {
  const auto pairs = static_cast<std::ptrdiff_t>(p.values.size() * (p.values.size() - 1) / 2);
  std::vector<std::string> nodes{"join"};
  if (truth(p.options.positive)) {
    nodes.insert(nodes.end(), {"select", "unique"});
    repeat(nodes, pairs, {"join left", "join"});
  } else {
    nodes.insert(nodes.end(), {"filter", "select", "unique"});
    repeat(nodes, pairs - 1, {"join left", "join"});
  }
  nodes.insert(nodes.end(), {"join right", "join"});
  return nodes;
}
std::string missing_tail(const EventTable& source, const Prepared& p, bool eager, bool sorting, bool attribute) {
  if (eager) return p.operation == Op::kDifferentPersons && !attribute ? render(kDifferentEagerGroupTail, source, p) : "";
  if (p.operation == Op::kEventuallyFollows) {
    if (sorting) return render(kEventSortTail, source, p);
    // Round CT: a list or int case key sorts as a literal and windows over
    // its items (probe ct/l1).
    const auto& key = p.options.case_id_key;
    const bool literal = !key.is_null && (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple ||
                                          (key.container == AttributeContainerKind::kScalar && integer(key)));
    return restack(render(literal ? kEventListOver : kEventAttributeTail, source, p), eventually_stack(p, 0, false));
  }
  if (p.operation == Op::kNext) return sorting ? render(kNextSortTail, source, p) : "";
  if (p.operation == Op::kFourEyes) return restack(render(attribute ? kFourAttributeTail : kFourMissingTail, source, p), four_eyes_stack(p, false));
  return render(attribute ? kDifferentAttributeTail : kDifferentMissingTail, source, p);
}
void validate_queries(const EventTable& source, const Prepared& p, bool eager, bool resolution_only = false, bool first_only = false, bool coercion = false, std::size_t limit = static_cast<std::size_t>(-1));
void compute_query(const EventTable& source, const Prepared& p, bool eager, std::size_t at);
bool listed_key(const EventAttribute& key) {
  return !key.is_null && (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple);
}
// Round CS: A_next_B_next_C sorts by a case key that is no str as a
// literal and numbers each case's rows with int_range(len()).over(key)
// (probe cs/n1): a str item of a list key naming no column fails that
// window while the plan resolves, whatever the frame's height.
void next_window(const EventTable& source, const Prepared& p, bool eager) {
  namespace lit = detail::polars_lit;
  const auto& o = p.options;
  if (!listed_key(o.case_id_key)) return;
  for (const auto& item : o.case_id_key.children) {
    if (!text(item) || column(source, item)) continue;
    if (eager) throw std::invalid_argument("Native Polars LTL A_next_B_next_C on an eager frame with a missing list case item is outside the observed domain");
    std::string sort = "SORT BY [" + list_key_display(o.case_id_key);
    const auto& ts = o.timestamp_key;
    if (!null(ts)) sort += ", " + (!collection(ts) && !text(ts) ? lit::scalar_display(lit::value_of(ts)) : time_sort(ts));
    fail("ColumnNotFoundError", "apply",
         missing_message(source, item.value) + "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' <---\n" + sort +
             "]\n  " + frame_plan(source) + "\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n");
  }
}
[[noreturn]] void next_outside(const std::string& what) {
  throw std::invalid_argument("Native Polars LTL A_next_B_next_C on one row is outside the observed domain: " + what);
}
// The last step on one row: a list key is no dict key, a tuple no Series
// name (probes cs/ln2, ln6).
[[noreturn]] void next_last_step(const EventAttribute& key) {
  fail("TypeError", "apply", key.container == AttributeContainerKind::kTuple ? "Series name must be a string" : "unhashable type: 'list'");
}
// The Python type of the one-row frame's cell in column `at` (the last is
// the "_pos" window column).
std::string next_cell_type(const EventTable& source, std::size_t at) {
  if (at == source.columns.size()) return "int";
  const auto& cell = source.rows.front()[at];
  if (!cell || cell->is_null) return "NoneType";
  const auto& d = source.dtypes[at];
  if (d == "String" || categorical_dtype(d) || enum_dtype(d)) return "str";
  if (dtype(d) == Dtype::kInteger) return "int";
  if (dtype(d) == Dtype::kFloat) return "float";
  if (d == "Boolean") return "bool";
  if (d == "Null") return "NoneType";
  if (d == "Date") return "datetime.date";
  if (d.starts_with("Datetime")) return "datetime.datetime";
  next_outside("the cell type of a (row, column) key");
}
// Round CS: on one row the literal sort passes and the window runs; Python
// then indexes the collected frame (its columns and "_pos") with the key
// as polars' DataFrame.__getitem__ reads it, takes unique() of the
// selection, builds pl.col(key) and the filter that compares it with the
// selection when that has rows (probes cs/n1, ln2 to ln6). Returns when
// only the attribute read and the last step are left (a one-item name key,
// or a mask that selects nothing).
void next_one_row(const EventTable& source, const Prepared& p) {
  const auto& key = p.options.case_id_key;
  if (std::find(source.columns.begin(), source.columns.end(), "_pos") != source.columns.end()) next_outside("a _pos column");
  const auto width = source.columns.size() + 1;
  const auto col_error = [](const EventAttribute& first) {
    fail("TypeError", "apply", "invalid input for `col`\n\nExpected iterable of type `str` or `DataType`, got iterable of type '" + python_type(first) + "'.");
  };
  if (!listed_key(key)) {
    if (boolean(key)) {
      if (truth(key)) fail("IndexError", "apply", "index True is out of bounds for DataFrame of height 1");
      invalid_col(key);
    }
    if (integer(key)) {
      const auto v = exact(key);
      if (v != TokenInteger(0) && v != TokenInteger(-1)) fail("IndexError", "apply", "index " + v.decimal() + " is out of bounds for DataFrame of height 1");
      invalid_col(key);
    }
    if (real(key)) fail("TypeError", "apply", "cannot select columns using key of type 'float': " + python::float_repr(native_attribute_number(key)));
    next_outside("the scalar key type");
  }
  const auto& items = key.children;
  if (items.empty()) return;  // the window failed (validate_schema)
  const bool tuple = key.container == AttributeContainerKind::kTuple;
  // A pair is (row, column) unless its first item is a bool or a str.
  if (tuple && items.size() == 2 && !boolean(items[0]) && !text(items[0])) {
    const auto& row = items[0];
    const auto& col = items[1];
    std::size_t at = 0;
    if (null(col)) fail("TypeError", "apply", "cannot select columns using key of type 'NoneType': None");
    if (real(col)) fail("TypeError", "apply", "cannot select columns using key of type 'float': " + python::float_repr(native_attribute_number(col)));
    if (text(col)) at = *column(source, col);
    else if (integer(col) && !boolean(col)) {
      const auto v = exact(col);
      const auto w = TokenInteger(static_cast<std::int64_t>(width));
      if (v < -w || v >= w) fail("IndexError", "apply", "index " + v.decimal() + " is out of bounds for sequence of length " + std::to_string(width));
      at = static_cast<std::size_t>((v < TokenInteger(0) ? v + w : v).small_value());
    } else next_outside("the column item of a (row, column) key");
    if (null(row)) fail("TypeError", "apply", "cannot select elements using key of type 'NoneType': None");
    if (!integer(row) || boolean(row)) next_outside("the row item of a (row, column) key");
    const auto r = exact(row);
    if (r < TokenInteger(-1) || r >= TokenInteger(1)) fail("IndexError", "apply", "index " + r.decimal() + " is out of bounds for sequence of length 1");
    fail("AttributeError", "apply", "'" + next_cell_type(source, at) + "' object has no attribute 'unique'");
  }
  const auto& first = items.front();
  // A bool first item selects columns by mask.
  if (boolean(first)) {
    if (items.size() != width)
      fail("ValueError", "apply", "expected " + std::to_string(width) + " values when selecting columns by boolean mask, got " + std::to_string(items.size()));
    for (const auto& item : items) if (!boolean(item) && !null(item)) next_outside("a mask item");
    if (std::any_of(items.begin(), items.end(), [](const EventAttribute& v) { return boolean(v) && truth(v); })) col_error(first);
    // Nothing selected: the result fails before the attribute column is read.
    next_last_step(key);
  }
  // Rows, when every present item is an int (a bool counts as 0 or 1).
  bool rows = !text(first), present = false;
  for (const auto& item : items) {
    if (null(item)) continue;
    if (integer(item) || boolean(item)) present = true;
    else rows = false;
  }
  if (rows && present) {
    // Polars bounds the indices by the u32 maximum first.
    const TokenInteger limit((std::int64_t{1} << 32) - 1), zero(0), one(1);
    for (const auto& item : items)
      if (!null(item) && !exact(item).is_small()) next_outside("an index past the int64 range");
    for (const auto& item : items)
      if (!null(item) && exact(item) >= limit) fail("ValueError", "apply", "index positions should be smaller than 2^32");
    for (const auto& item : items)
      if (!null(item) && exact(item) < -limit) fail("ValueError", "apply", "index positions should be greater than or equal to -2^32");
    std::vector<std::string> failed;
    std::size_t failures = 0;
    for (const auto& item : items) {
      if (null(item)) continue;
      const auto v = exact(item);
      if (v < zero && v + one < zero) {
        ++failures;
        if (failed.size() < 10) failed.push_back((v + one).decimal());
      }
    }
    if (failures) {
      std::string shown = "[";
      if (failed.size() <= 3) {
        for (std::size_t i = 0; i < failed.size(); ++i) shown += (i ? ", " : "") + failed[i];
      } else {
        shown += failed[0] + ", " + failed[1] + ", \xe2\x80\xa6 " + failed.back();
      }
      shown += "]";
      fail("InvalidOperationError", "apply",
           "conversion from `i64` to `u32` failed in column 'literal' for " + std::to_string(failures) + " out of " + std::to_string(items.size()) +
               " values: " + shown + (failures > 10 ? "\n\nDid not show all failed cases as there were too many." : "") +
               "\n\nThis error occurred in the following expression:\n\t.when([(col(\"\")) < (0)]).then([(1) + (col(\"\"))]).otherwise(col(\"\")).strict_cast(UInt32)\n");
    }
    for (const auto& item : items) {
      if (null(item)) continue;
      const auto v = exact(item);
      if ((v < zero ? v + one : v) >= one) fail("OutOfBoundsError", "apply", "gather indices are out of bounds");
    }
    col_error(first);
  }
  // Columns by name.
  if (!text(first)) fail("TypeError", "apply", "cannot select columns using Sequence with elements of type '" + core_type(first) + "'");
  for (const auto& item : items) {
    if (null(item)) fail("TypeError", "apply", "'None' is not an instance of 'str'");
    if (!text(item)) next_outside("a name item");
  }
  // The first name (in key order) that occurs again (probe cs/n2).
  for (std::size_t i = 0; i < items.size(); ++i)
    for (std::size_t j = i + 1; j < items.size(); ++j)
      if (items[i].value == items[j].value)
        fail("DuplicateError", "apply", "could not create a new DataFrame: column with name '" + items[i].value + "' has more than one occurrence");
  if (items.size() >= 2) {
    std::string message = "The predicate passed to 'LazyFrame.filter' expanded to multiple expressions: \n\n";
    for (const auto& item : items) message += "\t[(col(" + quote(item.value) + ")) == (Series[" + first.value + "])],\n";
    fail("ComputeError", "apply", message + "This is ambiguous. Try to combine the predicates with the 'all_horizontal' or `any_horizontal' expression.");
  }
}
// A timestamp key the sort reads as a literal (a dict, a list or a number).
bool literal_sort(const EventAttribute& key) { return collection(key) || (!text(key) && !null(key)); }
// Round CQ: the first activity is the same literal (type and value) as a
// later one, so both filters are one shared plan (probed: ['a', 'a'] and
// ['a', 'b', 'a'] share, ['b', 'a', 'a'], [1, 1.0] and ['a', ('a',)] do not).
bool first_activity_shared(const Prepared& p) {
  namespace lit = detail::polars_lit;
  if (p.values.size() < 2) return false;
  const auto shape = [](const EventAttribute& v) {
    try {
      const auto value = lit::value_of(v);
      if (lit::scalar_literal(value)) return lit::debug_name(lit::lit_value(value).type) + "|" + lit::scalar_display(value);
      return std::string(v.container == AttributeContainerKind::kTuple ? "t|" : "l|") + lit::literal_text(lit::argument_literal(v));
    } catch (const std::exception&) {
      return std::string();
    }
  };
  const auto first = shape(p.values.front());
  if (first.empty()) return false;
  return std::any_of(p.values.begin() + 1, p.values.end(), [&](const EventAttribute& v) { return shape(v) == first; });
}
void eventually_steps(const EventTable& source, const Prepared& p, bool eager);
std::string filter_plan(const EventTable& source, const Prepared& p, std::size_t at);
void validate_schema(const EventTable& source, const Prepared& p, bool eager) {
  source.validate();
  if (source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars LTL requires every column dtype");
  const auto& o = p.options;
  const auto phase = eager || p.operation == Op::kNext ? "apply" : "collect";
  if (p.operation == Op::kFourEyes && p.values.size() < 2) return;
  const auto required = [&](const EventAttribute& role, bool sorting, bool attribute) {
    if (!text(role) || column(source, role)) return;
    if (p.operation == Op::kNext && attribute && !eager) {
      // Round CI: an empty frame never reads the attribute column; its empty
      // selection fails the join instead (select_cases).
      if (source.rows.empty()) return;
      fail("ColumnNotFoundError", "apply", debug_quote(role.value) + " not found");
    }
    const bool index = p.operation == Op::kEventuallyFollows && attribute;
    fail("ColumnNotFoundError", phase, missing_message(source, role.value, index) + missing_tail(source, p, eager, sorting, attribute));
  };
  if (sequence_rule(p.operation)) {
    required(o.case_id_key, true, false); required(o.timestamp_key, true, false);
    // Round CN: eventually_follows sorts by a literal timestamp only with two
    // or more activities, and only after its plan resolves (below).
    // Round CO: a one-row frame sorts by the literal (case 355 of the
    // fresh CK bank).
    // Round CR/CS: a list case item naming no column fails the window as the
    // plan resolves; then the literal sort fails unless the frame has one
    // row, where an empty key fails the window's group_by and Python reads
    // the collected frame (next_one_row).
    if (p.operation == Op::kNext) next_window(source, p, eager);
    if (p.operation == Op::kNext && source.rows.size() != 1 && (literal_sort(o.timestamp_key) || !text(o.case_id_key)))
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(source.rows.size()) + " and Series length: 1");
    const bool one_row_literal = p.operation == Op::kNext && source.rows.size() == 1 && !text(o.case_id_key);
    if (one_row_literal && listed_key(o.case_id_key) && o.case_id_key.children.empty())
      fail("ComputeError", "apply", "at least one key is required in a group_by operation\n\nThis error occurred in the following expression:\n\t0.int_range([len().cast(Int64)]).over([])\n");
    if (p.operation == Op::kNext && eager) fail("AttributeError", "apply", "'DataFrame' object has no attribute 'collect'");
    if (one_row_literal) next_one_row(source, p);
  }
  if (p.operation == Op::kNext) {
    if (null(o.attribute_key)) fail("TypeError", "apply", "cannot select columns using key of type 'NoneType': None");
    // Round CP: an empty frame selects no case, so a dict key (or a list
    // holding one) is never read; the empty selection fails the join.
    const bool dict_read = keys::dict_key(o.attribute_key) ||
                           (collection(o.attribute_key) && std::any_of(o.attribute_key.children.begin(), o.attribute_key.children.end(),
                                                                       [](const EventAttribute& v) { return keys::dict_key(v); }));
    // Round DC: a set key as well (CI set probe cases 173, 228).
    if ((dict_read || detail::polars_lit::set_argument(o.attribute_key)) && source.rows.empty()) return;
    if (keys::dict_key(o.attribute_key)) fail("TypeError", "apply", "cannot select columns using key of type 'dict': " + key_repr(o.attribute_key));
    if (collection(o.attribute_key) && std::any_of(o.attribute_key.children.begin(), o.attribute_key.children.end(), [](const EventAttribute& v) { return keys::dict_key(v); })) {
      if (keys::dict_key(o.attribute_key.children.front()))
        fail("TypeError", "apply", "cannot select columns using Sequence with elements of type 'dict'");
      fail("TypeError", "apply", "'dict' object is not an instance of 'str'");
    }
    // Round DB: the collected frame refuses a set as a column selector.
    if (detail::polars_lit::set_argument(o.attribute_key)) fail("TypeError", "apply", detail::polars_lit::set_selector_error(o.attribute_key));
    if (collection(o.attribute_key)) fail("AttributeError", "apply", "'DataFrame' object has no attribute 'to_list'");
    if (integer(o.attribute_key)) {
      std::size_t height = 0;
      // Round CS: a literal case key reads the one-row frame itself.
      if (!text(o.case_id_key)) {
        const auto v = exact(o.attribute_key);
        if (v == TokenInteger(0) || v == TokenInteger(-1)) next_outside("an attribute index inside the frame");
        height = 1;
      }
      if (text(o.case_id_key)) {
        const auto ci = *column(source, o.case_id_key);std::map<std::string,std::size_t> groups;
        for (const auto& row : source.rows) ++groups[group_key(row[ci],source.dtypes[ci])];
        if (!source.rows.empty()) height=groups[group_key(source.rows.front()[ci],source.dtypes[ci])];
      }
      fail("IndexError", "apply", "index " + exact(o.attribute_key).decimal() + " is out of bounds for DataFrame of height " + std::to_string(height));
    }
  }
  // eventually_follows without activities builds no attribute filter.
  const bool no_filter = p.operation == Op::kEventuallyFollows && p.values.empty();
  // Round CT: a list case key's str item naming no column fails the window
  // before the attribute filter resolves (probe ct/l1).
  if (p.operation == Op::kEventuallyFollows && !no_filter && listed_key(o.case_id_key) && text(o.attribute_key) && !column(source, o.attribute_key))
    for (const auto& item : o.case_id_key.children)
      if (text(item) && !column(source, item)) fail("ColumnNotFoundError", phase, missing_message(source, item.value) + render(kEventListSort, source, p));
  if (!no_filter) required(o.attribute_key, false, true);
  // Round CS: a one-item name key (or a mask selecting nothing) on one row
  // fails building the result: a list is no dict key, a tuple no Series
  // name (probes cs/ln2, ln6).
  if (p.operation == Op::kNext && source.rows.size() == 1 && !text(o.case_id_key)) {
    if (!text(o.attribute_key)) next_outside("the attribute key type");
    next_last_step(o.case_id_key);
  }
  // The attribute filter resolves before the case and resource columns.
  if (p.operation == Op::kDifferentPersons && text(o.attribute_key)) validate_queries(source, p, eager, true);
  // The first activity's filter resolves before the first pair selects the
  // case and resource columns.
  if (p.operation == Op::kFourEyes && text(o.attribute_key)) validate_queries(source, p, eager, true, true);
  // Round CP: a selector key expanding to no column fails the attribute
  // filter before the case and resource columns resolve (CK bank 2026111590
  // cases 156, 399).
  if (!sequence_rule(p.operation) && collection(o.attribute_key) && !no_filter)
    fail("ComputeError", phase, "The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame" + missing_tail(source,p,eager,false,true));
  if (!sequence_rule(p.operation)) { required(o.case_id_key, false, false); required(o.resource_key, false, false); }
  const auto literal = literal_key(o.case_id_key);
  if (collection(o.case_id_key) && !literal) {
    if (!o.case_id_key.children.empty()) list_key_failure(source, p, phase);
    else {
      if (p.operation == Op::kEventuallyFollows) fail("InvalidOperationError", phase, restack(render(kEventEmptyJoin, source, p), eventually_stack(p, 0, false, true)));
      // Round CQ: the stack follows the activity count and sign (probe cq/q5).
      if (p.operation == Op::kFourEyes) fail("InvalidOperationError", phase, restack(render(kFourEmptyJoin, source, p), four_first_join_stack(p)));
      fail("InvalidOperationError", phase, render(kDifferentEmptyJoin, source, p));
    }
  }
  if (p.operation != Op::kNext && collection(o.attribute_key) && !no_filter) {
    fail("ComputeError", phase, "The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame" + missing_tail(source,p,eager,false,true));
  }
  // Round CO: a literal case key joins each activity's selection on the
  // literal, so a third activity's join finds the right literal column
  // twice (probe ltl2), and two eventually_follows activities sort by the
  // literal (one value) when the plan runs.
  if (literal && p.operation != Op::kDifferentPersons && p.values.size() >= 3) {
    if (p.operation == Op::kEventuallyFollows) {
      // Round CS: the third activity's join resolves its inputs, the first
      // three activity filters, before its schema (CK bank 2026111671 case
      // 93).
      validate_queries(source, p, eager, true, false, false, 3);
      std::vector<std::string> nodes{"join schema resolving", "join"};
      repeat(nodes, static_cast<std::ptrdiff_t>(p.values.size()) - 3, {"join left", "join"});
      nodes.insert(nodes.end(), {"select", "unique", "join left", "join"});
      repeat(nodes, static_cast<std::ptrdiff_t>(p.values.size()) - 2, {"join left", "join"});
      nodes.insert(nodes.end(), {"join right", "join", "select"});
      fail("DuplicateError", phase, restack(render(kEventLiteralJoin, source, p), nodes));
    }
    // Round CQ: later pairs add their joins below (probe cq/q5).
    const auto pairs = static_cast<std::ptrdiff_t>(p.values.size() * (p.values.size() - 1) / 2);
    std::vector<std::string> nodes{"join schema resolving", "join"};
    repeat(nodes, pairs - 3, {"join left", "join"});
    nodes.insert(nodes.end(), {"join right", "join"});
    fail("DuplicateError", phase, restack(render(truth(o.positive) ? kFourLiteralJoin : kFourLiteralJoinNegative, source, p), nodes));
  }
  // Round CO: the activity filters run below the sort, so their compute
  // errors come first (probe p20). Round CQ: only when the first activity's
  // filter is shared with another activity (the same literal: the plan then
  // caches it and resolves its schema before anything runs); otherwise the
  // sort fails first (CK bank 2026111611 case 114).
  if (literal && p.operation == Op::kEventuallyFollows && p.values.size() == 2) {
    validate_queries(source, p, eager, true); if (first_activity_shared(p)) validate_queries(source, p, eager, false, false, true);
    if (source.rows.size() != 1)
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(source.rows.size()) + " and Series length: 1");
  }
  // Round CW: a case column runs the activity filters and sorts in the
  // order of the reference's cached plan (polars_ltl_cse_model.hpp): fresh
  // CK 2026111913 case 117 (attribute key = case key: each filter runs below
  // its own sort, whose height is that selection's) and repeated activities.
  if (p.operation == Op::kEventuallyFollows && p.values.size() >= 2 && text(o.case_id_key)) {
    validate_queries(source, p, eager, true);
    // Round CW: a timestamp key naming the case column selects that column
    // twice in the first pair's left input once every filter has resolved
    // (probe cw/w3).
    if (text(o.timestamp_key) && column(source, o.timestamp_key) == column(source, o.case_id_key)) {
      std::vector<std::string> nodes{"select", "join left", "join", "filter", "select", "unique", "join right", "join"};
      repeat(nodes, static_cast<std::ptrdiff_t>(p.values.size()) - 2, {"join left", "join"});
      nodes.insert(nodes.end(), {"join right", "join", "select"});
      fail("DuplicateError", eager ? "apply" : "collect",
           "projections contained duplicate output name '" + o.case_id_key.value +
               "'. It's possible that multiple expressions are returning the same default column name. If this is the case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names." +
               restack(render(kEventFilterTail, source, p, filter_plan(source, p, 0)), nodes));
    }
    eventually_steps(source, p, eager);
    return;
  }
  // The literal sort key (one value) fails the sort when it runs, unless the
  // frame has one row.
  if (p.operation == Op::kEventuallyFollows && literal_sort(o.timestamp_key) && p.values.size() >= 2) {
    validate_queries(source, p, eager, true); if (first_activity_shared(p)) validate_queries(source, p, eager, false, false, true);
    if (source.rows.size() != 1)
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(source.rows.size()) + " and Series length: 1");
  }
}
// Round CD: each activity filter compares the attribute column with one
// value as Polars types it (detail::polars_lit::check_scalar): a str with a
// temporal column fails while the plan resolves, a number with a string
// column, mismatched temporal types and the like when the filter runs.
void validate_queries(const EventTable& source, const Prepared& p, bool eager, bool resolution_only, bool first_only, bool coercion, std::size_t limit) {
  if (p.operation == Op::kNext || (p.operation == Op::kFourEyes && p.values.size() < 2) || p.values.empty()) return;
  namespace lit = detail::polars_lit;
  const auto ai = *column(source,p.options.attribute_key);
  const auto& declared = source.dtypes[ai];
  // Round CE: only a String column casts a list literal to String; an Enum
  // or Categorical column compares it (and fails) like any other column.
  const bool stringy = declared == "String";
  const auto categories = category_list(source, ai);
  const auto phase=eager?"apply":"collect";
  const auto& key = p.options.attribute_key.value;
  const auto resolve_fail = [&](std::size_t at, const std::string& prefix, const std::string& expression) {
    std::string tail;
    if (!eager) {
      if (p.operation == Op::kEventuallyFollows) tail = restack(render(kEventFilterTail,source,p,expression),eventually_stack(p,at,true));
      else if (p.operation == Op::kFourEyes) tail = restack(render(kDifferentFilterTail,source,p,expression),four_eyes_stack(p,true,at));
      else tail = render(kDifferentFilterTail,source,p,expression);
    }
    fail("InvalidOperationError",phase,prefix+tail);
  };
  // The whole plan resolves before any filter runs.
  const auto count = std::min(limit, first_only ? std::min<std::size_t>(1, p.values.size()) : p.values.size());
  for (std::size_t at = 0; at < count; ++at) {
    const auto& query = p.values[at];
    if (collection(query) && query.container != AttributeContainerKind::kContainer) {
      if (stringy) {
        // Round CE: the literal as pl.lit built it.
        std::string inner = "Null", shown = literal(query);
        try {
          const auto built = lit::argument_literal(query);
          inner = lit::debug_name(built.type);
          shown = lit::literal_list_display(built);
        } catch (const PolarsSequenceFilterError&) {}
        resolve_fail(at,"cannot cast List type (inner: '"+inner+"', to: 'String')","col("+quote(key)+") == "+shown+".cast(String)");
      }
      continue;
    }
    const auto value = lit::value_of(query);
    if (!lit::scalar_literal(value)) continue;
    const auto check = lit::check_scalar(key, declared, value, lit::CompareOp::kEq, &categories);
    if (check.stage == lit::ScalarCheck::Stage::kResolve) resolve_fail(at, check.message, "col("+quote(key)+") == "+lit::scalar_display(value));
  }
  // Round CO: the first activity's filter runs below the literal sort, so
  // its compute errors come before the sort fails (probe p20, case 130 of
  // the CM seed 50 bank); the other activities' come after it (case 46 of
  // the CL bank, case 3 of the CN one).
  if (resolution_only && !coercion) return;
  // eventually_follows runs its first activity's filter, then the last
  // one's, then the others in order (round CI: probed with 3 to 5
  // activities; round CE fitted three).
  const bool backward = p.operation == Op::kEventuallyFollows;
  for (std::size_t step = 0; step < (coercion ? std::min<std::size_t>(1, p.values.size()) : p.values.size()); ++step)
    compute_query(source, p, eager, backward && step ? (step == 1 ? p.values.size() - 1 : step - 1) : step);
}

// One activity filter's compute-stage error (the plan has resolved).
void compute_query(const EventTable& source, const Prepared& p, bool eager, std::size_t at) {
  namespace lit = detail::polars_lit;
  const auto ai = *column(source, p.options.attribute_key);
  const auto& declared = source.dtypes[ai];
  const auto categories = category_list(source, ai);
  const auto phase = eager ? "apply" : "collect";
  const auto& key = p.options.attribute_key.value;
  {
    const auto value = lit::value_of(p.values[at]);
    // Round CI: a dict compares as a struct (the timeframe filter's model).
    if (value.kind == lit::Kind::kDict) {
      const auto m = lit::struct_model(p.values[at], declared);
      const bool present = std::any_of(source.rows.begin(), source.rows.end(), [&](const auto& row) { return !lit::null_cell(row[ai]); });
      if (const auto f = lit::struct_failure(key, declared, m, present))
        fail(f->type, phase, f->message + "\n\nThis error occurred in the following expression:\n\t" + f->expression +
                                 (f->larger.empty() ? std::string() : "\nwhile evaluating this larger expression:\n\t" + f->larger) + "\n");
      return;
    }
    if (!lit::scalar_literal(value)) {
      // A list compares with any column but a String (cast above) or Null
      // one as a List literal (round CE: temporal, Boolean, Categorical and
      // Enum columns too; probed in Polars 1.44.1).
      if (value.kind == lit::Kind::kDict || declared == "String" || declared == "Null") return;
      std::optional<lit::Literal> list;
      try { list = lit::list_literal(value.items); } catch (const PolarsSequenceFilterError&) { return; }
      const auto shown = "\n\nThis error occurred in the following expression:\n\t[(col("+quote(key)+")) == ("+lit::lit_display(lit::list_of(list->type),lit::Value::list(list->values))+")]\n";
      if (lit::categorical_dtype(declared) || lit::enum_dtype(declared))
        fail("ComputeError",phase,"cannot compare categorical with "+lit::short_name(lit::list_of(list->type))+shown);
      fail("SchemaError",phase,"could not evaluate comparison between series '"+key+"' of dtype: "+lit::is_in_dtype_name(declared)+" and series 'literal' of dtype: List("+lit::literal_dtype_name(*list)+")"+shown);
    }
    const auto check = lit::check_scalar(key, declared, value, lit::CompareOp::kEq, &categories);
    if (check.stage == lit::ScalarCheck::Stage::kCompute) fail(check.type,phase,check.message+lit::expression_suffix(key,lit::CompareOp::kEq,value));
  }
}

bool matches(const Cell& cell, const EventAttribute& value) {
  if (!cell || null(*cell) || null(value) || collection(value)) return false;
  if (text(*cell) || text(value)) return text(*cell) && text(value) && cell->value == value.value;
  if (date(*cell) || date(value)) {
    // Python: a datetime.date never equals a datetime.datetime.
    const auto day = [](const EventAttribute& v) {
      const auto* native = std::get_if<NativeDate>(&v.typed_value);
      return (native ? native->value : v.value).size() == 10;
    };
    return date(*cell) && date(value) && day(*cell) == day(value) && root_cause_scalar_keys_equal(*cell,value);
  }
  return root_cause_scalar_keys_equal(*cell,value);
}
// Round CW: equal activity literals build equal filter plans, which the
// plan's common-subplan elimination shares. The literal is compared as
// Polars coerced it to the column (probe t3/ident.py): on a Float column a
// bool, int or float is the column's float (1, 1.0 and True share; 0.1 and
// its float32 neighbours share on Float32; -0.0 and 0.0 do not); on an
// integer column a bool is the int it equals, a float keeps its own
// (Float64) compare.
std::string activity_identity(const EventAttribute& v, const std::string& declared) {
  if (v.container == AttributeContainerKind::kScalar && !null(v)) {
    // An aware datetime's literal is its instant in UTC whatever its offset.
    const auto typed = detail::polars_lit::value_of(v);
    if (typed.kind == detail::polars_lit::Kind::kDatetime) {
      const auto literal = detail::polars_lit::literal_datetime(typed);
      return "dt:" + literal.text + ":" + std::to_string(literal.integer);
    }
    const auto s = python::from_attribute(v);
    using K = python::Scalar::Kind;
    const bool number = s.kind == K::kBool || s.kind == K::kInt || s.kind == K::kFloat;
    const auto as_double = [&]() {
      if (s.kind == K::kBool) return s.boolean ? 1.0 : 0.0;
      if (s.kind == K::kFloat) return s.real;
      return std::stod((s.negative ? "-" : "") + s.digits);
    };
    static const std::unordered_set<std::string> integers{"Int8", "Int16", "Int32", "Int64", "Int128", "UInt8", "UInt16", "UInt32", "UInt64"};
    if (number && declared == "Float32") return "f32:" + std::to_string(std::bit_cast<std::uint32_t>(static_cast<float>(as_double())));
    if (number && declared == "Float64") return "f64:" + std::to_string(std::bit_cast<std::uint64_t>(as_double()));
    if (integers.contains(declared)) {
      if (s.kind == K::kBool) return std::string("i:") + (s.boolean ? "1" : "0");
      if (s.kind == K::kInt) return "i:" + std::string(s.negative && s.digits != "0" ? "-" : "") + s.digits;
      if (s.kind == K::kFloat) return "f64:" + std::to_string(std::bit_cast<std::uint64_t>(s.real));
    }
  }
  std::string out = std::to_string(static_cast<int>(v.container)) + ":";
  if (v.container == AttributeContainerKind::kScalar) return out + (null(v) ? std::string("n") : std::to_string(static_cast<int>(v.kind)) + "=" + v.value);
  out += "[";
  for (const auto& child : v.children) out += child.key + "=" + activity_identity(child, std::string()) + ",";
  return out + "]";
}
// The rows one activity filter keeps (select_cases' comparison).
std::size_t activity_rows(const EventTable& source, const Prepared& p, std::size_t at) {
  namespace lit = detail::polars_lit;
  const auto ai = *column(source, p.options.attribute_key);
  const auto categories = category_list(source, ai);
  const auto& value = p.values[at];
  const auto typed = lit::value_of(value);
  const bool column_valid = std::any_of(source.rows.begin(), source.rows.end(), [&](const auto& row) { return !lit::null_cell(row[ai]); });
  std::size_t kept = 0;
  for (const auto& row : source.rows) {
    bool hit = false;
    if (typed.kind == lit::Kind::kDict) hit = lit::struct_matches(row[ai], source.dtypes[ai], lit::struct_model(value, source.dtypes[ai]), column_valid);
    else if (!lit::scalar_literal(typed)) hit = matches(row[ai], value);
    else hit = lit::compare_scalar(row[ai], source.dtypes[ai], typed, lit::CompareOp::kEq, &categories) == 1;
    if (hit) ++kept;
  }
  return kept;
}
// Round CW: an activity filter as a resolved plan prints it (the literal
// coerced to the column where Polars casts it; probe cw/w3).
std::string filter_plan(const EventTable& source, const Prepared& p, std::size_t at) {
  namespace lit = detail::polars_lit;
  const auto ai = *column(source, p.options.attribute_key);
  const auto& declared = source.dtypes[ai];
  const auto& key = p.options.attribute_key.value;
  const auto& value = p.values[at];
  if (null(value)) return "null.repeat([col(" + quote(key) + ").len()])";
  const auto typed = lit::value_of(value);
  if (typed.kind == lit::Kind::kDict) {
    const auto m = lit::struct_model(value, declared);
    if (declared == "String") return "col(" + quote(key) + ") == " + lit::struct_literal(m);
    if (m.cast_column) return lit::struct_cast_text(key, m) + " == " + lit::struct_display(m);
    return "col(" + quote(key) + ") == " + lit::struct_display(m);
  }
  if (!lit::scalar_literal(typed)) {
    try { return "col(" + quote(key) + ") == " + lit::literal_list_display(lit::argument_literal(value)); } catch (const PolarsSequenceFilterError&) {}
    return "col(" + quote(key) + ") == " + literal(value);
  }
  const auto categories = category_list(source, ai);
  return lit::comparison_plan(key, declared, typed, lit::CompareOp::kEq, &categories);
}
void eventually_steps(const EventTable& source, const Prepared& p, bool eager) {
  const auto& o = p.options;
  const auto ai = column(source, o.attribute_key), ci = column(source, o.case_id_key);
  if (!ai || !ci) return;
  std::vector<std::string> ids;
  std::vector<std::size_t> first;
  std::vector<int> groups;
  std::vector<bool> bound;
  for (std::size_t at = 0; at < p.values.size(); ++at) {
    const auto id = activity_identity(p.values[at], source.dtypes[*ai]);
    const auto found = std::find(ids.begin(), ids.end(), id);
    if (found == ids.end()) { ids.push_back(id); first.push_back(at); bound.push_back(null(p.values[at])); groups.push_back(static_cast<int>(ids.size()) - 1); }
    else groups.push_back(static_cast<int>(found - ids.begin()));
  }
  const bool sorting = literal_sort(o.timestamp_key);
  const auto phase = eager ? "apply" : "collect";
  namespace cse = detail::polars_ltl_cse;
  for (const auto& step : cse::eventually_unit_order(groups, *ai == *ci, bound)) {
    if (step.kind == cse::StepKind::kFilter) { compute_query(source, p, eager, first[static_cast<std::size_t>(step.group)]); continue; }
    if (!sorting) continue;
    const auto height = step.group < 0 ? source.rows.size() : activity_rows(source, p, first[static_cast<std::size_t>(step.group)]);
    if (height != 1)
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(height) + " and Series length: 1");
  }
}
struct Selection { std::unordered_set<std::string> keys; std::string deferred_type, deferred_message; };
std::vector<std::size_t> sorted_rows(const EventTable& source, const Prepared& p) {
  std::vector<std::size_t> order(source.rows.size());std::iota(order.begin(),order.end(),0);
  if (!sequence_rule(p.operation)) return order;
  const auto ci=column(source,p.options.case_id_key),ti=column(source,p.options.timestamp_key);
  std::map<std::size_t,EnumRanks> ranks;
  for (const auto index:{ci,ti}) if(index && enum_dtype(source.dtypes[*index])) {
    if(source.categorical_dtypes.empty() || !source.categorical_dtypes[*index]) throw std::invalid_argument("Native Polars LTL Enum ordering requires its category dictionary");
    auto& map=ranks[*index];const auto& values=source.categorical_dtypes[*index]->categories;
    for(std::size_t i=0;i<values.size();++i)map.emplace(values[i].value,i);
    for(const auto& row:source.rows)if(row[*index]&&!null(*row[*index])&&!map.contains(row[*index]->value))throw std::invalid_argument("Native Polars LTL Enum cell is absent from its category dictionary");
  }
  const auto cmp=[&](std::size_t index,std::size_t a,std::size_t b){const auto found=ranks.find(index);return compare(source.rows[a][index],source.rows[b][index],dtype(source.dtypes[index]),found==ranks.end()?nullptr:&found->second);};
  const auto less=[&](auto a,auto b){if(ci){const auto c=cmp(*ci,a,b);if(c)return c<0;}return ti && cmp(*ti,a,b)<0;};
  // Polars' unstable multi-column sort leaves equal rows in its own order (round BX).
  if(ci && ti) polars_sort::sort_by_keys(order,less,source.dtypes[*ci],source.dtypes[*ti]); else std::stable_sort(order.begin(),order.end(),less);
  return order;
}
std::string schema_join_error(const std::string& key,const std::string& left,const std::string& right) {
  return "datatypes of join keys don't match - `"+key+"`: "+left+" on left does not match `"+key+"`: "+right+" on right (and no other type was available to cast to)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\nDF ["+quote(key)+"]; PROJECT */1 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'join'\n\t[2] 'select'\n";
}
// Round CQ: a row's join key: its case cell, or the cells of the columns a
// list of str names (eventually_follows with at most one activity; probe
// cq/q4); "n" when any is null.
std::string row_case_key(const EventTable& source, const Prepared& p, const EventTable::Row& row) {
  const auto& key = p.options.case_id_key;
  if (collection(key) && str_items(key)) {
    std::string out;
    for (const auto& name : list_names(key)) {
      const auto at = static_cast<std::size_t>(std::find(source.columns.begin(), source.columns.end(), name) - source.columns.begin());
      if (at >= source.columns.size()) throw std::invalid_argument("Native Polars LTL list case key names a missing column");
      const auto part = group_key(row[at], source.dtypes[at]);
      if (part == "n") return "n";
      out += part;
      out += '\x1f';
    }
    return out;
  }
  const auto ci = column(source, key);
  return ci ? group_key(row[*ci], source.dtypes[*ci]) : std::string("L");
}
Selection select_cases(const EventTable& source,const Prepared& p) {
  const auto ci=column(source,p.options.case_id_key),ai=column(source,p.options.attribute_key),ri=column(source,p.options.resource_key);
  namespace lit = detail::polars_lit;
  // Round CD: activity filters compare as Polars types col == value.
  const auto categories = ai ? category_list(source,*ai) : std::vector<std::string>{};
  std::vector<lit::Value> typed;for(const auto& v:p.values)typed.push_back(lit::value_of(v));
  const auto typed_original = lit::value_of(p.original);
  // Round CI: a dict matches as the struct model compares it.
  const bool column_valid = ai && std::any_of(source.rows.begin(),source.rows.end(),[&](const auto& row){return !lit::null_cell(row[*ai]);});
  const auto hit=[&](const Cell& cell,const EventAttribute& value,const lit::Value& v){
    if(v.kind==lit::Kind::kDict)return lit::struct_matches(cell,source.dtypes[*ai],lit::struct_model(value,source.dtypes[*ai]),column_valid);
    if(!lit::scalar_literal(v))return matches(cell,value);
    return lit::compare_scalar(cell,source.dtypes[*ai],v,lit::CompareOp::kEq,&categories)==1;
  };
  const auto case_key=[&](std::size_t i){return row_case_key(source,p,source.rows[i]);};
  std::map<std::string,std::vector<std::size_t>> groups;
  for(const auto i:sorted_rows(source,p))groups[case_key(i)].push_back(i);
  Selection selected;const bool positive=truth(p.options.positive);
  for(const auto& [key,rows]:groups) {
    bool keep=false;
    if(p.operation==Op::kDifferentPersons) {
      std::unordered_set<std::string> resources;bool present=false;
      for(const auto i:rows)if(hit(source.rows[i][*ai],p.original,typed_original)){present=true;resources.insert(ri?group_key(source.rows[i][*ri],source.dtypes[*ri]):"literal");}
      keep=present && ((resources.size()>1)==positive);
    } else if(p.operation==Op::kFourEyes) {
      keep=true;
      std::vector<bool> present(p.values.size(),false);
      std::vector<std::unordered_set<std::string>> performers(p.values.size());
      for(std::size_t at=0;at<p.values.size();++at)for(const auto i:rows)if(hit(source.rows[i][*ai],p.values[at],typed[at])) {
        present[at]=true;
        if(source.rows[i][*ri]&&!null(*source.rows[i][*ri]))performers[at].insert(group_key(source.rows[i][*ri],source.dtypes[*ri]));
      }
      for(std::size_t a=0;a<p.values.size();++a)for(std::size_t b=a+1;b<p.values.size();++b) {
        bool overlap=false;
        const auto& small=performers[a].size()<performers[b].size()?performers[a]:performers[b];
        const auto& large=performers[a].size()<performers[b].size()?performers[b]:performers[a];
        for(const auto& resource:small)if(large.contains(resource)){overlap=true;break;}
        keep=keep && present[a] && present[b] && (overlap!=positive);
      }
    } else if(p.operation==Op::kNext) {
      bool match=false;
      if(p.original.container==AttributeContainerKind::kList && p.values.size()<=rows.size())
        for(std::size_t start=0;start+p.values.size()<=rows.size();++start) {
          bool found=true;for(std::size_t at=0;at<p.values.size();++at)found=found&&matches(source.rows[rows[start+at]][*ai],p.values[at]);
          if(found){match=true;break;}
        }
      // The public Python-side selection excludes null group IDs before its join.
      if(match && key!="n")selected.keys.insert(key);
      continue;
    } else {
      bool match=true;
      std::vector<std::size_t> first(p.values.size(),rows.size()),last(p.values.size(),0);
      for(std::size_t at=0;at<p.values.size();++at)for(std::size_t i=0;i<rows.size();++i)if(hit(source.rows[rows[i]][*ai],p.values[at],typed[at])){first[at]=std::min(first[at],i);last[at]=i;}
      for(const auto pos:first)match=match&&pos<rows.size();
      for(std::size_t at=0;at+1<p.values.size();++at) {
        match=match&&first[at]<last[at+1];
      }
      if(key=="n" || key=="fNaN")match=false;
      keep=match==positive;
    }
    if(keep && (key!="n" || p.operation==Op::kEventuallyFollows))selected.keys.insert(key);
  }
  if(p.operation==Op::kNext && ci) {
    const auto& declared=source.dtypes[*ci];const bool empty=selected.keys.empty();
    if((empty && declared!="Null") || categorical_dtype(declared) || enum_dtype(declared)) {
      const auto left=detail::polars_lit::short_dtype(declared);
      selected.deferred_type="SchemaError";selected.deferred_message=schema_join_error(p.options.case_id_key.value,left,empty?"null":"str");
    }
    if(!positive && selected.deferred_type.empty()) {
      std::unordered_set<std::string> complement;
      for(const auto& [key,rows]:groups)if(!selected.keys.contains(key))complement.insert(key);
      selected.keys=std::move(complement);
    }
  }
  return selected;
}
EventTable project(const EventTable& source,const Prepared& p,const Selection& selected,bool eager) {
  if(!selected.deferred_type.empty())fail(selected.deferred_type,"collect",selected.deferred_message);
  EventTable result=source;result.rows.clear();if(result.index)result.index->labels.clear();
  auto order=std::vector<std::size_t>(source.rows.size());std::iota(order.begin(),order.end(),0);
  // Round DC: a lazy frame keeps the sorted order when its one activity is
  // None and the anti join runs (col == None filters to an empty frame; CH
  // set probe case 182, probe dc/ef2); two or more activities do not.
  const bool null_anti=p.values.size()==1&&null(p.values.front())&&!truth(p.options.positive);
  if((eager||null_anti) && p.operation==Op::kEventuallyFollows)order=sorted_rows(source,p);
  for(const auto i:order) {
    const auto key=row_case_key(source,p,source.rows[i]);
    if(selected.keys.contains(key)){result.rows.push_back(source.rows[i]);if(result.index)result.index->labels.push_back(source.index->labels[i]);}
  }
  return result;
}
// Round CO: a literal case key puts every row in one case that every
// selection joins on the literal (probe ltl2): four_eyes keeps nothing
// (positive) or everything, attr_value_different_persons everything,
// eventually_follows with at most one activity everything (positive) or
// nothing; a Null literal turns each of these round. Every inner join
// appends the literal column; the anti join (eventually_follows negative)
// does not.
void append_literal(EventTable& table, const LiteralKey& key) {
  if (std::find(table.columns.begin(), table.columns.end(), "literal") != table.columns.end())
    throw std::invalid_argument("Native Polars LTL literal-case projection cannot overwrite an existing literal column");
  table.columns.push_back("literal");
  table.dtypes.push_back(key.item.dtype);
  if (!table.categorical_dtypes.empty()) table.categorical_dtypes.emplace_back(std::nullopt);
  for (auto& row : table.rows) row.push_back(key.item.cell);
}
bool literal_extra(const Prepared& p) { return p.operation != Op::kEventuallyFollows || truth(p.options.positive); }
// Round CS: two eventually_follows activities on one row keep the same
// outcome: each ordered-pair selection of the literal alone has one row
// whatever its filter kept (CK bank 2026111671 case 477).
EventTable literal_result(const EventTable& source, const Prepared& p, const LiteralKey& key) {
  const bool positive = truth(p.options.positive), null = key.item.null;
  bool keep = !null;
  if (p.operation == Op::kFourEyes) keep = !positive && !null;
  if (p.operation == Op::kEventuallyFollows) keep = positive != null;
  EventTable result = source;
  if (!keep) { result.rows.clear(); if (result.index) result.index->labels.clear(); }
  if (literal_extra(p)) append_literal(result, key);
  return result;
}
EventTable collect(const EventTable& source,const Prepared& p,bool eager) {
  validate_schema(source,p,eager);validate_queries(source,p,eager);
  if(p.operation==Op::kFourEyes && p.values.size()<2)return source;
  if(p.operation!=Op::kNext)if(const auto key=literal_key(p.options.case_id_key))return literal_result(source,p,*key);
  return project(source,p,select_cases(source,p),eager);
}
}  // namespace

EventTable invoke_polars_ltl_filter(const EventTable& source,const PolarsLtlFilterCall& call) {
  return collect(source,prepare(call,source.columns),true);
}
LazyEventTable invoke_polars_ltl_filter(const LazyEventTable& source,const PolarsLtlFilterCall& call) {
  const auto p=prepare(call,source.columns());
  if(p.operation==Op::kNext) {
    const auto snapshot=source.materialize();validate_schema(snapshot,p,false);
    const auto selected=select_cases(snapshot,p);
    return LazyEventTable(source.columns(),[source,p,selected](const auto& consume){const auto output=project(source.materialize(),p,selected,false);for(const auto& row:output.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,source.dtypes(),source.index(),source.categorical_dtypes());
  }
  auto columns=source.columns();auto dtypes=source.dtypes();auto categories=source.categorical_dtypes();
  // Round CO: the literal column a literal case key appends (the plan's
  // schema; with fewer than two four_eyes activities nothing joins).
  std::optional<LiteralKey> literal;
  try { literal = literal_key(p.options.case_id_key); } catch (const std::invalid_argument&) {}
  if(literal && literal_extra(p) && (p.operation!=Op::kFourEyes || p.values.size()>=2)) {
    columns.push_back("literal");dtypes.push_back(literal->item.dtype);if(!categories.empty())categories.emplace_back(std::nullopt);
  }
  return LazyEventTable(std::move(columns),[source,p](const auto& consume){const auto output=collect(source.materialize(),p,false);for(const auto& row:output.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,std::move(dtypes),source.index(),std::move(categories));
}

#define MINERA_LTL_FRIENDLY(NAME, OPERATION) \
EventTable NAME(const EventTable& source,const EventAttribute& values,const PolarsLtlFilterOptions& options){PolarsLtlFilterCall call;call.operation=Op::OPERATION;call.options=options;call.values=values;return invoke_polars_ltl_filter(source,call);} \
LazyEventTable NAME(const LazyEventTable& source,const EventAttribute& values,const PolarsLtlFilterOptions& options){PolarsLtlFilterCall call;call.operation=Op::OPERATION;call.options=options;call.values=values;return invoke_polars_ltl_filter(source,call);}
MINERA_LTL_FRIENDLY(filter_polars_ltl_eventually_follows,kEventuallyFollows)
MINERA_LTL_FRIENDLY(filter_polars_ltl_next,kNext)
MINERA_LTL_FRIENDLY(filter_polars_ltl_four_eyes,kFourEyes)
MINERA_LTL_FRIENDLY(filter_polars_ltl_different_persons,kDifferentPersons)
#undef MINERA_LTL_FRIENDLY
}  // namespace minera
