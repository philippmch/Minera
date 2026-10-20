#pragma once

#include "minera/event_log.hpp"

namespace minera {

struct LogTraceFilterOptions {
  bool positive = true;
  std::string activity_key = "concept:name";
};

// Reference-semantic comma-serialized trace filtering, separate from the
// literal event-vector/disabled-control extension in filter_trace_patterns.
// Empty admissions match every trace; negative polarity therefore keeps none.
// Empty patterns/labels and the exact wildcard token "..." are supported.
[[nodiscard]] EventLog filter_log_traces(
    const EventLog& source,
    const std::vector<std::vector<std::string>>& admitted_traces,
    const LogTraceFilterOptions& options = {});

}  // namespace minera
