#include "minera/chaotic_activities.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace {

minera::EventLog log() {
  minera::EventLog result;
  result.traces.push_back({"c1", {{"c1", "A", "", "", 0, {}, ""},
                                  {"c1", "B", "", "", 1, {}, ""}}});
  result.traces.push_back({"c2", {{"c2", "A", "", "", 0, {}, ""},
                                  {"c2", "C", "", "", 1, {}, ""}}});
  result.traces.push_back({"c3", {{"c3", "A", "", "", 0, {}, ""},
                                  {"c3", "B", "", "", 1, {}, ""}}});
  return result;
}

void computes_deterministic_entropy_metrics() {
  minera::ChaoticActivitiesOptions options;
  options.alpha = 0.0;
  const auto first = minera::discover_chaotic_activities(log(), options);
  const auto second = minera::discover_chaotic_activities(log(), options);
  assert(first.activities.size() == 3);
  assert(first.activities == second.activities);
  assert(first.diagnostics.activity_count == 3);
  assert(first.activities[0].frequency >= first.activities[1].frequency);
  for (const auto& row : first.activities) {
    assert(row.frequency > 0);
    assert(std::isfinite(row.entropy));
    assert(std::isfinite(row.entropy_smooth));
    assert(std::isfinite(row.entropy_gain));
    assert(std::isfinite(row.chaotic_score));
  }
}

void supports_custom_activity_attributes_and_validation() {
  auto source = log();
  for (auto& trace : source.traces)
    for (auto& event : trace.events)
      event.attributes.push_back({"label", event.activity,
                                  minera::AttributeKind::kString});
  minera::ChaoticActivitiesOptions options;
  options.activity_key = "label";
  options.alpha = 1.0;
  const auto result = minera::discover_chaotic_activities(source, options);
  assert(result.activities.size() == 3);
  assert(result.diagnostics.alpha == 1.0);

  options.alpha = -1.0;
  bool threw = false;
  try { (void)minera::discover_chaotic_activities(source, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

void serializes_a_stable_json_envelope() {
  const auto result = minera::discover_chaotic_activities(log());
  const auto json = minera::chaotic_activities_json(result);
  assert(json.find("\"family\":\"CHAOTIC_ACTIVITIES\"") !=
         std::string::npos);
  assert(json.find("\"variant\":\"NIEK_SIDOROVA\"") !=
         std::string::npos);
  assert(json.find("\"diagnostics\":") != std::string::npos);
  assert(json == minera::chaotic_activities_json(result));
}

}  // namespace

int main() {
  computes_deterministic_entropy_metrics();
  supports_custom_activity_attributes_and_validation();
  serializes_a_stable_json_envelope();
}
