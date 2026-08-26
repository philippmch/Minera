#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "minera/petri_usage_comparison.hpp"

namespace {

minera::EventLog log_of(std::initializer_list<std::string> activities) {
  minera::Trace trace;
  trace.case_id = "case";
  for (const auto& activity : activities) {
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    trace.events.push_back(std::move(event));
  }
  return minera::EventLog{{std::move(trace)}};
}

minera::PetriNet simple_net() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();  // deliberately unused
  (void)p3;
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0}, {p1});
  net.add_transition(std::nullopt, {p1}, {p2});
  return net;
}

void usage_directions_and_silent_activation() {
  const auto net = simple_net();
  const auto one = log_of({"A"});
  const auto empty = log_of({});
  const auto equal = minera::compare_petri_usage(net, one, one);
  assert(equal.transitions.size() == 2);
  assert(equal.transitions[0].log1_occurrences == 1);
  assert(equal.transitions[0].log2_occurrences == 1);
  assert(equal.transitions[0].direction == 0.0);
  assert(equal.transitions[1].log1_occurrences == 1);  // silent routing
  assert(equal.places[3].total_occurrences == 0);
  assert(equal.places[3].direction == 0.0);  // zero-total is finite and neutral

  const auto negative = minera::compare_element_usage_two_logs(net, one, empty);
  assert(negative.transitions[0].direction == -1.0);
  assert(negative.transitions[1].direction == -1.0);
  const auto positive = minera::compare_petri_usage(net, empty, one);
  assert(positive.transitions[0].direction == 1.0);
  assert(std::isfinite(positive.places[0].direction));
}

void weighted_arc_and_duplicate_label_contract() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0, p0}, {p1});  // weighted input arc
  net.add_transition(0, {p1}, {p2});      // duplicate label, distinct index

  const auto result = minera::compare_petri_usage(net, log_of({"A"}), log_of({}));
  assert(result.transitions.size() == 2);
  assert(result.transitions[0].log1_occurrences == 1);
  assert(result.transitions[1].log1_occurrences == 0);
  assert(result.places[p0].log1_occurrences == 2);  // weighted consumption
  assert(result.arcs.size() == 5);
  assert(result.arcs[0].place == p0 && result.arcs[1].place == p0);
  assert(result.arcs[0].total_occurrences == result.transitions[0].total_occurrences);
}

void finish_routes_when_sink_has_a_token_and_other_work_remains() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto branch = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {sink, branch});
  net.add_transition(std::nullopt, {branch, sink}, {sink});

  const auto result = minera::compare_petri_usage(net, log_of({"A"}),
                                                   log_of({}));
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.places[branch].log1_occurrences == 1);
}

void explicit_markings_control_replay_boundaries() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto entry = net.add_place();
  const auto after = net.add_place();
  const auto alternate_final = net.add_place();
  net.set_source(source);
  net.set_sink(after);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, {entry});
  net.add_transition(0, {entry}, {after});
  net.add_transition(std::nullopt, {after}, {alternate_final});

  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {0, 1, 0, 0};
  options.final_marking = {0, 0, 0, 1};
  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}), options);
  assert(result.transitions[0].log1_occurrences == 0);
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.transitions[2].log1_occurrences == 1);
  assert(result.places[alternate_final].log1_occurrences == 1);
}

void extended_arcs_are_replayed_and_counted() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto buffered = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {buffered, buffered, gate});
  const auto reset = net.add_transition(std::nullopt, {gate}, {sink});
  net.add_reset_arc(reset, buffered);

  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}));
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.places[buffered].log1_occurrences == 2);
}

void inhibitor_arcs_gate_silent_completion() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto guard = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {sink});
  net.add_inhibitor_arc(0, guard);

  const auto enabled_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}));
  assert(enabled_result.transitions[0].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions blocked_options;
  blocked_options.initial_marking = {1, 1, 0};
  blocked_options.final_marking = {1, 1, 0};
  const auto blocked_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}), blocked_options);
  assert(blocked_result.transitions[0].log1_occurrences == 0);
}

void marking_dimensions_are_checked() {
  const auto net = simple_net();
  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {1};
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void invalid_net_indices_are_rejected() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  net.set_source(p0);
  net.set_sink(p0);
  net.set_activities({"A"});
  net.add_transition(4, {}, {});
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void silent_search_budget_is_reported() {
  constexpr std::size_t branch_count = 13;  // 2^13 reachable silent markings
  minera::PetriNet net;
  const auto source = net.add_place();
  std::vector<std::size_t> pending, yes, no;
  pending.reserve(branch_count);
  yes.reserve(branch_count);
  no.reserve(branch_count);
  for (std::size_t i = 0; i < branch_count; ++i) {
    pending.push_back(net.add_place());
    yes.push_back(net.add_place());
    no.push_back(net.add_place());
  }
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, pending);
  for (std::size_t i = 0; i < branch_count; ++i) {
    // Add the non-target choice first so the target combination is reached
    // only after the breadth-first frontier has exceeded the safety budget.
    net.add_transition(std::nullopt, {pending[i]}, {no[i]});
    net.add_transition(std::nullopt, {pending[i]}, {yes[i]});
  }
  net.add_transition(0, yes, {sink});

  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({"A"}), log_of({}));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  usage_directions_and_silent_activation();
  weighted_arc_and_duplicate_label_contract();
  finish_routes_when_sink_has_a_token_and_other_work_remains();
  explicit_markings_control_replay_boundaries();
  extended_arcs_are_replayed_and_counted();
  inhibitor_arcs_gate_silent_completion();
  marking_dimensions_are_checked();
  invalid_net_indices_are_rejected();
  silent_search_budget_is_reported();
}
