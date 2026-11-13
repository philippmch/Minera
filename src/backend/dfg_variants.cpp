#include "minera/dfg_variants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>

#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"

#include "business_hours.hpp"
#include "python_scalar.hpp"
#include "performance_arithmetic.hpp"

namespace minera {
namespace {

double required_timestamp(std::string_view text,
                          std::string_view description) {
  const auto parsed = parse_timestamp(text);
  if (!parsed) {
    throw std::invalid_argument("DFG discovery: invalid " +
                                std::string(description) + " timestamp");
  }
  return *parsed;
}

std::string_view start_timestamp(const Event& event,
                                 const DfgStatisticalOptions& options) {
  if (!options.start_timestamp_attribute.empty()) {
    if (const auto* value = dfg_event_field_value(
            event, options.start_timestamp_attribute)) {
      return *value;
    }
    throw std::invalid_argument(
        "DFG discovery: event is missing start timestamp attribute '" +
        options.start_timestamp_attribute + "'");
  }
  const auto selector = options.timestamp_attribute.empty()
                            ? std::string_view("time:timestamp")
                            : std::string_view(options.timestamp_attribute);
  if (const auto* value = dfg_event_field_value(event, selector))
    return *value;
  throw std::invalid_argument("DFG discovery: event is missing timestamp attribute '" +
                              options.timestamp_attribute + "'");
}

const std::string& activity_value(const Event& event,
                                  const DfgStatisticalOptions& options) {
  if (const auto* value =
          dfg_event_field_value(event, options.activity_attribute)) {
    return *value;
  }
  throw std::invalid_argument(
      "DFG discovery: event is missing activity attribute '" +
      options.activity_attribute + "'");
}

const std::string& timestamp_value(const Event& event,
                                   const DfgStatisticalOptions& options) {
  const auto selector = options.timestamp_attribute.empty()
                            ? std::string_view("time:timestamp")
                            : std::string_view(options.timestamp_attribute);
  if (const auto* value = dfg_event_field_value(event, selector)) {
    return *value;
  }
  throw std::invalid_argument(
      "DFG discovery: event is missing timestamp attribute '" +
      options.timestamp_attribute + "'");
}

DfgPerformanceSummary summarize(const std::vector<double>& values,
                                bool dataframe) {
  DfgPerformanceSummary result;
  result.count = static_cast<std::uint64_t>(values.size());
  if (values.empty()) return result;
  const auto aggregates = detail::aggregate_performance(values, dataframe);
  result.sum = aggregates.sum;
  result.mean = aggregates.mean;
  result.median = aggregates.median;
  result.minimum = aggregates.minimum;
  result.maximum = aggregates.maximum;
  result.standard_deviation = aggregates.stdev;
  return result;
}

DfgPerformanceValue selected_performance_value(
    const DfgPerformanceSummary& summary, const std::vector<double>& values,
    DfgPerformanceAggregation aggregation) {
  switch (aggregation) {
    case DfgPerformanceAggregation::kMedian:
      return summary.median;
    case DfgPerformanceAggregation::kMinimum:
      return summary.minimum;
    case DfgPerformanceAggregation::kMaximum:
      return summary.maximum;
    case DfgPerformanceAggregation::kStandardDeviation:
      return summary.standard_deviation;
    case DfgPerformanceAggregation::kSum:
      return summary.sum;
    case DfgPerformanceAggregation::kRawValues:
      return values;
    case DfgPerformanceAggregation::kAll:
      return summary;
    case DfgPerformanceAggregation::kMean:
    default:
      return summary.mean;
  }
}

struct TimedEventRef {
  const Event* event = nullptr;
  double timestamp = 0.0;
  std::size_t position = 0;
};

std::vector<TimedEventRef> sorted_strict_events(
    const Trace& trace, const DfgStatisticalOptions& options) {
  std::vector<TimedEventRef> events;
  events.reserve(trace.events.size());
  for (std::size_t i = 0; i < trace.events.size(); ++i) {
    events.push_back({&trace.events[i],
                      required_timestamp(timestamp_value(trace.events[i], options),
                                         "event"),
                      i});
  }
  std::stable_sort(events.begin(), events.end(), [](const auto& lhs,
                                                     const auto& rhs) {
    if (lhs.timestamp != rhs.timestamp) return lhs.timestamp < rhs.timestamp;
    if (lhs.event->input_order != rhs.event->input_order) {
      return lhs.event->input_order < rhs.event->input_order;
    }
    return lhs.position < rhs.position;
  });
  return events;
}

std::optional<std::string> case_attribute_value(const Trace& trace,
                                                const std::string& name) {
  if (name == "concept:name" || name == "case_id") return trace.case_id;
  if (const auto* value = trace.attribute(name)) return *value;
  return std::nullopt;
}

}  // namespace

const std::string* dfg_event_field_value(const Event& event,
                                         std::string_view selector) {
  const auto first_class = [](const std::string& value,
                              FirstClassAttributeState state)
      -> const std::string* {
    if (state == FirstClassAttributeState::kNull) return nullptr;
    if (state != FirstClassAttributeState::kMissing || !value.empty()) {
      return &value;
    }
    return static_cast<const std::string*>(nullptr);
  };
  const auto matches_source = [&](std::string_view source_key,
                                  std::string_view selected) {
    return !source_key.empty() && source_key == selected;
  };

  // An explicitly selected first-class source key has precedence over the
  // generic attribute list.  This matters when a flat adapter stores a
  // custom activity/timestamp key in the corresponding projection metadata.
  if (selector.empty() || selector == "concept:name" ||
      selector == "activity" ||
      matches_source(event.activity_source_key, selector)) {
    if (const auto* value = first_class(event.activity, event.activity_state))
      return value;
    if (selector.empty() || selector == "concept:name" ||
        selector == "activity" ||
        matches_source(event.activity_source_key, selector)) {
      if (event.activity_attribute &&
          !event.activity_attribute->is_null &&
          event.activity_attribute->first_class_state !=
              FirstClassAttributeState::kNull &&
          (selector.empty() || selector == event.activity_attribute->key ||
           matches_source(event.activity_source_key, selector))) {
        return &event.activity_attribute->value;
      }
    }
  }
  // Some adapters retain the selected activity cell only in its typed
  // projection metadata. Honor its own key/source spelling before consulting
  // potentially stale generic attributes.
  if (event.activity_attribute &&
      (event.activity_attribute->key == selector ||
       event.activity_attribute->first_class_source_key == selector)) {
    if (event.activity_attribute->is_null ||
        event.activity_attribute->first_class_state ==
            FirstClassAttributeState::kNull)
      return nullptr;
    return &event.activity_attribute->value;
  }

  if (selector == "time:timestamp" || selector == "timestamp" ||
      matches_source(event.timestamp_source_key, selector)) {
    if (const auto* value =
            first_class(event.timestamp, event.timestamp_state))
      return value;
    if (event.timestamp_attribute && !event.timestamp_attribute->is_null &&
        event.timestamp_attribute->first_class_state !=
            FirstClassAttributeState::kNull)
      return &event.timestamp_attribute->value;
  }
  if (event.timestamp_attribute &&
      (event.timestamp_attribute->key == selector ||
       event.timestamp_attribute->first_class_source_key == selector)) {
    if (event.timestamp_attribute->is_null ||
        event.timestamp_attribute->first_class_state ==
            FirstClassAttributeState::kNull)
      return nullptr;
    return &event.timestamp_attribute->value;
  }
  if (selector == "org:resource" || selector == "resource" ||
      matches_source(event.resource_source_key, selector)) {
    if (const auto* value = first_class(event.resource, event.resource_state))
      return value;
    if (event.resource_attribute && !event.resource_attribute->is_null &&
        event.resource_attribute->first_class_state !=
            FirstClassAttributeState::kNull)
      return &event.resource_attribute->value;
  }
  if (event.resource_attribute &&
      (event.resource_attribute->key == selector ||
       event.resource_attribute->first_class_source_key == selector)) {
    if (event.resource_attribute->is_null ||
        event.resource_attribute->first_class_state ==
            FirstClassAttributeState::kNull)
      return nullptr;
    return &event.resource_attribute->value;
  }
  if (selector == "lifecycle:transition" || selector == "lifecycle" ||
      matches_source(event.lifecycle_source_key, selector)) {
    if (const auto* value =
            first_class(event.lifecycle, event.lifecycle_state))
      return value;
    if (event.lifecycle_attribute && !event.lifecycle_attribute->is_null &&
        event.lifecycle_attribute->first_class_state !=
            FirstClassAttributeState::kNull)
      return &event.lifecycle_attribute->value;
  }
  if (event.lifecycle_attribute &&
      (event.lifecycle_attribute->key == selector ||
       event.lifecycle_attribute->first_class_source_key == selector)) {
    if (event.lifecycle_attribute->is_null ||
        event.lifecycle_attribute->first_class_state ==
            FirstClassAttributeState::kNull)
      return nullptr;
    return &event.lifecycle_attribute->value;
  }
  if (selector == "case:concept:name" || selector == "case_id" ||
      selector == "case:id" || selector == "case" ||
      matches_source(event.case_id_source_key, selector)) {
    if (const auto* value = first_class(event.case_id, event.case_id_state))
      return value;
  }

  for (const auto& attribute : event.attributes) {
    if (attribute.key == selector) {
      return attribute.is_null ? nullptr : &attribute.value;
    }
  }
  return nullptr;
}

namespace {

// pm4py's DFG variants keyed by a column of an upload's frame read that
// column's cells as Python objects (round BT, observed black-box): a number,
// a date or a missing cell names its activity str() of the cell ("7" for
// "007", "2.0", "2021-05-01 00:00:00+00:00", "nan", "NaT"). The frame-keyed
// log holds those labels under concept:name; nullopt when every cell is text
// (the log's own lookup reads the same) or the log has no frame.
std::optional<FrameKeyedActivities> dfg_frame_keyed(const EventLog& log,
                                                    const DfgStatisticalOptions& options) {
  auto keyed = frame_keyed_activities(log, options.activity_attribute);
  if (!keyed || !keyed->non_text) return std::nullopt;
  return keyed;
}

DfgStatisticalOptions dfg_concept_name(DfgStatisticalOptions options) {
  options.activity_attribute = "concept:name";
  return options;
}

}  // namespace

DfgFrequency discover_frequency_dfg(const EventLog& log,
                                    const DfgStatisticalOptions& options) {
  DfgFrequency result;
  for (const auto& trace : log.traces) {
    if (options.window >= trace.events.size() && options.window != 0) continue;
    if (options.keep_once_per_case) {
      std::set<DfgActivityPair> observed;
      for (std::size_t i = options.window; i < trace.events.size(); ++i) {
        observed.emplace(
            activity_value(trace.events[i - options.window], options),
            activity_value(trace.events[i], options));
      }
      for (const auto& pair : observed) ++result[pair];
    } else {
      for (std::size_t i = options.window; i < trace.events.size(); ++i) {
        ++result[{activity_value(trace.events[i - options.window], options),
                  activity_value(trace.events[i], options)}];
      }
    }
  }
  return result;
}

DfgTripleFrequency discover_frequency_triples(
    const EventLog& log, const DfgStatisticalOptions& options) {
  DfgTripleFrequency result;
  for (const auto& trace : log.traces) {
    for (std::size_t i = 2; i < trace.events.size(); ++i) {
      ++result[{activity_value(trace.events[i - 2], options),
                activity_value(trace.events[i - 1], options),
                activity_value(trace.events[i], options)}];
    }
  }
  return result;
}

DfgPerformance discover_performance_dfg(
    const EventLog& log, const DfgStatisticalOptions& options) {
  std::map<DfgActivityPair, std::vector<double>> observations;
  const bool dataframe = options.dataframe_arithmetic;
  const auto slots = options.use_business_hours
                         ? detail::normalize_business_hour_slots(
                               options.business_hour_slots,
                               options.business_hour_slots_explicit,
                               "DFG discovery")
                         : std::vector<BusinessHourSlot>{};
  for (const auto* trace_pointer :
       detail::traces_in_aggregation_order(log, dataframe)) {
    const auto& trace = *trace_pointer;
    for (std::size_t i = 1; i < trace.events.size(); ++i) {
      const auto& previous = trace.events[i - 1];
      const auto& current = trace.events[i];
      const auto previous_complete_text = timestamp_value(previous, options);
      const double previous_complete =
          required_timestamp(previous_complete_text, "completion");
      const auto current_start_text = start_timestamp(current, options);
      const double current_start =
          required_timestamp(current_start_text, "start");
      // Gaps come from the exact instants: epoch doubles would lose the
      // sub-second part (one microsecond in 2026 reads 9.5e-7).
      const auto gap = detail::exact_gap(previous_complete_text, current_start_text);
      double elapsed = gap ? detail::gap_seconds(*gap, dataframe)
                           : current_start - previous_complete;
      if (options.use_business_hours) {
        // The DataFrame path counts business time on the UTC clock (a CSV
        // offset is converted; an XES import already holds wall-clock UTC);
        // an EventLog uses each timestamp's own wall clock.
        const bool utc_clock = dataframe || options.business_hours_utc_clock;
        const auto previous_wall =
            utc_clock ? parse_timestamp_microseconds(previous_complete_text)
                      : detail::local_wall_timestamp_microseconds(
                            previous_complete_text);
        const auto current_wall =
            utc_clock ? parse_timestamp_microseconds(current_start_text)
                      : detail::local_wall_timestamp_microseconds(
                            current_start_text);
        if (!previous_wall || !current_wall) {
          throw std::invalid_argument(
              "DFG discovery: invalid business-hours timestamp");
        }
        elapsed = detail::business_elapsed_microseconds(*previous_wall,
                                                        *current_wall, slots);
      }
      observations[{activity_value(previous, options),
                    activity_value(current, options)}].push_back(
          std::max(0.0, elapsed));
    }
  }

  DfgPerformance result;
  for (auto& [pair, values] : observations) {
    const auto summary = summarize(values, dataframe);
    result.emplace(pair,
                   DfgPerformanceStatistics{
                       summary, values,
                       selected_performance_value(
                           summary, values, options.performance_aggregation)});
  }
  return result;
}

DfgCaseAttributeAnalysis discover_case_attribute_dfg(
    const EventLog& log, const DfgStatisticalOptions& options) {
  DfgCaseAttributeAnalysis result;
  const std::set<std::string> requested(options.case_attributes.begin(),
                                        options.case_attributes.end());
  for (const auto& trace : log.traces) {
    for (const auto& attribute : requested) {
      const auto value = case_attribute_value(trace, attribute);
      if (!value) continue;
      for (std::size_t i = 1; i < trace.events.size(); ++i) {
        ++result.edges[{activity_value(trace.events[i - 1], options),
                        activity_value(trace.events[i], options)}][attribute][*value];
      }
      if (options.return_node_attributes) {
        for (const auto& event : trace.events) {
          ++result.nodes[activity_value(event, options)][attribute][*value];
        }
      }
    }
  }
  return result;
}

DfgCleanAnalysis discover_clean_dfg(
    const EventLog& log, const DfgStatisticalOptions& options) {
  DfgCleanAnalysis result;
  for (const auto& trace : log.traces) {
    const auto events = sorted_strict_events(trace, options);
    if (events.empty()) continue;
    ++result.start_activities[activity_value(*events.front().event, options)];
    ++result.end_activities[activity_value(*events.back().event, options)];
    // Intentional CLEAN oddity: the first event never owns an edge and
    // the last event cannot own one. A length-three case keeps middle->last.
    for (std::size_t i = 1; i + 1 < events.size(); ++i) {
      ++result.graph[{activity_value(*events[i].event, options),
                      activity_value(*events[i + 1].event, options)}];
    }
  }
  return result;
}

DfgCleanAnalysis discover_clean_polars_dfg(
    const EventLog& log, const DfgStatisticalOptions& options) {
  // The lazy-frame form works on one globally sorted table. Reconstruct that
  // table from the native trace representation so case boundaries are handled
  // exactly once, including the first event of the next case.
  struct Row {
    std::string case_id;
    std::string activity;
    double timestamp = 0.0;
    std::size_t input_order = 0;
    std::size_t trace_position = 0;
  };

  std::vector<Row> rows;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    std::vector<TimedEventRef> events = sorted_strict_events(trace, options);
    for (const auto& item : events) {
      rows.push_back({trace.case_id,
                      activity_value(*item.event, options),
                      item.timestamp,
                      item.event->input_order,
                      trace_index});
    }
  }
  std::stable_sort(rows.begin(), rows.end(), [](const Row& lhs, const Row& rhs) {
    return std::tie(lhs.case_id, lhs.timestamp, lhs.input_order,
                    lhs.trace_position) <
           std::tie(rhs.case_id, rhs.timestamp, rhs.input_order,
                    rhs.trace_position);
  });

  DfgCleanAnalysis result;
  if (rows.empty()) return result;
  const auto first_activity = rows.front().activity;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto next = i + 1 < rows.size() ? &rows[i + 1] : nullptr;
    if (next != nullptr && rows[i].case_id == next->case_id) {
      ++result.graph[{rows[i].activity, next->activity}];
      continue;
    }

    ++result.end_activities[rows[i].activity];
    // A boundary row contributes the first activity of the following case.
    // The columnar implementation also compensates for a first activity that
    // matches the first row of the globally sorted table; retain that
    // observable edge behavior for parity.
    if (next != nullptr) {
      ++result.start_activities[next->activity];
      if (next->activity == first_activity)
        ++result.start_activities[next->activity];
    }
  }
  return result;
}

DfgCleanTime discover_clean_time_dfg(
    const EventLog& log, const DfgStatisticalOptions& options) {
  // pandas' groupby means of the frame's datetime64 and timedelta64 values
  // (round BT, observed black-box): each value a float64 of its int64 count
  // of units, a Kahan-compensated sum, the quotient truncated to a whole
  // unit. The per-case activity mean instant less the case's first instant
  // is that case's offset; the result is the mean of an activity's offsets.
  // The frame's unit is the microsecond unless a timestamp has digits below.
  struct Instant {
    const std::string* activity;
    std::int64_t seconds;
    std::uint32_t nanoseconds;
  };
  std::vector<std::vector<Instant>> cases;
  bool nanoseconds = false;
  for (const auto& trace : log.traces) {
    const auto events = sorted_strict_events(trace, options);
    if (events.empty()) continue;
    auto& rows = cases.emplace_back();
    for (const auto& item : events) {
      const auto exact = parse_timestamp_exact(timestamp_value(*item.event, options));
      if (!exact) throw std::invalid_argument("DFG discovery: invalid event timestamp");
      nanoseconds |= exact->nanoseconds % 1000 != 0;
      rows.push_back({&activity_value(*item.event, options), exact->seconds, exact->nanoseconds});
    }
  }
  // datetime64[ns] spans the years 1677-2262 only (outside them the frame
  // keeps microseconds).
  for (const auto& rows : cases)
    for (const auto& row : rows)
      if (row.seconds < -9'223'372'035 || row.seconds > 9'223'372'035) nanoseconds = false;
  const std::int64_t per_second = nanoseconds ? 1'000'000'000 : 1'000'000;
  // int64 counts of the unit.
  const auto ticks = [&](const Instant& instant) {
    return instant.seconds * per_second +
           static_cast<std::int64_t>(nanoseconds ? instant.nanoseconds : instant.nanoseconds / 1000);
  };
  struct KahanMean {
    double sum = 0.0;
    double compensation = 0.0;
    std::size_t count = 0;
    void add(double value) {
      const double y = value - compensation;
      const double t = sum + y;
      compensation = t - sum - y;
      if (compensation != compensation) compensation = 0.0;
      sum = t;
      ++count;
    }
    // astype(int64) of the float mean: truncation toward zero.
    [[nodiscard]] double truncated() const {
      return std::trunc(sum / static_cast<double>(count));
    }
  };
  std::map<std::string, KahanMean> offsets;
  for (const auto& rows : cases) {
    std::map<std::string, KahanMean> instants;
    for (const auto& row : rows)
      instants[*row.activity].add(static_cast<double>(ticks(row)));
    // The mean instant and the case's first are int64 datetimes; their
    // difference is exact before it becomes a float again.
    const auto start = ticks(rows.front());
    for (const auto& [activity, mean] : instants)
      offsets[activity].add(static_cast<double>(static_cast<std::int64_t>(mean.truncated()) - start));
  }
  DfgCleanTime result;
  for (const auto& [activity, mean] : offsets)
    result[activity] = mean.truncated() / static_cast<double>(per_second);
  return result;
}

namespace {

// pm4py's CLEAN variant on the frame keyed by a typed column (round BT,
// observed black-box): a missing cell is no row of the case, and the rows on
// either side of it are separate runs, each with its own start, end and
// boundary rule. An int64 column's next-row labels pass through a float
// column ("7.0").
DfgCleanAnalysis frame_keyed_clean_dfg(const FrameKeyedActivities& keyed,
                                       const DfgStatisticalOptions& options) {
  const bool float_targets = keyed.column == DataFrameColumnType::kInt64;
  const auto target_label = [&](const std::string& label) {
    if (!float_targets) return label;
    return python::float_repr(std::strtod(label.c_str(), nullptr));
  };
  DfgCleanAnalysis result;
  for (std::size_t t = 0; t < keyed.log.traces.size(); ++t) {
    const auto events = sorted_strict_events(keyed.log.traces[t], options);
    std::vector<const std::string*> run;
    const auto close_run = [&] {
      if (run.empty()) return;
      ++result.start_activities[*run.front()];
      ++result.end_activities[*run.back()];
      for (std::size_t i = 1; i + 1 < run.size(); ++i)
        ++result.graph[{*run[i], target_label(*run[i + 1])}];
      run.clear();
    };
    for (const auto& item : events) {
      if (keyed.missing[t][item.position]) {
        close_run();
        continue;
      }
      run.push_back(&item.event->activity);
    }
    close_run();
  }
  return result;
}

DfgStatisticalAnalysis frame_keyed_dfg_statistics(const FrameKeyedActivities& keyed,
                                                  const DfgStatisticalOptions& options) {
  const auto projected = dfg_concept_name(options);
  DfgStatisticalAnalysis result;
  result.variant = options.variant;
  switch (options.variant) {
    case DfgStatisticalVariant::kPerformance:
      // The DataFrame path (pm4py.discover_performance_dfg on the upload's
      // frame) checks the key column first: a typed column is no str
      // column, and a str or object column must have no gap.
      if (options.dataframe_arithmetic)
        if (const auto error = frame_activity_column_error(keyed); !error.empty())
          throw std::invalid_argument(error);
      result.performance_dfg = discover_performance_dfg(keyed.log, projected);
      return result;
    case DfgStatisticalVariant::kClean:
      result.clean = frame_keyed_clean_dfg(keyed, projected);
      result.frequency_dfg = result.clean.graph;
      return result;
    case DfgStatisticalVariant::kCleanTime:
      // The CLEAN_TIME variant selects the key and timestamp columns, so the
      // timestamp column as the key is a duplicate label; it fails on a key
      // column with a missing cell (the message names these dtypes whatever
      // the timestamps' unit).
      if (const auto& timestamp = options.timestamp_attribute.empty() ? std::string("time:timestamp")
                                                                       : options.timestamp_attribute;
          options.activity_attribute == timestamp)
        throw std::invalid_argument("ValueError: The column label '" + timestamp + "' is not unique.");
      if (keyed.any_missing)
        throw std::invalid_argument("TypeError: dtype datetime64[s] cannot be converted to timedelta64[ns]");
      result.clean_time = discover_clean_time_dfg(keyed.log, projected);
      return result;
    case DfgStatisticalVariant::kCleanPolars:
      // Not swept: the Polars frame keeps its own cells.
      result.clean = discover_clean_polars_dfg(keyed.log, projected);
      result.frequency_dfg = result.clean.graph;
      return result;
    default:
      return discover_dfg_statistics(keyed.log, projected);
  }
}

}  // namespace

DfgStatisticalAnalysis discover_dfg_statistics(
    const EventLog& log, const DfgStatisticalOptions& options) {
  // pm4py's EventLog variants keyed by a typed column of an upload's frame
  // read its cells as Python objects (round BT).
  if (const auto keyed = dfg_frame_keyed(log, options))
    return frame_keyed_dfg_statistics(*keyed, options);
  DfgStatisticalAnalysis result;
  result.variant = options.variant;
  if (options.variant == DfgStatisticalVariant::kNative) {
    result.frequency_dfg = discover_frequency_dfg(log, options);
  } else if (options.variant == DfgStatisticalVariant::kFrequencyTriples) {
    result.frequency_triples = discover_frequency_triples(log, options);
  } else if (options.variant == DfgStatisticalVariant::kPerformance) {
    result.performance_dfg = discover_performance_dfg(log, options);
  } else if (options.variant == DfgStatisticalVariant::kCaseAttributes) {
    result.case_attributes = discover_case_attribute_dfg(log, options);
  } else if (options.variant == DfgStatisticalVariant::kClean) {
    result.clean = discover_clean_dfg(log, options);
    result.frequency_dfg = result.clean.graph;
  } else if (options.variant == DfgStatisticalVariant::kCleanPolars) {
    result.clean = discover_clean_polars_dfg(log, options);
    result.frequency_dfg = result.clean.graph;
  } else if (options.variant == DfgStatisticalVariant::kCleanTime) {
    result.clean_time = discover_clean_time_dfg(log, options);
  }
  return result;
}

std::vector<std::string> dfg_variant_activities(const DfgVariantsAnalysis& analysis) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  const auto add = [&](const std::string& activity) {
    if (seen.insert(activity).second) out.push_back(activity);
  };
  const auto add_pair = [&](const DfgActivityPair& pair) {
    add(pair.first);
    add(pair.second);
  };
  for (const auto& named : analysis.statistical) {
    const auto& a = named.analysis;
    for (const auto& [pair, count] : a.frequency_dfg) add_pair(pair);
    for (const auto& [triple, count] : a.frequency_triples) {
      add(std::get<0>(triple));
      add(std::get<1>(triple));
      add(std::get<2>(triple));
    }
    for (const auto& [pair, statistics] : a.performance_dfg) add_pair(pair);
    for (const auto& [pair, attributes] : a.case_attributes.edges) add_pair(pair);
    for (const auto& [activity, attributes] : a.case_attributes.nodes) add(activity);
    for (const auto& [pair, count] : a.clean.graph) add_pair(pair);
    for (const auto& [activity, count] : a.clean.start_activities) add(activity);
    for (const auto& [activity, count] : a.clean.end_activities) add(activity);
    for (const auto& [activity, value] : a.clean_time) add(activity);
  }
  for (const auto& named : analysis.causal)
    for (const auto& [pair, value] : named.analysis.relations) add_pair(pair);
  return out;
}

DfgCausalAnalysis discover_dfg_causality(const DfgFrequency& dfg,
                                          DfgCausalVariant variant) {
  if (variant != DfgCausalVariant::kAlpha &&
      variant != DfgCausalVariant::kHeuristic) {
    throw std::invalid_argument("DFG causal variant is unsupported");
  }
  DfgCausalAnalysis result;
  result.variant = variant;
  if (variant == DfgCausalVariant::kAlpha) {
    for (const auto& [pair, frequency] : dfg) {
      if (frequency == 0) continue;
      const auto reverse = dfg.find({pair.second, pair.first});
      if (reverse == dfg.end() || reverse->second == 0) {
        result.relations[pair] = 1.0;
      }
    }
    return result;
  }

  const auto causal_strength = [](std::uint64_t frequency,
                                  std::uint64_t reverse_frequency) {
    // Keep the arithmetic in the integer domain until the final conversion.
    // In particular, converting two adjacent values near 2^64 to double
    // first would erase their one-count difference.
    const bool negative = frequency < reverse_frequency;
    const std::uint64_t difference =
        negative ? reverse_frequency - frequency
                 : frequency - reverse_frequency;

    // The denominator is frequency + reverse_frequency + 1. It can need 65
    // bits, so retain the carry separately instead of allowing uint64_t wrap.
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const bool sum_carry = frequency > maximum - reverse_frequency;
    const std::uint64_t sum_low = frequency + reverse_frequency;
    const std::uint64_t denominator_low = sum_low + 1;
    const bool denominator_carry = sum_carry || denominator_low == 0;
    const double denominator = denominator_carry
                                   ? std::ldexp(
                                         1.0 +
                                             std::ldexp(
                                                 static_cast<double>(
                                                     denominator_low),
                                                 -64),
                                         64)
                                   : static_cast<double>(denominator_low);
    const double magnitude = static_cast<double>(difference) / denominator;
    return negative ? -magnitude : magnitude;
  };

  for (const auto& [pair, frequency] : dfg) {
    if (result.relations.contains(pair)) continue;
    const auto reverse_pair = DfgActivityPair{pair.second, pair.first};
    const auto reverse = dfg.find(reverse_pair);
    const std::uint64_t reverse_frequency =
        reverse == dfg.end() ? 0 : reverse->second;
    const double strength = causal_strength(frequency, reverse_frequency);
    result.relations[pair] = strength;
    // For self-pairs this intentionally overwrites the same dictionary key,
    // matching Python assignment behavior (the value remains negative zero).
    result.relations[reverse_pair] = -strength;
  }
  return result;
}

}  // namespace minera
