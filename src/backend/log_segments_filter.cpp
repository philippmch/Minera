#include "minera/log_segments_filter.hpp"

#include <optional>

namespace minera {
namespace {
bool matches(const EventAttribute& attribute, std::string_view target) {
  return attribute.kind == AttributeKind::kString && !attribute.is_null &&
      attribute.container == AttributeContainerKind::kScalar &&
      !attribute.scalar_wrapper && !attribute.scalar_children_list &&
      attribute.children.empty() &&
      std::holds_alternative<std::monostate>(attribute.typed_value) &&
      attribute.value == target;
}

bool matches(const Event& event, std::string_view key, std::string_view target) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return matches(attribute, target);
  const auto projected = [&](std::string_view default_key, const std::string& source_key,
      const std::optional<EventAttribute>& attribute, FirstClassAttributeState state,
      const std::string& value) -> std::optional<bool> {
    const auto actual = !source_key.empty() ? std::string_view(source_key)
        : attribute ? std::string_view(attribute->key) : default_key;
    if (actual != key || (state == FirstClassAttributeState::kMissing && value.empty() && !attribute))
      return std::nullopt;
    return state != FirstClassAttributeState::kNull &&
        (attribute ? matches(*attribute, target) : value == target);
  };
  if (const auto result = projected("concept:name", event.activity_source_key,
          event.activity_attribute, event.activity_state, event.activity)) return *result;
  if (const auto result = projected("time:timestamp", event.timestamp_source_key,
          event.timestamp_attribute, event.timestamp_state, event.timestamp)) return *result;
  if (const auto result = projected("org:resource", event.resource_source_key,
          event.resource_attribute, event.resource_state, event.resource)) return *result;
  if (const auto result = projected("lifecycle:transition", event.lifecycle_source_key,
          event.lifecycle_attribute, event.lifecycle_state, event.lifecycle)) return *result;
  return false;
}

EventLog filter(const EventLog& source, std::string_view activity,
                const LogSegmentFilterOptions& options, bool prefix) {
  EventLog result;
  copy_event_log_context(source, result);
  result.traces.reserve(source.traces.size());
  for (const auto& trace : source.traces) {
    std::optional<std::size_t> anchor;
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      if (!matches(trace.events[i], options.activity_key, activity)) continue;
      anchor = i;
      if (options.occurrence == LogSegmentOccurrence::first) break;
    }
    if (!anchor) continue;
    Trace cut;
    copy_trace_context(trace, cut);
    if (prefix) {
      const auto end = *anchor + (options.strict ? 0U : 1U);
      cut.events.assign(trace.events.begin(), trace.events.begin() + end);
    } else {
      const auto begin = *anchor + (options.strict ? 1U : 0U);
      cut.events.assign(trace.events.begin() + begin, trace.events.end());
    }
    result.traces.push_back(std::move(cut));
  }
  return result;
}
}  // namespace

EventLog filter_log_prefix(const EventLog& source, std::string_view activity,
                          const LogSegmentFilterOptions& options) {
  return filter(source, activity, options, true);
}
EventLog filter_log_suffix(const EventLog& source, std::string_view activity,
                          const LogSegmentFilterOptions& options) {
  return filter(source, activity, options, false);
}
}  // namespace minera
