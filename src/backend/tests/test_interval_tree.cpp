#undef NDEBUG

#include "minera/interval_tree.hpp"
#include "minera/json_export.hpp"
#include "minera/timestamp.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace minera;

Event event(std::string activity, std::string timestamp) {
  Event result;
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  return result;
}

EventLog sample() {
  Trace trace;
  trace.case_id = "case-1";
  trace.events = {event("A", "2024-01-01T00:00:00Z"),
                  event("B", "2024-01-01T00:00:01Z"),
                  event("C", "2024-01-01T00:00:03Z")};
  return EventLog{{trace}};
}

void first_following_and_epsilon() {
  const auto intervals = discover_open_path_intervals(sample());
  assert(intervals.size() == 2);
  assert(intervals[0].source_activity == "A" &&
         intervals[0].target_activity == "B");
  const auto epoch = *parse_timestamp("2024-01-01T00:00:00Z");
  assert(std::abs(intervals[0].start - (epoch - 0.00001)) < 1e-7);
  assert(std::abs(intervals[0].end - (epoch + 1.00001)) < 1e-7);
  assert(intervals[1].source_activity == "B" &&
         intervals[1].target_activity == "C");
}

void filter_and_custom_attributes() {
  auto log = sample();
  for (auto& event : log.traces[0].events) {
    event.attributes.push_back({"phase", event.activity,
                                AttributeKind::kString});
    event.attributes.push_back({"started_at", event.timestamp,
                                AttributeKind::kDate});
    event.attributes.push_back({"completed_at", event.timestamp,
                                AttributeKind::kDate});
  }
  OpenPathIntervalOptions options;
  options.activity_key = "phase";
  options.start_timestamp_key = "started_at";
  options.timestamp_key = "completed_at";
  options.epsilon = 0.0;
  options.filter_activity_couple = std::pair{"A", "B"};
  const auto intervals = discover_open_path_intervals(log, options);
  assert(intervals.size() == 1);
  assert(intervals[0].source_activity == "A" &&
         intervals[0].target_activity == "B");
}

void invalid_and_empty() {
  assert(discover_open_path_intervals(EventLog{}).empty());
  auto log = sample();
  log.traces[0].events[1].timestamp = "bad";
  bool threw = false;
  try {
    (void)discover_open_path_intervals(log);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  OpenPathIntervalOptions options;
  options.epsilon = -1.0;
  threw = false;
  try {
    (void)discover_open_path_intervals(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void json_shape_is_stable() {
  const auto json = interval_tree_json(discover_open_path_intervals(sample()));
  assert(json.find(R"({"family":"LOG_TO_INTERVAL_TREE","intervals":[)" ) !=
         std::string::npos);
  assert(json.find(R"("case_id":"case-1")") != std::string::npos);
  assert(json.find(R"("source_activity":"A")") != std::string::npos);
  assert(json.find(R"("target_activity":"B")") != std::string::npos);
  assert(json.find(R"("source_event_index":0)") != std::string::npos);
}

void query_diagnostics_are_inclusive_and_indexed() {
  const auto intervals = discover_open_path_intervals(sample());
  const auto epoch = *parse_timestamp("2024-01-01T00:00:00Z");
  IntervalTreeQueryOptions options;
  options.point = epoch + 2.0;
  options.range = std::pair{epoch + 0.5, epoch + 1.5};
  const auto result = query_open_path_intervals(intervals, options);
  assert(result.intervals == intervals);
  assert(result.point && *result.point == epoch + 2.0);
  assert(result.point_matches == std::vector<std::size_t>{1});
  assert(result.range_matches == std::vector<std::size_t>({0, 1}));
  const auto json = interval_tree_json(result);
  assert(json.find(R"("point_query":)") != std::string::npos);
  assert(json.find(R"("point_matches":[1])") != std::string::npos);
  assert(json.find(R"("range_matches":[0,1])") != std::string::npos);

  bool threw = false;
  try {
    options.point = std::numeric_limits<double>::quiet_NaN();
    (void)query_open_path_intervals(intervals, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    options.point.reset();
    options.range = std::pair{2.0, 1.0};
    (void)query_open_path_intervals(intervals, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  first_following_and_epsilon();
  filter_and_custom_attributes();
  invalid_and_empty();
  json_shape_is_stable();
  query_diagnostics_are_inclusive_and_indexed();
}
