#include "minera/overlap.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct Interval {
  double start = 0.0;
  double end = 0.0;
};

std::string selected_key(const std::string& attribute,
                         const std::string& key,
                         const std::string& source) {
  if (!attribute.empty()) return attribute;
  if (!source.empty()) return source;
  return key;
}

std::string completion_key(const OverlapOptions& options) {
  if (!options.completion_timestamp_attribute.empty())
    return options.completion_timestamp_attribute;
  if (!options.completion_timestamp_source.empty())
    return options.completion_timestamp_source;
  if (!options.completion_timestamp_key.empty())
    return options.completion_timestamp_key;
  return selected_key(options.timestamp_attribute, options.timestamp_key,
                      options.timestamp_source);
}

const std::string* source_text(const Event& event, const std::string& key) {
  if (key.empty() || key == "time:timestamp" || key == "timestamp")
    return &event.timestamp;
  return event.attribute(key);
}

std::string activity_label(const Event& event, const OverlapOptions& options) {
  const std::string key = !options.activity_attribute.empty()
                              ? options.activity_attribute
                              : options.activity_key;
  if (key.empty() || key == "concept:name" || key == "activity")
    return event.activity;
  const auto* value = event.attribute(key);
  if (value == nullptr)
    throw std::invalid_argument("overlap: event is missing activity attribute");
  return *value;
}

double require_time(const Event& event, const std::string& key,
                    const char* role) {
  const auto* text = source_text(event, key);
  const auto parsed = text == nullptr ? std::optional<double>{}
                                      : parse_timestamp(*text);
  if (!parsed || !std::isfinite(*parsed)) {
    throw std::invalid_argument(std::string("overlap: invalid ") + role +
                                " timestamp");
  }
  return *parsed;
}

std::vector<Interval> intervals_of(const EventLog& log,
                                   const OverlapOptions& options) {
  const auto start_key = selected_key(options.start_timestamp_attribute,
                                      options.start_timestamp_key,
                                      options.start_timestamp_source);
  const auto end_key = completion_key(options);
  std::vector<Interval> intervals;
  intervals.reserve(log.event_count());
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      const double start = require_time(event, start_key, "start");
      const double end = require_time(event, end_key, "completion");
      if (start > end) {
        throw std::invalid_argument("overlap: start timestamp is after completion timestamp");
      }
      intervals.push_back({start, end});
    }
  }
  return intervals;
}

bool intersects(const Interval& left, const Interval& right,
                const OverlapOptions& options) {
  const double lo = std::max(left.start, right.start);
  const double hi = std::min(left.end, right.end);
  // Both intervals are expanded by epsilon before the interval-tree query,
  // so the tolerance is applied at both ends of the intersection.
  if (options.strict) return lo < hi;
  return lo <= hi + (2.0 * options.epsilon);
}

void validate_options(const OverlapOptions& options) {
  if (!std::isfinite(options.epsilon) || options.epsilon < 0.0)
    throw std::invalid_argument("overlap: epsilon must be finite and non-negative");
}

}  // namespace

IntervalEventOverlapCounts discover_interval_event_overlap_counts(
    const EventLog& log, const OverlapOptions& options) {
  validate_options(options);
  const auto intervals = intervals_of(log, options);
  IntervalEventOverlapCounts result(intervals.size(), 0);
  for (std::size_t i = 0; i < intervals.size(); ++i) {
    for (std::size_t j = 0; j < intervals.size(); ++j) {
      // Self-overlap is part of the canonical interval-tree statistic even
      // for a zero-duration interval in strict mode.
      if (i == j || intersects(intervals[i], intervals[j], options)) ++result[i];
    }
  }
  return result;
}

CaseOverlapCounts discover_case_overlap_counts(const EventLog& log,
                                               const OverlapOptions& options) {
  validate_options(options);
  const auto event_intervals = intervals_of(log, options);
  std::vector<Interval> envelopes;
  envelopes.reserve(log.traces.size());
  std::size_t offset = 0;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) {
      envelopes.push_back({});
      continue;
    }
    Interval envelope{event_intervals[offset].start, event_intervals[offset].end};
    for (std::size_t i = 1; i < trace.events.size(); ++i) {
      const auto& interval = event_intervals[offset + i];
      envelope.start = std::min(envelope.start, interval.start);
      envelope.end = std::max(envelope.end, interval.end);
    }
    envelopes.push_back(envelope);
    offset += trace.events.size();
  }

  CaseOverlapCounts result(envelopes.size(), 0);
  for (std::size_t i = 0; i < envelopes.size(); ++i) {
    if (log.traces[i].events.empty()) continue;
    for (std::size_t j = 0; j < envelopes.size(); ++j) {
      if (!log.traces[j].events.empty() &&
          (i == j || intersects(envelopes[i], envelopes[j], options)))
        ++result[i];
    }
  }
  return result;
}

ConcurrentActivityOverlapResult discover_concurrent_activity_overlap(
    const EventLog& log, const ConcurrentActivityOverlapOptions& options) {
  validate_options(options);
  const auto event_intervals = intervals_of(log, options);
  ConcurrentActivityOverlapResult result;
  std::size_t offset = 0;
  for (const auto& trace : log.traces) {
    struct ActivityInterval {
      Interval interval;
      std::string activity;
    };
    std::vector<ActivityInterval> sorted;
    sorted.reserve(trace.events.size());
    for (std::size_t i = 0; i < trace.events.size(); ++i) {
      sorted.push_back({event_intervals[offset + i],
                        activity_label(trace.events[i], options)});
    }
    // stable_sort deliberately retains input order for equal starts.
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto& left, const auto& right) {
                       return left.interval.start < right.interval.start;
                     });
    for (std::size_t i = 0; i < sorted.size(); ++i) {
      for (std::size_t j = i + 1; j < sorted.size(); ++j) {
        const double lo = std::max(sorted[i].interval.start,
                                  sorted[j].interval.start);
        const double hi = std::min(sorted[i].interval.end,
                                   sorted[j].interval.end);
        // Concurrent-activity overlap intentionally uses raw endpoint
        // comparisons; its strict option has no epsilon tolerance.
        if (options.strict ? !(lo < hi) : !(lo <= hi)) continue;
        const auto& first = sorted[i].activity;
        const auto& second = sorted[j].activity;
        const ConcurrentActivityOverlapKey key =
            first <= second ? ConcurrentActivityOverlapKey{first, second}
                             : ConcurrentActivityOverlapKey{second, first};
        ++result[key];
      }
    }
    offset += trace.events.size();
  }
  return result;
}

}  // namespace minera
