// Review BQ1: pm4py's EventLog algorithms read a timestamp as
// Timestamp.timestamp(), rounded to the microsecond: a CSV cell with
// nanosecond digits keeps them in the frame's log, so the spectrum's log
// variants, the temporal profile's EventLog discovery and conformance and the
// correlation miner see the rounded seconds; the log variants sort a trace by
// the exact instant (two cells tens of nanoseconds apart are one double).
// The process-tree reduction keyed by a CSV's own column header reads that
// column, as pm4py reads it on format_dataframe's frame. Values are taken
// from pm4py 2.7.22 black-box on the same inputs (review-harness round-bq,
// bq1/mk.py).
#include "minera/correlation_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/event_data.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/temporal_profile.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
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

std::string instants(const PerformanceSpectrum& spectrum) {
  std::string out;
  for (const auto& observation : spectrum.observations) {
    out += "[";
    for (std::size_t index = 0; index < observation.instants.size(); ++index)
      out += (index ? "," : "") + repr(observation.instants[index]);
    out += "]";
  }
  return out;
}

EventLog import(const std::string& csv, const std::string& activity = "act") {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = activity;
  options.timestamp_column = "ts";
  if (csv.find(",res") != std::string::npos && activity != "res") options.resource_column = "res";
  std::istringstream input(csv);
  return import_csv_event_data(input, options).log;
}

}  // namespace

int main() {
  const std::vector<std::string> pattern{"a", "b"};
  // The log variants on nanosecond cells: sorted by the exact instant (the b
  // 50 ns after a comes first in the file), the instants rounded to the
  // microsecond.
  {
    const auto log = import(
        "case,act,ts\n"
        "c1,b,2020-01-01 00:00:00.000000100\n"
        "c1,a,2020-01-01 00:00:00.000000050\n"
        "c1,b,2020-01-01 00:00:01\n");
    check("spectrum log tie", instants(discover_performance_spectrum_log(log, pattern)),
          "[1577836800,1577836800]");
    check("spectrum log_disconnected tie", instants(discover_performance_spectrum_log_disconnected(log, pattern)),
          "[1577836800,1577836800]");
    const auto log2 = import(
        "case,act,ts\n"
        "c1,b,2020-01-01 00:00:05.123456789\n"
        "c1,a,2020-01-01 00:00:05.123456700\n"
        "c2,a,2020-01-01 00:00:05.123456701\n"
        "c2,b,2020-01-01 00:00:05.123456702\n");
    check("spectrum log ns", instants(discover_performance_spectrum_log(log2, pattern)),
          "[1577836805.123457,1577836805.123457][1577836805.123457,1577836805.123457]");
  }
  // The temporal profile's EventLog discovery and conformance.
  {
    const auto log = import(
        "case,act,ts\n"
        "c1,a,2020-01-01 00:00:00.123456789\n"
        "c1,b,2020-01-01 00:01:00.987654321\n"
        "c2,a,2020-01-01 00:00:05.000000500\n"
        "c2,b,2020-01-01 00:00:06.000001500\n");
    const auto profile = discover_temporal_profile_log(log);
    std::string out;
    for (const auto& entry : profile.entries)
      out += entry.source + ">" + entry.target + "=" + repr(entry.mean_seconds) + "," +
             repr(entry.standard_deviation_seconds) + ";";
    check("temporal profile log ns", out, "a>b=30.932098984718323,42.33037898578345;");
    TemporalProfile reference;
    reference.entries.push_back({"a", "b", 2, 30.0, 1.0});
    TemporalProfileOptions options;
    options.zeta = 0.0;
    std::string deviations;
    for (const auto& trace : conform_temporal_profile_log(log, reference, options))
      for (const auto& deviation : trace)
        deviations += deviation.source + ">" + deviation.target + "=" + repr(deviation.elapsed_seconds) + ";";
    check("temporal conformance log ns", deviations, "a>b=60.86419701576233;a>b=1.0000009536743164;");
  }
  // The correlation miner's durations (Timestamp.timestamp() differences).
  {
    const auto log = import(
        "case,act,ts\n"
        "c1,a,2020-01-01 00:00:00.123456789\n"
        "c1,b,2020-01-01 00:00:10.987654321\n");
    for (const auto variant : {CorrelationVariant::kClassic, CorrelationVariant::kClassicSplit,
                               CorrelationVariant::kTraceBased}) {
      CorrelationMinerOptions options;
      options.variant = variant;
      const auto result = discover_correlation_miner(log, options);
      const auto found = result.performance_dfg.find({"a", "b"});
      check("correlation a>b " + std::to_string(static_cast<int>(variant)),
            found == result.performance_dfg.end() ? "missing" : repr(found->second), "10.864197015762329");
    }
  }
  // The process-tree reduction keyed by the CSV's own headers.
  {
    const std::string csv =
        "case,act,ts,res\n"
        "c1,a,2020-01-01 00:00:10,r1\n"
        "c1,b,2020-01-01 00:00:20,r2\n"
        "c1,c,2020-01-01 00:00:30,r1\n"
        "c2,a,2020-01-01 00:00:15,r1\n"
        "c2,c,2020-01-01 00:00:40,r2\n";
    const auto log = import(csv);
    const auto tree = discover_process_tree(log);
    const auto reduce = [&](const std::string& key) {
      ProcessTreeReductionOptions options;
      options.activity_key = key;
      try {
        return process_tree_json(reduce_process_tree(tree, log.traces[1], options));
      } catch (const std::exception& error) {
        return std::string("error: ") + error.what();
      }
    };
    check("tree reduction act", reduce("act"), reduce("concept:name"));
    if (reduce("act").rfind("error", 0) == 0) check("tree reduction act runs", reduce("act"), "no error");
    // The resource header: the same reduction as a log whose activity is
    // the resource column.
    const auto by_resource = import(csv, "res");
    ProcessTreeReductionOptions options;
    const auto expected = process_tree_json(reduce_process_tree(tree, by_resource.traces[1], options));
    check("tree reduction res", reduce("res"), expected);
  }

  if (failures) return 1;
  std::cout << "review BQ1 tests passed\n";
  return 0;
}
