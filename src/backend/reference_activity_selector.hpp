#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

namespace detail {

// The native carrier's own name for a canonical field ("activity" for
// concept:name), which is also the native streaming default selector.
inline std::string_view native_field_name(std::string_view canonical) {
  if (canonical == "concept:name") return "activity";
  if (canonical == "org:resource") return "resource";
  if (canonical == "time:timestamp") return "timestamp";
  return {};
}

// Whether a first-class projection stands for the field `selector` names.
// The projection is the field it was read from. A flat adapter (CSV or a
// table, which carries no selected XES cell) also exposes it under the
// canonical key, as format_dataframe copies the chosen column there. A
// manually constructed event without any recorded source is the canonical
// field, which the native Event type also names by its own member name
// ("activity", "resource", "timestamp"); a projection read from a recorded
// field never stands for those names.
inline bool projection_names(const std::string& source_key,
                             const std::optional<EventAttribute>& selected_cell,
                             std::string_view canonical, std::string_view selector) {
  const std::string_view source =
      !source_key.empty() ? std::string_view(source_key)
                          : selected_cell ? std::string_view(selected_cell->key) : std::string_view();
  if (source.empty()) {
    const auto native = native_field_name(canonical);
    return selector == canonical || (!native.empty() && selector == native);
  }
  if (source == selector) return true;
  return !selected_cell && selector == canonical;
}

inline const std::string* present_projection(const std::string& value,
                                             FirstClassAttributeState state) {
  if (state == FirstClassAttributeState::kNull) return nullptr;
  if (state == FirstClassAttributeState::kMissing && value.empty()) return nullptr;
  return &value;
}

inline std::invalid_argument missing_selected_attribute(std::string_view context,
                                                        std::string_view key) {
  return std::invalid_argument(std::string(context) + ": event is missing activity attribute '" +
                               std::string(key) + "'");
}

}  // namespace detail

// The value of the field an explicit reference-style key names on one event:
// the literal attribute with that exact spelling when the event has one, and
// otherwise a first-class projection only when it was read from that same
// field. nullptr when the field is missing or a present null; a present empty
// value is a value.
inline const std::string* reference_selected_field(const Event& event, std::string_view key) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return attribute.is_null ? nullptr : &attribute.value;
  if (detail::projection_names(event.activity_source_key, event.activity_attribute, "concept:name", key))
    return detail::present_projection(event.activity, event.activity_state);
  if (detail::projection_names(event.resource_source_key, event.resource_attribute, "org:resource", key))
    return detail::present_projection(event.resource, event.resource_state);
  if (detail::projection_names(event.timestamp_source_key, event.timestamp_attribute, "time:timestamp", key))
    return detail::present_projection(event.timestamp, event.timestamp_state);
  if (detail::projection_names(event.lifecycle_source_key, event.lifecycle_attribute, "lifecycle:transition", key))
    return detail::present_projection(event.lifecycle, event.lifecycle_state);
  return nullptr;
}

// The activity a reference-style `activity_key` names on one event. An empty
// selector is the omitted default and reads the activity projection; an
// explicit key follows reference_selected_field. A missing or null value is
// an error (the reference raises KeyError or skips it).
inline const std::string& reference_selected_activity(const Event& event, std::string_view selector,
                                                      std::string_view context) {
  const auto* value = selector.empty()
                          ? detail::present_projection(event.activity, event.activity_state)
                          : reference_selected_field(event, selector);
  if (!value)
    throw detail::missing_selected_attribute(
        context, selector.empty() ? std::string_view("concept:name") : selector);
  return *value;
}

// The same rule for keys that may name the resource ("org:resource",
// "resource") or any other field.
inline const std::string& reference_selected_label(const Event& event, std::string_view selector,
                                                   std::string_view context) {
  return reference_selected_activity(event, selector, context);
}

}  // namespace minera
