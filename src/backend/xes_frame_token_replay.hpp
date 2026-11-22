#pragma once

// pm4py's token replay on the frame of an XES upload (round GD, observed
// black-box on pm4py 2.7.22 with pandas 3.0.5 and Polars 1.44.1).
//
// token_replay.apply on a frame replays the frame's variants, not a log
// rebuilt from the upload: each case's activities in the frame's row order
// (no sort by timestamp), the cases of a pandas frame in its sorted groupby
// order without the rows whose case is missing, those of a Polars frame in
// first-row order with the rows whose case is null as one more case. The
// variants are then sorted by (number of cases, activity tuple), descending,
// so two variants with as many cases that first differ where one has a
// missing label (pandas' NaN, Polars' None; the same object throughout, so
// two missing labels are equal) and the other a name raise TypeError. A
// pandas NaN matches no transition of the heuristics net, but Alpha's NaN
// transition (labelled with the frame's own object); Polars' None matches
// the net's silent transitions (the replay's match of a null activity).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/petri_net.hpp"
#include "frame_key_column.hpp"
#include "python_scalar.hpp"
#include "xes_frame_columns.hpp"

namespace minera::detail {

struct XesFrameReplayLog {
  EventLog log;          // one trace per case, in the replay's case order
  bool polars = false;   // a missing label is None (matches silent transitions)
};

// The frame's cases and their activities as token_replay.apply groups them
// (see above); raises the variant sort's TypeError. `case_key` and
// `activity_key` must be columns of the frame (check_xes_frame_replay_columns).
inline XesFrameReplayLog xes_frame_replay_log(const EventDataView& data, const std::string& case_key,
                                              const std::string& activity_key) {
  XesFrameReplayLog result;
  result.polars = data.kind() == EventDataKind::kLazyEventTable;
  if (!result.polars && data.kind() != EventDataKind::kEventTable)
    throw std::invalid_argument("token replay of an XES frame needs a pandas or Polars frame");
  std::optional<EventTable> materialized;
  const EventTable* source = result.polars ? &materialized.emplace(data.lazy_table()->materialize()) : data.table();
  std::optional<EventTable> storage;
  const auto& table = xes_frame_present_columns(*source, storage, result.polars);
  std::optional<std::size_t> case_at, activity_at;
  for (std::size_t column = 0; column < table.columns.size(); ++column) {
    if (table.columns[column] == case_key && !case_at) case_at = column;
    if (table.columns[column] == activity_key && !activity_at) activity_at = column;
  }
  if (!case_at || !activity_at) throw std::invalid_argument("token replay of an XES frame: missing column");
  const auto cell = [&](std::size_t row, std::size_t column) -> const EventAttribute* {
    const auto& values = table.rows[row];
    return column < values.size() && values[column] ? &*values[column] : nullptr;
  };

  // The cases, each with its rows in frame order.
  std::vector<python::Scalar> case_ids;
  std::vector<std::vector<std::size_t>> case_rows;
  if (!result.polars) {
    std::vector<std::vector<python::Scalar>> keys;
    keys.reserve(table.rows.size());
    for (std::size_t row = 0; row < table.rows.size(); ++row) keys.push_back({frame_key_cell(cell(row, *case_at))});
    for (auto& group : python::groupby(keys)) {
      case_ids.push_back(group.labels.front());
      case_rows.push_back(std::move(group.rows));
    }
  } else {
    std::map<std::string, std::size_t> seen;
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
      auto key = frame_key_cell(cell(row, *case_at));
      const auto [it, added] = seen.emplace(python::identity(key), case_ids.size());
      if (added) {
        case_ids.push_back(std::move(key));
        case_rows.emplace_back();
      }
      case_rows[it->second].push_back(row);
    }
  }

  // The activity tuples; a missing label is NaN on pandas, None on Polars.
  const auto label = [&](std::size_t row) {
    auto value = frame_key_cell(cell(row, *activity_at));
    if (python::is_missing(value))
      return result.polars ? python::none() : python::real(std::numeric_limits<double>::quiet_NaN());
    return value;
  };
  std::vector<std::vector<python::Scalar>> variants;
  std::vector<std::size_t> counts;
  std::map<std::vector<std::string>, std::size_t> variant_index;
  for (const auto& rows : case_rows) {
    std::vector<python::Scalar> tuple;
    std::vector<std::string> identity;
    for (const auto row : rows) {
      tuple.push_back(label(row));
      identity.push_back(python::identity(tuple.back()));
    }
    const auto [it, added] = variant_index.emplace(std::move(identity), variants.size());
    if (added) {
      variants.push_back(std::move(tuple));
      counts.push_back(0);
    }
    ++counts[it->second];
  }
  // sorted(variants, key=lambda v: (count, tuple), reverse=True): tuple
  // comparison skips equal items (a missing label equals itself) and orders
  // the first unequal pair, which raises between a missing label and a name.
  std::vector<std::size_t> order(variants.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  try {
    python::sort_reverse(order, [&](std::size_t a, std::size_t b) {
      if (counts[a] != counts[b]) return counts[a] < counts[b];
      const auto& x = variants[a];
      const auto& y = variants[b];
      for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
        if (python::equal(x[i], y[i])) continue;
        return python::compare(x[i], y[i]) < 0;
      }
      return x.size() < y.size();
    });
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("TypeError: ") + error.what());
  }

  // One trace per case, its events carrying the label (null when missing).
  result.log.traces.reserve(case_rows.size());
  for (std::size_t c = 0; c < case_rows.size(); ++c) {
    Trace trace;
    if (python::is_missing(case_ids[c])) {
      trace.case_id_state = FirstClassAttributeState::kNull;
    } else {
      trace.case_id = python::str(case_ids[c]);
      trace.case_id_state = FirstClassAttributeState::kPresent;
    }
    trace.case_id_source_key = case_key;
    for (const auto row : case_rows[c]) {
      Event event;
      event.case_id = trace.case_id;
      event.input_order = row;
      const auto* field = cell(row, *activity_at);
      const auto value = frame_key_cell(field);
      event.activity_source_key = activity_key;
      if (python::is_missing(value) || !field) {
        event.activity_state = FirstClassAttributeState::kNull;
      } else {
        event.activity = field->value;
        event.activity_state = FirstClassAttributeState::kPresent;
      }
      trace.events.push_back(std::move(event));
    }
    result.log.traces.push_back(std::move(trace));
  }
  return result;
}

// pm4py's token replay first sorts the net's transitions by name. The
// heuristics net names a missing label's transition by the label (NaN on
// pandas, None on Polars), so with another transition beside it the sort
// raises TypeError, whatever the frame holds: the second transition's name
// against the missing label's when that transition comes first (created
// first: its label leads the net's activities), else the missing label's
// name against an earlier one (observed black-box).
inline void check_heuristics_null_transition_names(const HeuristicsNet& heuristics, const PetriNet& net) {
  const std::size_t n = heuristics.activities.size();
  if (!heuristics.null_activity || *heuristics.null_activity >= n || net.transitions().size() < 2) return;
  auto active = heuristics.active_activities;
  if (active.empty() && !heuristics.metadata.has_value())
    for (std::size_t i = 0; i < n; ++i) active.push_back(i);
  if (std::find(active.begin(), active.end(), *heuristics.null_activity) == active.end()) return;
  const std::string kind = heuristics_null_activity_is_none(heuristics) ? "NoneType" : "float";
  if (active.front() == *heuristics.null_activity)
    throw std::invalid_argument("TypeError: '<' not supported between instances of 'str' and '" + kind + "'");
  throw std::invalid_argument("TypeError: '<' not supported between instances of '" + kind + "' and 'str'");
}

// The same sort with Alpha's net on a frame, whose missing label (NaN on
// pandas, None on Polars) is a transition of that name beside the others'
// (Minera's net leaves it unlabeled: the only silent transition of an Alpha
// net). Which operand comes first follows the order of a Python set and
// changes with the hash seed (declared); the error names the missing label
// first, as most uploads do under PYTHONHASHSEED=0 (observed black-box).
inline void check_alpha_null_transition_names(const PetriNet& net, bool polars) {
  if (net.transitions().size() < 2) return;
  for (const auto& transition : net.transitions())
    if (transition.silent())
      throw std::invalid_argument(std::string("TypeError: '<' not supported between instances of '") +
                                  (polars ? "NoneType" : "float") + "' and 'str'");
}

}  // namespace minera::detail
