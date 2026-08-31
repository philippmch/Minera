#include "minera/case_grouping.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

bool matches_selected_key(std::string_view key, std::string_view source_key,
                          const std::optional<EventAttribute>& selected) {
  return (!source_key.empty() && key == source_key) ||
         (selected && key == selected->key);
}

const std::string* event_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return &event.activity;
  if (matches_selected_key(key, event.activity_source_key,
                           event.activity_attribute)) {
    return event.has_activity() ? &event.activity : nullptr;
  }
  if (key == "resource" || key == "org:resource")
    return event.resource.empty() ? nullptr : &event.resource;
  if (matches_selected_key(key, event.resource_source_key,
                           event.resource_attribute)) {
    return event.has_resource() ? &event.resource : nullptr;
  }
  if (key == "lifecycle" || key == "lifecycle:transition")
    return event.lifecycle.empty() ? nullptr : &event.lifecycle;
  if (matches_selected_key(key, event.lifecycle_source_key,
                           event.lifecycle_attribute)) {
    return event.has_lifecycle() ? &event.lifecycle : nullptr;
  }
  if (key == "timestamp" || key == "time:timestamp")
    return event.timestamp.empty() ? nullptr : &event.timestamp;
  if (matches_selected_key(key, event.timestamp_source_key,
                           event.timestamp_attribute)) {
    return event.has_timestamp() ? &event.timestamp : nullptr;
  }
  return event.attribute(key);
}

void set_event_value(Event& event, const std::string& key,
                     const std::string& value) {
  const bool activity_projection =
      key.empty() || key == "activity" || key == "concept:name" ||
      matches_selected_key(key, event.activity_source_key,
                           event.activity_attribute);
  if (activity_projection) {
    const Event source = event;
    set_event_activity_context(source, value, event);
    return;
  }
  if (key == "resource" || key == "org:resource") {
    event.resource = value;
    event.resource_state = FirstClassAttributeState::kPresent;
    if (event.resource_source_key.empty())
      event.resource_source_key = key == "org:resource" ? key : "resource";
    return;
  }
  if (key == "lifecycle" || key == "lifecycle:transition") {
    event.lifecycle = value;
    event.lifecycle_state = FirstClassAttributeState::kPresent;
    if (event.lifecycle_source_key.empty())
      event.lifecycle_source_key = key;
    return;
  }
  for (auto& attribute : event.attributes) {
    if (attribute.key == key) {
      attribute.value = value;
      attribute.is_null = false;
      attribute.container = AttributeContainerKind::kScalar;
      attribute.children.clear();
      if (attribute.kind != AttributeKind::kString &&
          attribute.kind != AttributeKind::kId) {
        attribute.kind = AttributeKind::kString;
      }
      attribute.typed_value = std::monostate{};
      return;
    }
  }
  event.attributes.push_back({key, value, AttributeKind::kString});
}

void validate_concat_separator(const std::string& separator) {
  if (separator.empty())
    throw std::invalid_argument(
        "timestamp grouping: concat separator must not be empty");
}

}  // namespace

EventLog group_consecutive_activities(
    const EventLog& event_log,
    const ConsecutiveActivityGroupingOptions& options) {
  EventLog result;
  copy_event_log_context(event_log, result);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    Trace output;
    copy_trace_context(trace, output);
    if (trace.events.empty()) {
      result.traces.push_back(std::move(output));
      continue;
    }
    std::size_t begin = 0;
    while (begin < trace.events.size()) {
      std::size_t end = begin + 1;
      const auto* key = event_value(trace.events[begin], options.activity_key);
      while (end < trace.events.size()) {
        const auto* candidate =
            event_value(trace.events[end], options.activity_key);
        if (key == nullptr || candidate == nullptr || *candidate != *key)
          break;
        ++end;
      }
      const std::size_t selected =
          options.filter_type == CaseGroupingFilterType::kLast ? end - 1 : begin;
      output.events.push_back(trace.events[selected]);
      begin = end;
    }
    result.traces.push_back(std::move(output));
  }
  return result;
}

EventLog filter_consecutive_activity_groups(
    const EventLog& event_log,
    const ConsecutiveActivityGroupingOptions& options) {
  return group_consecutive_activities(event_log, options);
}

EventLog group_same_timestamp_events(
    const EventLog& event_log,
    const TimestampCaseGroupingOptions& options) {
  validate_concat_separator(options.concat_separator);
  EventLog result;
  copy_event_log_context(event_log, result);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    Trace output;
    copy_trace_context(trace, output);
    std::map<std::string, std::vector<const Event*>> groups;
    std::vector<std::string> order;
    for (const auto& event : trace.events) {
      const auto* timestamp = event_value(
          event, options.timestamp_key.empty() ? std::string("timestamp")
                                               : options.timestamp_key);
      const std::string key = timestamp == nullptr ? std::string{} : *timestamp;
      if (!groups.contains(key)) order.push_back(key);
      groups[key].push_back(&event);
    }
    for (const auto& key : order) {
      const auto& events = groups.at(key);
      if (events.empty()) continue;
      const std::size_t selected =
          options.filter_type == TimestampCaseGroupingType::kLast
              ? events.size() - 1
              : 0;
      Event grouped = *events[selected];
      if (options.filter_type == TimestampCaseGroupingType::kConcat) {
        std::vector<std::string> activities;
        activities.reserve(events.size());
        for (const auto* event : events) {
          const auto* activity = event_value(*event, options.activity_key);
          if (activity != nullptr) activities.push_back(*activity);
        }
        std::sort(activities.begin(), activities.end());
        std::string concatenated;
        for (const auto& activity : activities) {
          if (!concatenated.empty()) concatenated += options.concat_separator;
          concatenated += activity;
        }
        set_event_value(grouped, options.activity_key, concatenated);
      }
      output.events.push_back(std::move(grouped));
    }
    result.traces.push_back(std::move(output));
  }
  return result;
}

EventLog filter_timestamp_case_groups(
    const EventLog& event_log,
    const TimestampCaseGroupingOptions& options) {
  return group_same_timestamp_events(event_log, options);
}

}  // namespace minera
