#include "minera/batches.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

enum class SelectorRole { kCase, kActivity, kTimestamp, kResource, kGeneric };

struct IndexedEvent {
  BatchEvent event;
  std::string activity;
  std::string resource;
};

struct TableSelector {
  std::string name;
  std::optional<std::size_t> column;
  SelectorRole role = SelectorRole::kGeneric;
};

bool is_alias(std::string_view value,
              std::initializer_list<std::string_view> aliases) {
  return std::find(aliases.begin(), aliases.end(), value) != aliases.end();
}

bool is_alias(std::string_view value,
              const std::vector<std::string_view>& aliases) {
  return std::find(aliases.begin(), aliases.end(), value) != aliases.end();
}

std::optional<SelectorRole> selector_role(std::string_view value) {
  if (is_alias(value, {"case:concept:name", "case_id", "case"}))
    return SelectorRole::kCase;
  if (is_alias(value, {"concept:name", "activity"}))
    return SelectorRole::kActivity;
  if (is_alias(value, {"time:timestamp", "timestamp"}))
    return SelectorRole::kTimestamp;
  if (is_alias(value, {"org:resource", "resource"}))
    return SelectorRole::kResource;
  return std::nullopt;
}

FirstClassAttributeRole first_class_role(SelectorRole role) {
  switch (role) {
    case SelectorRole::kCase: return FirstClassAttributeRole::kCaseId;
    case SelectorRole::kActivity: return FirstClassAttributeRole::kActivity;
    case SelectorRole::kTimestamp: return FirstClassAttributeRole::kTimestamp;
    case SelectorRole::kResource: return FirstClassAttributeRole::kResource;
    case SelectorRole::kGeneric: return FirstClassAttributeRole::kNone;
  }
  return FirstClassAttributeRole::kNone;
}

std::vector<std::string_view> aliases_for(SelectorRole role) {
  switch (role) {
    case SelectorRole::kCase:
      return {"case:concept:name", "case_id", "case"};
    case SelectorRole::kActivity:
      return {"concept:name", "activity"};
    case SelectorRole::kTimestamp:
      return {"time:timestamp", "timestamp"};
    case SelectorRole::kResource:
      return {"org:resource", "resource"};
    case SelectorRole::kGeneric:
      return {};
  }
  return {};
}

std::optional<std::size_t> exact_column(const EventTable& table,
                                        std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

const EventAttribute* row_cell(const EventTable::Row& row,
                               const EventTable& table,
                               const TableSelector& selector) {
  if (selector.column) {
    if (*selector.column >= row.size() || !row[*selector.column]) return nullptr;
    const auto& cell = *row[*selector.column];
    return cell.is_null ? nullptr : &cell;
  }

  const auto role = first_class_role(selector.role);
  const bool allow_role_projection = selector.name.empty() ||
                                     selector_role(selector.name).has_value();
  for (std::size_t index = 0; index < row.size(); ++index) {
    if (!row[index] || row[index]->is_null) continue;
    const auto& cell = *row[index];
    const auto column_name = index < table.columns.size()
                                 ? std::string_view(table.columns[index])
                                 : std::string_view{};
    if ((allow_role_projection && role != FirstClassAttributeRole::kNone &&
         (cell.first_class_role == role ||
          (cell.first_class_role == FirstClassAttributeRole::kNone &&
           is_alias(column_name, aliases_for(selector.role))))) ||
        cell.key == selector.name ||
        cell.first_class_source_key == selector.name ||
        column_name == selector.name) {
      return &cell;
    }
  }
  return nullptr;
}

bool any_selected_cell(const EventTable& table, const TableSelector& selector) {
  for (const auto& row : table.rows)
    if (row_cell(row, table, selector) != nullptr) return true;
  return false;
}

// A log-to-table conversion keeps the original spelling of a first-class
// selector in transport metadata while using a canonical table column for
// the projected value.  Resolve that source spelling only when the metadata
// carries the role requested by the caller; a source key belonging to an
// activity, resource, or timestamp must not silently become a case column.
std::optional<std::size_t> source_projection_column(
    const EventTable& table, std::string_view name, SelectorRole requested_role) {
  const auto expected_role = first_class_role(requested_role);
  if (expected_role == FirstClassAttributeRole::kNone || name.empty())
    return std::nullopt;
  for (const auto& row : table.rows) {
    for (std::size_t index = 0; index < row.size(); ++index) {
      if (!row[index] || row[index]->is_null) continue;
      const auto& cell = *row[index];
      if (cell.first_class_role == expected_role &&
          cell.first_class_source_key == name)
        return index;
    }
  }
  return std::nullopt;
}

TableSelector resolve_selector(const EventTable& table, std::string name,
                               SelectorRole requested_role,
                               std::string_view description,
                               bool required) {
  const auto semantic_role = selector_role(name);
  TableSelector selector{std::move(name), std::nullopt,
                         semantic_role.value_or(requested_role)};
  if (const auto found = exact_column(table, selector.name)) {
    selector.column = found;
    return selector;
  }
  if (semantic_role) {
    for (const auto alias : aliases_for(*semantic_role)) {
      if (const auto found = exact_column(table, alias)) {
        selector.column = found;
        return selector;
      }
    }
  }
  // Flat log adapters retain non-standard first-class source names in cell
  // metadata even when the visible table column is canonical.  Resolve that
  // metadata only with a matching role so an unrelated source key cannot be
  // selected by accident.
  if (const auto found =
          source_projection_column(table, selector.name, requested_role)) {
    selector.column = found;
    return selector;
  }
  // A non-standard selector otherwise names a concrete table column.  Do not
  // infer it from an unrelated first-class role or stale cell metadata after
  // a column rename.
  if (semantic_role && any_selected_cell(table, selector)) return selector;
  if (required) {
    throw std::invalid_argument("batches: missing " + std::string(description) +
                                " column '" + selector.name + "'");
  }
  return selector;
}

std::string attribute_text(const EventAttribute& cell) {
  if (!cell.value.empty()) return cell.value;
  if (const auto* date = std::get_if<NativeDate>(&cell.typed_value))
    return date->value;
  if (const auto* integer = std::get_if<std::int64_t>(&cell.typed_value))
    return std::to_string(*integer);
  if (const auto* floating = std::get_if<double>(&cell.typed_value))
    return std::to_string(*floating);
  if (const auto* boolean = std::get_if<bool>(&cell.typed_value))
    return *boolean ? "true" : "false";
  return {};
}

const std::string* event_value(const Event& event, std::string_view key,
                               std::string_view kind) {
  if (key.empty()) {
    if (kind == "activity") return &event.activity;
    if (kind == "resource") return &event.resource;
    if (kind == "case") return &event.case_id;
    return &event.timestamp;
  }
  if (kind == "activity" &&
      (key == "activity" || key == "concept:name" ||
       key == event.activity_source_key)) {
    return &event.activity;
  }
  if (kind == "resource" &&
      (key == "resource" || key == "org:resource" ||
       key == event.resource_source_key)) {
    return &event.resource;
  }
  if (kind == "timestamp" &&
      (key == "timestamp" || key == "time:timestamp" ||
       key == event.timestamp_source_key)) {
    return &event.timestamp;
  }
  if (kind == "case" &&
      (key == "case" || key == "case_id" || key == "case:id" ||
       key == "case:concept:name" || key == "concept:name" ||
       key == event.case_id_source_key)) {
    return &event.case_id;
  }
  return event.attribute(key);
}

const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" || key == "case") {
    return &trace.case_id;
  }
  return trace.attribute(key);
}

const std::string* log_case_value(const Trace& trace, const Event& event,
                                  std::string_view key) {
  const bool canonical =
      key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" || key == "case";
  if (canonical) return trace_value(trace, key);

  // A custom case selector is an event selector.  A trace-level value remains
  // a useful fallback for logs whose case metadata was imported separately.
  if (const auto* value = event_value(event, key, "case")) return value;
  return trace.attribute(key);
}

const std::string& required_event_value(const Event& event,
                                        std::string_view key,
                                        std::string_view kind) {
  const auto* value = event_value(event, key, kind);
  if (value == nullptr) {
    throw std::invalid_argument("batches: event is missing " +
                                std::string(kind) + " attribute '" +
                                std::string(key) + "'");
  }
  return *value;
}

const std::string& required_case_value(const Trace& trace, const Event& event,
                                       std::string_view key) {
  const auto* value = log_case_value(trace, event, key);
  if (value == nullptr) {
    throw std::invalid_argument("batches: event is missing case-id attribute '" +
                                std::string(key) + "'");
  }
  return *value;
}

void validate_options(const BatchDiscoveryOptions& options) {
  if (options.variant != BatchDiscoveryVariant::kLog &&
      options.variant != BatchDiscoveryVariant::kPandas &&
      options.variant != BatchDiscoveryVariant::kPolars) {
    throw std::invalid_argument("batches: unknown discovery variant");
  }
  if (!std::isfinite(options.merge_distance_seconds) ||
      options.merge_distance_seconds < 0.0) {
    throw std::invalid_argument(
        "batches: merge distance must be finite and non-negative");
  }
  if (options.min_batch_size < 2) {
    throw std::invalid_argument("batches: min_batch_size must be at least two");
  }
  if (options.activity_attribute.empty() || options.resource_attribute.empty() ||
      options.timestamp_attribute.empty() || options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "batches: activity, resource, timestamp, and case-id attributes must not be empty");
  }
}

struct EventIdentity {
  double start = 0.0;
  double complete = 0.0;
  std::string case_id;
  std::string event_id;

  bool operator<(const EventIdentity& other) const {
    return std::tie(start, complete, case_id, event_id) <
           std::tie(other.start, other.complete, other.case_id,
                    other.event_id);
  }
};

BatchType classify(const std::vector<BatchEvent>& events) {
  if (events.empty()) return BatchType::kConcurrent;
  const double first_start = events.front().start;
  const double first_complete = events.front().complete;
  bool equal_starts = true;
  bool equal_ends = true;
  bool simultaneous = true;
  for (const auto& event : events) {
    equal_starts = equal_starts && event.start == first_start;
    equal_ends = equal_ends && event.complete == first_complete;
    simultaneous = simultaneous && event.start == first_start &&
                   event.complete == first_complete;
  }
  if (simultaneous) return BatchType::kSimultaneous;
  if (equal_starts) return BatchType::kBatchingOnStart;
  if (equal_ends) return BatchType::kBatchingOnEnd;

  bool sequential = true;
  for (std::size_t i = 1; i < events.size(); ++i) {
    if (events[i - 1].complete != events[i].start) {
      sequential = false;
      break;
    }
  }
  return sequential ? BatchType::kSequential : BatchType::kConcurrent;
}

BatchDiscovery detect_grouped(
    std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>> grouped,
    BatchDiscoveryOptions options, bool include_event_id) {
  validate_options(options);

  std::vector<BatchGroup> result;
  for (auto& [key, values] : grouped) {
    std::sort(values.begin(), values.end(), [](const IndexedEvent& left,
                                               const IndexedEvent& right) {
      return std::tie(left.event.start, left.event.complete,
                      left.event.trace_index, left.event.event_index,
                      left.event.event_id) <
             std::tie(right.event.start, right.event.complete,
                      right.event.trace_index, right.event.event_index,
                      right.event.event_id);
    });

    // The detector consumes a set of event tuples.  A table event id extends
    // that tuple; without one, repeated timestamp/case tuples are one event.
    std::set<EventIdentity> identities;
    std::vector<IndexedEvent> unique;
    unique.reserve(values.size());
    for (auto& value : values) {
      EventIdentity identity{value.event.start, value.event.complete,
                             value.event.case_id,
                             include_event_id ? value.event.event_id : ""};
      if (identities.insert(std::move(identity)).second)
        unique.push_back(std::move(value));
    }
    values = std::move(unique);

    std::vector<Batch> batches;
    std::size_t cursor = 0;
    while (cursor < values.size()) {
      std::vector<BatchEvent> component;
      component.push_back(values[cursor].event);
      double envelope_end = values[cursor].event.complete;
      ++cursor;
      while (cursor < values.size()) {
        const auto& candidate = values[cursor].event;
        const double gap = candidate.start - envelope_end;
        if (candidate.start < envelope_end ||
            gap <= options.merge_distance_seconds) {
          component.push_back(candidate);
          envelope_end = std::max(envelope_end, candidate.complete);
          ++cursor;
        } else {
          break;
        }
      }
      if (component.size() < options.min_batch_size) continue;
      Batch batch;
      batch.start = component.front().start;
      batch.complete = component.front().complete;
      for (const auto& event : component) {
        batch.start = std::min(batch.start, event.start);
        batch.complete = std::max(batch.complete, event.complete);
      }
      batch.events = std::move(component);
      batch.type = classify(batch.events);
      batches.push_back(std::move(batch));
    }
    if (batches.empty()) continue;
    BatchGroup group;
    group.activity = key.first;
    group.resource = key.second;
    group.batch_count = batches.size();
    for (const auto& batch : batches) group.event_count += batch.events.size();
    group.batches = std::move(batches);
    result.push_back(std::move(group));
  }
  std::sort(result.begin(), result.end(), [](const BatchGroup& left,
                                             const BatchGroup& right) {
    if (left.batch_count != right.batch_count)
      return left.batch_count > right.batch_count;
    return std::tie(left.activity, left.resource) >
           std::tie(right.activity, right.resource);
  });
  return BatchDiscovery{options, options.variant, std::move(result)};
}

void append_log_events(
    const EventLog& log, const BatchDiscoveryOptions& options,
    std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>>&
        grouped) {
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& event = trace.events[event_index];
      const auto& activity =
          required_event_value(event, options.activity_attribute, "activity");
      const auto& resource =
          required_event_value(event, options.resource_attribute, "resource");
      const auto& completion_text =
          required_event_value(event, options.timestamp_attribute, "timestamp");
      const auto complete = parse_timestamp(completion_text);
      if (!complete || !std::isfinite(*complete))
        throw std::invalid_argument(
            "batches: event has an invalid completion timestamp");
      double start = *complete;
      if (!options.start_timestamp_attribute.empty()) {
        const auto* value = event_value(
            event, options.start_timestamp_attribute, "timestamp");
        if (value == nullptr || value->empty())
          throw std::invalid_argument(
              "batches: configured start timestamp is missing");
        const auto parsed = parse_timestamp(*value);
        if (!parsed || !std::isfinite(*parsed))
          throw std::invalid_argument(
              "batches: configured start timestamp is invalid");
        start = *parsed;
      }
      if (!std::isfinite(start))
        throw std::invalid_argument("batches: non-finite start timestamp");
      if (start > *complete)
        throw std::invalid_argument(
            "batches: start timestamp is after completion");
      const auto& case_id = required_case_value(trace, event,
                                                options.case_id_attribute);
      BatchEvent batch_event{start, *complete, case_id, trace_index, event_index,
                             {}};
      grouped[{activity, resource}].push_back(
          IndexedEvent{std::move(batch_event), activity, resource});
    }
  }
}

std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>>
table_events(const EventTable& table, const BatchDiscoveryOptions& options) {
  table.validate();
  const auto case_selector = resolve_selector(
      table, options.case_id_attribute, SelectorRole::kCase, "case-id", true);
  const auto activity_selector = resolve_selector(
      table, options.activity_attribute, SelectorRole::kActivity, "activity", true);
  const auto resource_selector = resolve_selector(
      table, options.resource_attribute, SelectorRole::kResource, "resource", true);
  const auto timestamp_selector = resolve_selector(
      table, options.timestamp_attribute, SelectorRole::kTimestamp, "timestamp", true);
  const auto start_selector = options.start_timestamp_attribute.empty()
                                  ? TableSelector{}
                                  : resolve_selector(
                                        table, options.start_timestamp_attribute,
                                        SelectorRole::kTimestamp,
                                        "start timestamp", true);
  const auto event_id_selector = options.event_id_attribute.empty()
                                     ? TableSelector{}
                                     : resolve_selector(
                                           table, options.event_id_attribute,
                                           SelectorRole::kGeneric, "event id", true);
  const bool event_id_selected = !options.event_id_attribute.empty() &&
                                 event_id_selector.column.has_value();

  std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>> grouped;
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    const auto* activity_cell = row_cell(row, table, activity_selector);
    const auto* resource_cell = row_cell(row, table, resource_selector);
    const auto* timestamp_cell = row_cell(row, table, timestamp_selector);
    const auto* case_cell = row_cell(row, table, case_selector);
    if (!activity_cell || !resource_cell || !timestamp_cell || !case_cell) {
      throw std::invalid_argument("batches: a required table cell is missing");
    }
    const auto activity = attribute_text(*activity_cell);
    const auto resource = attribute_text(*resource_cell);
    const auto case_id = attribute_text(*case_cell);
    const auto complete = parse_timestamp(attribute_text(*timestamp_cell));
    if (!complete || !std::isfinite(*complete))
      throw std::invalid_argument(
          "batches: table row has an invalid completion timestamp");

    double start = *complete;
    if (!options.start_timestamp_attribute.empty()) {
      const auto* start_cell = row_cell(row, table, start_selector);
      if (!start_cell || attribute_text(*start_cell).empty())
        throw std::invalid_argument(
            "batches: configured start timestamp is missing");
      const auto parsed = parse_timestamp(attribute_text(*start_cell));
      if (!parsed || !std::isfinite(*parsed))
        throw std::invalid_argument(
            "batches: configured start timestamp is invalid");
      start = *parsed;
    }
    if (start > *complete)
      throw std::invalid_argument(
          "batches: start timestamp is after completion");

    std::string event_id;
    if (event_id_selected) {
      if (const auto* event_id_cell = row_cell(row, table, event_id_selector))
        event_id = attribute_text(*event_id_cell);
    }
    BatchEvent batch_event{start, *complete, case_id, 0, row_index, event_id};
    grouped[{activity, resource}].push_back(
        IndexedEvent{std::move(batch_event), activity, resource});
  }
  return grouped;
}

BatchDiscovery discover_table(const EventTable& table,
                              BatchDiscoveryOptions options,
                              BatchDiscoveryVariant variant) {
  options.variant = variant;
  validate_options(options);
  auto grouped = table_events(table, options);
  return detect_grouped(std::move(grouped), std::move(options), true);
}

}  // namespace

BatchDiscovery discover_batches(const EventLog& log,
                                BatchDiscoveryOptions options) {
  validate_options(options);
  std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>> grouped;
  append_log_events(log, options, grouped);
  return detect_grouped(std::move(grouped), std::move(options), false);
}

BatchDiscovery discover_batches(const EventTable& table,
                                BatchDiscoveryOptions options) {
  return discover_table(table, std::move(options), BatchDiscoveryVariant::kPandas);
}

BatchDiscovery discover_batches(const LazyEventTable& table,
                                BatchDiscoveryOptions options) {
  return discover_table(table.materialize(), std::move(options),
                        BatchDiscoveryVariant::kPolars);
}

BatchDiscovery BatchLogDiscovery::apply(const EventLog& log,
                                         BatchDiscoveryOptions options) {
  options.variant = BatchDiscoveryVariant::kLog;
  return discover_batches(log, std::move(options));
}

BatchDiscovery BatchPandasDiscovery::apply(const EventTable& table,
                                            BatchDiscoveryOptions options) {
  return discover_table(table, std::move(options), BatchDiscoveryVariant::kPandas);
}

BatchDiscovery BatchPolarsDiscovery::apply(const LazyEventTable& table,
                                            BatchDiscoveryOptions options) {
  return discover_table(table.materialize(), std::move(options),
                        BatchDiscoveryVariant::kPolars);
}

}  // namespace minera
