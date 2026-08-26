#include "minera/eventually_follows.hpp"

namespace minera {

std::vector<EventuallyFollowsEdge> discover_eventually_follows(
    const EventLog& log, const DirectlyFollowsGraph& graph) {
  const std::size_t n = graph.activities().size();
  // Dense n×n counters: activity alphabets are small (tens to a few hundred),
  // so this stays cheap and keeps the per-pair increment branch-free.
  std::vector<std::uint64_t> counts(n * n, 0);

  std::vector<std::size_t> ids;
  for (const auto& trace : log.traces) {
    ids.clear();
    ids.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      if (const auto id = graph.activity_id(event.activity)) {
        ids.push_back(*id);
      }
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
      const std::size_t row = ids[i] * n;
      for (std::size_t j = i + 1; j < ids.size(); ++j) {
        ++counts[row + ids[j]];
      }
    }
  }

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

}  // namespace minera
