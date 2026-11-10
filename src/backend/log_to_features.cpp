#include "minera/log_to_features.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

std::string value_for(const Event& e, const std::string& key) {
  if (key.empty()) return {};
  if (key == "activity" || key == "concept:name") return e.activity;
  if (key == "time:timestamp" || key == "timestamp") return e.timestamp;
  if (key == "org:resource" || key == "resource") return e.resource;
  if (const auto* v = e.attribute(key)) return *v;
  return {};
}

const EventAttribute* attr_for(const Event& e, const std::string& key) {
  for (const auto& a : e.attributes)
    if (!a.is_null && a.key == key) return &a;
  return nullptr;
}
const EventAttribute* attr_for(const Trace& t, const std::string& key) {
  for (const auto& a : t.attributes)
    if (!a.is_null && a.key == key) return &a;
  return nullptr;
}

std::optional<EventAttribute> numeric_cell(const Event& event, const std::string& key);
std::optional<EventAttribute> numeric_cell(const Trace& trace, const std::string& key);

std::string event_attribute_value(const Event& event, const std::string& key) {
  const auto value = numeric_cell(event, key);
  return value ? root_cause_attribute_category(*value) : "UNDEFINED";
}

std::string trace_attribute_value(const Trace& trace, const std::string& key) {
  const auto value = numeric_cell(trace, key);
  return value ? root_cause_attribute_category(*value) : "UNDEFINED";
}

std::string categorical_value(std::string value) {
  return value;
}

std::vector<std::string> distinct_event_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces) {
    bool present = false;
    for (const auto& event : trace.events) {
      if (numeric_cell(event, key)) {
        values.insert(categorical_value(event_attribute_value(event, key)));
        present = true;
      }
    }
    if (!present) values.insert("UNDEFINED");
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> event_attribute_values_for_trace(
    const Trace& trace, const std::string& key) {
  std::set<std::string> values;
  for (const auto& event : trace.events) {
    if (numeric_cell(event, key))
      values.insert(categorical_value(event_attribute_value(event, key)));
  }
  if (values.empty()) values.insert("UNDEFINED");
  return {values.begin(), values.end()};
}

std::vector<std::string> distinct_trace_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces)
    values.insert(categorical_value(trace_attribute_value(trace, key)));
  return {values.begin(), values.end()};
}

bool is_numeric_kind(AttributeKind k) {
  return k == AttributeKind::kInt || k == AttributeKind::kFloat ||
         k == AttributeKind::kBoolean;
}

std::optional<EventAttribute> numeric_cell(const Event& event, const std::string& key) {
  auto cell = native_event_attribute(event, key);
  // A CSV row's case column under its own header (see event_feature_keys).
  if (!cell && !key.empty() && key == event.case_id_source_key && !key.starts_with("case:") &&
      key != "concept:name" && event.case_id_state == FirstClassAttributeState::kPresent) {
    EventAttribute value;
    value.key = key;
    value.value = event.case_id;
    return value;
  }
  return cell;
}

std::optional<EventAttribute> numeric_cell(const Trace& trace, const std::string& key) {
  if (key == "activity" && trace.case_id_source_key != key &&
      (!trace.case_id_attribute || (trace.case_id_attribute->key != key &&
       trace.case_id_attribute->first_class_source_key != key)) &&
      std::none_of(trace.attributes.begin(), trace.attributes.end(), [&](const auto& value) {
        return value.key == key || value.first_class_source_key == key;
      })) return std::nullopt;
  Event context;
  context.attributes = trace.attributes;
  context.activity = trace.case_id;
  context.activity_state = trace.case_id_state;
  context.activity_attribute = trace.case_id_attribute;
  context.activity_source_key = key == "case_id" || key == "case:concept:name"
                                    ? key : trace.case_id_source_key;
  return native_event_attribute(context, key);
}

bool absent_attribute(const EventAttribute& attribute) {
  return attribute.is_null || attribute.first_class_state == FirstClassAttributeState::kNull ||
         (attribute.first_class_role != FirstClassAttributeRole::kNone &&
          attribute.first_class_state == FirstClassAttributeState::kMissing);
}

bool native_numeric_cell(const EventAttribute& attribute) {
  if (is_native_na_attribute(attribute) || absent_attribute(attribute) ||
      attribute.container != AttributeContainerKind::kScalar ||
      !attribute.children.empty() || attribute.kind == AttributeKind::kBytes ||
      attribute.kind == AttributeKind::kDate ||
      std::holds_alternative<NativeDate>(attribute.typed_value)) return false;
  if (!std::holds_alternative<std::monostate>(attribute.typed_value)) return true;
  return is_numeric_kind(attribute.kind);
}

bool complete_numeric_event_attribute(const EventLog& log, const std::string& key) {
  bool present = false;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      const auto attribute = numeric_cell(event, key);
      if (!attribute || !native_numeric_cell(*attribute)) return false;
      present = true;
    }
  }
  return present;
}

bool native_string_cell(const EventAttribute& attribute) {
  return !absent_attribute(attribute) &&
         attribute.container == AttributeContainerKind::kScalar &&
         attribute.children.empty() &&
         std::holds_alternative<std::monostate>(attribute.typed_value) &&
         (attribute.kind == AttributeKind::kString || attribute.kind == AttributeKind::kId);
}

std::optional<EventAttribute> event_feature_cell(const Event& event, const std::string& key) {
  auto cell = numeric_cell(event, key);
  const auto matches_projection = [&](const std::string& source,
                                      const std::optional<EventAttribute>& stored,
                                      const char* canonical, const char* alias) {
    return key == canonical || key == alias || (!source.empty() && key == source) ||
           (stored && (stored->key == key ||
                       (!stored->first_class_source_key.empty() && stored->first_class_source_key == key)));
  };
  // Unretained timestamp text is the native date projection. Retained cells
  // carry their own type, including deliberately string-valued timestamps.
  if (cell && !event.timestamp_attribute &&
      !matches_projection(event.activity_source_key, event.activity_attribute, "concept:name", "activity") &&
      !matches_projection(event.resource_source_key, event.resource_attribute, "org:resource", "resource") &&
      (key == "time:timestamp" || key == "timestamp" ||
       (!event.timestamp_source_key.empty() && key == event.timestamp_source_key)) &&
      std::none_of(event.attributes.begin(), event.attributes.end(), [&](const auto& value) {
        return value.key == key || value.first_class_source_key == key;
      })) cell->kind = AttributeKind::kDate;
  return cell;
}

// standard_keys (a CSV-sourced log): format_dataframe copies the case,
// activity and timestamp columns into case:concept:name, concept:name and
// time:timestamp and keeps the original columns, so convert_to_event_log
// gives every event both names; a case column not prefixed with case:
// becomes an ordinary event attribute (a case:-prefixed one goes to the
// trace, the standard one is the trace's concept:name).
std::vector<std::string> event_feature_keys(const EventLog& log, bool standard_keys) {
  std::set<std::string> keys;
  for (const auto& trace : log.traces) for (const auto& event : trace.events) {
    for (const auto& attribute : event.attributes) keys.insert(attribute.key);
    const auto add = [&](const std::string& source, const std::optional<EventAttribute>& stored,
                         const char* canonical) {
      keys.insert(!source.empty() ? source
          : stored && !stored->first_class_source_key.empty() ? stored->first_class_source_key
          : stored && !stored->key.empty() ? stored->key : std::string(canonical));
    };
    add(event.activity_source_key, event.activity_attribute, "concept:name");
    add(event.timestamp_source_key, event.timestamp_attribute, "time:timestamp");
    if (standard_keys) {
      keys.insert("concept:name");
      keys.insert("time:timestamp");
      if (!event.case_id_source_key.empty() && !event.case_id_source_key.starts_with("case:"))
        keys.insert(event.case_id_source_key);
    }
    add(event.resource_source_key, event.resource_attribute, "org:resource");
    add(event.lifecycle_source_key, event.lifecycle_attribute, "lifecycle:transition");
  }
  return {keys.begin(), keys.end()};
}

double numeric_attribute_value(const Event& event, const std::string& key,
                               double fallback) {
  if (const auto attribute = numeric_cell(event, key))
    return native_attribute_number(*attribute);
  return fallback;
}

double numeric_trace_value(const Trace& trace, const std::string& key) {
  if (const auto attribute = numeric_cell(trace, key))
    return native_attribute_number(*attribute);
  throw std::invalid_argument("log_to_features: trace has no numeric trace attribute: " + key);
}

void append_unique(std::vector<std::string>& out,
                   const std::vector<std::string>& in) {
  out.insert(out.end(), in.begin(), in.end());
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

void validate_names(const std::vector<std::string>& names, const char* label) {
  std::set<std::string> seen;
  for (const auto& n : names) {
    if (n.empty()) throw std::invalid_argument(std::string("log_to_features: ") + label + " contains empty name");
    if (!seen.insert(n).second) throw std::invalid_argument(std::string("log_to_features: duplicate ") + label);
  }
}

std::vector<std::string> categories_for_events(const EventLog& log,
                                                const std::string& key) {
  std::set<std::string> values;
  for (const auto& t : log.traces)
    for (const auto& e : t.events) values.insert(value_for(e, key));
  return {values.begin(), values.end()};
}

bool is_numeric_kind(AttributeKind k);

bool trace_inference_numeric(const EventAttribute& attribute) {
  return native_numeric_cell(attribute) &&
      !std::holds_alternative<bool>(attribute.typed_value) &&
      (!std::holds_alternative<std::monostate>(attribute.typed_value) ||
       attribute.kind != AttributeKind::kBoolean);
}

bool trace_inference_hashable(EventAttribute attribute) {
  const auto normalize_null = [](auto&& self, EventAttribute& value) -> void {
    if (absent_attribute(value)) {
      value.is_null = true;
      value.container = AttributeContainerKind::kScalar;
      value.children.clear();
      value.scalar_wrapper = false;
      value.scalar_children_list = false;
      value.typed_value = std::monostate{};
    } else {
      for (auto& child : value.children) self(self, child);
    }
  };
  normalize_null(normalize_null, attribute);
  return root_cause_key_is_hashable(attribute);
}

std::vector<std::string> inferred_event_attributes(const EventLog& log,
                                                   bool numeric, bool standard_keys) {
  std::map<std::string, std::size_t> occurrences;
  std::map<std::string, std::set<std::string>> string_values;
  std::set<std::string> has_string;
  const auto trace_count = log.trace_count();
  const auto keys = event_feature_keys(log, standard_keys);
  for (const auto& trace : log.traces) {
    std::set<std::string> seen;
    for (const auto& event : trace.events) {
      for (const auto& key : keys) {
        const auto attribute = event_feature_cell(event, key);
        if (!numeric && attribute && !trace_inference_hashable(*attribute))
          throw std::invalid_argument("log_to_features: unhashable inferred event attribute: " + key);
        if (!attribute) continue;
        // A key mixing strings with numbers is one-hot encoded over every
        // value, numbers included, and is never numeric.
        if (native_string_cell(*attribute)) has_string.insert(key);
        if (trace_inference_numeric(*attribute) || (!numeric && native_string_cell(*attribute))) {
          seen.insert(key);
          if (!numeric) string_values[key].insert(attribute->value);
        }
      }
    }
    for (const auto& key : seen) ++occurrences[key];
  }
  std::vector<std::string> result;
  for (const auto& [key, count] : occurrences)
    if (count == trace_count && has_string.contains(key) != numeric &&
        (numeric || string_values[key].size() <= 12))
      result.push_back(key);
  return result;
}

std::vector<std::string> inferred_trace_attributes(const EventLog& log,
                                                   bool numeric,
                                                   bool standard_keys) {
  std::map<std::string, std::size_t> occurrences;
  std::map<std::string, std::set<std::string>> string_values;
  std::set<std::string> has_string;
  const auto trace_count = log.trace_count();
  for (const auto& trace : log.traces) {
    std::set<std::string> keys;
    for (const auto& attribute : trace.attributes) keys.insert(attribute.key);
    const std::string case_key = standard_keys ? "concept:name"
        : !trace.case_id_source_key.empty() ? trace.case_id_source_key
        : trace.case_id_attribute && !trace.case_id_attribute->key.empty()
            ? trace.case_id_attribute->key : "concept:name";
    keys.insert(case_key);
    keys.erase("concept:name");
    for (const auto& key : keys) {
      const auto attribute = numeric_cell(trace, key);
      if (!numeric && attribute && !trace_inference_hashable(*attribute))
        throw std::invalid_argument("log_to_features: unhashable inferred trace attribute: " + key);
      if (!attribute) continue;
      if (native_string_cell(*attribute)) has_string.insert(key);
      if (trace_inference_numeric(*attribute) || (!numeric && native_string_cell(*attribute))) {
        ++occurrences[key];
        if (!numeric) string_values[key].insert(attribute->value);
      }
    }
  }
  std::vector<std::string> result;
  for (const auto& [key, count] : occurrences)
    if (count == trace_count && has_string.contains(key) != numeric &&
        (numeric || string_values[key].size() <= 12))
      result.push_back(key);
  return result;
}

struct TimedEvent {
  const Event* event = nullptr;
  double finish = 0.0;
  double start = 0.0;
  std::size_t trace = 0;
  std::size_t index = 0;
};

double instant(const Event& e, const std::string& key, bool required) {
  const auto text = value_for(e, key);
  if (text.empty()) {
    if (required) throw std::invalid_argument("log_to_features: missing timestamp");
    return std::numeric_limits<double>::quiet_NaN();
  }
  const auto parsed = parse_timestamp(text);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("log_to_features: invalid or non-finite timestamp");
  return *parsed;
}

enum class TemporalFrequencyUnit { kSecond, kMinute, kHour, kDay, kWeek,
                                  kMonth, kQuarter, kYear, kSemiMonth, kWeekOfMonth };

struct TemporalFrequency {
  TemporalFrequencyUnit unit = TemporalFrequencyUnit::kWeek;
  std::int64_t multiple = 1;
  bool label_start = false;
  int week_end_day = 0;  // std::tm convention: Sunday = 0.
  int period_start_month = 0;
  bool business = false;
  int month_day = 15;
  int month_week = 1;
  std::int64_t tick_ns = 0;
};

std::string uppercase_ascii(std::string value) {
  for (auto& character : value)
    character = static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)));
  return value;
}

std::int64_t floor_division(std::int64_t numerator,
                            std::int64_t denominator) {
  if (denominator <= 0) throw std::invalid_argument("log_to_features: invalid temporal frequency");
  std::int64_t quotient = numerator / denominator;
  const std::int64_t remainder = numerator % denominator;
  if (remainder < 0) --quotient;
  return quotient;
}

// Normalize each component before adding it. Test the fractional residual
// against truncation, using direct conversions from the original unit so
// intermediate floating-point multiplications cannot change acceptance.
std::uint64_t normalized_temporal_tick(double coefficient, std::uint64_t scale) {
  constexpr std::uint64_t units[] = {3600000000000ULL, 60000000000ULL,
      1000000000ULL, 1000000ULL, 1000ULL, 1ULL};
  for (const auto unit : units) {
    if (unit > scale) continue;
    const double count = coefficient * static_cast<double>(scale / unit);
    const double integral = std::trunc(count);
    if (!std::isfinite(count) || count >= 9223372036854775808.0)
      throw std::invalid_argument("log_to_features: temporal tick width overflows");
    if (count - integral > 1e-8) continue;
    const auto number = static_cast<std::uint64_t>(integral);
    if (number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / unit)
      throw std::invalid_argument("log_to_features: temporal tick width overflows");
    return number * unit;
  }
  throw std::invalid_argument("log_to_features: temporal tick width is not representable in nanoseconds");
}

std::optional<std::int64_t> temporal_tick_width(std::string_view text) {
  std::uint64_t total = 0;
  std::size_t position = 0;
  bool found = false;
  while (position < text.size()) {
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position;
    if (position == text.size()) break;
    if (text[position] == '+') {
      ++position;
      if (position == text.size() ||
          (!std::isdigit(static_cast<unsigned char>(text[position])) && text[position] != '.'))
        return std::nullopt;
    }
    std::string coefficient;
    bool dot = false;
    while (position < text.size() && (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '.')) {
      const char c = text[position++];
      if (c == '.') { if (dot) return std::nullopt; dot = true; }
      coefficient += c;
    }
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position;
    const auto begin = position;
    while (position < text.size() && std::isalpha(static_cast<unsigned char>(text[position]))) ++position;
    const auto unit = std::string(text.substr(begin, position - begin));
    const auto upper = uppercase_ascii(unit);
    std::uint64_t scale = unit == "s" ? 1000000000ULL : unit == "h" ? 3600000000000ULL :
      upper == "D" ? 86400000000000ULL : upper == "MIN" ? 60000000000ULL :
      unit == "ms" ? 1000000ULL : upper == "US" ? 1000ULL : upper == "NS" ? 1ULL : 0;
    if (!scale) return std::nullopt;
    if (coefficient.empty()) coefficient = "1";
    double number = 0;
    const auto converted = std::from_chars(coefficient.data(), coefficient.data() + coefficient.size(), number);
    if (converted.ec != std::errc{} || converted.ptr != coefficient.data() + coefficient.size())
      throw std::invalid_argument("log_to_features: invalid temporal tick coefficient");
    const auto width = normalized_temporal_tick(number, scale);
    if (width > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - total)
      throw std::invalid_argument("log_to_features: temporal tick width overflows");
    total += width;
    found = true;
  }
  if (!found) return std::nullopt;
  if (!total) throw std::invalid_argument("log_to_features: temporal tick width must be positive");
  return static_cast<std::int64_t>(total);
}

using ExactTemporalInstant = std::pair<std::int64_t, std::int64_t>;

ExactTemporalInstant exact_temporal_instant(std::string text) {
  std::int64_t fraction = 0;
  const auto point = text.find('.', text.find_first_of("Tt "));
  if (point != std::string::npos) {
    auto end = point + 1;
    unsigned count = 0;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) {
      if (count++ < 9) fraction = fraction * 10 + text[end] - '0';
      ++end;
    }
    while (count++ < 9) fraction *= 10;
    text.erase(point, end - point);
  }
  const auto seconds = parse_timestamp(text);
  if (!seconds || !std::isfinite(*seconds)) throw std::invalid_argument("log_to_features: invalid exact temporal timestamp");
  return {static_cast<std::int64_t>(*seconds), fraction};
}

ExactTemporalInstant add_temporal_nanos(ExactTemporalInstant instant, std::int64_t nanos) {
  instant.first += nanos / 1000000000;
  instant.second += nanos % 1000000000;
  if (instant.second < 0) { --instant.first; instant.second += 1000000000; }
  if (instant.second >= 1000000000) { ++instant.first; instant.second -= 1000000000; }
  return instant;
}

std::string temporal_nanosecond_text(ExactTemporalInstant instant) {
  const bool negative = instant.first < 0;
  auto seconds = negative ? -instant.first : instant.first;
  auto fraction = instant.second;
  if (negative && fraction) { --seconds; fraction = 1000000000 - fraction; }
  auto tail = std::to_string(fraction);
  auto result = std::to_string(seconds) + std::string(9 - tail.size(), '0') + tail;
  const auto nonzero = result.find_first_not_of('0');
  result = nonzero == std::string::npos ? "0" : result.substr(nonzero);
  return negative && result != "0" ? "-" + result : result;
}

double temporal_metric_instant(ExactTemporalInstant instant, bool nanosecond_source) {
  auto integer = temporal_nanosecond_text(instant);
  if (!nanosecond_source) {
    if (integer == "0") return 0.0;
    integer.resize(integer.size() - 3);
    // Parse decimal seconds directly: converting the microsecond integer to
    // double first loses low bits at distant dates. Padding also avoids
    // subtractive cancellation for negative instants close to the epoch.
    const auto sign = integer.front() == '-' ? 1u : 0u;
    if (integer.size() - sign <= 6)
      integer.insert(sign, 7 - (integer.size() - sign), '0');
    integer.insert(integer.size() - 6, 1, '.');
  }
  double value = 0.0;
  const auto parsed = std::from_chars(integer.data(), integer.data() + integer.size(), value);
  if (parsed.ec != std::errc{}) throw std::invalid_argument("log_to_features: temporal metric timestamp out of range");
  if (nanosecond_source) {
    value /= 1e9;
    // Timestamp sources expose a double rounded to six decimal places.
    // Decimal formatting avoids changing halfway cases by multiplying first.
    char rounded[64];
    const auto formatted = std::to_chars(rounded, rounded + sizeof(rounded), value, std::chars_format::fixed, 6);
    if (formatted.ec != std::errc{}) throw std::invalid_argument("log_to_features: temporal metric timestamp out of range");
    const auto converted = std::from_chars(rounded, formatted.ptr, value);
    if (converted.ec != std::errc{}) throw std::invalid_argument("log_to_features: temporal metric timestamp out of range");
  }
  return value;
}

ExactTemporalInstant fixed_temporal_label(ExactTemporalInstant value, std::int64_t origin, std::int64_t width) {
  // Compute (seconds * 1e9 + fraction) modulo width without epoch-ns overflow.
  std::uint64_t seconds = static_cast<std::uint64_t>(value.first - origin);
  std::uint64_t factor = 1000000000ULL % width;
  std::uint64_t remainder = value.second % width;
  while (seconds) {
    if (seconds & 1) remainder = (remainder + factor) % width;
    seconds >>= 1;
    factor = (factor + factor) % width;
  }
  return add_temporal_nanos(value, -static_cast<std::int64_t>(remainder));
}

TemporalFrequency parse_temporal_frequency(const std::string& raw) {
  if (raw.empty())
    throw std::invalid_argument("log_to_features: temporal frequency must not be empty");
  if (const auto width = temporal_tick_width(raw)) {
    TemporalFrequency tick;
    tick.unit = TemporalFrequencyUnit::kSecond;
    tick.tick_ns = *width;
    return tick;
  }
  auto normalized = raw;
  while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.back()))) normalized.pop_back();
  while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.front()))) normalized.erase(0, 1);
  if (!normalized.empty() && normalized.front() == '+') normalized.erase(0, 1);
  normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](unsigned char c) { return std::isspace(c); }), normalized.end());
  const auto modern_begin = normalized.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  const auto modern = normalized.substr(modern_begin == std::string::npos ? normalized.size() : modern_begin);
  const auto modern_upper = uppercase_ascii(modern);
  if (!(modern_upper == "B" || modern_upper == "W" || modern_upper.starts_with("W-") ||
        (modern == modern_upper && (modern == "ME" || modern == "MS" || modern == "QE" || modern == "QS" ||
         modern == "YE" || modern == "YS" || modern.starts_with("QE-") || modern.starts_with("QS-") ||
         modern.starts_with("YE-") || modern.starts_with("YS-") || modern.starts_with("B") ||
         modern.starts_with("SM") || modern.starts_with("WOM-")))))
    throw std::invalid_argument("log_to_features: unsupported temporal frequency '" + raw + "'");
  const auto alias_begin = raw.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  if (alias_begin != std::string::npos) {
    const auto alias = raw.substr(alias_begin);
    const auto upper = uppercase_ascii(alias);
    if (upper.starts_with("QE-") || upper.starts_with("QS-") ||
        upper.starts_with("YE-") || upper.starts_with("YS-") ||
        upper.starts_with("BQE-") || upper.starts_with("BQS-") ||
        upper.starts_with("BYE-") || upper.starts_with("BYS-") ||
        upper == "B" || upper == "BME" || upper == "BMS" || upper == "BQE" ||
        upper == "BQS" || upper == "BYE" || upper == "BYS" ||
        upper == "SME" || upper == "SMS" || upper.starts_with("SME-") ||
        upper.starts_with("SMS-") || upper.starts_with("WOM-")) {
      if (alias != upper && upper != "B")
        throw std::invalid_argument("log_to_features: fiscal temporal frequency must use uppercase aliases");
      std::size_t position = !raw.empty() && raw.front() == '+' ? 1 : 0;
      const auto digits_begin = position;
      while (position < alias_begin && std::isdigit(static_cast<unsigned char>(raw[position]))) ++position;
      const auto digits_end = position;
      while (position < alias_begin && std::isspace(static_cast<unsigned char>(raw[position]))) ++position;
      if (position != alias_begin || (alias_begin != 0 && digits_begin == digits_end))
        throw std::invalid_argument("log_to_features: invalid fiscal temporal frequency multiplier");
      normalized = raw.substr(digits_begin, digits_end - digits_begin) + alias;
    }
  }
  const auto value = uppercase_ascii(normalized);
  std::size_t suffix_begin = 0;
  while (suffix_begin < value.size() &&
         std::isdigit(static_cast<unsigned char>(value[suffix_begin])))
    ++suffix_begin;
  std::int64_t multiple = 1;
  if (suffix_begin != 0) {
    try {
      multiple = std::stoll(value.substr(0, suffix_begin));
    } catch (...) {
      throw std::invalid_argument("log_to_features: invalid temporal frequency");
    }
  }
  if (multiple <= 0 || suffix_begin == value.size())
    throw std::invalid_argument("log_to_features: temporal frequency multiplier must be positive");
  auto suffix = value.substr(suffix_begin);
  std::optional<int> fiscal_month;
  if (const auto separator = suffix.find('-'); separator != std::string::npos &&
      suffix.front() != 'W' && !suffix.starts_with("SME-") && !suffix.starts_with("SMS-")) {
    static const std::vector<std::string> months{
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const auto base = suffix.substr(0, separator);
    const auto month = std::find(months.begin(), months.end(), suffix.substr(separator + 1));
    if ((base != "QE" && base != "QS" && base != "YE" && base != "YS" &&
         base != "BQE" && base != "BQS" && base != "BYE" && base != "BYS") || month == months.end())
      throw std::invalid_argument("log_to_features: invalid fiscal temporal frequency");
    fiscal_month = static_cast<int>(month - months.begin());
    suffix = base;
  }
  TemporalFrequency result;
  result.multiple = multiple;
  if (suffix == "BME" || suffix == "BMS" || suffix == "BQE" || suffix == "BQS" ||
      suffix == "BYE" || suffix == "BYS") {
    result.business = true;
    suffix.erase(0, 1);
  }
  if (suffix == "SME" || suffix == "SMS" || suffix.starts_with("SME-") || suffix.starts_with("SMS-")) {
    result.unit = TemporalFrequencyUnit::kSemiMonth;
    result.label_start = suffix.starts_with("SMS");
    if (suffix.size() > 3) {
      const auto day = suffix.substr(4);
      int parsed = 0;
      const auto conversion = std::from_chars(day.data(), day.data() + day.size(), parsed);
      if (conversion.ec != std::errc{} || conversion.ptr != day.data() + day.size())
        throw std::invalid_argument("log_to_features: invalid semi-month day");
      result.month_day = parsed;
    }
    if (result.month_day < (result.label_start ? 2 : 1) || result.month_day > 27)
      throw std::invalid_argument("log_to_features: semi-month day outside supported range");
  } else if (suffix.starts_with("WOM-")) {
    static const std::map<std::string, int> weekdays{{"SUN",0},{"MON",1},{"TUE",2},{"WED",3},{"THU",4},{"FRI",5},{"SAT",6}};
    if (suffix.size() != 8 || suffix[4] < '1' || suffix[4] > '4' || !weekdays.contains(suffix.substr(5)))
      throw std::invalid_argument("log_to_features: invalid week-of-month frequency");
    result.unit = TemporalFrequencyUnit::kWeekOfMonth;
    result.month_week = suffix[4] - '0';
    result.week_end_day = weekdays.at(suffix.substr(5));
  } else if (suffix == "B") {
    result.unit = TemporalFrequencyUnit::kDay;
    result.business = true;
  } else if (suffix == "S" || suffix == "SEC" || suffix == "SECOND" ||
      suffix == "SECONDS") {
    result.unit = TemporalFrequencyUnit::kSecond;
  } else if (suffix == "T" || suffix == "MIN" || suffix == "MINS" ||
             suffix == "MINUTE" || suffix == "MINUTES") {
    result.unit = TemporalFrequencyUnit::kMinute;
  } else if (suffix == "H" || suffix == "HR" || suffix == "HOUR" ||
             suffix == "HOURS") {
    result.unit = TemporalFrequencyUnit::kHour;
  } else if (suffix == "D" || suffix == "DAY" || suffix == "DAYS") {
    result.unit = TemporalFrequencyUnit::kDay;
  } else if (suffix.rfind("W", 0) == 0) {
    result.unit = TemporalFrequencyUnit::kWeek;
    const auto day = suffix.substr(1);
    if (!day.empty()) {
      if (day.front() != '-')
        throw std::invalid_argument("log_to_features: invalid weekly temporal frequency");
      const auto weekday = day.substr(1);
      static const std::map<std::string, int> weekdays = {
          {"SUN", 0}, {"MON", 1}, {"TUE", 2}, {"WED", 3},
          {"THU", 4}, {"FRI", 5}, {"SAT", 6}};
      const auto found = weekdays.find(weekday);
      if (found == weekdays.end())
        throw std::invalid_argument("log_to_features: invalid weekly temporal frequency");
      result.week_end_day = found->second;
    }
  } else if (suffix == "M" || suffix == "ME" || suffix == "MONTH" ||
             suffix == "MONTHS") {
    result.unit = TemporalFrequencyUnit::kMonth;
  } else if (suffix == "MS" || suffix == "MONTHSTART") {
    result.unit = TemporalFrequencyUnit::kMonth;
    result.label_start = true;
  } else if (suffix == "Q" || suffix == "QE" || suffix == "QUARTER" ||
             suffix == "QUARTERS") {
    result.unit = TemporalFrequencyUnit::kQuarter;
  } else if (suffix == "QS" || suffix == "QUARTERSTART") {
    result.unit = TemporalFrequencyUnit::kQuarter;
    result.label_start = true;
  } else if (suffix == "Y" || suffix == "A" || suffix == "YE" ||
             suffix == "YEAR" || suffix == "YEARS") {
    result.unit = TemporalFrequencyUnit::kYear;
  } else if (suffix == "YS" || suffix == "AS" || suffix == "YEARSTART") {
    result.unit = TemporalFrequencyUnit::kYear;
    result.label_start = true;
  } else {
    throw std::invalid_argument("log_to_features: unsupported temporal frequency '" + raw + "'");
  }
  if (fiscal_month) {
    const int months = result.unit == TemporalFrequencyUnit::kQuarter ? 3 : 12;
    result.period_start_month = *fiscal_month - (result.label_start ? 0 : months - 1);
  }
  return result;
}

std::tm utc_calendar(double seconds) {
  using namespace std::chrono;
  const auto day_count = std::floor(seconds / 86400.0);
  const auto minimum = sys_days{year::min() / January / day{1}}.time_since_epoch().count();
  const auto maximum = sys_days{year::max() / December / day{31}}.time_since_epoch().count();
  if (!std::isfinite(day_count) || day_count < static_cast<double>(minimum) ||
      day_count > static_cast<double>(maximum))
    throw std::invalid_argument("log_to_features: timestamp is outside the calendar range");
  const sys_days date{days{static_cast<days::rep>(day_count)}};
  const year_month_day civil{date};
  std::tm calendar{};
  calendar.tm_year = static_cast<int>(civil.year()) - 1900;
  calendar.tm_mon = static_cast<int>(static_cast<unsigned>(civil.month())) - 1;
  calendar.tm_mday = static_cast<int>(static_cast<unsigned>(civil.day()));
  calendar.tm_wday = static_cast<int>(weekday{date}.c_encoding());
  return calendar;
}

std::chrono::sys_days calendar_day(const std::tm& calendar) {
  using namespace std::chrono;
  return sys_days{year{calendar.tm_year + 1900} /
                  month{static_cast<unsigned>(calendar.tm_mon + 1)} /
                  day{static_cast<unsigned>(calendar.tm_mday)}};
}

std::int64_t epoch_seconds(std::chrono::sys_days day) {
  using namespace std::chrono;
  return duration_cast<seconds>(day.time_since_epoch()).count();
}

std::int64_t temporal_month_label(std::int64_t month_index, bool label_start, bool business = false);

std::int64_t business_day_ordinal(std::int64_t day_index) {
  const auto week = floor_division(day_index - 4, 7);
  const auto weekday = day_index - 4 - week * 7;
  return week * 5 + std::min<std::int64_t>(weekday, 4);
}

std::int64_t business_day_label(std::int64_t ordinal) {
  using namespace std::chrono;
  const auto week = floor_division(ordinal, 5);
  auto weekday = ordinal % 5;
  if (weekday < 0) weekday += 5;
  const long double day_index = 4.0L + static_cast<long double>(week) * 7 + weekday;
  const auto minimum = sys_days{year::min() / January / day{1}}.time_since_epoch().count();
  const auto maximum = sys_days{year::max() / December / day{31}}.time_since_epoch().count();
  if (day_index < minimum || day_index > maximum)
    throw std::invalid_argument("log_to_features: business day label outside calendar range");
  return epoch_seconds(sys_days{days{static_cast<days::rep>(day_index)}});
}

std::int64_t monthly_offset_label(std::int64_t ordinal, const TemporalFrequency& frequency) {
  if (frequency.unit == TemporalFrequencyUnit::kSemiMonth) {
    const auto month = floor_division(ordinal, 2);
    const bool second = ordinal % 2 != 0;
    if (!frequency.label_start && second) return temporal_month_label(month, false);
    const int day = frequency.label_start && !second ? 1 : frequency.month_day;
    return temporal_month_label(month, true) + static_cast<std::int64_t>(day - 1) * 86400;
  }
  const auto first = temporal_month_label(ordinal, true);
  const auto weekday = utc_calendar(static_cast<double>(first)).tm_wday;
  const auto day_offset = (frequency.week_end_day - weekday + 7) % 7 + (frequency.month_week - 1) * 7;
  return first + static_cast<std::int64_t>(day_offset) * 86400;
}

std::int64_t monthly_offset_ordinal(double seconds, const TemporalFrequency& frequency) {
  const auto calendar = utc_calendar(seconds);
  const auto month = static_cast<std::int64_t>(calendar.tm_year + 1900) * 12 + calendar.tm_mon;
  const auto date = epoch_seconds(calendar_day(calendar));
  if (frequency.unit == TemporalFrequencyUnit::kWeekOfMonth)
    return month - (date < monthly_offset_label(month, frequency));
  const auto first = month * 2;
  if (date < monthly_offset_label(first, frequency)) return first - 1;
  return first + (date >= monthly_offset_label(first + 1, frequency));
}

std::int64_t natural_temporal_bucket_label(double seconds,
                                   const TemporalFrequency& frequency) {
  using namespace std::chrono;
  const auto calendar = utc_calendar(seconds);
  const auto date = calendar_day(calendar);
  switch (frequency.unit) {
    case TemporalFrequencyUnit::kSecond:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple) * frequency.multiple;
    case TemporalFrequencyUnit::kMinute:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple * 60) * frequency.multiple * 60;
    case TemporalFrequencyUnit::kHour:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple * 3600) * frequency.multiple * 3600;
    case TemporalFrequencyUnit::kDay:
      return floor_division(epoch_seconds(date), frequency.multiple * 86400) *
             frequency.multiple * 86400;
    case TemporalFrequencyUnit::kWeek: {
      const int delta = (frequency.week_end_day - calendar.tm_wday + 7) % 7;
      const auto end_date = date + days{delta};
      const auto anchor_date =
          sys_days{year{1970} / month{1} / day{4}} +
          days{frequency.week_end_day};
      const auto weeks = floor_division(epoch_seconds(end_date) -
                                            epoch_seconds(anchor_date),
                                        7 * 86400);
      const auto bucket = floor_division(weeks, frequency.multiple);
      return epoch_seconds(anchor_date +
                           days{bucket * frequency.multiple * 7});
    }
    case TemporalFrequencyUnit::kMonth:
    case TemporalFrequencyUnit::kQuarter:
    case TemporalFrequencyUnit::kYear: {
      const std::int64_t months_per_period =
          frequency.unit == TemporalFrequencyUnit::kMonth
              ? 1
              : frequency.unit == TemporalFrequencyUnit::kQuarter ? 3 : 12;
      const std::int64_t month_index =
          static_cast<std::int64_t>(calendar.tm_year + 1900) * 12 +
          calendar.tm_mon;
      const auto period = floor_division(
          month_index - frequency.period_start_month, months_per_period * frequency.multiple);
      const auto first_month =
          period * months_per_period * frequency.multiple + frequency.period_start_month;
      auto label_month = frequency.label_start
                                   ? first_month
                                   : first_month +
                                         months_per_period * frequency.multiple - 1;
      if (frequency.business) {
        const auto label = temporal_month_label(label_month, frequency.label_start, true);
        if (frequency.label_start && epoch_seconds(date) < label) label_month -= months_per_period;
        if (!frequency.label_start && epoch_seconds(date) > label) label_month += months_per_period;
      }
      return temporal_month_label(label_month, frequency.label_start, frequency.business);
    }
    case TemporalFrequencyUnit::kSemiMonth:
    case TemporalFrequencyUnit::kWeekOfMonth: break;
  }
  throw std::invalid_argument("log_to_features: invalid temporal frequency");
}

std::int64_t temporal_frequency_span(const TemporalFrequency& frequency) {
  std::int64_t scale = 1;
  switch (frequency.unit) {
    case TemporalFrequencyUnit::kSecond: break;
    case TemporalFrequencyUnit::kMinute: scale = 60; break;
    case TemporalFrequencyUnit::kHour: scale = 3600; break;
    case TemporalFrequencyUnit::kDay: scale = 86400; break;
    case TemporalFrequencyUnit::kWeek: scale = 604800; break;
    case TemporalFrequencyUnit::kMonth: break;
    case TemporalFrequencyUnit::kQuarter: scale = 3; break;
    case TemporalFrequencyUnit::kYear: scale = 12; break;
    case TemporalFrequencyUnit::kSemiMonth:
    case TemporalFrequencyUnit::kWeekOfMonth: break;
  }
  if (frequency.multiple > std::numeric_limits<std::int64_t>::max() / scale)
    throw std::invalid_argument("log_to_features: temporal frequency overflows calendar range");
  return frequency.multiple * scale;
}

std::int64_t temporal_month_label(std::int64_t month_index, bool label_start, bool business) {
  using namespace std::chrono;
  const auto year_value = floor_division(month_index, 12);
  if (year_value < static_cast<int>(year::min()) || year_value > static_cast<int>(year::max()))
    throw std::invalid_argument("log_to_features: temporal label outside calendar range");
  const auto month_value = static_cast<unsigned>(month_index - year_value * 12 + 1);
  const year_month date{year{static_cast<int>(year_value)}, month{month_value}};
  auto label = label_start ? sys_days{date / day{1}} : sys_days{date / last};
  if (business) {
    const auto weekday = std::chrono::weekday{label}.c_encoding();
    if (weekday == 0) label += days{label_start ? 1 : -2};
    if (weekday == 6) label += days{label_start ? 2 : -1};
  }
  return epoch_seconds(label);
}

std::int64_t temporal_bucket_label(double seconds, const TemporalFrequency& frequency,
                                   double origin) {
  if (frequency.unit == TemporalFrequencyUnit::kSemiMonth || frequency.unit == TemporalFrequencyUnit::kWeekOfMonth) {
    const auto first = monthly_offset_ordinal(origin, frequency);
    const auto current = monthly_offset_ordinal(seconds, frequency);
    return monthly_offset_label(first + floor_division(current - first, frequency.multiple) * frequency.multiple, frequency);
  }
  if (frequency.business && frequency.unit == TemporalFrequencyUnit::kDay) {
    const auto first = business_day_ordinal(epoch_seconds(calendar_day(utc_calendar(origin))) / 86400);
    const auto current = business_day_ordinal(epoch_seconds(calendar_day(utc_calendar(seconds))) / 86400);
    return business_day_label(first + floor_division(current - first, frequency.multiple) * frequency.multiple);
  }
  auto natural = frequency;
  natural.multiple = 1;
  const auto span = temporal_frequency_span(frequency);
  if (frequency.unit <= TemporalFrequencyUnit::kDay) {
    const auto anchor = epoch_seconds(calendar_day(utc_calendar(origin)));
    const auto offset = static_cast<std::int64_t>(std::floor(seconds)) - anchor;
    return anchor + floor_division(offset, span) * span;
  }
  const auto first = natural_temporal_bucket_label(origin, natural);
  const auto current = natural_temporal_bucket_label(seconds, natural);
  if (frequency.unit == TemporalFrequencyUnit::kWeek) {
    const auto delta = current - first;
    const auto steps = delta / span + (delta % span != 0);
    if (steps > std::numeric_limits<std::int64_t>::max() / span ||
        (first > 0 && steps * span > std::numeric_limits<std::int64_t>::max() - first))
      throw std::invalid_argument("log_to_features: temporal label overflows calendar range");
    return first + steps * span;
  }
  const auto month_index = [](std::int64_t label) {
    const auto calendar = utc_calendar(static_cast<double>(label));
    return static_cast<std::int64_t>(calendar.tm_year + 1900) * 12 + calendar.tm_mon;
  };
  const auto first_month = month_index(first);
  const auto delta = month_index(current) - first_month;
  const auto steps = delta / span + (!frequency.label_start && delta % span != 0);
  if (steps > std::numeric_limits<std::int64_t>::max() / span ||
      (first_month > 0 && steps * span > std::numeric_limits<std::int64_t>::max() - first_month))
    throw std::invalid_argument("log_to_features: temporal label overflows calendar range");
  return temporal_month_label(first_month + steps * span, frequency.label_start, frequency.business);
}

std::int64_t next_temporal_bucket_label(std::int64_t label, const TemporalFrequency& frequency) {
  if (frequency.unit == TemporalFrequencyUnit::kSemiMonth || frequency.unit == TemporalFrequencyUnit::kWeekOfMonth) {
    const auto ordinal = monthly_offset_ordinal(static_cast<double>(label), frequency);
    if (ordinal > 0 && frequency.multiple > std::numeric_limits<std::int64_t>::max() - ordinal)
      throw std::invalid_argument("log_to_features: monthly offset grid overflows calendar range");
    return monthly_offset_label(ordinal + frequency.multiple, frequency);
  }
  if (frequency.business && frequency.unit == TemporalFrequencyUnit::kDay) {
    const auto ordinal = business_day_ordinal(label / 86400);
    if (ordinal > 0 && frequency.multiple > std::numeric_limits<std::int64_t>::max() - ordinal)
      throw std::invalid_argument("log_to_features: business day grid overflows calendar range");
    return business_day_label(ordinal + frequency.multiple);
  }
  const auto span = temporal_frequency_span(frequency);
  if (frequency.unit <= TemporalFrequencyUnit::kWeek) {
    if (label > std::numeric_limits<std::int64_t>::max() - span)
      throw std::invalid_argument("log_to_features: temporal grid overflows calendar range");
    return label + span;
  }
  const auto calendar = utc_calendar(static_cast<double>(label));
  const auto month_index = static_cast<std::int64_t>(calendar.tm_year + 1900) * 12 + calendar.tm_mon;
  if (month_index > std::numeric_limits<std::int64_t>::max() - span)
    throw std::invalid_argument("log_to_features: temporal grid overflows calendar range");
  return temporal_month_label(month_index + span, frequency.label_start, frequency.business);
}

template <typename Less>
void temporal_object_sort(std::vector<std::size_t>& order, const Less& less) {
  // The object relation is partial. Explicit partition/insertion operations
  // produce ranks without giving a partial comparator to a library sort.
  struct Partition { std::size_t first, end, depth; };
  std::size_t depth = 0;
  for (auto size = order.size(); size > 1; size >>= 1) depth += 2;
  std::vector<Partition> pending{{0, order.size(), depth}};
  while (!pending.empty()) {
    const auto [first, end, remaining] = pending.back();
    pending.pop_back();
    const auto size = end - first;
    if (size < 2) continue;
    if (size <= 16) {
      for (auto i = first + 1; i < end; ++i) {
        const auto value = order[i];
        auto at = i;
        while (at > first && less(value, order[at - 1])) {
          order[at] = order[at - 1];
          --at;
        }
        order[at] = value;
      }
      continue;
    }
    if (remaining == 0) {
      const auto sift = [&](std::size_t root, std::size_t count) {
        while (root < count / 2) {
          auto child = root * 2 + 1;
          if (child + 1 < count && less(order[first + child], order[first + child + 1])) ++child;
          if (!less(order[first + root], order[first + child])) break;
          std::swap(order[first + root], order[first + child]);
          root = child;
        }
      };
      for (auto root = size / 2; root > 0;) sift(--root, size);
      for (auto count = size; count > 1;) {
        std::swap(order[first], order[first + --count]);
        sift(0, count);
      }
      continue;
    }
    const auto last = end - 1, middle = first + (size - 1) / 2;
    if (less(order[middle], order[first])) std::swap(order[middle], order[first]);
    if (less(order[last], order[middle])) std::swap(order[last], order[middle]);
    if (less(order[middle], order[first])) std::swap(order[middle], order[first]);
    std::swap(order[middle], order[last - 1]);
    const auto pivot = order[last - 1];
    auto left = first, right = last - 1;
    while (true) {
      do { ++left; } while (left < last - 1 && less(order[left], pivot));
      do { --right; } while (right > first && less(pivot, order[right]));
      if (left >= right) break;
      std::swap(order[left], order[right]);
    }
    std::swap(order[left], order[last - 1]);
    pending.push_back({left + 1, end, remaining - 1});
    pending.push_back({first, left, remaining - 1});
  }
}

}  // namespace

LogToFeaturesResult log_to_features(const EventLog& log,
                                    const LogToFeaturesOptions& options) {
  const bool calendar_projection = options.variant == LogToFeaturesVariant::TEMPORAL &&
      !options.grouper_freq.empty();
  if (options.activity_key.empty() || options.timestamp_key.empty() ||
      options.resource_key.empty())
    throw std::invalid_argument("log_to_features: keys must not be empty");
  if (!std::isfinite(options.default_not_present))
    throw std::invalid_argument("log_to_features: default_not_present must be finite");
  if (options.grouper_freq.empty() &&
      (!std::isfinite(options.window_seconds) || options.window_seconds <= 0.0))
    throw std::invalid_argument("log_to_features: window_seconds must be finite and positive");
  if (!std::isfinite(options.epsilon) || options.epsilon < 0.0)
    throw std::invalid_argument("log_to_features: epsilon must be finite and non-negative");
  if (options.variant != LogToFeaturesVariant::TRACE_BASED && !calendar_projection &&
      options.min_num_different_string_values > options.max_num_different_string_values)
    throw std::invalid_argument("log_to_features: string cardinality bounds are inverted");
  if (options.variant == LogToFeaturesVariant::TEMPORAL && !calendar_projection) {
    validate_names(options.event_string_attributes, "event_string_attributes");
    validate_names(options.event_numeric_attributes, "event_numeric_attributes");
    validate_names(options.case_string_attributes, "case_string_attributes");
    validate_names(options.case_numeric_attributes, "case_numeric_attributes");
  }
  if (!calendar_projection) {
    validate_names(options.event_attributes, "event_attributes");
    validate_names(options.case_attributes, "case_attributes");
  }

  LogToFeaturesResult out;
  out.variant = options.variant;
  if (!options.feature_names.empty()) out.feature_names = options.feature_names;

  std::vector<std::string> event_strings = options.event_string_attributes;
  std::vector<std::string> event_numbers = options.event_numeric_attributes;
  std::vector<std::string> case_strings = options.case_string_attributes;
  std::vector<std::string> case_numbers = options.case_numeric_attributes;
  // Generic lists are deterministic and infer their representation from data.
  for (const auto& n : options.event_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces) for (const auto& e : t.events)
      if (const auto* a = attr_for(e, n)) { numeric = native_numeric_cell(*a); break; }
    (numeric ? event_numbers : event_strings).push_back(n);
  }
  for (const auto& n : options.case_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces)
      if (const auto* a = attr_for(t, n)) { numeric = native_numeric_cell(*a); break; }
    (numeric ? case_numbers : case_strings).push_back(n);
  }
  const bool explicit_trace_selection = options.event_string_attributes_supplied ||
      options.event_numeric_attributes_supplied || options.case_string_attributes_supplied ||
      options.case_numeric_attributes_supplied || !event_strings.empty() ||
      !event_numbers.empty() || !case_strings.empty() || !case_numbers.empty();
  if (options.variant == LogToFeaturesVariant::TRACE_BASED) {
    if (!explicit_trace_selection) {
      event_strings = inferred_event_attributes(log, false, options.standard_keys);
      event_numbers = inferred_event_attributes(log, true, options.standard_keys);
      case_strings = inferred_trace_attributes(log, false, options.standard_keys);
      case_numbers = inferred_trace_attributes(log, true, options.standard_keys);
      if (options.enable_activity_def_representation &&
          std::find(event_strings.begin(), event_strings.end(), options.activity_key) == event_strings.end())
        event_strings.push_back(options.activity_key);
    }
  }

  if (options.variant == LogToFeaturesVariant::EVENT_BASED) {
    append_unique(event_strings, {}); append_unique(event_numbers, {});
    if (log.traces.empty())
      throw std::invalid_argument("log_to_features: event-based extraction requires a trace");
    const auto candidates = event_feature_keys(log, options.standard_keys);
    if (options.event_string_attributes.empty() && options.event_attributes.empty())
      event_strings = options.event_string_attributes_supplied ? std::vector<std::string>{} : candidates;
    if (options.event_numeric_attributes.empty() && options.event_attributes.empty())
      event_numbers = options.event_numeric_attributes_supplied ? std::vector<std::string>{} : candidates;
    std::erase_if(event_numbers, [&](const std::string& key) {
      return !complete_numeric_event_attribute(log, key);
    });
    std::map<std::string, std::set<std::string>> string_values;
    for (const auto& key : event_strings) {
      std::set<std::string> values;
      for (const auto& trace : log.traces) for (const auto& event : trace.events) {
        const auto cell = event_feature_cell(event, key);
        if (cell && native_string_cell(*cell)) values.insert(cell->value);
      }
      if (!values.empty() && values.size() >= options.min_num_different_string_values &&
          values.size() <= options.max_num_different_string_values)
        string_values.emplace(key, std::move(values));
    }
    std::vector<std::string> names;
    for (const auto& [key, values] : string_values)
      for (const auto& value : values) names.push_back("event:" + key + "@" + value);
    for (const auto& key : event_numbers) names.push_back("event:" + key);
    std::sort(names.begin(), names.end());
    if (options.feature_names.empty() && !options.feature_names_supplied) out.feature_names = names;
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < out.feature_names.size(); ++i) index.emplace(out.feature_names[i], i);
    std::size_t max_events = 0;
    for (const auto& trace : log.traces) max_events = std::max(max_events, trace.events.size());
    for (const auto& trace : log.traces) {
      std::vector<std::vector<double>> rows;
      for (const auto& event : trace.events) {
        std::vector<double> row(out.feature_names.size(), 0.0);
        const auto set = [&](const std::string& name, double value) {
          if (const auto found = index.find(name); found != index.end()) row[found->second] = value;
        };
        for (const auto& key : candidates) {
          const auto cell = event_feature_cell(event, key);
          if (!cell) continue;
          if (native_string_cell(*cell)) set("event:" + key + "@" + cell->value, 1.0);
          else if (native_numeric_cell(*cell) && index.contains("event:" + key))
            set("event:" + key, static_cast<float>(native_attribute_number(*cell)));
        }
        // Explicit lists can use native aliases that are not stored keys.
        for (const auto& key : event_strings) {
          if (std::binary_search(candidates.begin(), candidates.end(), key)) continue;
          const auto cell = event_feature_cell(event, key);
          if (cell && native_string_cell(*cell)) set("event:" + key + "@" + cell->value, 1.0);
        }
        for (const auto& key : event_numbers) {
          if (std::binary_search(candidates.begin(), candidates.end(), key)) continue;
          set("event:" + key, static_cast<float>(numeric_attribute_value(event, key, 0.0)));
        }
        rows.push_back(std::move(row));
      }
      while (rows.size() < max_events) rows.emplace_back(out.feature_names.size(), 0.0);
      out.event_rows.push_back(std::move(rows));
    }
    return out;
  }
  if (options.variant == LogToFeaturesVariant::TRACE_BASED) {
    const auto instant = [](const Event& event, const std::string& key, bool required) {
      const auto cell = event_feature_cell(event, key);
      if (!cell) {
        if (required) throw std::invalid_argument("log_to_features: missing timestamp");
        return std::numeric_limits<double>::quiet_NaN();
      }
      if (absent_attribute(*cell) || cell->container != AttributeContainerKind::kScalar ||
          !cell->children.empty() || cell->kind == AttributeKind::kBytes)
        throw std::invalid_argument("log_to_features: invalid timestamp attribute");
      const auto* date = std::get_if<NativeDate>(&cell->typed_value);
      if (!date && !std::holds_alternative<std::monostate>(cell->typed_value))
        throw std::invalid_argument("log_to_features: timestamp must be a date");
      const auto parsed = parse_timestamp(date ? date->value : cell->value);
      if (!parsed || !std::isfinite(*parsed))
        throw std::invalid_argument("log_to_features: invalid or non-finite timestamp");
      return *parsed;
    };
    const auto activities = categories_for_events(log, options.activity_key);
    std::set<std::pair<std::string, std::string>> observed_direct_paths;
    std::set<std::pair<std::string, std::string>> observed_indirect_paths;
    const bool indirect_paths_enabled = options.enable_indirect_paths_times_last_occ ||
        (options.enable_all_extra_features && !options.enable_indirect_paths_times_last_occ_supplied);
    for (const auto& trace : log.traces) {
      for (std::size_t i = 1; i < trace.events.size(); ++i) {
        observed_direct_paths.emplace(
            value_for(trace.events[i - 1], options.activity_key),
            value_for(trace.events[i], options.activity_key));
      }
      if (indirect_paths_enabled) for (std::size_t i = 0; i + 2 < trace.events.size(); ++i)
        for (std::size_t j = i + 2; j < trace.events.size(); ++j) {
          observed_indirect_paths.emplace(
              value_for(trace.events[i], options.activity_key),
              value_for(trace.events[j], options.activity_key));
        }
    }
    std::vector<std::string> names;
    auto add_name = [&](std::string name) {
      if (std::find(names.begin(), names.end(), name) == names.end())
        names.push_back(std::move(name));
    };
    for (const auto& n : case_strings) {
      for (const auto& value : distinct_trace_values(log, n))
        names.push_back("trace:" + n + "@" + value);
    }
    for (const auto& n : event_strings) {
      const auto values = distinct_event_values(log, n);
      for (const auto& value : values) names.push_back("event:" + n + "@" + value);
    }
    for (const auto& n : case_numbers) names.push_back("trace:" + n);
    for (const auto& n : event_numbers) names.push_back("event:" + n);
    std::vector<std::string> successor_attributes;
    if (explicit_trace_selection) successor_attributes = options.event_successor_attributes;
    else if (options.enable_succ_def_representation)
      successor_attributes.push_back(options.activity_key);
    const auto successions = [&](const Trace& trace, const std::string& key) {
      std::set<std::string> values;
      for (std::size_t i = 1; i < trace.events.size(); ++i)
        if (numeric_cell(trace.events[i - 1], key) && numeric_cell(trace.events[i], key))
          values.insert(event_attribute_value(trace.events[i - 1], key) + "#" +
                        event_attribute_value(trace.events[i], key));
      if (values.empty()) values.insert("UNDEFINED");
      return values;
    };
    for (const auto& key : successor_attributes) {
      std::set<std::string> values;
      for (const auto& trace : log.traces) {
        const auto local = successions(trace, key);
        values.insert(local.begin(), local.end());
      }
      for (const auto& value : values) names.push_back("succession:" + key + "@" + value);
    }
    const auto core_feature_count = names.size();
    const bool all_extra = options.enable_all_extra_features;
    const bool case_duration = options.enable_case_duration || (all_extra && !options.enable_case_duration_supplied);
    const bool case_id = options.add_case_identifier_column;
    const bool first_times = options.enable_times_from_first_occurrence || (all_extra && !options.enable_times_from_first_occurrence_supplied);
    const bool last_times = options.enable_times_from_last_occurrence || (all_extra && !options.enable_times_from_last_occurrence_supplied);
    const bool direct_paths = options.enable_direct_paths_times_last_occ || (all_extra && !options.enable_direct_paths_times_last_occ_supplied);
    const bool indirect_paths = indirect_paths_enabled;
    const bool wip = options.enable_work_in_progress || (all_extra && !options.enable_work_in_progress_supplied);
    const bool workload = options.enable_resource_workload || (all_extra && !options.enable_resource_workload_supplied);
    const bool indexes = options.enable_first_last_activity_index || (all_extra && !options.enable_first_last_activity_index_supplied);
    const bool max_concurrent = options.enable_max_concurrent_events || (all_extra && !options.enable_max_concurrent_events_supplied);
    const bool max_per_activity = options.enable_max_concurrent_events_per_activity || (all_extra && !options.enable_max_concurrent_events_per_activity_supplied);
    if (case_duration) add_name("@@caseDuration");
    if (first_times) for (const auto& a : activities) {
      add_name("startToFirstOcc@@" + a); add_name("firstOccToEnd@@" + a);
    }
    if (last_times) for (const auto& a : activities) {
      add_name("startToLastOcc@@" + a); add_name("lastOccToEnd@@" + a);
    }
    if (direct_paths)
      for (const auto& [from, to] : observed_direct_paths)
        add_name("directPathPerformanceLastOcc@@" + from + "##" + to);
    if (indirect_paths)
      for (const auto& [from, to] : observed_indirect_paths)
        add_name("indirectPathPerformanceLastOcc@@" + from + "##" + to);
    if (wip) add_name("@@work_in_progress");
    const auto resource_value = [&](const Event& event) -> std::optional<std::string> {
      // format_dataframe copies only the case, activity and timestamp
      // columns: a CSV's resource column keeps its header, so the default
      // org:resource key finds nothing when the header differs.
      if (options.standard_keys && !event.resource_source_key.empty() &&
          event.resource_source_key != options.resource_key &&
          std::none_of(event.attributes.begin(), event.attributes.end(),
                       [&](const EventAttribute& value) { return value.key == options.resource_key; }))
        return std::nullopt;
      const auto attribute = numeric_cell(event, options.resource_key);
      if (!attribute) return std::nullopt;
      if (!native_string_cell(*attribute))
        throw std::invalid_argument("log_to_features: workload resource must be a string");
      return attribute->value;
    };
    std::set<std::string> resources;
    if (workload) {
      for (const auto& t : log.traces) for (const auto& e : t.events)
        if (const auto resource = resource_value(e)) resources.insert(*resource);
      for (const auto& resource : resources) add_name("resource_workload@@" + resource);
    }
    if (indexes) for (const auto& a : activities) {
      add_name("firstIndexAct@@" + a); add_name("lastIndexAct@@" + a);
    }
    if (max_concurrent) add_name("@@max_concurrent_activities_general");
    if (max_per_activity) for (const auto& a : activities)
      add_name("@@max_concurrent_activities_like_" + a);
    if (!explicit_trace_selection || (options.feature_names.empty() && !options.feature_names_supplied))
      out.feature_names.assign(names.begin(), names.begin() + core_feature_count);
    if (case_id) out.feature_names.insert(out.feature_names.begin(), "@@case_id_column");
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = case_id ? 1 : 0; i < out.feature_names.size(); ++i) index[out.feature_names[i]] = i;
    std::unordered_map<std::string, std::size_t> extra_index;
    for (std::size_t i = core_feature_count; i < names.size(); ++i) {
      extra_index[names[i]] = out.feature_names.size();
      out.feature_names.push_back(names[i]);
    }
    const auto path_start = [&](const Event& event, bool required = false) {
      return instant(event,
                     options.start_timestamp_key.empty()
                         ? options.timestamp_key
                         : options.start_timestamp_key,
                     required);
    };
    struct CaseInterval {
      double start = std::numeric_limits<double>::quiet_NaN();
      double finish = std::numeric_limits<double>::quiet_NaN();
      bool valid = false;
      std::set<std::string> resources;
    };
    std::vector<CaseInterval> case_intervals(log.traces.size());
    std::map<std::string, std::vector<std::pair<double, double>>>
        resource_intervals;
    for (std::size_t ti = 0; ti < log.traces.size(); ++ti) {
      const auto& trace = log.traces[ti];
      if (!(wip || workload)) continue;
      if (trace.events.empty()) {
        if (workload) throw std::invalid_argument("log_to_features: workload requires nonempty traces");
        continue;
      }
      auto& interval = case_intervals[ti];
      interval.start = path_start(trace.events.front(), true) - options.epsilon;
      interval.finish = instant(trace.events.back(), options.timestamp_key, true) + options.epsilon;
      interval.valid = std::isfinite(interval.start) &&
                       std::isfinite(interval.finish) &&
                       interval.start < interval.finish;
      if (!interval.valid)
        throw std::invalid_argument("log_to_features: workload interval must have positive length");
      if (workload) for (const auto& event : trace.events)
        if (const auto resource = resource_value(event)) interval.resources.insert(*resource);
      for (const auto& resource : interval.resources)
        resource_intervals[resource].emplace_back(interval.start,
                                                   interval.finish);
    }
    std::size_t trace_index = 0;
    for (const auto& t : log.traces) {
      std::vector<LogToFeaturesCell> row(out.feature_names.size(), 0.0);
      auto set_core = [&](const std::string& n, double v) { if (auto i = index.find(n); i != index.end()) row[i->second] = v; };
      auto set = [&](const std::string& n, double v) { if (auto i = extra_index.find(n); i != extra_index.end()) row[i->second] = v; };
      for (std::size_t i = core_feature_count; i < names.size(); ++i)
        set(names[i], names[i].starts_with("firstIndexAct@@") || names[i].starts_with("lastIndexAct@@")
                          ? -1.0 : options.default_not_present);
      std::map<std::string, int> count; std::map<std::string, std::size_t> first, last;
      std::set<std::string> seen;
      for (std::size_t i = 0; i < t.events.size(); ++i) {
        const auto& e = t.events[i]; const auto a = value_for(e, options.activity_key); ++count[a]; if (!seen.count(a)) first[a] = i; last[a] = i; seen.insert(a);
      }
      for (const auto& [a,c] : count) if (indexes) { set("firstIndexAct@@"+a, static_cast<double>(first[a])); set("lastIndexAct@@"+a, static_cast<double>(last[a])); }
      for (const auto& n : event_strings) {
        for (const auto& value : event_attribute_values_for_trace(t, n))
          set_core("event:" + n + "@" + value, 1.0);
      }
      for (const auto& n : case_strings) set_core("trace:"+n+"@"+trace_attribute_value(t,n), 1.0);
      for (const auto& n : successor_attributes) for (const auto& value : successions(t, n)) set_core("succession:"+n+"@"+value, 1.0);
      if (case_duration) {
        const auto duration = t.events.empty() ? 0.0 :
            instant(t.events.back(), options.timestamp_key, true) -
            path_start(t.events.front(), true);
        set("@@caseDuration", duration);
      }
      for (const auto& n : event_numbers) {
        if (!index.contains("event:" + n)) continue;
        double value = options.default_not_present;
        bool found = false;
        for (const auto& e : t.events) {
          if (numeric_cell(e, n)) {
            value = numeric_attribute_value(e, n, options.default_not_present);
            found = true;
          }
        }
        if (!found)
          throw std::invalid_argument("log_to_features: trace has no numeric event attribute: " + n);
        set_core("event:" + n, value);
      }
      for (const auto& n : case_numbers)
        if (index.contains("trace:" + n)) set_core("trace:"+n,numeric_trace_value(t,n));
      if ((first_times || last_times) && !t.events.empty()) {
        const auto first_finish = instant(t.events.front(), options.timestamp_key, true);
        const auto last_start = path_start(t.events.back(), true);
        const auto set_occurrence_times = [&](const std::string& activity,
                                               std::size_t position,
                                               const std::string& start_name,
                                               const std::string& end_name) {
          const auto& event = t.events[position];
          const auto start = path_start(event, true);
          const auto finish = instant(event, options.timestamp_key, true);
          if (std::isfinite(start) && std::isfinite(first_finish))
            set(start_name + activity, start - first_finish);
          if (std::isfinite(finish) && std::isfinite(last_start))
            set(end_name + activity, last_start - finish);
        };
        for (const auto& [activity, position] : first) {
          if (first_times)
            set_occurrence_times(activity, position, "startToFirstOcc@@", "firstOccToEnd@@");
          if (last_times)
            set_occurrence_times(activity, last.at(activity), "startToLastOcc@@", "lastOccToEnd@@");
        }
      }
      if (direct_paths) for (std::size_t i = 1; i < t.events.size(); ++i) {
        const auto from = value_for(t.events[i - 1], options.activity_key);
        const auto to = value_for(t.events[i], options.activity_key);
        const auto finish = instant(t.events[i - 1], options.timestamp_key, false);
        const auto start = path_start(t.events[i]);
        if (std::isfinite(finish) && std::isfinite(start) && start > finish)
          set("directPathPerformanceLastOcc@@" + from + "##" + to,
              start - finish);
      }
      if (indirect_paths)
        for (std::size_t i = 0; i + 2 < t.events.size(); ++i)
          for (std::size_t j = i + 2; j < t.events.size(); ++j) {
            const auto from = value_for(t.events[i], options.activity_key);
            const auto to = value_for(t.events[j], options.activity_key);
            const auto finish = instant(t.events[i], options.timestamp_key, false);
            const auto start = path_start(t.events[j]);
            if (std::isfinite(finish) && std::isfinite(start) && start > finish)
              set("indirectPathPerformanceLastOcc@@" + from + "##" + to,
                  start - finish);
          }
      if (wip) {
        const auto& interval = case_intervals[trace_index];
        if (interval.valid) {
          std::size_t overlapping = 0;
          for (const auto& other : case_intervals) {
            if (other.valid && other.start < interval.finish &&
                other.finish > interval.start)
              ++overlapping;
          }
          set("@@work_in_progress", static_cast<double>(overlapping));
        }
      }
      if (workload) {
        const auto& interval = case_intervals[trace_index];
        if (interval.valid) {
          for (const auto& resource : interval.resources) {
            const auto found = resource_intervals.find(resource);
            if (found == resource_intervals.end()) continue;
            std::size_t overlapping = 0;
            for (const auto& other : found->second) {
              if (other.first < interval.finish && other.second > interval.start)
                ++overlapping;
            }
            set("resource_workload@@" + resource,
                static_cast<double>(overlapping));
          }
        }
      }
      if (max_concurrent || max_per_activity) {
        int general = 0;
        std::map<std::string, int> per_activity;
        if (max_per_activity)
          for (const auto& activity : activities)
            set("@@max_concurrent_activities_like_" + activity, 0.0);
        for (std::size_t i = 0; i + 1 < t.events.size(); ++i) {
          const auto finish = instant(t.events[i], options.timestamp_key, true);
          const auto activity = value_for(t.events[i], options.activity_key);
          int active = 0;
          int same_activity = 0;
          for (std::size_t j = i + 1; j < t.events.size(); ++j) {
            const auto start = path_start(t.events[j], true);
            if (start > finish)
              break;
            ++active;
            if (value_for(t.events[j], options.activity_key) == activity)
              ++same_activity;
          }
          general = std::max(general, active);
          per_activity[activity] =
              std::max(per_activity[activity], same_activity);
        }
        if (max_concurrent)
          set("@@max_concurrent_activities_general",
              static_cast<double>(general));
        if (max_per_activity)
          for (const auto& [activity, overlap_count] : per_activity)
            set("@@max_concurrent_activities_like_" + activity,
                static_cast<double>(overlap_count));
      }
      if (case_id) {
        const auto key = options.case_id_key.value_or("concept:name");
        const auto identifier = numeric_cell(t, key);
        if (!identifier)
          throw std::invalid_argument("log_to_features: missing case identifier attribute: " + key);
        row.front() = *identifier;
        out.case_ids.push_back(t.case_id);
      }
      out.trace_rows.push_back(std::move(row));
      ++trace_index;
    }
    return out;
  }

  if (options.variant != LogToFeaturesVariant::TEMPORAL) throw std::invalid_argument("log_to_features: unknown variant");
  const std::optional<TemporalFrequency> temporal_frequency =
      options.grouper_freq.empty()
          ? std::nullopt
          : std::optional<TemporalFrequency>(
                parse_temporal_frequency(options.grouper_freq));
  std::vector<TimedEvent> events;
  double origin = std::numeric_limits<double>::infinity();
  struct TypedCategories {
    std::vector<EventAttribute> values;
    std::vector<EventAttribute> lookup_keys;
    EventAttribute tuple_nan = [] {
      EventAttribute value{"", "", AttributeKind::kFloat};
      value.typed_value = std::numeric_limits<double>::quiet_NaN();
      return value;
    }();
    std::unordered_map<std::string, std::vector<std::size_t>> buckets;
    std::size_t insert(const EventAttribute& value) {
      auto normalized = value;
      const auto normalize_null = [](auto&& self, EventAttribute& cell) -> void {
        if (absent_attribute(cell)) {
          cell.is_null = true;
          cell.container = AttributeContainerKind::kScalar;
          cell.children.clear();
          cell.scalar_wrapper = false;
          cell.scalar_children_list = false;
          cell.typed_value = std::monostate{};
        } else {
          for (auto& child : cell.children) self(self, child);
        }
      };
      normalize_null(normalize_null, normalized);
      if (!root_cause_key_is_hashable(normalized))
        throw std::invalid_argument("log_to_features: unhashable temporal category");
      auto lookup = normalized;
      const auto normalize_tuple_nan = [&](auto&& self, EventAttribute& tuple) -> void {
        if (tuple.container != AttributeContainerKind::kTuple) return;
        for (auto& child : tuple.children) {
          if (child.container == AttributeContainerKind::kTuple) {
            self(self, child);
            continue;
          }
          if (child.is_null || child.container != AttributeContainerKind::kScalar ||
              !child.children.empty() || child.kind == AttributeKind::kBytes) continue;
          const auto* number = std::get_if<double>(&child.typed_value);
          if ((number && std::isnan(*number)) ||
              (!number && child.kind == AttributeKind::kFloat &&
               std::holds_alternative<std::monostate>(child.typed_value) &&
               std::isnan(native_attribute_number(child)))) child = tuple_nan;
        }
      };
      normalize_tuple_nan(normalize_tuple_nan, lookup);
      auto& candidates = buckets[root_cause_scalar_key_fingerprint(lookup)];
      for (const auto candidate : candidates)
        if (root_cause_scalar_keys_equal(lookup_keys[candidate], lookup))
          return candidate;
      const auto index = values.size();
      values.push_back(std::move(normalized));
      lookup_keys.push_back(std::move(lookup));
      candidates.push_back(index);
      return index;
    }
  } activity_categories, resource_categories, case_categories;
  const auto validate_scalar_group = [](const auto& values, const auto& kind_of) {
    std::optional<NativeScalarOrderKind> nonstring_kind;
    for (const auto& value : values) {
      const auto kind = kind_of(value);
      if (kind == NativeScalarOrderKind::kUnsupported || kind == NativeScalarOrderKind::kString) continue;
      if (nonstring_kind && *nonstring_kind != kind)
        throw std::invalid_argument("log_to_features: incompatible temporal scalar ordering types");
      nonstring_kind = kind;
    }
  };
  struct EventCategories {
    std::optional<std::string> activity;
    std::optional<std::string> resource;
  };
  std::unordered_map<const Event*, EventCategories> temporal_categories;
  bool have_activity_column = false;
  bool have_resource_column = false;
  bool have_case_column = false;
  const auto temporal_case_key = options.case_id_key.value_or("case:concept:name");
  std::unordered_map<const Event*, std::unordered_map<std::string, EventAttribute>> virtual_columns;
  const auto projected_column_value = [&](const Trace& trace, const Event& event, const std::string& key) {
    if (const auto row = virtual_columns.find(&event); row != virtual_columns.end())
      if (const auto value = row->second.find(key); value != row->second.end())
        return std::optional<EventAttribute>{value->second};
    if (key.starts_with("case:")) {
      const auto trace_key = key.substr(5);
      const bool projected_identifier = trace_key == "concept:name" ||
          (!trace.case_id_source_key.empty() && trace_key == trace.case_id_source_key) ||
          (trace.case_id_attribute &&
           (trace_key == trace.case_id_attribute->key ||
            (!trace.case_id_attribute->first_class_source_key.empty() &&
             trace_key == trace.case_id_attribute->first_class_source_key)));
      if (projected_identifier) {
        if (auto value = numeric_cell(trace, trace_key)) return value;
      } else {
        Event context;
        context.attributes = trace.attributes;
        if (auto value = native_event_attribute(context, trace_key)) return value;
      }
    }
    return event_feature_cell(event, key);
  };
  const auto case_value = [&](const Trace& trace, const Event& event) {
    return projected_column_value(trace, event, temporal_case_key);
  };
  std::unordered_map<std::string, std::optional<int>> calendar_timezones;
  std::map<std::pair<const Event*, std::string>, ExactTemporalInstant> exact_calendar_instants;
  std::unordered_map<std::string, bool> nanosecond_columns;
  const auto calendar_instant = [&](const Trace& trace, const Event& event,
                                      const std::string& key, bool required) {
    const auto value = projected_column_value(trace, event, key);
    if (!value || absent_attribute(*value)) {
      if (required) throw std::invalid_argument("log_to_features: missing temporal timestamp");
      return std::numeric_limits<double>::quiet_NaN();
    }
    if (value->container != AttributeContainerKind::kScalar || !value->children.empty() ||
        value->kind == AttributeKind::kBytes)
      throw std::invalid_argument("log_to_features: invalid temporal timestamp attribute");
    const auto* native_date = std::get_if<NativeDate>(&value->typed_value);
    if (!native_date && (value->kind != AttributeKind::kDate ||
        !std::holds_alternative<std::monostate>(value->typed_value)))
      throw std::invalid_argument("log_to_features: temporal timestamp must be a date");
    std::string lexical = native_date ? native_date->value : value->value;
    const bool nanos = native_date && native_date->resolution == NativeDateResolution::kNanosecond;
    nanosecond_columns[key] = nanosecond_columns[key] || nanos;
    const auto point = lexical.find('.', lexical.find_first_of("Tt "));
    if (point != std::string::npos) {
      auto end = point + 1;
      while (end < lexical.size() && std::isdigit(static_cast<unsigned char>(lexical[end]))) ++end;
      const auto keep = point + 1 + (nanos ? 9 : 6);
      if (end > keep) lexical.erase(keep, end - keep);
    }
    std::string_view text = lexical;
    const auto parsed = parse_timestamp(text);
    if (!parsed || !std::isfinite(*parsed))
      throw std::invalid_argument("log_to_features: invalid temporal timestamp");
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    std::optional<int> offset;
    if (!text.empty() && (text.back() == 'Z' || text.back() == 'z')) {
      offset = 0;
    } else if (const auto sign = text.find_last_of("+-"); sign != std::string_view::npos && sign > 10) {
      const auto zone = text.substr(sign + 1);
      const auto hours = (zone[0] - '0') * 10 + zone[1] - '0';
      const auto minute_index = zone.size() > 2 && zone[2] == ':' ? 3u : 2u;
      const auto minutes = zone.size() >= minute_index + 2
          ? (zone[minute_index] - '0') * 10 + zone[minute_index + 1] - '0' : 0;
      offset = (text[sign] == '-' ? -1 : 1) * (hours * 3600 + minutes * 60);
    }
    const auto [timezone, inserted] = calendar_timezones.try_emplace(key, offset);
    if (!inserted && timezone->second != offset)
      throw std::invalid_argument("log_to_features: temporal timestamp column has mixed timezones");
    const auto exact = exact_temporal_instant(lexical);
    exact_calendar_instants[{&event, key}] = exact;
    return temporal_metric_instant(exact, nanos);
  };
  const auto missing_category = [](const EventAttribute& value) {
    if (absent_attribute(value)) return true;
    if (value.container != AttributeContainerKind::kScalar || !value.children.empty() ||
        value.kind == AttributeKind::kBytes) return false;
    if (const auto* number = std::get_if<double>(&value.typed_value)) return std::isnan(*number);
    return value.kind == AttributeKind::kFloat &&
        std::holds_alternative<std::monostate>(value.typed_value) &&
        std::isnan(native_attribute_number(value));
  };
  const auto promote_numeric_column = [&](const auto& selected_value) {
    bool numeric = true, floating = false;
    bool unsigned_integer = false, negative_integer = false;
    const EventAttribute minimum{"", "-9223372036854775808", AttributeKind::kInt};
    const EventAttribute maximum{"", "18446744073709551615", AttributeKind::kInt};
    const EventAttribute signed_maximum{"", "9223372036854775807", AttributeKind::kInt};
    const EventAttribute zero{"", "0", AttributeKind::kInt};
    for (const auto& trace : log.traces) for (const auto& event : trace.events) {
      const auto value = selected_value(trace, event);
      if (!value || missing_category(*value)) {
        floating = true;
        continue;
      }
      if (native_scalar_order_kind(*value) != NativeScalarOrderKind::kNumeric ||
          std::holds_alternative<bool>(value->typed_value) ||
          (std::holds_alternative<std::monostate>(value->typed_value) && value->kind == AttributeKind::kBoolean)) {
        numeric = false;
        continue;
      }
      if (std::holds_alternative<double>(value->typed_value) ||
          (std::holds_alternative<std::monostate>(value->typed_value) && value->kind == AttributeKind::kFloat))
        floating = true;
      else {
        if (native_scalar_key_compare(*value, minimum) < 0 || native_scalar_key_compare(*value, maximum) > 0)
          numeric = false;
        unsigned_integer = unsigned_integer || native_scalar_key_compare(*value, signed_maximum) > 0;
        negative_integer = negative_integer || native_scalar_key_compare(*value, zero) < 0;
      }
    }
    return numeric && floating && !(unsigned_integer && negative_integer);
  };
  const auto activity_value = [&](const Trace& trace, const Event& event) {
    return projected_column_value(trace, event, options.activity_key);
  };
  const auto resource_value = [&](const Trace& trace, const Event& event) {
    return projected_column_value(trace, event, options.resource_key);
  };
  const bool promote_cases = temporal_frequency && promote_numeric_column(case_value);
  const auto promoted_value = [](EventAttribute value, bool promote) {
    if (promote) {
      const auto number = std::holds_alternative<std::int64_t>(value.typed_value)
          ? static_cast<double>(std::get<std::int64_t>(value.typed_value))
          : std::holds_alternative<std::uint64_t>(value.typed_value)
              // Temporal frequency's promoted numeric output is float64 by
              // contract; make this narrowing explicit after exact scalar
              // classification and key comparison.
              ? static_cast<double>(std::get<std::uint64_t>(value.typed_value))
              : std::holds_alternative<double>(value.typed_value)
                  ? std::get<double>(value.typed_value)
                  : native_attribute_number(value);
      value.kind = AttributeKind::kFloat;
      value.typed_value = number;
    }
    return value;
  };
  struct CaseTemporal {
    double start = std::numeric_limits<double>::infinity();
    double finish = -std::numeric_limits<double>::infinity();
    double service = 0.0;
    std::size_t event_count = 0;
    std::set<std::string> activities;
    std::set<std::string> resources;
  };
  const auto no_case = std::numeric_limits<std::size_t>::max();
  std::unordered_map<const Event*, const Trace*> event_traces;
  std::vector<CaseTemporal> cases(temporal_frequency ? 0 : log.traces.size());
  for (std::size_t ti = 0; ti < log.traces.size(); ++ti) {
    for (std::size_t i = 0; i < log.traces[ti].events.size(); ++i) {
      const auto& event = log.traces[ti].events[i];
      event_traces[&event] = &log.traces[ti];
      std::size_t case_index = ti;
      if (temporal_frequency) {
        const auto identifier = case_value(log.traces[ti], event);
        case_index = no_case;
        if (identifier) {
          have_case_column = true;
          if (!missing_category(*identifier)) {
            case_index = case_categories.insert(promoted_value(*identifier, promote_cases));
            if (cases.size() <= case_index) cases.resize(case_index + 1);
          }
        }
      }
      const double finish = temporal_frequency
          ? calendar_instant(log.traces[ti], event, options.timestamp_key, false)
          : instant(event, options.timestamp_key, true);
      const double start = options.start_timestamp_key.empty()
                               ? finish
                               : temporal_frequency
                                   ? calendar_instant(log.traces[ti], event, options.start_timestamp_key, false)
                                   : instant(event, options.start_timestamp_key, true);
      if (!temporal_frequency && start > finish)
        throw std::invalid_argument("log_to_features: start timestamp after finish");
      if (std::isfinite(start)) origin = std::min(origin, start);
      events.push_back({&event, finish, start, case_index, i});
      if (case_index == no_case) continue;
      auto& metrics = cases[case_index];
      if (!temporal_frequency || std::isfinite(finish)) {
        metrics.start = std::min(metrics.start, temporal_frequency ? finish : start);
        metrics.finish = std::max(metrics.finish, finish);
      }
      if (!temporal_frequency) metrics.service += finish - start;
      ++metrics.event_count;
      metrics.activities.insert(value_for(event, options.activity_key));
      const auto resource = value_for(event, options.resource_key);
      if (!resource.empty()) metrics.resources.insert(resource);
    }
  }
  if (temporal_frequency && (!have_case_column || cases.empty()))
    throw std::invalid_argument("log_to_features: temporal case identifiers are required");
  if (temporal_frequency)
    for (const auto& metrics : cases)
      if (!std::isfinite(metrics.start) || !std::isfinite(metrics.finish))
        throw std::invalid_argument("log_to_features: temporal case has no completion date");
  out.feature_names = options.feature_names;
  if (temporal_frequency || out.feature_names.empty()) {
    out.feature_names = {
        "unique_resources", "unique_activities", "num_events",
        "average_arrival_rate", "average_finish_rate", "average_waiting_time",
        "average_sojourn_time", "average_service_time",
        "total_number_of_reworked_activities", "avg_cases_per_resource",
        "avg_events_per_case", "number_of_cases", "avg_resources_per_case"};
  } else {
    validate_names(out.feature_names, "feature_names");
  }
  if (events.empty()) return out;
  std::vector<std::size_t> arrival_order;
  std::vector<std::size_t> finish_order;
  for (std::size_t i = 0; i < cases.size(); ++i) {
    if (temporal_frequency && cases[i].event_count == 0) continue;
    arrival_order.push_back(i);
    finish_order.push_back(i);
  }
  const auto supports_case_order = [&](auto&& self, const EventAttribute& value) -> bool {
    if (absent_attribute(value)) return true;
    if (missing_category(value)) return false;
    if (value.container == AttributeContainerKind::kTuple)
      return std::all_of(value.children.begin(), value.children.end(),
          [&](const auto& child) { return self(self, child); });
    return native_scalar_order_kind(value) != NativeScalarOrderKind::kUnsupported;
  };
  const auto compare_case_keys = [](auto&& self, const EventAttribute& left,
                                   const EventAttribute& right) -> int {
    if (root_cause_scalar_keys_equal(left, right)) return 0;
    if (left.container == AttributeContainerKind::kTuple && right.container == AttributeContainerKind::kTuple) {
      for (std::size_t i = 0; i < std::min(left.children.size(), right.children.size()); ++i) {
        const auto comparison = self(self, left.children[i], right.children[i]);
        if (comparison != 0) return comparison;
      }
      return left.children.size() < right.children.size() ? -1 : 1;
    }
    if (left.container == AttributeContainerKind::kTuple || right.container == AttributeContainerKind::kTuple ||
        absent_attribute(left) || absent_attribute(right))
      throw std::invalid_argument("log_to_features: incompatible temporal case ordering types");
    return native_scalar_key_compare(left, right);
  };
  const bool ordered_case_keys = temporal_frequency &&
      std::all_of(case_categories.values.begin(), case_categories.values.end(),
          [&](const auto& value) { return supports_case_order(supports_case_order, value); });
  const auto object_less = [&](auto&& self, const EventAttribute& left,
                               const EventAttribute& right, bool grouping) -> bool {
    if (root_cause_scalar_keys_equal(left, right)) return false;
    if (grouping) {
      const auto rank = [&](const EventAttribute& cell) {
        if (absent_attribute(cell) || missing_category(cell)) return 2;
        return native_scalar_order_kind(cell) == NativeScalarOrderKind::kString ? 1 : 0;
      };
      if (rank(left) != rank(right)) return rank(left) < rank(right);
      if (rank(left) == 2) return false;
    }
    const bool left_tuple = left.container == AttributeContainerKind::kTuple;
    const bool right_tuple = right.container == AttributeContainerKind::kTuple;
    if (left_tuple || right_tuple) {
      const auto sequence = [](const EventAttribute& cell) {
        if (cell.container == AttributeContainerKind::kTuple) return cell.children;
        std::vector<EventAttribute> children;
        for (const unsigned char byte : decode_bytes_attribute(cell)) {
          EventAttribute child{"", "", AttributeKind::kInt};
          child.typed_value = static_cast<std::int64_t>(byte);
          children.push_back(std::move(child));
        }
        return children;
      };
      if ((!left_tuple && (!grouping || native_scalar_order_kind(left) != NativeScalarOrderKind::kBytes)) ||
          (!right_tuple && (!grouping || native_scalar_order_kind(right) != NativeScalarOrderKind::kBytes)))
        throw std::invalid_argument("log_to_features: incompatible temporal tuple ordering types");
      const auto a = sequence(left), b = sequence(right);
      for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        if (!root_cause_scalar_keys_equal(a[i], b[i])) return self(self, a[i], b[i], grouping);
      return a.size() < b.size();
    }
    if (left.container == AttributeContainerKind::kFrozenSet || right.container == AttributeContainerKind::kFrozenSet) {
      if (left.container != AttributeContainerKind::kFrozenSet || right.container != AttributeContainerKind::kFrozenSet)
        throw std::invalid_argument("log_to_features: incompatible temporal frozen ordering types");
      const auto a = root_cause_set_members(left), b = root_cause_set_members(right);
      return a.size() < b.size() && std::all_of(a.begin(), a.end(), [&](const auto* member) {
        return std::any_of(b.begin(), b.end(), [&](const auto* other) {
          return root_cause_scalar_keys_equal(*member, *other);
        });
      });
    }
    if (absent_attribute(left) || absent_attribute(right))
      throw std::invalid_argument("log_to_features: incompatible temporal null ordering types");
    if (native_scalar_order_kind(left) == NativeScalarOrderKind::kNumeric &&
        native_scalar_order_kind(right) == NativeScalarOrderKind::kNumeric &&
        (missing_category(left) || missing_category(right))) return false;
    return native_scalar_key_compare(left, right) < 0;
  };
  const auto validate_tuple_group = [&](const auto& values, const auto& value_of) {
    std::vector<const EventAttribute*> keys;
    bool have_container = false;
    for (const auto& value : values) {
      const auto& key = value_of(value);
      have_container = have_container || key.container != AttributeContainerKind::kScalar;
      keys.push_back(&key);
    }
    if (!have_container) return;
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < keys.size(); ++i) order.push_back(i);
    temporal_object_sort(order, [&](std::size_t left, std::size_t right) {
      return object_less(object_less, *keys[left], *keys[right], true);
    });
  };
  if (temporal_frequency) {
    validate_scalar_group(case_categories.values, native_scalar_order_kind);
    validate_tuple_group(case_categories.values, [](const EventAttribute& value) -> const EventAttribute& { return value; });
  }
  std::vector<std::size_t> object_ranks;
  if (temporal_frequency && !ordered_case_keys) {
    auto order = arrival_order;
    const auto group_key_less = [&](std::size_t left, std::size_t right) {
      return object_less(object_less, case_categories.values[left], case_categories.values[right], true);
    };
    temporal_object_sort(order, group_key_less);
    object_ranks.resize(order.size());
    for (std::size_t rank = 0; rank < order.size(); ++rank) object_ranks[order[rank]] = rank;
  }
  auto case_less = [&](std::size_t lhs, std::size_t rhs, bool by_finish) {
    const double a = by_finish ? cases[lhs].finish : cases[lhs].start;
    const double b = by_finish ? cases[rhs].finish : cases[rhs].start;
    if (a != b) return a < b;
    if (!object_ranks.empty()) return object_ranks[lhs] < object_ranks[rhs];
    if (ordered_case_keys)
      return compare_case_keys(compare_case_keys, case_categories.values[lhs], case_categories.values[rhs]) < 0;
    if (!temporal_frequency && log.traces[lhs].case_id != log.traces[rhs].case_id)
      return log.traces[lhs].case_id < log.traces[rhs].case_id;
    return lhs < rhs;
  };
  const auto sort_endpoints = [&](std::vector<std::size_t>& order, bool by_finish) {
    if (object_ranks.empty()) {
      std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        return case_less(left, right, by_finish);
      });
      return;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
      return object_ranks[left] < object_ranks[right];
    });
    const auto less = [&](std::size_t left, std::size_t right) {
      const auto a = by_finish ? cases[left].finish : cases[left].start;
      const auto b = by_finish ? cases[right].finish : cases[right].start;
      if (a != b) return a < b;
      return object_less(object_less, case_categories.values[left], case_categories.values[right], false);
    };
    auto minimum = order.size();
    std::size_t carry = 0;
    while (minimum >= 64) { carry |= minimum & 1; minimum >>= 1; }
    minimum += carry;
    struct Run { std::size_t first, length; };
    std::vector<Run> runs;
    int gallop_threshold = 7;
    const auto gallop = [&](const std::deque<std::size_t>& values, std::size_t key,
                            bool upper_bound, bool from_back) {
      const auto before = [&](std::size_t at) {
        return upper_bound ? !less(key, values[at]) : less(values[at], key);
      };
      const auto hint = from_back ? values.size() - 1 : 0;
      std::size_t low = 0, high = values.size(), step = 1;
      if (before(hint)) {
        low = hint + 1;
        while (step < values.size() - hint) {
          if (!before(hint + step)) { high = hint + step; break; }
          low = hint + step + 1;
          step = step > values.size() / 2 ? values.size() : step * 2 + 1;
        }
      } else {
        high = hint;
        while (step <= hint) {
          if (before(hint - step)) { low = hint - step + 1; break; }
          high = hint - step;
          step = step > values.size() / 2 ? values.size() : step * 2 + 1;
        }
      }
      while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (before(middle)) low = middle + 1;
        else high = middle;
      }
      return low;
    };
    const auto merge = [&](std::size_t index) {
      const auto left_run = runs[index], right_run = runs[index + 1];
      std::deque<std::size_t> left(order.begin() + left_run.first, order.begin() + right_run.first);
      std::deque<std::size_t> right(order.begin() + right_run.first,
                                    order.begin() + right_run.first + right_run.length);
      std::vector<std::size_t> combined, output, suffix;
      combined.reserve(left.size() + right.size());
      const auto prefix_count = gallop(left, right.front(), true, false);
      for (std::size_t i = 0; i < prefix_count; ++i) {
        combined.push_back(left.front()); left.pop_front();
      }
      if (!left.empty()) {
        const auto retained = gallop(right, left.back(), false, true);
        suffix.assign(right.begin() + retained, right.end());
        right.erase(right.begin() + retained, right.end());
      }
      const bool backward = left.size() > right.size();
      const auto take = [&](std::deque<std::size_t>& source) {
        output.push_back(backward ? source.back() : source.front());
        if (backward) source.pop_back(); else source.pop_front();
      };
      if (!left.empty() && !right.empty()) take(backward ? left : right);
      int left_wins = 0, right_wins = 0;
      while (!left.empty() && !right.empty()) {
        if (backward && right.size() == 1) {
          while (!left.empty()) take(left);
          break;
        }
        if (!backward && left.size() == 1) {
          while (!right.empty()) take(right);
          break;
        }
        const bool take_left = backward ? less(right.back(), left.back()) : !less(right.front(), left.front());
        if (take_left) { take(left); ++left_wins; right_wins = 0; }
        else { take(right); ++right_wins; left_wins = 0; }
        if (std::max(left_wins, right_wins) < gallop_threshold) continue;
        while (!left.empty() && !right.empty()) {
          auto cut = gallop(left, backward ? right.back() : right.front(), true, backward);
          const auto left_count = backward ? left.size() - cut : cut;
          for (std::size_t i = 0; i < left_count; ++i) take(left);
          if (left.empty() || (!backward && left.size() == 1)) break;
          take(right);
          if (right.empty()) break;
          cut = gallop(right, backward ? left.back() : left.front(), false, backward);
          const auto right_count = backward ? right.size() - cut : cut;
          for (std::size_t i = 0; i < right_count; ++i) take(right);
          if (right.empty() || (backward && right.size() == 1)) break;
          take(left);
          if ((backward && right.size() <= 1) || (!backward && left.size() <= 1)) break;
          --gallop_threshold;
          if (left_count < 7 && right_count < 7) break;
        }
        if (!left.empty() && !right.empty() &&
            !((backward && right.size() == 1) || (!backward && left.size() == 1)))
          gallop_threshold = std::max(gallop_threshold, 0) + 2;
        else gallop_threshold = std::max(gallop_threshold, 1);
        left_wins = right_wins = 0;
      }
      if (backward) {
        combined.insert(combined.end(), left.begin(), left.end());
        combined.insert(combined.end(), right.begin(), right.end());
        combined.insert(combined.end(), output.rbegin(), output.rend());
      } else {
        combined.insert(combined.end(), output.begin(), output.end());
        combined.insert(combined.end(), left.begin(), left.end());
        combined.insert(combined.end(), right.begin(), right.end());
      }
      combined.insert(combined.end(), suffix.begin(), suffix.end());
      std::copy(combined.begin(), combined.end(), order.begin() + left_run.first);
      runs[index].length += right_run.length;
      runs.erase(runs.begin() + index + 1);
    };
    for (std::size_t first = 0; first < order.size();) {
      auto end = first + 1;
      if (end < order.size()) {
        const bool descending = less(order[end], order[first]);
        ++end;
        while (end < order.size() && (descending ? less(order[end], order[end - 1])
                                                : !less(order[end], order[end - 1]))) ++end;
        if (descending) std::reverse(order.begin() + first, order.begin() + end);
      }
      const auto extended = std::min(order.size(), first + std::max(minimum, end - first));
      for (auto i = end; i < extended; ++i) {
        const auto value = order[i];
        auto low = first, high = i;
        while (low < high) {
          const auto middle = low + (high - low) / 2;
          if (less(value, order[middle])) high = middle;
          else low = middle + 1;
        }
        std::move_backward(order.begin() + low, order.begin() + i, order.begin() + i + 1);
        order[low] = value;
      }
      runs.push_back({first, extended - first});
      first = extended;
    }
    const auto natural_runs = std::move(runs);
    runs.clear();
    std::vector<std::size_t> powers;
    if (order.size() > std::numeric_limits<std::size_t>::max() / 4)
      throw std::invalid_argument("log_to_features: temporal ordering range overflows");
    const auto denominator = 2 * order.size();
    for (std::size_t i = 0; i < natural_runs.size(); ++i) {
      if (i > 0) {
        const auto previous = natural_runs[i - 1], current = natural_runs[i];
        auto left_midpoint = 2 * previous.first + previous.length;
        auto right_midpoint = 2 * current.first + current.length;
        std::size_t power = 0;
        while ((left_midpoint >= denominator) == (right_midpoint >= denominator)) {
          left_midpoint = (left_midpoint % denominator) * 2;
          right_midpoint = (right_midpoint % denominator) * 2;
          ++power;
        }
        while (!powers.empty() && powers.back() > power) {
          merge(runs.size() - 2);
          powers.pop_back();
        }
        powers.push_back(power);
      }
      runs.push_back(natural_runs[i]);
    }
    while (runs.size() > 1) merge(runs.size() - 2);
  };
  sort_endpoints(arrival_order, false);
  sort_endpoints(finish_order, true);
  std::vector<double> arrival_rate(cases.size(), 0.0);
  std::vector<double> finish_rate(cases.size(), 0.0);
  for (std::size_t i = 1; i < arrival_order.size(); ++i)
    arrival_rate[arrival_order[i]] = cases[arrival_order[i]].start - cases[arrival_order[i - 1]].start;
  for (std::size_t i = 1; i < finish_order.size(); ++i)
    finish_rate[finish_order[i]] = cases[finish_order[i]].finish - cases[finish_order[i - 1]].finish;
  const auto scratch_number = [&](const Event& event, const std::string& key) {
    const auto value = projected_column_value(*event_traces.at(&event), event, key);
    if (!value || missing_category(*value)) return std::numeric_limits<double>::quiet_NaN();
    return native_attribute_number(*value);
  };
  if (temporal_frequency) {
    for (const auto* destination : {&options.arrival_rate, &options.finish_rate,
                                    &options.service_time, &options.sojourn_time,
                                    &options.waiting_time})
      if (*destination == temporal_case_key)
        throw std::invalid_argument("log_to_features: temporal metric destination replaces the case column");
    const auto write_column = [&](const std::string& key, const std::vector<double>& values) {
      for (std::size_t i = 0; i < events.size(); ++i) {
        EventAttribute value{key, "", AttributeKind::kFloat};
        value.typed_value = values[i];
        virtual_columns[events[i].event][key] = std::move(value);
      }
    };
    const auto locate_case = [&](const Event& event) {
      const auto identifier = case_value(*event_traces.at(&event), event);
      if (!identifier || missing_category(*identifier)) return no_case;
      const auto previous_size = case_categories.values.size();
      const auto group = case_categories.insert(promoted_value(*identifier, promote_cases));
      if (group >= previous_size)
        throw std::invalid_argument("log_to_features: derived column changed a temporal case identifier");
      return group;
    };
    const auto write_case_column = [&](const std::string& key, const std::vector<double>& values) {
      std::vector<double> event_values;
      event_values.reserve(events.size());
      for (const auto& event : events) {
        const auto group = locate_case(*event.event);
        event_values.push_back(group == no_case ? std::numeric_limits<double>::quiet_NaN() : values.at(group));
      }
      write_column(key, event_values);
    };
    write_case_column(options.arrival_rate, arrival_rate);
    write_case_column(options.finish_rate, finish_rate);
    std::vector<double> differences;
    differences.reserve(events.size());
    for (const auto& event : events) {
      const auto finish = calendar_instant(*event_traces.at(event.event), *event.event, options.timestamp_key, false);
      differences.push_back(finish - finish);
    }
    write_column(options.diff_start_end, differences);

    // Grouping observes the diff destination before the case-level writes.
    case_categories = {};
    cases.clear();
    for (auto& event : events) {
      const auto& trace = *event_traces.at(event.event);
      const auto identifier = case_value(trace, *event.event);
      event.trace = no_case;
      if (!identifier || missing_category(*identifier)) continue;
      event.trace = case_categories.insert(promoted_value(*identifier, promote_cases));
      if (cases.size() <= event.trace) cases.resize(event.trace + 1);
      auto& metrics = cases[event.trace];
      const auto finish = calendar_instant(trace, *event.event, options.timestamp_key, false);
      if (std::isfinite(finish)) {
        metrics.start = std::min(metrics.start, finish);
        metrics.finish = std::max(metrics.finish, finish);
      }
      const auto difference = scratch_number(*event.event, options.diff_start_end);
      if (!std::isnan(difference)) metrics.service += difference;
      ++metrics.event_count;
    }
    std::vector<double> service, sojourn;
    for (const auto& metrics : cases) {
      if (!std::isfinite(metrics.start) || !std::isfinite(metrics.finish))
        throw std::invalid_argument("log_to_features: temporal case has no completion date");
      service.push_back(metrics.service);
      sojourn.push_back(metrics.finish - metrics.start);
    }
    write_case_column(options.service_time, service);
    // Sojourn's date access occurs after the service destination is written.
    for (const auto& event : events)
      (void)calendar_instant(*event_traces.at(event.event), *event.event, options.timestamp_key, false);
    write_case_column(options.sojourn_time, sojourn);
    std::vector<double> waiting;
    waiting.reserve(events.size());
    for (const auto& event : events) {
      (void)locate_case(*event.event);
      waiting.push_back(scratch_number(*event.event, options.sojourn_time) -
                        scratch_number(*event.event, options.service_time));
    }
    write_column(options.waiting_time, waiting);

    const bool promote_activities = promote_numeric_column(activity_value);
    const bool promote_resources = promote_numeric_column(resource_value);
    origin = std::numeric_limits<double>::infinity();
    for (auto& event : events) {
      const auto& trace = *event_traces.at(event.event);
      event.trace = locate_case(*event.event);
      auto& categories = temporal_categories[event.event];
      if (const auto activity = activity_value(trace, *event.event)) {
        have_activity_column = true;
        if (!missing_category(*activity)) categories.activity = std::to_string(activity_categories.insert(promoted_value(*activity, promote_activities)));
      }
      if (const auto resource = resource_value(trace, *event.event)) {
        have_resource_column = true;
        if (!missing_category(*resource)) categories.resource = std::to_string(resource_categories.insert(promoted_value(*resource, promote_resources)));
      }
      event.start = calendar_instant(trace, *event.event,
          options.start_timestamp_key.empty() ? options.timestamp_key : options.start_timestamp_key, false);
      if (std::isfinite(event.start)) origin = std::min(origin, event.start);
    }
    if (!have_activity_column || !have_resource_column)
      throw std::invalid_argument("log_to_features: temporal activity and resource columns are required");
  }
  std::map<ExactTemporalInstant, std::vector<const TimedEvent*>> buckets;
  std::map<ExactTemporalInstant, double> bucket_timestamps;
  const auto bucket_time_key = options.start_timestamp_key.empty() ? options.timestamp_key : options.start_timestamp_key;
  const auto bucket_zone = calendar_timezones.find(bucket_time_key);
  const auto bucket_offset = bucket_zone == calendar_timezones.end() ? 0 : bucket_zone->second.value_or(0);
  const auto tick_width = temporal_frequency ? temporal_frequency->tick_ns : 0;
  if (tick_width && !nanosecond_columns[bucket_time_key] && tick_width % 1000)
    throw std::invalid_argument("log_to_features: tick width requires nanosecond timestamp sources");
  std::int64_t tick_origin = 0;
  std::int64_t exact_origin = 0;
  if (temporal_frequency) {
    std::optional<ExactTemporalInstant> earliest;
    for (const auto& event : events) if (std::isfinite(event.start)) {
      auto exact = exact_calendar_instants.at({event.event, bucket_time_key});
      exact.first += bucket_offset;
      if (!earliest || exact < *earliest) earliest = exact;
    }
    if (earliest) {
      exact_origin = earliest->first;
      tick_origin = floor_division(exact_origin, 86400) * 86400;
    }
  }
  for (const auto& event : events) {
    if (temporal_frequency) {
      if (!std::isfinite(event.start)) continue;
      auto exact = exact_calendar_instants.at({event.event, bucket_time_key});
      exact.first += bucket_offset;
      const ExactTemporalInstant label = tick_width
          ? fixed_temporal_label(exact, tick_origin, tick_width)
          : ExactTemporalInstant{temporal_bucket_label(static_cast<double>(exact.first), *temporal_frequency, static_cast<double>(exact_origin)), 0};
      buckets[label].push_back(&event);
      bucket_timestamps[label] = static_cast<double>(label.first - bucket_offset) + static_cast<double>(label.second) / 1e9;
    } else {
      const ExactTemporalInstant bucket{static_cast<long long>(
          std::floor((event.start - origin) / options.window_seconds)), 0};
      buckets[bucket].push_back(&event);
      bucket_timestamps[bucket] =
          origin + static_cast<double>(bucket.first) * options.window_seconds;
    }
  }
  if (temporal_frequency) {
    if (buckets.empty()) throw std::invalid_argument("log_to_features: temporal start column has no dates");
    const auto last_label = buckets.rbegin()->first;
    for (auto label = buckets.begin()->first; label < last_label;) {
      const auto next = tick_width ? add_temporal_nanos(label, tick_width) : ExactTemporalInstant{next_temporal_bucket_label(label.first, *temporal_frequency), 0};
      if (next <= label || next > last_label)
        throw std::invalid_argument("log_to_features: inconsistent temporal grid");
      buckets.try_emplace(next);
      bucket_timestamps[next] = static_cast<double>(next.first - bucket_offset) + static_cast<double>(next.second) / 1e9;
      label = next;
    }
  }
  std::unordered_map<std::string,std::size_t> index; for (std::size_t i=0;i<out.feature_names.size();++i) index[out.feature_names[i]]=i;
  for (const auto& [bucket, bucket_events] : buckets) {
    TemporalFeatureRow row;
    row.timestamp = bucket_timestamps.at(bucket);
    if (temporal_frequency) row.timestamp_ns = temporal_nanosecond_text({bucket.first - bucket_offset, bucket.second});
    row.values.assign(out.feature_names.size(), temporal_frequency && bucket_events.empty() ? 0.0 : options.default_not_present);
    auto add = [&](const std::string& name, double value) {
      if (const auto it = index.find(name); it != index.end()) row.values[it->second] = value;
    };
    std::set<std::size_t> trace_ids;
    std::set<std::string> resources;
    std::set<std::string> activities;
    std::map<std::size_t, std::vector<const TimedEvent*>> by_trace;
    std::map<std::string, std::set<std::size_t>> cases_by_resource;
    std::map<std::size_t, std::set<std::string>> resources_by_trace;
    for (const auto* event : bucket_events) {
      if (event->trace != no_case) {
        trace_ids.insert(event->trace);
        by_trace[event->trace].push_back(event);
      }
      std::optional<std::string> resource;
      if (temporal_frequency) {
        const auto& categories = temporal_categories.at(event->event);
        if (categories.activity) activities.insert(*categories.activity);
        resource = categories.resource;
      } else {
        activities.insert(value_for(*event->event, options.activity_key));
        const auto value = value_for(*event->event, options.resource_key);
        if (!value.empty()) resource = value;
      }
      if (resource) {
        resources.insert(*resource);
        auto& resource_cases = cases_by_resource[*resource];
        if (event->trace != no_case) {
          resource_cases.insert(event->trace);
          resources_by_trace[event->trace].insert(*resource);
        }
      }
    }
    double average_arrival = 0.0, average_finish = 0.0;
    if (temporal_frequency)
      validate_scalar_group(resources, [&](const std::string& id) {
        return native_scalar_order_kind(resource_categories.values.at(static_cast<std::size_t>(std::stoull(id))));
      });
    if (temporal_frequency)
      validate_tuple_group(resources, [&](const std::string& id) -> const EventAttribute& {
        return resource_categories.values.at(static_cast<std::size_t>(std::stoull(id)));
      });
    double average_waiting = 0.0, average_sojourn = 0.0, average_service = 0.0;
    std::size_t arrival_count = 0, finish_count = 0, waiting_count = 0, sojourn_count = 0, service_count = 0;
    for (const auto trace_id : trace_ids) {
      if (temporal_frequency) {
        const auto& case_events = by_trace.at(trace_id);
        const auto add_first_valid = [&](const std::string& key, double& total, std::size_t& count) {
          const TimedEvent* representative = nullptr;
          double first_value = 0.0;
          for (const auto* event : case_events) {
            const auto value = scratch_number(*event->event, key);
            if (!std::isnan(value) && (!representative || event->start < representative->start)) {
              representative = event;
              first_value = value;
            }
          }
          if (representative) {
            total += first_value;
            ++count;
          }
        };
        add_first_valid(options.arrival_rate, average_arrival, arrival_count);
        add_first_valid(options.finish_rate, average_finish, finish_count);
        add_first_valid(options.waiting_time, average_waiting, waiting_count);
        add_first_valid(options.sojourn_time, average_sojourn, sojourn_count);
        add_first_valid(options.service_time, average_service, service_count);
        continue;
      }
      average_arrival += arrival_rate[trace_id];
      average_finish += finish_rate[trace_id];
      const auto& metrics = cases[trace_id];
      const double sojourn = metrics.finish - metrics.start;
      average_waiting += sojourn - metrics.service;
      average_sojourn += sojourn;
      average_service += metrics.service;
    }
    const double case_count = static_cast<double>(trace_ids.size());
    if (temporal_frequency) {
      if (arrival_count) average_arrival /= static_cast<double>(arrival_count);
      if (finish_count) average_finish /= static_cast<double>(finish_count);
      if (waiting_count) average_waiting /= static_cast<double>(waiting_count);
      if (sojourn_count) average_sojourn /= static_cast<double>(sojourn_count);
      if (service_count) average_service /= static_cast<double>(service_count);
    } else if (case_count > 0.0) {
      average_arrival /= case_count;
      average_finish /= case_count;
      average_waiting /= case_count;
      average_sojourn /= case_count;
      average_service /= case_count;
    }
    double reworked = 0.0;
    double events_per_case = 0.0;
    double resources_per_case = 0.0;
    for (const auto& [trace_id, trace_events] : by_trace) {
      std::set<std::string> trace_activities;
      for (const auto* event : trace_events) {
        if (temporal_frequency) {
          const auto& activity = temporal_categories.at(event->event).activity;
          if (activity) trace_activities.insert(*activity);
        } else trace_activities.insert(value_for(*event->event, options.activity_key));
      }
      reworked += static_cast<double>(trace_events.size() - trace_activities.size());
      events_per_case += static_cast<double>(trace_events.size());
      resources_per_case += static_cast<double>(resources_by_trace[trace_id].size());
    }
    if (case_count > 0.0) {
      events_per_case /= case_count;
      resources_per_case /= case_count;
    }
    double cases_per_resource = 0.0;
    for (const auto& [resource, resource_cases] : cases_by_resource)
      cases_per_resource += static_cast<double>(resource_cases.size());
    if (!cases_by_resource.empty()) cases_per_resource /= static_cast<double>(cases_by_resource.size());
    add("unique_resources", static_cast<double>(resources.size()));
    add("unique_activities", static_cast<double>(activities.size()));
    add("num_events", static_cast<double>(bucket_events.size()));
    add("average_arrival_rate", average_arrival);
    add("average_finish_rate", average_finish);
    add("average_waiting_time", average_waiting);
    add("average_sojourn_time", average_sojourn);
    add("average_service_time", average_service);
    add("total_number_of_reworked_activities", reworked);
    if (temporal_frequency && !bucket_events.empty() && trace_ids.empty())
      row.values[index.at("total_number_of_reworked_activities")] = std::string{};
    add("avg_cases_per_resource", cases_per_resource);
    add("avg_events_per_case", events_per_case);
    add("number_of_cases", case_count);
    add("avg_resources_per_case", resources_per_case);
    // Keep the former compact schema as an explicit compatibility projection.
    add("arrivals", static_cast<double>(bucket_events.size()));
    add("finishes", static_cast<double>(bucket_events.size()));
    add("service_seconds", average_service * case_count);
    add("waiting_seconds", average_waiting * case_count);
    add("sojourn_seconds", average_sojourn * case_count);
    add("cases", case_count);
    add("resource_workload", static_cast<double>(bucket_events.size()));
    add("rework", reworked);
    out.temporal_rows.push_back(std::move(row));
  }
  return out;
}

}  // namespace minera
