#include "minera/attribute_value_repetition.hpp"

#include <stdexcept>

namespace minera {
namespace {

const std::string* event_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name") {
    return &event.activity;
  }
  if (key == "resource") {
    return event.resource.empty() ? nullptr : &event.resource;
  }
  if (key == "lifecycle" || key == "lifecycle:transition") {
    return event.lifecycle.empty() ? nullptr : &event.lifecycle;
  }
  if (key == "timestamp" || key == "time:timestamp") {
    return event.timestamp.empty() ? nullptr : &event.timestamp;
  }
  return event.attribute(key);
}

void validate_options(const AttributeValueRepetitionOptions& options) {
  if (options.min_repetitions > options.max_repetitions) {
    throw std::invalid_argument(
        "attribute repetition: minimum must not exceed maximum");
  }
}

}  // namespace

EventLog filter_attribute_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options) {
  validate_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    std::uint64_t repetitions = 0;
    for (const auto& event : trace.events) {
      const auto* candidate = event_value(event, options.attribute_key);
      if (candidate != nullptr && *candidate == options.value) {
        ++repetitions;
      }
    }
    if (repetitions >= options.min_repetitions &&
        repetitions <= options.max_repetitions) {
      result.traces.push_back(trace);
    }
  }
  return result;
}

EventLog filter_attr_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options) {
  return filter_attribute_value_repetition(event_log, options);
}

EventLog filter_event_log_attr_value_repetition(
    const EventLog& event_log,
    const AttributeValueRepetitionOptions& options) {
  return filter_attribute_value_repetition(event_log, options);
}

}  // namespace minera
