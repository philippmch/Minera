#pragma once

#include <cstddef>
#include <cstdint>
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
// transitively — by an occurrence of b later in the same case. Every position
// pair i < j within a trace contributes one observation, so a pair connected
// only through intermediate steps still shows up, which is what makes the EFG
// useful for spotting long-range order (and, by absence, activities that
// never co-occur). Activity ids index into `graph.activities()`, so they line
// up with everything else in an export. Edges come out grouped by source, in
// registry order.
[[nodiscard]] std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph);

}  // namespace minera
