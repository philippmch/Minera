#include "minera/event_log.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
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
  // Events already in order (the usual case) stay where they are.
  bool ordered = true;
  for (std::size_t i = 0; i < order.size() && ordered; ++i) ordered = order[i] == i;
  if (ordered) return;

  std::vector<Event> sorted;
  sorted.reserve(events.size());
  for (const std::size_t i : order) {
    sorted.push_back(std::move(events[i]));
  }
  events = std::move(sorted);
}

void order_events_by_exact_time(std::vector<Event>& events) {
  std::vector<ParsedExactTimestamp> instants;
  instants.reserve(events.size());
  for (const auto& event : events) {
    auto instant = parse_timestamp_exact(event.timestamp);
    if (!instant) {
      // Same fallback as order_events_by_time.
      order_events_by_time(events);
      return;
    }
    instants.push_back(*instant);
  }
  std::vector<std::size_t> order(events.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) {
                     if (instants[a].seconds != instants[b].seconds)
                       return instants[a].seconds < instants[b].seconds;
                     if (instants[a].nanoseconds != instants[b].nanoseconds)
                       return instants[a].nanoseconds < instants[b].nanoseconds;
                     return events[a].input_order < events[b].input_order;
                   });
  // Events already in order (the usual case) stay where they are.
  bool ordered = true;
  for (std::size_t i = 0; i < order.size() && ordered; ++i) ordered = order[i] == i;
  if (ordered) return;
  std::vector<Event> sorted;
  sorted.reserve(events.size());
  for (const std::size_t i : order) sorted.push_back(std::move(events[i]));
  events = std::move(sorted);
}

void merge_dataframe_cases(EventLog& log) {
  std::unordered_map<std::string, std::size_t> case_of;
  std::vector<Trace> cases;
  cases.reserve(log.traces.size());
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    const bool present = trace.case_id_state == FirstClassAttributeState::kPresent ||
                         (trace.case_id_state == FirstClassAttributeState::kMissing &&
                          !trace.case_id.empty());
    if (present) {
      const auto [found, added] = case_of.try_emplace(trace.case_id, cases.size());
      if (!added) {
        auto& events = cases[found->second].events;
        events.insert(events.end(), std::make_move_iterator(trace.events.begin()),
                      std::make_move_iterator(trace.events.end()));
        continue;
      }
    }
    cases.push_back(std::move(trace));
  }
  // Appending keeps a merged case's events in the frame's row (file) order.
  log.traces = std::move(cases);
}

std::vector<std::size_t> case_id_order(const EventLog& log) {
  std::vector<std::size_t> order(log.traces.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return log.traces[a].case_id < log.traces[b].case_id;
  });
  return order;
}

}  // namespace minera
