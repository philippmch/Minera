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

void merge_dataframe_cases(EventLog& log,
                           std::vector<std::vector<std::size_t>>* frame_rows) {
  std::unordered_map<std::string, std::size_t> case_of;
  std::vector<Trace> cases;
  // Per case: the attributes of each trace merged into it (one entry per
  // trace that has events) and the trace each event came from.
  std::vector<std::vector<std::vector<EventAttribute>>> trace_cells;
  std::vector<std::vector<std::size_t>> event_trace;
  std::vector<std::vector<std::size_t>> rows;
  cases.reserve(log.traces.size());
  std::size_t row = 0;
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    const bool present = trace.case_id_state == FirstClassAttributeState::kPresent ||
                         (trace.case_id_state == FirstClassAttributeState::kMissing &&
                          !trace.case_id.empty());
    std::size_t index = cases.size();
    if (present) {
      const auto [found, added] = case_of.try_emplace(trace.case_id, cases.size());
      index = found->second;
      if (!added) {
        auto& target = cases[index];
        trace_cells[index].push_back(std::move(trace.attributes));
        for (auto& event : trace.events) {
          event_trace[index].push_back(trace_cells[index].size() - 1);
          rows[index].push_back(row++);
          target.events.push_back(std::move(event));
        }
        continue;
      }
    }
    trace_cells.push_back({trace.attributes});
    event_trace.emplace_back(trace.events.size(), 0);
    rows.emplace_back();
    for (std::size_t event = 0; event < trace.events.size(); ++event) rows.back().push_back(row++);
    cases.push_back(std::move(trace));
  }
  // Appending keeps a merged case's events in the frame's row (file) order.
  // Where the merged traces' attributes differ, every row keeps its own
  // trace's case cells, as the frame does.
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto& cells = trace_cells[index];
    if (cells.size() < 2 ||
        std::all_of(cells.begin() + 1, cells.end(),
                    [&](const auto& other) { return other == cells.front(); }))
      continue;
    std::vector<std::string> keys;
    for (const auto& attributes : cells)
      for (const auto& attribute : attributes)
        if (std::find(keys.begin(), keys.end(), attribute.key) == keys.end())
          keys.push_back(attribute.key);
    auto& events = cases[index].events;
    for (std::size_t event = 0; event < events.size(); ++event) {
      const auto& own = cells[event_trace[index][event]];
      auto& row_cells = events[event].case_row_attributes;
      row_cells.clear();
      row_cells.reserve(keys.size());
      for (const auto& key : keys) {
        const auto found = std::find_if(own.begin(), own.end(),
                                        [&](const auto& attribute) { return attribute.key == key; });
        if (found != own.end()) {
          row_cells.push_back(*found);
        } else {
          EventAttribute missing{key, ""};
          missing.is_null = true;
          row_cells.push_back(std::move(missing));
        }
      }
    }
  }
  log.traces = std::move(cases);
  if (frame_rows) *frame_rows = std::move(rows);
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
