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
  // pm4py's single pass leaves B behind the pointer once A(0, 100) passed it
  // (round FS: get_PS_dur_matrix gives 0 here, not 1 of 2).
  assert_near(result.precede_succeed_probability.at(kAB), 0.0);
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

void correlation_utility_matrix_and_transport_contract() {
  EventLog log;
  log.traces = {trace(
      "rectangular",
      {interval_event("A", 0.0, 0.0), interval_event("A", 1.0, 5.0),
       interval_event("A", 2.0, 2.0), interval_event("B", 4.0, 4.0),
       interval_event("B", 6.0, 6.0)})};
  CorrelationMinerOptions options;
  options.start_timestamp_attribute = "start";

  const auto greedy = discover_correlation_miner(log, options);
  assert_near(greedy.precede_succeed_probability.at(kAB), 2.0 / 3.0);
  assert_near(greedy.duration_matrix.at(kAB), 2.5);

  options.exact_time_matching = true;
  const auto exact = discover_correlation_miner(log, options);
  assert_near(exact.precede_succeed_probability.at(kAB), 2.0 / 3.0);
  assert_near(exact.duration_matrix.at(kAB), 1.5);

  std::map<std::string, std::uint64_t> outgoing;
  std::map<std::string, std::uint64_t> incoming;
  for (const auto& [edge, count] : exact.frequency_dfg) {
    outgoing[edge.first] += count;
    incoming[edge.second] += count;
  }
  assert(outgoing == exact.activity_counts);
  assert(incoming == exact.activity_counts);
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

// Round FS: pm4py's trace-based variant pairs activities on a projected log
// that keeps only concept:name and time:timestamp (observed black-box).
void review_fs_trace_based_reference_keys() {
  EventLog intervals;
  intervals.traces = {
      trace("1", {interval_event("A", 0.0, 1.0), interval_event("B", 3.0, 5.0)}),
      trace("2", {interval_event("A", 0.0, 2.0), interval_event("B", 1.0, 9.0)})};
  const auto message = [&](const CorrelationMinerOptions& options) {
    try {
      (void)discover_correlation_miner(intervals, options);
    } catch (const std::invalid_argument& error) {
      return std::string(error.what());
    }
    return std::string("no error");
  };
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kTraceBased;
  const auto plain = discover_correlation_miner(intervals, options);
  assert(plain.frequency_dfg.at(kAB) == 2 && plain.frequency_dfg.at(kBA) == 2);
  assert_near(plain.performance_dfg.at(kAB), 5.5);
  assert_near(plain.performance_dfg.at(kBA), 0.0);

  options.reference_timestamp_key = "time:timestamp";
  options.start_timestamp_attribute = "start";
  assert(message(options) == "KeyError: 'start'");
  options.start_timestamp_attribute = "concept:name";
  assert(message(options) ==
         "TypeError: '<' not supported between instances of 'float' and 'str'");
  options.reference_timestamp_key = "ts";
  assert(message(options) == "KeyError: 'ts'");
  options.start_timestamp_attribute.clear();
  assert(message(options) == "KeyError: 'ts'");
  // The standard keys pair as before.
  options.reference_timestamp_key = "time:timestamp";
  options.start_timestamp_attribute = "time:timestamp";
  const auto standard = discover_correlation_miner(intervals, options);
  assert(standard.frequency_dfg == plain.frequency_dfg);
  assert(standard.performance_dfg == plain.performance_dfg);
  // One activity never pairs.
  EventLog single;
  single.traces = {trace("1", {interval_event("A", 0.0, 1.0), interval_event("A", 0.0, 2.0)})};
  options.reference_timestamp_key = "ts";
  options.start_timestamp_attribute = "start";
  const auto alone = discover_correlation_miner(single, options);
  assert(alone.activity_counts.at("A") == 2);
  // The native contract (no reference key) and the classic variants read the
  // start key as given.
  options = CorrelationMinerOptions{};
  options.variant = CorrelationVariant::kTraceBased;
  options.start_timestamp_attribute = "start";
  assert(message(options) == "no error");
  options.variant = CorrelationVariant::kClassic;
  options.reference_timestamp_key = "time:timestamp";
  assert(message(options) == "no error");
}

// Round FS: the precede/succeed count walks the start-sorted lists once, so
// with a start key a nested interval (A at 18-19 inside A at 0-20) counts
// only the targets after its predecessor's: pm4py's probability is 0.5 here,
// the full pairwise count 0.75 (observed black-box).
void review_fs_nested_interval_precedence() {
  EventLog log;
  log.traces = {
      trace("1", {interval_event("A", 0.0, 20.0), interval_event("A", 18.0, 19.0),
                  interval_event("B", 20.0, 21.0)}),
      trace("2", {interval_event("B", 59.0, 60.0)})};
  CorrelationMinerOptions options;
  options.start_timestamp_attribute = "start";
  for (const auto variant : {CorrelationVariant::kClassic, CorrelationVariant::kClassicSplit}) {
    options.variant = variant;
    const auto result = discover_correlation_miner(log, options);
    assert_near(result.precede_succeed_probability.at(kAB), 0.5);
    assert_near(result.precede_succeed_probability.at(kBA), 0.0);
    assert_near(result.duration_matrix.at(kAB), 39.0);
    assert(result.frequency_dfg.at(kAB) == 2 && result.frequency_dfg.at(kBA) == 2);
    assert_near(result.performance_dfg.at(kAB), 39.0);
  }
}

// Round FT: pm4py's exact time matching is networkx's minimum weight full
// matching, i.e. SciPy's linear_sum_assignment on (target start - source
// completion), sys.maxsize for a target that starts before the source
// completes, with the targets as columns in CPython set order. The maxsize
// entries absorb the real costs next to them, so the matching is SciPy's,
// not a minimum one: the reference values below are pm4py's on the same
// events (observed black-box).
void review_ft_exact_time_matching_assignment() {
  CorrelationMinerOptions options;
  options.exact_time_matching = true;
  EventLog atomic;
  atomic.traces = {trace("1", {event_at("A", 271.0), event_at("A", 301.0), event_at("A", 361.0),
                               event_at("B", 180.0), event_at("B", 781.0), event_at("B", 782.0)})};
  const auto first = discover_correlation_miner(atomic, options);
  assert_near(first.duration_matrix.at(kAB), 495.5);
  assert_near(first.duration_matrix.at(kBA), 91.0);

  options.start_timestamp_attribute = "start";
  EventLog early;
  early.traces = {trace("1", {interval_event("A", 100.0, 100.0), interval_event("A", 101.0, 101.0),
                              interval_event("B", 95.0, 100.0), interval_event("B", 110.0, 115.0)})};
  assert_near(discover_correlation_miner(early, options).duration_matrix.at(kAB), 10.0);

  // Four targets after five sources wrap the set's table: the last target
  // is the first column.
  EventLog wrapped;
  wrapped.traces = {trace("1", {interval_event("A", 10.0, 10.0), interval_event("A", 20.0, 20.0),
                                interval_event("A", 30.0, 30.0), interval_event("A", 40.0, 40.0),
                                interval_event("A", 50.0, 50.0), interval_event("B", 15.0, 16.0),
                                interval_event("B", 5.0, 6.0), interval_event("B", 45.0, 46.0),
                                interval_event("B", 60.0, 61.0)})};
  const auto wrap = discover_correlation_miner(wrapped, options);
  assert_near(wrap.duration_matrix.at(kAB), 20.0 / 3.0);
  assert_near(wrap.duration_matrix.at(kBA), 4.0);
}

// Round FT: with another completion key and no start key, pm4py's classic
// variants start each event at its time:timestamp (A 0-10, B 12-20 pair at
// 2 seconds, not at the 10 between the completions; observed black-box).
void review_ft_classic_default_start_key() {
  const auto timed = [](std::string activity, double start, double complete) {
    Event event = event_at(std::move(activity), start);
    event.attributes.push_back({"ts", format_timestamp(complete), AttributeKind::kDate});
    return event;
  };
  EventLog log;
  log.traces = {trace("1", {timed("A", 0.0, 10.0), timed("B", 12.0, 20.0)})};
  CorrelationMinerOptions options;
  options.timestamp_attribute = "ts";
  options.reference_timestamp_key = "ts";
  for (const auto variant : {CorrelationVariant::kClassic, CorrelationVariant::kClassicSplit}) {
    options.variant = variant;
    options.sample_size = 2;
    const auto result = discover_correlation_miner(log, options);
    assert_near(result.duration_matrix.at(kAB), 2.0);
    assert_near(result.performance_dfg.at(kAB), 2.0);
  }
  // The native contract (no reference key) keeps the completion as start.
  options.reference_timestamp_key.clear();
  options.variant = CorrelationVariant::kClassic;
  assert_near(discover_correlation_miner(log, options).duration_matrix.at(kAB), 10.0);
}

// Round FT: pm4py sorts the frame's own timestamps, at a CSV upload's
// nanoseconds, before it cuts classic_split's samples: d completing at
// :30.000000000 comes before d at :30.000000001, so the first sample pairs a
// with the d that starts at :29 (pm4py: 28.876543045043945; observed
// black-box).
void review_ft_split_samples_sort_at_nanoseconds() {
  const auto ns = [](std::string activity, const char* complete, const char* start) {
    Event event;
    event.activity = std::move(activity);
    event.timestamp = complete;
    event.attributes.push_back({"st", start, AttributeKind::kDate});
    return event;
  };
  EventLog log;
  log.traces = {trace("c0", {ns("a", "2020-01-01T06:20:30.123456789+05:30", "2020-01-01T06:20:30.000000500+05:30"),
                             ns("d", "2020-01-01T06:20:30.000000001+05:30", "2020-01-01T06:20:10.999999999+05:30"),
                             ns("d", "2020-01-01T06:20:30+05:30", "2020-01-01T06:20:29+05:30"),
                             ns("a", "2020-01-01T06:20:00.123456789+05:30", "2020-01-01T06:18:30+05:30")})};
  CorrelationMinerOptions options;
  options.variant = CorrelationVariant::kClassicSplit;
  options.sample_size = 2;
  options.start_timestamp_attribute = "st";
  const auto result = discover_correlation_miner(log, options);
  const DfgActivityPair ad{"a", "d"};
  assert_near(result.duration_matrix.at(ad), 28.876543045043945);
  assert_near(result.performance_dfg.at(ad), 28.876543045043945);
}

int main() {
  review_ft_exact_time_matching_assignment();
  review_ft_classic_default_start_key();
  review_ft_split_samples_sort_at_nanoseconds();
  review_fs_nested_interval_precedence();
  review_fs_trace_based_reference_keys();
  classic_matches_reference_fixture();
  trace_based_keeps_case_boundaries_and_ignores_exact_matching();
  split_uses_raw_chunks_and_reference_aggregation();
  transportation_can_force_self_and_reverse_edges();
  empty_and_single_activity_logs_are_supported();
  greedy_matching_preserves_zero_when_one_direction_is_empty();
  exact_matching_accepts_equal_boundaries_and_large_populations();
  correlation_utility_matrix_and_transport_contract();
  custom_interval_key_is_used_and_timestamp_errors_are_strict();
  split_rejects_zero_sample_size();
  transportation_is_deterministic_and_balanced();
  return 0;
}
