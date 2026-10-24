#include "minera/decomposed_alignment.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace minera;

namespace {
PetriNet chain() {
  PetriNet n; const auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place();
  n.set_source(p0); n.set_sink(p2); n.set_activities({"a", "b"});
  n.add_transition(0,{p0},{p1}); n.add_transition(1,{p1},{p2}); return n;
}
PetriNet duplicate() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(), p3=n.add_place();
  n.set_source(p0); n.set_sink(p3); n.set_activities({"a"});
  n.add_transition(0,{p0},{p1}); n.add_transition(0,{p0},{p2});
  n.add_transition(std::nullopt,{p1,p2},{p3}); return n;
}
PetriNet boundary(bool with_loop = false) {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(), p3=n.add_place();
  n.set_source(p0); n.set_sink(p3); n.set_activities({"a"});
  n.add_transition(std::nullopt,{p0},{p1});
  n.add_transition(std::nullopt,{p2},{p3});
  n.add_transition(0,{p1},{p3});
  if (with_loop) n.add_transition(0,{p1},{p1});
  return n;
}
PetriNet weighted() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(); n.set_source(p0); n.set_sink(p1); n.set_activities({"a"});
  n.add_transition(0,{p0,p0},{p1,p1}); return n;
}
PetriNet same_label_chain() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(); n.set_source(p0); n.set_sink(p2); n.set_activities({"a"});
  n.add_transition(0,{p0},{p1}); n.add_transition(0,{p1},{p2}); return n;
}
PetriNet zero_input_split() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(); n.set_source(p0); n.set_sink(p2); n.set_activities({"a"});
  n.add_transition(std::nullopt,{}, {p1}); n.add_transition(0,{p1},{p2}); return n;
}
EventLog log_of(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog l; for (auto ts : traces) { Trace t; for (auto a : ts) t.events.push_back(Event{"",a}); l.traces.push_back(std::move(t)); } return l;
}
}

int main() {
  const auto n = chain();
  auto exact = align_decomposed(log_of({{"a","b"}}), n);
  assert(exact.complete && exact.traces.size() == 1 && exact.traces[0].cost == 0.0);
  assert(exact.traces[0].alignment.size() == 2);
  assert(exact.diagnostics.components > 1 && exact.diagnostics.merges == 0);
  assert(exact.traces[0].alignment[0].synchronous() && exact.traces[0].alignment[1].synchronous());
  auto dev = align_decomposed(log_of({{"x"}}), n);
  // As in the reference, events of activities outside the net are dropped and
  // components that see no event are not aligned, so nothing is charged.
  assert(dev.complete && dev.traces[0].cost == 0.0 && dev.traces[0].alignment.empty());
  assert(dev.traces[0].best_worst_cost == 30000.0 && dev.traces[0].fitness == 1.0);
  auto empty = align_decomposed(log_of({{}}), n);
  assert(empty.complete && empty.traces[0].cost == 0.0 && empty.traces[0].alignment.empty());
  for (const auto& m : empty.traces[0].alignment) assert(m.log && *m.log == ">>");
  auto timeout = align_decomposed(log_of({{"a"}}), n, [] { DecomposedAlignmentOptions o; o.max_states=1; return o; }());
  assert(!timeout.complete && timeout.exhausted);
  auto dup = align_decomposed(log_of({{"a","b"},{"a","b"}}), n);
  assert(dup.traces.size() == 2 && dup.diagnostics.components > 1 && dup.diagnostics.cache_hits > 0);
  assert(dup.total_cost == 0.0 && dup.fitness == 1.0);
  auto same_empty = align_decomposed(log_of({{}}), same_label_chain());
  auto same_exact = align_decomposed(log_of({{"a","a"}}), same_label_chain());
  assert(same_empty.complete && same_empty.traces[0].cost==0.0 && same_empty.traces[0].alignment.empty());
  assert(same_exact.complete && same_exact.traces[0].cost==0.0 && same_exact.traces[0].alignment.size()==2);

  // A boundary label projected twice in one region but once in its neighbour
  // is a real disagreement; threshold zero aborts before merging.
  auto conflict = align_decomposed(log_of({{"a","a"}}), boundary(true), [] { DecomposedAlignmentOptions o; o.threshold_border_agreement=0; return o; }());
  assert(!conflict.complete && conflict.traces.size()==1 && conflict.diagnostics.border_disagreements>0 && conflict.diagnostics.merges==0);
  auto repaired = align_decomposed(log_of({{"a","a"}}), boundary(true));
  assert(repaired.complete && repaired.diagnostics.merges>0 && repaired.diagnostics.border_disagreements>0 && repaired.traces[0].cost==1.0 && repaired.traces[0].alignment.size()>=2);

  // Tau prerequisites remain ahead of a shared boundary event in the DAG.
  auto tau = align_decomposed(log_of({{"a"}}), boundary(false));
  assert(tau.complete && tau.traces.size()==1);
  std::size_t sync = tau.traces[0].alignment.size();
  for (std::size_t i=0;i<tau.traces[0].alignment.size();++i) if (tau.traces[0].alignment[i].synchronous()) sync=i;
  assert(sync>0);
  bool tau_before=false; for (std::size_t i=0;i<sync;++i) if (tau.traces[0].alignment[i].log && *tau.traces[0].alignment[i].log==">>") tau_before=true;
  assert(tau_before);

  // Explicit non-unit markings and repeated weighted arcs are supported.
  PetriNet marked; auto m0=marked.add_place(), m1=marked.add_place(); marked.set_source(m0); marked.set_sink(m1); marked.set_activities({"a"}); marked.add_transition(0,{m0},{m0});
  DecomposedAlignmentOptions mo; mo.initial_marking={3,0}; mo.final_marking={3,0};
  assert(align_decomposed(log_of({{"a"}}), marked, mo).traces[0].cost==0.0);
  DecomposedAlignmentOptions wo; wo.initial_marking={2,0}; wo.final_marking={0,2};
  assert(align_decomposed(log_of({{"a"}}), weighted(), wo).traces[0].cost==0.0);

  // Invalid place IDs are rejected, and a zero-input silent split is safe.
  PetriNet bad; auto bp=bad.add_place(); bad.set_source(bp); bad.set_sink(bp); bad.set_activities({"a"}); bad.add_transition(0,{99},{bp});
  assert(!align_decomposed(log_of({{"a"}}), bad).complete);
  DecomposedAlignmentOptions so; so.initial_marking={0,0,0}; so.final_marking={0,0,1};
  auto split = align_decomposed(log_of({{"a"}}), zero_input_split(), so);
  assert(split.complete && split.traces.size()==1 && split.traces[0].cost==1.0);

  auto d1 = align_decomposed(log_of({{"a","b"}}), n);
  auto d2 = align_decomposed(log_of({{"a","b"}}), n);
  assert(d1.traces[0].alignment.size()==d2.traces[0].alignment.size());
  for (std::size_t i=0;i<d1.traces[0].alignment.size();++i) assert(d1.traces[0].alignment[i].synchronous()==d2.traces[0].alignment[i].synchronous());

  DecomposedAlignmentOptions limited; limited.max_states=1;
  auto limited_result = align_decomposed(log_of({{"a"},{"b"}}), n, limited);
  assert(limited_result.traces.size()==2 && !limited_result.complete && limited_result.exhausted);
  for (const auto& row : limited_result.traces) assert(!row.complete && row.exhausted);
  DecomposedAlignmentOptions timed; timed.max_time_total=1e-12;
  auto timed_result = align_decomposed(log_of({{"a"},{"b"}}), n, timed);
  assert(timed_result.traces.size()==2 && !timed_result.complete && timed_result.timed_out);

  auto one_unknown = align_decomposed(log_of({{"x"}}), n);
  auto two_unknown = align_decomposed(log_of({{"x"},{"x"}}), n);
  assert(two_unknown.traces.size()==2 && two_unknown.total_cost==2*one_unknown.total_cost && two_unknown.fitness==one_unknown.fitness);
  DecomposedAlignmentOptions custom; custom.model_move_cost=5000; custom.log_move_cost=2000;
  auto custom_unknown = align_decomposed(log_of({{"x"}}), n, custom);
  assert(custom_unknown.best_worst_cost==10000.0 && custom_unknown.traces[0].cost==0.0 && custom_unknown.traces[0].best_worst_cost==12000.0 && custom_unknown.traces[0].fitness==1.0);
  // Silent moves count in the cost but not in the best-worst cost or fitness.
  custom.tau_model_cost=7;
  auto custom_tau = align_decomposed(log_of({{"a"}}), zero_input_split(), [&] { auto x=custom; x.initial_marking={0,0,0}; x.final_marking={0,0,1}; return x; }());
  assert(custom_tau.complete && custom_tau.best_worst_cost==5000.0 && custom_tau.traces[0].cost==7.0 && custom_tau.traces[0].best_worst_cost==7000.0 && custom_tau.traces[0].fitness==1.0);

  // The decomposed search and recomposition use the configured generic
  // activity field for every projected trace.
  EventLog selected;
  Trace selected_trace;
  Event first{"c", "ignored"}; first.attributes.push_back({"phase", "a"});
  Event second{"c", "ignored"}; second.attributes.push_back({"phase", "b"});
  selected_trace.events = {std::move(first), std::move(second)};
  selected.traces.push_back(std::move(selected_trace));
  DecomposedAlignmentOptions selected_options;
  selected_options.activity_attribute = "phase";
  const auto selected_result = align_decomposed(selected, n, selected_options);
  assert(selected_result.complete && selected_result.traces.size() == 1 &&
         selected_result.traces.front().activities ==
             std::vector<std::string>({"a", "b"}) &&
         selected_result.traces.front().cost == 0.0);
  DecomposedAlignmentOptions missing_options = selected_options;
  missing_options.activity_attribute = "missing-phase";
  bool missing = false;
  try { (void)align_decomposed(selected, n, missing_options); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  EventLog null_selected = selected;
  null_selected.traces.front().events.front().attributes.front().is_null = true;
  missing = false;
  try { (void)align_decomposed(null_selected, n, selected_options); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  missing_options.activity_attribute = " ";
  missing = false;
  try { (void)align_decomposed(selected, n, missing_options); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  std::cout << "decomposed alignment tests passed\n";
}
