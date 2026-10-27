#include "minera/streaming_tbr.hpp"

#include <cassert>
#include <optional>
#include <string>
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

int main() {
  reference_contract();
  forward_replay_and_termination();
  missing_tokens_and_silent_routing_are_reported();
  backwards_duplicate_and_reset_behaviors();
  return 0;
}
