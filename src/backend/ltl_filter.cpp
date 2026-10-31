#include "minera/ltl_filter.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <optional>
#include <unordered_set>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

// A non-empty key names the event attribute as the reference dictionary sees
// it: generic attributes first, then first-class fields under their source
// (or canonical XES) key, so `concept:name`/`org:resource` select the fields
// the importer promoted out of the generic attribute list.
const std::string* keyed_value(const Event& event, std::string_view key) {
  for (const auto& entry : event.attributes)
    if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
  const auto first_class = [&](std::string_view canonical, const std::string& source,
                               FirstClassAttributeState state, bool present,
                               const std::string& value) -> std::optional<const std::string*> {
    if ((source.empty() ? canonical : std::string_view(source)) != key || !present)
      return std::nullopt;
    return state == FirstClassAttributeState::kNull ? nullptr : &value;
  };
  for (const auto& found : {
           first_class("concept:name", event.activity_source_key, event.activity_state, event.has_activity(), event.activity),
           first_class("org:resource", event.resource_source_key, event.resource_state, event.has_resource(), event.resource),
           first_class("time:timestamp", event.timestamp_source_key, event.timestamp_state, event.has_timestamp(), event.timestamp),
           first_class("lifecycle:transition", event.lifecycle_source_key, event.lifecycle_state, event.has_lifecycle(), event.lifecycle)})
    if (found) return *found;
  return nullptr;
}

const std::string* event_value(const Event& event, std::string_view attribute) {
  return attribute.empty() ? &event.activity : keyed_value(event, attribute);
}

const std::string* resource_value(const Event& event, std::string_view key) {
  return key.empty() ? &event.resource : keyed_value(event, key);
}

void validate_options(const LtlFilterOptions& options) {
  for (const auto& value : options.attribute_values) {
    if (value.empty()) {
      throw std::invalid_argument("LTL filter: values must not be empty");
    }
  }
  switch (options.variant) {
    case LtlFilterVariant::kEventuallyFollows:
      if (options.attribute_values.empty()) {
        throw std::invalid_argument(
            "LTL filter: eventually_follows requires at least one value");
      }
      if (!options.enable_timestamp &&
          !options.timestamp_diff_boundaries.empty()) {
        throw std::invalid_argument(
            "LTL filter: timestamp boundaries require enable_timestamp");
      }
      if (options.enable_timestamp &&
          options.timestamp_diff_boundaries.size() !=
              options.attribute_values.size() - 1) {
        throw std::invalid_argument(
            "LTL filter: one timestamp boundary is required per value pair");
      }
      for (const auto& boundary : options.timestamp_diff_boundaries) {
        const double lower = boundary.first;
        const double upper = boundary.second;
        if (!std::isfinite(lower) || !std::isfinite(upper)) {
          throw std::invalid_argument(
              "LTL filter: timestamp bounds must be finite");
        }
        if (lower > upper) {
          throw std::invalid_argument(
              "LTL filter: timestamp bounds must be ordered");
        }
      }
      break;
    case LtlFilterVariant::kANextBNextC:
      if (options.attribute_values.size() != 3) {
        throw std::invalid_argument(
            "LTL filter: a_next_b_next_c requires exactly three values");
      }
      if (options.enable_timestamp || !options.timestamp_diff_boundaries.empty()) {
        throw std::invalid_argument(
            "LTL filter: timestamp bounds only apply to eventually_follows");
      }
      break;
    case LtlFilterVariant::kFourEyes:
      if (options.attribute_values.size() != 2) {
        throw std::invalid_argument(
            "LTL filter: four_eyes_principle requires exactly two values");
      }
      if (options.enable_timestamp || !options.timestamp_diff_boundaries.empty()) {
        throw std::invalid_argument(
            "LTL filter: timestamp bounds only apply to eventually_follows");
      }
      break;
    case LtlFilterVariant::kAttributeValueDifferentPersons:
      if (options.attribute_values.size() != 1) {
        throw std::invalid_argument(
            "LTL filter: attr_value_different_persons requires one value");
      }
      if (options.enable_timestamp || !options.timestamp_diff_boundaries.empty()) {
        throw std::invalid_argument(
            "LTL filter: timestamp bounds only apply to eventually_follows");
      }
      break;
    default:
      throw std::invalid_argument("LTL filter: unknown variant");
  }
}

// Reference parity (ltl_checker.eventually_follows, also log_ltl_filter.cpp):
// an ordered occurrence of the pattern must exist, and when bounds are enabled
// the i-th bound is checked against the timestamps of trace positions i and
// i + 1, not against the matched events.
bool eventually_matches(const Trace& trace, const LtlFilterOptions& options) {
  const auto& values = options.attribute_values;
  std::size_t begin = 0;
  for (const auto& target : values) {
    bool found = false;
    for (std::size_t i = begin; i < trace.events.size(); ++i) {
      const auto* value = event_value(trace.events[i], options.attribute_key);
      if (value != nullptr && *value == target) {begin = i + 1; found = true; break;}
    }
    if (!found) return false;
  }
  if (!options.enable_timestamp) return true;
  for (std::size_t i = 0; i + 1 < values.size(); ++i) {
    const auto start = parse_timestamp(trace.events[i].timestamp);
    const auto end = parse_timestamp(trace.events[i + 1].timestamp);
    if (!start || !end) return false;
    const double elapsed = *end - *start;
    const auto bound = options.timestamp_diff_boundaries[i];
    if (!std::isfinite(elapsed) || elapsed < bound.first || elapsed > bound.second) return false;
  }
  return true;
}

bool next_matches(const Trace& trace, const LtlFilterOptions& options) {
  for (std::size_t i = 0; i + 2 < trace.events.size(); ++i) {
    const auto* a = event_value(trace.events[i], options.attribute_key);
    const auto* b = event_value(trace.events[i + 1], options.attribute_key);
    const auto* c = event_value(trace.events[i + 2], options.attribute_key);
    if (a && b && c && *a == options.attribute_values[0] &&
        *b == options.attribute_values[1] && *c == options.attribute_values[2]) {
      return true;
    }
  }
  return false;
}

std::optional<bool> four_eyes_relation(const Trace& trace,
                                       const LtlFilterOptions& options) {
  std::unordered_set<std::string> first_resources;
  std::unordered_set<std::string> second_resources;
  for (const auto& event : trace.events) {
    const auto* value = event_value(event, options.attribute_key);
    const auto* resource = resource_value(event, options.resource_key);
    if (!value || !resource || resource->empty()) continue;
    if (*value == options.attribute_values[0]) first_resources.insert(*resource);
    if (*value == options.attribute_values[1]) second_resources.insert(*resource);
  }
  if (first_resources.empty() || second_resources.empty()) return std::nullopt;
  for (const auto& resource : first_resources) {
    if (second_resources.count(resource) != 0) return false;
  }
  return true;
}

bool different_persons_matches(const Trace& trace,
                               const LtlFilterOptions& options) {
  std::unordered_set<std::string> resources;
  for (const auto& event : trace.events) {
    const auto* value = event_value(event, options.attribute_key);
    const auto* resource = resource_value(event, options.resource_key);
    if (value && *value == options.attribute_values.front() && resource &&
        !resource->empty()) {
      resources.insert(*resource);
      if (resources.size() > 1) return true;
    }
  }
  return false;
}

bool matches(const Trace& trace, const LtlFilterOptions& options) {
  switch (options.variant) {
    case LtlFilterVariant::kEventuallyFollows:
      return eventually_matches(trace, options);
    case LtlFilterVariant::kANextBNextC:
      return next_matches(trace, options);
    case LtlFilterVariant::kFourEyes:
      return four_eyes_relation(trace, options).value_or(false);
    case LtlFilterVariant::kAttributeValueDifferentPersons:
      return different_persons_matches(trace, options);
    default:
      return false;
  }
}

}  // namespace

EventLog filter_ltl_event_log(const EventLog& event_log,
                              const LtlFilterOptions& options) {
  validate_options(options);
  EventLog result;
  copy_event_log_context(event_log, result);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    if (options.variant == LtlFilterVariant::kFourEyes) {
      const auto relation = four_eyes_relation(trace, options);
      if (relation && *relation == options.positive) result.traces.push_back(trace);
    } else if (matches(trace, options) == options.positive) {
      result.traces.push_back(trace);
    }
  }
  return result;
}

EventLog filter_ltl(const EventLog& event_log, const LtlFilterOptions& options) {
  return filter_ltl_event_log(event_log, options);
}

EventLog filter_event_log_ltl(const EventLog& event_log,
                              const LtlFilterOptions& options) {
  return filter_ltl_event_log(event_log, options);
}

}  // namespace minera
