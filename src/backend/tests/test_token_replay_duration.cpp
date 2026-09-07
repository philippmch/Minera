#include "minera/conformance.hpp"
#include "minera/timestamp.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace minera;

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Exception = std::invalid_argument, typename Function>
void rejects(Function function) {
  bool rejected = false;
  try { function(); } catch (const Exception&) { rejected = true; }
  require(rejected, "expected contract rejection");
}

Event event(std::string activity, std::string timestamp) {
  Event value;
  value.activity = std::move(activity);
  value.timestamp = std::move(timestamp);
  return value;
}

Trace timed_trace(double seconds, std::string first = "a", std::string last = "b") {
  Trace trace;
  trace.case_id = "duplicate-case-id";
  trace.events = {event(std::move(first), "1970-01-01T00:00:00Z"),
                  event(std::move(last), format_timestamp(seconds))};
  return trace;
}

PetriNet sequence() {
  PetriNet net;
  net.set_activities({"a", "b"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  return net;
}

void scalar_contracts() {
  rejects([&] { (void)token_replay_case_duration(Trace{}); });
  Trace singleton;
  singleton.events = {event("a", "")};
  rejects([&] { (void)token_replay_case_duration(singleton); });
  singleton.events.front().timestamp = "1970-01-01T00:00:00Z";
  require(token_replay_case_duration(singleton) == 0, "singleton duration");
  auto trace = timed_trace(10);
  trace.events.insert(trace.events.begin() + 1, event("middle", "invalid"));
  require(token_replay_case_duration(trace) == 10, "only endpoints are read");
  trace.events[1].timestamp = "1970-01-01T00:00:30Z";
  require(token_replay_case_duration(trace) == 10, "no timestamp sorting");
  trace = timed_trace(30);
  std::swap(trace.events.front(), trace.events.back());
  require(token_replay_case_duration(trace) == -30, "negative duration retained");
  trace.events.front().timestamp = "1970-01-01T01:00:00.250+01:00";
  trace.events.back().timestamp = "1970-01-01T00:00:01.500Z";
  require(token_replay_case_duration(trace) == 1.25, "timezone and fractional seconds");
  trace.events.front().timestamp_state = FirstClassAttributeState::kNull;
  rejects([&] { (void)token_replay_case_duration(trace); });
  rejects([&] { (void)token_replay_case_duration(trace, ""); });
  trace = timed_trace(10);
  trace.events.front().attributes.push_back({"completed", "1970-01-01T00:00:20Z"});
  trace.events.back().attributes.push_back({"completed", "1970-01-01T00:00:50Z"});
  require(token_replay_case_duration(trace, "completed") == 30, "custom selector");
  trace.events.front().attributes.push_back(trace.events.front().attributes.front());
  rejects([&] { (void)token_replay_case_duration(trace, "completed"); });
  trace.events.front().attributes.pop_back();
  trace.events.front().attributes.front().is_null = true;
  rejects([&] { (void)token_replay_case_duration(trace, "completed"); });
  rejects([&] { (void)token_replay_case_duration(trace, "absent"); });
  trace = timed_trace(10);
  for (auto& value : trace.events) value.timestamp_source_key = "source-time";
  require(token_replay_case_duration(trace, "source-time") == 10, "source timestamp alias");
  trace.events.front().attributes.push_back({"time:timestamp", "1970-01-01T00:00:05Z"});
  require(token_replay_case_duration(trace) == 5, "exact timestamp cell takes priority");

  EventLog log;
  log.traces = {timed_trace(30), timed_trace(10), timed_trace(20), Trace{}};
  require(token_replay_median_case_duration(log, {1}) == 10, "one median");
  require(token_replay_median_case_duration(log, {1, 2}) == 15, "even median");
  require(token_replay_median_case_duration(log, {0, 1, 2}) == 20, "odd median");
  require(token_replay_median_case_duration(log, {0, 1, 1}) == 10,
          "selected duplicate indices retain weighting");
  rejects([&] { (void)token_replay_median_case_duration(log, {}); });
  rejects([&] { (void)token_replay_median_case_duration(log, {3}); });
  rejects<std::out_of_range>([&] { (void)token_replay_median_case_duration(log, {4}); });
  rejects([&] { (void)token_replay_median_case_duration(log, {1}, ""); });
}

void transition_contracts() {
  EventLog log;
  log.traces = {timed_trace(10), timed_trace(30), timed_trace(80), timed_trace(100)};
  auto rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {2, 2}}});
  require(rows.size() == 1 && rows[0].transition_index == 7, "explicit transition identity");
  const auto& row = rows.front();
  require(row.fit_trace_indices == std::vector<std::size_t>({0, 1, 3}) &&
          row.problem_trace_indices == std::vector<std::size_t>({2}),
          "baseline from full log, cohort deduplicated by source identity");
  require(row.fit_count == 3 && row.problem_count == 1 &&
          row.fit_median_seconds == 30 && row.problem_median_seconds == 80 &&
          std::abs(row.relative_throughput - 8.0 / 3) < 1e-12, "transition summary");
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {0, 2}}});
  require(rows[0].fit_median_seconds == 65 && rows[0].problem_median_seconds == 45,
          "even cohorts");
  log.traces[1].events.back().activity = "c";
  log.traces[3].events.back().activity = "c";
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {2}}});
  require(rows[0].fit_trace_indices == std::vector<std::size_t>({0}), "label baseline");
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {1}}});
  require(rows[0].fit_median_seconds == 45 && rows[0].problem_median_seconds == 30,
          "problem trace need not contain transition label");
  TokenReplayOptions options;
  options.activity_attribute = "phase";
  for (auto& trace : log.traces)
    for (auto& value : trace.events) value.attributes.push_back({"phase", "b"});
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {2}}}, options);
  require(rows[0].fit_trace_indices == std::vector<std::size_t>({0, 1, 3}),
          "transition baseline uses selected activity attribute");
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {1}}}, options);
  require(rows[0].fit_median_seconds == 80 && rows[0].relative_throughput == 0.375,
          "custom transition filter contract");
  require(token_replay_transition_duration_diagnostics(log, {{7, "b", {}}}).empty(),
          "empty problem cohort omitted");
  require(token_replay_transition_duration_diagnostics(log, {{7, "b", {0, 2}}}).empty(),
          "empty fit cohort omitted");
  log.traces[0] = timed_trace(0);
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {2}}});
  require(rows[0].relative_throughput == 0, "zero denominator");
  log.traces[0] = timed_trace(-10);
  rows = token_replay_transition_duration_diagnostics(log, {{7, "b", {2}}});
  require(rows[0].relative_throughput == 0, "negative denominator");
  rejects<std::out_of_range>([&] {
    (void)token_replay_transition_duration_diagnostics(log, {{7, "b", {4}}});
  });
  EventLog null_log;
  null_log.traces = {timed_trace(10), timed_trace(30)};
  for (auto& trace : null_log.traces) {
    trace.events.back().activity.clear();
    trace.events.back().activity_state = FirstClassAttributeState::kNull;
  }
  require(token_replay_transition_duration_diagnostics(null_log, {{9, std::nullopt, {1}}})
              .front().relative_throughput == 3, "null transition label membership");
  auto missing = timed_trace(80);
  for (auto& value : missing.events) value.activity.clear();
  null_log.traces.push_back(missing);
  require(token_replay_transition_duration_diagnostics(null_log, {{9, std::nullopt, {1}}})
              .front().fit_trace_indices == std::vector<std::size_t>({0}),
          "missing activity differs from null membership");
  require(token_replay_unknown_activity_duration_diagnostics(null_log, {{"", true, {1}}})
              .front().fit_trace_indices == std::vector<std::size_t>({2}),
          "unknown null filter retains traces missing activity");
  null_log.traces.pop_back();
  for (auto& trace : null_log.traces)
    trace.events.back().activity_state = FirstClassAttributeState::kPresent;
  require(token_replay_transition_duration_diagnostics(null_log, {{9, "", {1}}})
              .front().relative_throughput == 3, "empty transition label membership");
}

void unknown_contracts() {
  EventLog log;
  log.traces = {timed_trace(10), timed_trace(30), timed_trace(80, "a", "X"),
                timed_trace(100, "a", "Y")};
  auto rows = token_replay_unknown_activity_duration_diagnostics(
      log, {{"X", false, {2, 2}}, {"Y", false, {3}}});
  require(rows.size() == 2 && rows[0].fit_count == 2 && rows[1].fit_count == 2 &&
          rows[0].fit_median_seconds == 20 && rows[0].problem_median_seconds == 80 &&
          rows[0].relative_throughput == 4, "unknown baseline excludes all named keys");
  TokenReplayOptions selected;
  selected.activity_attribute = "phase";
  selected.timestamp_attribute = "completed";
  for (auto& trace : log.traces) {
    for (auto& value : trace.events) value.attributes.push_back({"phase", "b"});
    trace.events.front().attributes.push_back({"completed", format_timestamp(0)});
    trace.events.back().attributes.push_back({"completed", format_timestamp(
        2 * token_replay_case_duration(trace))});
  }
  rows = token_replay_unknown_activity_duration_diagnostics(
      log, {{"X", false, {2}}, {"Y", false, {3}}}, selected);
  require(rows[0].fit_count == 3 && rows[0].fit_median_seconds == 60 &&
          rows[0].problem_median_seconds == 160, "custom activity and timestamp filters");
  rows = token_replay_unknown_activity_duration_diagnostics(
      log, {{"X", false, {2}}, {"Y", false, {}}});
  require(rows.size() == 1 && rows[0].fit_count == 2, "empty cohort still filters baseline");
  rows = token_replay_unknown_activity_duration_diagnostics(log, {{"X", false, {2}}});
  require(rows[0].fit_count == 3 && rows[0].fit_median_seconds == 30, "only supplied keys filter");
  for (auto& trace : log.traces) trace.events.back().activity = "b";
  rows = token_replay_unknown_activity_duration_diagnostics(
      log, {{"X", false, {2}}, {"Y", false, {3}}});
  require(rows[0].fit_trace_indices == std::vector<std::size_t>({0, 1, 3}) &&
          rows[1].fit_trace_indices == std::vector<std::size_t>({0, 1, 2}),
          "only current row problem membership is excluded");
  require(token_replay_unknown_activity_duration_diagnostics(
      log, {{"b", false, {2}}}).empty(), "unknown empty baseline omitted");
  require(token_replay_unknown_activity_duration_diagnostics(log, {}).empty(), "no unknown keys");
  TokenReplayOptions options;
  options.timestamp_attribute.clear();
  rejects([&] { (void)token_replay_unknown_activity_duration_diagnostics(log, {}, options); });
  rejects([&] { (void)token_replay_transition_duration_diagnostics(log, {}, options); });
}

void integration_contracts() {
  EventLog log;
  log.traces = {timed_trace(10), timed_trace(30), timed_trace(80, "b", "b"), timed_trace(100)};
  log.traces[3].events.insert(log.traces[3].events.begin() + 1, event("X", "invalid-middle"));
  const auto net = sequence();
  const auto plain = check_conformance(log, net);
  require(!plain.duration_diagnostics, "diagnostics disabled by default");
  TokenReplayOptions options;
  options.include_duration_diagnostics = true;
  auto result = check_conformance(log, net, options);
  require(result.duration_diagnostics.has_value() && !result.trace_diagnostics,
          "duration opt-in independent from trace opt-in");
  require(result.fitness == plain.fitness && result.precision == plain.precision &&
          result.generalization == plain.generalization && result.simplicity == plain.simplicity &&
          result.produced == plain.produced && result.consumed == plain.consumed &&
          result.missing == plain.missing && result.remaining == plain.remaining &&
          result.fitting_traces == plain.fitting_traces &&
          result.transition_frequencies == plain.transition_frequencies,
          "duration diagnostics leave numeric replay unchanged");
  const auto& duration = *result.duration_diagnostics;
  require(duration.transitions.size() == 1 && duration.transitions[0].transition_index == 1 &&
          duration.transitions[0].problem_trace_indices == std::vector<std::size_t>({2}) &&
          duration.transitions[0].fit_trace_indices == std::vector<std::size_t>({0, 1, 3}),
          "repeated transition problems retain one source trace membership");
  require(duration.unknown_activities.size() == 1 && duration.unknown_activities[0].activity == "X" &&
          duration.unknown_activities[0].problem_trace_indices == std::vector<std::size_t>({3}),
          "unknown duration uses original source trace");
  auto empty_selector = options;
  empty_selector.activity_attribute.clear();
  const auto normalized = check_conformance(log, net, empty_selector);
  const auto same_comparison = [](const TokenReplayDurationComparison& left,
                                  const TokenReplayDurationComparison& right) {
    return left.fit_trace_indices == right.fit_trace_indices &&
           left.problem_trace_indices == right.problem_trace_indices &&
           left.fit_count == right.fit_count && left.problem_count == right.problem_count &&
           left.fit_median_seconds == right.fit_median_seconds &&
           left.problem_median_seconds == right.problem_median_seconds &&
           left.relative_throughput == right.relative_throughput;
  };
  require(normalized.activity_attribute == result.activity_attribute &&
          normalized.fitness == result.fitness &&
          normalized.duration_diagnostics->transitions.size() == duration.transitions.size() &&
          normalized.duration_diagnostics->unknown_activities.size() == duration.unknown_activities.size() &&
          normalized.duration_diagnostics->transitions[0].transition_index == duration.transitions[0].transition_index &&
          normalized.duration_diagnostics->unknown_activities[0].activity == duration.unknown_activities[0].activity &&
          same_comparison(normalized.duration_diagnostics->transitions[0], duration.transitions[0]) &&
          same_comparison(normalized.duration_diagnostics->unknown_activities[0], duration.unknown_activities[0]),
          "integrated empty activity selector uses canonical default for both comparisons");
  const auto empty_default = check_conformance(EventLog{}, net, options);
  const auto empty_normalized = check_conformance(EventLog{}, net, empty_selector);
  require(empty_normalized.activity_attribute == empty_default.activity_attribute &&
          empty_normalized.duration_diagnostics.has_value() &&
          empty_normalized.duration_diagnostics->transitions.empty() &&
          empty_normalized.duration_diagnostics->unknown_activities.empty() &&
          empty_normalized.fitness == empty_default.fitness,
          "integrated empty selector accepts empty logs");
  options.include_trace_diagnostics = true;
  result = check_conformance(log, net, options);
  require(result.trace_diagnostics->size() == 4, "trace and duration diagnostics together");
  options.stop_immediately_unfit = true;
  result = check_conformance(log, net, options);
  require(result.duration_diagnostics->transitions.front().problem_median_seconds == 80,
          "stopped replay still uses full source case duration");
  auto traces = replay_token_traces(log, net);
  traces[2].transitions_with_problems.clear();
  require(compute_token_replay_duration_diagnostics(log, net, traces).transitions.size() == 1,
          "computed cohorts consume event diagnostics");
  for (auto& row : traces[2].event_diagnostics) row.transitions_with_problems.clear();
  require(compute_token_replay_duration_diagnostics(log, net, traces).transitions.empty(),
          "aggregate problems do not supply event cohorts");
  options.timestamp_attribute.clear();
  rejects([&] { (void)check_conformance(log, net, options); });
  EventLog untimed;
  untimed.traces = {timed_trace(10)};
  for (auto& value : untimed.traces.front().events) value.timestamp.clear();
  options = {};
  options.include_duration_diagnostics = true;
  require(check_conformance(untimed, net, options).duration_diagnostics->transitions.empty(),
          "timestamps not demanded when no comparisons exist");
}
}  // namespace

int main() {
  try {
    scalar_contracts();
    transition_contracts();
    unknown_contracts();
    integration_contracts();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
