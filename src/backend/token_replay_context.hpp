#pragma once

#include "minera/conformance.hpp"
#include <map>
#include <set>
#include <stdexcept>

namespace minera::detail {
// Replay's context contains the latest values from examined known activities.
// Unknown events do not update it. Later known events update every observation
// of that case, including observations attached to earlier firings.
inline Event token_replay_context(const Trace& trace, const TokenReplayTraceDiagnostics& diagnostics) {
  if (diagnostics.replayed_events > trace.events.size())
    throw std::out_of_range("root cause: examined event count out of range");
  std::set<std::size_t> unknown(diagnostics.unknown_event_indices.begin(),
                                diagnostics.unknown_event_indices.end());
  Event result;
  std::map<std::string, EventAttribute> attributes;
  const auto present_projection = [](bool present, FirstClassAttributeState state,
                                      const std::optional<EventAttribute>& stored) {
    if (!present || (stored && state == FirstClassAttributeState::kMissing)) return false;
    return state == FirstClassAttributeState::kNull || !stored ||
           stored->first_class_role == FirstClassAttributeRole::kNone ||
           stored->first_class_state != FirstClassAttributeState::kMissing;
  };
  for (std::size_t i = 0; i < diagnostics.replayed_events; ++i) {
    if (unknown.contains(i)) continue;
    const auto& event = trace.events[i];
    for (const auto& attribute : event.attributes) {
      if (attribute.first_class_role != FirstClassAttributeRole::kNone &&
          attribute.first_class_state == FirstClassAttributeState::kMissing) continue;
      attributes[attribute.key] = attribute;
    }
    if (present_projection(event.has_activity(), event.activity_state, event.activity_attribute)) {
      result.activity = event.activity; result.activity_state = event.activity_state;
      result.activity_source_key = event.activity_source_key; result.activity_attribute = event.activity_attribute;
    }
    if (present_projection(event.has_timestamp(), event.timestamp_state, event.timestamp_attribute))
      copy_event_timestamp_context(event, result);
    if (present_projection(event.has_resource(), event.resource_state, event.resource_attribute)) {
      result.resource = event.resource; result.resource_state = event.resource_state;
      result.resource_source_key = event.resource_source_key; result.resource_attribute = event.resource_attribute;
    }
    if (present_projection(event.has_lifecycle(), event.lifecycle_state, event.lifecycle_attribute)) {
      result.lifecycle = event.lifecycle; result.lifecycle_state = event.lifecycle_state;
      result.lifecycle_source_key = event.lifecycle_source_key; result.lifecycle_attribute = event.lifecycle_attribute;
    }
  }
  for (auto& [key, attribute] : attributes) result.attributes.push_back(std::move(attribute));
  return result;
}
}  // namespace minera::detail
