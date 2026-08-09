#include "minera/streaming_tbr.hpp"

#include <cassert>
#include <string>

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

}  // namespace

int main() {
  forward_replay_and_termination();
  missing_tokens_and_silent_routing_are_reported();
  backwards_duplicate_and_reset_behaviors();
  return 0;
}
