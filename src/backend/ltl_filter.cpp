#include "minera/ltl_filter.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <optional>
#include <unordered_set>
#include <vector>

#include "minera/timestamp.hpp"
#include "dataframe_cell.hpp"

namespace minera {
namespace {

// A non-empty key names the event attribute as the reference dictionary sees
// it: generic attributes first, then first-class fields under their source
// (or canonical XES) key, so `concept:name`/`org:resource` select the fields
// the importer promoted out of the generic attribute list.
const std::string* keyed_value(const Event& event, std::string_view key) {
  for (const auto& entry : event.attributes)
    if (entry.key == key) return dataframe_cell::missing_cell(entry) ? nullptr : &entry.value;
  // A float NaN cell (XES <float value="nan"/>) is missing, as in the
  // DataFrame that pm4py's read_xes returns.
  const auto first_class = [&](std::string_view canonical, const std::string& source,
                               FirstClassAttributeState state, bool present,
                               const std::string& value,
                               const std::optional<EventAttribute>& metadata) -> std::optional<const std::string*> {
    if ((source.empty() ? canonical : std::string_view(source)) != key || !present)
      return std::nullopt;
    return dataframe_cell::missing_first_class(present, state, metadata) ? nullptr : &value;
  };
  for (const auto& found : {
           first_class("concept:name", event.activity_source_key, event.activity_state, event.has_activity(), event.activity, event.activity_attribute),
           first_class("org:resource", event.resource_source_key, event.resource_state, event.has_resource(), event.resource, event.resource_attribute),
           first_class("time:timestamp", event.timestamp_source_key, event.timestamp_state, event.has_timestamp(), event.timestamp, event.timestamp_attribute),
           first_class("lifecycle:transition", event.lifecycle_source_key, event.lifecycle_state, event.has_lifecycle(), event.lifecycle, event.lifecycle_attribute)})
    if (found) return *found;
  return nullptr;
}

const std::string* event_value(const Event& event, std::string_view attribute) {
  if (!attribute.empty()) return keyed_value(event, attribute);
  return dataframe_cell::activity_missing(event) ? nullptr : &event.activity;
}

const std::string* resource_value(const Event& event, std::string_view key) {
  if (!key.empty()) return keyed_value(event, key);
  return dataframe_cell::resource_missing(event) ? nullptr : &event.resource;
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

// The EventLog-object reference (pm4py's log ltl_checker.eventually_follows,
// also log_ltl_filter.cpp): an ordered occurrence of the pattern must exist,
// and when bounds are enabled the i-th bound is checked against the
// timestamps of trace positions i and i + 1, not against the matched events.
bool eventually_matches_event_log(const Trace& trace,
                                  const LtlFilterOptions& options) {
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

// The EventLog-object reference: any window A, B, C of adjacent events.
bool next_matches_event_log(const Trace& trace,
                            const LtlFilterOptions& options) {
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

// The EventLog-object reference: a case violates the principle when any
// resource of an A event also performs some B event.
std::optional<bool> four_eyes_relation_event_log(
    const Trace& trace, const LtlFilterOptions& options) {
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

// pm4py's DataFrame ltl_checker, the reference for imported CSV/XES logs.
// Only rows whose inspected cells (the attribute, and the resource or the
// timestamp where the rule reads one) are all present take part; a missing,
// null or NaN cell drops the row, while an empty string is an ordinary value.

// Eventually-follows: a backtracking search for an ordered occurrence of the
// pattern in which every bound (inclusive, in seconds) holds between the two
// matched events it relates.
bool eventually_matches_dataframe(const Trace& trace,
                                  const LtlFilterOptions& options) {
  const auto& values = options.attribute_values;
  const bool timed = options.enable_timestamp;
  const bool bounded = timed && !options.timestamp_diff_boundaries.empty();
  struct Row {
    const std::string* value;
    double instant;
  };
  std::vector<Row> rows;
  rows.reserve(trace.events.size());
  for (const auto& event : trace.events) {
    const auto* value = event_value(event, options.attribute_key);
    if (value == nullptr) continue;
    double instant = 0.0;
    if (timed) {
      const auto parsed = parse_timestamp(event.timestamp);
      if (!parsed) continue;
      instant = *parsed;
    }
    rows.push_back({value, instant});
  }
  const auto search = [&](auto&& self, std::size_t depth, std::size_t begin,
                          std::size_t previous) -> bool {
    if (depth == values.size()) return true;
    for (std::size_t i = begin; i < rows.size(); ++i) {
      if (*rows[i].value != values[depth]) continue;
      if (depth != 0 && bounded) {
        const double elapsed = rows[i].instant - rows[previous].instant;
        const auto bound = options.timestamp_diff_boundaries[depth - 1];
        if (!std::isfinite(elapsed) || elapsed < bound.first ||
            elapsed > bound.second) {
          continue;
        }
      }
      if (self(self, depth + 1, i + 1, i)) return true;
    }
    return false;
  };
  return search(search, 0, 0, 0);
}

// A-next-B-next-C: the trace's last B and last C must be adjacent, in that
// order, and the event right before that B must be an A. An earlier A, B, C
// window does not count once a later B or C occurs.
bool next_matches_dataframe(const Trace& trace,
                            const LtlFilterOptions& options) {
  std::optional<std::size_t> last_b;
  std::optional<std::size_t> last_c;
  for (std::size_t i = 0; i < trace.events.size(); ++i) {
    const auto* value = event_value(trace.events[i], options.attribute_key);
    if (value == nullptr) continue;
    if (*value == options.attribute_values[1]) last_b = i;
    if (*value == options.attribute_values[2]) last_c = i;
  }
  if (!last_b || !last_c || *last_b == 0 || *last_c != *last_b + 1) return false;
  const auto* a = event_value(trace.events[*last_b - 1], options.attribute_key);
  return a != nullptr && *a == options.attribute_values[0];
}

// Four eyes: a case is eligible when it has both an A and a B event with a
// resource; it satisfies the principle unless some A resource equals the
// resource of the case's last B event.
std::optional<bool> four_eyes_relation_dataframe(
    const Trace& trace, const LtlFilterOptions& options) {
  bool any_first = false;
  const std::string* last_second = nullptr;
  std::vector<const std::string*> first_resources;
  for (const auto& event : trace.events) {
    const auto* value = event_value(event, options.attribute_key);
    const auto* resource = resource_value(event, options.resource_key);
    if (!value || !resource) continue;
    if (*value == options.attribute_values[0]) {
      any_first = true;
      first_resources.push_back(resource);
    }
    if (*value == options.attribute_values[1]) last_second = resource;
  }
  if (!any_first || last_second == nullptr) return std::nullopt;
  for (const auto* resource : first_resources) {
    if (*resource == *last_second) return false;
  }
  return true;
}

// Different persons: some two A events with a resource differ in it.
bool different_persons_matches_dataframe(const Trace& trace,
                                         const LtlFilterOptions& options) {
  const std::string* first = nullptr;
  for (const auto& event : trace.events) {
    const auto* value = event_value(event, options.attribute_key);
    const auto* resource = resource_value(event, options.resource_key);
    if (!value || !resource || *value != options.attribute_values.front()) continue;
    if (first == nullptr) {
      first = resource;
    } else if (*resource != *first) {
      return true;
    }
  }
  return false;
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

std::optional<bool> four_eyes_relation(const Trace& trace,
                                       const LtlFilterOptions& options,
                                       LtlInputShape shape) {
  return shape == LtlInputShape::kEventLogObject
             ? four_eyes_relation_event_log(trace, options)
             : four_eyes_relation_dataframe(trace, options);
}

bool matches(const Trace& trace, const LtlFilterOptions& options,
             LtlInputShape shape) {
  const bool object = shape == LtlInputShape::kEventLogObject;
  switch (options.variant) {
    case LtlFilterVariant::kEventuallyFollows:
      return object ? eventually_matches_event_log(trace, options)
                    : eventually_matches_dataframe(trace, options);
    case LtlFilterVariant::kANextBNextC:
      return object ? next_matches_event_log(trace, options)
                    : next_matches_dataframe(trace, options);
    case LtlFilterVariant::kAttributeValueDifferentPersons:
      return object ? different_persons_matches(trace, options)
                    : different_persons_matches_dataframe(trace, options);
    default:
      return false;
  }
}

}  // namespace

EventLog filter_ltl_event_log(const EventLog& event_log,
                              const LtlFilterOptions& options) {
  return filter_ltl_event_log(event_log, options,
                              LtlInputShape::kImportedTable);
}

EventLog filter_ltl_event_log(const EventLog& event_log,
                              const LtlFilterOptions& options,
                              LtlInputShape shape) {
  validate_options(options);
  EventLog result;
  copy_event_log_context(event_log, result);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    if (options.variant == LtlFilterVariant::kFourEyes) {
      const auto relation = four_eyes_relation(trace, options, shape);
      if (relation && *relation == options.positive) result.traces.push_back(trace);
    } else if (matches(trace, options, shape) == options.positive) {
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
