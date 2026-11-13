// Review BT1: typed columns of a CSV upload's frame as the activity key of
// the DFG variants, the LLM abstractions, and the start/end activities and
// variant statistics; and the features descriptions' character budget.
// pandas names a DataFrame DFG's activities str() of the typed cells (an
// int column's 007 is "7"), its shifted target column turns an int64 column
// into floats ("7.0"), and a pair with a missing cell is dropped; CLEAN
// splits a case at a missing cell; CLEAN_TIME raises on a missing cell; the
// simplified interface checks the key column's dtype and gaps first. Values
// are taken from pm4py 2.7.22 black-box on the same input (review-harness
// round-bt, bt1/mk.py).
#include "minera/boundary_activities.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/llm_abstractions.hpp"
#include "minera/variant_statistics.hpp"

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

void check_error(const std::string& label, const std::function<void()>& run, const std::string& message) {
  try {
    run();
  } catch (const std::exception& error) {
    if (std::string(error.what()).find(message) != std::string::npos) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.what() << "\n  expected " << message << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << message << "\n";
}

const std::string kCsv =
    "case,act,ts,n,t,d\n"
    "c1,a,2020-01-01 00:00:00,1,x,2021-03-01\n"
    "c1,b,2020-01-01 00:01:00,007,,2021-03-02\n"
    "c1,c,2020-01-01 00:03:00,2,y,\n"
    "c1,a,2020-01-01 00:04:00,1,x,2021-03-01\n"
    "c2,a,2020-01-01 00:00:00,2,x,2021-03-01\n"
    "c2,b,2020-01-01 00:02:00,1,y,2021-03-02\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

template <class Map>
std::string counts(const Map& map) {
  std::string out;
  for (const auto& [key, value] : map) {
    if (!out.empty()) out += ", ";
    if constexpr (std::is_same_v<typename Map::key_type, DfgActivityPair>)
      out += key.first + "->" + key.second;
    else
      out += key;
    std::ostringstream number;
    number << value;
    out += "=" + number.str();
  }
  return out;
}

DfgStatisticalAnalysis statistics(const EventLog& log, DfgStatisticalVariant variant, const std::string& key) {
  DfgStatisticalOptions options;
  options.variant = variant;
  options.activity_attribute = key;
  options.dataframe_arithmetic = true;
  return discover_dfg_statistics(log, options);
}

void dfg_variants(const EventLog& log) {
  check("frequency DFG keyed by an int column", counts(statistics(log, DfgStatisticalVariant::kNative, "n").frequency_dfg),
        "1->7=1, 2->1=2, 7->2=1");
  const auto clean = statistics(log, DfgStatisticalVariant::kClean, "n").clean;
  check("CLEAN graph keyed by an int column", counts(clean.graph), "2->1.0=1, 7->2.0=1");
  check("CLEAN starts", counts(clean.start_activities), "1=1, 2=1");
  check("CLEAN ends", counts(clean.end_activities), "1=2");
  const auto gaps = statistics(log, DfgStatisticalVariant::kClean, "t").clean;
  check("CLEAN splits a case at a missing cell", counts(gaps.graph), "");
  check("CLEAN runs' starts", counts(gaps.start_activities), "x=2, y=1");
  check("CLEAN runs' ends", counts(gaps.end_activities), "x=2, y=1");
  check("CLEAN_TIME keyed by an int column", counts(statistics(log, DfgStatisticalVariant::kCleanTime, "n").clean_time),
        "1=120, 2=90, 7=60");
  check_error("CLEAN_TIME on a missing cell", [&] { (void)statistics(log, DfgStatisticalVariant::kCleanTime, "t"); },
              "TypeError: dtype datetime64[s] cannot be converted to timedelta64[ns]");
  check_error("CLEAN_TIME keyed by the timestamp", [&] {
    (void)statistics(log, DfgStatisticalVariant::kCleanTime, "time:timestamp");
  }, "ValueError: The column label 'time:timestamp' is not unique.");
  check_error("performance DFG keyed by an int column",
              [&] { (void)statistics(log, DfgStatisticalVariant::kPerformance, "n"); },
              "the activity column should be of type string.");
  check_error("performance DFG keyed by text with a gap",
              [&] { (void)statistics(log, DfgStatisticalVariant::kPerformance, "t"); },
              "the activity column should not contain any empty value.");
}

void llm(const EventLog& log) {
  LlmAbstractionOptions options;
  options.activity_key = "n";
  check("LLM DFG keyed by an int column", llm_log_dfg_description(log, options),
        "If I have a process with flow:\n\n2 -> 1.0 ( frequency = 2  performance = 90.000 )\n"
        "7 -> 2.0 ( frequency = 1  performance = 120.000 )\n1 -> 7.0 ( frequency = 1  performance = 60.000 )\n\n\n");
  check_error("LLM variants keyed by an int column", [&] { (void)llm_log_variants_description(log, options); },
              "TypeError: sequence item 0: expected str instance, int found");
  options.activity_key = "t";
  check("LLM DFG drops the pairs of a missing cell", llm_log_dfg_description(log, options),
        "If I have a process with flow:\n\ny -> x ( frequency = 1  performance = 60.000 )\n"
        "x -> y ( frequency = 1  performance = 120.000 )\n\n\n");
  check_error("LLM variants keyed by text with a gap", [&] { (void)llm_log_variants_description(log, options); },
              "TypeError: sequence item 1: expected str instance, float found");
  options.activity_key = "case";
  check("LLM variants keyed by the case column", llm_log_variants_description(log, options),
        "If I have a process with the following process variants:\n\n c1 -> c1 -> c1 -> c1 ( frequency = 1  "
        "performance = 240.000 )\n c2 -> c2 ( frequency = 1  performance = 120.000 )\n\n\n");
  LlmAbstractionOptions budget;
  budget.max_len = 185;  // the text is exactly 185 long after the first feature
  check("LLM features stop at max_len", llm_log_features_description(log, budget),
        "\n Given the following features:\n\nValue 'index' for Event Attribute '':    number of non-zero values: 2 ; "
        "quantiles of the non-zero: {0.0: 3.0, 0.25: 3.5, 0.5: 4.0, 0.75: 4.5, 1.0: 5.0}\n");
}

void statistics_routes(const EventLog& log) {
  BoundaryActivitiesOptions boundary;
  boundary.activity_key = "n";
  check_error("start activities keyed by an int column",
              [&] { (void)discover_boundary_activities_as_uploaded(log, boundary); },
              "the activity column should be of type string.");
  boundary.activity_key = "t";
  check_error("start activities keyed by text with a gap",
              [&] { (void)discover_boundary_activities_as_uploaded(log, boundary); },
              "the activity column should not contain any empty value.");
  boundary.activity_key = "case";
  const auto by_case = discover_boundary_activities_as_uploaded(log, boundary);
  check("start activities keyed by the case column", counts(by_case.starts), "c1=1, c2=1");
  check("end activities keyed by the case column", counts(by_case.ends), "c1=1, c2=1");
  VariantStatisticsOptions variants;
  variants.activity_key = "d";
  check_error("variants keyed by a date column", [&] { (void)discover_variant_statistics_as_uploaded(log, variants); },
              "the activity column should be of type string.");
  variants.activity_key = "case";
  std::string seen;
  for (const auto& entry : discover_variant_statistics_as_uploaded(log, variants).variants) {
    for (const auto& activity : entry.activities) seen += activity + ",";
    seen += "=" + std::to_string(entry.count) + " ";
  }
  // In get_variants_sorted_by_count order: descending count, then variant.
  check("variants keyed by the case column", seen, "c2,c2,=1 c1,c1,c1,c1,=1 ");
}

}  // namespace

int main() {
  const auto log = import();
  dfg_variants(log);
  llm(log);
  statistics_routes(log);
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review BT1 checks passed\n";
  return 0;
}
