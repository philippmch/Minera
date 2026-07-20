#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

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
    const EventLog& log);

}  // namespace minera
