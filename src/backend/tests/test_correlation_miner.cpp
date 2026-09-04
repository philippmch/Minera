#include "minera/correlation_miner.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

using namespace minera;

namespace {

constexpr double kTolerance = 1.0e-9;

Event event_at(std::string activity, double complete) {
  Event event;
  event.activity = std::move(activity);
  event.timestamp = format_timestamp(complete);
  return event;
}

Event interval_event(std::string activity, double start, double complete,
                     std::string key = "start") {
  Event event = event_at(std::move(activity), complete);
  event.attributes.push_back(
      {std::move(key), format_timestamp(start), AttributeKind::kDate});
  return event;
}

Trace trace(std::string case_id, std::vector<Event> events) {
  Trace result;
  result.case_id = std::move(case_id);
  result.events = std::move(events);
  return result;
}

EventLog fixture() {
  EventLog log;
  log.traces = {
      trace("1", {event_at("A", 0.0), event_at("B", 10.0)}),
      trace("2", {event_at("A", 20.0), event_at("B", 50.0)}),
  };
  return log;
}

const DfgActivityPair kAA{"A", "A"};
const DfgActivityPair kAB{"A", "B"};
const DfgActivityPair kBA{"B", "A"};
const DfgActivityPair kBB{"B", "B"};

void assert_near(double actual, double expected) {
  if (std::abs(actual - expected) > kTolerance) {
    std::fprintf(stderr, "expected %.17g, got %.17g\n", expected, actual);
  }
  assert(std::abs(actual - expected) <= kTolerance);
}

void assert_complete_two_activity_matrices(
    const CorrelationMinerResult& result) {
  assert(result.precede_succeed_probability.size() == 4);
  assert(result.duration_matrix.size() == 4);
  assert_near(result.precede_succeed_probability.at(kAA), 0.0);
  assert_near(result.precede_succeed_probability.at(kBB), 0.0);
  assert_near(result.duration_matrix.at(kAA), 0.0);
  assert_near(result.duration_matrix.at(kBB), 0.0);
}

void classic_matches_reference_fixture() {
  const auto result = discover_correlation_miner(fixture());
  assert(result.variant == CorrelationVariant::kClassic);
  assert(result.frequency_dfg.at(kAB) == 2);
  assert(result.frequency_dfg.at(kBA) == 2);
  assert_near(result.performance_dfg.at(kAB), 20.0);
  assert_near(result.performance_dfg.at(kBA), 10.0);
  assert_near(result.precede_succeed_probability.at(kAB), 0.75);
  assert_near(result.precede_succeed_probability.at(kBA), 0.25);
  assert_near(result.duration_matrix.at(kAB), 20.0);
  assert_near(result.duration_matrix.at(kBA), 10.0);
  assert_complete_two_activity_matrices(result);
}

void trace_based_keeps_case_boundaries_and_ignores_exact_matching() {
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kTraceBased;
  const auto result = discover_correlation_miner(fixture(), options);
  assert(result.frequency_dfg.at(kAB) == 2);
  assert(result.frequency_dfg.at(kBA) == 2);
  assert_near(result.performance_dfg.at(kAB), 20.0);
  assert_near(result.performance_dfg.at(kBA), 0.0);
  assert_near(result.precede_succeed_probability.at(kAB), 1.0);
  assert_near(result.precede_succeed_probability.at(kBA), 0.0);
  assert_near(result.duration_matrix.at(kAB), 20.0);
  assert_near(result.duration_matrix.at(kBA), 0.0);

  options.exact_time_matching = true;
  const auto exact_option = discover_correlation_miner(fixture(), options);
  assert(exact_option.frequency_dfg == result.frequency_dfg);
  assert(exact_option.performance_dfg == result.performance_dfg);
  assert(exact_option.precede_succeed_probability ==
         result.precede_succeed_probability);
  assert(exact_option.duration_matrix == result.duration_matrix);
}

void split_uses_raw_chunks_and_reference_aggregation() {
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kClassicSplit;
  options.sample_size = 2;
  const auto split = discover_correlation_miner(fixture(), options);
  assert_near(split.precede_succeed_probability.at(kAB), 1.0);
  assert_near(split.precede_succeed_probability.at(kBA), 0.0);
  assert_near(split.duration_matrix.at(kAB), 30.0);
  assert_near(split.duration_matrix.at(kBA), 0.0);
  assert_near(split.performance_dfg.at(kAB), 30.0);

  options.sample_size = fixture().event_count();
  const auto one_chunk = discover_correlation_miner(fixture(), options);
  const auto classic = discover_correlation_miner(fixture());
  assert(one_chunk.frequency_dfg == classic.frequency_dfg);
  assert(one_chunk.performance_dfg == classic.performance_dfg);
  assert(one_chunk.precede_succeed_probability ==
         classic.precede_succeed_probability);
  assert(one_chunk.duration_matrix == classic.duration_matrix);

  EventLog raw_order;
  raw_order.traces = {
      trace("a", {event_at("A", 100.0), event_at("A", 0.0)}),
      trace("b", {event_at("B", 10.0), event_at("B", 110.0)}),
  };
  options.sample_size = 2;
  const auto raw_chunks =
      discover_correlation_miner(raw_order, options);
  assert_near(raw_chunks.precede_succeed_probability.at(kAB), 0.0);
  assert_near(raw_chunks.precede_succeed_probability.at(kBA), 0.0);
  assert_near(raw_chunks.duration_matrix.at(kAB), 0.0);
  assert_near(raw_chunks.duration_matrix.at(kBA), 0.0);
}

void transportation_can_force_self_and_reverse_edges() {
  EventLog forced;
  forced.traces = {trace(
      "forced",
      {event_at("A", 0.0), event_at("A", 100.0),
       event_at("B", 50.0)})};
  const auto result = discover_correlation_miner(forced);
  assert(result.frequency_dfg.at(kAA) == 1);
  assert(result.frequency_dfg.at(kAB) == 1);
  assert(result.frequency_dfg.at(kBA) == 1);
  assert_near(result.performance_dfg.at(kAA), 0.0);
  assert_near(result.performance_dfg.at(kAB), 50.0);
  assert_near(result.performance_dfg.at(kBA), 50.0);

  EventLog ordered;
  ordered.traces = {
      trace("ordered", {event_at("A", 0.0), event_at("B", 10.0)})};
  const auto reverse = discover_correlation_miner(ordered);
  assert(reverse.frequency_dfg.at(kAB) == 1);
  assert(reverse.frequency_dfg.at(kBA) == 1);
  assert_near(reverse.performance_dfg.at(kAB), 10.0);
  assert_near(reverse.performance_dfg.at(kBA), 0.0);
}

void empty_and_single_activity_logs_are_supported() {
  const auto empty = discover_correlation_miner(EventLog{});
  assert(empty.frequency_dfg.empty());
  assert(empty.performance_dfg.empty());
  assert(empty.precede_succeed_probability.empty());
  assert(empty.duration_matrix.empty());

  EventLog single;
  single.traces = {
      trace("single", {event_at("A", 0.0), event_at("A", 1.0),
                       event_at("A", 2.0)})};
  const auto result = discover_correlation_miner(single);
  assert(result.frequency_dfg.size() == 1);
  assert(result.frequency_dfg.at(kAA) == 3);
  assert(result.performance_dfg.at(kAA) == 0.0);
  assert(result.precede_succeed_probability.size() == 1);
  assert(result.duration_matrix.size() == 1);
  assert(result.precede_succeed_probability.at(kAA) == 0.0);
  assert(result.duration_matrix.at(kAA) == 0.0);
}

void greedy_matching_preserves_zero_when_one_direction_is_empty() {
  EventLog log;
  log.traces = {trace(
      "intervals",
      {interval_event("A", 0.0, 100.0),
       interval_event("A", 1.0, 2.0),
       interval_event("B", 3.0, 3.0)})};
  CorrelationMinerOptions options;
  options.start_timestamp_attribute = "start";
  const auto result = discover_correlation_miner(log, options);
  assert_near(result.precede_succeed_probability.at(kAB), 0.5);
  assert_near(result.duration_matrix.at(kAB), 0.0);
}

void exact_matching_accepts_equal_boundaries_and_large_populations() {
  EventLog equal;
  equal.traces = {trace(
      "equal",
      {interval_event("A", 0.0, 10.0),
       interval_event("B", 10.0, 20.0)})};
  CorrelationMinerOptions options;
  options.start_timestamp_attribute = "start";
  options.exact_time_matching = true;
  const auto equality = discover_correlation_miner(equal, options);
  assert_near(equality.precede_succeed_probability.at(kAB), 0.0);
  assert_near(equality.duration_matrix.at(kAB), 0.0);
  assert_near(equality.duration_matrix.at(kBA), 0.0);

  EventLog large;
  std::vector<Event> events;
  for (int index = 0; index < 30; ++index) {
    events.push_back(event_at("A", static_cast<double>(index)));
  }
  for (int index = 0; index < 30; ++index) {
    events.push_back(event_at("B", 100.0 + index));
  }
  large.traces = {trace("large", std::move(events))};
  options.start_timestamp_attribute.clear();
  const auto result = discover_correlation_miner(large, options);
  assert_near(result.precede_succeed_probability.at(kAB), 1.0);
  assert_near(result.duration_matrix.at(kAB), 100.0);
  assert_near(result.duration_matrix.at(kBA), 0.0);
}

void custom_interval_key_is_used_and_timestamp_errors_are_strict() {
  EventLog valid;
  valid.traces = {trace(
      "valid",
      {interval_event("A", 0.0, 10.0, "began"),
       interval_event("B", 20.0, 30.0, "began")})};
  CorrelationMinerOptions options;
  options.start_timestamp_attribute = "began";
  const auto result = discover_correlation_miner(valid, options);
  assert_near(result.precede_succeed_probability.at(kAB), 1.0);
  assert_near(result.duration_matrix.at(kAB), 10.0);

  const auto expect_invalid = [](EventLog log,
                                 const CorrelationMinerOptions& settings) {
    bool threw = false;
    try {
      (void)discover_correlation_miner(log, settings);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };

  EventLog missing;
  missing.traces = {
      trace("missing", {event_at("A", 0.0)})};
  expect_invalid(missing, options);

  Event invalid_start = event_at("A", 1.0);
  invalid_start.attributes.push_back(
      {"began", "not-a-timestamp", AttributeKind::kDate});
  EventLog bad_start;
  bad_start.traces = {trace("bad-start", {invalid_start})};
  expect_invalid(bad_start, options);

  EventLog bad_complete;
  Event broken = event_at("A", 1.0);
  broken.timestamp = "not-a-timestamp";
  bad_complete.traces = {trace("bad-complete", {broken})};
  CorrelationMinerOptions atomic;
  expect_invalid(bad_complete, atomic);
}

void split_rejects_zero_sample_size() {
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kClassicSplit;
  options.sample_size = 0;
  bool threw = false;
  try {
    (void)discover_correlation_miner(fixture(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void transportation_is_deterministic_and_balanced() {
  EventLog tied;
  tied.traces = {trace(
      "tied",
      {event_at("A", 0.0), event_at("A", 0.0),
       event_at("B", 0.0), event_at("B", 0.0)})};
  const auto first = discover_correlation_miner(tied);
  const auto second = discover_correlation_miner(tied);
  assert(first.frequency_dfg == second.frequency_dfg);
  assert(first.performance_dfg == second.performance_dfg);

  const std::map<std::string, std::uint64_t> expected{
      {"A", 2}, {"B", 2}};
  std::map<std::string, std::uint64_t> outgoing;
  std::map<std::string, std::uint64_t> incoming;
  for (const auto& [edge, count] : first.frequency_dfg) {
    outgoing[edge.first] += count;
    incoming[edge.second] += count;
  }
  assert(outgoing == expected);
  assert(incoming == expected);
}

}  // namespace

int main() {
  classic_matches_reference_fixture();
  trace_based_keeps_case_boundaries_and_ignores_exact_matching();
  split_uses_raw_chunks_and_reference_aggregation();
  transportation_can_force_self_and_reverse_edges();
  empty_and_single_activity_logs_are_supported();
  greedy_matching_preserves_zero_when_one_direction_is_empty();
  exact_matching_accepts_equal_boundaries_and_large_populations();
  custom_interval_key_is_used_and_timestamp_errors_are_strict();
  split_rejects_zero_sample_size();
  transportation_is_deterministic_and_balanced();
  return 0;
}
