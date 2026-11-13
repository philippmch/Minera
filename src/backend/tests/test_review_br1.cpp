// Review BR1: the log families on CSV cells with nanosecond digits. pm4py's
// EventLog algorithms read a timestamp as Timestamp.timestamp(), the exact
// nanoseconds rounded to the microsecond (the batches' log variant, the
// interval tree, the trace-based time features, the temporal features); the
// pandas batches read a datetime64[ns] column as its int64 nanoseconds over
// 1e9; the stochastic map's replay durations are Timedelta.total_seconds()
// of exact instants (the nanoseconds of the difference dropped). Values are
// taken from pm4py 2.7.22 black-box on the same input (review-harness
// round-br, br1/mk.py).
#include "minera/batches.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/event_data.hpp"
#include "minera/interval_tree.hpp"
#include "minera/log_to_features.hpp"
#include "minera/petri_net.hpp"
#include "minera/stochastic_map.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

// The shortest fixed-point spelling that reads back as the same double
// (Python's repr for these magnitudes, without a trailing ".0").
std::string repr(double value) {
  for (int digits = 0; digits <= 17; ++digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", digits, value);
    if (std::stod(buffer) == value) return buffer;
  }
  return "?";
}

const std::string kCsv =
    "case,act,ts,res\n"
    "c1,a,2020-01-01 00:00:00.000000600,r1\n"
    "c1,b,2020-01-01 00:00:05.123456789,r2\n"
    "c2,a,2020-01-01 00:00:01.000001400,r1\n"
    "c2,b,2020-01-01 00:00:07.999999999,r2\n"
    "c3,a,2020-01-01 00:00:02.5,r1\n"
    "c3,b,2020-01-01 00:00:09.000000001,r2\n";

CsvEventData import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  options.resource_column = "res";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options);
}

std::string batches(const BatchDiscovery& discovery) {
  std::string out;
  for (const auto& group : discovery.groups) {
    out += group.activity + "/" + group.resource + ":";
    for (const auto& batch : group.batches) {
      out += "[" + repr(batch.start) + "," + repr(batch.complete);
      std::vector<std::string> events;
      for (const auto& event : batch.events)
        events.push_back(event.case_id + "=" + repr(event.start) + "," + repr(event.complete));
      std::sort(events.begin(), events.end());
      for (const auto& event : events) out += " " + event;
      out += "]";
    }
    out += ";";
  }
  return out;
}

}  // namespace

int main() {
  const auto data = import();
  // The batches: the log variant rounds each instant to the microsecond, the
  // pandas variant divides the int64 nanoseconds by 1e9 (pm4py lists the
  // b/r2 group first).
  {
    BatchDiscoveryOptions options;
    options.resource_attribute = "res";
    check("batches log ns", batches(discover_batches(data.log, options)),
          "b/r2:[1577836805.123457,1577836809 c1=1577836805.123457,1577836805.123457 "
          "c2=1577836808,1577836808 c3=1577836809,1577836809];"
          "a/r1:[1577836800.000001,1577836802.5 c1=1577836800.000001,1577836800.000001 "
          "c2=1577836801.000001,1577836801.000001 c3=1577836802.5,1577836802.5];");
    // The frame's column is datetime64[ns, UTC] (a cell has nine fraction
    // digits); the other columns are text. The table is read under the CSV's
    // headers (format_dataframe's copies hold the same cells).
    options.variant = BatchDiscoveryVariant::kPandas;
    options.activity_attribute = "act";
    options.timestamp_attribute = "ts";
    options.case_id_attribute = "case";
    auto table = data.table;
    table.dtypes.assign(table.columns.size(), "str");
    for (std::size_t column = 0; column < table.columns.size(); ++column)
      if (table.columns[column] == "ts") table.dtypes[column] = "datetime64[ns, UTC]";
    check("batches pandas ns", batches(discover_batches(table, options)),
          "b/r2:[1577836805.1234567,1577836809 c1=1577836805.1234567,1577836805.1234567 "
          "c2=1577836808,1577836808 c3=1577836809,1577836809];"
          "a/r1:[1577836800.0000005,1577836802.5 c1=1577836800.0000005,1577836800.0000005 "
          "c2=1577836801.0000012,1577836801.0000012 c3=1577836802.5,1577836802.5];");
  }
  // The interval tree: each case's open path, its instants rounded to the
  // microsecond, widened by the epsilon.
  {
    OpenPathIntervalOptions options;
    options.activity_key = "concept:name";
    std::vector<std::string> intervals;
    for (const auto& interval : discover_open_path_intervals(data.log, options))
      intervals.push_back(repr(interval.start) + "," + repr(interval.end));
    std::sort(intervals.begin(), intervals.end());
    std::string out;
    for (const auto& interval : intervals) out += interval + ";";
    check("interval tree ns", out,
          "1577836799.999991,1577836805.123467;1577836800.999991,1577836808.00001;"
          "1577836802.49999,1577836809.00001;");
  }
  // The trace-based time features (float32 values, as pm4py's matrix).
  {
    LogToFeaturesOptions options;
    options.enable_activity_def_representation = false;
    options.enable_succ_def_representation = false;
    options.event_string_attributes_supplied = true;
    options.event_numeric_attributes_supplied = true;
    options.case_string_attributes_supplied = true;
    options.case_numeric_attributes_supplied = true;
    options.event_successor_attributes_supplied = true;
    options.enable_case_duration = options.enable_case_duration_supplied = true;
    options.enable_direct_paths_times_last_occ = options.enable_direct_paths_times_last_occ_supplied = true;
    const auto result = log_to_features(data.log, options);
    std::string out;
    for (const auto& row : result.trace_rows) {
      for (std::size_t index = 0; index < row.size() && index < result.feature_names.size(); ++index) {
        const auto& name = result.feature_names[index];
        if (name != "@@caseDuration" && name != "directPathPerformanceLastOcc@@a##b") continue;
        if (const auto* value = std::get_if<double>(&row[index])) out += name + "=" + repr(*value) + " ";
      }
      out += ";";
    }
    check("trace features ns", out,
          "@@caseDuration=5.123456001281738 directPathPerformanceLastOcc@@a##b=5.123456001281738 ;"
          "@@caseDuration=6.999999046325684 directPathPerformanceLastOcc@@a##b=6.999999046325684 ;"
          "@@caseDuration=6.5 directPathPerformanceLastOcc@@a##b=6.5 ;");
  }
  // The temporal features grouped by second: c1's first event (600 ns past
  // the second) arrives at 0.000001, so c2 arrives 1.0 later.
  {
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.resource_key = "res";
    options.grouper_freq = "s";
    const auto result = log_to_features(data.log, options);
    std::string out = std::to_string(result.temporal_rows.size()) + ":";
    for (std::size_t row = 0; row < result.temporal_rows.size() && row < 3; ++row) {
      const auto& temporal = result.temporal_rows[row];
      out += "[" + temporal.timestamp_ns.value_or("?");
      for (const auto& cell : temporal.values)
        out += "," + (std::holds_alternative<double>(cell) ? repr(std::get<double>(cell)) : std::get<std::string>(cell));
      out += "]";
    }
    check("temporal features ns", out,
          "10:[1577836800000000000,1,1,1,0,0,5.123456001281738,5.123456001281738,0,0,1,1,1,1]"
          "[1577836801000000000,1,1,1,1,2.8765430450439453,6.999999046325684,6.999999046325684,0,0,1,1,1,1]"
          "[1577836802000000000,1,1,1,1.4999990463256836,1,6.5,6.5,0,0,1,1,1,1]");
  }
  // The stochastic map: b's durations are Timedelta.total_seconds() of the
  // exact instants, so its uniform runs from 5.123456 to 6.999998.
  {
    PetriNet net;
    const auto source = net.add_place();
    const auto middle = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.set_activities({"a", "b"});
    net.add_transition(0, {source}, {middle});
    net.add_transition(1, {middle}, {sink});
    StochasticMapFitOptions fit;
    fit.replay.activity_attribute = "act";  // the CSV log holds its own header
    const auto fitted = fit_stochastic_map_with_report(data.log, net, fit);
    std::string out;
    for (const auto& statistics : fitted.statistics) {
      out += std::to_string(statistics.transition) + ":";
      for (const auto value : statistics.performance) out += repr(value) + ",";
      out += ";";
    }
    const auto& b = fitted.map.transitions.at(1);
    out += "b=" + std::to_string(static_cast<int>(b.distribution)) + "," + repr(b.parameter) + "," +
           repr(b.secondary_parameter);
    check("stochastic map ns", out,
          "0:0,0,0,;1:5.123456,6.999998,6.5,;b=" +
              std::to_string(static_cast<int>(StochasticMapDistribution::kUniform)) + ",5.123456,6.999998");
  }
  if (failures) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review BR1 checks passed\n";
  return 0;
}
