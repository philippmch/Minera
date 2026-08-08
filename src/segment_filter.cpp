#include "minera/segment_filter.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

std::unordered_set<std::string> activity_set(
    const std::vector<std::string>& activities, const char* label) {
  std::unordered_set<std::string> result;
  result.reserve(activities.size());
  for (const auto& activity : activities) {
    if (activity.empty()) {
      throw std::invalid_argument(std::string(label) +
                                  ": activity names must not be empty");
    }
    result.insert(activity);
  }
  return result;
}

EventLog with_trace_attributes(const EventLog& source) {
  EventLog result;
  result.attributes = source.attributes;
  return result;
}

}  // namespace

EventLog filter_prefix(const EventLog& event_log,
                       const PrefixFilterOptions& options) {
  if (options.activity.empty()) {
    throw std::invalid_argument("prefix filter: an activity is required");
  }
  EventLog result = with_trace_attributes(event_log);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    std::size_t anchor_index = trace.events.size();
    if (options.occurrence == AnchorOccurrence::last) {
      for (std::size_t i = trace.events.size(); i-- > 0;) {
        if (trace.events[i].activity == options.activity) {
          anchor_index = i;
          break;
        }
      }
    } else {
      for (std::size_t i = 0; i < trace.events.size(); ++i) {
        if (trace.events[i].activity == options.activity) {
          anchor_index = i;
          break;
        }
      }
    }
    if (anchor_index == trace.events.size()) continue;
    const auto count = anchor_index + (options.strict ? 0U : 1U);
    Trace cut = trace;
    cut.events.assign(trace.events.begin(), trace.events.begin() + count);
    cut.case_id = trace.case_id;
    result.traces.push_back(std::move(cut));
  }
  return result;
}

EventLog filter_prefix(const EventLog& event_log, std::string_view activity,
                       bool strict) {
  return filter_prefix(event_log,
                       PrefixFilterOptions{std::string(activity), strict});
}

EventLog filter_suffix(const EventLog& event_log,
                       const SuffixFilterOptions& options) {
  if (options.activity.empty()) {
    throw std::invalid_argument("suffix filter: an activity is required");
  }
  EventLog result = with_trace_attributes(event_log);
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    std::size_t index = trace.events.size();
    if (options.occurrence == AnchorOccurrence::last) {
      for (std::size_t i = trace.events.size(); i-- > 0;) {
        if (trace.events[i].activity == options.activity) {
          index = i;
          break;
        }
      }
    } else {
      for (std::size_t i = 0; i < trace.events.size(); ++i) {
        if (trace.events[i].activity == options.activity) {
          index = i;
          break;
        }
      }
    }
    if (index == trace.events.size()) continue;
    const auto begin = index + (options.strict ? 0U : 1U);
    Trace cut = trace;
    cut.events.assign(trace.events.begin() + begin, trace.events.end());
    cut.case_id = trace.case_id;
    result.traces.push_back(std::move(cut));
  }
  return result;
}

EventLog filter_suffix(const EventLog& event_log, std::string_view activity,
                       bool strict) {
  return filter_suffix(event_log,
                       SuffixFilterOptions{std::string(activity), strict});
}

EventLog filter_between(const EventLog& event_log,
                        const BetweenFilterOptions& options) {
  const auto starts = activity_set(options.start_activities,
                                   "between filter start");
  const auto ends = activity_set(options.end_activities, "between filter end");
  if (starts.empty() || ends.empty()) {
    throw std::invalid_argument(
        "between filter: at least one start and end activity are required");
  }
  const std::string separator = options.subcase_separator.empty()
                                    ? options.case_id_separator
                                    : options.subcase_separator;
  if (separator.empty()) {
    throw std::invalid_argument("between filter: case id separator is empty");
  }

  EventLog result = with_trace_attributes(event_log);
  for (const auto& trace : event_log.traces) {
    std::vector<Trace> segments;
    Trace current;
    bool collecting = false;
    for (const auto& event : trace.events) {
      const bool is_start = starts.find(event.activity) != starts.end();
      const bool is_end = ends.find(event.activity) != ends.end();
      if (!collecting) {
        if (!is_start) continue;
        collecting = true;
        current = Trace{};
        current.events.push_back(event);
        // When the same activity is both a start and an end, the event opens
        // a segment; closure requires a subsequent occurrence.
        continue;
      }

      current.events.push_back(event);
      if (is_end) {
        segments.push_back(std::move(current));
        current = Trace{};
        collecting = false;
        if (is_start) {
          // Reuse a boundary event as the start of the next segment.  This is
          // important for repeated activities and overlapping endpoint sets.
          collecting = true;
          current.events.push_back(event);
        }
      }
    }
    for (std::size_t i = 0; i < segments.size(); ++i) {
      auto& segment = segments[i];
      segment.attributes = trace.attributes;
      segment.case_id = segments.size() == 1
                            ? trace.case_id
                            : trace.case_id + separator + std::to_string(i + 1);
      result.traces.push_back(std::move(segment));
    }
  }
  return result;
}

EventLog filter_between(const EventLog& event_log, std::string_view start_activity,
                        std::string_view end_activity,
                        std::string_view case_id_separator) {
  return filter_between(
      event_log,
      BetweenFilterOptions{{std::string(start_activity)},
                            {std::string(end_activity)},
                            std::string(case_id_separator), {}});
}

EventLog filter_segments(const EventLog& event_log,
                         const SegmentFilterOptions& options) {
  EventLog result = event_log;
  if (options.use_prefix) result = filter_prefix(result, options.prefix);
  if (options.use_suffix) result = filter_suffix(result, options.suffix);
  if (options.use_between) result = filter_between(result, options.between);
  return result;
}

}  // namespace minera
