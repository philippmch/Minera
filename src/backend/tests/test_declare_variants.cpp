#include "minera/declare_playout.hpp"
#include "minera/declare_streaming.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace minera;

namespace {

DeclareModel unary(const char* family, const char* a = "A") {
  DeclareModel model;
  model.add(family, {a, {}, false, 99, 99});
  return model;
}

DeclareModel binary(const char* family, const char* a = "A",
                    const char* b = "B") {
  DeclareModel model;
  model.add(family, {a, b, true, 99, 99});
  return model;
}

DeclareStreamingEventResult put(DeclareStreamingConformance& monitor,
                                std::string case_id, std::string activity,
                                std::string timestamp = {}) {
  return monitor.receive(std::move(case_id), std::move(activity),
                         std::move(timestamp));
}

void expect_no_deviation(const DeclareModel& model,
                         const std::vector<std::string>& activities) {
  DeclareStreamingConformance monitor(model);
  for (const auto& activity : activities)
    assert(put(monitor, "positive", activity).deviations == 0);
}

void expect_violation(const DeclareModel& model,
                      const std::vector<std::string>& prefix,
                      const std::string& violating_activity) {
  DeclareStreamingConformance monitor(model);
  for (const auto& activity : prefix)
    assert(put(monitor, "negative", activity).deviations == 0);
  const auto result = put(monitor, "negative", violating_activity);
  assert(result.deviations == 1);
  assert(result.violated_constraints.size() == 1);
  // Violations are sticky: the automaton remains violated, but is counted only
  // on the event that first entered the state.
  assert(put(monitor, "negative", "Z").deviations == 0);
  const auto snapshot = monitor.snapshot();
  assert(snapshot.total_deviations == 1);
  assert(snapshot.cases.at("negative").deviations == 1);
  assert(snapshot.cases.at("negative").constraint_states[0].state ==
         "violated");
}

void all_streaming_automata() {
  expect_no_deviation(unary(kDeclareExistence), {"B", "A", "B"});
  expect_no_deviation(unary(kDeclareExactlyOne), {"B", "A", "B"});
  expect_violation(unary(kDeclareExactlyOne), {"B", "A", "B"}, "A");
  expect_no_deviation(unary(kDeclareInit), {"A", "B", "A"});
  expect_violation(unary(kDeclareInit), {}, "B");
  expect_no_deviation(unary(kDeclareAbsence), {"B", "C"});
  expect_violation(unary(kDeclareAbsence), {"B"}, "A");

  expect_no_deviation(binary(kDeclareRespondedExistence), {"A", "C", "B"});
  expect_no_deviation(binary(kDeclareResponse), {"A", "C", "B", "A"});
  expect_no_deviation(binary(kDeclarePrecedence), {"A", "C", "B"});
  expect_violation(binary(kDeclarePrecedence), {"C"}, "B");
  expect_no_deviation(binary(kDeclareSuccession), {"A", "C", "B"});
  expect_violation(binary(kDeclareSuccession), {"C"}, "B");
  expect_no_deviation(binary(kDeclareAltResponse),
                      {"A", "C", "B", "A", "B"});
  expect_violation(binary(kDeclareAltResponse), {"A", "C"}, "A");
  expect_no_deviation(binary(kDeclareAltPrecedence), {"A", "B", "A", "B"});
  expect_violation(binary(kDeclareAltPrecedence), {"A", "B"}, "B");
  expect_no_deviation(binary(kDeclareAltSuccession), {"A", "B", "A", "B"});
  expect_violation(binary(kDeclareAltSuccession), {"A", "B"}, "B");
  expect_no_deviation(binary(kDeclareChainResponse), {"C", "A", "B"});
  expect_violation(binary(kDeclareChainResponse), {"A"}, "C");
  expect_no_deviation(binary(kDeclareChainPrecedence), {"C", "A", "B"});
  expect_violation(binary(kDeclareChainPrecedence), {"C"}, "B");
  expect_no_deviation(binary(kDeclareChainSuccession), {"A", "B", "A", "B"});
  expect_violation(binary(kDeclareChainSuccession), {"A"}, "C");
  expect_no_deviation(binary(kDeclareCoexistence), {"A", "C", "B"});
  expect_no_deviation(binary(kDeclareNonCoexistence), {"A", "C"});
  expect_violation(binary(kDeclareNonCoexistence), {"A", "C"}, "B");
  expect_no_deviation(binary(kDeclareNonSuccession), {"B", "C", "A"});
  expect_violation(binary(kDeclareNonSuccession), {"A", "C"}, "B");
  expect_no_deviation(binary(kDeclareNonChainSuccession), {"A", "C", "B"});
  expect_violation(binary(kDeclareNonChainSuccession), {"A"}, "B");
}

void snapshots_are_lossless_and_fallbacks_reset() {
  DeclareModel model;
  model.add(kDeclareAbsence, {"A", {}, false, 0, 0});
  model.add(kDeclareAbsence, {"A", {}, false, 1, 1});
  model.add("vendor", {"A,B", "C", true, 0, 0});
  DeclareStreamingConformance monitor(model);
  auto first = put(monitor, "", "A", "");
  assert(first.case_id == "undefined_case");
  assert(first.timestamp == "1");
  assert(first.event_index == 1);
  assert(first.deviations == 2);
  assert(first.violated_constraints.size() == 2);
  assert(first.violated_constraints[0].constraint_index !=
         first.violated_constraints[1].constraint_index);
  auto second = put(monitor, "", "B", "2026-01-01T00:00:00Z");
  assert(second.timestamp == "2026-01-01T00:00:00Z");
  const auto snapshot = monitor.snapshot();
  const auto& states = snapshot.cases.at("undefined_case").constraint_states;
  assert(states.size() == 3);
  assert(states[0].constraint_index == 0);
  assert(states[1].constraint_index == 1);
  assert(states[0].template_name == kDeclareAbsence);
  assert(states[0].first == states[1].first);
  assert(states[2].template_name == "vendor");
  assert(states[2].first == "A,B");
  assert(states[2].second == "C");
  assert(states[2].binary);
  assert(states[2].state == "init");

  monitor.reset_case("");
  assert(monitor.snapshot().cases.empty());
  put(monitor, "case", "B");
  monitor.reset_all();
  assert(monitor.snapshot().total_events_processed == 0);
  assert(monitor.snapshot().deviations_per_time.empty());
}

void model_arity_is_validated() {
  DeclareModel bad_unary;
  bad_unary.add(kDeclareExistence, {"A", "B", true, 0, 0});
  DeclareModel bad_binary;
  bad_binary.add(kDeclareResponse, {"A", {}, false, 0, 0});
  for (const auto* model : {&bad_unary, &bad_binary}) {
    bool streaming_threw = false;
    try {
      DeclareStreamingConformance monitor(*model);
    } catch (const std::invalid_argument&) {
      streaming_threw = true;
    }
    assert(streaming_threw);
    bool playout_threw = false;
    try {
      DeclarePlayoutOptions options;
      options.n_traces = 0;
      (void)declare_playout(*model, options);
    } catch (const std::invalid_argument&) {
      playout_threw = true;
    }
    assert(playout_threw);
  }

  // Unknown templates retain their declared arity and stay inert.
  DeclareModel unknown_unary;
  unknown_unary.add("vendor", {"X", "ignored", false, 0, 0});
  DeclareStreamingConformance unary_monitor(unknown_unary);
  assert(put(unary_monitor, "c", "X").deviations == 0);
  DeclareModel unknown_binary;
  unknown_binary.add("vendor", {"X", "Y", true, 0, 0});
  DeclareStreamingConformance binary_monitor(unknown_binary);
  assert(put(binary_monitor, "c", "Y").deviations == 0);
}

void concurrent_receive_and_snapshot() {
  DeclareStreamingConformance monitor(unary(kDeclareExistence));
  constexpr int kThreads = 4;
  constexpr int kEvents = 250;
  std::atomic<bool> receivers_done = false;
  std::thread observer([&] {
    while (!receivers_done.load()) {
      const auto current = monitor.snapshot();
      assert(current.total_events_processed ==
             current.deviations_per_time.size());
    }
  });
  std::vector<std::thread> receivers;
  for (int thread = 0; thread < kThreads; ++thread) {
    receivers.emplace_back([&, thread] {
      for (int i = 0; i < kEvents; ++i)
        (void)put(monitor, "case-" + std::to_string(thread), "A");
    });
  }
  for (auto& receiver : receivers) receiver.join();
  receivers_done = true;
  observer.join();
  const auto result = monitor.snapshot();
  assert(result.total_events_processed == kThreads * kEvents);
  assert(result.deviations_per_time.size() == kThreads * kEvents);
  assert(result.cases.size() == kThreads);
  for (const auto& [_, row] : result.cases) assert(row.events == kEvents);
}

void playout_contract() {
  DeclarePlayoutOptions options;
  options.n_traces = 2;
  options.min_length = 3;
  options.max_length = 3;
  options.seed = 7;
  DeclareModel empty;
  const auto empty_log = declare_playout(empty, options);
  assert(empty_log.traces.size() == 2);
  assert(empty_log.traces[0].events.empty());

  // The only mentioned activity is forbidden, so the trace stops immediately.
  const auto stopped = declare_playout(unary(kDeclareAbsence), options);
  for (const auto& trace : stopped.traces) assert(trace.events.empty());

  // Precedence/alternate/chain precedence all exclude B at the first step.
  for (const auto* family :
       {kDeclarePrecedence, kDeclareAltPrecedence, kDeclareChainPrecedence}) {
    options.n_traces = 10;
    options.min_length = options.max_length = 1;
    const auto generated = declare_playout(binary(family), options);
    for (const auto& trace : generated.traces) {
      assert(trace.events.size() == 1);
      assert(trace.events[0].activity == "A");
    }
  }

  // Candidate simulation must be cloned. Once A occurred, exactly-one rejects
  // A without contaminating the accepted B candidate's state.
  DeclareModel clone_model = unary(kDeclareExactlyOne);
  clone_model.add("vendor", {"B", {}, false, 0, 0});
  options.n_traces = 50;
  options.min_length = options.max_length = 6;
  options.seed = 120;
  const auto clone_log = declare_playout(clone_model, options);
  for (const auto& trace : clone_log.traces) {
    int count_a = 0;
    for (const auto& event : trace.events)
      if (event.activity == "A") ++count_a;
    assert(count_a <= 1);
  }

  // Response/existence obligations are not finalized; a pending final A is
  // therefore a valid generated prefix.
  options.n_traces = 100;
  options.min_length = options.max_length = 1;
  options.seed = 44;
  const auto pending = declare_playout(binary(kDeclareResponse), options);
  bool saw_pending_a = false;
  for (const auto& trace : pending.traces)
    saw_pending_a |= trace.events[0].activity == "A";
  assert(saw_pending_a);

  // Every individual known template produces prefixes accepted by its own
  // immediate automaton.
  const char* unary_families[] = {kDeclareExistence, kDeclareExactlyOne,
                                  kDeclareInit, kDeclareAbsence};
  const char* binary_families[] = {
      kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
      kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
      kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
      kDeclareChainSuccession, kDeclareCoexistence, kDeclareNonCoexistence,
      kDeclareNonSuccession, kDeclareNonChainSuccession};
  options.n_traces = 8;
  options.min_length = 0;
  options.max_length = 5;
  options.seed = 99;
  auto assert_prefix_safe = [&](const DeclareModel& model) {
    const auto generated = declare_playout(model, options);
    for (const auto& trace : generated.traces) {
      DeclareStreamingConformance monitor(model);
      for (const auto& event : trace.events)
        assert(monitor.receive(event).deviations == 0);
    }
  };
  for (const auto* family : unary_families) assert_prefix_safe(unary(family));
  for (const auto* family : binary_families) assert_prefix_safe(binary(family));

  DeclareModel unknown = binary("unknown", "X", "Y");
  options.n_traces = 200;
  options.min_length = 2;
  options.max_length = 4;
  options.seed = 42;
  const auto first = declare_playout(unknown, options);
  const auto replay = declare_playout(unknown, options);
  assert(first.traces.size() == 200);
  std::set<std::size_t> lengths;
  for (std::size_t i = 0; i < first.traces.size(); ++i) {
    const auto& trace = first.traces[i];
    const auto& same = replay.traces[i];
    lengths.insert(trace.events.size());
    assert(trace.case_id == std::to_string(i));
    assert(trace.events.size() == same.events.size());
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      assert(trace.events[j].case_id == trace.case_id);
      assert(trace.events[j].input_order == j);
      assert(trace.events[j].timestamp.empty());
      assert(trace.events[j].resource.empty());
      assert(trace.events[j].activity == same.events[j].activity);
    }
  }
  assert((lengths == std::set<std::size_t>{2, 3, 4}));

  options.n_traces = 0;
  assert(declare_playout(unknown, options).traces.empty());
  options.n_traces = 1;
  options.min_length = 4;
  options.max_length = 3;
  bool threw = false;
  try {
    (void)declare_playout(empty, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // Unknown unary constraints mention only their first key.
  DeclareModel unknown_unary;
  unknown_unary.add("vendor", {"X", "Y", false, 0, 0});
  options.min_length = options.max_length = 2;
  const auto unary_unknown_log = declare_playout(unknown_unary, options);
  for (const auto& event : unary_unknown_log.traces[0].events)
    assert(event.activity == "X");
}

}  // namespace

int main() {
  all_streaming_automata();
  snapshots_are_lossless_and_fallbacks_reset();
  model_arity_is_validated();
  concurrent_receive_and_snapshot();
  playout_contract();
  std::cout << "declare variants tests passed\n";
}
