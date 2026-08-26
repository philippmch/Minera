#include "minera/event_log.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {

void order_events_by_time(std::vector<Event>& events) {
  std::vector<std::optional<double>> instants;
  instants.reserve(events.size());
  bool all_parsed = true;
  for (const auto& event : events) {
    auto instant = parse_timestamp(event.timestamp);
    if (!instant) {
      all_parsed = false;
    }
    instants.push_back(instant);
  }

  if (!all_parsed) {
    // At least one timestamp is not a recognizable instant. Comparing parsed
    // instants for some events and raw strings for others would not be a
    // consistent ordering, so fall back to a stable lexicographic sort with
    // document order breaking ties -- the historical behaviour.
    std::stable_sort(events.begin(), events.end(),
                     [](const Event& lhs, const Event& rhs) {
                       if (lhs.timestamp != rhs.timestamp) {
                         return lhs.timestamp < rhs.timestamp;
                       }
                       return lhs.input_order < rhs.input_order;
                     });
    return;
  }

  // Sort indices so the parsed instants follow the permutation, then rebuild the
  // event vector. Equal instants keep document order.
  std::vector<std::size_t> order(events.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) {
                     if (*instants[a] != *instants[b]) {
                       return *instants[a] < *instants[b];
                     }
                     return events[a].input_order < events[b].input_order;
                   });

  std::vector<Event> sorted;
  sorted.reserve(events.size());
  for (const std::size_t i : order) {
    sorted.push_back(std::move(events[i]));
  }
  events = std::move(sorted);
}

}  // namespace minera
