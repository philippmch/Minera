#include "minera/polars_attribute_repetition_filter.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

using Scalar = PolarsAttributeRepetitionScalar;
enum class Dtype { kString, kInteger, kFloat, kBoolean, kNull, kDate, kDatetime };

[[noreturn]] void error(std::string type, std::string phase, std::string message) {
  throw PolarsAttributeRepetitionError(std::move(type), std::move(phase), std::move(message));
}

std::string scalar_type(const Scalar& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) return "NoneType";
  if (std::holds_alternative<bool>(value)) return "bool";
  if (std::holds_alternative<TokenInteger>(value)) return "int";
  if (std::holds_alternative<double>(value)) return "float";
  return "str";
}

void validate_apply(const PolarsAttributeRepetitionOptions& options) {
  if (!std::holds_alternative<std::string>(options.attribute_key))
    error("TypeError", "apply", "invalid input for `col`\n\nExpected `str` or `DataType`, got '" +
          scalar_type(options.attribute_key) + "'.");
  if (std::holds_alternative<std::nullptr_t>(options.case_id_key))
    error("ValueError", "apply", "must specify `on` OR `left_on` and `right_on`");
  if (!std::holds_alternative<std::string>(options.case_id_key))
    throw std::invalid_argument("Native Polars repetition companion requires a string case selector");
}

std::string quoted_raw(const std::string& value) { return "\"" + value + "\""; }

std::string quoted_debug(const std::string& value) {
  std::string result = "\"";
  for (const unsigned char c : value) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\0': result += "\\0"; break;
      default: result += static_cast<char>(c);
    }
  }
  return result + '"';
}

std::string frame_plan(const EventTable& source) {
  std::string result = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, source.columns.size()); ++i) {
    if (i) result += ", ";
    result += quoted_raw(source.columns[i]);
  }
  if (source.columns.size() > 4) result += ", ...";
  return result + "]; PROJECT */" + std::to_string(source.columns.size()) + " COLUMNS";
}

std::string real_text(double number) {
  if (std::isnan(number)) return "NaN";
  if (std::isinf(number)) return number < 0 ? "-inf" : "inf";
  char buffer[128];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number);
  if (result.ec != std::errc{}) throw std::invalid_argument("Invalid native Polars numeric text");
  return std::string(buffer, result.ptr);
}

bool null(const EventAttribute& value) { return value.is_null || is_native_na_attribute(value); }
bool date(const EventAttribute& value) {
  return value.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}
bool real(const EventAttribute& value) {
  return value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value);
}
bool integer(const EventAttribute& value) {
  return value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
         std::holds_alternative<std::uint64_t>(value.typed_value);
}
bool boolean(const EventAttribute& value) {
  return value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value);
}
bool text(const EventAttribute& value) {
  return !null(value) && !date(value) && !real(value) && !integer(value) && !boolean(value);
}
TokenInteger exact_integer(const EventAttribute& value) {
  if (const auto* stored = std::get_if<std::int64_t>(&value.typed_value)) return TokenInteger(*stored);
  if (const auto* stored = std::get_if<std::uint64_t>(&value.typed_value)) return TokenInteger(*stored);
  if (const auto* stored = std::get_if<bool>(&value.typed_value)) return TokenInteger(*stored ? 1 : 0);
  if (boolean(value)) return TokenInteger(value.value == "true" || value.value == "1" ? 1 : 0);
  return TokenInteger(value.value);
}
double number(const EventAttribute& value) {
  return integer(value) || boolean(value) ? static_cast<double>(exact_integer(value)) : native_attribute_number(value);
}
std::string literal(const EventAttribute& target, bool dynamic = false) {
  if (null(target)) return "null";
  if (boolean(target)) return static_cast<bool>(exact_integer(target)) ? "true" : "false";
  if (integer(target)) return (dynamic ? "dyn int: " : "") + exact_integer(target).decimal();
  if (real(target)) return (dynamic ? "dyn float: " : "") + real_text(number(target));
  return quoted_raw(target.value);
}

std::string trace(const EventTable& source, const std::string& attribute,
                   const EventAttribute& target, bool include_filter, bool temporal) {
  std::string result = "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\n";
  if (include_filter)
    result += "FILTER col(" + quoted_raw(attribute) + ") == " + literal(target) + "\nFROM\n  ";
  result += frame_plan(source) + "\n\nThis error occurred with the following context stack:\n";
  const std::vector<std::string> nodes = temporal
      ? std::vector<std::string>{"filter", "group_by", "filter", "select", "join right", "join"}
      : std::vector<std::string>{"group_by", "filter", "select", "join right", "join"};
  for (std::size_t i = 0; i < nodes.size(); ++i)
    result += "\t[" + std::to_string(i + 1) + "] '" + nodes[i] + "'" +
              (temporal && i == 0 ? " failed\n" : "\n");
  return result;
}

[[noreturn]] void missing_column(const EventTable& source, const std::string& name,
                                  const std::string& attribute, const EventAttribute& target,
                                  bool include_filter) {
  std::string message = "unable to find column " + quoted_debug(name) + "; valid columns: [";
  for (std::size_t i = 0; i < source.columns.size(); ++i) {
    if (i) message += ", ";
    message += quoted_debug(source.columns[i]);
  }
  message += ']';
  // The retained diagnostics expose a shorter close suffix suggestion. Other
  // suggestion heuristics are outside this bounded diagnostic projection.
  for (const auto& candidate : source.columns) {
    if (!candidate.empty() && candidate.size() < name.size() && name.ends_with(candidate) &&
        5 * candidate.size() >= 2 * (name.size() + candidate.size())) {
      message += "\n\nDid you mean " + quoted_debug(candidate) + '?';
      break;
    }
  }
  error("ColumnNotFoundError", "collect", message + trace(source, attribute, target, include_filter, false));
}

Dtype dtype(const std::string& declared) {
  if (declared == "String" || declared == "Categorical") return Dtype::kString;
  if (declared == "Int64" || declared == "UInt64" || declared == "UInt32") return Dtype::kInteger;
  if (declared == "Float64") return Dtype::kFloat;
  if (declared == "Boolean") return Dtype::kBoolean;
  if (declared == "Null") return Dtype::kNull;
  if (declared == "Date") return Dtype::kDate;
  if (declared.starts_with("Datetime(time_unit='us', time_zone=")) return Dtype::kDatetime;
  throw std::invalid_argument("Unsupported native Polars repetition dtype: " + declared);
}

std::string numeric_name(const std::string& declared) {
  if (declared == "Int64") return "i64";
  if (declared == "UInt64") return "u64";
  if (declared == "UInt32") return "u32";
  return "f64";
}

void validate_comparison(const EventTable& source, std::size_t column,
                          const EventAttribute& target) {
  if (target.container != AttributeContainerKind::kScalar || target.scalar_wrapper || target.scalar_children_list)
    throw std::invalid_argument("Native Polars repetition comparison requires an ordinary scalar");
  const auto kind = dtype(source.dtypes[column]);
  const auto& key = source.columns[column];
  if (null(target) || kind == Dtype::kNull) return;
  if ((kind == Dtype::kDate || kind == Dtype::kDatetime) && text(target))
    error("InvalidOperationError", "collect",
          "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal column)" +
          trace(source, key, target, true, true));
  std::string numeric;
  if (kind == Dtype::kString && (integer(target) || real(target))) {
    if (real(target)) numeric = "f64";
    else {
      const auto value = exact_integer(target);
      numeric = value >= TokenInteger(std::numeric_limits<std::int32_t>::min()) &&
                value <= TokenInteger(std::numeric_limits<std::int32_t>::max()) ? "i32" : "i64";
    }
  } else if ((kind == Dtype::kInteger || kind == Dtype::kFloat) && text(target))
    numeric = numeric_name(source.dtypes[column]);
  if (!numeric.empty())
    error("ComputeError", "collect", "cannot compare string with numeric type (" + numeric +
          ")\n\nThis error occurred in the following expression:\n\t[(col(" + quoted_raw(key) + ")) == (" +
          literal(target, kind == Dtype::kString) + ")]\n");
}

std::string bound_literal(const Scalar& value) {
  return std::visit([](const auto& item) -> std::string {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) return "null";
    else if constexpr (std::is_same_v<T, std::string>) return quoted_raw(item);
    else if constexpr (std::is_same_v<T, bool>) return item ? "1" : "0";
    else if constexpr (std::is_same_v<T, TokenInteger>) return item.decimal();
    else return real_text(item);
  }, value);
}

std::string bound_expression(const Scalar& value, bool lower) {
  if (const auto* whole = std::get_if<TokenInteger>(&value)) {
    const auto count_max = TokenInteger(std::numeric_limits<std::uint32_t>::max());
    if ((!lower && *whole >= count_max) || (lower && *whole <= TokenInteger(0)))
      return ".when(col(\"count\").is_not_null()).then(true).otherwise(null)";
  }
  return "[(col(\"count\")) " + std::string(lower ? ">=" : "<=") + " (" + bound_literal(value) + ")]";
}

void validate_bounds(const PolarsAttributeRepetitionOptions& options) {
  for (const bool lower : {true, false}) {
    const auto& value = lower ? options.minimum : options.maximum;
    if (!std::holds_alternative<std::string>(value)) continue;
    error("ComputeError", "collect",
          "cannot compare string with numeric type (u32)\n\nThis error occurred in the following expression:\n\t" +
          bound_expression(value, lower) + "\nwhile evaluating this larger expression:\n\t[(" +
          bound_expression(options.minimum, true) + ") & (" + bound_expression(options.maximum, false) + ")]\n");
  }
}

bool bound_accepts(std::uint64_t count, const Scalar& bound, bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_same_v<T, std::string>) return false;
    else if constexpr (std::is_same_v<T, TokenInteger>)
      return lower ? value <= TokenInteger(count) : TokenInteger(count) <= value;
    else if constexpr (std::is_same_v<T, bool>) return lower ? count >= std::uint64_t(value) : count <= std::uint64_t(value);
    else {
      // Polars total ordering places NaN after all finite/infinite numbers.
      if (std::isnan(value)) return !lower;
      return lower ? static_cast<double>(count) >= value : static_cast<double>(count) <= value;
    }
  }, bound);
}

bool same(const EventAttribute& candidate, const EventAttribute& target, Dtype kind) {
  if (null(candidate) || null(target) || kind == Dtype::kNull) return false;
  if (kind == Dtype::kString || (kind == Dtype::kBoolean && text(target))) {
    const auto text_value = [](const EventAttribute& value) {
      return boolean(value) ? (static_cast<bool>(exact_integer(value)) ? std::string("true") : std::string("false")) : value.value;
    };
    return text_value(candidate) == text_value(target);
  }
  if (kind == Dtype::kDate || kind == Dtype::kDatetime) {
    if (!date(target)) throw std::invalid_argument("Unsupported native Polars temporal query type");
    const auto stored = [](const EventAttribute& value) {
      const auto* date_value = std::get_if<NativeDate>(&value.typed_value);
      return parse_timestamp_exact(date_value ? date_value->value : value.value);
    };
    const auto a = stored(candidate), b = stored(target);
    return a && b && a->seconds == b->seconds && a->nanoseconds == b->nanoseconds;
  }
  if (kind == Dtype::kFloat || real(target)) {
    const auto left = number(candidate), right = number(target);
    return (std::isnan(left) && std::isnan(right)) || left == right;
  }
  return exact_integer(candidate) == exact_integer(target);
}

std::optional<std::string> group(const std::optional<EventAttribute>& candidate, Dtype kind) {
  if (!candidate || null(*candidate) || kind == Dtype::kNull) return std::nullopt;
  if (kind == Dtype::kString) return "s:" + candidate->value;
  if (kind == Dtype::kFloat) {
    const auto value = number(*candidate);
    if (std::isnan(value)) return "f:NaN";
    return "f:" + std::to_string(std::bit_cast<std::uint64_t>(value == 0 ? 0.0 : value));
  }
  if (kind == Dtype::kInteger || kind == Dtype::kBoolean) return "n:" + exact_integer(*candidate).decimal();
  const auto* stored = std::get_if<NativeDate>(&candidate->typed_value);
  const auto instant = parse_timestamp_exact(stored ? stored->value : candidate->value);
  if (!instant) return std::nullopt;
  return "d:" + std::to_string(instant->seconds) + ':' + std::to_string(instant->nanoseconds);
}

EventTable collect(const EventTable& source, const EventAttribute& target,
                    const PolarsAttributeRepetitionOptions& options) {
  source.validate();
  if (source.index) throw std::invalid_argument("Native Polars repetition tables cannot have dataframe index metadata");
  if (source.dtypes.size() != source.columns.size())
    throw std::invalid_argument("Native Polars repetition requires declared column dtypes");
  const auto& attribute = std::get<std::string>(options.attribute_key);
  const auto& case_key = std::get<std::string>(options.case_id_key);
  const auto attr_at = std::find(source.columns.begin(), source.columns.end(), attribute);
  if (attr_at == source.columns.end()) missing_column(source, attribute, attribute, target, false);
  const auto case_at = std::find(source.columns.begin(), source.columns.end(), case_key);
  if (case_at == source.columns.end()) missing_column(source, case_key, attribute, target, true);
  const auto attribute_column = static_cast<std::size_t>(attr_at - source.columns.begin());
  const auto case_column = static_cast<std::size_t>(case_at - source.columns.begin());
  const auto attribute_kind = dtype(source.dtypes[attribute_column]);
  const auto case_kind = dtype(source.dtypes[case_column]);
  validate_comparison(source, attribute_column, target);
  validate_bounds(options);
  std::unordered_map<std::string, std::uint64_t> counts;
  for (const auto& row : source.rows) {
    const auto key = group(row[case_column], case_kind);
    if (key && row[attribute_column] && same(*row[attribute_column], target, attribute_kind)) ++counts[*key];
  }
  EventTable result = source;
  result.rows.clear();
  for (const auto& row : source.rows) {
    const auto key = group(row[case_column], case_kind);
    if (!key) continue;
    const auto found = counts.find(*key);
    if (found != counts.end() && bound_accepts(found->second, options.minimum, true) &&
        bound_accepts(found->second, options.maximum, false)) result.rows.push_back(row);
  }
  return result;
}

}  // namespace

EventTable filter_polars_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& options) {
  validate_apply(options);
  return collect(source, target, options);
}

LazyEventTable filter_polars_attribute_value_repetition(
    const LazyEventTable& source, const EventAttribute& target,
    const PolarsAttributeRepetitionOptions& options) {
  validate_apply(options);
  return LazyEventTable(source.columns(), [source, target, options](const auto& consume) {
    const auto result = collect(source.materialize(), target, options);
    for (const auto& row : result.rows) consume(row);
  }, source.attributes(), source.metadata(), std::nullopt, source.dtypes(),
     source.index(), source.categorical_dtypes());
}

}  // namespace minera
