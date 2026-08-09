#pragma once

#include <cstddef>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

struct CycleTimeOptions {
  // Empty, "timestamp", and "time:timestamp" select Event::timestamp.
  std::string timestamp_key = "timestamp";
  // The start instant used for interval ordering. The default is the
  // completion timestamp, matching atomic event logs.
  std::string start_timestamp_key = "timestamp";
};

struct CycleTimeResult {
  double cycle_time = 0.0;
  std::size_t instances = 0;
  std::size_t timed_events = 0;
};

[[nodiscard]] CycleTimeResult discover_cycle_time(
    const EventLog& log, const CycleTimeOptions& options = {});

[[nodiscard]] inline CycleTimeResult cycle_time(
    const EventLog& log, const CycleTimeOptions& options = {}) {
  return discover_cycle_time(log, options);
}

[[nodiscard]] inline CycleTimeResult compute_cycle_time(
    const EventLog& log, const CycleTimeOptions& options = {}) {
  return discover_cycle_time(log, options);
}

}  // namespace minera
