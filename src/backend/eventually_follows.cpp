#include "minera/eventually_follows.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>

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

std::int64_t required_time(const Event& event, const std::string& key,
                           const char* role) {
  const std::string* text = nullptr;
  if (key.empty() || key == "timestamp" || key == "time:timestamp") {
    text = &event.timestamp;
  } else {
    text = event.attribute(key);
  }
  if (text == nullptr || text->empty()) {
    throw std::invalid_argument(std::string("eventually follows: event is missing ") +
                                role + " timestamp attribute '" + key + "'");
  }
  const auto parsed = parse_timestamp_microseconds(*text);
  if (!parsed) {
    throw std::invalid_argument(std::string("eventually follows: invalid ") + role +
                                " timestamp '" + *text + "'");
  }
  return *parsed;
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

  if (!options.start_timestamp_attribute.empty()) {
    struct Interval {
      std::int64_t start = 0;
      std::int64_t end = 0;
      std::size_t id = 0;
    };
    std::vector<Interval> events;
    for (const auto& trace : log.traces) {
      events.clear();
      events.reserve(trace.events.size());
      for (const auto& event : trace.events) {
        const auto id = graph.activity_id(event.activity);
        if (!id) continue;
        events.push_back({required_time(event, options.start_timestamp_attribute, "start"),
                          required_time(event, options.timestamp_attribute, "completion"),
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
