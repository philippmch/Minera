#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/model_decoration.hpp"

#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

Event event(std::string activity) {
  Event result;
  result.activity = std::move(activity);
  result.activity_state = FirstClassAttributeState::kPresent;
  return result;
}

Event selected_event(std::string activity, std::string label) {
  Event result = event(std::move(activity));
  result.attributes.push_back(
      {"label", std::move(label), AttributeKind::kString});
  return result;
}

Trace trace(std::initializer_list<std::string> activities) {
  Trace result;
  for (const auto& activity : activities) result.events.push_back(event(activity));
  return result;
}

EventLog log(std::initializer_list<Trace> traces) {
  EventLog result;
  result.traces.assign(traces.begin(), traces.end());
  return result;
}

const PetriTransition* transition_for(const PetriNet& net,
                                      const std::string& activity) {
  for (std::size_t id = 0; id < net.activities().size(); ++id) {
    if (net.activities()[id] != activity) continue;
    for (const auto& transition : net.transitions()) {
      if (transition.activity && *transition.activity == id) return &transition;
    }
  }
  return nullptr;
}

std::vector<const PetriTransition*> transitions_for(
    const PetriNet& net, const std::string& activity) {
  std::vector<const PetriTransition*> result;
  for (std::size_t id = 0; id < net.activities().size(); ++id) {
    if (net.activities()[id] != activity) continue;
    for (const auto& transition : net.transitions()) {
      if (transition.activity && *transition.activity == id)
        result.push_back(&transition);
    }
  }
  return result;
}

bool contains(const std::vector<std::size_t>& values, std::size_t value) {
  for (const auto candidate : values) {
    if (candidate == value) return true;
  }
  return false;
}

void alpha_plus_attaches_all_boundary_loop_positions() {
  for (const auto& [source, loop_name, expected_silent] : {
           std::tuple{log({trace({"first", "first", "a", "b"})}),
                      std::string("first"), std::size_t{1}},
           std::tuple{log({trace({"a", "b", "last", "last"})}),
                      std::string("last"), std::size_t{1}},
           std::tuple{log({trace({"only", "only"})}),
                      std::string("only"), std::size_t{2}},
       }) {
    const auto net = discover_petri_net(source, {.short_loops = true});
    const auto* loop = transition_for(net, loop_name);
    assert(loop != nullptr);
    assert(loop->inputs.size() == 1);
    assert(loop->inputs == loop->outputs);
    assert(!contains(loop->inputs, net.source()));
    assert(!contains(loop->inputs, net.sink()));
    std::size_t silent = 0;
    for (const auto& transition : net.transitions())
      silent += transition.silent() ? 1 : 0;
    assert(silent == expected_silent);
    assert(check_conformance(source, net).fitness > 0.999);
  }
}

void alpha_plus_keeps_interior_loop_on_the_surrounding_place() {
  const auto net = discover_petri_net(
      log({trace({"a", "loop", "loop", "b"}), trace({"a", "b"})}),
      {.short_loops = true});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* loop = transition_for(net, "loop");
  assert(a != nullptr && b != nullptr && loop != nullptr);
  assert(a->outputs.size() == 1);
  assert(a->outputs == b->inputs);
  assert(loop->inputs == a->outputs);
  assert(loop->outputs == a->outputs);
}

void alpha_plus_remove_unconnected_is_explicit() {
  const auto source_log = log({trace({"x", "a", "b", "a", "y"})});
  const auto retained = discover_petri_net(
      source_log, {.short_loops = true, .remove_unconnected = false});
  const auto removed = discover_petri_net(
      source_log, {.short_loops = true, .remove_unconnected = true});
  assert(transition_for(retained, "a") != nullptr);
  assert(transition_for(retained, "b") != nullptr);
  assert(transition_for(retained, "b")->inputs.empty());
  assert(transition_for(retained, "b")->outputs.empty());
  assert(!transition_for(retained, "a")->inputs.empty() ||
         !transition_for(retained, "a")->outputs.empty());
  assert(transition_for(removed, "a") != nullptr);
  assert(transition_for(removed, "b") == nullptr);
  assert(transition_for(removed, "x") != nullptr);
  assert(transition_for(removed, "y") != nullptr);

  // The option is an enhanced-variant control.  Classic Alpha and the
  // direct-graph overload retain their historical disconnected transitions.
  const auto classic = discover_petri_net(
      source_log, {.short_loops = false, .remove_unconnected = true});
  assert(transition_for(classic, "a") != nullptr);
  assert(transition_for(classic, "b") != nullptr);
}

void alpha_plus_aggregates_loop_contexts_before_matching() {
  const auto mixed = discover_petri_net(
      log({trace({"loop", "loop", "a", "b"}),
           trace({"a", "loop", "loop", "b"})}),
      {.short_loops = true});
  const auto mixed_loops = transitions_for(mixed, "loop");
  assert(mixed_loops.size() == 1);
  const auto* mixed_loop = mixed_loops.front();
  assert(mixed_loop->inputs.empty());
  assert(mixed_loop->inputs == mixed_loop->outputs);

  const auto both_boundaries = discover_petri_net(
      log({trace({"loop", "loop", "a"}),
           trace({"a", "loop", "loop"})}),
      {.short_loops = true});
  const auto boundary_loops = transitions_for(both_boundaries, "loop");
  assert(boundary_loops.size() == 1);
  const auto* boundary_loop = boundary_loops.front();
  assert(boundary_loop->inputs.empty());
  assert(boundary_loop->inputs == boundary_loop->outputs);
}

void alpha_uses_a_strict_custom_activity_field() {
  EventLog source;
  Trace custom;
  custom.events.push_back(selected_event("ignored-a", "A"));
  custom.events.push_back(selected_event("ignored-b", "B"));
  source.traces.push_back(custom);

  AlphaMinerOptions options;
  options.activity_attribute = "label";
  const auto projected = normalize_alpha_event_log(source, options);
  assert(projected.traces.size() == 1);
  assert(projected.traces.front().events[0].activity == "A");
  assert(projected.traces.front().events[1].activity == "B");
  assert(projected.traces.front().events[0].attributes ==
         source.traces.front().events[0].attributes);
  assert(projected.traces.front().events[0].input_order ==
         source.traces.front().events[0].input_order);

  const auto net = discover_petri_net(source, options);
  assert(net.activities() == std::vector<std::string>({"A", "B"}));
  assert(transition_for(net, "ignored-a") == nullptr);

  EventLog missing = source;
  missing.traces.front().events.front().attributes.clear();
  bool threw = false;
  try {
    (void)discover_petri_net(missing, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog null_value = source;
  null_value.traces.front().events.front().attributes.front().is_null = true;
  threw = false;
  try {
    (void)normalize_alpha_event_log(null_value, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void alpha_custom_first_class_projection_is_authoritative() {
  AlphaMinerOptions options;
  options.activity_attribute = "label";

  EventLog present;
  Event authoritative = event("current");
  authoritative.activity_source_key = "label";
  authoritative.activity_state = FirstClassAttributeState::kPresent;
  authoritative.activity_attribute =
      EventAttribute{"label", "stale", AttributeKind::kString};
  present.traces.push_back(Trace{"case", {authoritative}});
  const auto normalized = normalize_alpha_event_log(present, options);
  assert(normalized.traces.front().events.front().activity == "current");

  EventLog null_projection = present;
  null_projection.traces.front().events.front().activity_state =
      FirstClassAttributeState::kNull;
  bool threw = false;
  try {
    (void)normalize_alpha_event_log(null_projection, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog missing_projection = present;
  auto& missing_event = missing_projection.traces.front().events.front();
  missing_event.activity.clear();
  missing_event.activity_state = FirstClassAttributeState::kMissing;
  threw = false;
  try {
    (void)normalize_alpha_event_log(missing_projection, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void alpha_canonical_projection_precedes_retained_attribute() {
  AlphaMinerOptions options;
  EventLog present;
  Event authoritative = event("current");
  authoritative.activity_attribute =
      EventAttribute{"concept:name", "stale", AttributeKind::kString};
  present.traces.push_back(Trace{"case", {authoritative}});
  const auto normalized = normalize_alpha_event_log(present, options);
  assert(normalized.traces.front().events.front().activity == "current");

  EventLog null_projection = present;
  auto& null_event = null_projection.traces.front().events.front();
  null_event.activity_state = FirstClassAttributeState::kNull;
  bool threw = false;
  try {
    (void)normalize_alpha_event_log(null_projection, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog missing_projection = present;
  auto& missing_event = missing_projection.traces.front().events.front();
  missing_event.activity.clear();
  missing_event.activity_state = FirstClassAttributeState::kMissing;
  threw = false;
  try {
    (void)normalize_alpha_event_log(missing_projection, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void alpha_loop_does_not_attach_to_unrelated_empty_difference_places() {
  const auto net = discover_petri_net(
      log({trace({"loop", "loop", "a", "loop", "loop"}),
           trace({"x", "b"}), trace({"c", "y"})}),
      {.short_loops = true});
  const auto loop_transitions = transitions_for(net, "loop");
  assert(loop_transitions.size() == 1);
  const auto* loop = loop_transitions.front();
  assert(loop->inputs.empty());
  assert(loop->inputs == loop->outputs);
}

void alpha_plus_leaves_adjacent_distinct_loop_runs_unconnected() {
  const auto source = log({trace({"a", "a", "b", "b", "c"})});
  const auto retained = discover_petri_net(
      source, {.short_loops = true, .remove_unconnected = false});
  const auto* a = transition_for(retained, "a");
  const auto* b = transition_for(retained, "b");
  assert(a != nullptr && b != nullptr);
  assert(a->inputs.empty() && a->outputs.empty());
  assert(b->inputs.empty() && b->outputs.empty());

  const auto removed = discover_petri_net(
      source, {.short_loops = true, .remove_unconnected = true});
  assert(transition_for(removed, "a") == nullptr);
  assert(transition_for(removed, "b") == nullptr);
  assert(transition_for(removed, "c") != nullptr);
}

void alpha_custom_projection_drives_model_decoration() {
  EventLog source;
  Trace custom;
  custom.events.push_back(selected_event("ignored-a", "A"));
  custom.events.push_back(selected_event("ignored-b", "B"));
  source.traces.push_back(custom);

  AlphaMinerOptions options;
  options.activity_attribute = "label";
  const auto projected = normalize_alpha_event_log(source, options);
  const auto net = discover_petri_net(source, options);
  const auto decoration = decorate_model(projected, net);
  assert(decoration.trace_count == 1);
  assert(decoration.decorated_traces == 1);
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (net.transitions()[i].activity)
      assert(decoration.transition_counts[i] == 1);
  }
}

void alpha_default_projection_remains_compatible() {
  const auto source = log({trace({"a", "b", "c"})});
  const auto implicit = discover_petri_net(source);
  AlphaMinerOptions explicit_default;
  explicit_default.activity_attribute = "concept:name";
  const auto explicit_net = discover_petri_net(source, explicit_default);
  assert(implicit.activities() == explicit_net.activities());
  assert(implicit.place_count() == explicit_net.place_count());
  assert(implicit.transitions().size() == explicit_net.transitions().size());
  for (std::size_t i = 0; i < implicit.transitions().size(); ++i) {
    assert(implicit.transitions()[i].inputs == explicit_net.transitions()[i].inputs);
    assert(implicit.transitions()[i].outputs ==
           explicit_net.transitions()[i].outputs);
  }
}

}  // namespace

int main() {
  alpha_plus_attaches_all_boundary_loop_positions();
  alpha_plus_keeps_interior_loop_on_the_surrounding_place();
  alpha_plus_remove_unconnected_is_explicit();
  alpha_plus_aggregates_loop_contexts_before_matching();
  alpha_uses_a_strict_custom_activity_field();
  alpha_custom_first_class_projection_is_authoritative();
  alpha_canonical_projection_precedes_retained_attribute();
  alpha_loop_does_not_attach_to_unrelated_empty_difference_places();
  alpha_plus_leaves_adjacent_distinct_loop_runs_unconnected();
  alpha_custom_projection_drives_model_decoration();
  alpha_default_projection_remains_compatible();
  return 0;
}
