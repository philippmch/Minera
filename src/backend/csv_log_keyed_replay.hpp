#pragma once

// pm4py's token replay of the converted heuristics net on a CSV upload's log
// shape keyed by another column of its frame (round GO, observed black-box on
// pm4py 2.7.22 with pandas 3.0.5).
//
// convert_to_event_log keeps every column of format_dataframe's frame in each
// event dict, a gap as NaN (one shared object in a text or object column, a
// fresh float per cell in a float64 column) or NaT (one object) in a date
// column, and the frame's dates as UTC Timestamps. The replay reads the key
// from those dicts, so a gap is no KeyError: it groups the traces into
// variants by their tuples of cells (equal cells by Python's ==, a shared NaN
// equal to itself) and sorts them by (number of traces, tuple), descending;
// two variants with as many traces whose first unequal cells cannot be
// ordered (NaN or NaT beside text) raise that comparison's TypeError. Each
// cell then matches the transition labelled with an equal object, which is
// the heuristics miner's node of that cell (frame_keyed_activities names the
// nodes and the replayed events alike). Before the variants, the replay
// sorts the net's transitions by name: a gap's node in a text or object
// column is a transition named NaN, which raises TypeError beside any other
// (check_heuristics_null_transition_names).

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/dataframe_columns.hpp"
#include "minera/event_log.hpp"
#include "minera/frame_keys.hpp"
#include "frame_cells.hpp"
#include "python_scalar.hpp"
#include "xes_frame_token_replay.hpp"

namespace minera::detail {

// The log to replay with "concept:name" in place of `key` (nullopt when the
// log is no CSV upload's log shape or `key` names no column of its frame).
// Raises the transition sort's TypeError (with the mined heuristics net and
// its Petri net), then the variant sort's.
inline std::optional<EventLog> csv_log_keyed_replay_log(const EventLog& log, const std::string& key,
                                                        const HeuristicsNet* heuristics = nullptr,
                                                        const PetriNet* net = nullptr) {
  if (!log.dataframe_columns || !log.dataframe_columns->csv_text) return std::nullopt;
  auto keyed = frame_keyed_activities(log, key);
  if (!keyed) return std::nullopt;
  const auto frame = dataframe_frame_event_log(log);
  if (frame.traces.size() != log.traces.size()) return std::nullopt;
  const auto column = frame_column_type(frame, key);
  const bool text_column = column && (*column == DataFrameColumnType::kString || *column == DataFrameColumnType::kObject);
  if (heuristics && net && keyed->any_missing && text_column) {
    const auto at = std::find(heuristics->activities.begin(), heuristics->activities.end(), std::string("nan"));
    // The other transitions' names: silent ones are text, a node's its cell
    // (bools and numbers order beside NaN).
    bool text_name = std::any_of(net->transitions().begin(), net->transitions().end(),
                                 [](const auto& transition) { return transition.silent(); });
    for (std::size_t t = 0; t < keyed->log.traces.size() && !text_name; ++t)
      for (std::size_t e = 0; e < keyed->types[t].size(); ++e)
        if (keyed->types[t][e].empty() && !keyed->missing[t][e]) text_name = true;
    if (at != heuristics->activities.end() && text_name) {
      auto named = *heuristics;
      named.null_activity = static_cast<std::size_t>(at - heuristics->activities.begin());
      check_heuristics_null_transition_names(named, *net);
    }
  }
  enum class Gap { kNone, kNaN, kNaT };
  struct Cell {
    python::Scalar value;
    Gap gap = Gap::kNone;
    std::string identity;
  };
  const auto type_of = [](const Cell& cell) -> std::string {
    if (cell.gap == Gap::kNaT) return "NaTType";
    if (cell.gap == Gap::kNaN) return "float";
    return python::type_name(cell.value);
  };
  std::vector<std::vector<Cell>> variants;
  std::vector<std::size_t> counts;
  std::map<std::vector<std::string>, std::size_t> index;
  for (std::size_t t = 0; t < frame.traces.size(); ++t) {
    const auto& trace = frame.traces[t];
    std::vector<Cell> tuple;
    std::vector<std::string> identity;
    for (std::size_t e = 0; e < trace.events.size(); ++e) {
      Cell cell;
      const auto value = frame_scalar(frame, trace, trace.events[e], key, FrameKeyScope::kEventLog);
      if (!value || python::is_missing(*value)) {
        const bool dates = column && *column == DataFrameColumnType::kDate;
        cell.gap = dates ? Gap::kNaT : Gap::kNaN;
        // A float64 column's gaps are distinct objects, never equal.
        cell.identity = dates ? "NaT"
            : column && *column == DataFrameColumnType::kFloat64 ? "nan#" + std::to_string(t) + ":" + std::to_string(e)
            : "nan";
      } else {
        cell.value = *value;
        cell.identity = python::identity(*value);
      }
      identity.push_back(cell.identity);
      tuple.push_back(std::move(cell));
    }
    const auto [it, added] = index.emplace(std::move(identity), variants.size());
    if (added) {
      variants.push_back(std::move(tuple));
      counts.push_back(0);
    }
    ++counts[it->second];
  }
  const auto unorderable = [&](const Cell& a, const Cell& b) {
    throw std::invalid_argument("TypeError: '<' not supported between instances of '" + type_of(a) + "' and '" +
                                type_of(b) + "'");
  };
  // a < b on two unequal cells: NaN against a number and NaT against a
  // Timestamp are False, against anything else TypeError.
  const auto less = [&](const Cell& a, const Cell& b) -> bool {
    if (a.gap == Gap::kNone && b.gap == Gap::kNone) {
      try {
        return python::compare(a.value, b.value) < 0;
      } catch (const python::TypeError&) {
        unorderable(a, b);
      }
    }
    const auto numeric = [](const Cell& c) { return c.gap == Gap::kNaN || (c.gap == Gap::kNone && python::is_number(c.value)); };
    const auto dated = [](const Cell& c) {
      return c.gap == Gap::kNaT || (c.gap == Gap::kNone && c.value.kind == python::Scalar::Kind::kDate);
    };
    if ((numeric(a) && numeric(b)) || (dated(a) && dated(b))) return false;
    unorderable(a, b);
    return false;
  };
  std::vector<std::size_t> order(variants.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  python::sort_reverse(order, [&](std::size_t a, std::size_t b) {
    if (counts[a] != counts[b]) return counts[a] < counts[b];
    const auto& x = variants[a];
    const auto& y = variants[b];
    for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
      if (x[i].identity == y[i].identity) continue;
      return less(x[i], y[i]);
    }
    return x.size() < y.size();
  });
  return std::move(keyed->log);
}

}  // namespace minera::detail
