#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct OpenPathIntervalOptions {
  std::string activity_key = "activity";
  std::string start_timestamp_key = "time:timestamp";
  std::string timestamp_key = "time:timestamp";
  double epsilon = 0.00001;
  std::optional<std::pair<std::string, std::string>> filter_activity_couple;
  std::optional<double> query_point;
  std::optional<std::pair<double, double>> query_range;
};

struct OpenPathInterval {
  double start = 0.0;
  double end = 0.0;
  std::string case_id;
  std::string source_activity;
  std::string target_activity;
  std::size_t trace_index = 0;
  std::size_t source_event_index = 0;
  std::size_t target_event_index = 0;
  friend bool operator==(const OpenPathInterval&, const OpenPathInterval&) =
      default;
};

struct IntervalTreeModel {
  std::vector<OpenPathInterval> intervals;
  friend bool operator==(const IntervalTreeModel&, const IntervalTreeModel&) =
      default;
};

struct IntervalTreeQueryOptions {
  std::optional<double> point;
  std::optional<std::pair<double, double>> range;
};

struct IntervalTreeQueryResult {
  std::optional<double> point;
  std::optional<std::pair<double, double>> range;
  std::vector<OpenPathInterval> intervals;
  std::vector<std::size_t> point_matches;
  std::vector<std::size_t> range_matches;
  friend bool operator==(const IntervalTreeQueryResult&,
                         const IntervalTreeQueryResult&) = default;
};

using IntervalTree = IntervalTreeModel;
using IntervalTreeOptions = OpenPathIntervalOptions;

[[nodiscard]] std::vector<OpenPathInterval> discover_open_path_intervals(
    const EventLog& log, const OpenPathIntervalOptions& options = {});

[[nodiscard]] IntervalTreeQueryResult query_open_path_intervals(
    const std::vector<OpenPathInterval>& intervals,
    const IntervalTreeQueryOptions& options = {});

[[nodiscard]] IntervalTreeQueryOptions interval_tree_query_options(
    const OpenPathIntervalOptions& options);

[[nodiscard]] inline IntervalTreeModel log_to_interval_tree(
    const EventLog& log, const OpenPathIntervalOptions& options = {}) {
  return {discover_open_path_intervals(log, options)};
}

}  // namespace minera
