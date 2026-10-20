#pragma once

#include "minera/event_log.hpp"
#include "minera/filtering_boundary_common.hpp"

namespace minera {

struct LogBoundaryFilterOptions {
  std::string activity_key = "concept:name";
  bool positive = true;
};

// Exact endpoint admissions. Empty labels/admissions are valid. Empty traces
// are omitted for either polarity; missing selected endpoint keys are errors.
// The older filter_start/end_activities convenience APIs remain unchanged.
[[nodiscard]] EventLog filter_log_start_activities(
    const EventLog& source, const std::vector<std::string>& admitted,
    const LogBoundaryFilterOptions& options = {});
[[nodiscard]] EventLog filter_log_end_activities(
    const EventLog& source, const std::vector<std::string>& admitted,
    const LogBoundaryFilterOptions& options = {});

using LogActivityVariant = std::vector<std::string>;
struct LogActivityVariantGroup {
  LogActivityVariant variant;
  std::vector<Trace> traces;
};
using RankedLogActivityVariants =
    std::vector<std::pair<LogActivityVariant, std::uint64_t>>;

// Supplied mapping iteration order and supplied variant ranking are distinct.
// Counts/ranking are used as supplied, not rediscovered or sorted. The result
// is a new log with default log context; complete retained traces are copied.
[[nodiscard]] EventLog filter_log_by_start_activities(
    const OrderedBoundaryActivityCounts& counts,
    const std::vector<LogActivityVariantGroup>& variants,
    const RankedLogActivityVariants& ranked, double threshold,
    std::string_view activity_key = "concept:name");
[[nodiscard]] EventLog filter_log_by_end_activities(
    const OrderedBoundaryActivityCounts& counts,
    const std::vector<LogActivityVariantGroup>& variants,
    const RankedLogActivityVariants& ranked, double threshold,
    std::string_view activity_key = "concept:name");

}  // namespace minera
