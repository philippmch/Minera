#pragma once

// pm4py's DataFrame directly-follows graph (pm4py.discover_dfg,
// discover_performance_dfg and the DFG performance variant) on an upload's
// frame (read_csv + format_dataframe, or read_xes) that holds a
// start_timestamp column (round GU, observed black-box against pm4py
// 2.7.22). The frame names the column as its start key, and the DFG sorts
// each case's rows by (start, timestamp): the start column by its own values
// whatever their dtype (pandas' sort ranks, a gap last, a column it cannot
// order raises), ties keeping the frame's order. Neighbours in that order
// are the directly-follows pairs. A pair's duration is the later row's start
// minus the earlier row's timestamp, signed; a NaT start gives NaN, which the
// aggregates skip (an edge of only NaN durations sums to 0, its mean NaN).
// Business hours clamp a duration at 0 and fail on a NaT start (ValueError).
// A start column that holds no dates fails the subtraction even without a
// pair (TypeError naming the shifted column's dtype: float64 for numbers or
// an all-empty column, object for text, booleans and mixed cells; with
// business hours AttributeError at the .dt accessor). Start and
// end activities keep the timestamp order. The timestamp column itself
// under the name start_timestamp changes nothing.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/timestamp.hpp"

namespace minera {

struct FrameDfgStartOrder {
  // Per trace of the log, its events' indices in the order the DFG pairs them.
  std::vector<std::vector<std::size_t>> order;
  // Per trace and event index, the event's start (nullopt: NaT, or no date).
  std::vector<std::vector<std::optional<ParsedExactTimestamp>>> start;
  // The error the performance DFG raises subtracting a start column that
  // holds no dates; empty for a date column. With business hours it fails
  // at the .dt accessor instead (business_hours_error).
  std::string subtraction_error;
  std::string business_hours_error;
};

// nullopt when the log was not read through a frame, or its frame holds no
// start_timestamp column (or holds the timestamp under that name).
[[nodiscard]] std::optional<FrameDfgStartOrder> frame_dfg_start_order(const EventLog& log);

// `graph` with its edges recounted in the order above.
[[nodiscard]] DirectlyFollowsGraph frame_dfg_graph(const DirectlyFollowsGraph& graph,
                                                   const EventLog& log,
                                                   const FrameDfgStartOrder& order);

// One directly-follows pair of that order: the trace and the two events'
// indices, the earlier event's timestamp and the later one's start.
struct FrameDfgPair {
  std::size_t trace = 0;
  std::size_t previous = 0;
  std::size_t current = 0;
  std::optional<ParsedExactTimestamp> previous_timestamp;
  std::optional<ParsedExactTimestamp> current_start;
  // current_start - previous_timestamp as pandas' Timedelta.total_seconds(),
  // signed; NaN without a start.
  double seconds = 0.0;
};

// The pairs in the order they reach the aggregates: the traces by case id
// (the frame sorted by case), each trace's pairs in order.
[[nodiscard]] std::vector<FrameDfgPair> frame_dfg_pairs(const EventLog& log,
                                                        const FrameDfgStartOrder& order);

}  // namespace minera
