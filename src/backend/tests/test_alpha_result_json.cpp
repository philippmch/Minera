#include <cassert>
#include <stdexcept>
#include "minera/alpha_result_json.hpp"

int main() {
  minera::EventLog log;
  log.traces.push_back({});
  const auto result = minera::discover_alpha_plus_result(log);
  const auto json = minera::alpha_plus_result_json(result);
  assert(json.find("\"family\":\"ALPHA_PLUS\"") != std::string::npos);
  assert(json.find("\"detached_initial_marking\":[{\"name\":{\"before\":[\"artificial_start\"],\"after\":[\"artificial_end\"]},\"tokens\":1}]") != std::string::npos);
  assert(json.find("\"initial\":0,\"final\":1") != std::string::npos);
  auto bad = result;
  bad.initial_marking.clear();
  bool rejected = false;
  try { (void)minera::alpha_plus_result_json(bad); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  bad = result;
  bad.net.add_transition(std::nullopt, {bad.net.place_count()}, {});
  rejected = false;
  try { (void)minera::alpha_plus_result_json(bad); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  auto escaped = result;
  escaped.detached_initial_marking.front().name.before = {"quoted\"\nlabel"};
  assert(minera::alpha_plus_result_json(escaped).find("quoted\\\"\\u000alabel") != std::string::npos);
  log.traces.front().events.push_back({"case", "a"});
  const auto ordinary = minera::alpha_plus_result_json(minera::discover_alpha_plus_result(log));
  assert(ordinary.find("\"detached_initial_marking\":[]") != std::string::npos);
  assert(ordinary.find("\"label\":\"a\"") != std::string::npos);
  log.traces.front().events.front().activity_state = minera::FirstClassAttributeState::kNull;
  const auto nullable = minera::alpha_plus_result_json(minera::discover_alpha_plus_result(log));
  assert(nullable.find("\"after\":[null]") != std::string::npos);
  assert(nullable.find("\"label\":null") != std::string::npos);
  auto mixed = result;
  mixed.detached_initial_marking.front().name.before = {std::nullopt, "", "None"};
  assert(minera::alpha_plus_result_json(mixed).find("\"before\":[null,\"\",\"None\"]") != std::string::npos);
}
