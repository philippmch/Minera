#include "minera/passed_time.hpp"
#include "minera/json_export.hpp"

#undef NDEBUG
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

minera::Event event(std::string activity, std::string timestamp) {
  minera::Event value;
  value.activity = std::move(activity);
  value.timestamp = std::move(timestamp);
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.events = {event("A", "2024-01-01T00:00:00Z"),
                  event("B", "2024-01-01T00:10:00Z"),
                  event("C", "2024-01-01T00:20:00Z")};
  minera::Trace second;
  second.case_id = "c2";
  second.events = {event("A", "2024-01-02T00:00:00Z"),
                   event("B", "2024-01-02T00:30:00Z"),
                   event("C", "2024-01-02T00:50:00Z")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

// Reference capture (pm4py 2.7.22 passed_time PREPOST for activity A, stable
// across hash seeds): pre [[C,3600,1],[B,900,2],[D,300,1]] avg 1425, post
// [[D,1800,1],[C,1800,1],[B,300,1]] avg 1300. Rows follow the first
// occurrence of each directly-follows pair in log order.
void reference_first_seen_order() {
  minera::EventLog log;
  const auto add = [&](std::string id, std::initializer_list<std::pair<const char*, const char*>> events) {
    minera::Trace trace;
    trace.case_id = std::move(id);
    for (const auto& [activity, time] : events) trace.events.push_back(event(activity, time));
    log.traces.push_back(std::move(trace));
  };
  add("c1", {{"C", "2024-01-01T00:00:00"}, {"A", "2024-01-01T01:00:00"}, {"D", "2024-01-01T01:30:00"}});
  add("c2", {{"B", "2024-01-01T00:00:00"}, {"A", "2024-01-01T00:10:00"}, {"C", "2024-01-01T00:40:00"}});
  add("c3", {{"D", "2024-01-01T00:00:00"}, {"A", "2024-01-01T00:05:00"}, {"B", "2024-01-01T00:10:00"}});
  add("c4", {{"B", "2024-01-01T00:00:00"}, {"A", "2024-01-01T00:20:00"}});
  minera::PassedTimeOptions options;
  options.activity = "A";
  options.variant = minera::PassedTimeVariant::kPrePost;
  const auto result = minera::discover_passed_time(log, options);
  const std::vector<std::tuple<std::string, double, std::uint64_t>> pre = {
      {"C", 3600.0, 1}, {"B", 900.0, 2}, {"D", 300.0, 1}};
  const std::vector<std::tuple<std::string, double, std::uint64_t>> post = {
      {"D", 1800.0, 1}, {"C", 1800.0, 1}, {"B", 300.0, 1}};
  const auto check = [](const auto& rows, const auto& expected) {
    assert(rows.size() == expected.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      assert(rows[i].activity == std::get<0>(expected[i]));
      assert(std::abs(rows[i].performance - std::get<1>(expected[i])) < 1e-9);
      assert(rows[i].frequency == std::get<2>(expected[i]));
    }
  };
  check(result.pre, pre);
  check(result.post, post);
  assert(std::abs(result.pre_avg_perf - 1425.0) < 1e-9);
  assert(std::abs(result.post_avg_perf - 1300.0) < 1e-9);
}

}  // namespace

int main() {
  reference_first_seen_order();
  const auto log = fixture();

  minera::PassedTimeOptions pre;
  pre.activity = "B";
  const auto before = minera::discover_passed_time(log, pre);
  assert(before.variant == minera::PassedTimeVariant::kPre);
  assert(before.pre.size() == 1);
  assert(before.pre.front().activity == "A");
  assert(before.pre.front().frequency == 2);
  assert(std::abs(before.pre.front().performance - 1200.0) < 1e-9);
  assert(std::abs(before.pre_avg_perf - 1200.0) < 1e-9);
  assert(before.post.empty());

  pre.variant = minera::PassedTimeVariant::kPost;
  const auto after = minera::discover_passed_time(log, pre);
  assert(after.pre.empty());
  assert(after.post.size() == 1);
  assert(after.post.front().activity == "C");
  assert(after.post.front().frequency == 2);
  assert(std::abs(after.post.front().performance - 900.0) < 1e-9);

  pre.variant = minera::PassedTimeVariant::kPrePost;
  const auto both = minera::discover_passed_time(log, pre);
  assert(both.pre.size() == 1 && both.post.size() == 1);
  assert(std::abs(both.post_avg_perf - 900.0) < 1e-9);
  const auto json = minera::passed_time_json(both);
  assert(json.find("\"family\":\"PASSED_TIME\"") != std::string::npos);
  assert(json.find("\"variant\":\"PREPOST\"") != std::string::npos);
  assert(json.find("\"frequency\":2") != std::string::npos);

  auto custom = fixture();
  for (auto& trace : custom.traces) {
    for (auto& item : trace.events) {
      item.attributes.push_back({"phase", item.activity,
                                 minera::AttributeKind::kString});
    }
  }
  pre.variant = minera::PassedTimeVariant::kPre;
  pre.activity_key = "phase";
  pre.timestamp_key = "timestamp";
  const auto custom_result = minera::discover_passed_time(custom, pre);
  assert(custom_result.pre == before.pre);

  pre.activity_key = "activity";
  pre.performance_aggregation = minera::DfgPerformanceAggregation::kRawValues;
  bool rejected = false;
  try {
    (void)minera::discover_passed_time(log, pre);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  minera::EventLog singleton;
  minera::Trace lone;
  lone.case_id = "singleton";
  lone.events.push_back(event("A", ""));
  singleton.traces.push_back(std::move(lone));
  pre.performance_aggregation = minera::DfgPerformanceAggregation::kMean;
  pre.timestamp_key = "missing_timestamp";
  const auto empty = minera::discover_passed_time(singleton, pre);
  assert(empty.pre.empty() && empty.post.empty());

  std::cout << "Passed-time tests passed\n";
}
