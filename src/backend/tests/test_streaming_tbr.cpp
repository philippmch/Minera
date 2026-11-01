#include "minera/streaming_tbr.hpp"

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

minera::PetriNet linear_net() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A", "B"});
  net.add_transition(0, {p0}, {p1});
  net.add_transition(1, {p1}, {p2});
  return net;
}

minera::PetriNet silent_prefix_net() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {p0}, {p1});
  net.add_transition(0, {p1}, {p2});
  return net;
}

void forward_replay_and_termination() {
  minera::StreamingTokenReplay replay(linear_net());
  assert(replay.receive("c1", "A").missing_added == 0);
  assert(replay.receive("c1", "B").missing_added == 0);
  const auto result = replay.terminate_case("c1");
  assert(result && result->is_fit && result->missing == 0 &&
         result->remaining == 0);
  assert(replay.snapshot().cases.empty());
}

void missing_tokens_and_silent_routing_are_reported() {
  minera::StreamingTokenReplay replay(linear_net());
  assert(replay.receive("bad", "B").missing_added == 1);
  assert(!replay.receive("bad", "X").accepted);
  const auto result = replay.terminate_case("bad");
  assert(result && !result->is_fit && result->missing >= 1);

  minera::StreamingTokenReplay routed(silent_prefix_net());
  assert(routed.receive("c", "A").missing_added == 0);
  assert(routed.terminate_case("c")->is_fit);
}

void backwards_duplicate_and_reset_behaviors() {
  minera::StreamingTbrOptions options;
  options.variant = minera::StreamingTbrVariant::kBackwards;
  minera::StreamingTokenReplay replay(linear_net(), options);
  assert(replay.receive("c", "A").accepted);
  assert(replay.receive("c", "B").accepted);
  assert(replay.snapshot().variant == "BACKWARDS");
  replay.reset_case("c");
  assert(replay.snapshot().cases.empty());
  replay.reset_all();
  assert(replay.snapshot().total_events_processed == 0);
}

// General net for the reference contract: transitions are (label or -1,
// inputs, outputs) over `places` places, labels index {"A", "B"}.
struct Spec { int label; std::vector<std::size_t> in, out; };
minera::StreamingTokenReplay reference_replay(std::size_t places, const std::vector<Spec>& spec,
                                              std::vector<int> initial, std::vector<int> final_marking) {
  minera::PetriNet net;
  for (std::size_t i = 0; i < places; ++i) (void)net.add_place();
  net.set_activities({"A", "B"});
  for (const auto& t : spec)
    net.add_transition(t.label < 0 ? std::nullopt : std::optional<std::size_t>(t.label), t.in, t.out);
  minera::StreamingTbrOptions options;
  options.initial_marking = std::move(initial);
  options.final_marking = std::move(final_marking);
  return minera::StreamingTokenReplay(net, options);
}
std::vector<int> marking_of(const minera::StreamingTokenReplay& replay) {
  return replay.snapshot().cases.at("c").marking;
}

// Reference values from PM4Py 2.7.22 streaming token replay (public calls).
void reference_contract() {
  constexpr int tau = -1, A = 0, B = 1;
  {
    // Two silent steps reach A's input; termination at the final marking.
    auto r = reference_replay(4, {{tau, {0}, {1}}, {tau, {1}, {2}}, {A, {2}, {3}}}, {1, 0, 0, 0}, {0, 0, 0, 1});
    assert(r.receive("c", "A").missing_added == 0 && marking_of(r) == (std::vector<int>{0, 0, 0, 1}));
    assert(r.terminate_case("c")->is_fit);
  }
  {
    // Termination does not fire silent transitions: p1 stays short of p2.
    auto r = reference_replay(3, {{A, {0}, {1}}, {tau, {1}, {2}}}, {1, 0, 0}, {0, 0, 1});
    assert(r.receive("c", "A").missing_added == 0);
    assert(!r.terminate_case("c")->is_fit);
  }
  {
    // Each needed place takes a path from a distinct marked place outside
    // the preset: sources p0 and p4 enable A ...
    auto r = reference_replay(5, {{tau, {0}, {1}}, {tau, {4}, {2}}, {A, {1, 2}, {3}}}, {1, 0, 0, 0, 1}, {0, 0, 0, 1, 0});
    assert(r.receive("c", "A").missing_added == 0 && marking_of(r) == (std::vector<int>{0, 0, 0, 1, 0}));
  }
  {
    // ... but two paths from one place (two tokens) do not: 2 missing tokens.
    auto r = reference_replay(4, {{tau, {0}, {1}}, {A, {1, 2}, {3}}, {tau, {0}, {2}}}, {2, 0, 0, 0}, {0, 0, 0, 1});
    assert(r.receive("c", "A").missing_added == 2 && marking_of(r) == (std::vector<int>{2, 0, 0, 1}));
    assert(!r.snapshot().cases.at("c").is_fit);
  }
  {
    // A source inside the preset is not used: A needs p0 and p1, p1 only via
    // p0. The reference fails at p0 (still marked): no change, no missing.
    auto r = reference_replay(3, {{tau, {0}, {1}}, {A, {0, 1}, {2}}}, {2, 0, 0}, {0, 0, 1});
    const auto result = r.receive("c", "A");
    assert(!result.accepted && result.missing_added == 0);
    assert(marking_of(r) == (std::vector<int>{2, 0, 0}) && r.snapshot().cases.at("c").is_fit);
  }
  {
    // Missing tokens counted before that failure stay: B checks p2 (absent,
    // 1 missing), then p0 (marked) and fails.
    auto r = reference_replay(4, {{B, {2, 0}, {3}}}, {1, 0, 0, 0}, {0, 0, 0, 1});
    assert(r.receive("c", "B").missing_added == 1 && marking_of(r) == (std::vector<int>{1, 0, 0, 0}));
    assert(!r.snapshot().cases.at("c").is_fit && r.snapshot().cases.at("c").missing == 1);
  }
  {
    // The shortest path wins; among equal ones the earliest marked place.
    auto shortest = reference_replay(6, {{tau, {4}, {5}}, {tau, {5}, {1}}, {tau, {0}, {1}}, {A, {1}, {3}}},
                                     {1, 0, 0, 0, 1, 0}, {0, 0, 0, 1, 0, 0});
    assert(shortest.receive("c", "A").missing_added == 0 && marking_of(shortest) == (std::vector<int>{0, 0, 0, 1, 1, 0}));
    auto earliest = reference_replay(5, {{A, {0}, {2}}, {tau, {2}, {1}}, {tau, {4}, {1}}, {B, {1}, {3}}},
                                     {1, 0, 0, 0, 1}, {0, 0, 0, 1, 0});
    (void)earliest.receive("c", "A");  // marked places now in order p4, p2
    assert(earliest.receive("c", "B").missing_added == 0 && marking_of(earliest) == (std::vector<int>{0, 0, 1, 1, 0}));
  }
  {
    // An unknown activity opens no case; empty ids are values.
    auto r = reference_replay(2, {{A, {0}, {1}}}, {1, 0}, {0, 1});
    assert(!r.receive("c", "X").accepted && r.snapshot().cases.empty());
    assert(r.receive("", "A").accepted && r.snapshot().cases.contains(""));
  }
}

}  // namespace


// MAXIMUM_ITERATIONS_INVISIBLES on the forward path (PM4Py 2.7.22): a silent
// chain of any length before A is one iteration plus the enabling check, so
// it needs a limit of 2; two parallel branches that each need their own path
// need 3. Reference missing counts per limit 0, 1, 2, 3, 10.
std::int64_t limited_missing(std::size_t silent_chain, bool parallel, std::size_t limit) {
  minera::PetriNet net;
  net.set_activities({"A", "B"});
  const auto p0 = net.add_place();
  std::vector<std::size_t> ends;
  if (!parallel) {
    auto previous = p0;
    for (std::size_t i = 0; i < silent_chain; ++i) {
      const auto next = net.add_place();
      net.add_transition(std::nullopt, {previous}, {next});
      previous = next;
    }
    ends.push_back(previous);
  } else {
    const auto a = net.add_place(), b = net.add_place();
    net.add_transition(std::nullopt, {p0}, {a, b});
    for (auto branch : {a, b}) {
      for (std::size_t i = 0; i < silent_chain; ++i) {
        const auto next = net.add_place();
        net.add_transition(std::nullopt, {branch}, {next});
        branch = next;
      }
      ends.push_back(branch);
    }
  }
  const auto q = net.add_place(), end = net.add_place();
  net.add_transition(0, ends, {q});
  net.add_transition(1, {q}, {end});
  minera::StreamingTbrOptions options;
  options.maximum_iterations_invisibles = limit;
  options.initial_marking.assign(net.place_count(), 0);
  options.initial_marking[p0] = 1;
  options.final_marking.assign(net.place_count(), 0);
  options.final_marking[end] = 1;
  minera::StreamingTokenReplay replay(net, options);
  (void)replay.receive("c", "A");
  (void)replay.receive("c", "B");
  return replay.snapshot().cases.at("c").missing;
}
void forward_invisible_iteration_limit() {
  const std::size_t limits[] = {0, 1, 2, 3, 10};
  const struct { std::size_t chain; bool parallel; std::int64_t expected[5]; } cases[] = {
      {3, false, {1, 1, 0, 0, 0}}, {12, false, {1, 1, 0, 0, 0}}, {0, false, {0, 0, 0, 0, 0}},
      {0, true, {2, 2, 0, 0, 0}}, {1, true, {2, 2, 2, 0, 0}}, {3, true, {2, 2, 2, 0, 0}}};
  for (const auto& c : cases)
    for (std::size_t i = 0; i < 5; ++i) assert(limited_missing(c.chain, c.parallel, limits[i]) == c.expected[i]);
}


// Weighted, zero-weight and parallel arcs. Each case runs on a net with
// explicit occurrence weights and, when no arc is parallel, zero or large,
// again with the repeated-unit endpoints the PNML importer stores small
// weights as. Expected markings and per-case missing counts after every
// event, and whether termination reaches the final marking, are from PM4Py
// 2.7.22 streaming TBR (captured with PetriNet.Arc weights; a parallel arc
// raises a TypeError inside receive, which the reference logs and swallows).
struct WeightedTransition {
  int label;
  std::vector<std::pair<std::size_t, int>> in, out;
};
struct WeightedStep {
  const char* case_id;
  const char* activity;
  std::vector<int> marking;
  std::int64_t missing;
};
void check_weighted(const char* name, std::size_t places, const std::vector<WeightedTransition>& spec,
                    std::vector<int> initial, std::vector<int> final_marking, std::size_t limit,
                    const std::vector<WeightedStep>& steps,
                    const std::vector<std::tuple<const char*, bool, long long, long long>>& fits) {
  bool repeatable = true;
  for (const auto& t : spec)
    for (const auto* side : {&t.in, &t.out})
      for (std::size_t i = 0; i < side->size(); ++i) {
        repeatable = repeatable && (*side)[i].second >= 1 && (*side)[i].second <= 50;
        for (std::size_t j = 0; j < i; ++j) repeatable = repeatable && (*side)[j].first != (*side)[i].first;
      }
  for (const bool compact : {true, false}) {
    if (!compact && !repeatable) continue;
    minera::PetriNet net;
    for (std::size_t i = 0; i < places; ++i) (void)net.add_place();
    net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
    for (const auto& t : spec) {
      std::vector<std::size_t> in, out;
      std::vector<minera::PetriArcWeight> in_weights, out_weights;
      const auto expand = [&](const auto& arcs, auto& endpoints, auto& weights) {
        for (const auto& [place, weight] : arcs) {
          if (compact) endpoints.push_back(place), weights.push_back({weight});
          else endpoints.insert(endpoints.end(), static_cast<std::size_t>(weight), place);
        }
      };
      expand(t.in, in, in_weights);
      expand(t.out, out, out_weights);
      const auto id = net.add_transition(
          t.label < 0 ? std::nullopt : std::optional<std::size_t>(t.label), in, out);
      if (compact) net.set_ordinary_arc_weights(id, in_weights, out_weights);
    }
    minera::StreamingTbrOptions options;
    options.initial_marking = initial;
    options.final_marking = final_marking;
    options.maximum_iterations_invisibles = limit;
    minera::StreamingTokenReplay replay(net, options);
    for (const auto& step : steps) {
      (void)replay.receive(step.case_id, step.activity);
      const auto snapshot = replay.snapshot();
      const auto& state = snapshot.cases.at(step.case_id);
      if (state.marking != step.marking || state.missing != step.missing) {
        std::fprintf(stderr, "%s (%s): %s %s missing %lld marking", name,
                     compact ? "weights" : "repeated", step.case_id, step.activity,
                     static_cast<long long>(state.missing));
        for (const auto tokens : state.marking) std::fprintf(stderr, " %d", tokens);
        std::fprintf(stderr, "\n");
        assert(false);
      }
    }
    // Fitness, missing and remaining at termination are the reference's
    // (signed sums: a surplus sink token lowers missing).
    for (const auto& [case_id, fit, missing, remaining] : fits) {
      const auto result = replay.terminate_case(case_id);
      if (!result || result->is_fit != fit || result->missing != missing || result->remaining != remaining) {
        std::fprintf(stderr, "%s (%s): terminate %s\n", name, compact ? "weights" : "repeated", case_id);
        assert(false);
      }
    }
  }
}
void weighted_and_parallel_arcs_follow_reference() {
  check_weighted("in0_outA1", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,0}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},0},{"c2","A",{0,1,0},0},{"c2","B",{0,1,1},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,2,1},1},{"c4","A",{0,1,0},0},{"c4","B",{0,1,1},0},{"c4","B",{0,1,2},0}}, {{"c1",false,0,1},{"c2",false,0,1},{"c3",false,1,2},{"c4",false,-1,2}});
  check_weighted("in2_outA2", 3, {{0,{{0,1}},{{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},2},{"c2","A",{0,2,0},0},{"c2","B",{0,0,1},0},{"c3","A",{0,2,0},0},{"c3","A",{0,4,0},1},{"c3","B",{0,2,1},1},{"c4","A",{0,2,0},0},{"c4","B",{0,0,1},0},{"c4","B",{0,0,2},2}}, {{"c1",false,2,1},{"c2",true,0,0},{"c3",false,1,2},{"c4",false,1,1}});
  check_weighted("in3_outA2", 3, {{0,{{0,1}},{{1,2}}},{1,{{1,3}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},3},{"c2","A",{0,2,0},0},{"c2","B",{0,2,0},0},{"c3","A",{0,2,0},0},{"c3","A",{0,4,0},1},{"c3","B",{0,1,1},1},{"c4","A",{0,2,0},0},{"c4","B",{0,2,0},0},{"c4","B",{0,2,0},0}}, {{"c1",false,3,1},{"c2",false,1,1},{"c3",false,1,1},{"c4",false,1,1}});
  check_weighted("in1000_outA1", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,1000}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},1000},{"c2","A",{0,1,0},0},{"c2","B",{0,1,0},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,2,0},1},{"c4","A",{0,1,0},0},{"c4","B",{0,1,0},0},{"c4","B",{0,1,0},0}}, {{"c1",false,1000,1},{"c2",false,1,0},{"c3",false,2,1},{"c4",false,1,0}});
  check_weighted("in1_outA0", 3, {{0,{{0,1}},{{1,0}}},{1,{{1,1}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},1},{"c2","A",{0,0,0},0},{"c2","B",{0,0,0},0},{"c3","A",{0,0,0},0},{"c3","A",{0,0,0},1},{"c3","B",{0,0,0},1},{"c4","A",{0,0,0},0},{"c4","B",{0,0,0},0},{"c4","B",{0,0,0},0}}, {{"c1",false,1,1},{"c2",false,1,-1},{"c3",false,2,-1},{"c4",false,1,-1}});
  check_weighted("in2_outA1_bout0", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,2}},{{2,0}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,0},2},{"c2","A",{0,1,0},0},{"c2","B",{0,1,0},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,0,0},1},{"c4","A",{0,1,0},0},{"c4","B",{0,1,0},0},{"c4","B",{0,1,0},0}}, {{"c1",false,3,0},{"c2",false,1,0},{"c3",false,2,-1},{"c4",false,1,0}});
  check_weighted("par_in_1_1", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,1},{1,1}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,0},1},{"c2","A",{0,1,0},0},{"c2","B",{0,0,1},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,0,1},1},{"c4","A",{0,1,0},0},{"c4","B",{0,0,1},0},{"c4","B",{0,0,1},1}}, {{"c1",false,2,0},{"c2",true,0,0},{"c3",false,1,0},{"c4",false,1,0}});
  check_weighted("par_in_1_2", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,1},{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,0},1},{"c2","A",{0,1,0},0},{"c2","B",{0,1,0},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,0,1},1},{"c4","A",{0,1,0},0},{"c4","B",{0,1,0},0},{"c4","B",{0,1,0},0}}, {{"c1",false,2,0},{"c2",false,1,0},{"c3",false,1,0},{"c4",false,1,0}});
  check_weighted("par_in_0_1", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,0},{1,1}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,0},0},{"c2","A",{0,1,0},0},{"c2","B",{0,0,1},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,1,1},1},{"c4","A",{0,1,0},0},{"c4","B",{0,0,1},0},{"c4","B",{0,0,1},0}}, {{"c1",false,1,0},{"c2",true,0,0},{"c3",false,1,1},{"c4",true,0,0}});
  check_weighted("par_in_0_0", 3, {{0,{{0,1}},{{1,1}}},{1,{{1,0},{1,0}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},0},{"c2","A",{0,1,0},0},{"c2","B",{0,1,1},0},{"c3","A",{0,1,0},0},{"c3","A",{0,2,0},1},{"c3","B",{0,2,1},1},{"c4","A",{0,1,0},0},{"c4","B",{0,1,1},0},{"c4","B",{0,1,2},0}}, {{"c1",false,0,1},{"c2",false,0,1},{"c3",false,1,2},{"c4",false,-1,2}});
  check_weighted("par_out_1_2", 3, {{0,{{0,1}},{{1,1},{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","B",{1,0,1},2},{"c2","A",{0,3,0},0},{"c2","B",{0,1,1},0},{"c3","A",{0,3,0},0},{"c3","A",{0,6,0},1},{"c3","B",{0,4,1},1},{"c4","A",{0,3,0},0},{"c4","B",{0,1,1},0},{"c4","B",{0,1,1},0}}, {{"c1",false,2,1},{"c2",false,0,1},{"c3",false,1,4},{"c4",false,0,1}});
  check_weighted("join_3_2", 4, {{0,{{0,1}},{{1,1},{2,1}}},{2,{{1,1}},{{1,1}}},{1,{{1,3},{2,2}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","B",{1,0,0,1},5},{"c2","A",{0,1,1,0},0},{"c2","B",{0,1,1,0},0},{"c3","A",{0,1,1,0},0},{"c3","C",{0,1,1,0},0},{"c3","B",{0,1,1,0},0},{"c5","B",{1,0,0,1},5},{"c5","B",{1,0,0,2},10}}, {{"c1",false,5,1},{"c2",false,1,1},{"c3",false,1,1},{"c5",false,9,2}});
  check_weighted("join_2_0", 4, {{0,{{0,1}},{{1,1},{2,1}}},{2,{{1,1}},{{1,1}}},{1,{{1,2},{2,0}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","B",{1,0,0,1},2},{"c2","A",{0,1,1,0},0},{"c2","B",{0,1,1,0},0},{"c3","A",{0,1,1,0},0},{"c3","C",{0,1,1,0},0},{"c3","B",{0,1,1,0},0},{"c5","B",{1,0,0,1},2},{"c5","B",{1,0,0,2},4}}, {{"c1",false,2,1},{"c2",false,1,1},{"c3",false,1,1},{"c5",false,3,2}});
  check_weighted("tau_1_2_2", 4, {{0,{{0,1}},{{1,1}}},{-1,{{1,1}},{{2,2}}},{1,{{2,2}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","A",{0,1,0,0},0},{"c1","B",{0,0,0,1},0},{"c2","B",{1,0,0,1},2},{"c3","A",{0,1,0,0},0},{"c3","A",{0,2,0,0},1},{"c3","B",{0,1,0,1},1},{"c4","A",{0,1,0,0},0},{"c4","B",{0,0,0,1},0},{"c4","B",{0,0,0,2},2}}, {{"c1",true,0,0},{"c2",false,2,1},{"c3",false,1,1},{"c4",false,1,1}});
  check_weighted("tau_1_1_2", 4, {{0,{{0,1}},{{1,1}}},{-1,{{1,1}},{{2,1}}},{1,{{2,2}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","A",{0,1,0,0},0},{"c1","B",{0,1,0,1},2},{"c2","B",{1,0,0,1},2},{"c3","A",{0,1,0,0},0},{"c3","A",{0,2,0,0},1},{"c3","B",{0,0,0,1},1},{"c4","A",{0,1,0,0},0},{"c4","B",{0,1,0,1},2},{"c4","B",{0,1,0,2},4}}, {{"c1",false,2,1},{"c2",false,2,1},{"c3",false,1,0},{"c4",false,3,2}});
  check_weighted("tau_0_1_1", 4, {{0,{{0,1}},{{1,1}}},{-1,{{1,0}},{{2,1}}},{1,{{2,1}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","A",{0,1,0,0},0},{"c1","B",{0,1,0,1},0},{"c2","B",{1,0,0,1},1},{"c3","A",{0,1,0,0},0},{"c3","A",{0,2,0,0},1},{"c3","B",{0,2,0,1},1},{"c4","A",{0,1,0,0},0},{"c4","B",{0,1,0,1},0},{"c4","B",{0,1,0,2},0}}, {{"c1",false,0,1},{"c2",false,1,1},{"c3",false,1,2},{"c4",false,-1,2}});
  check_weighted("tau_1_0_1", 4, {{0,{{0,1}},{{1,1}}},{-1,{{1,1}},{{2,0}}},{1,{{2,1}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c1","A",{0,1,0,0},0},{"c1","B",{0,1,0,1},1},{"c2","B",{1,0,0,1},1},{"c3","A",{0,1,0,0},0},{"c3","A",{0,2,0,0},1},{"c3","B",{0,2,0,1},2},{"c4","A",{0,1,0,0},0},{"c4","B",{0,1,0,1},1},{"c4","B",{0,1,0,2},2}}, {{"c1",false,1,1},{"c2",false,1,1},{"c3",false,2,2},{"c4",false,1,2}});
  check_weighted("dup_label", 3, {{0,{{0,2}},{{1,1}}},{0,{{0,1}},{{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c1","A",{0,2,0},0},{"c1","B",{0,0,1},0},{"c2","B",{1,0,1},2},{"c3","A",{0,2,0},0},{"c3","A",{0,3,0},2},{"c3","B",{0,1,1},2}}, {{"c1",true,0,0},{"c2",false,2,1},{"c3",false,2,1}});
  check_weighted("im3_w2", 2, {{0,{{0,2}},{{1,1}}}}, {3,0}, {0,1}, 10, {{"c1","A",{1,1},0},{"c2","A",{1,1},0},{"c2","A",{1,1},0},{"c3","A",{1,1},0},{"c3","A",{1,1},0},{"c3","A",{1,1},0}}, {{"c1",false,0,1},{"c2",false,0,1},{"c3",false,0,1}});
  check_weighted("parcons_1_1_n3", 2, {{1,{{0,1},{0,1}},{{1,1}}}}, {3,0}, {0,1}, 10, {{"c","B",{1,1},0},{"c","B",{0,2},0}}, {{"c",false,-1,1}});
  check_weighted("parcons_1_2_n2", 2, {{1,{{0,1},{0,2}},{{1,1}}}}, {2,0}, {0,1}, 10, {{"c","B",{0,1},0},{"c","B",{0,1},1}}, {{"c",false,1,0}});
  check_weighted("parcons_3_1_n4", 2, {{1,{{0,3},{0,1}},{{1,1}}}}, {4,0}, {0,1}, 10, {{"c","B",{0,1},0},{"c","B",{0,1},3}}, {{"c",false,3,0}});
  check_weighted("zero_key_final", 3, {{0,{{0,1}},{{1,1},{2,0}}}}, {1,0,0}, {0,1,0}, 10, {{"c","A",{0,1,0},0}}, {{"c",true,0,0}});
  check_weighted("zero_key_consume0", 4, {{0,{{0,1}},{{1,1},{2,0}}},{1,{{2,0},{1,1}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c","A",{0,1,0,0},0},{"c","B",{0,0,0,1},0},{"d","A",{0,1,0,0},0},{"d","B",{0,0,0,1},0},{"d","B",{0,0,0,2},1}}, {{"c",true,0,0},{"d",false,0,1}});
  check_weighted("zero_key_insert", 4, {{0,{{0,1}},{{2,0}}},{1,{{1,1},{2,1}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c","A",{0,0,0,0},0},{"c","B",{0,0,0,0},1},{"d","B",{1,0,0,1},2}}, {{"c",false,2,-1},{"d",false,2,1}});
  check_weighted("zero_key_silent_src", 4, {{0,{{0,1}},{{1,0}}},{-1,{{1,0}},{{2,1}}},{1,{{2,1}},{{3,1}}}}, {1,0,0,0}, {0,0,0,1}, 10, {{"c","A",{0,0,0,0},0},{"c","B",{0,0,0,1},0},{"d","B",{1,0,0,1},1}}, {{"c",true,0,0},{"d",false,1,1}});
  check_weighted("preset_w0_src", 3, {{-1,{{0,1}},{{1,1}}},{1,{{0,0},{1,1}},{{2,1}}}}, {1,0,0}, {0,0,1}, 10, {{"c","B",{1,0,0},0}}, {{"c",false,1,0}});
  check_weighted("multi_k2_n1_t12", 3, {{-1,{{0,1}},{{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 2, {{"c","B",{0,0,1},0}}, {});
  check_weighted("multi_k2_n1_t12", 3, {{-1,{{0,1}},{{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 3, {{"c","B",{0,0,1},0}}, {});
  check_weighted("multi_k2_n1_t12", 3, {{-1,{{0,1}},{{1,2}}},{1,{{1,2}},{{2,1}}}}, {1,0,0}, {0,0,1}, 4, {{"c","B",{0,0,1},0}}, {});
  check_weighted("multi_k2_n2_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {2,0,0}, {0,0,1}, 2, {{"c","B",{2,0,1},2}}, {});
  check_weighted("multi_k2_n2_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {2,0,0}, {0,0,1}, 3, {{"c","B",{0,0,1},0}}, {});
  check_weighted("multi_k2_n2_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {2,0,0}, {0,0,1}, 4, {{"c","B",{0,0,1},0}}, {});
  check_weighted("multi_k2_n3_t21", 3, {{-1,{{0,2}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {3,0,0}, {0,0,1}, 2, {{"c","B",{3,0,1},2}}, {});
  check_weighted("multi_k2_n3_t21", 3, {{-1,{{0,2}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {3,0,0}, {0,0,1}, 3, {{"c","B",{3,0,1},2}}, {});
  check_weighted("multi_k2_n3_t21", 3, {{-1,{{0,2}},{{1,1}}},{1,{{1,2}},{{2,1}}}}, {3,0,0}, {0,0,1}, 4, {{"c","B",{3,0,1},2}}, {});
  check_weighted("multi_k3_n4_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,3}},{{2,1}}}}, {4,0,0}, {0,0,1}, 2, {{"c","B",{4,0,1},3}}, {});
  check_weighted("multi_k3_n4_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,3}},{{2,1}}}}, {4,0,0}, {0,0,1}, 3, {{"c","B",{4,0,1},3}}, {});
  check_weighted("multi_k3_n4_t11", 3, {{-1,{{0,1}},{{1,1}}},{1,{{1,3}},{{2,1}}}}, {4,0,0}, {0,0,1}, 4, {{"c","B",{1,0,1},0}}, {});
  // A silent self-loop (p2,p1 -> p2,p1) leaves the marking unchanged, so the
  // reference's invisible loop stops at a marking rather than None and C
  // inserts its missing tokens from the original marking (random net n22).
  check_weighted("silent_self_loop_stall", 3,
                 {{0,{{2,1}},{{2,1}}},{1,{{2,1},{1,1}},{{2,1},{1,1}}},{2,{{1,2},{0,2}},{{0,2},{1,2}}},{-1,{{2,1},{1,1}},{{2,1},{1,1}}}},
                 {1,1,0}, {0,1,1}, 10,
                 {{"c2","B",{1,1,0},1},{"c2","A",{1,1,1},2},{"c0","C",{1,1,0},0},{"c2","A",{1,1,1},2},{"c1","B",{1,1,0},1},{"c0","B",{1,1,0},1},{"c2","C",{2,2,1},4},{"c1","A",{1,1,1},2},
                  {"c0","B",{1,1,0},2},{"c1","B",{1,1,1},2},{"c2","C",{2,2,1},4},{"c1","C",{2,2,1},4},{"c4","C",{1,1,0},0},{"c1","C",{2,2,1},4},{"c0","C",{1,1,0},2},{"c4","C",{1,1,0},0}},
                 {{"c0",false,3,0},{"c1",false,3,3},{"c2",false,3,3},{"c4",false,1,0}});
  // The reference searches a shortest silent path from any listed place to
  // the transition itself, again in every round, and an exhausted round limit
  // stops at a marking (random nets n274 and n69).
  check_weighted("silent_path_to_transition_n274", 3, {{0,{{1,1}},{{2,1}}},{1,{{0,2},{2,1}},{{0,2},{1,1}}},{-1,{{1,1}},{{0,1},{1,2}}},{3,{{2,1},{0,1}},{{2,1},{0,2}}},{4,{{1,2},{0,2}},{{2,1}}},{5,{{1,1},{2,1}},{{0,1},{1,1}}}}, {1,1,0}, {0,0,1}, 10, {{"c1","B",{2,2,0},1},{"c0","B",{2,2,0},1},{"c2","B",{2,2,0},1},{"c1","A",{2,1,1},1},{"c3","A",{1,0,1},0},{"c3","A",{1,0,2},1},{"c2","A",{2,1,1},1},{"c0","A",{2,1,1},1},{"c3","B",{1,0,2},1},{"c2","E",{2,1,1},1},{"c2","E",{2,1,1},1},{"c1","A",{2,0,2},1},{"c0","E",{2,1,1},1},{"c3","D",{2,0,2},1},{"c2","F",{3,1,0},1},{"c0","B",{2,2,0},1},{"c1","D",{3,0,2},1},{"c0","F",{2,2,0},1},{"c2","A",{3,0,1},1},{"c3","D",{3,0,2},1}}, {{"c0",false,2,3},{"c1",false,0,4},{"c2",false,1,3},{"c3",false,0,4}});
  check_weighted("silent_loop_iteration_limit_n69", 3, {{0,{{2,1},{1,1}},{{1,1}}},{-1,{{0,1}},{{1,1},{0,1}}},{-1,{{2,1}},{{1,1}}},{3,{{1,1}},{{1,0},{0,1}}}}, {1,1,0}, {0,1,1}, 10, {{"c0","A",{1,1,0},1},{"c1","A",{1,1,0},1},{"c0","D",{2,0,0},1},{"c0","D",{3,0,0},1},{"c0","A",{3,1,0},2}}, {{"c0",false,3,2},{"c1",false,2,0}});
}

void negative_arc_weights_are_rejected() {
  minera::PetriNet net = linear_net();
  net.set_ordinary_arc_weights(1, {{1, true}}, {});
  bool threw = false;
  try { minera::StreamingTokenReplay replay(net); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

// PM4Py 2.7.22 streaming TBR on p0 -A-> p1 -B-> p2 (im p0, fm p2): case 1
// [A] terminates with missing 1, remaining 0 (the absent sink token offsets
// the stranded p1 token); case 2 [B, B] with missing 1, remaining 2 (the
// surplus sink token lowers missing from 2).
void termination_counts_are_reference_signed_sums() {
  minera::PetriNet net;
  const auto p0 = net.add_place(), p1 = net.add_place(), p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A", "B"});
  net.add_transition(0, {p0}, {p1});
  net.add_transition(1, {p1}, {p2});
  minera::StreamingTokenReplay replay(net);
  (void)replay.receive("1", "A");
  (void)replay.receive("2", "B");
  (void)replay.receive("2", "B");
  const auto one = replay.terminate_case("1");
  assert(one && !one->is_fit && one->missing == 1 && one->remaining == 0);
  const auto two = replay.terminate_case("2");
  assert(two && !two->is_fit && two->missing == 1 && two->remaining == 2);
}

// A stalled silent loop (iteration limit 1) inserts missing tokens from the
// original marking: x held 3 tokens, the silent path drained it, so the
// reference adds 1 - 3 for x and 1 for y, a per-case count of -1. With the
// default limit the next search returns None and the reference's TypeError
// leaves the marking and count unchanged. PM4Py 2.7.22 values for the net
// p0 -B-> x(3), q; x, y -A-> out; q, x(3) -silent-> y (events B, A).
void stalled_loop_counts_negative_missing() {
  for (const std::size_t limit : {std::size_t{1}, std::size_t{10}}) {
    minera::PetriNet net;
    const auto p0 = net.add_place(), x = net.add_place(), q = net.add_place(),
               y = net.add_place(), out = net.add_place();
    net.set_source(p0);
    net.set_sink(out);
    net.set_activities({"A", "B"});
    const auto b = net.add_transition(1, {p0}, {x, q});
    net.set_ordinary_arc_weights(b, {{1}}, {{3}, {1}});
    (void)net.add_transition(0, {x, y}, {out});
    const auto s = net.add_transition(std::nullopt, {q, x}, {y});
    net.set_ordinary_arc_weights(s, {{1}, {3}}, {{1}});
    minera::StreamingTbrOptions options;
    options.maximum_iterations_invisibles = limit;
    minera::StreamingTokenReplay replay(net, options);
    assert(replay.receive("c", "B").missing_added == 0);
    const auto result = replay.receive("c", "A");
    const auto snapshot = replay.snapshot();
    const auto& state = snapshot.cases.at("c");
    if (limit == 1) {
      assert(result.missing_added == -1 && state.missing == -1 && snapshot.total_missing == -1);
      assert(state.marking == (std::vector<int>{0, 0, 1, 0, 1}) && !state.is_fit);
      const auto json = minera::serialize_streaming_tbr_json(snapshot);
      assert(json.find("\"missing\":-1,") != std::string::npos);
      const auto end = replay.terminate_case("c");
      assert(end && end->missing == -1 && end->remaining == 1 && !end->is_fit);
    } else {
      assert(result.missing_added == 0 && state.missing == 0);
      assert(state.marking == (std::vector<int>{0, 3, 1, 0, 0}));
      const auto end = replay.terminate_case("c");
      assert(end && end->missing == 1 && end->remaining == 3 && !end->is_fit);
    }
  }
}

int main() {
  stalled_loop_counts_negative_missing();
  termination_counts_are_reference_signed_sums();
  weighted_and_parallel_arcs_follow_reference();
  negative_arc_weights_are_rejected();
  forward_invisible_iteration_limit();
  reference_contract();
  forward_replay_and_termination();
  missing_tokens_and_silent_routing_are_reported();
  backwards_duplicate_and_reset_behaviors();
  return 0;
}
