// Review AW1: the time statistics, network analysis and resource profiles on
// CSV uploads, against pm4py 2.7.22 on read_csv + format_dataframe(case_id=
// "cid", activity_key="act", timestamp_key="ts") of the same text (values
// and messages taken from the reference black-box).
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/cycle_time.hpp"
#include "minera/json_export.hpp"
#include "minera/network_analysis.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/service_time.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace minera;

namespace {

EventLog upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

// The call fails with a message that ends in `expected`.
void expect_error(const std::function<void()>& call, const std::string& expected) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    if (message.size() >= expected.size() && message.ends_with(expected)) return;
    std::cerr << "unexpected message: " << message << "\n  wanted: " << expected << "\n";
    assert(false);
  }
  std::cerr << "no error, wanted: " << expected << "\n";
  assert(false);
}

const std::string kLog =
    "cid,act,ts,who,st,txt,one\n"
    "c1,a,2020-01-03 07:30:00.5,R,2020-01-03 07:00:00,x,\n"
    "c1,b,2020-01-03 07:50:00.123456,S,,y,k\n"
    "c2,a,2020-01-01 02:10:00.123456,R,2020-01-01 02:00:00,z,\n"
    "c2,b,2020-01-01 02:31:00.123456,R,2020-01-01 02:30:00,,\n"
    "c3,a,2020-01-02 09:00:00,S,,w,\n";

const std::string kNoDt = "AttributeError: Can only use .dt accessor with datetimelike values";
const std::string kStrStr = "TypeError: unsupported operand type(s) for -: 'str' and 'str'";

void case_statistics_on_uploads(const EventLog& log) {
  // Business hours count the week offsets from the exact microseconds.
  CaseStatisticsOptions bh;
  bh.timestamp_key = "time:timestamp";
  bh.use_business_hours = true;
  const auto hours = discover_case_statistics(log, bh);
  std::map<std::string, double> durations;
  for (const auto& entry : hours.cases) durations[entry.case_id] = entry.case_duration;
  assert(durations.at("c1") == 1199.6234560000012);
  assert(durations.at("c2") == 0.0 && durations.at("c3") == 0.0);

  // A case whose cells are all NaT has no times (null in JSON), and with
  // business hours fails.
  CaseStatisticsOptions start;
  start.timestamp_key = "st";
  const auto starts = discover_case_statistics(log, start);
  for (const auto& entry : starts.cases) {
    if (entry.case_id == "c3") assert(std::isnan(entry.start_time) && std::isnan(entry.case_duration));
    if (entry.case_id == "c2") assert(entry.case_duration == 1800.0);
    if (entry.case_id == "c1") assert(entry.case_duration == 0.0);
  }
  assert(case_statistics_json(starts).find("null") != std::string::npos);
  start.use_business_hours = true;
  expect_error([&] { (void)discover_case_statistics(log, start); },
               "ValueError: cannot convert float NaN to integer");

  CaseStatisticsOptions text;
  text.timestamp_key = "txt";
  expect_error([&] { (void)discover_case_statistics(log, text); }, kStrStr);
}

void case_arrival_on_uploads(const EventLog& log) {
  CaseArrivalOptions options;
  options.timestamp_key = "time:timestamp";
  const auto result = discover_case_arrival(log, options);
  assert(result.arrival_average == 96000.188272);
  assert(result.dispersion_average == 95970.0);
  // Text fails the subtraction once two cases have a cell; a column where
  // one case has one fails at the .dt accessor.
  options.timestamp_key = "txt";
  expect_error([&] { (void)discover_case_arrival(log, options); }, kStrStr);
  options.timestamp_key = "one";
  expect_error([&] { (void)discover_case_arrival(log, options); }, kNoDt);
}

void service_and_cycle_time_on_uploads(const EventLog& log) {
  // NaT starts give NaN durations, which the aggregation skips.
  ServiceTimeOptions options;
  options.start_timestamp_key = "st";
  options.timestamp_key = "time:timestamp";
  options.activity_key = "concept:name";
  options.aggregation = DfgPerformanceAggregation::kSum;
  auto result = discover_service_time(log, options);
  assert(result.values.at("a") == 2400.6234560000003);
  assert(result.values.at("b") == 60.123456);
  options.aggregation = DfgPerformanceAggregation::kMean;
  result = discover_service_time(log, options);
  assert(result.values.at("a") == 1200.3117280000001);
  assert(result.values.at("b") == 60.123456);

  // Each row's start and end take .timestamp(), which NaT has not.
  CycleTimeOptions cycle;
  cycle.start_timestamp_key = "st";
  cycle.timestamp_key = "time:timestamp";
  expect_error([&] { (void)discover_cycle_time(log, cycle); }, "ValueError: NaTType does not support timestamp");
}

void network_analysis_on_uploads(const EventLog& log) {
  // Durations are the dates' exact difference (Timedelta.total_seconds).
  NetworkAnalysisOptions options;
  options.source_node_attribute = "who";
  options.target_node_attribute = "who";
  options.include_performance = true;
  const auto result = discover_network_analysis(log, options);
  bool seen = false;
  for (const auto& edge : result.edges)
    if (edge.source == "R" && edge.target == "S") {
      assert(edge.performance.size() == 1 && edge.performance.front() == 1199.623456);
      seen = true;
    }
  assert(seen);
}

void resource_profiles_with_a_column_twice(const EventLog& log) {
  const auto multitasking = [&](std::map<std::string, std::string> keys) {
    ResourceProfilesOptions o;
    o.metric = ResourceProfileMetric::kMultitasking;
    o.resource_attribute = "who";
    o.t1 = "2019-06-01 00:00:00";
    o.t2 = "2021-01-01 00:00:00";
    o.resource = "R";
    o.resource_given = true;
    if (keys.contains("activity")) o.activity_attribute = keys["activity"];
    if (keys.contains("start")) o.start_timestamp_attribute = keys["start"];
    if (keys.contains("timestamp")) o.timestamp_attribute = keys["timestamp"];
    return discover_resource_profiles(log, o);
  };
  // With a start column only the resource column named twice fails (its
  // mask cannot realign); the activity on the start column runs.
  const auto runs = multitasking({{"activity", "st"}, {"start", "st"}});
  assert(runs.rows.size() == 1 && runs.rows.front().value == 0.0);
  const std::string reindex = "ValueError: cannot reindex on an axis with duplicate labels";
  expect_error([&] { (void)multitasking({{"start", "who"}}); }, reindex);
  // Without one, the case and timestamp columns sort first, then the
  // resource's mask fails.
  expect_error([&] { (void)multitasking({{"activity", "who"}}); }, reindex);
  expect_error([&] { (void)multitasking({{"activity", "time:timestamp"}}); },
               "ValueError: The column label 'time:timestamp' is not unique.");
  // An interval that ends before it starts is refused by the interval tree.
  expect_error([&] { (void)multitasking({{"start", "time:timestamp"}, {"timestamp", "st"}}); },
               "ValueError: Start must be less than end");
}

}  // namespace

int main() {
  const auto log = upload(kLog);
  case_statistics_on_uploads(log);
  case_arrival_on_uploads(log);
  service_and_cycle_time_on_uploads(log);
  network_analysis_on_uploads(log);
  resource_profiles_with_a_column_twice(log);
  std::cout << "review AW1 tests passed\n";
  return 0;
}
