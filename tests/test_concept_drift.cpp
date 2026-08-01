#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "minera/concept_drift.hpp"

namespace {

minera::Trace trace(std::initializer_list<const char*> activities,
                    const char* id) {
  minera::Trace result;
  result.case_id = id;
  std::size_t order = 0;
  for (const char* activity : activities) {
    result.events.push_back(
        {id, activity, "2024-01-01T00:00:0" + std::to_string(order) + "Z",
         "", order++});
  }
  return result;
}

bool throws_invalid(const auto& fn) {
  try {
    fn();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  minera::EventLog log;
  log.traces = {trace({"a", "b"}, "c1"), trace({"a", "b"}, "c2"),
                trace({"a", "c"}, "c3"), trace({"a", "c"}, "c4")};

  const auto activities = minera::extract_concept_drift_activities(log);
  assert((activities == std::vector<std::string>{"a", "b", "c"}));
  // In this log a precedes b in half the cases and c in the other half.
  const auto features = minera::extract_concept_drift_features(log);
  assert(features.size() == 9);
  assert(features[0] == 0.0 && features[1] == 0.0 && features[2] == 3.0);
  assert(features[3] == 0.0 && features[4] == 1.0 && features[5] == 2.0);
  assert(features[6] == 0.0 && features[7] == 1.0 && features[8] == 2.0);

  minera::ConceptDriftOptions options;
  options.sub_log_size = 2;
  options.window_size = 1;
  options.num_permutations = 25;
  options.threshold = 1.0;
  options.max_no_change_points = 1;
  options.seed = 42;
  options.include_timestamps = true;
  const auto result = minera::detect_concept_drift(log, options);
  assert(result.sublogs.size() == 2);
  assert(result.cumulative_sublogs.size() == 2);
  assert(result.cumulative_sublogs[0].traces.size() == 2);
  assert(result.cumulative_sublogs[1].traces.size() == 4);
  assert(result.sublog_start_timestamps.size() == 2);
  assert(std::isfinite(result.sublog_start_timestamps[0]));
  assert(result.change_points.empty());  // strict p < threshold; p is 1 here

  options.threshold = 0.0;
  assert(minera::detect_concept_drift(log, options).change_points.empty());
  options.sub_log_size = 0;
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.sub_log_size = 2;
  options.threshold = 0.5;
  options.num_permutations = 0;
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.num_permutations = 10;
  options.threshold = std::nan("");
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));

  minera::EventLog bad_timestamp;
  bad_timestamp.traces.push_back(trace({"a"}, "bad"));
  bad_timestamp.traces[0].events[0].timestamp = "invalid";
  options.threshold = 0.5;
  options.include_timestamps = true;
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(bad_timestamp, options); }));

  minera::EventLog oversized;
  oversized.traces.resize(2500);
  options.include_timestamps = false;
  options.sub_log_size = 1;
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(oversized, options); }));

  minera::EventLog oversized_sublog;
  oversized_sublog.traces.resize(1'000'001);
  options.sub_log_size = oversized_sublog.traces.size();
  assert(throws_invalid(
      [&] { (void)minera::detect_concept_drift(oversized_sublog, options); }));
}
