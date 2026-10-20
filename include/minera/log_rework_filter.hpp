#pragma once

#include "minera/event_log.hpp"

namespace minera {

// monostate is Python None. Invalid string/None comparisons raise only for
// traces containing the target. Signed/unsigned controls retain exact integers.
using LogReworkMinimum = std::variant<std::monostate, bool, std::int64_t,
    std::uint64_t, double, std::string>;
struct LogReworkFilterOptions {
  std::string activity_key = "concept:name";
  LogReworkMinimum min_occurrences = std::int64_t{2};
  bool positive = true;
};

// A qualifying positive trace must contain the activity even for minimum <= 0.
// Negative mode uses count < minimum, so NaN polarities are not complements.
// Empty labels are active; missing selected event keys raise out_of_range.
// Complete log context and retained traces/events are copied without mutation.
[[nodiscard]] EventLog filter_log_rework(const EventLog& source,
    std::string_view activity, const LogReworkFilterOptions& options = {});
// Primitive scalar target: string, integer, float, bool, or present null.
// Numeric equality is exact across integer/float/bool; strings stay distinct.
[[nodiscard]] EventLog filter_log_rework(const EventLog& source,
    const EventAttribute& activity, const LogReworkFilterOptions& options = {});

}  // namespace minera
