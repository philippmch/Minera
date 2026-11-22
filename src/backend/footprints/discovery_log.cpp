#include "discovery_registry.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "../polars_column_hint.hpp"
#include "../python_str_set.hpp"

namespace minera::footprints_detail {
namespace {

std::string require_selector(std::string_view value,
                             std::string_view description) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    throw std::invalid_argument(std::string(description) +
                                " selector must not be empty");
  // Selectors name source columns exactly.  In particular, retaining leading
  // or trailing whitespace prevents a typo from silently selecting a default
  // column with a different spelling.
  return std::string(value);
}

const EventAttribute* find_event_attribute(const Event& event,
                                           std::string_view key) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return &attribute;
  return nullptr;
}

std::string selected_activity(const Event& event,
                              const DiscoveryParameters& parameters) {
  const auto key = require_selector(parameters.activity_key, "activity");
  if (key == "concept:name") {
    // The projection is authoritative for manually constructed events, while
    // imported records also retain the selected attribute for round-tripping.
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_attribute && event.activity_attribute->is_null))
      throw std::invalid_argument("activity selector contains a null event");
    if (!event.has_activity())
      throw std::invalid_argument("activity selector is missing from an event");
    return event.activity;
  }
  if (event.activity_source_key == key) {
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_attribute && event.activity_attribute->is_null))
      throw std::invalid_argument("activity selector contains a null event");
    if (!event.has_activity())
      throw std::invalid_argument("activity selector is missing from an event");
    return event.activity;
  }
  const auto* attribute = find_event_attribute(event, key);
  if (!attribute || attribute->is_null)
    throw std::invalid_argument("activity selector is missing from an event");
  return attribute->value;
}

void increment(std::uint64_t& target, std::string_view description) {
  if (target == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error(std::string(description) + " count overflow");
  ++target;
}

void derive_relations(Footprint& footprint) {
  for (const auto& [pair, count] : footprint.dfg) {
    (void)count;
    if (footprint.dfg.find({pair.second, pair.first}) !=
        footprint.dfg.end()) {
      footprint.parallel.insert(pair);
    } else {
      footprint.sequence.insert(pair);
    }
  }
}

Footprint from_trace(const Trace& trace,
                     const DiscoveryParameters& parameters) {
  Footprint footprint;
  footprint.min_trace_length = trace.events.size();
  footprint.trace.emplace();
  footprint.trace->reserve(trace.events.size());
  for (const auto& event : trace.events) {
    const auto activity = selected_activity(event, parameters);
    footprint.trace->push_back(activity);
    footprint.activities.insert(activity);
  }
  if (!footprint.trace->empty()) {
    footprint.start_activities.insert(footprint.trace->front());
    footprint.end_activities.insert(footprint.trace->back());
    for (std::size_t i = 1; i < footprint.trace->size(); ++i)
      increment(footprint.dfg[{(*footprint.trace)[i - 1],
                               (*footprint.trace)[i]}],
               "footprint DFG");
  }
  derive_relations(footprint);
  return footprint;
}

const EventAttribute* cell_value(const EventTable::Row& row,
                                 std::size_t column) {
  if (column >= row.size() || !row[column] || row[column]->is_null)
    return nullptr;
  return &*row[column];
}

std::optional<std::size_t> exact_column(const EventTable& table,
                                        std::string_view name) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  return static_cast<std::size_t>(found - table.columns.begin());
}

std::size_t resolve_column(const EventTable& table, std::string_view selector,
                           std::string_view description) {
  const auto exact_name = require_selector(selector, description);
  if (const auto exact = exact_column(table, exact_name)) return *exact;
  throw std::invalid_argument(std::string(description) + " selector '" +
                              exact_name + "' is not a table column");
}

struct TableRow {
  std::size_t input_order = 0;
  std::string case_id;
  std::string activity;
  // Only the lazy adapter accepts null case ids and activities.
  bool case_null = false;
  bool activity_null = false;
  std::string timestamp;
  std::optional<double> timestamp_value;
  bool timestamp_present = false;
  std::string start_timestamp;
  std::optional<double> start_timestamp_value;
  bool start_timestamp_present = false;
  Event event;
};

std::optional<double> parse_sort_timestamp(const TableRow& row) {
  if (row.timestamp.empty()) return std::nullopt;
  return parse_timestamp(row.timestamp);
}

// pm4py's frame variants name the columns a frame lacks (round FX, observed
// black-box): the pandas variant selects [case, activity, timestamp], so a
// KeyError names the missing labels in that order (or the whole selection
// when none is there); the Polars variant names every missing column of
// those and the start timestamp, sorted, in pm4py's own Exception.
void require_footprint_columns(const EventTable& table, const std::string& case_key,
                               const std::string& activity_key, const std::string& timestamp_key,
                               const std::optional<std::string>& start_key, bool lazy) {
  const std::set<std::string> columns(table.columns.begin(), table.columns.end());
  if (!lazy) {
    require_frame_columns(columns, {case_key, activity_key, timestamp_key});
    return;
  }
  std::set<std::string> missing;
  for (const auto* key : {&case_key, &activity_key, &timestamp_key})
    if (!columns.contains(*key)) missing.insert(*key);
  if (start_key && !columns.contains(*start_key)) missing.insert(*start_key);
  if (missing.empty()) return;
  std::string message = "The provided Polars LazyFrame does not contain the following required columns: ";
  bool first = true;
  for (const auto& key : missing) {
    if (!first) message += ", ";
    message += key;
    first = false;
  }
  throw std::invalid_argument(message);
}

std::string polars_quoted(const std::string& name) {
  std::string out = "\"";
  for (const char c : name) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

// The Polars variant's plan (round FX, observed black-box by tracing its
// LazyFrame calls): select the set {case, activity, timestamp[, start]},
// add a row index, sort by [case, start, timestamp, @@index] (without a
// start key by [case, timestamp, @@index], then alias the timestamp as
// start_timestamp; without sort_required neither the index nor the sort),
// select the set {case, activity}, and shift [case,
// activity] within each case into "<key>_2" columns, plus the start key
// when it names the case, activity or timestamp column and then the
// timestamp when it names the case or activity column. Polars resolves that
// with_columns at the first collect (the DFG's group_by): a shifted column
// the second select dropped is a ColumnNotFoundError, two shifts of one
// column a duplicate name. The sets list their columns in the order of
// Python's hash seed; Minera follows seed 0.
void polars_footprints_plan_check(const EventTable& table, const std::string& case_key,
                                  const std::string& activity_key, const std::string& timestamp_key,
                                  const std::optional<std::string>& start_key, bool sort_required) {
  std::vector<std::string> shifted{case_key, activity_key};
  if (start_key && (*start_key == case_key || *start_key == activity_key || *start_key == timestamp_key))
    shifted.push_back(*start_key);
  if (timestamp_key == case_key || timestamp_key == activity_key) shifted.push_back(timestamp_key);
  std::optional<std::string> missing, duplicate;
  for (std::size_t i = 0; i < shifted.size(); ++i) {
    if (shifted[i] != case_key && shifted[i] != activity_key) {
      if (!missing) missing = shifted[i];
    } else if (!duplicate && std::find(shifted.begin(), shifted.begin() + static_cast<std::ptrdiff_t>(i),
                                       shifted[i]) != shifted.begin() + static_cast<std::ptrdiff_t>(i)) {
      duplicate = shifted[i];
    }
  }
  if (!missing && !duplicate) return;
  const auto kept = detail::python_str_set_order({case_key, activity_key});
  std::vector<std::string> selected{case_key, activity_key, timestamp_key};
  if (start_key) selected.push_back(*start_key);
  selected = detail::python_str_set_order(selected);
  std::vector<std::string> sort_keys{case_key};
  if (start_key) sort_keys.push_back(*start_key);
  sort_keys.push_back(timestamp_key);
  sort_keys.push_back("@@index");
  const auto cols = [](const std::vector<std::string>& names) {
    std::string out;
    for (const auto& name : names) out += (out.empty() ? "" : ", ") + std::string("col(") + polars_quoted(name) + ")";
    return out;
  };
  std::string frame = "DF [";
  for (std::size_t i = 0; i < std::min<std::size_t>(4, table.columns.size()); ++i) {
    if (i) frame += ", ";
    frame += polars_quoted(table.columns[i]);
  }
  if (table.columns.size() > 4) frame += ", ...";
  frame += "]; PROJECT */" + std::to_string(table.columns.size()) + " COLUMNS";
  std::string plan = "SELECT [" + cols(kept) + "]\n";
  std::string pad = "  ";
  if (!start_key) {
    plan += "   WITH_COLUMNS:\n   [col(" + polars_quoted(timestamp_key) + ").alias(\"start_timestamp\")] \n";
    pad = "    ";
  }
  if (sort_required) {
    plan += pad + "SORT BY [" + cols(sort_keys) + "]\n" + pad + "  ROW INDEX name: @@index, offset: 0\n";
    pad += "    ";
  }
  plan += pad + "SELECT [" + cols(selected) + "]\n" + pad + "  " + frame;
  std::string message;
  if (missing) {
    message = "unable to find column " + polars_quoted(*missing) + "; valid columns: [";
    for (std::size_t i = 0; i < kept.size(); ++i) message += (i ? ", " : "") + polars_quoted(kept[i]);
    message += "]";
    if (const auto hint = detail::polars_column_hint(*missing, kept))
      message += "\n\nDid you mean " + polars_quoted(*hint) + "?";
  } else {
    message = "the name '" + *duplicate +
              "_2' passed to `LazyFrame.with_columns` is duplicate\n\nIt's possible that multiple expressions are "
              "returning the same default column name. If this is the case, try renaming the columns with "
              "`.alias(\"new_name\")` to avoid duplicate column names.";
  }
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n" + plan +
             "\n\nThis error occurred with the following context stack:\n"
             "\t[1] 'with_columns'\n\t[2] 'filter'\n\t[3] 'filter'\n\t[4] 'group_by'\n";
  throw std::invalid_argument(message);
}

// The pandas variant with two of the case, activity and timestamp keys
// naming one column: its frame selection holds that label twice, so sorting
// by it fails, or without sorting the case grouping (round FX, observed
// black-box; the start key plays no part).
void pandas_footprints_label_check(const std::string& case_key, const std::string& activity_key,
                                   const std::string& timestamp_key, bool sort_required) {
  std::optional<std::string> duplicate;
  if (case_key == activity_key || case_key == timestamp_key) duplicate = case_key;
  else if (activity_key == timestamp_key) duplicate = activity_key;
  if (!duplicate) return;
  throw std::invalid_argument(sort_required ? "ValueError: The column label '" + *duplicate + "' is not unique."
                                            : "ValueError: Grouper for '" + *duplicate + "' not 1-dimensional");
}

std::vector<TableRow> read_table_rows(const EventTable& table,
                                      const DiscoveryParameters& parameters,
                                      bool lazy_ordering) {
  table.validate();
  const auto case_key = require_selector(parameters.case_id_key, "case");
  const auto activity_key = require_selector(parameters.activity_key, "activity");
  const auto timestamp_key = require_selector(parameters.timestamp_key,
                                              "timestamp");
  std::optional<std::string> start_key;
  if (lazy_ordering && parameters.start_timestamp_key)
    start_key = require_selector(*parameters.start_timestamp_key, "start timestamp");
  require_footprint_columns(table, case_key, activity_key, timestamp_key, start_key, lazy_ordering);
  if (lazy_ordering)
    polars_footprints_plan_check(table, case_key, activity_key, timestamp_key, start_key, parameters.sort_required);
  else pandas_footprints_label_check(case_key, activity_key, timestamp_key, parameters.sort_required);
  const auto case_column = resolve_column(table, case_key, "case");
  const auto activity_column = resolve_column(table, activity_key, "activity");
  const auto timestamp_column = resolve_column(table, timestamp_key, "timestamp");
  std::optional<std::size_t> start_timestamp_column;
  if (start_key) start_timestamp_column = resolve_column(table, *start_key, "start timestamp");

  // Without rows the Polars variant takes the int of a missing minimum trace
  // length (round FX, observed black-box); the pandas variant's NaN minimum
  // is refused in footprint_from_table_rows.
  if (lazy_ordering && table.rows.empty())
    throw std::invalid_argument(
        "TypeError: int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");

  std::vector<TableRow> rows;
  rows.reserve(table.rows.size());
  for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
    const auto& row = table.rows[row_index];
    TableRow item;
    item.input_order = row_index;
    // Both variants keep rows without a case id or an activity (round FX,
    // observed black-box: the pandas variant reads them as NaN).
    if (const auto* case_cell = cell_value(row, case_column))
      item.case_id = case_cell->value;
    else
      item.case_null = true;
    const auto* activity_cell = cell_value(row, activity_column);
    item.activity_null = !activity_cell;
    if (activity_cell) item.activity = activity_cell->value;
    const auto timestamp_cell = cell_value(row, timestamp_column);
    if (timestamp_cell) {
      item.timestamp = timestamp_cell->value;
      item.timestamp_present = true;
    }
    item.timestamp_value = parse_sort_timestamp(item);
    if (start_timestamp_column) {
      const auto start_cell = cell_value(row, *start_timestamp_column);
      if (start_cell) {
        item.start_timestamp = start_cell->value;
        item.start_timestamp_present = true;
        item.start_timestamp_value = parse_timestamp(item.start_timestamp);
      }
    }
    // The index selector is part of the adapter surface, but neither source
    // engine uses it to order footprint rows. Missing, null, and blank index
    // selectors are therefore intentionally ignored.

    item.event.case_id = item.case_id;
    item.event.case_id_state = FirstClassAttributeState::kPresent;
    item.event.case_id_source_key = case_key;
    item.event.activity = item.activity;
    item.event.activity_state = FirstClassAttributeState::kPresent;
    item.event.activity_source_key = activity_key;
    item.event.timestamp = item.timestamp;
    item.event.timestamp_state = item.timestamp_present
                                     ? FirstClassAttributeState::kPresent
                                     : FirstClassAttributeState::kNull;
    item.event.timestamp_source_key = timestamp_key;
    item.event.input_order = row_index;
    for (std::size_t column = 0; column < row.size(); ++column) {
      if (!row[column]) continue;
      auto attribute = *row[column];
      attribute.key = table.columns[column];
      item.event.attributes.push_back(attribute);
    }
    rows.push_back(std::move(item));
  }

  if (parameters.sort_required) {
    const auto compare_timestamp = [lazy_ordering](
        const std::string& left_raw, bool left_present,
        const std::optional<double>& left, const std::string& right_raw,
        bool right_present, const std::optional<double>& right) {
      // The eager and lazy source engines place null-like timestamp cells on
      // opposite sides of valid values. Present empty text is distinct from
      // a null/missing cell and follows the source engine's own empty-value
      // position. Invalid non-empty text is kept deterministic by lexical
      // comparison after valid values.
      const auto rank = [lazy_ordering](const std::string& raw,
                                        bool present,
                                        const std::optional<double>& value) {
        if (!present) return lazy_ordering ? 0 : 3;
        if (raw.empty()) return lazy_ordering ? 1 : 0;
        if (value) return lazy_ordering ? 2 : 1;
        return lazy_ordering ? 3 : 2;
      };
      const auto left_rank = rank(left_raw, left_present, left);
      const auto right_rank = rank(right_raw, right_present, right);
      if (left_rank != right_rank)
        return left_rank < right_rank ? -1 : 1;
      if (left && right && *left != *right)
        return *left < *right ? -1 : 1;
      if (left && right) {
        // Equal doubles can still be distinct nanosecond instants.
        const auto a = parse_timestamp_exact(left_raw);
        const auto b = parse_timestamp_exact(right_raw);
        if (a && b && (a->seconds != b->seconds || a->nanoseconds != b->nanoseconds))
          return std::pair(a->seconds, a->nanoseconds) < std::pair(b->seconds, b->nanoseconds) ? -1 : 1;
      }
      if (!left && !right && left_raw != right_raw)
        return left_raw < right_raw ? -1 : 1;
      return 0;
    };
    const bool use_start_timestamp = lazy_ordering &&
                                      start_timestamp_column.has_value();
    std::stable_sort(rows.begin(), rows.end(), [use_start_timestamp,
                                                compare_timestamp](
                                                   const TableRow& left,
                                                   const TableRow& right) {
      if (left.case_null != right.case_null) return left.case_null;
      if (left.case_id != right.case_id) return left.case_id < right.case_id;
      const auto order = use_start_timestamp
                             ? compare_timestamp(
                                   left.start_timestamp,
                                   left.start_timestamp_present,
                                   left.start_timestamp_value,
                                   right.start_timestamp,
                                   right.start_timestamp_present,
                                   right.start_timestamp_value)
                             : compare_timestamp(left.timestamp,
                                                 left.timestamp_present,
                                                 left.timestamp_value,
                                                 right.timestamp,
                                                 right.timestamp_present,
                                                 right.timestamp_value);
      if (order != 0) return order < 0;
      if (use_start_timestamp) {
        const auto completion_order = compare_timestamp(
            left.timestamp, left.timestamp_present, left.timestamp_value,
            right.timestamp, right.timestamp_present, right.timestamp_value);
        if (completion_order != 0) return completion_order < 0;
      }
      // Stable sort preserves source order when both ordering keys are tied.
      return false;
    });
  }

  return rows;
}

using CaseKey = std::pair<bool, std::string>;

CaseKey case_key(const TableRow& row) { return {row.case_null, row.case_id}; }

// The pandas variant (`pandas`) groups by the case column, which leaves out
// the rows without a case id (their activities still count), takes each
// case's first and last activity that is not NaN (NaN when the case has
// none) and the minimum group size, whose NaN without any case it cannot
// convert to an int; the Polars variant keeps a null case as a group and a
// null activity as its first or last (round FX, observed black-box).
Footprint footprint_from_table_rows(const std::vector<TableRow>& rows, bool pandas = false) {
  Footprint result;
  result.min_trace_length = 0;
  const auto grouped = [pandas](const TableRow& row) { return !pandas || !row.case_null; };
  if (pandas && std::none_of(rows.begin(), rows.end(), grouped))
    throw std::invalid_argument("ValueError: cannot convert float NaN to integer");
  if (rows.empty()) return result;

  std::map<CaseKey, std::size_t> case_lengths;
  for (const auto& row : rows) {
    if (grouped(row)) ++case_lengths[case_key(row)];
    if (row.activity_null)
      result.activities_include_null = true;
    else
      result.activities.insert(row.activity);
  }
  bool first_length = true;
  for (const auto& [case_id, length] : case_lengths) {
    (void)case_id;
    if (first_length || length < *result.min_trace_length) {
      result.min_trace_length = length;
      first_length = false;
    }
  }

  if (pandas) {
    // Each case's first and last row with an activity.
    std::map<CaseKey, std::pair<std::optional<std::size_t>, std::optional<std::size_t>>> ends;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (!grouped(rows[i])) continue;
      auto& [first, last] = ends[case_key(rows[i])];
      if (rows[i].activity_null) continue;
      if (!first) first = i;
      last = i;
    }
    for (const auto& [key, positions] : ends) {
      (void)key;
      if (positions.first) result.start_activities.insert(rows[*positions.first].activity);
      else result.start_activities_include_null = true;
      if (positions.second) result.end_activities.insert(rows[*positions.second].activity);
      else result.end_activities_include_null = true;
    }
  }
  std::map<CaseKey, std::size_t> last_position;
  for (std::size_t i = 0; i < rows.size(); ++i)
    last_position[case_key(rows[i])] = i;
  std::set<CaseKey> first_seen;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    if (!pandas && first_seen.insert(case_key(row)).second) {
      if (row.activity_null)
        result.start_activities_include_null = true;
      else
        result.start_activities.insert(row.activity);
    }
    if (!pandas && last_position.at(case_key(row)) == i) {
      if (row.activity_null)
        result.end_activities_include_null = true;
      else
        result.end_activities.insert(row.activity);
    }
    // Directly-follows is derived only from physically adjacent rows that
    // belong to the same case. This preserves the unsorted adapter behavior
    // when cases are interleaved.
    // Null case ids and null activities never form a pair.
    if (i > 0 && !row.case_null && !rows[i - 1].case_null &&
        rows[i - 1].case_id == row.case_id && !row.activity_null &&
        !rows[i - 1].activity_null)
      increment(result.dfg[{rows[i - 1].activity, row.activity}],
               "footprint DFG");
  }
  derive_relations(result);
  return result;
}

}  // namespace

Footprint EntireEventLogFootprintsDiscovery::apply(
    const EventLog& log, const DiscoveryParameters& parameters) {
  // Validate the one selector this log-shaped adapter consumes even when the
  // log has no traces, so an empty input cannot hide a malformed option.
  (void)require_selector(parameters.activity_key, "activity");
  Footprint result;
  result.min_trace_length = 0;
  bool first = true;
  for (const auto& trace : log.traces) {
    if (first || trace.events.size() < *result.min_trace_length) {
      result.min_trace_length = trace.events.size();
      first = false;
    }
    if (trace.events.empty()) continue;
    result.start_activities.insert(
        selected_activity(trace.events.front(), parameters));
    result.end_activities.insert(
        selected_activity(trace.events.back(), parameters));
    std::string previous;
    bool have_previous = false;
    for (const auto& event : trace.events) {
      const auto activity = selected_activity(event, parameters);
      result.activities.insert(activity);
      if (have_previous)
        increment(result.dfg[{previous, activity}], "footprint DFG");
      previous = activity;
      have_previous = true;
    }
  }
  derive_relations(result);
  return result;
}

std::vector<Footprint> TraceByTraceFootprintsDiscovery::apply(
    const EventLog& log, const DiscoveryParameters& parameters) {
  (void)require_selector(parameters.activity_key, "activity");
  std::vector<Footprint> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces)
    result.push_back(from_trace(trace, parameters));
  return result;
}

Footprint EntireDataframeFootprintsDiscovery::apply(
    const EventTable& table, const DiscoveryParameters& parameters) {
  return footprint_from_table_rows(read_table_rows(table, parameters, false), true);
}

Footprint PolarsLazyframesFootprintsDiscovery::apply(
    const LazyEventTable& table, const DiscoveryParameters& parameters) {
  // Materialization is deliberately isolated at this adapter boundary. A
  // lazy source is scanned once, then follows the exact selector semantics
  // of the lazy adapter.
  const auto materialized = table.materialize();
  auto rows = read_table_rows(materialized, parameters, true);
  if (!parameters.sort_required) {
    // Lazy grouping preserves each case's row subsequence before flattening.
    // This differs intentionally from the eager table adapter, whose
    // unsorted mode considers only physically adjacent equal-case rows.
    std::map<std::pair<bool, std::string>, std::vector<TableRow>> by_case;
    for (const auto& row : rows)
      by_case[{!row.case_null, row.case_id}].push_back(row);
    std::vector<TableRow> grouped;
    grouped.reserve(rows.size());
    for (auto& [case_id, case_rows] : by_case) {
      (void)case_id;
      for (auto& row : case_rows) grouped.push_back(std::move(row));
    }
    rows = std::move(grouped);
  }
  return footprint_from_table_rows(rows);
}

Footprint DfgFootprintsDiscovery::apply(
    const std::map<FootprintPair, std::uint64_t>& dfg) {
  Footprint result;
  result.dfg_present = false;
  result.min_trace_length_present = false;
  result.min_trace_length.reset();
  for (const auto& [pair, count] : dfg) {
    (void)count;
    result.activities.insert(pair.first);
    result.activities.insert(pair.second);
    if (dfg.find({pair.second, pair.first}) != dfg.end())
      result.parallel.insert(pair);
    else
      result.sequence.insert(pair);
  }
  for (const auto& activity : result.activities) {
    bool incoming = false;
    bool outgoing = false;
    for (const auto& [pair, count] : dfg) {
      (void)count;
      incoming = incoming || pair.second == activity;
      outgoing = outgoing || pair.first == activity;
    }
    if (!incoming) result.start_activities.insert(activity);
    if (!outgoing) result.end_activities.insert(activity);
  }
  return result;
}

Footprint DfgFootprintsDiscovery::apply(const DirectlyFollowsGraph& graph) {
  std::map<FootprintPair, std::uint64_t> dfg;
  for (const auto& edge : graph.edges()) {
    if (edge.source >= graph.activities().size() ||
        edge.target >= graph.activities().size())
      throw std::invalid_argument("DFG edge points outside activity registry");
    dfg[{graph.activities()[edge.source], graph.activities()[edge.target]}] =
        edge.count;
  }
  auto result = apply(dfg);
  for (const auto& activity : graph.activities()) {
    if (graph.activity_count(activity) == 0) continue;
    result.activities.insert(activity);
    if (graph.start_activity_count(activity) > 0)
      result.start_activities.insert(activity);
    if (graph.end_activity_count(activity) > 0)
      result.end_activities.insert(activity);
  }
  return result;
}

}  // namespace minera::footprints_detail
