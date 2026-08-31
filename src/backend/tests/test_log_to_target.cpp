#include "minera/log_to_target.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

minera::Event event(const char* activity, const char* timestamp = "") {
  minera::Event result;
  result.activity = activity;
  result.timestamp = timestamp;
  return result;
}

minera::EventLog sample() {
  minera::Trace first;
  first.events = {event("B", "2024-01-01T00:00:00.000Z"),
                  event("A", "2024-01-01T00:00:01.500Z"),
                  event("C", "2024-01-01T00:00:04+00:00")};
  minera::Trace second;
  second.events = {event("A", "2024-01-01T01:00:00+01:00")};
  return minera::EventLog{{first, second}};
}

void next_activity_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::NEXT_ACTIVITY;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::NEXT_ACTIVITY);
  assert((result.classes == std::vector<std::string>{"A", "B", "C"}));
  assert(result.rows.size() == 2);
  assert(result.trace_count == 2 && result.target_width == 3);
  assert(!result.padded);
  assert(result.rows[0].size() == 3);
  assert(result.rows[0][0][0] == 1.0 && result.rows[0][0][1] == 0.0);
  assert(result.rows[0][1][2] == 1.0);
  assert(std::all_of(result.rows[0][2].begin(), result.rows[0][2].end(),
                     [](double value) { return value == 0.0; }));
}

void next_time_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::NEXT_TIME;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::NEXT_TIME);
  assert((result.classes == std::vector<std::string>{"@@next_time"}));
  assert(result.target_width == 1 && result.rows.size() == 2);
  assert(std::abs(result.rows[0][0][0] - 1.5) < 1e-9);
  assert(std::abs(result.rows[0][1][0] - 2.5) < 1e-9);
  assert(result.rows[0][2][0] == 0.0);
  assert(result.rows[1].size() == 1 && result.rows[1][0][0] == 0.0);
}

void remaining_time_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::REMAINING_TIME;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::REMAINING_TIME);
  assert((result.classes ==
          std::vector<std::string>{"@@remaining_time"}));
  assert(result.target_width == 1 && result.rows.size() == 2);
  assert(std::abs(result.rows[0][0][0] - 4.0) < 1e-9);
  assert(std::abs(result.rows[0][1][0] - 2.5) < 1e-9);
  assert(result.rows[0][2][0] == 0.0);
  assert(result.rows[1].size() == 1 && result.rows[1][0][0] == 0.0);
}

void options_and_errors() {
  minera::LogToTargetOptions options;
  options.activities = {"C", "A"};
  options.enable_padding = true;
  options.pad_size = 4;
  const auto result = minera::log_to_target(sample(), options);
  assert((result.classes == std::vector<std::string>{"C", "A"}));
  assert(result.rows[0].size() == 4 && result.rows[1].size() == 4);
  assert(result.padded && result.trace_count == 2 && result.target_width == 2);

  bool threw = false;
  try {
    options.enable_padding = false;
    (void)minera::log_to_target(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    options.enable_padding = true;
    options.pad_size = 1;
    (void)minera::log_to_target(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    options.activities = {"A", "B", "A"};
    options.enable_padding = false;
    options.pad_size = 0;
    (void)minera::log_to_target(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  minera::Trace custom;
  custom.events = {event("ignored"), event("ignored")};
  custom.events[0].attributes.push_back(
      {"my.activity", "A", minera::AttributeKind::kString});
  custom.events[1].attributes.push_back(
      {"my.activity", "B", minera::AttributeKind::kString});
  minera::LogToTargetOptions custom_options;
  custom_options.activity_key = "my.activity";
  const auto custom_result =
      minera::log_to_target(minera::EventLog{{custom}}, custom_options);
  assert((custom_result.classes == std::vector<std::string>{"A", "B"}));
  assert(custom_result.rows[0][0][1] == 1.0);
}

void empty_and_invalid_time() {
  const auto empty = minera::log_to_target(minera::EventLog{});
  assert(empty.rows.empty() && empty.classes.empty());
  minera::LogToTargetOptions explicit_options;
  explicit_options.activities = {"A"};
  const auto explicit_empty =
      minera::log_to_target(minera::EventLog{}, explicit_options);
  assert(explicit_empty.rows.empty());
  assert((explicit_empty.classes == std::vector<std::string>{"A"}));
  minera::LogToTargetOptions next_only;
  next_only.timestamp_key.clear();
  assert(minera::log_to_target(sample(), next_only).rows.size() == 2);
  minera::LogToTargetOptions temporal_only;
  temporal_only.activity_key.clear();
  temporal_only.variant = minera::LogToTargetVariant::NEXT_TIME;
  const auto temporal_result = minera::log_to_target(sample(), temporal_only);
  assert(temporal_result.rows.size() == 2);
  assert((temporal_result.classes ==
          std::vector<std::string>{"@@next_time"}));
  minera::Trace trace;
  trace.events = {event("A", "bad")};
  bool threw = false;
  try {
    (void)minera::log_to_target(minera::EventLog{{trace}},
                                {.variant = minera::LogToTargetVariant::NEXT_TIME});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void json_shape() {
  const auto result = minera::log_to_target(sample());
  const auto json = minera::log_to_target_json(result);
  assert(json.find("\"family\":\"LOG_TO_TARGET\"") != std::string::npos);
  assert(json.find("\"variant\":\"next_activity\"") != std::string::npos);
  assert(json.find("\"trace_count\":2") != std::string::npos);
  assert(json.find("\"target_width\":3") != std::string::npos);
  assert(json.find("\"padded\":false") != std::string::npos);
  assert(json.find("\"classes\":[\"A\",\"B\",\"C\"]") !=
         std::string::npos);
  assert(json.find("\"rows\":[") != std::string::npos);

  auto invalid = result;
  invalid.variant = static_cast<minera::LogToTargetVariant>(99);
  bool threw = false;
  try {
    (void)minera::log_to_target_json(invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  next_activity_contract();
  next_time_contract();
  remaining_time_contract();
  options_and_errors();
  empty_and_invalid_time();
  json_shape();
}
