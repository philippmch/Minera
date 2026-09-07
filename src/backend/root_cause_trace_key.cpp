#include "root_cause_trace_key.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {
using Attributes = std::map<std::string, EventAttribute>;

bool present(const EventAttribute& value) {
  return value.first_class_role == FirstClassAttributeRole::kNone ||
         value.first_class_state != FirstClassAttributeState::kMissing;
}

EventAttribute normalized(EventAttribute value) {
  if (value.first_class_state == FirstClassAttributeState::kNull) value.is_null = true;
  return value;
}

void project(Attributes& attributes, const std::string& text,
             FirstClassAttributeState state, const std::string& source,
             const std::optional<EventAttribute>& stored, const char* canonical) {
  const auto key = !source.empty() ? source
      : stored && !stored->first_class_source_key.empty() ? stored->first_class_source_key
      : stored && !stored->key.empty() ? stored->key : std::string(canonical);
  if (state == FirstClassAttributeState::kMissing && (stored || text.empty())) return;
  EventAttribute value = stored ? normalized(*stored) : EventAttribute{key, text};
  if (state == FirstClassAttributeState::kNull) value.is_null = true;
  else if (stored && !present(*stored)) return;
  attributes[key] = std::move(value);
}

void overlay(Attributes& attributes, const std::vector<EventAttribute>& values) {
  for (const auto& value : values) {
    if (!present(value)) attributes.erase(value.key);
    else attributes[value.key] = normalized(value);
  }
}

Attributes event_attributes(const Event& event) {
  Attributes result;
  project(result, event.activity, event.activity_state, event.activity_source_key, event.activity_attribute, "concept:name");
  project(result, event.timestamp, event.timestamp_state, event.timestamp_source_key, event.timestamp_attribute, "time:timestamp");
  project(result, event.resource, event.resource_state, event.resource_source_key, event.resource_attribute, "org:resource");
  project(result, event.lifecycle, event.lifecycle_state, event.lifecycle_source_key, event.lifecycle_attribute, "lifecycle:transition");
  for (const auto& value : event.attributes) {
    if (value.first_class_role == FirstClassAttributeRole::kCaseId) continue;
    if (!present(value)) result.erase(value.key);
    else result[value.key] = normalized(value);
  }
  return result;
}

Attributes trace_attributes(const Trace& trace) {
  Attributes result;
  project(result, trace.case_id, trace.case_id_state, trace.case_id_source_key, trace.case_id_attribute, "concept:name");
  overlay(result, trace.attributes);
  return result;
}

enum class Scalar { kNull, kString, kInteger, kReal, kDate, kBytes };
Scalar scalar_kind(const EventAttribute& value) {
  if (value.is_null) return Scalar::kNull;
  if (value.kind == AttributeKind::kBytes) return Scalar::kBytes;
  if (std::holds_alternative<NativeDate>(value.typed_value)) return Scalar::kDate;
  if (std::holds_alternative<double>(value.typed_value)) return Scalar::kReal;
  if (std::holds_alternative<std::int64_t>(value.typed_value) || std::holds_alternative<bool>(value.typed_value)) return Scalar::kInteger;
  if (value.kind == AttributeKind::kInt || value.kind == AttributeKind::kBoolean) return Scalar::kInteger;
  if (value.kind == AttributeKind::kFloat) return Scalar::kReal;
  if (value.kind == AttributeKind::kDate) return Scalar::kDate;
  return Scalar::kString;
}

std::string integer_text(const EventAttribute& value) {
  const auto text = root_cause_attribute_category(value);
  if (text == "True") return "1";
  if (text == "False") return "0";
  return text;
}

double real_value(const EventAttribute& value) {
  if (const auto* real = std::get_if<double>(&value.typed_value)) return *real;
  const auto text = root_cause_attribute_category(value);
  return std::strtod(text.c_str(), nullptr);
}

struct DateValue {
  bool aware = false;
  std::int64_t microseconds = 0;
  bool operator==(const DateValue&) const = default;
};

DateValue date_value(const EventAttribute& value) {
  // The encoder validates and canonicalizes the lexical components. Work
  // directly in integer microseconds so distant dates retain their precision.
  const auto text = root_cause_attribute_category(value);
  const auto field = [&](std::size_t at, std::size_t count) { return std::stoi(text.substr(at, count)); };
  const int year = field(0, 4), month = field(5, 2), day = field(8, 2);
  const std::int64_t preceding_years = year - 1;
  constexpr int days_before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  const auto days = preceding_years * 365 + preceding_years / 4 - preceding_years / 100 + preceding_years / 400 + days_before_month[month - 1] + (month > 2 && leap ? 1 : 0) + day - 1;
  DateValue result;
  result.microseconds = ((days * 24 + field(11, 2)) * 60 + field(14, 2)) * 60000000 + field(17, 2) * 1000000LL;
  std::size_t position = 19;
  if (position < text.size() && text[position] == '.') {
    result.microseconds += field(position + 1, 6);
    position += 7;
  }
  if (position < text.size()) {
    result.aware = true;
    const int sign = text[position] == '-' ? -1 : 1;
    std::int64_t offset = (field(position + 1, 2) * 60LL + field(position + 4, 2)) * 60000000;
    position += 6;
    if (position < text.size()) {
      offset += field(position + 1, 2) * 1000000LL;
      position += 3;
      if (position < text.size()) offset += field(position + 1, 6);
    }
    result.microseconds -= sign * offset;
  }
  return result;
}

bool scalar_equal(const EventAttribute& left, const EventAttribute& right) {
  const auto a = scalar_kind(left), b = scalar_kind(right);
  if (a == Scalar::kNull || b == Scalar::kNull) return a == b;
  if (a == Scalar::kBytes || b == Scalar::kBytes)
    return a == b && decode_bytes_attribute(left) == decode_bytes_attribute(right);
  if (a == Scalar::kDate || b == Scalar::kDate)
    return a == b && date_value(left) == date_value(right);
  if (a == Scalar::kString || b == Scalar::kString)
    return a == b && left.value == right.value;
  if (a == Scalar::kInteger && b == Scalar::kInteger)
    return integer_text(left) == integer_text(right);
  if (a == Scalar::kReal && b == Scalar::kReal) {
    const double x = real_value(left), y = real_value(right);
    if (std::isnan(x) && std::isnan(y))
      return left.typed_value.scalar_identity() == right.typed_value.scalar_identity();
    return x == y;
  }
  const auto& integer = a == Scalar::kInteger ? left : right;
  const double real = real_value(a == Scalar::kReal ? left : right);
  if (!std::isfinite(real) || std::trunc(real) != real) return false;
  if (real == 0) return integer_text(integer) == "0";
  char buffer[512];
  const auto formatted = std::to_chars(buffer, buffer + sizeof(buffer), real, std::chars_format::fixed, 0);
  if (formatted.ec != std::errc{}) throw std::invalid_argument("root cause: numeric trace attribute cannot be compared");
  return integer_text(integer) == std::string(buffer, formatted.ptr);
}

bool attribute_equal(const EventAttribute& left, const EventAttribute& right);

bool dictionary_equal(const Attributes& left, const Attributes& right) {
  if (left.size() != right.size()) return false;
  auto a = left.begin(), b = right.begin();
  for (; a != left.end(); ++a, ++b)
    if (a->first != b->first || !attribute_equal(a->second, b->second)) return false;
  return true;
}

bool attribute_equal(const EventAttribute& left, const EventAttribute& right) {
  if (left.container == AttributeContainerKind::kTuple && left.is_null) {
    auto null = left;
    null.container = AttributeContainerKind::kScalar;
    null.children.clear();
    return attribute_equal(null, right);
  }
  if (right.container == AttributeContainerKind::kTuple && right.is_null) {
    auto null = right;
    null.container = AttributeContainerKind::kScalar;
    null.children.clear();
    return attribute_equal(left, null);
  }
  if (left.container == AttributeContainerKind::kTuple || right.container == AttributeContainerKind::kTuple) {
    if (left.container != right.container || left.children.size() != right.children.size()) return false;
    for (std::size_t i = 0; i < left.children.size(); ++i)
      if (!attribute_equal(left.children[i], right.children[i])) return false;
    return true;
  }
  const bool left_nested = left.container != AttributeContainerKind::kScalar || !left.children.empty();
  const bool right_nested = right.container != AttributeContainerKind::kScalar || !right.children.empty();
  if (left_nested != right_nested) return false;
  if (!left_nested) return scalar_equal(left, right);
  const bool left_list = left.container == AttributeContainerKind::kList;
  const bool right_list = right.container == AttributeContainerKind::kList;
  if (left_list != right_list) return false;
  auto left_scalar = left, right_scalar = right;
  left_scalar.children.clear();
  right_scalar.children.clear();
  left_scalar.is_null = left_scalar.is_null || left.container != AttributeContainerKind::kScalar;
  right_scalar.is_null = right_scalar.is_null || right.container != AttributeContainerKind::kScalar;
  if (!scalar_equal(left_scalar, right_scalar)) return false;
  if (left_list) {
    if (left.children.size() != right.children.size()) return false;
    for (std::size_t i = 0; i < left.children.size(); ++i)
      if (left.children[i].key != right.children[i].key || !attribute_equal(left.children[i], right.children[i])) return false;
    return true;
  }
  Attributes a, b;
  overlay(a, left.children);
  overlay(b, right.children);
  return dictionary_equal(a, b);
}

void append_part(std::string& result, const std::string& part) {
  result += std::to_string(part.size()) + ":" + part;
}

std::string scalar_fingerprint(const EventAttribute& value) {
  switch (scalar_kind(value)) {
    case Scalar::kNull: return "N";
    case Scalar::kBytes: return "B" + decode_bytes_attribute(value);
    case Scalar::kString: return "S" + value.value;
    case Scalar::kInteger: return "I" + integer_text(value);
    case Scalar::kReal: {
      const double real = real_value(value);
      if (std::isnan(real)) return "RN" + std::to_string(value.typed_value.scalar_identity());
      if (real == 0) return "I0";
      if (std::isfinite(real) && std::trunc(real) == real) {
        char buffer[512];
        const auto formatted = std::to_chars(buffer, buffer + sizeof(buffer), real, std::chars_format::fixed, 0);
        if (formatted.ec != std::errc{}) throw std::invalid_argument("root cause: numeric trace attribute cannot be fingerprinted");
        return "I" + std::string(buffer, formatted.ptr);
      }
      return "R" + root_cause_attribute_category(value);
    }
    case Scalar::kDate: {
      const auto date = date_value(value);
      return std::string(date.aware ? "DA" : "DN") + std::to_string(date.microseconds);
    }
  }
  throw std::invalid_argument("root cause: invalid trace attribute kind");
}

std::string attribute_fingerprint(const EventAttribute& value);
std::string dictionary_fingerprint(const Attributes& values) {
  std::string result = "D";
  for (const auto& [key, value] : values) {
    append_part(result, key);
    append_part(result, attribute_fingerprint(value));
  }
  return result;
}

std::string attribute_fingerprint(const EventAttribute& value) {
  if (value.container == AttributeContainerKind::kTuple) {
    if (value.is_null) return scalar_fingerprint(value);
    std::string result = "T";
    for (const auto& child : value.children) append_part(result, attribute_fingerprint(child));
    return result;
  }
  if (value.container == AttributeContainerKind::kScalar && value.children.empty())
    return scalar_fingerprint(value);
  auto scalar = value;
  scalar.children.clear();
  scalar.is_null = scalar.is_null || value.container != AttributeContainerKind::kScalar;
  std::string result = "C";
  append_part(result, scalar_fingerprint(scalar));
  if (value.container == AttributeContainerKind::kList) {
    result += 'L';
    for (const auto& child : value.children) {
      append_part(result, child.key);
      append_part(result, attribute_fingerprint(child));
    }
  } else {
    Attributes children;
    overlay(children, value.children);
    result += dictionary_fingerprint(children);
  }
  return result;
}

}  // namespace

bool root_cause_traces_equal(const Trace& left, const Trace& right) {
  if (left.events.size() != right.events.size() ||
      !dictionary_equal(trace_attributes(left), trace_attributes(right))) return false;
  for (std::size_t i = 0; i < left.events.size(); ++i) {
    const auto a = event_attributes(left.events[i]), b = event_attributes(right.events[i]);
    if (a.size() != b.size()) return false;
    auto x = a.begin(), y = b.begin();
    for (; x != a.end(); ++x, ++y)
      if (x->first != y->first || root_cause_attribute_category(x->second) != root_cause_attribute_category(y->second)) return false;
  }
  return true;
}

bool root_cause_key_is_hashable(const EventAttribute& value) {
  if (value.container == AttributeContainerKind::kTuple) {
    if (value.is_null) return true;
    for (const auto& child : value.children)
      if (!root_cause_key_is_hashable(child)) return false;
    return true;
  }
  return value.container == AttributeContainerKind::kScalar && value.children.empty();
}

bool root_cause_scalar_keys_equal(const EventAttribute& left, const EventAttribute& right) {
  if (!root_cause_key_is_hashable(left) || !root_cause_key_is_hashable(right))
    throw std::invalid_argument("root cause: unhashable activity key");
  return attribute_equal(left, right);
}

std::string root_cause_scalar_key_fingerprint(const EventAttribute& value) {
  (void)root_cause_scalar_keys_equal(value, value);
  return attribute_fingerprint(value);
}

std::string root_cause_trace_fingerprint(const Trace& trace) {
  std::string result;
  append_part(result, dictionary_fingerprint(trace_attributes(trace)));
  for (const auto& event : trace.events) {
    std::string values;
    for (const auto& [key, value] : event_attributes(event)) {
      append_part(values, key);
      append_part(values, root_cause_attribute_category(value));
    }
    append_part(result, values);
  }
  return result;
}

}  // namespace minera
