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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/dataframe_columns.hpp"
#include "minera/event_log.hpp"
#include "minera/frame_keys.hpp"
#include "frame_cells.hpp"
#include "python_scalar.hpp"
#include "xes_frame_token_replay.hpp"
#include "xes_legacy_cell.hpp"

namespace minera::detail {

// One replayed cell: its value, a gap (NaN or NaT) and its identity (equal
// cells by Python's ==; a shared NaN equal to itself).
struct KeyedReplayCell {
  enum class Gap { kNone, kNaN, kNaT };
  python::Scalar value;
  Gap gap = Gap::kNone;
  std::string identity;
  std::string type;  // the Python type name when set (a legacy XES object)
};

// The replay's variants (traces grouped by their tuples of cells) sorted by
// (number of traces, tuple), descending, as Python sorts them: raises the
// first comparison's TypeError between cells that do not order.
inline void check_keyed_replay_variant_sort(const std::vector<std::vector<KeyedReplayCell>>& traces) {
  using Cell = KeyedReplayCell;
  using Gap = KeyedReplayCell::Gap;
  const auto type_of = [](const Cell& cell) -> std::string {
    if (!cell.type.empty()) return cell.type;
    if (cell.gap == Gap::kNaT) return "NaTType";
    if (cell.gap == Gap::kNaN) return "float";
    return python::type_name(cell.value);
  };
  std::vector<const std::vector<Cell>*> variants;
  std::vector<std::size_t> counts;
  std::map<std::vector<std::string>, std::size_t> index;
  for (const auto& tuple : traces) {
    std::vector<std::string> identity;
    for (const auto& cell : tuple) identity.push_back(cell.identity);
    const auto [it, added] = index.emplace(std::move(identity), variants.size());
    if (added) {
      variants.push_back(&tuple);
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
    const auto& x = *variants[a];
    const auto& y = *variants[b];
    for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
      if (x[i].identity == y[i].identity) continue;
      return less(x[i], y[i]);
    }
    return x.size() < y.size();
  });
}

// The log to replay with "concept:name" in place of `key` (nullopt when the
// log is no CSV upload's log shape or `key` names no column of its frame).
// Raises the transition sort's TypeError (with the mined heuristics net and
// its Petri net), then the variant sort's.
// The replay's sort of the heuristics net's transitions by name, the net
// mined under the frame column `key` (whatever key the replay reads): a gap's
// node in a text or object column is a transition named NaN.
inline void check_csv_keyed_heuristic_transitions(const EventLog& log, const FrameKeyedActivities& keyed,
                                                  const std::string& key, const HeuristicsNet& heuristics,
                                                  const PetriNet& net) {
  const auto frame = dataframe_frame_event_log(log);
  const auto column = frame_column_type(frame, key);
  const bool text_column = column && (*column == DataFrameColumnType::kString || *column == DataFrameColumnType::kObject);
  if (keyed.any_missing && text_column) {
    const auto at = std::find(heuristics.activities.begin(), heuristics.activities.end(), std::string("nan"));
    // The other transitions' names: silent ones are text, a node's its cell
    // (bools and numbers order beside NaN).
    bool text_name = std::any_of(net.transitions().begin(), net.transitions().end(),
                                 [](const auto& transition) { return transition.silent(); });
    for (std::size_t t = 0; t < keyed.log.traces.size() && !text_name; ++t)
      for (std::size_t e = 0; e < keyed.types[t].size(); ++e)
        if (keyed.types[t][e].empty() && !keyed.missing[t][e]) text_name = true;
    if (at != heuristics.activities.end() && text_name) {
      auto named = heuristics;
      named.null_activity = static_cast<std::size_t>(at - heuristics.activities.begin());
      check_heuristics_null_transition_names(named, net);
    }
  }
}

// `sort_variants` false leaves out the variant sort (pm4py's alignments read
// the variants unsorted, round HH).
inline std::optional<EventLog> csv_log_keyed_replay_log(const EventLog& log, const std::string& key,
                                                        const HeuristicsNet* heuristics = nullptr,
                                                        const PetriNet* net = nullptr, bool sort_variants = true) {
  if (!log.dataframe_columns || !log.dataframe_columns->csv_text) return std::nullopt;
  auto keyed = frame_keyed_activities(log, key);
  if (!keyed) return std::nullopt;
  distinct_float_gap_labels(*keyed);
  const auto frame = dataframe_frame_event_log(log);
  if (frame.traces.size() != log.traces.size()) return std::nullopt;
  const auto column = frame_column_type(frame, key);
  if (heuristics && net) check_csv_keyed_heuristic_transitions(log, *keyed, key, *heuristics, *net);
  std::vector<std::vector<KeyedReplayCell>> cells;
  for (std::size_t t = 0; t < frame.traces.size(); ++t) {
    const auto& trace = frame.traces[t];
    std::vector<KeyedReplayCell> tuple;
    for (std::size_t e = 0; e < trace.events.size(); ++e) {
      KeyedReplayCell cell;
      const auto value = frame_scalar(frame, trace, trace.events[e], key, FrameKeyScope::kEventLog);
      if (!value || python::is_missing(*value)) {
        const bool dates = column && *column == DataFrameColumnType::kDate;
        cell.gap = dates ? KeyedReplayCell::Gap::kNaT : KeyedReplayCell::Gap::kNaN;
        // A float64 column's gaps are distinct objects, never equal.
        cell.identity = dates ? "NaT"
            : column && *column == DataFrameColumnType::kFloat64 ? "nan#" + std::to_string(t) + ":" + std::to_string(e)
            : "nan";
      } else {
        cell.value = *value;
        cell.identity = python::identity(*value);
      }
      tuple.push_back(std::move(cell));
    }
    cells.push_back(std::move(tuple));
  }
  if (sort_variants) check_keyed_replay_variant_sort(cells);
  return std::move(keyed->log);
}

// A CSV upload's log shape replayed under `replay_key` against a net mined
// under another column `model_key` (round HG, observed black-box):
// convert_to_event_log's cells match the transition named by an equal object
// (1 == 1.0 == True, '1' unequal to 1; a float64 column's gaps each its own
// NaN, a text column's one shared NaN, NaT one object). The model's labels
// are frame_keyed_activities' (a text column's cells under concept:name);
// the replayed cells take an equal model cell's label, else one no model
// label equals. Returns the keyed activities (types and gaps of the
// replayed cells) with the log to replay under concept:name; raises the
// variant sort's TypeError. nullopt when the log is no CSV upload's log
// shape or either key names no column of its frame.
inline std::optional<FrameKeyedActivities> csv_log_keyed_replay_in(const EventLog& log, const std::string& replay_key,
                                                                   const std::string& model_key,
                                                                   bool sort_variants = true) {
  if (!log.dataframe_columns || !log.dataframe_columns->csv_text) return std::nullopt;
  const auto standard = [](const std::string& k) { return k.empty() || k == "concept:name" || k == "activity"; };
  if (standard(replay_key) && standard(model_key)) return std::nullopt;
  const auto frame = dataframe_frame_event_log(log);
  if (frame.traces.size() != log.traces.size()) return std::nullopt;
  // One cell's identity (equal objects alike) and printed text.
  struct Cell {
    std::string identity, shown, type;
    bool gap = false;
    python::Scalar value;
  };
  const auto cells_of = [&](const std::string& key) -> std::optional<std::vector<std::vector<Cell>>> {
    std::vector<std::vector<Cell>> out(frame.traces.size());
    const auto column = standard(key) ? std::optional<DataFrameColumnType>(DataFrameColumnType::kString)
                                      : frame_column_type(frame, key);
    if (!column) return std::nullopt;
    for (std::size_t t = 0; t < frame.traces.size(); ++t) {
      if (frame.traces[t].events.size() != log.traces[t].events.size()) return std::nullopt;
      for (std::size_t e = 0; e < frame.traces[t].events.size(); ++e) {
        Cell cell;
        if (standard(key)) {
          cell.value = python::text(log.traces[t].events[e].activity);
        } else if (const auto value = frame_scalar(frame, frame.traces[t], frame.traces[t].events[e], key,
                                                   FrameKeyScope::kEventLog);
                   value && !python::is_missing(*value)) {
          cell.value = *value;
        } else {
          cell.gap = true;
          const bool dates = *column == DataFrameColumnType::kDate;
          cell.type = dates ? "NaTType" : "float";
          cell.shown = dates ? "NaT" : "nan";
          cell.identity = dates ? "NaT"
              : *column == DataFrameColumnType::kFloat64 ? key + "#nan#" + std::to_string(t) + ":" + std::to_string(e)
              : "nan";
          out[t].push_back(std::move(cell));
          continue;
        }
        cell.identity = python::identity(cell.value);
        cell.shown = cell.value.kind == python::Scalar::Kind::kText ? cell.value.text : python::str(cell.value);
        if (cell.value.kind != python::Scalar::Kind::kText) cell.type = python::type_name(cell.value);
        out[t].push_back(std::move(cell));
      }
    }
    return out;
  };
  const auto model = cells_of(model_key);
  const auto replay = cells_of(replay_key);
  if (!model || !replay) return std::nullopt;
  // The model's labels as its miner names them.
  std::optional<FrameKeyedActivities> model_keyed;
  if (!standard(model_key)) {
    model_keyed = frame_keyed_activities(log, model_key);
    if (!model_keyed) return std::nullopt;
    distinct_float_gap_labels(*model_keyed);
  }
  std::map<std::string, std::string> label_of;
  std::set<std::string> labels;
  for (std::size_t t = 0; t < model->size(); ++t)
    for (std::size_t e = 0; e < (*model)[t].size(); ++e) {
      const auto& label = model_keyed ? model_keyed->log.traces[t].events[e].activity : (*model)[t][e].shown;
      label_of.emplace((*model)[t][e].identity, label);
      labels.insert(label);
    }
  FrameKeyedActivities out;
  out.log = log;
  out.types.resize(log.traces.size());
  out.missing.resize(log.traces.size());
  std::map<std::string, std::string> fresh;
  std::vector<std::vector<KeyedReplayCell>> sorted_cells(log.traces.size());
  for (std::size_t t = 0; t < replay->size(); ++t)
    for (std::size_t e = 0; e < (*replay)[t].size(); ++e) {
      const auto& cell = (*replay)[t][e];
      std::string label;
      if (const auto found = label_of.find(cell.identity); found != label_of.end()) {
        label = found->second;
      } else if (const auto made = fresh.find(cell.identity); made != fresh.end()) {
        label = made->second;
      } else {
        label = cell.shown;
        while (labels.contains(label)) label += '\x1f';
        labels.insert(label);
        fresh.emplace(cell.identity, label);
      }
      auto& event = out.log.traces[t].events[e];
      event.activity = label;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key.clear();
      event.activity_attribute.reset();
      out.types[t].push_back(cell.type);
      out.missing[t].push_back(cell.gap);
      out.any_missing = out.any_missing || cell.gap;
      KeyedReplayCell sorted;
      if (cell.gap) {
        sorted.gap = cell.type == "NaTType" ? KeyedReplayCell::Gap::kNaT : KeyedReplayCell::Gap::kNaN;
      } else {
        sorted.value = cell.value;
      }
      sorted.identity = cell.identity;
      sorted_cells[t].push_back(std::move(sorted));
    }
  if (sort_variants) check_keyed_replay_variant_sort(sorted_cells);
  return out;
}

// The same on an XES upload's log shape keyed by an event attribute (round
// HF): the replay reads read_xes' legacy objects (legacy_keyed_activities),
// each NaN float its own; nullopt where that model does not apply.
inline std::optional<EventLog> legacy_log_keyed_replay_log(const EventLog& log, const std::string& key) {
  auto keyed = legacy_keyed_activities(log, key);
  if (!keyed || keyed->any_absent || keyed->any_dict) return std::nullopt;
  std::vector<std::vector<KeyedReplayCell>> cells;
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    std::vector<KeyedReplayCell> tuple;
    for (std::size_t e = 0; e < log.traces[t].events.size(); ++e) {
      const auto legacy = legacy_declare_cell(log.traces[t].events[e], key);
      KeyedReplayCell cell;
      cell.type = legacy.type;
      if (keyed->missing[t][e]) {
        cell.gap = KeyedReplayCell::Gap::kNaN;
        cell.identity = "nan#" + std::to_string(t) + ":" + std::to_string(e);
      } else {
        cell.value = legacy.value;
        cell.identity = python::identity(legacy.value);
      }
      tuple.push_back(std::move(cell));
    }
    cells.push_back(std::move(tuple));
  }
  check_keyed_replay_variant_sort(cells);
  return std::move(keyed->log);
}

}  // namespace minera::detail
