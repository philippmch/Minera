#include "minera/minimum_self_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/log_conversion.hpp"

namespace minera {
namespace {

struct ActivitySequence {
  std::string case_id;
  std::vector<std::string> activities;
};

struct DistanceSegment {
  std::string activity;
  std::size_t sequence_index = 0;
  std::size_t left = 0;
  std::size_t right = 0;
  std::uint64_t distance = 0;
};

struct DiscoveryData {
  MinimumSelfDistances distances;
  std::vector<ActivitySequence> sequences;
  std::vector<DistanceSegment> segments;
};

const std::string* first_class_value(
    const std::string& value, FirstClassAttributeState state) {
  if (state == FirstClassAttributeState::kNull) return nullptr;
  if (state != FirstClassAttributeState::kMissing || !value.empty())
    return &value;
  return nullptr;
}

bool matches_source(std::string_view source_key, std::string_view selector) {
  return !source_key.empty() && source_key == selector;
}

const std::string* selected_event_value(const Event& event,
                                        std::string_view key,
                                        std::string_view role) {
  if (role == "activity" &&
      (key.empty() || key == "activity" || key == "concept:name" ||
       matches_source(event.activity_source_key, key))) {
    if (const auto* value =
            first_class_value(event.activity, event.activity_state))
      return value;
    if (event.activity_state != FirstClassAttributeState::kNull &&
        event.activity_attribute && !event.activity_attribute->is_null &&
        (key.empty() || key == event.activity_attribute->key ||
         matches_source(event.activity_source_key, key)))
      return &event.activity_attribute->value;
  }
  if (role == "resource" &&
      (key == "resource" || key == "org:resource" ||
       matches_source(event.resource_source_key, key))) {
    if (const auto* value =
            first_class_value(event.resource, event.resource_state))
      return value;
    if (event.resource_state != FirstClassAttributeState::kNull &&
        event.resource_attribute && !event.resource_attribute->is_null)
      return &event.resource_attribute->value;
  }
  if (role == "lifecycle" &&
      (key == "lifecycle" || key == "lifecycle:transition" ||
       matches_source(event.lifecycle_source_key, key))) {
    if (const auto* value =
            first_class_value(event.lifecycle, event.lifecycle_state))
      return value;
    if (event.lifecycle_state != FirstClassAttributeState::kNull &&
        event.lifecycle_attribute && !event.lifecycle_attribute->is_null)
      return &event.lifecycle_attribute->value;
  }
  if (role == "timestamp" &&
      (key.empty() || key == "timestamp" || key == "time:timestamp" ||
       matches_source(event.timestamp_source_key, key))) {
    if (const auto* value =
            first_class_value(event.timestamp, event.timestamp_state))
      return value;
    if (event.timestamp_state != FirstClassAttributeState::kNull &&
        event.timestamp_attribute && !event.timestamp_attribute->is_null &&
        (key.empty() || key == event.timestamp_attribute->key ||
         matches_source(event.timestamp_source_key, key)))
      return &event.timestamp_attribute->value;
  }
  return event.attribute(key);
}

const std::string* selected_case_value(const Event& event,
                                       std::string_view key) {
  if (key.empty() || key == "case" || key == "case_id" ||
      key == "case:id" || key == "case:concept:name" ||
      key == "concept:name" || matches_source(event.case_id_source_key, key))
    if (const auto* value =
            first_class_value(event.case_id, event.case_id_state))
      return value;
  return event.attribute(key);
}

const std::string* selected_trace_case_value(const Trace& trace,
                                             std::string_view key) {
  if (key.empty() || key == "case" || key == "case_id" ||
      key == "case:id" || key == "case:concept:name" ||
      key == "concept:name" || matches_source(trace.case_id_source_key, key)) {
    if (const auto* value =
            first_class_value(trace.case_id, trace.case_id_state))
      return value;
    if (trace.case_id_state != FirstClassAttributeState::kNull &&
        trace.case_id_attribute && !trace.case_id_attribute->is_null)
      return &trace.case_id_attribute->value;
  }
  return trace.attribute(key);
}

FirstClassAttributeRole canonical_activity_role(std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return FirstClassAttributeRole::kActivity;
  if (key == "resource" || key == "org:resource")
    return FirstClassAttributeRole::kResource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return FirstClassAttributeRole::kLifecycle;
  if (key == "timestamp" || key == "time:timestamp")
    return FirstClassAttributeRole::kTimestamp;
  // A custom source spelling is resolved from the first-class metadata on an
  // Event or flat cell. Generic attributes remain valid activity selectors.
  return FirstClassAttributeRole::kNone;
}

std::string_view first_class_role_name(FirstClassAttributeRole role) {
  switch (role) {
    case FirstClassAttributeRole::kActivity: return "activity";
    case FirstClassAttributeRole::kTimestamp: return "timestamp";
    case FirstClassAttributeRole::kResource: return "resource";
    case FirstClassAttributeRole::kLifecycle: return "lifecycle";
    case FirstClassAttributeRole::kCaseId:
    case FirstClassAttributeRole::kNone: break;
  }
  return {};
}

FirstClassAttributeRole selected_activity_role(const Event& event,
                                               std::string_view key) {
  const auto canonical_role = canonical_activity_role(key);
  if (canonical_role != FirstClassAttributeRole::kNone)
    return canonical_role;
  if (matches_source(event.activity_source_key, key))
    return FirstClassAttributeRole::kActivity;
  if (matches_source(event.resource_source_key, key))
    return FirstClassAttributeRole::kResource;
  if (matches_source(event.lifecycle_source_key, key))
    return FirstClassAttributeRole::kLifecycle;
  if (matches_source(event.timestamp_source_key, key))
    return FirstClassAttributeRole::kTimestamp;
  return FirstClassAttributeRole::kNone;
}

std::string selected_activity(const Event& event, std::string_view key) {
  const auto role = selected_activity_role(event, key);
  if (role != FirstClassAttributeRole::kNone) {
    if (const auto* value = selected_event_value(
            event, key, first_class_role_name(role)))
      return *value;
  } else if (const auto* value = event.attribute(key)) {
    return *value;
  }
  throw std::invalid_argument(
      "minimum self-distance event is missing activity attribute '" +
      std::string(key) + "'");
}

bool canonical_case_selector(std::string_view key) {
  return key.empty() || key == "case_id" || key == "case:id" ||
         key == "concept:name" || key == "case:concept:name" ||
         key == "case";
}

std::string selected_case_id(const Trace& trace, const Event& event,
                             std::string_view key) {
  if (canonical_case_selector(key)) {
    if (const auto* value = selected_trace_case_value(trace, key)) return *value;
  } else {
    if (const auto* value = selected_case_value(event, key)) return *value;
    if (const auto* value = selected_trace_case_value(trace, key)) return *value;
  }
  if (key.rfind("case:", 0) == 0) {
    if (const auto* value = trace.attribute(key.substr(5))) return *value;
  } else if (const auto* value =
                 trace.attribute("case:" + std::string(key))) {
    return *value;
  }
  throw std::invalid_argument(
      "minimum self-distance trace is missing case-id attribute '" +
      std::string(key) + "'");
}

std::string effective_activity_key(const MinimumSelfDistanceOptions& options) {
  if (options.activity_key.empty())
    throw std::invalid_argument(
        "minimum self-distance activity selector must not be empty");
  return options.activity_key;
}

std::string effective_case_id_key(const MinimumSelfDistanceOptions& options) {
  if (options.case_id_key.empty())
    throw std::invalid_argument(
        "minimum self-distance case-id selector must not be empty");
  return options.case_id_key;
}

std::vector<ActivitySequence> event_log_sequences(
    const EventLog& log, const MinimumSelfDistanceOptions& options) {
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  std::vector<ActivitySequence> sequences;
  sequences.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    if (canonical_case_selector(case_id_key)) {
      ActivitySequence sequence;
      sequence.case_id = trace.case_id;
      sequence.activities.reserve(trace.events.size());
      for (const auto& event : trace.events)
        sequence.activities.push_back(selected_activity(event, activity_key));
      sequences.push_back(std::move(sequence));
      continue;
    }

    // A non-standard case selector remains available for callers whose trace
    // object intentionally contains several logical cases. Standard EventLog
    // case selectors above always use the enclosing trace boundary.
    std::map<std::string, std::size_t> local_groups;
    for (const auto& event : trace.events) {
      const auto case_id = selected_case_id(trace, event, case_id_key);
      const auto [found, inserted] =
          local_groups.emplace(case_id, sequences.size());
      if (inserted) {
        ActivitySequence sequence;
        sequence.case_id = case_id;
        sequences.push_back(std::move(sequence));
      }
      sequences[found->second].activities.push_back(
          selected_activity(event, activity_key));
    }
  }
  return sequences;
}

const EventAttribute* find_stream_field(
    const EventRecord& record, std::string_view key,
    FirstClassAttributeRole role,
    std::initializer_list<std::string_view> aliases) {
  if (!key.empty()) {
    if (const auto* exact = record.field(key)) {
      if (role != FirstClassAttributeRole::kNone &&
          exact->first_class_role != FirstClassAttributeRole::kNone &&
          exact->first_class_role != role)
        return nullptr;
      return exact;
    }
    for (const auto& field : record.fields) {
      if (field.first_class_source_key != key) continue;
      if (role != FirstClassAttributeRole::kNone &&
          field.first_class_role != FirstClassAttributeRole::kNone &&
          field.first_class_role != role)
        return nullptr;
      return &field;
    }
  }
  const auto is_alias = std::any_of(
      aliases.begin(), aliases.end(),
      [&](std::string_view alias) { return alias == key; });
  if (key.empty() || is_alias) {
    for (const auto& field : record.fields) {
      if (field.first_class_role == role) return &field;
    }
  }
  if (key.empty() || is_alias) {
    for (const auto alias : aliases) {
      if (const auto* exact = record.field(alias)) {
        if (role != FirstClassAttributeRole::kNone &&
            exact->first_class_role != FirstClassAttributeRole::kNone &&
            exact->first_class_role != role)
          return nullptr;
        return exact;
      }
    }
  }
  return nullptr;
}

const EventAttribute* find_table_field(
    const EventTable& table, const EventTable::Row& row, std::string_view key,
    FirstClassAttributeRole role,
    std::initializer_list<std::string_view> aliases) {
  auto find_column = [&](std::string_view name) -> const EventAttribute* {
    const auto found = std::find(table.columns.begin(), table.columns.end(),
                                 name);
    if (found == table.columns.end()) return nullptr;
    const auto index = static_cast<std::size_t>(
        std::distance(table.columns.begin(), found));
    if (!row[index]) return nullptr;
    return &*row[index];
  };
  if (!key.empty()) {
    if (const auto* exact = find_column(key)) {
      if (role != FirstClassAttributeRole::kNone &&
          exact->first_class_role != FirstClassAttributeRole::kNone &&
          exact->first_class_role != role)
        return nullptr;
      return exact;
    }
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index] || row[index]->first_class_source_key != key) continue;
      if (role != FirstClassAttributeRole::kNone &&
          row[index]->first_class_role != FirstClassAttributeRole::kNone &&
          row[index]->first_class_role != role)
        return nullptr;
      return &*row[index];
    }
  }
  const auto is_alias = std::any_of(
      aliases.begin(), aliases.end(),
      [&](std::string_view alias) { return alias == key; });
  if (key.empty() || is_alias) {
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index]) continue;
      if (row[index]->first_class_role == role) return &*row[index];
    }
  }
  if (key.empty() || is_alias) {
    for (const auto alias : aliases) {
      if (const auto* exact = find_column(alias)) {
        if (role != FirstClassAttributeRole::kNone &&
            exact->first_class_role != FirstClassAttributeRole::kNone &&
            exact->first_class_role != role)
          return nullptr;
        return exact;
      }
    }
  }
  return nullptr;
}

const EventAttribute* find_activity_stream_field(const EventRecord& record,
                                                 std::string_view key) {
  switch (canonical_activity_role(key)) {
    case FirstClassAttributeRole::kActivity:
      return find_stream_field(record, key, FirstClassAttributeRole::kActivity,
                               {"activity", "concept:name"});
    case FirstClassAttributeRole::kTimestamp:
      return find_stream_field(record, key,
                               FirstClassAttributeRole::kTimestamp,
                               {"timestamp", "time:timestamp"});
    case FirstClassAttributeRole::kResource:
      return find_stream_field(record, key, FirstClassAttributeRole::kResource,
                               {"resource", "org:resource"});
    case FirstClassAttributeRole::kLifecycle:
      return find_stream_field(
          record, key, FirstClassAttributeRole::kLifecycle,
          {"lifecycle", "lifecycle:transition"});
    case FirstClassAttributeRole::kCaseId:
    case FirstClassAttributeRole::kNone:
      return find_stream_field(record, key, FirstClassAttributeRole::kNone, {});
  }
  return nullptr;
}

const EventAttribute* find_activity_table_field(
    const EventTable& table, const EventTable::Row& row, std::string_view key) {
  switch (canonical_activity_role(key)) {
    case FirstClassAttributeRole::kActivity:
      return find_table_field(table, row, key,
                              FirstClassAttributeRole::kActivity,
                              {"activity", "concept:name"});
    case FirstClassAttributeRole::kTimestamp:
      return find_table_field(table, row, key,
                              FirstClassAttributeRole::kTimestamp,
                              {"timestamp", "time:timestamp"});
    case FirstClassAttributeRole::kResource:
      return find_table_field(table, row, key,
                              FirstClassAttributeRole::kResource,
                              {"resource", "org:resource"});
    case FirstClassAttributeRole::kLifecycle:
      return find_table_field(table, row, key,
                              FirstClassAttributeRole::kLifecycle,
                              {"lifecycle", "lifecycle:transition"});
    case FirstClassAttributeRole::kCaseId:
    case FirstClassAttributeRole::kNone:
      return find_table_field(table, row, key,
                              FirstClassAttributeRole::kNone, {});
  }
  return nullptr;
}

const EventAttribute& required_flat_field(const EventAttribute* field,
                                          std::string_view kind,
                                          std::string_view key) {
  if (field == nullptr || field->is_null ||
      field->first_class_state == FirstClassAttributeState::kNull) {
    throw std::invalid_argument("minimum self-distance event is missing " +
                                std::string(kind) + " attribute '" +
                                std::string(key) + "'");
  }
  return *field;
}

template <typename RowReader>
std::vector<ActivitySequence> flat_sequences(std::string_view activity_key,
                                              std::string_view case_id_key,
                                              RowReader&& read_rows) {
  if (activity_key.empty() || case_id_key.empty())
    throw std::invalid_argument(
        "minimum self-distance selectors must not be empty");
  std::vector<ActivitySequence> sequences;
  std::map<std::string, std::size_t> groups;
  read_rows([&](const EventAttribute* activity,
                const EventAttribute* case_id) {
    const auto& selected_activity_field =
        required_flat_field(activity, "activity", activity_key);
    const auto& selected_case_field =
        required_flat_field(case_id, "case-id", case_id_key);
    const auto [found, inserted] =
        groups.emplace(selected_case_field.value, sequences.size());
    if (inserted) {
      ActivitySequence sequence;
      sequence.case_id = selected_case_field.value;
      sequences.push_back(std::move(sequence));
    }
    sequences[found->second].activities.push_back(
        selected_activity_field.value);
  });
  return sequences;
}

std::vector<ActivitySequence> stream_sequences(
    const EventStream& stream, const MinimumSelfDistanceOptions& options) {
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  return flat_sequences(activity_key, case_id_key,
                        [&](const auto& consume) {
                          for (const auto& record : stream.events) {
                            consume(find_activity_stream_field(record,
                                                                activity_key),
                                    find_stream_field(
                                        record, case_id_key,
                                        FirstClassAttributeRole::kCaseId,
                                        {"case_id", "case:id",
                                         "case:concept:name"}));
                          }
                        });
}

std::vector<ActivitySequence> table_sequences(
    const EventTable& table, const MinimumSelfDistanceOptions& options) {
  table.validate();
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  return flat_sequences(activity_key, case_id_key,
                        [&](const auto& consume) {
                          for (const auto& row : table.rows) {
                            consume(find_activity_table_field(table, row,
                                                              activity_key),
                                    find_table_field(
                                        table, row, case_id_key,
                                        FirstClassAttributeRole::kCaseId,
                                        {"case_id", "case:id",
                                         "case:concept:name"}));
                          }
                        });
}

DiscoveryData discover_from_sequences(std::vector<ActivitySequence> sequences) {
  DiscoveryData data;
  data.sequences = std::move(sequences);
  std::map<std::string, std::uint64_t> minima;
  for (std::size_t sequence_index = 0; sequence_index < data.sequences.size();
       ++sequence_index) {
    const auto& activities = data.sequences[sequence_index].activities;
    std::map<std::string, std::pair<std::size_t, std::size_t>> previous;
    for (std::size_t index = 0; index < activities.size(); ++index) {
      const auto& activity = activities[index];
      const auto found = previous.find(activity);
      if (found != previous.end()) {
        const auto distance = static_cast<std::uint64_t>(
            index - found->second.first - 1);
        data.segments.push_back(
            {activity, sequence_index, found->second.second, index, distance});
        const auto minimum = minima.find(activity);
        if (minimum == minima.end() || distance < minimum->second)
          minima[activity] = distance;
      }
      previous[activity] = {index, index};
    }
  }

  data.distances.reserve(minima.size());
  for (const auto& [activity, distance] : minima)
    data.distances.push_back({activity, distance, {}});
  return data;
}

MinimumSelfDistanceWitnessMap derive_from_sequences(
    const DiscoveryData& data,
    const std::map<std::string, std::uint64_t>& requested) {
  std::map<std::string, std::uint64_t> observed;
  for (const auto& distance : data.distances)
    observed.emplace(distance.activity, distance.distance);

  MinimumSelfDistanceWitnessMap witnesses;
  for (const auto& [activity, distance] : requested) {
    auto& target = witnesses[activity];
    if (distance == 0) continue;
    const auto observed_it = observed.find(activity);
    if (observed_it == observed.end() || observed_it->second != distance)
      continue;
    for (const auto& segment : data.segments) {
      if (segment.activity != activity || segment.distance != distance)
        continue;
      const auto& activities = data.sequences[segment.sequence_index].activities;
      for (std::size_t index = segment.left + 1; index < segment.right;
           ++index)
        target.insert(activities[index]);
    }
  }
  return witnesses;
}

MinimumSelfDistanceWitnessMap derive_from_log(
    const EventLog& log, const std::map<std::string, std::uint64_t>& requested,
    const MinimumSelfDistanceOptions& options) {
  return derive_from_sequences(
      discover_from_sequences(event_log_sequences(log, options)), requested);
}

void force_variant(MinimumSelfDistanceOptions& options,
                   MinimumSelfDistanceVariant variant) {
  options.variant = variant;
}

void validate_variant(MinimumSelfDistanceVariant variant) {
  if (variant != MinimumSelfDistanceVariant::kLog &&
      variant != MinimumSelfDistanceVariant::kPandas &&
      variant != MinimumSelfDistanceVariant::kPolars) {
    throw std::invalid_argument(
        "minimum_self_distance_variant must be log, pandas, or polars");
  }
}

}  // namespace

MinimumSelfDistances discover_minimum_self_distances(
    const EventLog& log, const MinimumSelfDistanceOptions& options) {
  validate_variant(options.variant);
  auto data = discover_from_sequences(event_log_sequences(log, options));
  const auto witnesses = derive_from_sequences(
      data, [&] {
        std::map<std::string, std::uint64_t> requested;
        for (const auto& distance : data.distances)
          requested.emplace(distance.activity, distance.distance);
        return requested;
      }());
  for (auto& distance : data.distances) {
    const auto found = witnesses.find(distance.activity);
    if (found != witnesses.end() && distance.distance != 0)
      distance.witnesses.assign(found->second.begin(), found->second.end());
  }
  return data.distances;
}

MinimumSelfDistances discover_minimum_self_distances(
    std::initializer_list<Trace> traces,
    const MinimumSelfDistanceOptions& options) {
  EventLog log;
  log.traces.assign(traces.begin(), traces.end());
  return discover_minimum_self_distances(log, options);
}

MinimumSelfDistances discover_minimum_self_distances(
    const EventStream& stream, const MinimumSelfDistanceOptions& options) {
  validate_variant(options.variant);
  auto data = discover_from_sequences(stream_sequences(stream, options));
  const auto witnesses = derive_from_sequences(
      data, [&] {
        std::map<std::string, std::uint64_t> requested;
        for (const auto& distance : data.distances)
          requested.emplace(distance.activity, distance.distance);
        return requested;
      }());
  for (auto& distance : data.distances) {
    const auto found = witnesses.find(distance.activity);
    if (found != witnesses.end() && distance.distance != 0)
      distance.witnesses.assign(found->second.begin(), found->second.end());
  }
  return data.distances;
}

MinimumSelfDistances discover_minimum_self_distances(
    const EventTable& table, const MinimumSelfDistanceOptions& options) {
  validate_variant(options.variant);
  auto data = discover_from_sequences(table_sequences(table, options));
  const auto witnesses = derive_from_sequences(
      data, [&] {
        std::map<std::string, std::uint64_t> requested;
        for (const auto& distance : data.distances)
          requested.emplace(distance.activity, distance.distance);
        return requested;
      }());
  for (auto& distance : data.distances) {
    const auto found = witnesses.find(distance.activity);
    if (found != witnesses.end() && distance.distance != 0)
      distance.witnesses.assign(found->second.begin(), found->second.end());
  }
  return data.distances;
}

MinimumSelfDistances discover_minimum_self_distances(
    const LazyEventTable& table, const MinimumSelfDistanceOptions& options) {
  if (!table.valid())
    throw std::invalid_argument("minimum self-distance lazy table is invalid");
  return discover_minimum_self_distances(table.materialize(), options);
}

MinimumSelfDistances discover_minimum_self_distances(
    const EventDataView& data, const MinimumSelfDistanceOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return discover_minimum_self_distances(*data.log(), options);
    case EventDataKind::kEventStream:
      return discover_minimum_self_distances(*data.stream(), options);
    case EventDataKind::kEventTable:
      return discover_minimum_self_distances(*data.table(), options);
    case EventDataKind::kLazyEventTable:
      return discover_minimum_self_distances(*data.lazy_table(), options);
  }
  throw std::invalid_argument("event data: invalid input kind");
}

MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const std::map<std::string, std::uint64_t>& msd,
    const MinimumSelfDistanceOptions& options) {
  validate_variant(options.variant);
  (void)effective_activity_key(options);
  (void)effective_case_id_key(options);
  return derive_from_log(log, msd, options);
}

MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const MinimumSelfDistanceOptions& options) {
  const auto distances = discover_minimum_self_distances(log, options);
  std::map<std::string, std::uint64_t> msd;
  for (const auto& distance : distances)
    msd.emplace(distance.activity, distance.distance);
  return derive_msd_witnesses(log, msd, options);
}

MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log,
    const std::map<std::string, std::uint64_t>* msd,
    const MinimumSelfDistanceOptions& options) {
  if (msd != nullptr) return derive_msd_witnesses(log, *msd, options);
  return derive_msd_witnesses(log, options);
}

MinimumSelfDistanceWitnessMap derive_msd_witnesses(
    const EventLog& log, const MinimumSelfDistances& distances,
    const MinimumSelfDistanceOptions& options) {
  return derive_minimum_self_distance_witnesses(log, distances, options);
}

MinimumSelfDistanceWitnessMap derive_minimum_self_distance_witnesses(
    const EventLog& log, const MinimumSelfDistances& distances,
    const MinimumSelfDistanceOptions& options) {
  std::map<std::string, std::uint64_t> msd;
  for (const auto& distance : distances)
    msd.emplace(distance.activity, distance.distance);
  return derive_msd_witnesses(log, msd, options);
}

MinimumSelfDistances MinimumSelfDistanceLogDiscovery::apply(
    const EventLog& log, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kLog);
  return discover_minimum_self_distances(log, options);
}

MinimumSelfDistances MinimumSelfDistanceLogDiscovery::apply(
    const EventStream& stream, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kLog);
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_key;
  return discover_minimum_self_distances(
      event_stream_to_event_log(stream, conversion), options);
}

MinimumSelfDistances MinimumSelfDistanceLogDiscovery::apply(
    const EventDataView& data, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kLog);
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_key;
  return discover_minimum_self_distances(
      event_data_to_event_log(data, conversion), options);
}

MinimumSelfDistances MinimumSelfDistancePandasDiscovery::apply(
    const EventTable& table, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kPandas);
  return discover_minimum_self_distances(table, options);
}

MinimumSelfDistances MinimumSelfDistancePandasDiscovery::apply(
    const EventDataView& data, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kPandas);
  return discover_minimum_self_distances(event_data_to_table(data), options);
}

MinimumSelfDistances MinimumSelfDistancePolarsDiscovery::apply(
    const LazyEventTable& table, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kPolars);
  return discover_minimum_self_distances(table, options);
}

MinimumSelfDistances MinimumSelfDistancePolarsDiscovery::apply(
    const EventDataView& data, MinimumSelfDistanceOptions options) {
  force_variant(options, MinimumSelfDistanceVariant::kPolars);
  return discover_minimum_self_distances(event_data_to_lazy_table(data),
                                         options);
}

const std::vector<MinimumSelfDistanceSelectorDescriptor>&
MinimumSelfDistanceSelectorRegistry::all() {
  static const std::vector<MinimumSelfDistanceSelectorDescriptor> selectors = {
      {MinimumSelfDistanceSelector::kLog, "LOG"},
      {MinimumSelfDistanceSelector::kPandas, "PANDAS"},
      {MinimumSelfDistanceSelector::kPolars, "POLARS"}};
  return selectors;
}

bool MinimumSelfDistanceSelectorRegistry::supports(
    MinimumSelfDistanceSelector selector) noexcept {
  return selector == MinimumSelfDistanceSelector::kLog ||
         selector == MinimumSelfDistanceSelector::kPandas ||
         selector == MinimumSelfDistanceSelector::kPolars;
}

std::optional<MinimumSelfDistanceSelector>
MinimumSelfDistanceSelectorRegistry::parse(std::string_view name) {
  if (name == "LOG" || name == "log" || name == "Variants.LOG")
    return MinimumSelfDistanceSelector::kLog;
  if (name == "PANDAS" || name == "pandas" || name == "Variants.PANDAS")
    return MinimumSelfDistanceSelector::kPandas;
  if (name == "POLARS" || name == "polars" || name == "Variants.POLARS")
    return MinimumSelfDistanceSelector::kPolars;
  return std::nullopt;
}

MinimumSelfDistances MinimumSelfDistanceSelectorRegistry::dispatch(
    const EventLog& log, MinimumSelfDistanceSelector selector,
    MinimumSelfDistanceOptions options) {
  switch (selector) {
    case MinimumSelfDistanceSelector::kLog:
      return MinimumSelfDistanceLogDiscovery::apply(log, options);
    case MinimumSelfDistanceSelector::kPandas:
      return MinimumSelfDistancePandasDiscovery::apply(
          event_log_to_table(log), options);
    case MinimumSelfDistanceSelector::kPolars:
      return MinimumSelfDistancePolarsDiscovery::apply(
          event_log_to_lazy_table(log), options);
  }
  throw std::invalid_argument("minimum self-distance: unsupported selector");
}

MinimumSelfDistances MinimumSelfDistanceSelectorRegistry::dispatch(
    const EventStream& stream, MinimumSelfDistanceSelector selector,
    MinimumSelfDistanceOptions options) {
  return dispatch(EventDataView(stream), selector, std::move(options));
}

MinimumSelfDistances MinimumSelfDistanceSelectorRegistry::dispatch(
    const EventTable& table, MinimumSelfDistanceSelector selector,
    MinimumSelfDistanceOptions options) {
  return dispatch(EventDataView(table), selector, std::move(options));
}

MinimumSelfDistances MinimumSelfDistanceSelectorRegistry::dispatch(
    const LazyEventTable& table, MinimumSelfDistanceSelector selector,
    MinimumSelfDistanceOptions options) {
  return dispatch(EventDataView(table), selector, std::move(options));
}

MinimumSelfDistances MinimumSelfDistanceSelectorRegistry::dispatch(
    const EventDataView& data, MinimumSelfDistanceSelector selector,
    MinimumSelfDistanceOptions options) {
  switch (selector) {
    case MinimumSelfDistanceSelector::kLog:
      return MinimumSelfDistanceLogDiscovery::apply(data, std::move(options));
    case MinimumSelfDistanceSelector::kPandas:
      return MinimumSelfDistancePandasDiscovery::apply(data,
                                                       std::move(options));
    case MinimumSelfDistanceSelector::kPolars:
      return MinimumSelfDistancePolarsDiscovery::apply(data,
                                                       std::move(options));
  }
  throw std::invalid_argument("minimum self-distance: unsupported selector");
}

std::string_view minimum_self_distance_variant_name(
    MinimumSelfDistanceSelector selector) {
  switch (selector) {
    case MinimumSelfDistanceSelector::kLog: return "LOG";
    case MinimumSelfDistanceSelector::kPandas: return "PANDAS";
    case MinimumSelfDistanceSelector::kPolars: return "POLARS";
  }
  throw std::invalid_argument("minimum self-distance: invalid selector");
}

MinimumSelfDistanceSelector parse_minimum_self_distance_variant(
    std::string_view name) {
  const auto parsed = MinimumSelfDistanceSelectorRegistry::parse(name);
  if (!parsed)
    throw std::invalid_argument("minimum self-distance: unsupported selector");
  return *parsed;
}

}  // namespace minera
