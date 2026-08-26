#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "minera/concept_drift.hpp"
#include "minera/json_export.hpp"

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

  minera::EventLog projected = log;
  for (auto& current : projected.traces) {
    current.attributes.push_back({"case:order", "batch-" + current.case_id});
    for (auto& event : current.events) {
      event.attributes.push_back({"event:type",
                                  event.activity == "a" ? "Alpha" : "Beta"});
      event.attributes.push_back({"started_at", event.timestamp});
    }
  }
  minera::ConceptDriftOptions projected_options;
  projected_options.variant = minera::ConceptDriftVariant::BOSE;
  projected_options.sub_log_size = 2;
  projected_options.window_size = 1;
  projected_options.num_permutations = 5;
  projected_options.threshold = 0.0;
  projected_options.include_timestamps = true;
  projected_options.activity_attribute = "event:type";
  projected_options.timestamp_attribute = "started_at";
  projected_options.case_id_attribute = "case:order";
  const auto projected_result =
      minera::detect_concept_drift(projected, projected_options);
  assert((projected_result.activities ==
          std::vector<std::string>{"Alpha", "Beta"}));
  assert(projected_result.variant == minera::ConceptDriftVariant::kBose);
  assert(projected_result.activity_attribute == "event:type");
  assert(projected_result.timestamp_attribute == "started_at");
  assert(projected_result.case_id_attribute == "case:order");
  assert(projected_result.sublog_start_timestamps.size() == 2);
  const auto projected_json = minera::concept_drift_json(projected_result);
  assert(projected_json.find("\"activity_attribute\":\"event:type\"") !=
         std::string::npos);
  assert(projected_json.find("\"timestamp_attribute\":\"started_at\"") !=
         std::string::npos);
  assert(projected_json.find("\"variant\":\"bose\"") !=
         std::string::npos);

  minera::ConceptDriftOptions options;
  options.sub_log_size = 2;
  options.window_size = 1;
  options.num_permutations = 25;
  options.threshold = 1.0;
  options.max_no_change_points = 1;
  options.seed = 42;
  options.include_timestamps = true;
  const auto result = minera::detect_concept_drift(log, options);
  assert(result.variant == minera::ConceptDriftVariant::kBose);
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

  options.threshold = 0.5;
  options.variant = static_cast<minera::ConceptDriftVariant>(99);
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.variant = minera::ConceptDriftVariant::kBose;

  options.threshold = 0.5;
  options.activity_attribute.clear();
  assert(throws_invalid([&] { (void)minera::detect_concept_drift(log, options); }));
  options.activity_attribute = "concept:name";
  options.case_id_attribute = "missing_case_key";
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
