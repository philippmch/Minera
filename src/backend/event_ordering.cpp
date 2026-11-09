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
  // Per case: the attributes of each further trace merged into it (the first
  // trace's stay on the case) and, once one was merged, the trace each event
  // came from (0 for the first, i for the i-th further one).
  std::vector<std::vector<std::vector<EventAttribute>>> merged_cells;
  std::vector<std::vector<std::size_t>> event_trace;
  std::vector<std::vector<std::size_t>> rows;
  cases.reserve(log.traces.size());
  const auto present = [](const Trace& trace) {
    return trace.case_id_state == FirstClassAttributeState::kPresent ||
           (trace.case_id_state == FirstClassAttributeState::kMissing && !trace.case_id.empty());
  };
  // Each case's event count, so a merged case's events are moved once into
  // storage of their final size (no regrowth left behind as free heap).
  std::unordered_map<std::string, std::size_t> case_events;
  bool repeated = false;
  for (const auto& trace : log.traces)
    if (!trace.events.empty() && present(trace)) {
      auto [found, added] = case_events.try_emplace(trace.case_id, 0);
      repeated = repeated || !added;
      found->second += trace.events.size();
    }
  std::size_t row = 0;
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    if (present(trace)) {
      const auto [found, added] = case_of.try_emplace(trace.case_id, cases.size());
      if (!added) {
        const auto index = found->second;
        auto& target = cases[index];
        auto& origins = event_trace[index];
        if (origins.empty()) origins.assign(target.events.size(), 0);
        merged_cells[index].push_back(std::move(trace.attributes));
        const auto origin = merged_cells[index].size();
        for (auto& event : trace.events) {
          origins.push_back(origin);
          if (frame_rows) rows[index].push_back(row);
          ++row;
          target.events.push_back(std::move(event));
        }
        continue;
      }
    }
    merged_cells.emplace_back();
    event_trace.emplace_back();
    rows.emplace_back();
    if (frame_rows) {
      rows.back().resize(trace.events.size());
      std::iota(rows.back().begin(), rows.back().end(), row);
    }
    row += trace.events.size();
    if (repeated && present(trace)) trace.events.reserve(case_events.at(trace.case_id));
    cases.push_back(std::move(trace));
  }
  // Appending keeps a merged case's events in the frame's row (file) order.
  // Where the merged traces' attributes differ, every row keeps its own
  // trace's case cells, as the frame does: one shared list per trace.
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto& extra = merged_cells[index];
    const auto& first = cases[index].attributes;
    if (extra.empty() ||
        std::all_of(extra.begin(), extra.end(), [&](const auto& other) { return other == first; }))
      continue;
    std::vector<const std::vector<EventAttribute>*> origins{&first};
    for (const auto& attributes : extra) origins.push_back(&attributes);
    std::vector<std::string> keys;
    for (const auto* attributes : origins)
      for (const auto& attribute : *attributes)
        if (std::find(keys.begin(), keys.end(), attribute.key) == keys.end())
          keys.push_back(attribute.key);
    std::vector<CaseRowCells> shared;
    shared.reserve(origins.size());
    for (const auto* own : origins) {
      std::vector<EventAttribute> row_cells;
      row_cells.reserve(keys.size());
      for (const auto& key : keys) {
        const auto found = std::find_if(own->begin(), own->end(),
                                        [&](const auto& attribute) { return attribute.key == key; });
        if (found != own->end()) {
          row_cells.push_back(*found);
        } else {
          EventAttribute missing{key, ""};
          missing.is_null = true;
          row_cells.push_back(std::move(missing));
        }
      }
      shared.emplace_back(std::move(row_cells));
    }
    auto& events = cases[index].events;
    for (std::size_t event = 0; event < events.size(); ++event)
      events[event].case_row_attributes = shared[event_trace[index][event]];
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

void order_cases_by_id(EventLog& log) {
  const auto order = case_id_order(log);
  bool ordered = true;
  for (std::size_t i = 0; i < order.size() && ordered; ++i) ordered = order[i] == i;
  if (ordered) return;
  std::vector<Trace> traces;
  traces.reserve(order.size());
  for (const auto i : order) traces.push_back(std::move(log.traces[i]));
  log.traces = std::move(traces);
}

}  // namespace minera
