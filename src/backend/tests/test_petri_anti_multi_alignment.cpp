#include <cassert>
#include <cmath>
#include <stdexcept>

#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"

using namespace minera;

static EventLog log_of(std::initializer_list<std::initializer_list<const char*>> rows) {
  EventLog log;
  for (const auto& row : rows) {
    Trace trace;
    for (const auto* activity : row) trace.events.push_back(Event{"", activity});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

static PetriNet choice_net() {
  PetriNet net; net.set_activities({"a", "b"});
  const auto source = net.add_place(), sink = net.add_place(); net.set_source(source); net.set_sink(sink);
  net.add_transition(0, {source}, {sink}); net.add_transition(1, {source}, {sink});
  return net;
}

static PetriNet single_label_net(const char* label) {
  PetriNet net; net.set_activities({label});
  const auto source = net.add_place(), sink = net.add_place(); net.set_source(source); net.set_sink(sink);
  net.add_transition(0, {source}, {sink});
  return net;
}

int main() {
  const auto net = choice_net();
  PetriAntiAlignmentOptions ao; ao.exponent = 2.0; ao.epsilon = 0.0;
  const auto anti = anti_align_petri(net, log_of({{"a"}}), ao);
  assert(anti.activities == std::vector<std::string>{"b"});
  // Mismatch is insertion+deletion at coordinate (0,0), cost 2^-1 twice.
  assert(std::abs(anti.cost + 1.5) < 1e-12);
  assert(std::abs(anti.precision) < 1e-12);

  PetriMultiAlignmentOptions mo; mo.exponent = 2.0;
  const auto multi = multi_align_petri(net, log_of({{"a"}, {"b"}, {"a"}}), mo);
  assert(multi.activities == std::vector<std::string>{"a"});
  assert(std::abs(multi.cost - 1.5) < 1e-12);
  assert(multi.max_distance_to_log == 2.0);

  // Tau transitions are retained in the transition path but free in distance.
  PetriNet tau; tau.set_activities({"a"});
  const auto s = tau.add_place(), mid = tau.add_place(), f = tau.add_place(); tau.set_source(s); tau.set_sink(f);
  tau.add_transition(std::nullopt, {s}, {mid}); tau.add_transition(0, {mid}, {f});
  const auto ta = anti_align_petri(tau, log_of({{}}), ao);
  assert(ta.transitions.size() == 2 && ta.transitions[0] == 0 && ta.activities.size() == 1);

  // The length swap exercises the unmodeled/tau branch of discounted DP.
  PetriNet tau_only; tau_only.set_activities({});
  const auto ts = tau_only.add_place(), tf = tau_only.add_place(); tau_only.set_source(ts); tau_only.set_sink(tf);
  tau_only.add_transition(std::nullopt, {ts}, {tf});
  const auto swapped = anti_align_petri(tau_only, log_of({{"x", "y"}}), ao);
  assert(std::abs(swapped.cost + 0.75) < 1e-12);
  PetriAntiAlignmentOptions precision_options = ao; precision_options.epsilon = 0.5;
  const auto tau_precision = anti_align_petri(tau_only, log_of({{"x"}}), precision_options);
  assert(std::abs(tau_precision.precision - 2.0 / 3.0) < 1e-12);

  // Duplicate traces collapse to one variant; result is deterministic.
  const auto d1 = multi_align_petri(net, log_of({{"a"}, {"a"}}), mo);
  const auto d2 = multi_align_petri(net, log_of({{"a"}, {"a"}}), mo);
  assert(d1.activities == std::vector<std::string>{"a"});
  assert(d1.transitions == d2.transitions && d1.cost == d2.cost);

  // Arbitrary initial/final markings are accepted.
  PetriMultiAlignmentOptions custom = mo;
  custom.initial_marking = {1, 0}; custom.final_marking = {0, 1};
  const auto c = multi_align_petri(net, log_of({{"a"}}), custom);
  assert(c.activities == std::vector<std::string>{"a"});

  // A self-loop revisits one marking and terminates at a small per-marking limit.
  PetriNet loop; loop.set_activities({"a"});
  const auto ls = loop.add_place(), lf = loop.add_place(); loop.set_source(ls); loop.set_sink(lf);
  loop.add_transition(0, {ls}, {ls}); loop.add_transition(0, {ls}, {lf});
  PetriAntiAlignmentOptions limited = ao; limited.marking_limit = 1;
  const auto lr = anti_align_petri(loop, log_of({{"b"}}), limited);
  assert(lr.diagnostics.exhausted);

  bool threw = false;
  try { auto bad = mo; bad.exponent = 1.0; (void)multi_align_petri(net, log_of({{"a"}}), bad); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { auto bad = ao; bad.marking_limit = 0; bad.max_states = 0; (void)anti_align_petri(net, log_of({{"a"}}), bad); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  // Both discounted Petri families read the selected generic activity field.
  EventLog selected;
  Trace selected_trace;
  Event selected_event{"", "ignored"};
  selected_event.attributes.push_back({"phase", "a"});
  selected_trace.events.push_back(std::move(selected_event));
  selected.traces.push_back(std::move(selected_trace));
  PetriAntiAlignmentOptions selected_anti = ao;
  selected_anti.activity_attribute = "phase";
  const auto selected_anti_result = anti_align_petri(net, selected, selected_anti);
  assert(selected_anti_result.activities == std::vector<std::string>{"b"});
  PetriMultiAlignmentOptions selected_multi = mo;
  selected_multi.activity_attribute = "phase";
  const auto selected_multi_result = multi_align_petri(net, selected, selected_multi);
  assert(selected_multi_result.activities == std::vector<std::string>{"a"});
  PetriAntiAlignmentOptions missing_anti = selected_anti;
  missing_anti.activity_attribute = "missing-phase";
  threw = false;
  try { (void)anti_align_petri(net, selected, missing_anti); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  PetriMultiAlignmentOptions missing_multi = selected_multi;
  missing_multi.activity_attribute = "\t";
  threw = false;
  try { (void)multi_align_petri(net, selected, missing_multi); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog null_selected = selected;
  null_selected.traces.front().events.front().attributes.front().is_null = true;
  threw = false;
  try { (void)multi_align_petri(net, null_selected, selected_multi); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  // Pinned label conventions: prefix handling recognizes exact tau and
  // n*-labels, while modeled cells additionally recognize *skip*.  Ordinary
  // precision/max-distance deletion recognizes all tau/skip substrings.
  const char* visible_labels[] = {"tau", "n_hidden", "xskipx", "containstau"};
  const double expected_anti[] = {-0.75, -0.75, -1.75, -1.75};
  for (std::size_t i = 0; i < 4; ++i) {
    const auto labeled = single_label_net(visible_labels[i]);
    const auto label_log = log_of({{"z", "q"}});
    const auto labeled_anti = anti_align_petri(labeled, label_log, ao);
    assert(std::abs(labeled_anti.objective - expected_anti[i]) < 1e-12);
    assert(std::abs(labeled_anti.precision - 1.0 / 3.0) < 1e-12);
    const auto labeled_multi = multi_align_petri(labeled, label_log, mo);
    assert(std::abs(labeled_multi.max_distance_to_log - 2.0) < 1e-12);

    // A model-side label that follows a silent convention is still not a
    // free synchronous substitution.  Its cheapest ordinary edit is a free
    // model deletion followed by one log insertion.
    const auto matching_log = log_of({{visible_labels[i]}});
    const auto matching_anti = anti_align_petri(labeled, matching_log, ao);
    assert(std::abs(matching_anti.precision - 0.5) < 1e-12);
    const auto matching_multi = multi_align_petri(labeled, matching_log, mo);
    assert(std::abs(matching_multi.max_distance_to_log - 1.0) < 1e-12);
  }
  return 0;
}
