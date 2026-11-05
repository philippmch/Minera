#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct EventuallyFollowsEdge {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t count = 0;
};

// Discovers the eventually-follows graph: for every ordered pair of
// activities (a, b), how often an occurrence of a is followed — directly or
// transitively — by an occurrence of b later in the same case. Each trace is
// first sorted stably by timestamp, as the reference sorts each case (a trace
// with an untimed event keeps its order). Every position
// pair i < j within the sorted trace contributes one observation, so a pair connected
// only through intermediate steps still shows up, which is what makes the EFG
// useful for spotting long-range order (and, by absence, activities that
// never co-occur). Activity ids index into `graph.activities()`, so they line
// up with everything else in an export. Edges come out grouped by source, in
// registry order.
[[nodiscard]] std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph);

struct EventuallyFollowsOptions {
  // Empty keeps the position semantics above. Otherwise the reference's
  // interval semantics (statistics.eventually_follows with a start timestamp
  // key): each trace is stably sorted by this start attribute, and position
  // pair i < j counts only when event j starts at or after event i completes.
  // A missing or unreadable start or completion timestamp throws, where the
  // reference raises KeyError.
  std::string start_timestamp_attribute;
  // Completion timestamp; empty, "timestamp" or "time:timestamp" read
  // Event::timestamp.
  std::string timestamp_attribute;
};

[[nodiscard]] std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph,
    const EventuallyFollowsOptions& options);

}  // namespace minera
