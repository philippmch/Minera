#include "minera/dfg_variants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>

#include "minera/timestamp.hpp"

#include "business_hours.hpp"
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
  std::map<std::string, std::vector<double>> case_offsets;
  for (const auto& trace : log.traces) {
    const auto events = sorted_strict_events(trace, options);
    if (events.empty()) continue;
    std::map<std::string, std::vector<double>> activity_instants;
    for (const auto& item : events) {
      activity_instants[activity_value(*item.event, options)].push_back(
          item.timestamp);
    }
    const double case_start = events.front().timestamp;
    for (const auto& [activity, instants] : activity_instants) {
      const double mean =
          std::accumulate(instants.begin(), instants.end(), 0.0) /
          static_cast<double>(instants.size());
      case_offsets[activity].push_back(mean - case_start);
    }
  }
  DfgCleanTime result;
  for (const auto& [activity, offsets] : case_offsets) {
    result[activity] =
        std::accumulate(offsets.begin(), offsets.end(), 0.0) /
        static_cast<double>(offsets.size());
  }
  return result;
}

DfgStatisticalAnalysis discover_dfg_statistics(
    const EventLog& log, const DfgStatisticalOptions& options) {
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
