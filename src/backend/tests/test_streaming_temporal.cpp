#include "minera/streaming_temporal.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>

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
  // Reference: a zero deviation reports sys.maxsize.
  assert(result.deviations[0].standard_deviation_distance == 9223372036854775807.0);
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

// Reference values (PM4Py 2.7.22 streaming temporal profile, public calls,
// profile A->B mean 10 s, std 1 s, zeta 6).
void reference_contract() {
  minera::TemporalProfile p;
  p.entries.push_back({"A", "B", 1, 10.0, 1.0});
  const auto run = [&](std::vector<std::tuple<std::string, std::string, std::string>> events) {
    minera::StreamingTemporalConformance monitor(p);
    for (const auto& [c, a, t] : events) (void)monitor.receive(c, a, t, t);
    return monitor.snapshot();
  };
  // Strict bounds: 16 s and 4 s pass, 3 s deviates (distance 7).
  assert(run({{"c", "A", "2026-01-01T00:00:00Z"}, {"c", "B", "2026-01-01T00:00:16Z"}}).total_deviations == 0);
  assert(run({{"c", "A", "2026-01-01T00:00:00Z"}, {"c", "B", "2026-01-01T00:00:04Z"}}).total_deviations == 0);
  const auto low = run({{"c", "A", "2026-01-01T00:00:00Z"}, {"c", "B", "2026-01-01T00:00:03Z"}});
  assert(low.deviations.at("c").size() == 1 && low.deviations.at("c")[0].standard_deviation_distance == 7.0);
  // Equal timestamps pair (0 s, distance 10); a later predecessor does not.
  assert(run({{"c", "A", "2026-01-01T00:00:05Z"}, {"c", "B", "2026-01-01T00:00:05Z"}}).total_deviations == 1);
  assert(run({{"c", "A", "2026-01-01T00:00:30Z"}, {"c", "B", "2026-01-01T00:00:00Z"}}).total_deviations == 0);
  // Empty case ids and activities are values.
  minera::TemporalProfile empty_profile;
  empty_profile.entries.push_back({"", "B", 1, 10.0, 1.0});
  minera::StreamingTemporalConformance monitor(empty_profile);
  assert(monitor.receive("", "", "2026-01-01T00:00:00Z", "2026-01-01T00:00:00Z").event_index == 1);
  assert(monitor.receive("", "B", "2026-01-01T00:00:50Z", "2026-01-01T00:00:50Z").deviations.size() == 1);
}

}  // namespace

int main() {
  reference_contract();
  checks_all_prior_events_and_isolates_cases();
  handles_zero_variance_and_attributes();
  rejects_invalid_profile_options_and_resets();
  return 0;
}
