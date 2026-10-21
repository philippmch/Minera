#include "minera/log_attribute_repetition_filter.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "root_cause_trace_key.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::optional<EventAttribute> selected(const Event& event,
                                      const std::string& key) {
  // Native dictionaries retain their last present value. First-class state
  // markers can explicitly represent an absent or present-null cell.
  for (auto it = event.attributes.rbegin(); it != event.attributes.rend(); ++it) {
    if (it->key != key) continue;
    if (it->first_class_role != FirstClassAttributeRole::kNone &&
        it->first_class_state == FirstClassAttributeState::kMissing)
      return std::nullopt;
    auto value = *it;
    if (it->first_class_state == FirstClassAttributeState::kNull)
      value.is_null = true;
    return value;
  }
  const auto projected = [&](std::string_view canonical,
                             const std::string& source_key,
                             const std::optional<EventAttribute>& stored,
                             FirstClassAttributeState state,
                             const std::string& text)
      -> std::optional<EventAttribute> {
    const auto actual = !source_key.empty() ? std::string_view(source_key)
        : stored && !stored->first_class_source_key.empty()
            ? std::string_view(stored->first_class_source_key)
        : stored ? std::string_view(stored->key) : canonical;
    if (actual != key || (state == FirstClassAttributeState::kMissing &&
                         text.empty() && !stored)) return std::nullopt;
    if (stored && stored->first_class_role != FirstClassAttributeRole::kNone &&
        stored->first_class_state == FirstClassAttributeState::kMissing)
      return std::nullopt;
    auto value = stored ? *stored : EventAttribute{key, text};
    if (state == FirstClassAttributeState::kNull) value.is_null = true;
    return value;
  };
  for (const auto value : {
      projected("concept:name", event.activity_source_key,
                event.activity_attribute, event.activity_state, event.activity),
      projected("time:timestamp", event.timestamp_source_key,
                event.timestamp_attribute, event.timestamp_state, event.timestamp),
      projected("org:resource", event.resource_source_key,
                event.resource_attribute, event.resource_state, event.resource),
      projected("lifecycle:transition", event.lifecycle_source_key,
                event.lifecycle_attribute, event.lifecycle_state, event.lifecycle)})
    if (value) return value;
  return std::nullopt;
}

bool set_kind(const EventAttribute& value) {
  return value.container == AttributeContainerKind::kSet ||
         value.container == AttributeContainerKind::kFrozenSet;
}

bool equal(const EventAttribute& left, const EventAttribute& right,
           bool nested = false) {
  if (left.is_null || right.is_null) return left.is_null && right.is_null;
  if (is_native_na_attribute(left) || is_native_na_attribute(right)) {
    if (nested && is_native_na_attribute(left) && is_native_na_attribute(right)) return true;
    // Ordinary scalar comparison returns the NA truth object; evaluating it
    // for event counting raises, even when both operands are that singleton.
    if (left.container == AttributeContainerKind::kScalar && right.container == AttributeContainerKind::kScalar)
      throw std::invalid_argument("boolean value of NA is ambiguous");
  }
  if (left.scalar_wrapper || right.scalar_wrapper ||
      left.scalar_children_list || right.scalar_children_list)
    throw std::invalid_argument(
        "attribute repetition target comparison requires ordinary values");
  if (set_kind(left) || set_kind(right)) {
    if (!set_kind(left) || !set_kind(right)) return false;
    const auto a = root_cause_set_members(left);
    const auto b = root_cause_set_members(right);
    if (a.size() != b.size()) return false;
    for (const auto* member : a)
      if (std::none_of(b.begin(), b.end(), [&](const auto* other) {
            return equal(*member, *other, true);
          })) return false;
    return true;
  }
  if (left.container != right.container) return false;
  if (left.container == AttributeContainerKind::kList ||
      left.container == AttributeContainerKind::kTuple) {
    if (left.children.size() != right.children.size()) return false;
    for (std::size_t i = 0; i < left.children.size(); ++i)
      if (!equal(left.children[i], right.children[i], true)) return false;
    return true;
  }
  if (left.container == AttributeContainerKind::kContainer) {
    std::map<std::string, const EventAttribute*> a, b;
    for (const auto& child : left.children) a[child.key] = &child;
    for (const auto& child : right.children) b[child.key] = &child;
    if (a.size() != b.size()) return false;
    for (const auto& [key, value] : a) {
      const auto other = b.find(key);
      if (other == b.end() || !equal(*value, *other->second, true)) return false;
    }
    return true;
  }
  // A direct Python float comparison never equals NaN, even when both sides
  // retain the same scalar identity. Container equality has its own identity
  // shortcut, represented by the established immutable-value helper.
  if (!nested) {
    for (const auto* value : {&left, &right}) {
      if (const auto* number = std::get_if<double>(&value->typed_value);
          number && std::isnan(*number)) return false;
      if (value->kind == AttributeKind::kFloat &&
          std::holds_alternative<std::monostate>(value->typed_value) &&
          std::isnan(native_attribute_number(*value))) return false;
    }
  }
  const auto date = [](const EventAttribute& value) {
    return std::holds_alternative<NativeDate>(value.typed_value) ||
           value.kind == AttributeKind::kDate;
  };
  if (date(left) || date(right)) {
    if (!date(left) || !date(right)) return false;
    const auto instant = [](const EventAttribute& value) {
      const auto* native = std::get_if<NativeDate>(&value.typed_value);
      const auto parsed = parse_timestamp_exact(native ? native->value : value.value);
      if (!parsed) throw std::invalid_argument("attribute repetition comparison has an invalid datetime value");
      return *parsed;
    };
    const auto a = instant(left), b = instant(right);
    return a.timezone_aware == b.timezone_aware &&
           a.seconds == b.seconds && a.nanoseconds == b.nanoseconds;
  }
  return root_cause_scalar_keys_equal(left, right);
}

bool bound_accepts(std::uint64_t count,
                   const LogAttributeRepetitionBound& bound, bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::nullptr_t> ||
                  std::is_same_v<T, std::string>) {
      const auto type = std::is_same_v<T, std::nullptr_t> ? "NoneType" : "str";
      throw std::invalid_argument(lower
          ? std::string("'<=' not supported between instances of '") + type + "' and 'int'"
          : std::string("'<=' not supported between instances of 'int' and '") + type + "'");
    } else if constexpr (std::is_same_v<T, NativeNA>) {
      throw std::invalid_argument("boolean value of NA is ambiguous");
    } else if constexpr (std::is_same_v<T, TokenInteger>) {
      return lower ? value <= TokenInteger(count) : TokenInteger(count) <= value;
    } else if constexpr (std::is_same_v<T, bool>) {
      return lower ? static_cast<std::uint64_t>(value) <= count
                   : count <= static_cast<std::uint64_t>(value);
    } else {
      if (std::isnan(value)) return false;
      if (lower) {
        if (value <= 0) return true;
        return value < 0x1p64 &&
               count >= static_cast<std::uint64_t>(std::ceil(value));
      }
      if (value < 0) return false;
      return value >= 0x1p64 ||
             count <= static_cast<std::uint64_t>(std::floor(value));
    }
  }, bound);
}

}  // namespace

EventLog filter_log_attribute_value_repetition(
    const EventLog& source, const EventAttribute& target,
    const LogAttributeRepetitionOptions& options) {
  EventLog result;
  copy_event_log_context(source, result);
  result.traces.reserve(source.traces.size());
  const auto* key = std::get_if<std::string>(&options.attribute_key);
  for (const auto& trace : source.traces) {
    std::uint64_t count = 0;
    if (key)
      for (const auto& event : trace.events)
        if (const auto candidate = selected(event, *key);
            candidate && equal(*candidate, target)) ++count;
    if (bound_accepts(count, options.minimum, true) &&
        bound_accepts(count, options.maximum, false))
      result.traces.push_back(trace);
  }
  return result;
}

}  // namespace minera
