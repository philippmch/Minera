// Review BN1: pm4py's DataFrame functions on a CSV upload's frame. The
// batches read the frame's "@@index" as the default event id (two rows alike
// in every other value stay two events); the DFG adapters read the column
// dtypes format_dataframe gives before it drops the rows without a case, so
// an emptied frame keeps a datetime start column; without column reduction
// a NaN cell in any column drops its row from the concurrent events; the
// heuristics miner keyed by the CSV's own activity column. Values are taken
// from pm4py 2.7.22 black-box on the same inputs (review-harness round-bn,
// bn1/mk.py).
#include "minera/batches.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/heuristic_miner.hpp"

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

std::optional<EventAttribute> null_cell(std::string key) {
  EventAttribute attribute{std::move(key), {}, AttributeKind::kString};
  attribute.is_null = true;
  return attribute;
}

std::optional<EventAttribute> int_cell(std::string key, std::int64_t value) {
  EventAttribute attribute{std::move(key), std::to_string(value), AttributeKind::kInt};
  attribute.typed_value = value;
  return attribute;
}

std::string error_of(const auto& call) {
  try {
    (void)call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "no error";
}

// format_dataframe's frame of
//   case,act,ts,res
//   c1,a,2020-01-01 00:12:00,r1
//   c0,a,2020-01-01 00:10:00,r1
//   c1,a,2020-01-01 00:12:00,r1
//   c0,b,2020-01-01 00:20:00,r2
//   c2,b,2020-01-01 00:21:00,r2
// (rows by case, then timestamp, then file order).
EventTable batch_frame(bool with_index) {
  EventTable table;
  table.columns = {"case", "act", "ts", "res"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]", "str"};
  if (with_index) {
    table.columns.push_back("@@index");
    table.dtypes.push_back("int64");
  }
  const char* rows[][4] = {{"c0", "a", "2020-01-01 00:10:00", "r1"},
                           {"c0", "b", "2020-01-01 00:20:00", "r2"},
                           {"c1", "a", "2020-01-01 00:12:00", "r1"},
                           {"c1", "a", "2020-01-01 00:12:00", "r1"},
                           {"c2", "b", "2020-01-01 00:21:00", "r2"}};
  std::int64_t index = 0;
  for (const auto& row : rows) {
    EventTable::Row cells{cell("case", row[0]), cell("act", row[1]), cell("ts", row[2], AttributeKind::kDate),
                          cell("res", row[3])};
    if (with_index) cells.push_back(int_cell("@@index", index));
    ++index;
    table.rows.push_back(std::move(cells));
  }
  return table;
}

std::string batches_text(const BatchDiscovery& discovery) {
  std::vector<std::string> groups;
  for (const auto& group : discovery.groups) {
    for (const auto& batch : group.batches) {
      std::vector<std::string> events;
      for (const auto& event : batch.events)
        events.push_back(number(event.start) + "/" + number(event.complete) + "/" + event.case_id +
                         (event.event_id.empty() ? "" : "/" + event.event_id));
      std::sort(events.begin(), events.end());
      std::string text = group.activity + "," + group.resource + " " + number(batch.start) + "-" +
                         number(batch.complete) + ":";
      for (const auto& event : events) text += " " + event;
      groups.push_back(std::move(text));
    }
  }
  std::sort(groups.begin(), groups.end());
  std::string seen;
  for (const auto& group : groups) seen += (seen.empty() ? "" : " | ") + group;
  return seen;
}

BatchDiscoveryOptions batch_options() {
  BatchDiscoveryOptions options;
  options.activity_attribute = "act";
  options.timestamp_attribute = "ts";
  options.resource_attribute = "res";
  options.case_id_attribute = "case";
  return options;
}

int batches() {
  int failures = 0;
  // The frame's "@@index" is the event id: c1's two rows stay two events
  // (before: one, as the tuples without an id are a set).
  failures += check("pandas batches (@@index)", batches_text(BatchPandasDiscovery::apply(batch_frame(true), batch_options())),
                    "a,r1 1577837.4-1577837.52: 1577837.4/1577837.4/c0/0 1577837.52/1577837.52/c1/2 "
                    "1577837.52/1577837.52/c1/3 | b,r2 1577838-1577838.06: 1577838.06/1577838.06/c2/4 "
                    "1577838/1577838/c0/1");
  failures += check("polars batches (@@index)",
                    batches_text(BatchPolarsDiscovery::apply(LazyEventTable::from_table(batch_frame(true)), batch_options())),
                    "a,r1 1577837400-1577837520: 1577837400/1577837400/c0/0 1577837520/1577837520/c1/2 "
                    "1577837520/1577837520/c1/3 | b,r2 1577838000-1577838060: 1577838000/1577838000/c0/1 "
                    "1577838060/1577838060/c2/4");
  // Without the column the events are (start, complete, case) tuples, so the
  // two alike rows are one event.
  failures += check("pandas batches (no @@index)", batches_text(BatchPandasDiscovery::apply(batch_frame(false), batch_options())),
                    "a,r1 1577837.4-1577837.52: 1577837.4/1577837.4/c0 1577837.52/1577837.52/c1 | "
                    "b,r2 1577838-1577838.06: 1577838.06/1577838.06/c2 1577838/1577838/c0");
  return failures;
}

DfgTableOptions with_start() {
  DfgTableOptions options;
  options.start_timestamp_attribute = "start";
  return options;
}

// The frame of a CSV whose only row has no case: no rows left, but
// format_dataframe typed the columns before it dropped the row.
EventTable emptied_frame(const char* completion_dtype) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start", "@@index"};
  table.dtypes = {"string", "string", completion_dtype, "datetime64[us, UTC]", "int64"};
  return table;
}

int dfg_declared_dtypes() {
  int failures = 0;
  // The start column is a datetime column, so the partial order of no rows
  // is empty (before: the float-column error of a start column with no
  // value), and the concurrent events compare an empty timedelta column
  // (before: no pairs and no error).
  failures += check("partial order (emptied frame)",
                    error_of([] { return discover_pandas_partial_order(emptied_frame("datetime64[us, UTC]"), with_start()); }),
                    "no error");
  failures += check("concurrent events (emptied frame)",
                    error_of([] {
                      return discover_pandas_concurrent_events(emptied_frame("datetime64[us, UTC]"), with_start());
                    }),
                    "Invalid comparison between dtype=timedelta64[us] and int");
  // A dropped row's nanosecond completion makes the column datetime64[ns].
  failures += check("concurrent events (emptied frame, nanoseconds)",
                    error_of([] {
                      return discover_pandas_concurrent_events(emptied_frame("datetime64[ns, UTC]"), with_start());
                    }),
                    "Invalid comparison between dtype=timedelta64[ns] and int");
  // The start values sat only in a dropped row: a datetime column of NaT, so
  // the performance graph clamps the NaN flow to 0 (before: the float-column
  // error of a start column with no value).
  EventTable nat_starts;
  nat_starts.columns = {"case:concept:name", "concept:name", "time:timestamp", "start", "@@index"};
  nat_starts.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "int64"};
  nat_starts.rows.push_back({cell("case:concept:name", "c0"), cell("concept:name", "b"),
                             cell("time:timestamp", "2020-01-01 00:05:50"), cell("start", ""), int_cell("@@index", 0)});
  nat_starts.rows.push_back({cell("case:concept:name", "c0"), cell("concept:name", "a"),
                             cell("time:timestamp", "2020-01-01 00:06:10"), cell("start", ""), int_cell("@@index", 1)});
  auto performance = with_start();
  performance.measure = DfgTableMeasure::kPerformance;
  std::string flows;
  for (const auto& [pair, statistics] : discover_pandas_dfg(nat_starts, performance).performance)
    flows += (flows.empty() ? "" : " ") + pair.first + "," + pair.second + ":" + number(statistics.summary.mean);
  failures += check("performance DFG (starts only in dropped rows)", flows, "b,a:0");
  return failures;
}

int dfg_no_reduction() {
  // case,act,ts,start,res with res "NA" in b's row: without column reduction
  // pandas drops the row with a NaN, so only (a, c) overlap.
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp", "start", "res", "@@index"};
  const char* rows[][4] = {{"a", "2020-01-01 00:01:00", "2020-01-01 00:00:00", "r1"},
                           {"b", "2020-01-01 00:02:00", "2020-01-01 00:00:30", nullptr},
                           {"c", "2020-01-01 00:03:00", "2020-01-01 00:00:40", "r2"}};
  std::int64_t index = 0;
  for (const auto& row : rows)
    table.rows.push_back({cell("case:concept:name", "c0"), cell("concept:name", row[0]), cell("time:timestamp", row[1]),
                          cell("start", row[2]), row[3] ? cell("res", row[3]) : null_cell("res"),
                          int_cell("@@index", index++)});
  const auto pairs = [&](bool reduce) {
    auto options = with_start();
    options.reduce_columns = reduce;
    std::string text;
    for (const auto& pair : discover_pandas_concurrent_events(table, options).pairs)
      text += (text.empty() ? "" : " ") + pair.left_index_value + "," + pair.right_index_value + ":" +
              number(pair.overlap_seconds);
    return text;
  };
  return check("concurrent events (reduced)", pairs(true), "0,1:30 0,2:20 1,2:80") +
         check("concurrent events (not reduced, NaN res)", pairs(false), "0,2:20");
}

int heuristics_own_activity_column() {
  // A CSV's table read as events keeps the "act" column as the first-class
  // activity under its own name; the miner keyed by "act" reads it (before:
  // "event is missing activity attribute 'act'").
  EventLog log;
  const char* traces[][3] = {{"a", "b", nullptr}, {"a", "b", "b"}};
  int minute = 1;
  for (std::size_t t = 0; t < 2; ++t) {
    Trace trace;
    trace.case_id = "c" + std::to_string(t);
    for (const auto* activity : traces[t]) {
      if (!activity) break;
      Event event;
      event.activity = activity;
      event.activity_source_key = "act";
      event.timestamp = "2020-01-01 00:0" + std::to_string(minute++) + ":00";
      event.timestamp_source_key = "ts";
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  HeuristicMinerOptions options;
  options.activity_attribute = "act";
  options.timestamp_attribute = "ts";
  std::string seen;
  const auto net = discover_heuristics_net(log, options);
  std::vector<std::string> items;
  for (const auto& edge : net.edges)
    if (edge.dependency > 0)
      items.push_back(net.activities[edge.source] + "," + net.activities[edge.target] + ":" + number(edge.dependency) +
                      "/" + std::to_string(edge.frequency));
  std::sort(items.begin(), items.end());
  for (const auto& item : items) seen += (seen.empty() ? "" : " ") + item;
  return check("heuristics (own activity column)", seen, "a,b:0.6666666666666666/2 b,b:0.5/1");
}

}  // namespace

int main() {
  const int failures = batches() + dfg_declared_dtypes() + dfg_no_reduction() + heuristics_own_activity_column();
  if (failures != 0) {
    std::cerr << failures << " review BN1 failure(s)\n";
    return 1;
  }
  std::cout << "review BN1 tests passed\n";
  return 0;
}
