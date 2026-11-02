#pragma once

// Event and trace field lookup with DataFrame missing-value semantics.
//
// pm4py 2.7.22 hands CSV (read_csv + format_dataframe) and XES (read_xes)
// users a DataFrame, so the mining filters and statistics over imported logs
// follow DataFrame columns: a cell is missing when the column is absent for
// the row, explicitly null, or a float NaN (an XES <float value="nan"/>
// reads as NaN). A missing cell has no value: it never matches a value list
// and is not counted.

#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "minera/event_log.hpp"

namespace minera::dataframe_cell {

// Whether a stored cell is a float NaN.
inline bool float_nan(const EventAttribute& attribute) {
  if (attribute.kind != AttributeKind::kFloat) return false;
  if (const auto* number = std::get_if<double>(&attribute.typed_value))
    return std::isnan(*number);
  return attribute.value == "nan" || attribute.value == "NaN";
}

inline bool missing_cell(const EventAttribute& attribute) {
  return attribute.is_null || float_nan(attribute);
}

// Whether a first-class projection holds a missing cell.
inline bool missing_first_class(bool has_value, FirstClassAttributeState state,
                                const std::optional<EventAttribute>& metadata) {
  return !has_value || state == FirstClassAttributeState::kNull ||
         (metadata && float_nan(*metadata));
}

inline bool resource_missing(const Event& event) {
  return missing_first_class(event.has_resource(), event.resource_state,
                             event.resource_attribute);
}
inline bool lifecycle_missing(const Event& event) {
  return missing_first_class(event.has_lifecycle(), event.lifecycle_state,
                             event.lifecycle_attribute);
}
inline bool activity_missing(const Event& event) {
  return missing_first_class(event.has_activity(), event.activity_state,
                             event.activity_attribute);
}

// The value of the event column `key`, or nullptr when that cell is missing.
// A generic attribute with exactly that key wins; otherwise the first-class
// fields answer to their canonical XES key and to the source column they were
// read from.
inline const std::string* event_value(const Event& event, std::string_view key) {
  if (key.empty()) return nullptr;
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return missing_cell(attribute) ? nullptr : &attribute.value;
  const auto matches = [&](std::string_view canonical, const std::string& source) {
    return key == canonical || (!source.empty() && key == source);
  };
  if (matches("concept:name", event.activity_source_key))
    return activity_missing(event) ? nullptr : &event.activity;
  if (matches("org:resource", event.resource_source_key))
    return resource_missing(event) ? nullptr : &event.resource;
  if (matches("lifecycle:transition", event.lifecycle_source_key))
    return lifecycle_missing(event) ? nullptr : &event.lifecycle;
  if (matches("time:timestamp", event.timestamp_source_key))
    return missing_first_class(event.has_timestamp(), event.timestamp_state,
                               event.timestamp_attribute)
               ? nullptr
               : &event.timestamp;
  return nullptr;
}

// The value of the case column `key`, or nullptr when it is missing. Trace
// attributes appear in a DataFrame under a `case:` prefix, so `case:x` reads
// the trace attribute `x`; the unprefixed key is still accepted.
inline const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key.empty()) return nullptr;
  if (key == "case:concept:name" ||
      (!trace.case_id_source_key.empty() && key == trace.case_id_source_key))
    return trace.has_case_id() && trace.case_id_state != FirstClassAttributeState::kNull
               ? &trace.case_id
               : nullptr;
  const auto lookup = [&](std::string_view name) -> std::optional<const std::string*> {
    for (const auto& attribute : trace.attributes)
      if (attribute.key == name)
        return missing_cell(attribute) ? nullptr : &attribute.value;
    return std::nullopt;
  };
  if (const auto found = lookup(key)) return *found;
  constexpr std::string_view prefix = "case:";
  if (key.starts_with(prefix))
    if (const auto found = lookup(key.substr(prefix.size()))) return *found;
  return nullptr;
}

}  // namespace minera::dataframe_cell
