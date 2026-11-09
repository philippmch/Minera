#include "minera/event_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
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

namespace {

// Where a case id falls in the order pandas sorts a frame's case column
// (observed through pm4py's public frame results): ids without a value
// first (pm4py refuses them; see case_id_order), then the typed ids that
// are not strings, by value, then the strings in code-point order. pandas
// cannot compare numbers with dates (pm4py raises); here numbers go first.
struct CaseIdKey {
  enum Rank { kNone, kNumber, kNotANumber, kDate, kText } rank = kText;
  bool integral = false;
  bool negative = false;      // integral ids: sign of the value
  std::uint64_t magnitude = 0;  // integral ids: absolute value
  double real = 0;
  ParsedExactTimestamp instant{};
  const std::string* text = nullptr;
};

CaseIdKey case_id_key(const Trace& trace) {
  CaseIdKey key;
  key.text = &trace.case_id;
  const auto& cell = trace.case_id_attribute;
  if (!trace.has_case_id() || trace.case_id_state == FirstClassAttributeState::kNull ||
      (cell && cell->is_null)) {
    key.rank = CaseIdKey::kNone;
    return key;
  }
  if (!cell) return key;
  const auto& value = static_cast<const NativeAttributeValue::Base&>(cell->typed_value);
  if (cell->kind == AttributeKind::kInt || cell->kind == AttributeKind::kBoolean ||
      cell->kind == AttributeKind::kFloat) {
    if (const auto* number = std::get_if<std::int64_t>(&value)) {
      key.rank = CaseIdKey::kNumber, key.integral = true, key.negative = *number < 0;
      key.magnitude = *number < 0 ? static_cast<std::uint64_t>(-(*number + 1)) + 1
                                  : static_cast<std::uint64_t>(*number);
    } else if (const auto* number = std::get_if<std::uint64_t>(&value)) {
      key.rank = CaseIdKey::kNumber, key.integral = true, key.magnitude = *number;
    } else if (const auto* flag = std::get_if<bool>(&value)) {
      key.rank = CaseIdKey::kNumber, key.integral = true, key.magnitude = *flag ? 1 : 0;
    } else if (const auto* number = std::get_if<double>(&value)) {
      key.rank = std::isnan(*number) ? CaseIdKey::kNotANumber : CaseIdKey::kNumber;
      key.real = *number;
    }
  } else if (cell->kind == AttributeKind::kDate) {
    const auto* date = std::get_if<NativeDate>(&value);
    if (auto instant = parse_timestamp_exact(date ? date->value : cell->value)) {
      key.rank = CaseIdKey::kDate, key.instant = *instant;
    }
  }
  return key;
}

// Exact three-way comparison of two integers held as sign and magnitude.
int compare_integers(bool negative_a, std::uint64_t a, bool negative_b, std::uint64_t b) {
  negative_a = negative_a && a != 0;
  negative_b = negative_b && b != 0;
  if (negative_a != negative_b) return negative_a ? -1 : 1;
  const int magnitude = a < b ? -1 : b < a ? 1 : 0;
  return negative_a ? -magnitude : magnitude;
}

// Exact three-way comparison of an integer with a float that is not NaN,
// as Python compares int and float (no rounding of the integer).
int compare_integer_real(bool negative, std::uint64_t magnitude, double real) {
  if (std::isinf(real)) return real > 0 ? -1 : 1;
  const double whole = std::trunc(real);
  constexpr double kTwo64 = 18446744073709551616.0;
  if (whole >= kTwo64) return -1;
  if (whole <= -kTwo64) return 1;
  const bool real_negative = whole < 0;
  const auto real_magnitude = static_cast<std::uint64_t>(std::fabs(whole));
  const int result = compare_integers(negative, magnitude, real_negative, real_magnitude);
  if (result != 0) return result;
  const double fraction = real - whole;
  return fraction > 0 ? -1 : fraction < 0 ? 1 : 0;
}

bool case_id_less(const CaseIdKey& left, const CaseIdKey& right) {
  if (left.rank != right.rank) return left.rank < right.rank;
  switch (left.rank) {
    case CaseIdKey::kNumber:
      if (left.integral && right.integral)
        return compare_integers(left.negative, left.magnitude, right.negative, right.magnitude) < 0;
      if (left.integral) return compare_integer_real(left.negative, left.magnitude, right.real) < 0;
      if (right.integral) return compare_integer_real(right.negative, right.magnitude, left.real) > 0;
      return left.real < right.real;
    case CaseIdKey::kDate:
      if (left.instant.seconds != right.instant.seconds)
        return left.instant.seconds < right.instant.seconds;
      return left.instant.nanoseconds < right.instant.nanoseconds;
    case CaseIdKey::kText:
      return *left.text < *right.text;
    case CaseIdKey::kNone:
    case CaseIdKey::kNotANumber:
      return false;
  }
  return false;
}

}  // namespace

std::vector<std::size_t> case_id_order(const EventLog& log) {
  std::vector<CaseIdKey> keys;
  keys.reserve(log.traces.size());
  for (const auto& trace : log.traces) keys.push_back(case_id_key(trace));
  std::vector<std::size_t> order(log.traces.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return case_id_less(keys[a], keys[b]);
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
