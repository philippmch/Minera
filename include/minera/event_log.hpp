#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace minera {

struct Event {
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string resource;  // who performed the activity; empty when unknown
  std::size_t input_order = 0;
};

struct Trace {
  std::string case_id;
  std::vector<Event> events;
};

// Orders a trace's events by their parsed timestamp instant (normalized to UTC
// via parse_timestamp), so events recorded with different timezone offsets or
// formats still sort by the moment they actually occurred. Document order
// (input_order) breaks ties. When any event's timestamp is not a recognizable
// instant, the function falls back to a stable lexicographic order so the
// comparison stays a consistent strict weak ordering.
void order_events_by_time(std::vector<Event>& events);

struct EventLog {
  std::vector<Trace> traces;

  [[nodiscard]] std::size_t trace_count() const noexcept {
    return traces.size();
  }

  [[nodiscard]] std::size_t event_count() const noexcept {
    std::size_t count = 0;
    for (const auto& trace : traces) {
      count += trace.events.size();
    }
    return count;
  }
};

}  // namespace minera
