#include "minera/temporal_profile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/cancellation.hpp"
#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"
#include "business_hours.hpp"
#include "exact_stdev.hpp"
#include "frame_cells.hpp"
#include "frame_temporal_profile.hpp"
#include "frame_times.hpp"

namespace minera {
namespace {

constexpr double kWeekSeconds = 7.0 * 86400.0;

std::vector<BusinessHourSlot> default_slots() {
  std::vector<BusinessHourSlot> slots;
  slots.reserve(5);
  for (int day = 0; day < 5; ++day) {
    const double start = day * 86400.0 + 7.0 * 3600.0;
    slots.push_back({start, start + 10.0 * 3600.0});
  }
  return slots;
}

std::vector<BusinessHourSlot> normalized_slots(
    const TemporalProfileOptions& options) {
  auto slots = !options.business_hour_slots_explicit &&
                       options.business_hour_slots.empty()
                   ? default_slots()
                   : options.business_hour_slots;
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) || !std::isfinite(slot.end_seconds) ||
        slot.start_seconds < 0.0 || slot.end_seconds > kWeekSeconds ||
        slot.start_seconds >= slot.end_seconds) {
      throw std::invalid_argument(
          "temporal profile: business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.start_seconds != rhs.start_seconds) {
      return lhs.start_seconds < rhs.start_seconds;
    }
    return lhs.end_seconds < rhs.end_seconds;
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    if (!merged.empty() && slot.start_seconds <= merged.back().end_seconds) {
      merged.back().end_seconds = std::max(merged.back().end_seconds,
                                            slot.end_seconds);
    } else {
      merged.push_back(slot);
    }
  }
  return merged;
}

const std::string& configured_attribute(const std::string& primary,
                                        const std::string& alias) {
  return primary.empty() ? alias : primary;
}

std::optional<double> local_wall_instant(std::string_view text);

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

std::optional<std::string> event_activity(
    const Event& event, const TemporalProfileOptions& options) {
  const auto* value = selected_event_value(
      event, options.activity_attribute, "activity");
  if (value != nullptr) return *value;
  if (options.strict_attributes) {
    throw std::invalid_argument(
        "temporal profile: selected activity attribute is missing");
  }
  return std::nullopt;
}

std::optional<double> event_instant(const Event& event,
                                    const std::string& primary,
                                    const std::string& alias,
                                    bool strict_attributes) {
  const auto& attribute_name = configured_attribute(primary, alias);
  const auto* value = selected_event_value(event, attribute_name, "timestamp");
  if (value == nullptr) {
    if (strict_attributes) {
      throw std::invalid_argument(
          "temporal profile: selected timestamp attribute is missing");
    }
    return std::nullopt;
  }
  const auto instant = parse_timestamp(*value);
  if (!instant && strict_attributes) {
    throw std::invalid_argument(
        "temporal profile: selected timestamp attribute is invalid");
  }
  return instant;
}

std::optional<double> event_local_instant(const Event& event,
                                          const std::string& primary,
                                          const std::string& alias,
                                          bool strict_attributes) {
  const auto& attribute_name = configured_attribute(primary, alias);
  const auto* value = selected_event_value(event, attribute_name, "timestamp");
  if (value == nullptr) {
    if (strict_attributes) {
      throw std::invalid_argument(
          "temporal profile: selected timestamp attribute is missing");
    }
    return std::nullopt;
  }
  const auto instant = local_wall_instant(*value);
  if (!instant && strict_attributes) {
    throw std::invalid_argument(
        "temporal profile: selected timestamp attribute is invalid");
  }
  return instant;
}

// Business-hour semantics follow the wall-clock treatment: timezone
// offsets are validated but ignored when selecting the weekday and clock
// time. Ordinary durations continue to use the absolute UTC instants above.
std::optional<double> local_wall_instant(std::string_view text) {
  // Validate the full instant first, then remove only its timezone suffix and
  // parse the remaining wall-clock fields as if they were UTC. This preserves
  // the local weekday/time without duplicating the ISO-8601 parser.
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

// pm4py's BusinessHours on microsecond wall clocks
// (detail::business_elapsed_seconds_microseconds); an epoch double holds its
// microsecond exactly enough for rounding to recover it (review AY1).
double business_elapsed(double source, double target,
                        const std::vector<BusinessHourSlot>& slots) {
  return detail::business_elapsed_seconds_microseconds(std::llround(source * 1e6),
                                                       std::llround(target * 1e6), slots);
}

double elapsed_between(double source, double target,
                       double source_local, double target_local,
                       const TemporalProfileOptions& options,
                       const std::vector<BusinessHourSlot>* slots) {
  if (!options.use_business_hours) return target - source;
  return business_elapsed(source_local, target_local, *slots);
}

void validate_slots(const TemporalProfileOptions& options) {
  if (options.use_business_hours) (void)normalized_slots(options);
}

using ProfileLookup =
    std::map<std::pair<std::string, std::string>,
             const TemporalProfileEntry*>;

ProfileLookup build_profile_lookup(const TemporalProfile& profile) {
  ProfileLookup lookup;
  for (const auto& entry : profile.entries) {
    if (entry.count == 0 || !std::isfinite(entry.mean_seconds) ||
        entry.mean_seconds < 0.0 ||
        !std::isfinite(entry.standard_deviation_seconds) ||
        entry.standard_deviation_seconds < 0.0) {
      throw std::invalid_argument(
          "temporal profile: count must be positive and timing statistics must be finite and non-negative");
    }
    if (!lookup
             .emplace(std::make_pair(entry.source, entry.target), &entry)
             .second) {
      throw std::invalid_argument(
          "temporal profile: duplicate source/target profile entry");
    }
  }
  return lookup;
}

// pm4py's EventLog temporal profile takes a pair's mean and deviation with
// statistics.mean and statistics.stdev over the epoch-second durations:
// exact sums rounded once (round BI, observed black-box; a running Welford
// mean was a few ulps off in most profiles).
using Aggregate = detail::ExactMoments;

struct TemporalTableColumns {
  std::size_t case_id = 0;
  std::size_t activity = 0;
  std::size_t source_timestamp = 0;
  std::size_t target_timestamp = 0;
};

std::optional<std::size_t> exact_table_column(const EventTable& table,
                                              std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::optional<std::size_t> temporal_table_column(
    const EventTable& table, std::string_view selector,
    FirstClassAttributeRole role,
    std::initializer_list<std::string_view> aliases) {
  if (const auto exact = exact_table_column(table, selector)) return exact;
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    for (const auto& row : table.rows) {
      if (index < row.size() && row[index] &&
          row[index]->first_class_role == role &&
          row[index]->first_class_source_key == selector) {
        return index;
      }
    }
  }
  const bool canonical = std::any_of(
      aliases.begin(), aliases.end(),
      [&](std::string_view alias) { return alias == selector; });
  if (!canonical) return std::nullopt;
  for (const auto alias : aliases) {
    if (const auto exact = exact_table_column(table, alias)) return exact;
  }
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    for (const auto& row : table.rows) {
      if (index < row.size() && row[index] &&
          row[index]->first_class_role == role) {
        return index;
      }
    }
  }
  return std::nullopt;
}

std::string source_timestamp_key(const TemporalProfileOptions& options) {
  const auto& configured = configured_attribute(
      options.source_timestamp_attribute,
      options.source_completion_timestamp_attribute);
  return configured.empty() ? "time:timestamp" : configured;
}

std::string target_timestamp_key(const TemporalProfileOptions& options) {
  const auto& configured = configured_attribute(
      options.target_timestamp_attribute,
      options.target_start_timestamp_attribute);
  // The target column is independent from the source/completion selector.
  // When only the source is customized, tabular adapters still use their
  // canonical timestamp column for the target/start side.
  return configured.empty() ? "time:timestamp" : configured;
}

TemporalTableColumns temporal_table_columns(
    const EventTable& table, const TemporalProfileOptions& options) {
  if (options.activity_attribute.empty() || options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "temporal profile: activity and case-id attributes must not be empty");
  }
  table.validate();
  const auto case_id = temporal_table_column(
      table, options.case_id_attribute, FirstClassAttributeRole::kCaseId,
      {"case_id", "case:id", "case:concept:name", "concept:name"});
  const auto activity = temporal_table_column(
      table, options.activity_attribute, FirstClassAttributeRole::kActivity,
      {"activity", "concept:name"});
  const auto source = temporal_table_column(
      table, source_timestamp_key(options),
      FirstClassAttributeRole::kTimestamp,
      {"timestamp", "time:timestamp"});
  const auto target = temporal_table_column(
      table, target_timestamp_key(options),
      FirstClassAttributeRole::kTimestamp,
      {"timestamp", "time:timestamp"});
  if (!case_id || !activity || !source || !target) {
    throw std::invalid_argument(
        "temporal profile: selected dataframe column is missing");
  }
  return {*case_id, *activity, *source, *target};
}

const std::string* table_value(const EventTable::Row& row, std::size_t column) {
  if (column >= row.size() || !row[column] || row[column]->is_null) return nullptr;
  return &row[column]->value;
}

struct TemporalTableEvent {
  std::string case_id;
  std::string activity;
  std::string source_timestamp;
  std::string target_timestamp;
  double source_instant = 0.0;
  std::size_t row_index = 0;
};

EventLog temporal_table_to_log(const EventTable& table,
                               const TemporalProfileOptions& options) {
  const auto columns = temporal_table_columns(table, options);
  std::map<std::string, std::vector<TemporalTableEvent>> grouped;
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    const auto* case_id = table_value(row, columns.case_id);
    const auto* activity = table_value(row, columns.activity);
    const auto* source = table_value(row, columns.source_timestamp);
    const auto* target = table_value(row, columns.target_timestamp);
    if (case_id == nullptr || activity == nullptr || source == nullptr ||
        target == nullptr) {
      if (options.strict_attributes) {
        throw std::invalid_argument(
            "temporal profile: selected dataframe value is missing");
      }
      continue;
    }
    const auto source_instant = parse_timestamp(*source);
    const auto target_instant = parse_timestamp(*target);
    if (!source_instant || !target_instant) {
      if (options.strict_attributes) {
        throw std::invalid_argument(
            "temporal profile: selected dataframe timestamp is invalid");
      }
      continue;
    }
    grouped[*case_id].push_back({*case_id, *activity, *source, *target,
                                 *source_instant, row_index});
  }

  EventLog log;
  log.traces.reserve(grouped.size());
  for (auto& [case_id, events] : grouped) {
    std::stable_sort(events.begin(), events.end(),
                     [](const auto& left, const auto& right) {
                       return std::tie(left.source_instant, left.row_index) <
                              std::tie(right.source_instant, right.row_index);
                     });
    Trace trace;
    trace.case_id = case_id;
    trace.case_id_state = FirstClassAttributeState::kPresent;
    trace.events.reserve(events.size());
    for (const auto& item : events) {
      Event event;
      event.case_id = case_id;
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.activity = item.activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.timestamp = item.source_timestamp;
      event.timestamp_state = FirstClassAttributeState::kPresent;
      event.input_order = item.row_index;
      event.attributes.push_back(
          {"__minera_temporal_target", item.target_timestamp,
           AttributeKind::kDate});
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

TemporalProfileOptions normalized_table_options(
    const TemporalProfileOptions& options) {
  auto normalized = options;
  normalized.activity_attribute = "concept:name";
  normalized.case_id_attribute = "case:concept:name";
  normalized.source_timestamp_attribute.clear();
  normalized.source_completion_timestamp_attribute.clear();
  normalized.target_timestamp_attribute = "__minera_temporal_target";
  normalized.target_start_timestamp_attribute.clear();
  return normalized;
}

struct FrameProfileRow {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
  const std::string* activity = nullptr;
};

// pm4py's DataFrame arithmetic over cases given by sorted id, each case's rows
// in the frame's row order (stably sorted by timestamp here).
TemporalProfile frame_arithmetic_profile(
    std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>>& ordered,
    const std::function<bool()>& cancelled);

}  // namespace

const TemporalProfileEntry* TemporalProfile::find(const std::string& source,
                                                   const std::string& target) const noexcept {
  // Discovery emits sorted entries, but accepting a manually assembled
  // profile in any order makes the conformance API less surprising.
  for (const auto& entry : entries) {
    if (entry.source == source && entry.target == target) return &entry;
  }
  return nullptr;
}

TemporalProfile discover_temporal_profile_log(
    const EventLog& log, const TemporalProfileOptions& options) {
  return discover_temporal_profile(log, options, {});
}

TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options,
    const std::function<bool()>& cancelled) {
  if (options.activity_attribute.empty()) {
    throw std::invalid_argument(
        "temporal profile: activity attribute must not be empty");
  }
  validate_slots(options);
  const auto slots = options.use_business_hours ? normalized_slots(options)
                                                : std::vector<BusinessHourSlot>{};
  const auto& source_attribute = configured_attribute(
      options.source_timestamp_attribute,
      options.source_completion_timestamp_attribute);
  const auto& target_attribute = configured_attribute(
      options.target_timestamp_attribute,
      options.target_start_timestamp_attribute);

  // Every ordered event pair of a trace is visited, as the definition asks,
  // but each event's target-side values (instant, activity, local instant)
  // are read once and remembered: they depend on the event alone, and a
  // first read happens at the same point of the walk as before, so a strict
  // read throws where it did. Pairs aggregate under interned activity ids;
  // each (source, target) aggregate receives the same durations in the same
  // order, so its floating-point statistics are unchanged.
  std::unordered_map<std::string, std::uint32_t> activity_ids;
  std::vector<const std::string*> activity_names;
  const auto intern = [&](std::string activity) {
    const auto [it, added] = activity_ids.try_emplace(
        std::move(activity), static_cast<std::uint32_t>(activity_names.size()));
    if (added) activity_names.push_back(&it->first);
    return it->second;
  };
  // A pair's slot is 1 + its index in pair_aggregates (0: no duration yet),
  // kept in a dense table for the first kDenseActivities activities and in a
  // hash map beyond, so many distinct activities cannot blow the table up.
  constexpr std::uint32_t kDenseActivities = 2048;
  std::vector<std::vector<std::uint32_t>> dense_slot;
  std::unordered_map<std::uint64_t, std::uint32_t> sparse_slot;
  std::vector<std::pair<std::uint64_t, Aggregate>> pair_aggregates;
  const auto aggregate_for = [&](std::uint32_t source, std::uint32_t target) -> Aggregate& {
    const std::uint64_t key = (std::uint64_t{source} << 32) | target;
    std::uint32_t* slot = nullptr;
    if (source < kDenseActivities && target < kDenseActivities) {
      if (dense_slot.size() <= source) dense_slot.resize(source + 1);
      auto& row = dense_slot[source];
      if (row.size() <= target)
        row.resize(std::min<std::size_t>(kDenseActivities, std::max<std::size_t>(target + 1, activity_names.size())), 0);
      slot = &row[target];
    } else {
      slot = &sparse_slot[key];
    }
    if (*slot == 0) {
      pair_aggregates.push_back({key, Aggregate{}});
      *slot = static_cast<std::uint32_t>(pair_aggregates.size());
    }
    return pair_aggregates[*slot - 1].second;
  };
  struct TargetSide {
    bool instant_read = false, activity_read = false, local_read = false;
    std::optional<double> instant, local;
    std::optional<std::uint32_t> activity;
  };
  std::vector<TargetSide> targets;
  constexpr std::size_t kPairsPerCancellationPoll = std::size_t{1} << 20;
  std::size_t pairs_since_poll = 0;
  for (const auto& trace : log.traces) {
    targets.assign(trace.events.size(), TargetSide{});
    for (std::size_t i = 0; i + 1 < trace.events.size(); ++i) {
      pairs_since_poll += trace.events.size() - i;
      if (pairs_since_poll >= kPairsPerCancellationPoll) {
        pairs_since_poll = 0;
        throw_if_cancelled(cancelled, "temporal profile discovery was cancelled");
      }
      const auto source_time =
          event_instant(trace.events[i], source_attribute, "",
                        options.strict_attributes);
      if (!source_time) continue;
      const auto source_activity = event_activity(trace.events[i], options);
      if (!source_activity) continue;
      const auto source_local =
          options.use_business_hours
              ? event_local_instant(trace.events[i], source_attribute, "",
                                    options.strict_attributes)
              : source_time;
      if (!source_local) continue;
      const std::uint32_t source_id = intern(*source_activity);
      for (std::size_t j = i + 1; j < trace.events.size(); ++j) {
        auto& target = targets[j];
        if (!target.instant_read) {
          target.instant = event_instant(trace.events[j], target_attribute, "",
                                         options.strict_attributes);
          target.instant_read = true;
        }
        const auto& target_time = target.instant;
        if (!target_time || *target_time < *source_time) continue;
        if (!target.activity_read) {
          const auto target_activity = event_activity(trace.events[j], options);
          if (target_activity) target.activity = intern(*target_activity);
          target.activity_read = true;
        }
        if (!target.activity) continue;
        if (options.use_business_hours && !target.local_read) {
          target.local = event_local_instant(trace.events[j], target_attribute, "",
                                             options.strict_attributes);
          target.local_read = true;
        }
        const auto& target_local =
            options.use_business_hours ? target.local : target_time;
        if (!target_local) continue;
        const double duration = elapsed_between(
            *source_time, *target_time, *source_local, *target_local, options,
            options.use_business_hours ? &slots : nullptr);
        aggregate_for(source_id, *target.activity).add(duration);
      }
    }
  }
  std::map<std::pair<std::string, std::string>, Aggregate> aggregates;
  for (const auto& [key, aggregate] : pair_aggregates)
    aggregates.emplace(std::make_pair(*activity_names[key >> 32],
                                      *activity_names[key & 0xffffffffu]),
                       aggregate);

  TemporalProfile profile;
  profile.entries.reserve(aggregates.size());
  for (const auto& [key, aggregate] : aggregates) {
    TemporalProfileEntry entry;
    entry.source = key.first;
    entry.target = key.second;
    entry.count = aggregate.count();
    entry.mean_seconds = aggregate.mean();
    entry.standard_deviation_seconds = aggregate.stdev();
    profile.entries.push_back(std::move(entry));
  }
  return profile;
}

TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options) {
  return discover_temporal_profile_log(log, options);
}

TemporalProfile discover_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfileOptions& options) {
  const auto log = temporal_table_to_log(table, options);
  // pm4py's DataFrame discovery (one timestamp column, no business hours):
  // Timedelta durations, the compensated mean and Welford's deviation over
  // the cases by sorted id, each sorted by timestamp (round BI, observed
  // black-box; temporal_table_to_log already orders both).
  const auto columns = temporal_table_columns(table, options);
  if (!options.use_business_hours && columns.source_timestamp == columns.target_timestamp) {
    std::vector<std::vector<FrameProfileRow>> rows(log.traces.size());
    std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>> ordered;
    bool exact = true;
    for (std::size_t t = 0; t < log.traces.size() && exact; ++t) {
      for (const auto& event : log.traces[t].events) {
        const auto instant = parse_timestamp_exact(event.timestamp);
        if (!instant) {
          exact = false;
          break;
        }
        rows[t].push_back({instant->seconds, instant->nanoseconds, &event.activity});
      }
      ordered.push_back({&log.traces[t].case_id, &rows[t]});
    }
    if (exact) return frame_arithmetic_profile(ordered, {});
  }
  return discover_temporal_profile_log(log, normalized_table_options(options));
}

TemporalProfile discover_temporal_profile(
    const EventStream& stream, const TemporalProfileOptions& options) {
  return discover_temporal_profile_dataframe(event_stream_to_table(stream), options);
}

TemporalProfile discover_temporal_profile(
    const EventTable& table, const TemporalProfileOptions& options) {
  return discover_temporal_profile_dataframe(table, options);
}

TemporalProfile discover_temporal_profile(
    const LazyEventTable& table, const TemporalProfileOptions& options) {
  return discover_temporal_profile_dataframe(table.materialize(), options);
}

TemporalProfile discover_temporal_profile(
    const EventDataView& data, const TemporalProfileOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return discover_temporal_profile_log(*data.log(), options);
    case EventDataKind::kEventStream:
      return discover_temporal_profile(*data.stream(), options);
    case EventDataKind::kEventTable:
      return discover_temporal_profile_dataframe(*data.table(), options);
    case EventDataKind::kLazyEventTable:
      return discover_temporal_profile(*data.lazy_table(), options);
  }
  throw std::invalid_argument("temporal profile: unsupported event-data input");
}

TemporalConformanceResult conform_temporal_profile_log(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  if (options.activity_attribute.empty()) {
    throw std::invalid_argument(
        "temporal profile: activity attribute must not be empty");
  }
  const auto profile_lookup = build_profile_lookup(profile);
  validate_slots(options);
  const auto slots = options.use_business_hours ? normalized_slots(options)
                                                : std::vector<BusinessHourSlot>{};
  const auto& source_attribute = configured_attribute(
      options.source_timestamp_attribute,
      options.source_completion_timestamp_attribute);
  const auto& target_attribute = configured_attribute(
      options.target_timestamp_attribute,
      options.target_start_timestamp_attribute);

  TemporalConformanceResult result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    std::vector<TemporalDeviation> deviations;
    for (std::size_t i = 0; i + 1 < trace.events.size(); ++i) {
      const auto source_time =
          event_instant(trace.events[i], source_attribute, "",
                        options.strict_attributes);
      if (!source_time) continue;
      const auto source_activity = event_activity(trace.events[i], options);
      if (!source_activity) continue;
      const auto source_local =
          options.use_business_hours
              ? event_local_instant(trace.events[i], source_attribute, "",
                                    options.strict_attributes)
              : source_time;
      if (!source_local) continue;
      for (std::size_t j = i + 1; j < trace.events.size(); ++j) {
        const auto target_activity = event_activity(trace.events[j], options);
        if (!target_activity) continue;
        const auto entry_it = profile_lookup.find(
            {*source_activity, *target_activity});
        if (entry_it == profile_lookup.end()) continue;
        const auto* entry = entry_it->second;
        const auto target_time =
            event_instant(trace.events[j], target_attribute, "",
                          options.strict_attributes);
        if (!target_time || *target_time < *source_time) continue;
        const auto target_local =
            options.use_business_hours
                ? event_local_instant(trace.events[j], target_attribute, "",
                                      options.strict_attributes)
                : target_time;
        if (!target_local) continue;
        const double duration = elapsed_between(
            *source_time, *target_time, *source_local, *target_local, options,
            options.use_business_hours ? &slots : nullptr);
        const double difference = std::abs(duration - entry->mean_seconds);
        double distance = 0.0;
        bool sentinel = false;
        if (entry->standard_deviation_seconds == 0.0) {
          if (difference == 0.0) continue;
          // The reference reports the integer sys.maxsize.
          distance = static_cast<double>(
              std::numeric_limits<std::int64_t>::max());
          sentinel = true;
        } else {
          distance = difference / entry->standard_deviation_seconds;
        }
        if (!(distance > options.zeta)) continue;
        deviations.push_back({trace.case_id, *source_activity,
                              *target_activity, duration, distance, sentinel});
      }
    }
    result.push_back(std::move(deviations));
  }
  return result;
}

TemporalConformanceResult conform_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  const auto log = temporal_table_to_log(table, options);
  return conform_temporal_profile_log(log, profile,
                                      normalized_table_options(options));
}

TemporalConformanceResult conform_temporal_profile(
    const EventDataView& data, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return conform_temporal_profile_log(*data.log(), profile, options);
    case EventDataKind::kEventStream:
      return conform_temporal_profile_dataframe(
          event_stream_to_table(*data.stream()), profile, options);
    case EventDataKind::kEventTable:
      return conform_temporal_profile_dataframe(*data.table(), profile,
                                                options);
    case EventDataKind::kLazyEventTable:
      return conform_temporal_profile_dataframe(
          data.lazy_table()->materialize(), profile, options);
  }
  throw std::invalid_argument("temporal profile: unsupported event-data input");
}

std::vector<TemporalDeviation> check_temporal_profile_conformance(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  const auto grouped = conform_temporal_profile_log(log, profile, options);
  std::vector<TemporalDeviation> flattened;
  for (const auto& trace : grouped) {
    flattened.insert(flattened.end(), trace.begin(), trace.end());
  }
  return flattened;
}

namespace {

// discover_temporal_profile_frame on a built frame; nullopt when a row has no
// timestamp (pm4py raises there).
std::optional<TemporalProfile> frame_temporal_profile(const frame_times::Frame& frame,
                                                      const std::function<bool()>& cancelled) {
  using Row = FrameProfileRow;
  std::map<const Trace*, std::vector<Row>> cases;
  for (const auto& row : frame_rows(frame.log())) {
    const auto date = frame.date(row.trace, row.event, "time:timestamp");
    if (!date) return std::nullopt;
    cases[&row.trace].push_back({date->seconds, date->nanoseconds, &row.event.activity});
  }
  std::vector<std::pair<const std::string*, std::vector<Row>*>> ordered;
  for (auto& [trace, rows] : cases) ordered.push_back({&trace->case_id, &rows});
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const auto& left, const auto& right) { return *left.first < *right.first; });
  return frame_arithmetic_profile(ordered, cancelled);
}

TemporalProfile frame_arithmetic_profile(
    std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>>& ordered,
    const std::function<bool()>& cancelled) {
  using Row = FrameProfileRow;
  struct Sums {
    std::uint64_t count = 0;
    double sum = 0.0, compensation = 0.0;  // Kahan
    double mean = 0.0, squares = 0.0;      // Welford
  };
  std::map<std::pair<std::string, std::string>, Sums> pairs;
  std::uint64_t visited = 0;
  for (auto& [case_id, rows] : ordered) {
    (void)case_id;
    std::stable_sort(rows->begin(), rows->end(), [](const Row& left, const Row& right) {
      return std::tie(left.seconds, left.nanoseconds) < std::tie(right.seconds, right.nanoseconds);
    });
    for (std::size_t i = 0; i < rows->size(); ++i)
      for (std::size_t j = i + 1; j < rows->size(); ++j) {
        if ((++visited & 0xfffff) == 0) throw_if_cancelled(cancelled, "temporal profile discovery was cancelled");
        const auto& source = (*rows)[i];
        const auto& target = (*rows)[j];
        std::int64_t whole = target.seconds - source.seconds;
        std::int64_t fraction = static_cast<std::int64_t>(target.nanoseconds) - source.nanoseconds;
        if (fraction < 0) {
          fraction += 1000000000;
          --whole;
        }
        // Timedelta: days * 86400 + seconds (an int), + microseconds * 1e-6,
        // + nanoseconds * 1e-9, left to right.
        const double value = static_cast<double>(whole) + static_cast<double>(fraction / 1000) * 1e-6 +
                             static_cast<double>(fraction % 1000) * 1e-9;
        auto& sums = pairs[{*source.activity, *target.activity}];
        ++sums.count;
        const double y = value - sums.compensation;
        const double t = sums.sum + y;
        sums.compensation = t - sums.sum - y;
        sums.sum = t;
        const double previous = sums.mean;
        sums.mean += (value - previous) / static_cast<double>(sums.count);
        sums.squares += (value - sums.mean) * (value - previous);
      }
  }
  TemporalProfile profile;
  for (const auto& [key, sums] : pairs) {
    TemporalProfileEntry entry;
    entry.source = key.first;
    entry.target = key.second;
    entry.count = sums.count;
    entry.mean_seconds = sums.sum / static_cast<double>(sums.count);
    entry.standard_deviation_seconds =
        sums.count > 1 ? std::sqrt(sums.squares / static_cast<double>(sums.count - 1)) : 0.0;
    profile.entries.push_back(std::move(entry));
  }
  return profile;
}

}  // namespace

TemporalProfile discover_temporal_profile_frame(const EventLog& log,
                                                const std::function<bool()>& cancelled) {
  auto profile = frame_temporal_profile(frame_times::Frame(log), cancelled);
  if (!profile) throw std::invalid_argument("the timestamp column should not contain any empty value.");
  return std::move(*profile);
}

TemporalProfile discover_temporal_profile_as_uploaded(const EventLog& log,
                                                     const TemporalProfileOptions& options,
                                                     const std::function<bool()>& cancelled) {
  const auto canonical = [](const std::string& primary, const std::string& alias) {
    const auto& key = primary.empty() ? alias : primary;
    return key.empty() || key == "time:timestamp";
  };
  const bool defaults = options.activity_attribute == "concept:name" &&
                        canonical(options.source_timestamp_attribute, options.source_completion_timestamp_attribute) &&
                        canonical(options.target_timestamp_attribute, options.target_start_timestamp_attribute) &&
                        !options.use_business_hours;
  if (log.dataframe_columns && defaults)
    if (auto profile = frame_temporal_profile(frame_times::Frame(log), cancelled)) return std::move(*profile);
  return discover_temporal_profile(log, options, cancelled);
}

}  // namespace minera
