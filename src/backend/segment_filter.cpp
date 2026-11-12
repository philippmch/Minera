#include "minera/segment_filter.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "dataframe_cell.hpp"
#include "frame_cells.hpp"

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

// Whether the event's activity cell holds `activity`; a missing cell (absent,
// null or NaN) matches nothing, as in a DataFrame column.
bool activity_is(const Event& event, const std::string& activity) {
  return !dataframe_cell::activity_missing(event) && event.activity == activity;
}

bool activity_in(const Event& event,
                 const std::unordered_set<std::string>& activities) {
  return !dataframe_cell::activity_missing(event) &&
         activities.find(event.activity) != activities.end();
}

EventLog with_trace_attributes(const EventLog& source) {
  EventLog result;
  copy_event_log_context(source, result);
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
        if (activity_is(trace.events[i], options.activity)) {
          anchor_index = i;
          break;
        }
      }
    } else {
      for (std::size_t i = 0; i < trace.events.size(); ++i) {
        if (activity_is(trace.events[i], options.activity)) {
          anchor_index = i;
          break;
        }
      }
    }
    if (anchor_index == trace.events.size()) continue;
    const auto count = anchor_index + (options.strict ? 0U : 1U);
    // A cut with no rows leaves no case behind, as in pm4py's DataFrame
    // filter_prefixes (the reference for imported CSV/XES logs).
    if (count == 0) continue;
    Trace cut;
    copy_trace_context(trace, cut);
    cut.events.assign(trace.events.begin(), trace.events.begin() + count);
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
        if (activity_is(trace.events[i], options.activity)) {
          index = i;
          break;
        }
      }
    } else {
      for (std::size_t i = 0; i < trace.events.size(); ++i) {
        if (activity_is(trace.events[i], options.activity)) {
          index = i;
          break;
        }
      }
    }
    if (index == trace.events.size()) continue;
    // Strict leaves the anchor out, as pm4py's filter_suffixes does.
    const auto begin = index + (options.strict ? 1U : 0U);
    // A cut with no rows leaves no case behind, as in the DataFrame filter.
    if (begin >= trace.events.size()) continue;
    Trace cut;
    copy_trace_context(trace, cut);
    cut.events.assign(trace.events.begin() + begin, trace.events.end());
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

  // pm4py's DataFrame filter_between (the reference for imported CSV/XES
  // logs): each event is tested as a closer before it can open; an open
  // stretch closes at the next end activity and is labelled
  // <case><separator><k>, k counted from 0 and always appended. Only when the
  // start and end lists are equal does the closing event open the next
  // stretch, and it then belongs to that later stretch only. A stretch still
  // open at the end of the trace is discarded.
  const bool reopen = options.start_activities == options.end_activities;
  constexpr std::size_t kUnmarked = static_cast<std::size_t>(-1);
  // The stretch of each of `count` events (event_at(row)) or kUnmarked;
  // `subcases` receives the number of stretches.
  const auto stretches = [&](std::size_t count, const auto& event_at, std::size_t& subcases) {
    std::vector<std::size_t> marks(count, kUnmarked);
    std::optional<std::size_t> opening;
    std::size_t subcase = 0;
    for (std::size_t row = 0; row < count; ++row) {
      const Event& event = event_at(row);
      const bool closing = activity_in(event, ends);
      if (!opening) {
        if (activity_in(event, starts)) opening = row;
      } else if (closing) {
        for (std::size_t kept = *opening; kept <= row; ++kept)
          marks[kept] = subcase;
        ++subcase;
        opening = reopen ? std::optional<std::size_t>(row) : std::nullopt;
      }
    }
    subcases = subcase;
    return marks;
  };
  EventLog result = with_trace_attributes(event_log);
  // An XES upload whose cases' rows are not adjacent (trace names repeated
  // apart): pm4py's DataFrame filter marks each case's slice of the frame's
  // rows (its first row on, as many as it has) by sorted case id, a later
  // case's mark replacing an earlier one's on a shared row, and the marked
  // rows keep the frame's order (rounds BI and BJ, observed black-box).
  if (event_log.dataframe_columns && !event_log.dataframe_columns->csv_text) {
    const auto cases = frame_cases(event_log);
    if (!cases.adjacent) {
      std::vector<std::string> names;
      std::vector<std::size_t> label_of(cases.rows.size(), kUnmarked);
      for (const auto* trace : cases.traces) {
        const auto [first, count] = cases.extent.at(trace);
        std::size_t subcases = 0;
        const auto marks = stretches(
            count, [&](std::size_t row) -> const Event& { return cases.rows[first + row].event; }, subcases);
        const std::size_t base = names.size();
        for (std::size_t k = 0; k < subcases; ++k) names.push_back(trace->case_id + separator + std::to_string(k));
        for (std::size_t row = 0; row < count; ++row)
          if (marks[row] != kUnmarked) label_of[first + row] = base + marks[row];
      }
      std::vector<const std::string*> labels(cases.rows.size(), nullptr);
      for (std::size_t i = 0; i < labels.size(); ++i)
        if (label_of[i] != kUnmarked) labels[i] = &names[label_of[i]];
      result.traces = relabel_frame_rows(cases.rows, labels);
      return result;
    }
  }
  for (const auto& trace : event_log.traces) {
    std::size_t subcase = 0;
    const auto marks = stretches(
        trace.events.size(), [&](std::size_t row) -> const Event& { return trace.events[row]; }, subcase);
    std::vector<Trace> segments(subcase);
    for (std::size_t row = 0; row < marks.size(); ++row) {
      if (marks[row] != kUnmarked)
        segments[marks[row]].events.push_back(trace.events[row]);
    }
    for (std::size_t i = 0; i < segments.size(); ++i) {
      auto& segment = segments[i];
      copy_trace_context(trace, segment);
      set_trace_case_id(segment,
                        trace.case_id + separator + std::to_string(i));
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
