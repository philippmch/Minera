#include "minera/streaming_temporal.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

minera::TemporalProfile profile() {
  minera::TemporalProfile result;
  result.entries.push_back({"A", "B", 2, 10.0, 2.0});
  result.entries.push_back({"A", "C", 1, 5.0, 0.0});
  return result;
}

void checks_all_prior_events_and_isolates_cases() {
  minera::StreamingTemporalConformance monitor(profile(), {.zeta = 2.0});
  auto first = monitor.receive("c1", "A", "2024-01-01T00:00:00Z",
                               "2024-01-01T00:00:01Z");
  assert(first.event_index == 1 && first.deviations.empty());
  auto second = monitor.receive("c1", "B", "2024-01-01T00:00:20Z",
                                "2024-01-01T00:00:21Z");
  assert(second.event_index == 2 && second.deviations.size() == 1);
  assert(second.deviations[0].elapsed_seconds == 19.0);
  auto other = monitor.receive("c2", "B", "2024-01-01T00:00:20Z",
                               "2024-01-01T00:00:21Z");
  assert(other.deviations.empty());
  const auto snapshot = monitor.snapshot();
  assert(snapshot.total_events_processed == 3);
  assert(snapshot.total_deviations == 1);
  assert(snapshot.cases.at("c1").deviations == 1);
}

void handles_zero_variance_and_attributes() {
  minera::StreamingTemporalConformance monitor(profile(),
                                                {.activity_attribute = "kind",
                                                 .start_timestamp_attribute = "begin",
                                                 .timestamp_attribute = "finish",
                                                 .case_id_attribute = "cid",
                                                 .zeta = 1.0});
  minera::Event a;
  a.case_id = "c";
  a.attributes = {{"kind", "A"}, {"begin", "2024-01-01T00:00:00Z"},
                  {"finish", "2024-01-01T00:00:01Z"}, {"cid", "c"}};
  minera::Event b;
  b.case_id = "c";
  b.attributes = {{"kind", "C"}, {"begin", "2024-01-01T00:00:10Z"},
                  {"finish", "2024-01-01T00:00:11Z"}, {"cid", "c"}};
  assert(monitor.receive(a).event_index == 1);
  const auto result = monitor.receive(b);
  assert(result.deviations.size() == 1);
  assert(result.deviations[0].standard_deviation_distance ==
         std::numeric_limits<double>::max());
  const auto json = minera::serialize_streaming_temporal_json(monitor.snapshot());
  assert(json.find("STREAMING_TEMPORAL") != std::string::npos);
}

void rejects_invalid_profile_options_and_resets() {
  bool rejected = false;
  try {
    minera::StreamingTemporalConformance invalid(profile(), {.zeta = 0.0});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  minera::StreamingTemporalConformance monitor(profile());
  assert(monitor.receive("c", "A", "bad", "bad").event_index == 0);
  monitor.reset_all();
  assert(monitor.snapshot().total_events_processed == 0);
}

}  // namespace

int main() {
  checks_all_prior_events_and_isolates_cases();
  handles_zero_variance_and_attributes();
  rejects_invalid_profile_options_and_resets();
  return 0;
}
