#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct VariantStatisticsOptions {
  // Empty, "activity", and "concept:name" select Event::activity.
  std::string activity_key = "activity";
  std::size_t max_variants = 0;  // zero means return every variant
};

struct VariantStatisticsEntry {
  std::vector<std::string> activities;
  std::uint64_t count = 0;
  double probability = 0.0;
};

struct VariantStatisticsResult {
  std::string activity_key;
  std::vector<VariantStatisticsEntry> variants;
};

[[nodiscard]] VariantStatisticsResult discover_variant_statistics(
    const EventLog& log, const VariantStatisticsOptions& options = {});

// An upload's variants as pm4py's get_variants counts them on its read_xes
// frame: each case is the frame's rows from its first row on, as many as the
// case has (frame_slice_cases), so an XES case whose <trace> elements repeat
// its name apart from each other counts other cases' rows (round BK, observed
// black-box). Any other log is counted as discover_variant_statistics does.
[[nodiscard]] VariantStatisticsResult discover_variant_statistics_as_uploaded(
    const EventLog& log, const VariantStatisticsOptions& options = {});

[[nodiscard]] inline VariantStatisticsResult variant_statistics(
    const EventLog& log, const VariantStatisticsOptions& options = {}) {
  return discover_variant_statistics(log, options);
}

[[nodiscard]] const char* variant_statistics_variant_name() noexcept;

}  // namespace minera
