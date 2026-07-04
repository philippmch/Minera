#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"

namespace minera {

// Summary statistics over a set of durations, all in seconds. `count` is the
// number of observations; the rest are zero when nothing was observed.
struct DurationStats {
  std::uint64_t count = 0;
  double total = 0.0;
  double mean = 0.0;
  double median = 0.0;
  double min = 0.0;
  double max = 0.0;
};

// Waiting-time statistics for one directly-follows edge, identified by the same
// activity ids the DirectlyFollowsGraph uses.
struct EdgePerformance {
  std::size_t source = 0;
  std::size_t target = 0;
  DurationStats stats;
};

// The performance (time) perspective of a log: how long the process waits on
// each directly-follows edge and how long whole cases take end to end. Edge
// timings are keyed by activity id and emitted in the same order as the
// directly-follows graph's edges, so a frontend can zip the two together.
struct PerformanceMetrics {
  std::vector<EdgePerformance> edges;
  DurationStats case_duration;     // first-to-last event of each timed trace
  // Case arrival rate: the gaps between consecutive case start instants
  // (cases sorted by their first timed event). `mean` is the average time
  // until the next case arrives; empty when fewer than two cases are timed.
  DurationStats case_arrival;
  std::uint64_t timed_traces = 0;  // traces with at least two parseable instants
};

// Measures edge waiting times and case throughput times from event timestamps.
// Events whose timestamps do not parse are skipped, so a partially-timed or
// untimed log yields empty stats rather than an error. The `graph` supplies the
// activity ids the edge timings are reported against.
[[nodiscard]] PerformanceMetrics compute_performance_metrics(
    const EventLog& log,
    const DirectlyFollowsGraph& graph);

// Case throughput time (first-to-last event) summarized per trace variant. The
// returned vector is aligned to `graph.variants()` order, so entry i describes
// the variant graph.variants()[i]. A variant whose traces carry no parseable
// timestamps yields an empty DurationStats (count 0) rather than fabricated
// numbers, matching compute_performance_metrics.
[[nodiscard]] std::vector<DurationStats> compute_variant_durations(
    const EventLog& log,
    const DirectlyFollowsGraph& graph);

}  // namespace minera
