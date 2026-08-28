#include "minera/log_to_features.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

#include "minera/json_export.hpp"

using namespace minera;

static EventLog fixture() {
  EventLog log;
  Trace t; t.case_id = "c1";
  t.attributes.push_back({"segment", "gold", AttributeKind::kString});
  Event a{"c1", "A", "2024-01-01T00:00:00Z", "r1", 0};
  a.attributes.push_back({"amount", "2.5", AttributeKind::kFloat});
  a.attributes.push_back({"kind", "x", AttributeKind::kString});
  Event b{"c1", "B", "2024-01-01T01:00:00Z", "r1", 1};
  b.attributes.push_back({"amount", "3", AttributeKind::kFloat});
  t.events = {a, b}; log.traces.push_back(t); return log;
}

static Event timed_event(const std::string& case_id, const std::string& activity,
                         const std::string& start, const std::string& finish,
                         std::size_t order) {
  Event event{case_id, activity, finish, "r1", order};
  event.attributes.push_back({"start_time", start, AttributeKind::kDate});
  return event;
}

static void observed_path_timing_uses_configured_starts() {
  EventLog log;
  Trace paths;
  paths.case_id = "paths";
  paths.events = {
      timed_event("paths", "A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", 0),
      timed_event("paths", "B", "2024-01-01T00:00:15Z", "2024-01-01T00:00:30Z", 1),
      timed_event("paths", "A", "2024-01-01T00:00:35Z", "2024-01-01T00:00:40Z", 2),
      timed_event("paths", "B", "2024-01-01T00:00:20Z", "2024-01-01T00:00:42Z", 3),
      timed_event("paths", "C", "2024-01-01T00:00:50Z", "2024-01-01T00:01:00Z", 4),
  };
  log.traces.push_back(paths);
  Trace self_loop;
  self_loop.case_id = "self";
  self_loop.events = {
      timed_event("self", "A", "2024-01-01T00:01:00Z", "2024-01-01T00:01:10Z", 0),
      timed_event("self", "A", "2024-01-01T00:01:15Z", "2024-01-01T00:01:20Z", 1),
  };
  log.traces.push_back(self_loop);

  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.start_timestamp_key = "start_time";
  options.enable_succ_def_representation = false;
  options.enable_direct_paths_times_last_occ = true;
  options.enable_indirect_paths_times_last_occ = true;
  const auto result = log_to_features(log, options);

  const auto find_feature = [&](const std::string& name) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), name);
  };
  const auto direct_ab = find_feature("directPathPerformanceLastOcc@@A##B");
  assert(direct_ab != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(direct_ab - result.feature_names.begin())] == 5.0);
  assert(find_feature("directPathPerformanceLastOcc@@A##C") == result.feature_names.end());
  assert(find_feature("directPathPerformanceLastOcc@@A##A") != result.feature_names.end());

  const auto indirect_ac = find_feature("indirectPathPerformanceLastOcc@@A##C");
  assert(indirect_ac != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(indirect_ac - result.feature_names.begin())] == 10.0);
  const auto indirect_aa = find_feature("indirectPathPerformanceLastOcc@@A##A");
  assert(indirect_aa != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(indirect_aa - result.feature_names.begin())] == 25.0);
}

int main() {
  const auto log = fixture();
  LogToFeaturesOptions e; e.variant = LogToFeaturesVariant::EVENT_BASED; e.event_numeric_attributes = {"amount"}; e.event_string_attributes = {"kind"}; e.case_string_attributes = {"segment"};
  const auto er = log_to_features(log, e);
  assert(er.event_rows.size() == 1 && er.event_rows[0].size() == 2);
  assert(!er.feature_names.empty());
  assert(std::find(er.feature_names.begin(), er.feature_names.end(), "event:amount") != er.feature_names.end());
  e.min_num_different_string_values = 1;
  const auto er_strings = log_to_features(log, e);
  assert(std::find(er_strings.feature_names.begin(), er_strings.feature_names.end(), "event:kind@x") != er_strings.feature_names.end());
  EventLog ragged = log;
  ragged.traces.push_back(Trace{"c2", {Event{"c2", "A", "2024-01-01T02:00:00Z", "r2", 2}}});
  const auto rectangular = log_to_features(ragged, e);
  assert(rectangular.event_rows.size() == 2 && rectangular.event_rows[0].size() == 2 && rectangular.event_rows[1].size() == 2);
  assert(std::all_of(rectangular.event_rows[1][1].begin(), rectangular.event_rows[1][1].end(), [](double value) { return value == 0.0; }));
  const auto event_json = log_to_features_json(er);
  assert(event_json.find("\"family\":\"LOG_TO_FEATURES\"") != std::string::npos);
  assert(event_json.find("\"event_rows\"") != std::string::npos);
  e.variant = LogToFeaturesVariant::TRACE_BASED;
  const auto tr = log_to_features(log, e);
  assert(tr.trace_rows.size() == 1 && tr.trace_rows[0].size() == tr.feature_names.size());
  assert(std::find(tr.feature_names.begin(), tr.feature_names.end(), "activity:A") != tr.feature_names.end());
  e.enable_all_extra_features = true;
  e.add_case_identifier_column = true;
  const auto rich = log_to_features(log, e);
  assert(rich.case_ids.size() == 1 && rich.case_ids[0] == "c1");
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@caseDuration") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@max_concurrent_activities_general") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "succession:activity@A#B") != rich.feature_names.end());
  e.variant = LogToFeaturesVariant::TEMPORAL; e.window_seconds = 3600;
  const auto tm = log_to_features(log, e);
  assert(tm.temporal_rows.size() == 2 && tm.temporal_rows[0].values.size() == tm.feature_names.size());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "average_service_time") != tm.feature_names.end());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "number_of_cases") != tm.feature_names.end());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "unique_resources") != tm.feature_names.end());
  assert(log_to_features_json(tm).find("\"temporal_rows\"") != std::string::npos);
  bool threw = false; try { e.window_seconds = 0; (void)log_to_features(log, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog bad = log; bad.traces[0].events[0].timestamp = "nonsense"; e.window_seconds = 3600; threw = false;
  try { (void)log_to_features(bad, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog empty;
  const auto empty_temporal = log_to_features(empty, e);
  assert(empty_temporal.temporal_rows.empty() && !empty_temporal.feature_names.empty());
  observed_path_timing_uses_configured_starts();
}
