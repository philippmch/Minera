#include "minera/polars_ltl_filter.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
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


Dtype dtype(const std::string& value) {
  if (value == "String" || categorical_dtype(value)) return Dtype::kString;
  if (enum_dtype(value)) return Dtype::kEnum;
  if (value == "Int64" || value == "UInt64" || value == "UInt32" || value == "Int32") return Dtype::kInteger;
  if (value == "Float64") return Dtype::kFloat;
  if (value == "Boolean") return Dtype::kBoolean;
  if (value == "Null") return Dtype::kNull;
  if (value == "Date" || value.starts_with("Datetime(time_unit='us', time_zone=")) return Dtype::kDate;
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
  fail("TypeError", "apply", "'" + python_type(value) + "' object is not iterable");
}
EventAttribute item(const EventAttribute& value, std::size_t at) {
  if (value.container == AttributeContainerKind::kContainer) fail("KeyError", "apply", std::to_string(at));
  if (text(value) || value.container == AttributeContainerKind::kList || value.container == AttributeContainerKind::kTuple) {
    const auto values = iter(value);
    if (at < values.size()) return values[at];
    fail("IndexError", "apply", (text(value) ? "string" : python_type(value)) + " index out of range");
  }
  fail("TypeError", "apply", "'" + python_type(value) + "' object is not subscriptable");
}

bool sequence_rule(Op op) { return op == Op::kEventuallyFollows || op == Op::kNext; }
std::string function_name(Op op) {
  return op == Op::kEventuallyFollows ? "eventually_follows" : op == Op::kNext ? "A_next_B_next_C" :
         op == Op::kFourEyes ? "four_eyes_principle" : "attr_value_different_persons";
}
std::string group_key(const Cell& value, const std::string& declared) {
  if (!value || null(*value) || declared == "Null") return "n";
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return "s" + value->value;
  if (declared == "Float64") {
    const auto n = number(*value);
    return std::isnan(n) ? "fNaN" : n == 0 ? "f0" : "f" + std::to_string(std::bit_cast<std::uint64_t>(n));
  }
  if (declared == "Boolean" || declared == "Int64" || declared == "UInt64" || declared == "UInt32" || declared == "Int32") return "i" + exact(*value).decimal();
  if (declared == "Date" || declared.starts_with("Datetime(")) {
    const auto* stored = std::get_if<NativeDate>(&value->typed_value);
    const auto parsed = parse_timestamp_microseconds(stored ? stored->value : value->value);
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
  if (real(value)) return "dyn float: " + real_text(number(value));
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
std::string render(std::string_view pattern, const EventTable& source, const Prepared& p,
                   const std::string& expression = "") {
  const auto& o = p.options;
  const auto value = [&](std::size_t i) { return i < p.values.size() ? literal(p.values[i]) : std::string("null"); };
  const std::map<std::string_view, std::string> substitutions = {
    {"FRAME", frame_plan(source)}, {"CASE_Q", quote(o.case_id_key.value)},
    {"ATTRIBUTE_Q", quote(o.attribute_key.value)}, {"TIME_Q", quote(o.timestamp_key.value)},
    {"RESOURCE_Q", quote(o.resource_key.value)}, {"RESOURCE2_Q", quote(o.resource_key.value + "_2")},
    {"LITERAL", integer(o.case_id_key) ? exact(o.case_id_key).decimal() : "0"},
    {"FIRST", value(0)}, {"SECOND", value(1)}, {"LAST", p.values.empty() ? "null" : value(p.values.size() - 1)},
    {"PAIR_A", p.values.size() < 2 ? "null" : value(p.values.size() - 2)},
    {"PAIR_B", p.values.empty() ? "null" : value(p.values.size() - 1)}, {"EXPR", expression}
  };
  std::string out;
  for (std::size_t at = 0; at < pattern.size();) {
    if (pattern[at] != '%') { out += pattern[at++]; continue; }
    const auto end = pattern.find('%', at + 1);
    if (end == std::string_view::npos) throw std::logic_error("Unclosed native LTL diagnostic placeholder");
    const auto found = substitutions.find(pattern.substr(at + 1, end - at - 1));
    if (found == substitutions.end()) throw std::logic_error("Unknown native LTL diagnostic placeholder");
    out += found->second; at = end + 1;
  }
  return out;
}
std::string missing_message(const EventTable& source, const std::string& key, bool index = false) {
  std::string out = "unable to find column " + debug_quote(key) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) { if (i) out += ", "; out += debug_quote(source.columns[i]); }
  if (index) { if (!source.columns.empty()) out += ", "; out += "\"_index\""; }
  out += ']';
  if (key == "case:concept:name" && std::find(source.columns.begin(), source.columns.end(), "concept:name") != source.columns.end()) out += "\n\nDid you mean \"concept:name\"?";
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
  if (value.container == AttributeContainerKind::kList) {
    if (value.children.empty()) return {};
    fail("TypeError", "apply", "list indices must be integers or slices, not str");
  }
  if (text(value)) fail("TypeError", "apply", "string indices must be integers, not 'str'");
  fail("TypeError", "apply", "'" + python_type(value) + "' object is not iterable");
}
Prepared prepare(const PolarsLtlFilterCall& call) {
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
  if (sequence_rule(out.operation) && null(o.case_id_key))
    fail("InvalidOperationError", "apply", "At least one of `partition_by` and `order_by` must be specified in `over`");
  if (out.operation == Op::kDifferentPersons) out.values = {out.original};
  else {
    if (out.operation != Op::kEventuallyFollows && !text(out.original) && !collection(out.original))
      fail("TypeError", "apply", "object of type '" + python_type(out.original) + "' has no len()");
    out.values = iter(out.original);
    if (out.operation != Op::kNext && out.values.size() > 1) for (std::size_t i = 0; i < out.values.size(); ++i) (void)item(out.original, i);
  }
  if (out.operation != Op::kNext && (out.operation != Op::kFourEyes || out.values.size() > 1)) {
    if (!text(o.attribute_key) && !collection(o.attribute_key)) invalid_col(o.attribute_key);
    if (out.operation == Op::kFourEyes) {
      if (!text(o.resource_key) && !collection(o.resource_key)) invalid_col(o.resource_key);
      if (collection(o.resource_key)) fail("TypeError", "apply", "can only concatenate list (not \"str\") to list");
    }
    if (out.operation == Op::kDifferentPersons && (out.original.container == AttributeContainerKind::kSet || out.original.container == AttributeContainerKind::kFrozenSet))
      fail("TypeError", "apply", "cannot create expression literal for value of type set.\n\nHint: Pass `allow_object=True` to accept any value and create a literal of type Object.");
  }
  if (!sequence_rule(out.operation) && null(o.case_id_key) && (out.operation != Op::kFourEyes || out.values.size() > 1))
    fail("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
  return out;
}

std::string missing_tail(const EventTable& source, const Prepared& p, bool eager, bool sorting, bool attribute) {
  if (eager) return p.operation == Op::kDifferentPersons && !attribute ? render(kDifferentEagerGroupTail, source, p) : "";
  if (p.operation == Op::kEventuallyFollows) return render(sorting ? kEventSortTail : kEventAttributeTail, source, p);
  if (p.operation == Op::kNext) return sorting ? render(kNextSortTail, source, p) : "";
  if (p.operation == Op::kFourEyes) return render(attribute ? kFourAttributeTail : kFourMissingTail, source, p);
  return render(attribute ? kDifferentAttributeTail : kDifferentMissingTail, source, p);
}
void validate_schema(const EventTable& source, const Prepared& p, bool eager) {
  source.validate();
  if (source.dtypes.size() != source.columns.size()) throw std::invalid_argument("Native Polars LTL requires every column dtype");
  const auto& o = p.options;
  const auto phase = eager || p.operation == Op::kNext ? "apply" : "collect";
  if (p.operation == Op::kFourEyes && p.values.size() < 2) return;
  const auto required = [&](const EventAttribute& role, bool sorting, bool attribute) {
    if (!text(role) || column(source, role)) return;
    if (p.operation == Op::kNext && attribute && !eager) fail("ColumnNotFoundError", "apply", debug_quote(role.value) + " not found");
    const bool index = p.operation == Op::kEventuallyFollows && attribute;
    fail("ColumnNotFoundError", phase, missing_message(source, role.value, index) + missing_tail(source, p, eager, sorting, attribute));
  };
  if (sequence_rule(p.operation)) {
    required(o.case_id_key, true, false); required(o.timestamp_key, true, false);
    if ((collection(o.timestamp_key) || (!text(o.timestamp_key) && !null(o.timestamp_key))) ||
        (p.operation == Op::kNext && !text(o.case_id_key)))
      fail("ShapeError", phase, "sort expressions must have same length as DataFrame, got DataFrame height: " + std::to_string(source.rows.size()) + " and Series length: 1");
    if (p.operation == Op::kNext && eager) fail("AttributeError", "apply", "'DataFrame' object has no attribute 'collect'");
  }
  if (p.operation == Op::kNext) {
    if (null(o.attribute_key)) fail("TypeError", "apply", "cannot select columns using key of type 'NoneType': None");
    if (collection(o.attribute_key)) fail("AttributeError", "apply", "'DataFrame' object has no attribute 'to_list'");
    if (integer(o.attribute_key)) {
      std::size_t height = 0;
      if (text(o.case_id_key)) {
        const auto ci = *column(source, o.case_id_key);std::map<std::string,std::size_t> groups;
        for (const auto& row : source.rows) ++groups[group_key(row[ci],source.dtypes[ci])];
        if (!source.rows.empty()) height=groups[group_key(source.rows.front()[ci],source.dtypes[ci])];
      }
      fail("IndexError", "apply", "index " + exact(o.attribute_key).decimal() + " is out of bounds for DataFrame of height " + std::to_string(height));
    }
  }
  required(o.attribute_key, false, true);
  if (!sequence_rule(p.operation)) { required(o.case_id_key, false, false); required(o.resource_key, false, false); }
  if (collection(o.case_id_key)) {
    fail("InvalidOperationError", phase, render(p.operation == Op::kEventuallyFollows ? kEventEmptyJoin : p.operation == Op::kFourEyes ? kFourEmptyJoin : kDifferentEmptyJoin,source,p));
  }
  if (p.operation != Op::kNext && collection(o.attribute_key)) {
    fail("ComputeError", phase, "The predicate expanded to zero expressions. This may for example be caused by a regex not matching column names or a column dtype match not hitting any dtypes in the DataFrame" + missing_tail(source,p,eager,false,true));
  }
  if (integer(o.case_id_key) && p.operation != Op::kDifferentPersons) {
    if (p.values.size()!=3) throw std::invalid_argument("Native Polars LTL literal-case diagnostic plan currently covers three-operand predicates");
    fail("DuplicateError", phase, render(p.operation==Op::kEventuallyFollows?kEventLiteralJoin:kFourLiteralJoin,source,p));
  }
}
void validate_queries(const EventTable& source, const Prepared& p, bool eager) {
  if (p.operation == Op::kNext || (p.operation == Op::kFourEyes && p.values.size() < 2)) return;
  const auto ai = *column(source,p.options.attribute_key);
  if (dtype(source.dtypes[ai]) != Dtype::kString && dtype(source.dtypes[ai]) != Dtype::kEnum) return;
  const auto phase=eager?"apply":"collect";
  for (const auto& query:p.values) {
    const auto expr="col("+quote(p.options.attribute_key.value)+") == "+literal(query);
    if (integer(query) || real(query)) {
      const auto kind=real(query)?"f64":exact(query)>=TokenInteger(-2147483648LL)&&exact(query)<=TokenInteger(2147483647LL)?"i32":"i64";
      fail("ComputeError",phase,"cannot compare string with numeric type ("+std::string(kind)+")\n\nThis error occurred in the following expression:\n\t[(col("+quote(p.options.attribute_key.value)+")) == ("+literal(query)+")]\n");
    }
    if ((collection(query) && query.container != AttributeContainerKind::kContainer) || date(query)) {
      const auto prefix=date(query)?"cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)":"cannot cast List type (inner: 'String', to: 'String')";
      const auto expression=expr+(date(query)?"":".cast(String)");
      fail("InvalidOperationError",phase,std::string(prefix)+(eager?"":render(p.operation==Op::kEventuallyFollows?kEventFilterTail:kDifferentFilterTail,source,p,expression)));
    }
  }
}

bool matches(const Cell& cell, const EventAttribute& value) {
  if (!cell || null(*cell) || null(value) || collection(value)) return false;
  if (text(*cell) || text(value)) return text(*cell) && text(value) && cell->value == value.value;
  if (date(*cell) || date(value)) return date(*cell) && date(value) && root_cause_scalar_keys_equal(*cell,value);
  return root_cause_scalar_keys_equal(*cell,value);
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
  std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){if(ci){const auto c=cmp(*ci,a,b);if(c)return c<0;}return ti && cmp(*ti,a,b)<0;});
  return order;
}
std::string schema_join_error(const std::string& key,const std::string& left,const std::string& right) {
  return "datatypes of join keys don't match - `"+key+"`: "+left+" on left does not match `"+key+"`: "+right+" on right (and no other type was available to cast to)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\nDF ["+quote(key)+"]; PROJECT */1 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'join'\n\t[2] 'select'\n";
}
Selection select_cases(const EventTable& source,const Prepared& p) {
  const auto ci=column(source,p.options.case_id_key),ai=column(source,p.options.attribute_key),ri=column(source,p.options.resource_key);
  const auto case_key=[&](std::size_t i){return ci?group_key(source.rows[i][*ci],source.dtypes[*ci]):"i"+exact(p.options.case_id_key).decimal();};
  std::map<std::string,std::vector<std::size_t>> groups;
  for(const auto i:sorted_rows(source,p))groups[case_key(i)].push_back(i);
  Selection selected;const bool positive=truth(p.options.positive);
  for(const auto& [key,rows]:groups) {
    bool keep=false;
    if(p.operation==Op::kDifferentPersons) {
      std::unordered_set<std::string> resources;bool present=false;
      for(const auto i:rows)if(matches(source.rows[i][*ai],p.original)){present=true;resources.insert(ri?group_key(source.rows[i][*ri],source.dtypes[*ri]):"literal");}
      keep=present && ((resources.size()>1)==positive);
    } else if(p.operation==Op::kFourEyes) {
      keep=true;
      std::vector<bool> present(p.values.size(),false);
      std::vector<std::unordered_set<std::string>> performers(p.values.size());
      for(std::size_t at=0;at<p.values.size();++at)for(const auto i:rows)if(matches(source.rows[i][*ai],p.values[at])) {
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
      for(std::size_t at=0;at<p.values.size();++at)for(std::size_t i=0;i<rows.size();++i)if(matches(source.rows[rows[i]][*ai],p.values[at])){first[at]=std::min(first[at],i);last[at]=i;}
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
      const auto left=declared=="String"?"str":categorical_dtype(declared)?"cat":enum_dtype(declared)?"enum":declared=="Int64"?"i64":declared=="UInt64"?"u64":declared=="Float64"?"f64":declared=="Boolean"?"bool":"null";
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
  const auto ci=column(source,p.options.case_id_key);EventTable result=source;result.rows.clear();if(result.index)result.index->labels.clear();
  auto order=std::vector<std::size_t>(source.rows.size());std::iota(order.begin(),order.end(),0);
  if(eager && p.operation==Op::kEventuallyFollows)order=sorted_rows(source,p);
  for(const auto i:order) {
    const auto key=ci?group_key(source.rows[i][*ci],source.dtypes[*ci]):"i"+exact(p.options.case_id_key).decimal();
    if(selected.keys.contains(key)){result.rows.push_back(source.rows[i]);if(result.index)result.index->labels.push_back(source.index->labels[i]);}
  }
  if(integer(p.options.case_id_key) && p.operation==Op::kDifferentPersons) {
    if(std::find(result.columns.begin(),result.columns.end(),"literal")!=result.columns.end())throw std::invalid_argument("Native Polars LTL literal-case projection cannot overwrite an existing literal column");
    result.columns.push_back("literal");auto value=p.options.case_id_key;value.key="literal";
    const auto n=exact(value);result.dtypes.push_back(n>=TokenInteger(-2147483648LL)&&n<=TokenInteger(2147483647LL)?"Int32":n<=TokenInteger(std::numeric_limits<std::int64_t>::max())?"Int64":"UInt64");
    if(!result.categorical_dtypes.empty())result.categorical_dtypes.emplace_back(std::nullopt);
    for(auto& row:result.rows)row.push_back(value);
  }
  return result;
}
EventTable collect(const EventTable& source,const Prepared& p,bool eager) {
  validate_schema(source,p,eager);validate_queries(source,p,eager);
  if(p.operation==Op::kFourEyes && p.values.size()<2)return source;
  return project(source,p,select_cases(source,p),eager);
}
}  // namespace

EventTable invoke_polars_ltl_filter(const EventTable& source,const PolarsLtlFilterCall& call) {
  return collect(source,prepare(call),true);
}
LazyEventTable invoke_polars_ltl_filter(const LazyEventTable& source,const PolarsLtlFilterCall& call) {
  const auto p=prepare(call);
  if(p.operation==Op::kNext) {
    const auto snapshot=source.materialize();validate_schema(snapshot,p,false);
    const auto selected=select_cases(snapshot,p);
    return LazyEventTable(source.columns(),[source,p,selected](const auto& consume){const auto output=project(source.materialize(),p,selected,false);for(const auto& row:output.rows)consume(row);},source.attributes(),source.metadata(),std::nullopt,source.dtypes(),source.index(),source.categorical_dtypes());
  }
  auto columns=source.columns();auto dtypes=source.dtypes();auto categories=source.categorical_dtypes();
  if(integer(p.options.case_id_key) && p.operation==Op::kDifferentPersons) {
    columns.push_back("literal");const auto n=exact(p.options.case_id_key);dtypes.push_back(n>=TokenInteger(-2147483648LL)&&n<=TokenInteger(2147483647LL)?"Int32":n<=TokenInteger(std::numeric_limits<std::int64_t>::max())?"Int64":"UInt64");if(!categories.empty())categories.emplace_back(std::nullopt);
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
