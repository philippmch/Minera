// Review BP1: pm4py's DataFrame performance spectrum reads a datetime column
// as its int64 units over 1e9 (a microsecond column gives seconds / 1000,
// exactly divided; a nanosecond one the seconds), its LazyFrame variants as
// epoch microseconds over 1e6 (a nanosecond cell truncated); and the log
// families keyed by a CSV's own column headers (rework, chaotic activities,
// variant statistics, boundary activities, overlap) read the selected
// columns, as pm4py reads them on format_dataframe's frame. Values are taken
// from pm4py 2.7.22 black-box on the same inputs (review-harness round-bp,
// bp1/mk.py).
#include "minera/boundary_activities.hpp"
#include "minera/chaotic_activities.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/event_data.hpp"
#include "minera/overlap.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/rework.hpp"
#include "minera/variant_statistics.hpp"

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

EventAttribute text(const char* key, const std::string& value) {
  return EventAttribute{key, value};
}

EventTable spectrum_table(const std::vector<std::string>& stamps, const std::string& dtype) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  const char* activities[] = {"a", "b"};
  for (std::size_t index = 0; index < stamps.size(); ++index)
    table.rows.push_back({text("case:concept:name", "c1"), text("concept:name", activities[index % 2]),
                          text("time:timestamp", stamps[index])});
  if (!dtype.empty()) table.dtypes = {"object", "object", dtype};
  return table;
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

CsvEventData import(const std::string& csv) {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  options.resource_column = "res";
  std::istringstream input(csv);
  return import_csv_event_data(input, options);
}

}  // namespace

int main() {
  const std::vector<std::string> pattern{"a", "b"};
  PerformanceSpectrumOptions options;
  // A microsecond column (pandas 3's default): the units over 1e9, exactly
  // divided (Minera multiplied by 0.001: 1577836.8090000001).
  {
    const auto table = spectrum_table({"2020-01-01 00:00:09", "2020-01-01 00:01:00"}, "datetime64[us, UTC]");
    check("dataframe us", instants(discover_performance_spectrum_dataframe(table, pattern, options)),
          "[1577836.809,1577836.86]");
    check("dataframe_disconnected us",
          instants(discover_performance_spectrum_dataframe_disconnected(table, pattern, options)),
          "[1577836.809,1577836.86]");
    const auto half = spectrum_table({"2020-01-01 00:00:09.5", "2020-01-01 00:01:00"}, "datetime64[us, UTC]");
    check("dataframe us fraction", instants(discover_performance_spectrum_dataframe(half, pattern, options)),
          "[1577836.8095,1577836.86]");
    // No declared dtype reads as microseconds, as before.
    const auto plain = spectrum_table({"2020-01-01 00:00:09", "2020-01-01 00:01:00"}, "");
    check("dataframe no dtype", instants(discover_performance_spectrum_dataframe(plain, pattern, options)),
          "[1577836.809,1577836.86]");
  }
  // A nanosecond column (a CSV cell below the microsecond): the seconds.
  {
    const auto table = spectrum_table({"2020-01-01 00:00:09.123456789", "2020-01-01 00:01:00"},
                                      "datetime64[ns, UTC]");
    check("dataframe ns", instants(discover_performance_spectrum_dataframe(table, pattern, options)),
          "[1577836809.1234567,1577836860]");
    // The LazyFrame variants: epoch microseconds over 1e6, truncated.
    check("lazyframe ns", instants(discover_performance_spectrum_lazyframe(LazyEventTable::from_table(table),
                                                                           pattern, options)),
          "[1577836809.123456,1577836860]");
    check("lazyframe_disconnected ns",
          instants(discover_performance_spectrum_lazyframe_disconnected(LazyEventTable::from_table(table),
                                                                        pattern, options)),
          "[1577836809.123456,1577836860]");
  }

  // The log families keyed by a CSV's own headers (act, res, ts, start).
  const std::string csv =
      "case,act,ts,res,start\n"
      "c1,a,2020-01-01 00:00:10,r1,2020-01-01 00:00:05\n"
      "c1,b,2020-01-01 00:00:20,r2,2020-01-01 00:00:08\n"
      "c1,a,2020-01-01 00:00:30,r1,2020-01-01 00:00:25\n"
      "c2,b,2020-01-01 00:00:15,r1,2020-01-01 00:00:15\n"
      "c2,b,2020-01-01 00:00:40,r1,2020-01-01 00:00:30\n";
  const auto data = import(csv);
  const auto rework = [&](const std::string& key) {
    ReworkOptions rework_options;
    rework_options.activity_key = key;
    try {
      std::string out;
      for (const auto& [activity, count] : discover_rework(data.log, rework_options).activity_cases)
        out += activity + "=" + std::to_string(count) + ";";
      return out;
    } catch (const std::exception& error) {
      return std::string("error: ") + error.what();
    }
  };
  // pm4py.get_rework_cases_per_activity(frame, activity_key=...)
  check("rework act", rework("act"), "a=1;b=1;");
  check("rework concept:name", rework("concept:name"), "a=1;b=1;");
  check("rework res", rework("res"), "r1=2;");
  const auto variants = [&](const std::string& key) {
    VariantStatisticsOptions variant_options;
    variant_options.activity_key = key;
    try {
      const auto result = discover_variant_statistics(data.log, variant_options);
      return std::to_string(result.variants.size());
    } catch (const std::exception& error) {
      return std::string("error: ") + error.what();
    }
  };
  check("variant statistics act", variants("act"), variants("concept:name"));
  const auto chaotic = [&](const std::string& key) {
    ChaoticActivitiesOptions chaotic_options;
    chaotic_options.activity_key = key;
    try {
      std::string out;
      for (const auto& activity : discover_chaotic_activities(data.log, chaotic_options).activities)
        out += activity.activity + ";";
      return out;
    } catch (const std::exception& error) {
      return std::string("error: ") + error.what();
    }
  };
  check("chaotic act", chaotic("act"), chaotic("concept:name"));
  if (chaotic("act").rfind("error", 0) == 0) check("chaotic act runs", chaotic("act"), "no error");
  const auto boundary = [&](const std::string& key) {
    try {
      std::string out;
      for (const auto& [activity, count] : get_start_activities(data.log, key))
        out += activity + "=" + std::to_string(count) + ";";
      return out;
    } catch (const std::exception& error) {
      return std::string("error: ") + error.what();
    }
  };
  check("start activities act", boundary("act"), "a=1;b=1;");
  const auto overlap = [&](const std::string& activity, const std::string& completion) {
    OverlapOptions overlap_options;
    overlap_options.activity_attribute = activity;
    overlap_options.start_timestamp_attribute = "start";
    overlap_options.completion_timestamp_attribute = completion;
    try {
      std::string out;
      for (const auto& [pair, count] : discover_concurrent_activity_overlap(data.log, overlap_options))
        out += pair.first + "|" + pair.second + "=" + std::to_string(count) + ";";
      return out;
    } catch (const std::exception& error) {
      return std::string("error: ") + error.what();
    }
  };
  check("overlap act/ts", overlap("act", "ts"), overlap("concept:name", "time:timestamp"));
  if (overlap("act", "ts").rfind("error", 0) == 0) check("overlap act/ts runs", overlap("act", "ts"), "no error");

  if (failures) return 1;
  std::cout << "review BP1 tests passed\n";
  return 0;
}
