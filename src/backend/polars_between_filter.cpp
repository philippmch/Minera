#include "minera/polars_between_filter.hpp"
#include "polars_datetime.hpp"
#include "polars_sort.hpp"
#include "polars_column_hint.hpp"
#include "polars_list_literal.hpp"
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
void validate_anchor(const EventAttribute& value) {
  if (!collection(value)) return;
  const auto values = anchors(value);
  const auto first = std::find_if(values.begin(), values.end(), [](const auto& item) { return !null(item); });
  if (first == values.end()) return;
  if (text(*first)) {
    for (const auto& item : values) {
      if (!null(item) && !text(item))
        fail("TypeError", "apply", "unexpected value while building Series of type String; found value of type " +
             std::string(integer(item) ? "Int64" : boolean(item) ? "Boolean" : "Float64") + ": " +
             (integer(item) ? exact(item).decimal() : boolean(item) ? (static_cast<bool>(exact(item)) ? "true" : "false") : real_text(number(item))) +
             "\n\nThis error occurred with the following context stack:\n\t[1] while constructing Series ''\n\n\nHint: Try setting `strict=False` to allow passing data with mixed types.");
    }
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
  if (value == "Int64" || value == "UInt64" || value == "UInt32") return Dtype::kInteger;
  if (value == "Float64") return Dtype::kFloat;
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
  if (!collection(value) && !text(value) && !null(value)) {
    std::string scalar;
    if (date(value)) {
      const auto* native = std::get_if<NativeDate>(&value.typed_value);
      auto text = native ? native->value : value.value;
      if (text.size() > 10) text[10] = ' ';
      scalar = text.size() > 19 ? text.substr(0, 19) + " UTC" : text;
    } else {
      scalar = std::string(integer(value) ? "dyn int: " : real(value) ? "dyn float: " : "") + literal(value);
    }
    return '[' + scalar + (resolved ? "]" : ".implode()]");
  }
  std::string output = "[[";
  const auto values = anchors(value);
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) output += ", ";
    output += literal(values[i]);
  }
  return output + "]]";
}
std::string anchor_dtype(const EventAttribute& value) {
  const auto values = anchors(value);
  const auto first = std::find_if(values.begin(), values.end(), [](const auto& item) { return !null(item); });
  if (first == values.end()) return "Null";
  if (text(*first)) return "String";
  if (boolean(*first)) return "Boolean";
  if (integer(*first)) return collection(value) ? "Int64" : "Unknown(Int(" + exact(*first).decimal() + "))";
  if (real(*first)) return collection(value) ? "Float64" : "Unknown(Float)";
  return detail::polars_literal_dtype(*first);
}
bool compatible(const EventAttribute& query, Dtype kind) {
  for (const auto& value : anchors(query)) {
    if (null(value) || kind == Dtype::kNull) continue;
    if ((kind == Dtype::kString || kind == Dtype::kEnum) && text(value)) continue;
    if (kind == Dtype::kDate && date(value)) continue;
    if ((kind == Dtype::kInteger || kind == Dtype::kFloat || kind == Dtype::kBoolean) &&
        (integer(value) || real(value) || boolean(value))) continue;
    return false;
  }
  return true;
}
void validate_membership(const EventTable& source, std::size_t at, const std::string& key,
                         const EventAttribute& act1, const EventAttribute& act2, const std::string& phase) {
  const auto kind = dtype(source.dtypes[at]);
  // The second anchor resolves last and is reported first.
  const EventAttribute* incompatible = !compatible(act2, kind) ? &act2 : !compatible(act1, kind) ? &act1 : nullptr;
  if (!incompatible) return;
  auto data_type = source.dtypes[at];
  if (polars_datetime::datetime(data_type)) data_type = polars_datetime::display(data_type);
  std::string message = "'is_in' cannot check for List(" + anchor_dtype(*incompatible) + ") values in " + data_type + " data";
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'select' <---\n WITH_COLUMNS:\n [col(" +
             quote(source.columns[at]) + ").is_in(" + anchor_expression(act1, incompatible == &act2) + ").alias(\"_is_act1\"), col(" +
             quote(source.columns[at]) + ").is_in(" + anchor_expression(act2) + ").alias(\"_is_act2\")] \n" +
             indexed_plan(source, key, "  ") + stack(false, true);
  fail("InvalidOperationError", phase, message);
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
                        const EventAttribute& act1, const EventAttribute& act2,
                        const EventAttribute& separator, const std::string& phase) {
  if (!collection(separator) || separator.container == AttributeContainerKind::kContainer) return;
  const auto expression = anchor_expression(separator);
  auto message = interpolate(kSeparatorListDiagnostic, {
      {"(inner: 'String',", "(inner: '" + detail::polars_list_literal(separator).inner + "',"},
      {"%CASE%", quote(key)}, {"%ACTIVITY%", quote(activity)},
      {"%ANCHOR1%", anchor_expression(act1)}, {"%ANCHOR2%", anchor_expression(act2)},
      {"%SEPARATOR%", expression.substr(1, expression.size() - 2)}, {"%FRAME%", frame_plan(source)}});
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
bool matches(const Cell& candidate, const EventAttribute& query, Dtype kind) {
  if (!candidate || null(*candidate) || kind == Dtype::kNull) return false;
  for (const auto& target : anchors(query)) {
    if (null(target)) continue;
    if ((kind == Dtype::kString || kind == Dtype::kEnum) && candidate->value == target.value) return true;
    if (kind == Dtype::kDate && compare(candidate, target, kind) == 0) return true;
    if (kind == Dtype::kFloat || real(target)) {
      const auto a = number(*candidate), b = number(target);
      if ((std::isnan(a) && std::isnan(b)) || a == b) return true;
    } else if ((kind == Dtype::kInteger || kind == Dtype::kBoolean) && exact(*candidate) == exact(target)) return true;
  }
  return false;
}
std::string string_cast(const EventAttribute& value, const std::string& declared = "") {
  if (declared == "String" || categorical_dtype(declared) || enum_dtype(declared)) return value.value;
  if (text(value)) return value.value;
  if (boolean(value)) return static_cast<bool>(exact(value)) ? "true" : "false";
  if (integer(value)) return exact(value).decimal();
  if (real(value)) return real_text(number(value), true);
  if (date(value)) {
    auto output = std::get_if<NativeDate>(&value.typed_value) ? std::get<NativeDate>(value.typed_value).value : value.value;
    if (declared.starts_with("Datetime(")) std::replace(output.begin(), output.end(), 'T', ' ');
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
  throw std::invalid_argument("Unsupported native Polars between string cast");
}
EventTable collect(const EventTable& source, const EventAttribute& act1, const EventAttribute& act2,
                   const PolarsBetweenOptions& options, const std::string& phase) {
  // Python builds the anchor and separator literals before Polars runs.
  for (const auto* value : {&act1, &act2, &options.subcase_concat_str})
    if (collection(*value) && value->container != AttributeContainerKind::kContainer)
      if (const auto error = detail::polars_list_literal(*value).error) fail("TypeError", phase, *error);
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
  const auto case_kind = dtype(source.dtypes[case_at]), time_kind = dtype(source.dtypes[time_at]), activity_kind = dtype(source.dtypes[activity_at]);
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
  validate_membership(source, activity_at, key, act1, act2, phase);
  validate_separator(source, key, activity, act1, act2, options.subcase_concat_str, phase);
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
        if (matches(row[activity_at], act1, activity_kind)) start = i;
        if (start && matches(row[activity_at], act2, activity_kind)) {
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
