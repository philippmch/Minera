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
    if (it->key != key || it->xes_public_shape == XesPublicShape::kAbsent) continue;
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

bool wrapped(const EventAttribute& value) {
  if (value.xes_public_shape == XesPublicShape::kWrapper ||
      value.xes_public_shape == XesPublicShape::kNone) return true;
  return value.container == AttributeContainerKind::kScalar &&
         (value.scalar_wrapper || value.scalar_children_list || !value.children.empty());
}

bool absent(const EventAttribute& value) {
  return value.xes_public_shape == XesPublicShape::kAbsent;
}

// The public value of an imported scalar with nested elements is the mapping
// {'value': scalar, 'children': mapping-or-pair-list}; nested wrappers recurse.
// An imported XES <list> is the same wrapper around None (pairs for a
// <values> list), or None without child elements; <container> elements are
// not stored.
EventAttribute public_view(const EventAttribute& value) {
  if (value.xes_public_shape == XesPublicShape::kNone) {
    EventAttribute none{value.key, ""};
    none.is_null = true;
    return none;
  }
  if (!wrapped(value)) {
    if (value.children.empty()) return value;
    auto copy = value;
    copy.children.clear();
    for (const auto& child : value.children)
      if (!absent(child)) copy.children.push_back(public_view(child));
    return copy;
  }
  const bool xes_list = value.xes_public_shape == XesPublicShape::kWrapper;
  EventAttribute scalar = value;
  if (xes_list) {
    scalar = EventAttribute{"value", ""};
    scalar.is_null = true;
  }
  scalar.key = "value";
  scalar.children.clear();
  scalar.scalar_wrapper = false;
  scalar.scalar_children_list = false;
  EventAttribute children{"children", ""};
  if (xes_list ? value.container == AttributeContainerKind::kList : value.scalar_children_list) {
    children.container = AttributeContainerKind::kList;
    for (const auto& child : value.children) {
      if (absent(child)) continue;
      EventAttribute pair{std::to_string(children.children.size()), ""};
      pair.container = AttributeContainerKind::kTuple;
      pair.children.push_back(EventAttribute{"0", child.key});
      auto item = public_view(child);
      item.key = "1";
      pair.children.push_back(std::move(item));
      children.children.push_back(std::move(pair));
    }
  } else {
    children.container = AttributeContainerKind::kContainer;
    // Later duplicate keys replace earlier values, as in a dictionary.
    for (const auto& child : value.children) {
      if (absent(child)) continue;
      const auto existing = std::find_if(children.children.begin(), children.children.end(),
                                         [&](const auto& item) { return item.key == child.key; });
      if (existing != children.children.end()) *existing = public_view(child);
      else children.children.push_back(public_view(child));
    }
  }
  EventAttribute result{value.key, ""};
  result.container = AttributeContainerKind::kContainer;
  result.children.push_back(std::move(scalar));
  result.children.push_back(std::move(children));
  return result;
}

bool equal(const EventAttribute& left, const EventAttribute& right,
           bool nested = false) {
  if (wrapped(left) || wrapped(right))
    return equal(wrapped(left) ? public_view(left) : left,
                 wrapped(right) ? public_view(right) : right, nested);
  if (left.is_null || right.is_null) return left.is_null && right.is_null;
  if (is_native_na_attribute(left) || is_native_na_attribute(right)) {
    if (nested && is_native_na_attribute(left) && is_native_na_attribute(right)) return true;
    // Ordinary scalar comparison returns the NA truth object; evaluating it
    // for event counting raises, even when both operands are that singleton.
    if (left.container == AttributeContainerKind::kScalar && right.container == AttributeContainerKind::kScalar)
      throw std::invalid_argument("boolean value of NA is ambiguous");
  }
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

std::string ordinary_type(const EventAttribute& value) {
  if (value.is_null) return "NoneType";
  switch (value.container) {
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "dict";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
    case AttributeContainerKind::kScalar: break;
  }
  if (value.kind == AttributeKind::kDate ||
      std::holds_alternative<NativeDate>(value.typed_value)) return "datetime.datetime";
  if (value.kind == AttributeKind::kBytes) return "bytes";
  return "str";
}

[[noreturn]] void bound_error(const std::string& type, bool lower) {
  throw std::invalid_argument(lower
      ? "'<=' not supported between instances of '" + type + "' and 'int'"
      : "'<=' not supported between instances of 'int' and '" + type + "'");
}

bool bound_accepts(std::uint64_t count,
                   const LogAttributeRepetitionBound& bound, bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::nullptr_t> ||
                  std::is_same_v<T, std::string>) {
      const auto type = std::is_same_v<T, std::nullptr_t> ? "NoneType" : "str";
      bound_error(type, lower);
    } else if constexpr (std::is_same_v<T, EventAttribute>) {
      if (value.scalar_wrapper || value.scalar_children_list)
        throw std::invalid_argument("attribute repetition bounds require ordinary values");
      if (value.is_null) bound_error("NoneType", lower);
      if (value.container != AttributeContainerKind::kScalar)
        bound_error(ordinary_type(value), lower);
      if (is_native_na_attribute(value))
        throw std::invalid_argument("boolean value of NA is ambiguous");
      if (const auto* number = std::get_if<std::int64_t>(&value.typed_value))
        return bound_accepts(count, TokenInteger(*number), lower);
      if (const auto* number = std::get_if<std::uint64_t>(&value.typed_value))
        return bound_accepts(count, TokenInteger(*number), lower);
      if (const auto* number = std::get_if<double>(&value.typed_value))
        return bound_accepts(count, *number, lower);
      if (const auto* boolean = std::get_if<bool>(&value.typed_value))
        return bound_accepts(count, *boolean, lower);
      if (value.kind == AttributeKind::kInt)
        return bound_accepts(count, TokenInteger(value.value), lower);
      if (value.kind == AttributeKind::kFloat || value.kind == AttributeKind::kBoolean)
        return bound_accepts(count, native_attribute_number(value), lower);
      bound_error(ordinary_type(value), lower);
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

void validate_lookup_key(const EventAttribute& value) {
  if (root_cause_key_is_hashable(value)) return;
  if (value.container == AttributeContainerKind::kTuple ||
      value.container == AttributeContainerKind::kFrozenSet) {
    for (const auto& child : value.children) validate_lookup_key(child);
  }
  if (value.scalar_wrapper || value.scalar_children_list)
    throw std::invalid_argument("attribute repetition selectors require ordinary values");
  throw std::invalid_argument("unhashable type: '" + ordinary_type(value) + "'");
}

}  // namespace

EventLog filter_log_attribute_value_repetition(
    const EventLog& source, const EventAttribute& target,
    const LogAttributeRepetitionOptions& options) {
  EventLog result;
  copy_event_log_context(source, result);
  result.traces.reserve(source.traces.size());
  const auto* key = std::get_if<std::string>(&options.attribute_key);
  const auto* typed_key = std::get_if<EventAttribute>(&options.attribute_key);
  if (typed_key && !typed_key->is_null &&
      typed_key->container == AttributeContainerKind::kScalar &&
      (typed_key->kind == AttributeKind::kString || typed_key->kind == AttributeKind::kId) &&
      std::holds_alternative<std::monostate>(typed_key->typed_value))
    key = &typed_key->value;
  for (const auto& trace : source.traces) {
    std::uint64_t count = 0;
    for (const auto& event : trace.events) {
      // Public dictionary lookup hashes even a missing key in an empty event,
      // but neither an empty trace nor an empty log performs a lookup.
      if (typed_key) validate_lookup_key(*typed_key);
      if (key)
        if (const auto candidate = selected(event, *key);
            candidate && equal(*candidate, target)) ++count;
    }
    if (bound_accepts(count, options.minimum, true) &&
        bound_accepts(count, options.maximum, false))
      result.traces.push_back(trace);
  }
  return result;
}

}  // namespace minera
