#include "minera/attribute_statistics.hpp"

#include <set>
#include <stdexcept>

namespace minera {
namespace {

const std::string* event_value(const Event& event, std::string_view key) {
  if (key.empty()) return nullptr;
  if (key == "activity" || key == "concept:name") return &event.activity;
  if (key == "timestamp" || key == "time:timestamp") return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key.empty()) return nullptr;
  if (key == "case_id" || key == "concept:name" ||
      key == "case:concept:name")
    return &trace.case_id;
  return trace.attribute(key);
}

}  // namespace

std::set<std::string> get_all_event_attributes_from_log(const EventLog& log) {
  std::set<std::string> result;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (!event.activity.empty()) result.insert("concept:name");
      if (!event.timestamp.empty()) result.insert("time:timestamp");
      if (!event.resource.empty()) result.insert("org:resource");
      // Keep the transition field out of this event-attribute inventory.
      for (const auto& attribute : event.attributes)
        if (!attribute.is_null && attribute.key != "lifecycle:transition")
          result.insert(attribute.key);
    }
  }
  return result;
}

std::set<std::string> get_all_trace_attributes_from_log(const EventLog& log) {
  std::set<std::string> result;
  for (const auto& trace : log.traces) {
    for (const auto& attribute : trace.attributes) {
      if (attribute.is_null) continue;
      if (attribute.key != "concept:name" &&
          attribute.key != "case:concept:name")
        result.insert(attribute.key);
    }
  }
  return result;
}

const char* attribute_statistics_scope_name(AttributeStatisticsScope scope) {
  switch (scope) {
    case AttributeStatisticsScope::kEvents: return "EVENTS";
    case AttributeStatisticsScope::kTraces: return "TRACES";
  }
  throw std::invalid_argument("attribute statistics: unknown scope");
}

AttributeStatisticsResult discover_attribute_statistics(
    const EventLog& log, const AttributeStatisticsOptions& options) {
  if (options.attribute_key.empty())
    throw std::invalid_argument("attribute statistics: attribute key is required");
  AttributeStatisticsResult result;
  result.scope = options.scope;
  result.attribute_key = options.attribute_key;
  result.keep_once_per_case = options.keep_once_per_case;
  if (options.scope == AttributeStatisticsScope::kEvents) {
    for (const auto& trace : log.traces) {
      std::set<std::string> seen;
      for (const auto& event : trace.events) {
        const auto* value = event_value(event, options.attribute_key);
        if (!value) continue;
        if (options.keep_once_per_case && !seen.insert(*value).second) continue;
        ++result.values[*value];
      }
    }
    return result;
  }
  if (options.scope == AttributeStatisticsScope::kTraces) {
    for (const auto& trace : log.traces) {
      const auto* value = trace_value(trace, options.attribute_key);
      if (value) ++result.values[*value];
    }
    return result;
  }
  throw std::invalid_argument("attribute statistics: unknown scope");
}

}  // namespace minera
