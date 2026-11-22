#include "minera/eventually_follows.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/dataframe_columns.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::vector<EventuallyFollowsEdge> edges_of(const std::vector<std::uint64_t>& counts,
                                            std::size_t n) {
  std::vector<EventuallyFollowsEdge> edges;
  for (std::size_t source = 0; source < n; ++source) {
    for (std::size_t target = 0; target < n; ++target) {
      if (const auto count = counts[source * n + target]) {
        edges.push_back({source, target, count});
      }
    }
  }
  return edges;
}

// An instant as (UTC seconds, nanoseconds). A CSV upload's cells keep their
// nanoseconds, as the Timestamps of pm4py's EventLog of the frame do; other
// logs hold Python datetimes (microseconds).
using Instant = std::pair<std::int64_t, std::int64_t>;

Instant required_time(const Event& event, const std::string& key,
                      const char* role, bool exact) {
  const std::string* text = nullptr;
  if (key.empty() || key == "timestamp" || key == "time:timestamp") {
    text = &event.timestamp;
  } else {
    text = event.attribute(key);
    // The timestamp column under its own header (a CSV upload's) is an
    // attribute of the frame's events too (round FT, observed black-box).
    if (text == nullptr && !event.timestamp_source_key.empty() &&
        key == event.timestamp_source_key)
      text = &event.timestamp;
  }
  if (text == nullptr || text->empty()) {
    throw std::invalid_argument(std::string("eventually follows: event is missing ") +
                                role + " timestamp attribute '" + key + "'");
  }
  if (exact) {
    if (const auto parsed = parse_timestamp_exact(*text))
      return {parsed->seconds, static_cast<std::int64_t>(parsed->nanoseconds)};
  }
  const auto parsed = parse_timestamp_microseconds(*text);
  if (!parsed) {
    throw std::invalid_argument(std::string("eventually follows: invalid ") + role +
                                " timestamp '" + *text + "'");
  }
  auto seconds = *parsed / 1'000'000;
  auto fraction = *parsed % 1'000'000;
  if (fraction < 0) {
    fraction += 1'000'000;
    --seconds;
  }
  return {seconds, fraction * 1'000};
}

}  // namespace

std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph) {
  return discover_eventually_follows(log, graph, EventuallyFollowsOptions{});
}

std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph,
    const EventuallyFollowsOptions& options) {
  const std::size_t n = graph.activities().size();
  // Dense n×n counters: activity alphabets are small (tens to a few hundred),
  // so this stays cheap and keeps the per-pair increment branch-free.
  std::vector<std::uint64_t> counts(n * n, 0);

  // A completion attribute other than the event time selects the interval
  // semantics with the event time as the start, as the reference's default
  // start key is the event time: pm4py.discover_eventually_follows_graph(log,
  // timestamp_key="x") sorts each case by time:timestamp and counts a pair
  // only when the later event starts at or after the earlier one's "x"
  // (observed black-box).
  const bool builtin_completion = options.timestamp_attribute.empty() ||
                                  options.timestamp_attribute == "timestamp" ||
                                  options.timestamp_attribute == "time:timestamp";
  if (!options.start_timestamp_attribute.empty() || !builtin_completion) {
    const std::string& start_attribute = options.start_timestamp_attribute.empty()
                                             ? std::string("time:timestamp")
                                             : options.start_timestamp_attribute;
    struct Interval {
      Instant start;
      Instant end;
      std::size_t id = 0;
    };
    // Round FT: a CSV upload's starts and completions are ordered and
    // compared at their nanoseconds (observed black-box).
    const bool exact = log.dataframe_columns && log.dataframe_columns->csv_text;
    std::vector<Interval> events;
    for (const auto& trace : log.traces) {
      events.clear();
      events.reserve(trace.events.size());
      for (const auto& event : trace.events) {
        const auto id = graph.activity_id(event.activity);
        if (!id) continue;
        events.push_back({required_time(event, start_attribute, "start", exact),
                          required_time(event, options.timestamp_attribute, "completion", exact),
                          *id});
      }
      std::stable_sort(events.begin(), events.end(),
                       [](const Interval& left, const Interval& right) {
                         return left.start < right.start;
                       });
      for (std::size_t i = 0; i < events.size(); ++i) {
        const std::size_t row = events[i].id * n;
        for (std::size_t j = i + 1; j < events.size(); ++j) {
          if (events[j].start >= events[i].end) ++counts[row + events[j].id];
        }
      }
    }
    return edges_of(counts, n);
  }

  // The reference sorts every case stably by its timestamp before counting
  // (an event log whose events are out of time order counts in time order).
  // Python datetimes hold microseconds, so ties are compared at that
  // precision. The reference raises KeyError on an untimed event; native keeps
  // the trace order of a trace with an untimed or unreadable timestamp.
  struct TimedId {
    std::int64_t time = 0;
    std::size_t id = 0;
  };
  std::vector<TimedId> timed;
  std::vector<std::size_t> ids;
  for (const auto& trace : log.traces) {
    ids.clear();
    ids.reserve(trace.events.size());
    timed.clear();
    bool all_timed = true;
    for (const auto& event : trace.events) {
      const auto id = graph.activity_id(event.activity);
      if (all_timed) {
        const auto time = event.timestamp.empty()
                              ? std::nullopt
                              : parse_timestamp_microseconds(event.timestamp);
        if (time) {
          if (id) timed.push_back({*time, *id});
        } else {
          all_timed = false;
        }
      }
      if (id) ids.push_back(*id);
    }
    if (all_timed) {
      std::stable_sort(timed.begin(), timed.end(),
                       [](const TimedId& left, const TimedId& right) {
                         return left.time < right.time;
                       });
      for (std::size_t i = 0; i < timed.size(); ++i) ids[i] = timed[i].id;
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
      const std::size_t row = ids[i] * n;
      for (std::size_t j = i + 1; j < ids.size(); ++j) {
        ++counts[row + ids[j]];
      }
    }
  }
  return edges_of(counts, n);
}

}  // namespace minera
