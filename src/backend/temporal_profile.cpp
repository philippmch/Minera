#include "minera/temporal_profile.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <deque>
#include <initializer_list>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
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
  if (const auto* value = event.attribute(key)) return value;
  // The resource and lifecycle fields are first-class too: a log read from
  // a file keeps org:resource and lifecycle:transition there, not among the
  // attributes, and pm4py reads them as any other key (round BK).
  const auto first_class = [&](std::string_view canonical, const std::string& value,
                               FirstClassAttributeState state, std::string_view source_key,
                               const std::optional<EventAttribute>& attribute) -> const std::string* {
    if (key != canonical && !matches_source(source_key, key) && !(attribute && attribute->key == key))
      return nullptr;
    if (state == FirstClassAttributeState::kNull) return nullptr;
    if (const auto* selected = first_class_value(value, state)) return selected;
    if (attribute && !attribute->is_null) return &attribute->value;
    return nullptr;
  };
  if (const auto* value = first_class("org:resource", event.resource, event.resource_state,
                                      event.resource_source_key, event.resource_attribute))
    return value;
  return first_class("lifecycle:transition", event.lifecycle, event.lifecycle_state,
                     event.lifecycle_source_key, event.lifecycle_attribute);
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
  auto target = temporal_table_column(
      table, target_timestamp_key(options),
      FirstClassAttributeRole::kTimestamp,
      {"timestamp", "time:timestamp"});
  // pm4py's DataFrame algorithms take the completion column as the start
  // when the frame has no start column (an XES upload whose events all lack
  // it, say); a missing completion column raises (round BL, observed
  // black-box).
  if (!target) target = source;
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

using PairRanks = std::vector<std::vector<std::int64_t>>;

// The upload's timestamp column, which format_dataframe copies over
// "time:timestamp" (round BL, observed black-box): the column whose cells
// carry the timestamp role, else "time:timestamp" or "timestamp".
std::optional<std::size_t> upload_timestamp_column(const EventTable& table) {
  for (std::size_t index = 0; index < table.columns.size(); ++index)
    for (const auto& row : table.rows)
      if (index < row.size() && row[index] && row[index]->first_class_role == FirstClassAttributeRole::kTimestamp)
        return index;
  return temporal_table_column(table, "time:timestamp", FirstClassAttributeRole::kTimestamp,
                               {"timestamp", "time:timestamp"});
}

struct TemporalTableEvent {
  std::string case_id;
  std::string activity;
  // The completion (source) timestamp; none when the cell is missing (NaT).
  std::optional<std::string> source_timestamp;
  // The start (target) timestamp; none when the cell is missing (NaT).
  std::optional<std::string> target_timestamp;
  double source_instant = 0.0;
  double target_instant = 0.0;
  std::optional<ParsedExactTimestamp> source_exact, target_exact;
  std::size_t row_index = 0;
  // The row's "@@index" (or its frame position within the case).
  std::int64_t rank = 0;
  // The row's format_dataframe timestamp, on a CSV upload's rows.
  std::optional<ParsedExactTimestamp> frame_exact;
  double frame_instant = 0.0;
};

// The table's rows as a log for pm4py's DataFrame algorithms: cases by
// sorted id, each case's rows sorted by the target (start) timestamp, then
// the source (completion) one, then the row. A missing cell is NaN or NaT
// there and never raises (round BL, observed black-box): a row without an
// activity drops out of the groups, a row whose start is NaT sorts last and
// pairs only as a source (so never without an "@@index"), one whose
// completion is NaT sorts after equal starts and pairs only as a target. `first_rows`, when given, receives each
// case's first row in the table (rows that drop out included); `ranks`, when
// given, each kept row's "@@index" per case when the pairs follow it (left
// empty when the sorted position decides). The rows without a case form one
// more case, after the others (pm4py pairs rows by merging the frame with
// itself on the case column, where NaN meets NaN; round BL, observed
// black-box), except on a CSV upload's rows, where format_dataframe drops
// the rows without a case or an activity.
EventLog temporal_table_to_log(const EventTable& table,
                               const TemporalProfileOptions& options,
                               std::vector<std::size_t>* first_rows = nullptr,
                               PairRanks* ranks = nullptr) {
  const auto columns = temporal_table_columns(table, options);
  // pd.to_datetime reads an empty cell as NaT, as a missing one.
  const auto timestamp_value = [](const EventTable::Row& row, std::size_t column) {
    const auto* value = table_value(row, column);
    return value && value->empty() ? nullptr : value;
  };
  // The pair order: on a CSV upload's rows (file order), format_dataframe's:
  // the rows with a timestamp, each case's by that timestamp, then file
  // order (format_dataframe overwrites a CSV's own "@@index" column; round
  // BM, observed black-box); else the table's own "@@index" column.
  const auto index_column = std::find(table.columns.begin(), table.columns.end(), "@@index");
  const auto frame_timestamp = options.frame_index_rows ? upload_timestamp_column(table) : std::nullopt;
  const bool own_index = index_column != table.columns.end() && !frame_timestamp;
  bool ranked = own_index || frame_timestamp.has_value();
  const bool split = columns.source_timestamp != columns.target_timestamp;
  // (no case, id): the rows without a case last.
  using CaseKey = std::pair<bool, std::string>;
  std::map<CaseKey, std::vector<TemporalTableEvent>> grouped;
  std::map<CaseKey, std::size_t> first_row;
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    const auto* case_id = table_value(row, columns.case_id);
    if (case_id == nullptr && frame_timestamp) continue;
    const CaseKey key{case_id == nullptr, case_id ? *case_id : std::string()};
    const auto* activity = table_value(row, columns.activity);
    if (frame_timestamp && activity == nullptr) continue;
    std::int64_t rank = 0;
    std::optional<ParsedExactTimestamp> frame_exact;
    double frame_instant = 0.0;
    if (frame_timestamp) {
      // format_dataframe drops a row without a timestamp.
      const auto* value = timestamp_value(row, *frame_timestamp);
      const auto instant = value ? parse_timestamp(*value) : std::nullopt;
      if (!instant) continue;
      frame_instant = *instant;
      frame_exact = parse_timestamp_exact(*value);
    } else if (own_index) {
      const auto* cell = table_value(row, static_cast<std::size_t>(index_column - table.columns.begin()));
      const auto [end, error] = cell ? std::from_chars(cell->data(), cell->data() + cell->size(), rank)
                                     : std::from_chars_result{nullptr, std::errc::invalid_argument};
      if (!cell || error != std::errc{} || end != cell->data() + cell->size()) ranked = false;
    }
    first_row.emplace(key, row_index);
    grouped.try_emplace(key);
    const auto* source = timestamp_value(row, columns.source_timestamp);
    const auto* target = timestamp_value(row, columns.target_timestamp);
    if (activity == nullptr || (!split && source == nullptr)) continue;
    const auto target_instant = target ? parse_timestamp(*target) : std::nullopt;
    const auto source_instant = source ? parse_timestamp(*source) : std::nullopt;
    if ((target && !target_instant) || (source && !source_instant)) {
      if (options.strict_attributes) {
        throw std::invalid_argument(
            "temporal profile: selected dataframe timestamp is invalid");
      }
      continue;
    }
    grouped[key].push_back({key.second, *activity,
                                 source ? std::optional<std::string>(*source) : std::nullopt,
                                 target ? std::optional<std::string>(*target) : std::nullopt,
                                 source_instant.value_or(0.0), target_instant.value_or(0.0),
                                 source ? parse_timestamp_exact(*source) : std::nullopt,
                                 target ? parse_timestamp_exact(*target) : std::nullopt, row_index, rank,
                                 frame_exact, frame_instant});
  }
  if (frame_timestamp) {
    // Each case's frame positions (the cases' blocks keep their relative
    // order, so the position within a case serves as the "@@index").
    for (auto& [case_id, events] : grouped) {
      const bool exact = std::all_of(events.begin(), events.end(),
                                     [](const TemporalTableEvent& event) { return event.frame_exact.has_value(); });
      std::vector<std::size_t> order(events.size());
      std::iota(order.begin(), order.end(), std::size_t{0});
      std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        const auto& l = events[left];
        const auto& r = events[right];
        if (exact)
          return std::tie(l.frame_exact->seconds, l.frame_exact->nanoseconds, l.row_index) <
                 std::tie(r.frame_exact->seconds, r.frame_exact->nanoseconds, r.row_index);
        return std::tie(l.frame_instant, l.row_index) < std::tie(r.frame_instant, r.row_index);
      });
      for (std::size_t position = 0; position < order.size(); ++position)
        events[order[position]].rank = static_cast<std::int64_t>(position);
    }
    // The frame is sorted by case: its cases' first rows are in id order.
    std::size_t position = 0;
    for (auto& [case_id, first] : first_row) first = position++;
  }

  EventLog log;
  log.traces.reserve(grouped.size());
  for (auto& [key, events] : grouped) {
    const auto& case_id = key.second;
    const bool exact = std::all_of(events.begin(), events.end(), [](const TemporalTableEvent& event) {
      return (event.target_exact || !event.target_timestamp) && (event.source_exact || !event.source_timestamp);
    });
    // Ties keep the frame's row order (format_dataframe's on a CSV upload).
    const auto row_of = [&](const TemporalTableEvent& event) {
      return frame_timestamp ? static_cast<std::int64_t>(event.rank) : static_cast<std::int64_t>(event.row_index);
    };
    std::stable_sort(events.begin(), events.end(),
                     [&](const TemporalTableEvent& left, const TemporalTableEvent& right) {
                       // NaT sorts last (pandas' na_position).
                       const bool left_missing = !left.source_timestamp, right_missing = !right.source_timestamp;
                       const bool left_no_target = !left.target_timestamp, right_no_target = !right.target_timestamp;
                       if (exact) {
                         const auto exact_of = [](const std::optional<ParsedExactTimestamp>& value) {
                           return value ? std::pair(value->seconds, value->nanoseconds)
                                        : std::pair<std::int64_t, std::uint32_t>{0, 0};
                         };
                         return std::make_tuple(left_no_target, exact_of(left.target_exact), left_missing,
                                                exact_of(left.source_exact), row_of(left)) <
                                std::make_tuple(right_no_target, exact_of(right.target_exact), right_missing,
                                                exact_of(right.source_exact), row_of(right));
                       }
                       return std::make_tuple(left_no_target, left.target_instant, left_missing, left.source_instant, row_of(left)) <
                              std::make_tuple(right_no_target, right.target_instant, right_missing, right.source_instant, row_of(right));
                     });
    if (first_rows) first_rows->push_back(first_row.at(key));
    if (ranks && ranked) {
      ranks->emplace_back();
      for (const auto& item : events) ranks->back().push_back(item.rank);
    }
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
      if (item.source_timestamp) {
        event.timestamp = *item.source_timestamp;
        event.timestamp_state = FirstClassAttributeState::kPresent;
      } else {
        event.timestamp_state = FirstClassAttributeState::kMissing;
      }
      event.input_order = item.row_index;
      if (item.target_timestamp)
        event.attributes.push_back(
            {"__minera_temporal_target", *item.target_timestamp,
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
  // temporal_table_to_log keeps a row whose completion is NaT (no value
  // here), which pm4py's DataFrame algorithms skip as a source (round BL).
  normalized.strict_attributes = false;
  return normalized;
}

struct FrameProfileRow {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
  const std::string* activity = nullptr;
  // The target (start) timestamp when it is another column; else the
  // source's.
  std::optional<std::pair<std::int64_t, std::uint32_t>> target;
  // False when the completion (source) is NaT: the row sorts after equal
  // starts and pairs only as a target (round BL).
  bool has_source = true;
  // False when the start (target) is NaT: the row sorts last and pairs only
  // as a source (round BL; with a frame "@@index" it can still pair).
  bool has_target = true;
  // The row's "@@index" when the frame has one (a CSV upload): a row pairs
  // only with rows of a higher one; without it, with the later sorted rows.
  std::optional<std::int64_t> index;
};

// pm4py's DataFrame arithmetic over cases given by sorted id, each case's rows
// in the frame's row order (stably sorted by timestamp here).
std::pair<std::int64_t, std::uint32_t> target_of(const FrameProfileRow& row) {
  return row.target ? *row.target : std::pair<std::int64_t, std::uint32_t>{row.seconds, row.nanoseconds};
}

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

namespace {

// pm4py reads an event stream as its EventLog: cases in the order of their
// first events, each case's events in stream order; the EventLog algorithm
// (discovery and conformance) runs on that (round BK, observed black-box).
EventLog temporal_stream_log(const EventStream& stream, const TemporalProfileOptions& options) {
  LogConversionOptions conversion;
  if (!options.case_id_attribute.empty()) conversion.case_id_key = options.case_id_attribute;
  return event_data_to_event_log(EventDataView(stream), conversion);
}

}  // namespace

TemporalProfile discover_temporal_profile_log(
    const EventLog& log, const TemporalProfileOptions& options) {
  return discover_temporal_profile(log, options, {});
}

namespace {

// The EventLog algorithm; with `ranks` (per trace, per event) an event pairs
// with every event of a higher rank, before or after it, as pm4py's DataFrame
// algorithms do with a frame "@@index" (round BL); without, with the later
// events.
TemporalProfile discover_temporal_profile_ranked(
    const EventLog& log, const TemporalProfileOptions& options,
    const std::function<bool()>& cancelled, const PairRanks* ranks) {
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
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& trace = log.traces[t];
    const auto* rank = ranks && t < ranks->size() && (*ranks)[t].size() == trace.events.size()
                           ? &(*ranks)[t] : nullptr;
    targets.assign(trace.events.size(), TargetSide{});
    for (std::size_t i = 0; i + (rank ? 0 : 1) < trace.events.size(); ++i) {
      pairs_since_poll += trace.events.size() - (rank ? 0 : i);
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
      for (std::size_t j = rank ? 0 : i + 1; j < trace.events.size(); ++j) {
        if (rank && (*rank)[j] <= (*rank)[i]) continue;
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

}  // namespace

TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options,
    const std::function<bool()>& cancelled) {
  return discover_temporal_profile_ranked(log, options, cancelled, nullptr);
}

TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options) {
  return discover_temporal_profile_log(log, options);
}

TemporalProfile discover_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfileOptions& options) {
  PairRanks ranks;
  const auto log = temporal_table_to_log(table, options, nullptr, &ranks);
  // pm4py's DataFrame discovery (without business hours): Timedelta
  // durations, the compensated mean and Welford's deviation over the cases
  // by sorted id, each sorted by timestamp (round BI, observed black-box;
  // temporal_table_to_log already orders both); with a separate start
  // column, each row's completion against the later rows' starts (round BK).
  const auto columns = temporal_table_columns(table, options);
  if (!options.use_business_hours) {
    const bool split = columns.source_timestamp != columns.target_timestamp;
    std::vector<std::vector<FrameProfileRow>> rows(log.traces.size());
    std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>> ordered;
    bool exact = true;
    for (std::size_t t = 0; t < log.traces.size() && exact; ++t) {
      for (const auto& event : log.traces[t].events) {
        const bool has_source = event.timestamp_state != FirstClassAttributeState::kMissing;
        const auto instant = has_source ? parse_timestamp_exact(event.timestamp) : std::nullopt;
        const auto* target_text = split ? selected_event_value(event, "__minera_temporal_target", "timestamp") : nullptr;
        const auto target = target_text ? parse_timestamp_exact(*target_text) : std::nullopt;
        if ((has_source && !instant) || (split && target_text && !target)) {
          exact = false;
          break;
        }
        const auto e = rows[t].size();
        rows[t].push_back({instant ? instant->seconds : 0, instant ? instant->nanoseconds : 0u, &event.activity,
                           split ? std::optional(target ? std::pair<std::int64_t, std::uint32_t>{target->seconds, target->nanoseconds}
                                                        : std::pair<std::int64_t, std::uint32_t>{0, 0})
                                 : std::nullopt,
                           has_source, !split || target.has_value(), ranks.empty() ? std::nullopt : std::optional(ranks[t][e])});
      }
      ordered.push_back({&log.traces[t].case_id, &rows[t]});
    }
    if (exact) return frame_arithmetic_profile(ordered, {});
  }
  return discover_temporal_profile_ranked(log, normalized_table_options(options), {},
                                          ranks.empty() ? nullptr : &ranks);
}

TemporalProfile discover_temporal_profile(
    const EventStream& stream, const TemporalProfileOptions& options) {
  return discover_temporal_profile_log(temporal_stream_log(stream, options), options);
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

namespace {

// A pandas Timedelta between two exact instants in seconds, as pm4py's
// DataFrame code reads it: days * 86400 + seconds (an int), plus
// microseconds * 1e-6, plus nanoseconds * 1e-9, left to right.
double timedelta_seconds(const ParsedExactTimestamp& source, const ParsedExactTimestamp& target) {
  std::int64_t whole = target.seconds - source.seconds;
  std::int64_t fraction = static_cast<std::int64_t>(target.nanoseconds) - source.nanoseconds;
  if (fraction < 0) {
    fraction += 1000000000;
    --whole;
  }
  return static_cast<double>(whole) + static_cast<double>(fraction / 1000) * 1e-6 +
         static_cast<double>(fraction % 1000) * 1e-9;
}

// conform_temporal_profile_log; with `timedelta`, a duration without
// business hours is the pandas Timedelta between the two timestamps
// (pm4py's DataFrame conformance, round BJ, observed black-box) instead of
// the difference of their epoch seconds (its EventLog conformance).
// `ranks` as in discover_temporal_profile_ranked.
TemporalConformanceResult conform_log(const EventLog& log, const TemporalProfile& profile,
                                      const TemporalProfileOptions& options, bool timedelta,
                                      const PairRanks* ranks = nullptr) {
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
  const bool exact = timedelta && !options.use_business_hours;
  const auto exact_instant = [&](const Event& event, const std::string& attribute) {
    const auto* value = selected_event_value(event, attribute, "timestamp");
    return value == nullptr ? std::nullopt : parse_timestamp_exact(*value);
  };
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& trace = log.traces[t];
    const auto* rank = ranks && t < ranks->size() && (*ranks)[t].size() == trace.events.size()
                           ? &(*ranks)[t] : nullptr;
    std::vector<TemporalDeviation> deviations;
    std::vector<std::optional<ParsedExactTimestamp>> exact_sources, exact_targets;
    if (exact) {
      for (const auto& event : trace.events) {
        exact_sources.push_back(exact_instant(event, source_attribute));
        exact_targets.push_back(exact_instant(event, target_attribute));
      }
    }
    for (std::size_t i = 0; i + (rank ? 0 : 1) < trace.events.size(); ++i) {
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
      for (std::size_t j = rank ? 0 : i + 1; j < trace.events.size(); ++j) {
        if (rank && (*rank)[j] <= (*rank)[i]) continue;
        const auto target_activity = event_activity(trace.events[j], options);
        if (!target_activity) continue;
        const auto entry_it = profile_lookup.find(
            {*source_activity, *target_activity});
        if (entry_it == profile_lookup.end()) continue;
        const auto* entry = entry_it->second;
        const auto target_time =
            event_instant(trace.events[j], target_attribute, "",
                          options.strict_attributes);
        if (!target_time) continue;
        if (exact && exact_sources[i] && exact_targets[j]
                ? std::tie(exact_targets[j]->seconds, exact_targets[j]->nanoseconds) <
                      std::tie(exact_sources[i]->seconds, exact_sources[i]->nanoseconds)
                : *target_time < *source_time)
          continue;
        const auto target_local =
            options.use_business_hours
                ? event_local_instant(trace.events[j], target_attribute, "",
                                      options.strict_attributes)
                : target_time;
        if (!target_local) continue;
        const double duration =
            exact && exact_sources[i] && exact_targets[j]
                ? timedelta_seconds(*exact_sources[i], *exact_targets[j])
                : elapsed_between(*source_time, *target_time, *source_local, *target_local, options,
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

}  // namespace

TemporalConformanceResult conform_temporal_profile_log(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  return conform_log(log, profile, options, false);
}

TemporalConformanceResult conform_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  std::vector<std::size_t> case_first_rows;
  PairRanks ranks;
  auto log = temporal_table_to_log(table, options, &case_first_rows, &ranks);
  // pm4py's DataFrame conformance lists the cases in the order of their
  // first rows (an XES upload's frame in file order), not by case id, and
  // takes its durations as pandas Timedeltas (round BJ, observed black-box);
  // each case's rows stay sorted by time. A case is listed even when none of
  // its rows can pair, the rows without a case as one more (round BL).
  std::vector<std::pair<std::size_t, std::size_t>> first_rows;
  first_rows.reserve(log.traces.size());
  for (std::size_t index = 0; index < log.traces.size(); ++index)
    first_rows.emplace_back(case_first_rows[index], index);
  if (!std::is_sorted(first_rows.begin(), first_rows.end())) {
    std::sort(first_rows.begin(), first_rows.end());
    std::vector<Trace> ordered;
    PairRanks ordered_ranks;
    ordered.reserve(log.traces.size());
    for (const auto& [first, index] : first_rows) {
      ordered.push_back(std::move(log.traces[index]));
      if (!ranks.empty()) ordered_ranks.push_back(std::move(ranks[index]));
    }
    log.traces = std::move(ordered);
    ranks = std::move(ordered_ranks);
  }
  return conform_log(log, profile, normalized_table_options(options), true,
                     ranks.empty() ? nullptr : &ranks);
}

TemporalConformanceResult conform_temporal_profile(
    const EventDataView& data, const TemporalProfile& profile,
    const TemporalProfileOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return conform_temporal_profile_log(*data.log(), profile, options);
    case EventDataKind::kEventStream:
      return conform_temporal_profile_log(temporal_stream_log(*data.stream(), options), profile,
                                          options);
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

using FrameCases = std::map<const Trace*, std::vector<FrameProfileRow>>;

// A row's case group: its trace, or none for a row without a case.
const Trace* frame_case(const frame_times::Frame& frame, const FrameRow& row) {
  return frame.cell(row.trace, row.event, "case:concept:name") ? &row.trace : nullptr;
}

// The cases by id, then the rows without a case as one more: pm4py pairs
// rows by merging the frame with itself on the case column, where NaN meets
// NaN (round BL, observed black-box; as temporal_table_to_log).
std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>> frame_cases_ordered(FrameCases& cases) {
  static const std::string no_case;
  std::vector<std::pair<const std::string*, std::vector<FrameProfileRow>*>> ordered;
  std::vector<FrameProfileRow>* missing = nullptr;
  for (auto& [trace, rows] : cases) {
    if (trace == nullptr) missing = &rows;
    else ordered.push_back({&trace->case_id, &rows});
  }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const auto& left, const auto& right) { return *left.first < *right.first; });
  if (missing) ordered.push_back({&no_case, missing});
  return ordered;
}

// discover_temporal_profile_frame on a built frame; nullopt when a row has no
// timestamp (pm4py raises there). A row without an activity leaves the
// groups (round BL).
std::optional<TemporalProfile> frame_temporal_profile(const frame_times::Frame& frame,
                                                      const std::function<bool()>& cancelled) {
  FrameCases cases;
  for (const auto& row : frame_rows(frame.log())) {
    const auto date = frame.date(row.trace, row.event, "time:timestamp");
    if (!date) return std::nullopt;
    if (!frame.cell(row.trace, row.event, "concept:name")) continue;
    cases[frame_case(frame, row)].push_back({date->seconds, date->nanoseconds, &row.event.activity, std::nullopt});
  }
  auto ordered = frame_cases_ordered(cases);
  return frame_arithmetic_profile(ordered, cancelled);
}

// The same with other columns: the activity from `activity_key` (text or
// numbers or dates), each row's completion from `source_key` and start from
// `target_key` (round BK, observed black-box); missing cells as pandas reads
// them (round BL). nullopt when the paired activities do not sort or a
// timestamp column holds no dates (pm4py raises or reads the column
// otherwise there).
std::optional<TemporalProfile> frame_temporal_profile(const frame_times::Frame& frame,
                                                      const std::string& activity_key,
                                                      const std::string& source_key,
                                                      const std::string& target_key,
                                                      const std::function<bool()>& cancelled) {
  if (!frame.has(activity_key) || !frame.has(source_key)) return std::nullopt;
  // No start column: the completion column serves (round BL, as
  // temporal_table_columns).
  if (!frame.has(target_key))
    return frame_temporal_profile(frame, activity_key, source_key, source_key, cancelled);
  // One name per activity group: pandas groups values that are == (1, 1.0
  // and True alike), named by str() of the first (pm4py's key, round BL).
  std::map<std::string, std::string> activities;
  std::map<std::string, python::Scalar> labels;
  FrameCases cases;
  if (frame.dtype(source_key) != DataFrameColumnType::kDate || frame.dtype(target_key) != DataFrameColumnType::kDate)
    return std::nullopt;
  const bool indexed = frame.has("@@index");
  for (const auto& row : frame_rows(frame.log())) {
    const auto activity = frame.cell(row.trace, row.event, activity_key);
    if (activity && activity->kind == python::Scalar::Kind::kNone) return std::nullopt;
    const auto source = frame.date(row.trace, row.event, source_key);
    const auto target = source_key == target_key ? source : frame.date(row.trace, row.event, target_key);
    // A NaN activity leaves the groups; a NaT start pairs only as a source
    // and a NaT completion only as a target (round BL, as
    // temporal_table_to_log).
    if (!activity || (!source && source_key == target_key)) continue;
    auto [name, added] = activities.try_emplace(python::identity(*activity));
    if (added) {
      name->second = python::str(*activity);
      labels.try_emplace(name->second, *activity);
    }
    // A CSV upload's frame has format_dataframe's "@@index" (round BL).
    std::optional<std::int64_t> index;
    if (indexed)
      if (const auto cell = frame.cell(row.trace, row.event, "@@index"); cell && cell->kind == python::Scalar::Kind::kInt) {
        std::int64_t value = 0;
        const auto [end, error] = std::from_chars(cell->digits.data(), cell->digits.data() + cell->digits.size(), value);
        if (error == std::errc{} && end == cell->digits.data() + cell->digits.size())
          index = cell->negative ? -value : value;
      }
    cases[frame_case(frame, row)].push_back({source ? source->seconds : 0, source ? source->nanoseconds : 0u, &name->second,
                                 source_key == target_key
                                     ? std::nullopt
                                     : std::optional(target ? std::pair<std::int64_t, std::uint32_t>{target->seconds, target->nanoseconds}
                                                            : std::pair<std::int64_t, std::uint32_t>{0, 0}),
                                 source.has_value(), source_key == target_key || target.has_value(), index});
  }
  auto ordered = frame_cases_ordered(cases);
  auto profile = frame_arithmetic_profile(ordered, cancelled);
  // pandas groups the pairs by the source and the target activity, sorting
  // each level's values: a date beside a number there raises (round BL,
  // observed black-box), and the EventLog algorithm answers instead.
  std::vector<python::Scalar> sources, targets;
  std::set<std::string> seen_sources, seen_targets;
  for (const auto& entry : profile.entries) {
    if (seen_sources.insert(entry.source).second) sources.push_back(labels.at(entry.source));
    if (seen_targets.insert(entry.target).second) targets.push_back(labels.at(entry.target));
  }
  try {
    (void)python::safe_sort_order(sources);
    (void)python::safe_sort_order(targets);
  } catch (const python::TypeError&) {
    return std::nullopt;
  }
  return profile;
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
    // With a separate start column pm4py sorts a case's rows by the start
    // (target) timestamp, then the completion (source) timestamp, and pairs
    // each row with the later ones that start at or after its completion
    // (round BK, observed black-box).
    const auto order = [](const Row& row) {
      const auto target = target_of(row);
      return std::make_tuple(!row.has_target, target.first, target.second, !row.has_source, row.seconds, row.nanoseconds);
    };
    std::stable_sort(rows->begin(), rows->end(),
                     [&](const Row& left, const Row& right) { return order(left) < order(right); });
    // With a frame "@@index" (format_dataframe's, a CSV upload) pm4py pairs
    // a row with the rows of a higher index in this sorted order, before or
    // after it (round BL, observed black-box).
    const bool ranked = !rows->empty() && std::all_of(rows->begin(), rows->end(),
                                                      [](const Row& row) { return row.index.has_value(); });
    for (std::size_t i = 0; i < rows->size(); ++i)
      for (std::size_t j = ranked ? 0 : i + 1; j < rows->size(); ++j) {
        if ((++visited & 0xfffff) == 0) throw_if_cancelled(cancelled, "temporal profile discovery was cancelled");
        if (ranked && *(*rows)[j].index <= *(*rows)[i].index) continue;
        const auto& source = (*rows)[i];
        if (!source.has_source) break;
        if (!(*rows)[j].has_target) continue;
        const auto& [target_seconds, target_nanoseconds] = target_of((*rows)[j]);
        if (std::tie(target_seconds, target_nanoseconds) < std::tie(source.seconds, source.nanoseconds)) continue;
        std::int64_t whole = target_seconds - source.seconds;
        std::int64_t fraction = static_cast<std::int64_t>(target_nanoseconds) - source.nanoseconds;
        if (fraction < 0) {
          fraction += 1000000000;
          --whole;
        }
        // Timedelta: days * 86400 + seconds (an int), + microseconds * 1e-6,
        // + nanoseconds * 1e-9, left to right.
        const double value = static_cast<double>(whole) + static_cast<double>(fraction / 1000) * 1e-6 +
                             static_cast<double>(fraction % 1000) * 1e-9;
        auto& sums = pairs[{*source.activity, *(*rows)[j].activity}];
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
  // Other columns on an upload: pm4py's DataFrame discovery on them, unless
  // a cell is missing or of another type (round BK; BJ1 kept the EventLog
  // algorithm here).
  if (log.dataframe_columns && !defaults && !options.use_business_hours &&
      !options.activity_attribute.empty()) {
    const auto key = [](const std::string& primary, const std::string& alias) {
      const auto& configured = primary.empty() ? alias : primary;
      return configured.empty() ? std::string("time:timestamp") : configured;
    };
    if (auto profile = frame_temporal_profile(
            frame_times::Frame(log), options.activity_attribute,
            key(options.source_timestamp_attribute, options.source_completion_timestamp_attribute),
            key(options.target_timestamp_attribute, options.target_start_timestamp_attribute), cancelled))
      return std::move(*profile);
  }
  return discover_temporal_profile(log, options, cancelled);
}

}  // namespace minera
