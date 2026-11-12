// Review BM1: pm4py's DataFrame DFG adapters on a CSV upload's frame (a
// blank start is NaT, the pandas errors on frames without a start or without
// pairs, the case-only sort as numpy's introsort, nanosecond comparisons,
// Timestamp.timestamp() overlaps, NaN cases meeting in the self-merge), the
// temporal profile on a CSV with its own "@@index" column (format_dataframe
// overwrites it), and the Declare frame slices of a table whose rows without
// a case stay their own cases. Values are taken from pm4py 2.7.22 black-box
// on the same inputs (review-harness round-bm, bm1/mk.py).
#include "minera/declare.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/event_data.hpp"
#include "minera/temporal_profile.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace minera;

namespace {

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

std::string number(double value) {
  char text[64];
  const auto end = std::to_chars(text, text + sizeof text, value).ptr;
  return std::string(text, end);
}

std::optional<EventAttribute> cell(std::string key, std::string value, AttributeKind kind = AttributeKind::kString) {
  return EventAttribute{std::move(key), std::move(value), kind};
}

std::optional<EventAttribute> null_cell(std::string key, std::string surrogate = {}) {
  EventAttribute attribute{std::move(key), std::move(surrogate), AttributeKind::kString};
  attribute.is_null = true;
  return attribute;
}

// CSV cells as the upload's frame holds them: "" is a blank cell.
struct Row {
  const char* id;
  const char* activity;
  const char* completion;
  const char* start;
};

EventTable csv_table(const std::vector<Row>& rows, bool with_start = true) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  if (with_start) table.columns.push_back("start");
  for (const auto& row : rows) {
    EventTable::Row cells{cell("case:concept:name", row.id), cell("concept:name", row.activity),
                          cell("time:timestamp", row.completion)};
    if (with_start) cells.push_back(cell("start", row.start));
    table.rows.push_back(std::move(cells));
  }
  return table;
}

std::string error_of(const auto& call) {
  try {
    (void)call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "no error";
}

DfgTableOptions with_start(DfgTableMeasure measure = DfgTableMeasure::kFrequency) {
  DfgTableOptions options;
  options.measure = measure;
  options.start_timestamp_attribute = "start";
  return options;
}

std::string performance(const DfgTableResult& result) {
  std::string text;
  for (const auto& [pair, statistics] : result.performance)
    text += (text.empty() ? "" : " ") + pair.first + "," + pair.second + ":" + number(statistics.summary.mean);
  return text;
}

int graph() {
  int failures = 0;
  // A blank start is NaT: the pair's flow is NaN, clamped to 0 (before: the
  // blank cell raised as an unparsable time).
  failures += check("performance DFG (blank start)",
                    performance(discover_pandas_dfg(csv_table({{"c0", "a", "2020-01-01 00:02:50", "2020-01-01 00:02:50"},
                                                               {"c0", "b", "2020-01-01 00:07:50", ""}}),
                                                    with_start(DfgTableMeasure::kPerformance))),
                    "a,b:0");
  // No row with a start: pandas reads the column as floats.
  failures += check("performance DFG (no start)",
                    error_of([] {
                      return discover_pandas_dfg(csv_table({{"c0", "a", "2020-01-01 00:02:50", ""},
                                                            {"c0", "b", "2020-01-01 00:07:50", ""}}),
                                                 with_start(DfgTableMeasure::kPerformance));
                    }),
                    "'>=' not supported between instances of 'float' and 'Timestamp'");
  // Starts but no directly-follows pair: the empty merge's columns are objects.
  failures += check("performance DFG (no pairs)",
                    error_of([] {
                      return discover_pandas_dfg(csv_table({{"c0", "a", "2020-01-01 00:02:50", "2020-01-01 00:02:40"},
                                                            {"c1", "b", "2020-01-01 00:07:50", "2020-01-01 00:07:40"}}),
                                                 with_start(DfgTableMeasure::kPerformance));
                    }),
                    "cannot subtract DatetimeArray from ndarray[object]");
  // The case-only sort is numpy's (unstable) introsort: 20 rows of one case
  // leave in its order, not the file's.
  EventTable unsorted;
  unsorted.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  for (const int i : {7, 3, 15, 0, 11, 19, 4, 8, 13, 1, 17, 6, 10, 2, 18, 5, 14, 9, 16, 12}) {
    const auto name = std::string(i < 10 ? "a0" : "a") + std::to_string(i);
    const auto minute = (i * 7) % 60;
    unsorted.rows.push_back({cell("case:concept:name", "c0"), cell("concept:name", name),
                             cell("time:timestamp", "2020-01-01 00:" + std::string(minute < 10 ? "0" : "") +
                                                        std::to_string(minute) + ":00")});
  }
  unsorted.rows.push_back({cell("case:concept:name", "c1"), cell("concept:name", "b"),
                           cell("time:timestamp", "2020-01-01 00:00:00")});
  DfgTableOptions case_only;
  case_only.sort_timestamp_along_case_id = false;
  std::string edges;
  for (const auto& [pair, count] : discover_pandas_dfg(unsorted, case_only).frequency)
    edges += (edges.empty() ? "" : " ") + pair.first + "," + pair.second;
  failures += check("frequency DFG (case-only sort)", edges,
                    "a00,a15 a02,a10 a03,a01 a04,a19 a05,a18 a06,a12 a07,a16 a08,a04 a09,a14 a10,a06 a11,a00 "
                    "a12,a17 a13,a08 a14,a05 a15,a03 a16,a09 a17,a13 a18,a02 a19,a11");
  return failures;
}

int partial_and_concurrent() {
  int failures = 0;
  failures += check("partial order (no start)",
                    error_of([] {
                      return discover_pandas_partial_order(csv_table({{"c0", "a", "2020-01-01 00:02:50", ""},
                                                                      {"c0", "b", "2020-01-01 00:07:50", ""}}),
                                                           with_start());
                    }),
                    "Invalid comparison between dtype=datetime64[us, UTC] and ndarray");
  failures += check("concurrent events (no candidate)",
                    error_of([] {
                      return discover_pandas_concurrent_events(
                          csv_table({{"c0", "a", "2020-01-01 00:02:50", "2020-01-01 00:02:40"},
                                     {"c1", "b", "2020-01-01 00:07:50", "2020-01-01 00:07:40"}}),
                          with_start());
                    }),
                    "Invalid comparison between dtype=timedelta64[us] and int");
  // An empty frame (every row of the upload without a case): its own start
  // column is a float column, a start read from the completion a datetime one.
  failures += check("partial order (empty frame, start column)",
                    error_of([] { return discover_pandas_partial_order(csv_table({}), with_start()); }),
                    "Invalid comparison between dtype=datetime64[us, UTC] and ndarray");
  failures += check("concurrent events (empty frame, no start column)",
                    error_of([] { return discover_pandas_concurrent_events(csv_table({}, false), {}); }),
                    "Invalid comparison between dtype=timedelta64[us] and int");
  failures += check("concurrent events (empty frame, start column)",
                    error_of([] { return discover_pandas_concurrent_events(csv_table({}), with_start()); }),
                    "no error");
  // A nanosecond completion after the next start: no partial order (before:
  // both read as microseconds, equal, so ordered).
  failures += check("partial order (nanoseconds)",
                    std::to_string(discover_pandas_partial_order(
                                       csv_table({{"c0", "a", "2020-01-01 00:02:50.000000001", "2020-01-01 00:02:40"},
                                                  {"c0", "b", "2020-01-01 00:03:00", "2020-01-01 00:02:50"}}),
                                       with_start())
                                       .pairs.size()),
                    "0");
  // The overlap is the difference of Timestamp.timestamp() floats.
  std::string overlaps;
  for (const auto& pair : discover_pandas_concurrent_events(
                              csv_table({{"c0", "a", "2020-01-01 00:00:00.700001", "2020-01-01 00:00:00.1"},
                                         {"c0", "b", "2020-01-01 00:00:00.9", "2020-01-01 00:00:00.300003"}}),
                              with_start())
                              .pairs)
    overlaps += pair.left.activity + "," + pair.right.activity + ":" + number(pair.overlap_seconds);
  failures += check("concurrent events (overlap)", overlaps, "a,b:0.3999979496002197");
  // Rows without a case meet in pandas' self-merge (NaN matches NaN).
  EventTable unnamed;
  unnamed.columns = {"case:concept:name", "concept:name", "time:timestamp", "start"};
  const char* starts[] = {"2020-01-01 00:00:05", "2020-01-01 00:00:15", "2020-01-01 00:00:25"};
  const char* ends[] = {"2020-01-01 00:00:10", "2020-01-01 00:00:20", "2020-01-01 00:00:30"};
  for (int i = 0; i < 3; ++i)
    unnamed.rows.push_back({i < 2 ? std::optional<EventAttribute>{} : cell("case:concept:name", "c1"),
                            cell("concept:name", std::string(1, static_cast<char>('a' + i))),
                            cell("time:timestamp", ends[i]), cell("start", starts[i])});
  std::string flows;
  for (const auto& pair : discover_pandas_partial_order(unnamed, with_start()).pairs)
    flows += pair.source.activity + "," + pair.target.activity + ":" + number(pair.flow_time_seconds);
  failures += check("partial order (NaN cases)", flows, "a,b:5");
  unnamed.rows[1][3] = cell("start", "2020-01-01 00:00:08");
  overlaps.clear();
  for (const auto& pair : discover_pandas_concurrent_events(unnamed, with_start()).pairs)
    overlaps += pair.left.activity + "," + pair.right.activity + ":" + number(pair.overlap_seconds);
  failures += check("concurrent events (NaN cases)", overlaps, "a,b:2");
  return failures;
}

int csv_own_index() {
  // A CSV with its own "@@index" column: format_dataframe overwrites it, so
  // the frame orders the rows by the upload timestamp (d, a, b, c), not by
  // the file's index (c, b, d, a).
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start", "@@index"};
  const char* cells[][4] = {{"a", "10", "05", "5"}, {"b", "20", "01", "1"}, {"c", "30", "40", "0"}, {"d", "07", "50", "3"}};
  for (const auto& row : cells)
    table.rows.push_back({cell("case:concept:name", "c1"), cell("concept:name", row[0]),
                          cell("time:timestamp", std::string("2020-01-01 00:00:") + row[1]),
                          cell("start", std::string("2020-01-01 00:00:") + row[2]),
                          cell("@@index", row[3], AttributeKind::kInt)});
  TemporalProfileOptions options;
  options.target_timestamp_attribute = "start";
  options.frame_index_rows = true;
  std::vector<std::string> items;
  for (const auto& entry : discover_temporal_profile_dataframe(table, options).entries)
    items.push_back(entry.source + "," + entry.target + ":" + number(entry.mean_seconds) + "," +
                    number(entry.standard_deviation_seconds));
  std::sort(items.begin(), items.end());
  std::string seen;
  for (const auto& item : items) seen += (seen.empty() ? "" : " ") + item;
  return check("temporal profile (CSV's own @@index)", seen, "a,c:30,0 b,c:20,0 d,c:33,0");
}

int declare_caseless_rows() {
  // x's rows apart (sliced: b, c), and two rows without a case (XES traces
  // without a name, each its own surrogate): those stay their own cases, so
  // there is one result per case (before: the caseless rows were dropped and
  // the mine response failed). pm4py's DataFrame Declare refuses such a
  // frame ("the case ID column should not contain any empty value."; declared).
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows.push_back({cell("case:concept:name", "x"), cell("concept:name", "b"), cell("time:timestamp", "2020-01-01 00:00:01")});
  table.rows.push_back({null_cell("case:concept:name", "1"), cell("concept:name", "c"), cell("time:timestamp", "2020-01-01 00:00:02")});
  table.rows.push_back({cell("case:concept:name", "x"), cell("concept:name", "a"), cell("time:timestamp", "2020-01-01 00:00:03")});
  table.rows.push_back({null_cell("case:concept:name", "3"), cell("concept:name", "a"), cell("time:timestamp", "2020-01-01 00:00:04")});
  const EventDataView view(table);
  DeclareModel model;
  model.add(kDeclareExistence, DeclareConstraint{"a", "", false, 1, 1});
  std::string fitness;
  for (const auto& result : check_declare_conformance_frame_slices(view, model))
    fitness += (fitness.empty() ? "" : " ") + std::to_string(result.no_dev_total) + "/" + number(result.dev_fitness);
  return check("declare conformance (caseless rows)", fitness, "1/0 1/0 0/1");
}

}  // namespace

int main() {
  const int failures = graph() + partial_and_concurrent() + csv_own_index() + declare_caseless_rows();
  if (failures != 0) {
    std::cerr << failures << " review BM1 failure(s)\n";
    return 1;
  }
  std::cout << "review BM1 tests passed\n";
  return 0;
}
