// Review BJ1: pm4py's DataFrame Declare conformance, temporal conformance
// and between filter on uploads. The Declare conformance list reads each
// case as its slice of the frame's rows (from its first row on, as many as
// it has), while its diagnostics table reads the merged cases; the DataFrame
// temporal conformance lists the cases in the order of their first rows and
// takes its durations as pandas Timedeltas; the segment filter's between
// marks each case's slice in sorted id order, a later case replacing an
// earlier one's mark. Values are taken from pm4py 2.7.22 black-box on the
// same inputs (review-harness round-bj, bj1/mk.py).
#include "minera/dataframe_columns.hpp"
#include "minera/declare.hpp"
#include "minera/event_data.hpp"
#include "minera/segment_filter.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

EventLog xes_upload(const std::string& text) {
  std::istringstream xes(text);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  return log;
}

// One <trace> per row: trace names repeat apart where cases interleave.
EventLog rows(const std::vector<std::pair<std::string, std::string>>& rows) {
  std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (std::size_t i = 0; i < rows.size(); ++i) {
    char time[32];
    std::snprintf(time, sizeof time, "2020-01-01T00:00:%02d+00:00", static_cast<int>(i));
    text += "<trace><string key=\"concept:name\" value=\"" + rows[i].first + "\"/><event><string key=\"concept:name\" value=\"" +
            rows[i].second + "\"/><date key=\"time:timestamp\" value=\"" + time + "\"/></event></trace>";
  }
  return xes_upload(text + "</log>");
}

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

// "case:deviations/fitness" per trace, in the log's order.
std::string fitness(const EventLog& log, const std::vector<DeclareConformance>& results) {
  std::string text;
  for (std::size_t i = 0; i < results.size(); ++i) {
    char value[48];
    std::snprintf(value, sizeof value, "%d/%g", results[i].no_dev_total, results[i].dev_fitness);
    text += (text.empty() ? "" : " ") + log.traces.at(i).case_id + ":" + value;
  }
  return text;
}

int declare() {
  // x b, y c, x a, w b, y c: x's slice is rows 0 and 1 (b, c), y's rows 1
  // and 2 (c, a), w's row 3 (b).
  const auto log = rows({{"x", "b"}, {"y", "c"}, {"x", "a"}, {"w", "b"}, {"y", "c"}});
  DeclareModel model;
  model.add(kDeclareExistence, DeclareConstraint{"a", "", false, 1, 1});
  int failures = 0;
  // Before: "x:0/1 y:1/0 w:1/0" (each trace's own rows).
  failures += check("declare conformance (list)", fitness(log, check_declare_conformance_as_uploaded(log, model)),
                    "x:1/0 y:0/1 w:1/0");
  // pm4py's diagnostics table: the merged cases, x (b, a) fits.
  failures += check("declare conformance (diagnostics)", fitness(log, check_declare_conformance(log, model)),
                    "x:0/1 y:1/0 w:1/0");
  return failures;
}

int temporal() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  const auto cell = [](std::string key, std::string value, AttributeKind kind = AttributeKind::kString) {
    return std::optional<EventAttribute>(EventAttribute{std::move(key), std::move(value), kind});
  };
  const auto row = [&](const char* id, const char* activity, const char* time) {
    return std::vector<std::optional<EventAttribute>>{cell("case:concept:name", id), cell("concept:name", activity),
                                                      cell("time:timestamp", time, AttributeKind::kDate)};
  };
  // q before p in the frame.
  table.rows = {row("q", "a", "2020-01-01T00:00:00.1+00:00"), row("q", "b", "2020-01-01T00:06:00.314369+00:00"),
                row("p", "a", "2020-01-01T00:00:00+00:00"), row("p", "b", "2020-01-01T00:00:59.645930+00:00")};
  TemporalProfile profile;
  profile.entries.push_back({"a", "b", 1, 100.0, 50.0});
  TemporalProfileOptions options;
  options.zeta = 0.0;
  std::string text;
  for (const auto& trace : conform_temporal_profile_dataframe(table, profile, options))
    for (const auto& deviation : trace) {
      char value[96];
      std::snprintf(value, sizeof value, "%s:%.17g,%.17g ", deviation.case_id.c_str(), deviation.elapsed_seconds,
                    deviation.standard_deviation_distance);
      text += value;
    }
  // pandas' Timedelta: 360 + 214369e-6 and 59 + 645930e-6. Before: p first,
  // and epoch-second differences (360.21436905860901, 59.645930051803589).
  return check("temporal conformance (dataframe)", text,
               "q:360.21436899999998,5.2042873799999994 p:59.64593,0.80708139999999995 ");
}

// "case:rows" per sub-case, by case id, rows by input order.
std::string kept(const EventLog& log) {
  std::map<std::string, std::vector<std::size_t>> cases;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) cases[trace.case_id].push_back(event.input_order);
  std::string text;
  for (auto& [id, orders] : cases) {
    std::sort(orders.begin(), orders.end());
    text += (text.empty() ? "" : " ") + id + ":";
    for (std::size_t i = 0; i < orders.size(); ++i) text += (i ? "," : "") + std::to_string(orders[i]);
  }
  return text;
}

int segments() {
  // x a, y a, x b, z a, y c, x b, w a, z b, y b.
  const auto log =
      rows({{"x", "a"}, {"y", "a"}, {"x", "b"}, {"z", "a"}, {"y", "c"}, {"x", "b"}, {"w", "a"}, {"z", "b"}, {"y", "b"}});
  int failures = 0;
  // Before: "x_0:0,2 y_0:1,4,8 z_0:3,7".
  failures += check("segment between", kept(filter_between(log, BetweenFilterOptions{{"a", "c"}, {"b"}, "_", {}})),
                    "x_0:0 y_0:1,2");
  // Equal lists reopen at the closing row: x marks rows 0 to 2, y's slice
  // (rows 1 to 3: a, b, a) replaces rows 1 to 3.
  failures += check("segment between (equal lists)",
                    kept(filter_between(log, BetweenFilterOptions{{"a", "b"}, {"a", "b"}, "-", {}})),
                    "x-0:0 y-0:1 y-1:2,3");
  return failures;
}

}  // namespace

int main() {
  const int failures = declare() + temporal() + segments();
  if (failures != 0) {
    std::cerr << failures << " review BJ1 failure(s)\n";
    return 1;
  }
  std::cout << "review BJ1 tests passed\n";
  return 0;
}
