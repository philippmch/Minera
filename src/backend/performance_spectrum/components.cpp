#include "components.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera::performance_spectrum_detail {
namespace {

struct TimedEvent {
  std::string case_id;
  std::string activity;
  double instant = 0.0;
  std::size_t source_order = 0;
  // The DataFrame and LazyFrame variants' instant of a table cell (round
  // BP, frame_instant), when it parses exactly; else instant * scale.
  std::optional<double> frame_instant;
};

// pandas' DataFrame spectrum reads a datetime column as its int64 units
// divided by 1e9 (numpy's true division of the int64 by an int): a
// microsecond column (pandas 3's default, an XES frame's too) gives seconds
// scaled down by 1e3, a nanosecond one (a CSV column with digits below the
// microsecond) the seconds themselves (round BP, observed black-box). A
// finer cell than the unit is truncated, as a datetime64 of that unit holds
// it.
std::int64_t pandas_units_per_second(const EventTable& table, std::size_t column) {
  if (column >= table.dtypes.size()) return 1000000;
  const std::string_view dtype = table.dtypes[column];
  if (dtype.starts_with("datetime64[ns")) return 1000000000;
  if (dtype.starts_with("datetime64[ms")) return 1000;
  if (dtype.starts_with("datetime64[s")) return 1;
  return 1000000;
}

// The cell's int64 units of `units_per_second` over `divisor`.
std::optional<double> datetime_units_instant(std::string_view text, std::int64_t units_per_second,
                                             double divisor) {
  const auto exact = parse_timestamp_exact(text);
  if (!exact) return std::nullopt;
  constexpr std::int64_t kBillion = 1000000000;
  if (exact->seconds > std::numeric_limits<std::int64_t>::max() / units_per_second - 1 ||
      exact->seconds < std::numeric_limits<std::int64_t>::min() / units_per_second + 1)
    return std::nullopt;
  const auto units = exact->seconds * units_per_second +
                     static_cast<std::int64_t>(exact->nanoseconds) / (kBillion / units_per_second);
  return static_cast<double>(units) / divisor;
}

// A table cell's instant as the component reads it: pandas' units over 1e9
// (above); Polars' epoch microseconds over 1e6, a nanosecond cell truncated
// (round BP, observed black-box); nullopt for the log components.
std::optional<double> frame_instant(std::string_view text, Component component,
                                    std::int64_t units_per_second) {
  switch (component) {
    case Component::kDataframe:
    case Component::kDataframeDisconnected:
      return datetime_units_instant(text, units_per_second, 1e9);
    case Component::kLazyframe:
    case Component::kLazyframeDisconnected:
      return datetime_units_instant(text, 1000000, 1e6);
    default:
      return std::nullopt;
  }
}

double scaled_instant(const TimedEvent& event, double scale) {
  if (event.frame_instant) return *event.frame_instant;
  return event.instant * scale;
}

using CaseEvents = std::vector<TimedEvent>;
using Cases = std::vector<CaseEvents>;

enum class SamplePolicy { kMidpointWithExtremes, kTable, kPrefix };

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

const std::string* event_attribute(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name" ||
      matches_source(event.activity_source_key, key)) {
    if (const auto* value =
            first_class_value(event.activity, event.activity_state))
      return value;
    if (event.activity_state != FirstClassAttributeState::kNull &&
        event.activity_attribute && !event.activity_attribute->is_null &&
        (key.empty() || key == event.activity_attribute->key ||
         matches_source(event.activity_source_key, key)))
      return &event.activity_attribute->value;
  }
  if (key == "timestamp" || key == "time:timestamp" ||
      matches_source(event.timestamp_source_key, key)) {
    if (const auto* value =
            first_class_value(event.timestamp, event.timestamp_state))
      return value;
    if (event.timestamp_state != FirstClassAttributeState::kNull &&
        event.timestamp_attribute && !event.timestamp_attribute->is_null &&
        (key == event.timestamp_attribute->key ||
         matches_source(event.timestamp_source_key, key)))
      return &event.timestamp_attribute->value;
  }
  if (key == "resource" || key == "org:resource" ||
      matches_source(event.resource_source_key, key)) {
    if (const auto* value =
            first_class_value(event.resource, event.resource_state))
      return value;
    if (event.resource_state != FirstClassAttributeState::kNull &&
        event.resource_attribute && !event.resource_attribute->is_null)
      return &event.resource_attribute->value;
  }
  if (key == "lifecycle" || key == "lifecycle:transition" ||
      matches_source(event.lifecycle_source_key, key)) {
    if (const auto* value =
            first_class_value(event.lifecycle, event.lifecycle_state))
      return value;
    if (event.lifecycle_state != FirstClassAttributeState::kNull &&
        event.lifecycle_attribute && !event.lifecycle_attribute->is_null)
      return &event.lifecycle_attribute->value;
  }
  return event.attribute(key);
}

const std::string* trace_attribute(const Trace& trace, std::string_view key) {
  if (key == "case_id" || key == "concept:name" ||
      key == "case:concept:name" || key == "case" ||
      matches_source(trace.case_id_source_key, key)) {
    if (const auto* value =
            first_class_value(trace.case_id, trace.case_id_state))
      return value;
    if (trace.case_id_state != FirstClassAttributeState::kNull &&
        trace.case_id_attribute && !trace.case_id_attribute->is_null)
      return &trace.case_id_attribute->value;
  }
  return trace.attribute(key);
}

const std::string* event_case_attribute(const Event& event,
                                        std::string_view key) {
  if (key == "case" || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" ||
      matches_source(event.case_id_source_key, key)) {
    if (const auto* value =
            first_class_value(event.case_id, event.case_id_state))
      return value;
  }
  return event.attribute(key);
}

bool is_requested(const std::set<std::string>& requested,
                  const std::string* activity) {
  return activity != nullptr && requested.contains(*activity);
}

double selected_timestamp(const Event& event, std::string_view selector) {
  const auto* text = event_attribute(event, selector);
  if (text == nullptr || text->empty()) {
    throw std::invalid_argument(
        "performance spectrum: selected event has a missing timestamp");
  }
  const auto parsed = parse_timestamp(*text);
  if (!parsed) {
    throw std::invalid_argument(
        "performance spectrum: selected event has an invalid timestamp");
  }
  return *parsed;
}

std::string selected_case(const Trace& trace, const Event& event,
                          std::string_view selector) {
  const bool canonical = selector.empty() || selector == "case" ||
                         selector == "case_id" || selector == "case:id" ||
                         selector == "case:concept:name" ||
                         selector == "concept:name";
  const auto* value = canonical ? trace_attribute(trace, selector)
                                : event_case_attribute(event, selector);
  if (value == nullptr && !canonical) value = trace_attribute(trace, selector);
  return value == nullptr ? std::string{} : *value;
}

void sort_case_events(CaseEvents& events) {
  std::stable_sort(events.begin(), events.end(),
                   [](const TimedEvent& left, const TimedEvent& right) {
                     return std::tie(left.instant, left.source_order) <
                            std::tie(right.instant, right.source_order);
                   });
}

Cases project_log(const EventLog& log, const std::set<std::string>& requested,
                  const PerformanceSpectrumOptions& options) {
  Cases cases;
  cases.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    CaseEvents events;
    events.reserve(trace.events.size());
    // input_order is the event's import position (0 for the first CSV row);
    // a trace whose events all carry 0 never recorded it, and then the
    // position in the trace stands in for it.
    const bool ordered = std::any_of(trace.events.begin(), trace.events.end(),
                                     [](const Event& event) { return event.input_order != 0; });
    for (std::size_t position = 0; position < trace.events.size(); ++position) {
      const auto& event = trace.events[position];
      const auto* activity = event_attribute(event, options.activity_attribute);
      if (!is_requested(requested, activity)) continue;
      const double instant =
          selected_timestamp(event, options.timestamp_attribute);
      const std::size_t source_order = ordered ? event.input_order : position;
      events.push_back(
          {selected_case(trace, event, options.case_id_attribute), *activity,
           instant, source_order});
    }
    if (options.sort_log) sort_case_events(events);
    if (!events.empty()) cases.push_back(std::move(events));
  }
  return cases;
}

std::optional<std::size_t> exact_table_column(const EventTable& table,
                                              std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::optional<std::size_t> role_table_column(
    const EventTable& table, FirstClassAttributeRole role) {
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

std::optional<std::size_t> resolve_table_column(const EventTable& table,
                                                std::string_view selector,
                                                FirstClassAttributeRole role) {
  if (const auto exact = exact_table_column(table, selector)) return exact;

  // Converted imported logs retain the original selected header as metadata
  // even when a transport uses a canonical column name.
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    for (const auto& row : table.rows) {
      if (index < row.size() && row[index] &&
          row[index]->first_class_role == role &&
          row[index]->first_class_source_key == selector) {
        return index;
      }
    }
  }

  const std::vector<std::string_view> aliases = [&] {
    switch (role) {
      case FirstClassAttributeRole::kCaseId:
        return std::vector<std::string_view>{"case_id", "case:concept:name",
                                             "concept:name"};
      case FirstClassAttributeRole::kActivity:
        return std::vector<std::string_view>{"activity", "concept:name"};
      case FirstClassAttributeRole::kTimestamp:
        return std::vector<std::string_view>{"timestamp", "time:timestamp"};
      case FirstClassAttributeRole::kResource:
        return std::vector<std::string_view>{"resource", "org:resource"};
      case FirstClassAttributeRole::kLifecycle:
        return std::vector<std::string_view>{"lifecycle",
                                             "lifecycle:transition"};
      case FirstClassAttributeRole::kNone:
        return std::vector<std::string_view>{};
    }
    return std::vector<std::string_view>{};
  }();
  if (std::find(aliases.begin(), aliases.end(), selector) == aliases.end()) {
    return std::nullopt;
  }
  for (const auto alias : aliases) {
    if (const auto found = exact_table_column(table, alias)) return found;
  }
  return role_table_column(table, role);
}

std::string table_cell_text(const EventTable::Row& row, std::size_t column) {
  if (column >= row.size() || !row[column] || row[column]->is_null) {
    return {};
  }
  const auto& cell = *row[column];
  if (!cell.value.empty()) return cell.value;
  if (const auto* date = std::get_if<NativeDate>(&cell.typed_value)) {
    return date->value;
  }
  return cell.value;
}

struct TableColumns {
  std::size_t case_id = 0;
  std::size_t activity = 0;
  std::size_t timestamp = 0;
};

TableColumns table_columns(const EventTable& table,
                           const PerformanceSpectrumOptions& options) {
  table.validate();
  const auto resolve = [&](std::string_view selector,
                           FirstClassAttributeRole role,
                           std::string_view description) {
    const auto found = resolve_table_column(table, selector, role);
    if (!found) {
      throw std::invalid_argument("performance spectrum: selected " +
                                  std::string(description) +
                                  " column is missing");
    }
    return *found;
  };
  return {resolve(options.case_id_attribute, FirstClassAttributeRole::kCaseId,
                   "case-id"),
          resolve(options.activity_attribute,
                  FirstClassAttributeRole::kActivity, "activity"),
          resolve(options.timestamp_attribute,
                  FirstClassAttributeRole::kTimestamp, "timestamp")};
}

Cases split_runs(std::vector<TimedEvent> events, bool sorted) {
  if (events.empty()) return {};
  if (sorted) {
    std::stable_sort(events.begin(), events.end(),
                     [](const TimedEvent& left, const TimedEvent& right) {
                       if (left.case_id != right.case_id) {
                         return left.case_id < right.case_id;
                       }
                       return std::tie(left.instant, left.source_order) <
                              std::tie(right.instant, right.source_order);
                     });
  }
  Cases cases;
  for (auto& event : events) {
    if (cases.empty() || cases.back().front().case_id != event.case_id) {
      cases.push_back({});
    }
    cases.back().push_back(std::move(event));
  }
  return cases;
}

Cases project_table(const EventTable& table,
                    const std::set<std::string>& requested,
                    const PerformanceSpectrumOptions& options, Component component) {
  const auto columns = table_columns(table, options);
  const auto units_per_second = pandas_units_per_second(table, columns.timestamp);
  std::vector<TimedEvent> events;
  events.reserve(table.rows.size());
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    const auto activity = table_cell_text(row, columns.activity);
    if (!requested.contains(activity)) continue;
    const auto timestamp = table_cell_text(row, columns.timestamp);
    if (timestamp.empty()) {
      throw std::invalid_argument(
          "performance spectrum: selected event has a missing timestamp");
    }
    const auto instant = parse_timestamp(timestamp);
    if (!instant) {
      throw std::invalid_argument(
          "performance spectrum: selected event has an invalid timestamp");
    }
    events.push_back({table_cell_text(row, columns.case_id), activity, *instant,
                      row_index, frame_instant(timestamp, component, units_per_second)});
  }
  return split_runs(std::move(events), options.sort_log);
}

Cases project_lazy_table(const LazyEventTable& table,
                         const std::set<std::string>& requested,
                         const PerformanceSpectrumOptions& options, Component component) {
  EventTable pending;
  pending.columns = table.columns();
  pending.validate();

  // A lazy schema carries column names but not the per-cell first-class role
  // metadata used by converted logs. Resolve immediately when names suffice;
  // otherwise buffer only until a scanned row exposes every selected role.
  std::optional<TableColumns> columns;
  try {
    columns = table_columns(pending, options);
  } catch (const std::invalid_argument&) {
    // Resolution is retried below as role-bearing rows arrive. If no row can
    // satisfy the selectors, the final call preserves the normal error.
  }

  std::vector<TimedEvent> events;
  if (table.row_count()) events.reserve(*table.row_count());
  std::size_t row_index = 0;

  const auto append_row = [&](const LazyEventTable::Row& row,
                              const TableColumns& selected,
                              std::size_t source_order) {
    const auto activity = table_cell_text(row, selected.activity);
    if (!requested.contains(activity)) return;
    const auto timestamp = table_cell_text(row, selected.timestamp);
    if (timestamp.empty()) {
      throw std::invalid_argument(
          "performance spectrum: selected event has a missing timestamp");
    }
    const auto instant = parse_timestamp(timestamp);
    if (!instant) {
      throw std::invalid_argument(
          "performance spectrum: selected event has an invalid timestamp");
    }
    events.push_back({table_cell_text(row, selected.case_id), activity,
                      *instant, source_order, frame_instant(timestamp, component, 1000000)});
  };

  table.scan([&](const LazyEventTable::Row& row) {
    if (columns) {
      append_row(row, *columns, row_index);
    } else {
      pending.rows.push_back(row);
      try {
        columns = table_columns(pending, options);
      } catch (const std::invalid_argument&) {
        ++row_index;
        return;
      }
      const auto first_pending_index = row_index + 1 - pending.rows.size();
      for (std::size_t index = 0; index < pending.rows.size(); ++index) {
        append_row(pending.rows[index], *columns, first_pending_index + index);
      }
      pending.rows.clear();
    }
    ++row_index;
  });
  if (!columns) {
    // Produce the same selected-column diagnostic as the eager table path.
    (void)table_columns(pending, options);
  }
  return split_runs(std::move(events), options.sort_log);
}

EventTable stream_table(const EventStream& stream) {
  EventTable table;
  table.attributes = stream.attributes;
  table.metadata = stream.metadata;
  for (const auto& record : stream.events) {
    for (const auto& field : record.fields) {
      if (field.key.empty()) {
        throw std::invalid_argument(
            "performance spectrum: event stream field has no name");
      }
      if (!exact_table_column(table, field.key)) {
        table.columns.push_back(field.key);
      }
    }
  }
  table.rows.reserve(stream.events.size());
  for (const auto& record : stream.events) {
    EventTable::Row row(table.columns.size());
    for (const auto& field : record.fields) {
      const auto column = exact_table_column(table, field.key);
      if (column) row[*column] = field;
    }
    table.rows.push_back(std::move(row));
  }
  table.validate();
  return table;
}

std::vector<SpectrumObservation> sample_midpoint(
    std::vector<SpectrumObservation> points, std::size_t sample_size) {
  if (points.size() <= sample_size) return points;
  const std::size_t n = points.size();
  std::vector<std::size_t> indexes;
  indexes.reserve(sample_size + 2);
  if (sample_size != 0) {
    for (std::size_t i = 0; i < sample_size; ++i) {
      indexes.push_back(i * n / sample_size + n / (2 * sample_size));
    }
  }
  indexes.insert(indexes.begin(), 0);
  indexes.push_back(n - 1);
  std::vector<SpectrumObservation> result;
  result.reserve(indexes.size());
  std::size_t previous = n;
  for (const auto index : indexes) {
    if (index == previous) continue;
    result.push_back(points[index]);
    previous = index;
  }
  return result;
}

std::vector<SpectrumObservation> sample_table(
    std::vector<SpectrumObservation> points, std::size_t sample_size) {
  if (points.size() <= sample_size) return points;
  if (sample_size == 0) return {};
  const std::size_t n = points.size();
  if (sample_size == 1) {
    return {points.front()};
  }

  // Keep the table adapter's bounded result stable across runs. The modular
  // rank gives interior rows a deterministic spread while retaining both
  // extrema, and avoids depending on allocator or hash iteration order.
  std::vector<std::size_t> candidates;
  candidates.reserve(n > 2 ? n - 2 : 0);
  for (std::size_t index = 1; index + 1 < n; ++index) {
    candidates.push_back(index);
  }
  std::stable_sort(candidates.begin(), candidates.end(), [n](std::size_t left,
                                                              std::size_t right) {
    const auto left_rank = (left * 2) % n;
    const auto right_rank = (right * 2) % n;
    return std::tie(left_rank, left) < std::tie(right_rank, right);
  });
  const std::size_t interior_count = std::min(sample_size - 2, candidates.size());
  std::vector<std::size_t> indexes{0};
  indexes.reserve(sample_size);
  for (std::size_t i = 0; i < interior_count; ++i) {
    indexes.push_back(candidates[i]);
  }
  indexes.push_back(n - 1);
  std::sort(indexes.begin(), indexes.end());
  indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

  std::vector<SpectrumObservation> result;
  result.reserve(indexes.size());
  for (const auto index : indexes) result.push_back(points[index]);
  return result;
}

std::vector<SpectrumObservation> sample_prefix(
    std::vector<SpectrumObservation> points, std::size_t sample_size) {
  if (points.size() > sample_size) points.resize(sample_size);
  return points;
}

PerformanceSpectrum make_spectrum(const std::vector<std::string>& activities,
                                  const PerformanceSpectrumOptions& options) {
  if (activities.size() < 2) {
    throw std::invalid_argument(
        "performance spectrum: the pattern needs at least two activities");
  }
  if (options.activity_attribute.empty() || options.timestamp_attribute.empty() ||
      options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "performance spectrum: activity, timestamp, and case-id attributes must not be empty");
  }
  PerformanceSpectrum spectrum;
  spectrum.activities = activities;
  spectrum.variant = options.variant;
  spectrum.activity_attribute = options.activity_attribute;
  spectrum.timestamp_attribute = options.timestamp_attribute;
  spectrum.case_id_attribute = options.case_id_attribute;
  return spectrum;
}

bool full_disconnected(Component component) {
  return component == Component::kDataframeDisconnected ||
         component == Component::kLazyframeDisconnected ||
         component == Component::kCompatibilityDisconnected;
}

bool pair_only_disconnected(Component component) {
  return component == Component::kLogDisconnected;
}

double instant_scale(Component component) {
  return component == Component::kDataframe ||
                 component == Component::kDataframeDisconnected
             ? 1.0 / 1000.0
             : 1.0;
}

SamplePolicy sample_policy(Component component) {
  if (component == Component::kDataframe) return SamplePolicy::kTable;
  if (component == Component::kLazyframe) return SamplePolicy::kPrefix;
  return SamplePolicy::kMidpointWithExtremes;
}

bool pattern_window_matches(const std::vector<std::string>& pattern,
                            const CaseEvents& events, std::size_t start,
                            std::size_t offset, std::size_t length,
                            bool concatenate) {
  if (concatenate) {
    std::string observed;
    std::string expected;
    for (std::size_t index = 0; index < length; ++index) {
      observed += events[start + index].activity;
      expected += pattern[offset + index];
    }
    return observed == expected;
  }
  for (std::size_t index = 0; index < length; ++index) {
    if (events[start + index].activity != pattern[offset + index]) {
      return false;
    }
  }
  return true;
}

std::size_t first_pattern_offset(const std::vector<std::string>& pattern,
                                 const CaseEvents& events,
                                 std::size_t start, std::size_t length,
                                 bool concatenate) {
  for (std::size_t offset = 0; offset + length <= pattern.size(); ++offset) {
    if (pattern_window_matches(pattern, events, start, offset, length,
                               concatenate)) {
      return offset;
    }
  }
  return pattern.size();
}

std::vector<SpectrumObservation> connected_points(
    const Cases& cases, const std::vector<std::string>& pattern,
    double scale, bool concatenate) {
  std::vector<SpectrumObservation> points;
  for (const auto& events : cases) {
    if (events.size() < pattern.size()) continue;
    for (std::size_t start = 0; start + pattern.size() <= events.size(); ++start) {
      if (!pattern_window_matches(pattern, events, start, 0, pattern.size(),
                                  concatenate)) {
        continue;
      }
      SpectrumObservation point;
      point.case_id = events[start].case_id;
      point.activities = pattern;
      point.instants.reserve(pattern.size());
      for (std::size_t offset = 0; offset < pattern.size(); ++offset) {
        point.instants.push_back(scaled_instant(events[start + offset], scale));
      }
      points.push_back(std::move(point));
    }
  }
  return points;
}

// The reference's LOG_DISCONNECTED collects each trace's matches as a Python
// set of (i, i + 1) position tuples and walks the set, so equally timed pairs
// leave in CPython's set iteration order: the tuple hash (xxHash-style lanes
// over the two small ints), open addressing with nine linear probes and
// perturbed jumps, and growth to four times the used slots.
std::uint64_t python_index_pair_hash(std::size_t first) {
  constexpr std::uint64_t kPrime1 = 11400714785074694791ULL;
  constexpr std::uint64_t kPrime2 = 14029467366897019727ULL;
  constexpr std::uint64_t kPrime5 = 2870177450012600261ULL;
  std::uint64_t acc = kPrime5;
  for (const std::uint64_t lane : {static_cast<std::uint64_t>(first),
                                   static_cast<std::uint64_t>(first) + 1}) {
    acc += lane * kPrime2;
    acc = std::rotl(acc, 31);
    acc *= kPrime1;
  }
  acc += 2 ^ (kPrime5 ^ 3527539ULL);
  return acc == UINT64_MAX ? 1546275796ULL : acc;
}

std::vector<std::size_t> python_index_pair_set_order(
    const std::vector<std::size_t>& starts) {
  struct Entry { std::size_t start; std::uint64_t hash; };
  const auto insert = [](const Entry& entry, std::vector<std::optional<Entry>>& table) {
    const std::size_t mask = table.size() - 1;
    std::size_t index = static_cast<std::size_t>(entry.hash) & mask;
    std::uint64_t perturb = entry.hash;
    for (;;) {
      const std::size_t probes = index + 9 <= mask ? 9 : 0;
      for (std::size_t offset = 0; offset <= probes; ++offset) {
        auto& slot = table[index + offset];
        if (!slot) {
          slot = entry;
          return;
        }
      }
      perturb >>= 5;
      index = (index * 5 + 1 + static_cast<std::size_t>(perturb)) & mask;
    }
  };
  std::vector<std::optional<Entry>> table(8);
  std::size_t used = 0;
  for (const auto start : starts) {
    insert({start, python_index_pair_hash(start)}, table);
    ++used;
    if (used * 5 >= (table.size() - 1) * 3) {
      const std::size_t minimum = used > 50000 ? used * 2 : used * 4;
      std::size_t size = 8;
      while (size <= minimum) size *= 2;
      auto old = std::move(table);
      table = std::vector<std::optional<Entry>>(size);
      for (const auto& slot : old)
        if (slot) insert(*slot, table);
    }
  }
  std::vector<std::size_t> ordered;
  ordered.reserve(used);
  for (const auto& slot : table)
    if (slot) ordered.push_back(slot->start);
  return ordered;
}

std::vector<SpectrumObservation> pair_disconnected_points(
    const Cases& cases, const std::vector<std::string>& pattern,
    double scale) {
  std::set<std::pair<std::string, std::string>> pairs;
  for (std::size_t i = 0; i + 1 < pattern.size(); ++i) {
    pairs.emplace(pattern[i], pattern[i + 1]);
  }

  std::vector<SpectrumObservation> points;
  for (const auto& events : cases) {
    std::vector<std::size_t> starts;
    for (std::size_t start = 0; start + 1 < events.size(); ++start) {
      if (pairs.contains({events[start].activity, events[start + 1].activity}))
        starts.push_back(start);
    }
    for (const auto start : python_index_pair_set_order(starts)) {
      SpectrumObservation point;
      point.case_id = events[start].case_id;
      point.activities = {events[start].activity, events[start + 1].activity};
      point.instants = {scaled_instant(events[start], scale),
                        scaled_instant(events[start + 1], scale)};
      for (std::size_t offset = 0; offset + 1 < pattern.size(); ++offset) {
        if (pattern[offset] == point.activities[0] &&
            pattern[offset + 1] == point.activities[1]) {
          point.pattern_offset = offset;
          break;
        }
      }
      points.push_back(std::move(point));
    }
  }
  return points;
}

std::vector<SpectrumObservation> full_disconnected_points(
    const Cases& cases, const std::vector<std::string>& pattern,
    double scale, bool concatenate) {
  std::vector<SpectrumObservation> points;
  for (const auto& source_events : cases) {
    if (source_events.size() < 2) continue;
    CaseEvents events = source_events;
    std::vector<bool> removed(events.size(), false);
    for (std::size_t length = pattern.size(); length >= 2; --length) {
      std::vector<std::size_t> active;
      active.reserve(events.size());
      for (std::size_t index = 0; index < events.size(); ++index) {
        if (!removed[index]) active.push_back(index);
      }
      struct Match {
        std::size_t start = 0;
        std::size_t offset = 0;
      };
      std::vector<Match> matches;
      for (std::size_t start = 0; start + length <= active.size(); ++start) {
        CaseEvents projected;
        projected.reserve(length);
        for (std::size_t index = 0; index < length; ++index) {
          projected.push_back(events[active[start + index]]);
        }
        const auto offset =
            first_pattern_offset(pattern, projected, 0, length, concatenate);
        if (offset != pattern.size()) matches.push_back({start, offset});
      }
      for (const auto match : matches) {
        SpectrumObservation point;
        point.case_id = events[active[match.start]].case_id;
        point.pattern_offset = match.offset;
        point.activities.assign(pattern.begin() + match.offset,
                                pattern.begin() + match.offset + length);
        point.instants.reserve(length);
        for (std::size_t index = 0; index < length; ++index) {
          point.instants.push_back(
              scaled_instant(events[active[match.start + index]], scale));
        }
        points.push_back(std::move(point));
      }
      for (const auto match : matches) {
        for (std::size_t index = 0; index + 1 < length; ++index) {
          removed[active[match.start + index]] = true;
        }
      }
      if (length == 2) break;
    }
  }
  return points;
}

// Stable by first instant. The reference DATAFRAME_DISCONNECTED gathers every
// case's longest subpatterns before the shorter ones, so with
// longer_first_on_ties an equally early longer observation leaves first.
void stable_sort_points(std::vector<SpectrumObservation>& points,
                        bool longer_first_on_ties = false) {
  std::stable_sort(points.begin(), points.end(),
                   [longer_first_on_ties](const SpectrumObservation& left,
                                          const SpectrumObservation& right) {
                     const double left_instant =
                         left.instants.empty()
                             ? std::numeric_limits<double>::infinity()
                             : left.instants.front();
                     const double right_instant =
                         right.instants.empty()
                             ? std::numeric_limits<double>::infinity()
                             : right.instants.front();
                     if (longer_first_on_ties && left_instant == right_instant)
                       return left.instants.size() > right.instants.size();
                     return left_instant < right_instant;
                   });
}

// The reference LOG variants sort the log first: events by time within each
// trace, and the traces by their first (pattern) event, ties in log order.
void sort_cases_by_first_instant(Cases& cases) {
  std::stable_sort(cases.begin(), cases.end(),
                   [](const CaseEvents& left, const CaseEvents& right) {
                     constexpr double kNone = std::numeric_limits<double>::infinity();
                     const double l = left.empty() ? kNone : left.front().instant;
                     const double r = right.empty() ? kNone : right.front().instant;
                     return l < r;
                   });
}

PerformanceSpectrum run_cases(
    Cases cases, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component) {
  auto spectrum = make_spectrum(activities, options);
  std::vector<SpectrumObservation> points;
  const double scale = instant_scale(component);
  const bool concatenate = component == Component::kDataframe ||
                           component == Component::kDataframeDisconnected ||
                           component == Component::kLazyframe ||
                           component == Component::kLazyframeDisconnected;
  if (options.sort_log && (component == Component::kLog ||
                           component == Component::kLogDisconnected))
    sort_cases_by_first_instant(cases);
  if (component == Component::kDataframeDisconnected) {
    std::size_t rows = 0;
    for (const auto& events : cases) rows += events.size();
    // The reference builds one shifted column per pattern step from the
    // pattern rows; with fewer rows than steps pandas refuses the frame.
    if (rows + 1 < activities.size())
      throw std::invalid_argument(
          "performance spectrum: all arrays must be same length (fewer "
          "pattern events than pattern steps)");
  }
  if (pair_only_disconnected(component)) {
    points = pair_disconnected_points(cases, activities, scale);
  } else if (full_disconnected(component)) {
    points = full_disconnected_points(cases, activities, scale, concatenate);
  } else {
    points = connected_points(cases, activities, scale, concatenate);
  }
  stable_sort_points(points, component == Component::kDataframeDisconnected);
  switch (sample_policy(component)) {
    case SamplePolicy::kMidpointWithExtremes:
      spectrum.observations =
          sample_midpoint(std::move(points), options.sample_size);
      break;
    case SamplePolicy::kTable:
      spectrum.observations =
          sample_table(std::move(points), options.sample_size);
      break;
    case SamplePolicy::kPrefix:
      spectrum.observations =
          sample_prefix(std::move(points), options.sample_size);
      break;
  }
  return spectrum;
}

}  // namespace

PerformanceSpectrum run_component(
    const EventLog& log, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component) {
  std::set<std::string> requested(activities.begin(), activities.end());
  auto cases = project_log(log, requested, options);
  return run_cases(std::move(cases), std::move(activities), std::move(options),
                   component);
}

PerformanceSpectrum run_component(
    const EventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component) {
  std::set<std::string> requested(activities.begin(), activities.end());
  auto cases = project_table(table, requested, options, component);
  return run_cases(std::move(cases), std::move(activities), std::move(options),
                   component);
}

PerformanceSpectrum run_component(
    const EventStream& stream, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component) {
  auto table = stream_table(stream);
  return run_component(table, std::move(activities), std::move(options),
                       component);
}

PerformanceSpectrum run_component(
    const LazyEventTable& table, std::vector<std::string> activities,
    PerformanceSpectrumOptions options, Component component) {
  std::set<std::string> requested(activities.begin(), activities.end());
  auto cases = project_lazy_table(table, requested, options, component);
  return run_cases(std::move(cases), std::move(activities), std::move(options),
                   component);
}

}  // namespace minera::performance_spectrum_detail
