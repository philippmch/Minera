#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class MinimumSelfDistanceVariant {
  kLog,
  kPandas,
  kPolars,
  LOG = kLog,
  PANDAS = kPandas,
  POLARS = kPolars,
};

struct MinimumSelfDistanceOptions {
  MinimumSelfDistanceVariant variant = MinimumSelfDistanceVariant::kLog;
  // Event activity and case-id selectors. Built-in aliases use the native
  // first-class fields; other names resolve typed event/trace attributes.
  std::string activity_key = "concept:name";
  std::string case_id_key = "case:concept:name";
};

struct MinimumSelfDistance {
  std::string activity;
  std::uint64_t distance = 0;
  std::vector<std::string> witnesses;
};

using MinimumSelfDistances = std::vector<MinimumSelfDistance>;

// Discovers the minimum number of events strictly between consecutive
// occurrences of each repeated activity, across all traces. Activities that
// occur at most once in every trace are omitted. Witnesses are the sorted
// union of activities occurring in every minimum-distance segment.
[[nodiscard]] MinimumSelfDistances discover_minimum_self_distances(
    const EventLog& log,
    const MinimumSelfDistanceOptions& options = {});

}  // namespace minera
