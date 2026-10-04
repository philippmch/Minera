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

void alpha_plus_boundary_loops_use_unmarked_separate_places() {
  for (const auto& [source, loop_name, expected_silent] : {
           std::tuple{log({trace({"first", "first", "a", "b"})}),
                      std::string("first"), std::size_t{0}},
           std::tuple{log({trace({"a", "b", "last", "last"})}),
                      std::string("last"), std::size_t{0}},
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
    // The reconstructed loop place has no initial token and is independent
    // of the ordinary flow, so these traces cannot replay perfectly.
    assert(check_conformance(source, net).fitness < 0.999);
  }
}

void alpha_plus_keeps_removed_initial_places_in_convenience_results() {
  for (const auto& source : {log({trace({"only", "only"})}),
                             log({trace({"a", "a"}), trace({"b", "b"})})}) {
    for (const bool cleanup : {false, true}) {
      AlphaMinerOptions options;
      options.short_loops = true;
      options.remove_unconnected = cleanup;
      for (const bool parent : {false, true}) {
        const auto net = parent ? discover_petri_net(source, options)
                                : discover_alpha_plus(source, options);
        assert(!net.has_source() && net.has_sink());
        assert(net.discovery_markings());
        const auto& marking = *net.discovery_markings();
        assert(marking.detached_places.size() == 1);
        assert(marking.initial.size() == 1 && marking.final.size() == 1);
        assert(marking.initial[0].place.kind == PetriMarkingPlaceKind::kDetached);
        assert(marking.initial[0].place.id == marking.detached_places[0].id);
        assert(marking.initial[0].tokens == 1);
        assert(marking.final[0].place.kind == PetriMarkingPlaceKind::kRoot);
        assert(marking.final[0].place.id == net.sink());
        assert(marking.final[0].tokens == 1);
        const auto& name = std::get<HeuristicStructuredValue::Object>(
            marking.detached_places[0].name.value);
        assert(std::get<std::string>(name.at("boundary").value).empty());
        const auto& before = std::get<HeuristicStructuredValue::Array>(name.at("before").value);
        const auto& after = std::get<HeuristicStructuredValue::Array>(name.at("after").value);
        assert(before.size() == 1 && after.size() == 1);
        assert(std::get<std::string>(before[0].value) == "artificial_start");
        assert(std::get<std::string>(after[0].value) == "artificial_end");
        // Every visible loop still refers exclusively to live, unmarked places.
        for (const auto& transition : net.transitions()) {
          for (const auto p : transition.inputs) assert(p < net.place_count());
          for (const auto p : transition.outputs) assert(p < net.place_count());
        }
      }
    }
  }
}

void alpha_plus_creates_a_separate_interior_loop_place() {
  const auto net = discover_petri_net(
      log({trace({"a", "loop", "loop", "b"}), trace({"a", "b"})}),
      {.short_loops = true});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* loop = transition_for(net, "loop");
  assert(a != nullptr && b != nullptr && loop != nullptr);
  assert(a->outputs.size() == 1);
  assert(a->outputs == b->inputs);
  assert(loop->inputs.size() == 1);
  assert(loop->inputs == loop->outputs);
  assert(loop->inputs != a->outputs);
  assert(net.place_count() == 4);
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

void alpha_plus_uses_last_predecessor_and_aggregated_successors() {
  const auto mixed = discover_petri_net(
      log({trace({"loop", "loop", "a", "b"}),
           trace({"a", "loop", "loop", "b"})}),
      {.short_loops = true});
  const auto mixed_loops = transitions_for(mixed, "loop");
  assert(mixed_loops.size() == 1);
  const auto* mixed_loop = mixed_loops.front();
  assert(mixed_loop->inputs.size() == 1);
  assert(mixed_loop->inputs == mixed_loop->outputs);

  const auto both_boundaries = discover_petri_net(
      log({trace({"loop", "loop", "a"}),
           trace({"a", "loop", "loop"})}),
      {.short_loops = true});
  const auto boundary_loops = transitions_for(both_boundaries, "loop");
  assert(boundary_loops.size() == 1);
  const auto* boundary_loop = boundary_loops.front();
  assert(boundary_loop->inputs.size() == 1);
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

void alpha_loop_matches_an_empty_predecessor_difference() {
  const auto net = discover_petri_net(
      log({trace({"loop", "loop", "a", "loop", "loop"}),
           trace({"x", "b"}), trace({"c", "y"})}),
      {.short_loops = true});
  const auto loop_transitions = transitions_for(net, "loop");
  assert(loop_transitions.size() == 1);
  const auto* loop = loop_transitions.front();
  assert(loop->inputs.size() == 1);
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
  // Multiple reduced endpoints expose identity-dependent cleanup choices.
  // Every sampled result must retain the public graph/marking alternatives.
  for (int sample = 0; sample < 32; ++sample) {
    const auto result = discover_alpha_plus_result(
        log({trace({"a"}), trace({"artificial_end"})}));
    const auto* a = transition_for(result.net, "a");
    assert(a && a->inputs.size() == 1 && a->outputs.size() == 1);
    assert(result.place_names[a->outputs[0]].boundary == "end");
    if (result.detached_initial_marking.empty()) {
      assert(result.net.place_count() == 3);
      assert(result.net.transitions().size() == 3);
      assert(result.net.has_source());
    } else {
      assert(result.net.place_count() == 2);
      assert(result.net.transitions().size() == 2);
      assert(!result.net.has_source());
      assert(result.detached_initial_marking.size() == 1);
      assert(result.detached_initial_marking[0].name.boundary == "start");
    }
  }
  const auto reverse_boundary = discover_alpha_plus_result(
      log({trace({"a"}), trace({"artificial_end", "artificial_start"})}));
  bool reverse_pair_found = false;
  for (std::size_t p = 0; p < reverse_boundary.place_names.size(); ++p) {
    if (reverse_boundary.place_names[p] !=
        AlphaPlaceName{"", {"artificial_end"}, {"artificial_start"}}) continue;
    reverse_pair_found = true;
    std::size_t incoming = 0, outgoing = 0;
    for (const auto& t : reverse_boundary.net.transitions()) {
      if (contains(t.inputs, p)) { assert(t.silent()); ++outgoing; }
      if (contains(t.outputs, p)) { assert(t.silent()); ++incoming; }
    }
    assert(incoming == 1 && outgoing == 1);
  }
  assert(reverse_pair_found);
  for (const bool cleanup : {false, true}) {
    AlphaMinerOptions options;
    options.remove_unconnected = cleanup;
    // A boundary collision participates in loop removal. The first/last
    // remaining labels then become hidden boundary transitions.
    for (const auto& word : {trace({"artificial_start"}),
                            trace({"a", "a", "artificial_end"}),
                            trace({"artificial_start", "a", "artificial_end"})}) {
      bool rejected = false;
      try { (void)discover_alpha_plus_result(log({word}), options); }
      catch (const std::invalid_argument&) { rejected = true; }
      assert(rejected);
    }
    const auto shifted = discover_alpha_plus_result(
        log({trace({"a", "artificial_end"})}), options);
    assert(shifted.net.place_count() == 1);
    assert(!shifted.net.has_source());
    assert(shifted.detached_initial_marking.size() == 1);
    assert((shifted.detached_initial_marking[0].name ==
            AlphaPlaceName{"", {"artificial_start"}, {"a"}}));
    assert(transition_for(shifted.net, "a") == nullptr);
    assert(shifted.net.transitions().size() == (cleanup ? 0 : 1));
    if (!cleanup) {
      const auto* remaining = transition_for(shifted.net, "artificial_end");
      assert(remaining && remaining->inputs.empty() && remaining->outputs.empty());
    }
    const auto retained = discover_alpha_plus_result(
        log({trace({"artificial_end", "a", "artificial_end"})}), options);
    const auto* loop = transition_for(retained.net, "artificial_end");
    assert(loop && loop->inputs.size() == 1 && loop->inputs == loop->outputs);
    assert(retained.initial_marking[loop->inputs[0]] == 0);
    assert(retained.detached_initial_marking.size() == 1);
    assert(retained.detached_initial_marking[0].name == retained.place_names[loop->inputs[0]]);
    const auto ordinary = discover_alpha_plus_result(log({trace({"a"})}), options);
    assert(ordinary.net.activities() == std::vector<std::string>({"a"}));
  }
  // Raw results retain marking identity after boundary cleanup, including
  // detached places whose names coincide with live loop places.
  for (const bool cleanup : {false, true}) {
    AlphaMinerOptions options;
    options.remove_unconnected = cleanup;
    const auto empty = discover_alpha_plus_result(log({trace({})}), options);
    assert(empty.net.place_count() == 1);
    assert(!empty.net.has_source());
    assert(empty.net.transitions().empty());
    assert(empty.place_names == std::vector<AlphaPlaceName>({{"end", {}, {}}}));
    assert(empty.initial_marking == std::vector<std::uint64_t>({0}));
    assert(empty.final_marking == std::vector<std::uint64_t>({1}));
    assert(empty.detached_initial_marking.size() == 1);
    const AlphaPlaceName boundary_pair{"", {"artificial_start"}, {"artificial_end"}};
    assert(empty.detached_initial_marking[0].name == boundary_pair);
    assert(empty.detached_initial_marking[0].tokens == 1);
    assert(empty.detached_final_marking.empty());

    const auto loops = discover_alpha_plus_result(log({trace({"a", "a"})}), options);
    assert(loops.net.place_count() == 2);
    assert(!loops.net.has_source());
    assert(loops.initial_marking == std::vector<std::uint64_t>({0, 0}));
    assert(loops.detached_initial_marking.size() == 1);
    assert(loops.detached_initial_marking[0].name == boundary_pair);
    const auto* a = transition_for(loops.net, "a");
    assert(a && a->inputs.size() == 1 && a->inputs == a->outputs);
    assert(loops.place_names[a->inputs[0]] == boundary_pair);
    assert(loops.initial_marking[a->inputs[0]] == 0);
    assert(loops.final_marking[a->inputs[0]] == 0);
    assert(loops.final_marking[loops.net.sink()] == 1);

    const auto ordinary = discover_alpha_plus_result(log({trace({"a"})}), options);
    assert(ordinary.net.place_count() == 2);
    assert(ordinary.detached_initial_marking.empty());
    assert(ordinary.detached_final_marking.empty());
    const auto* visible = transition_for(ordinary.net, "a");
    assert(visible && visible->inputs.size() == 1 && visible->outputs.size() == 1);
    assert((ordinary.place_names[visible->inputs[0]] ==
            AlphaPlaceName{"", {"artificial_start"}, {"a"}}));
    assert(ordinary.place_names[visible->outputs[0]].boundary == "end");
    assert(ordinary.initial_marking[visible->inputs[0]] == 1);
    assert(ordinary.final_marking[visible->outputs[0]] == 1);
  }
  bool no_traces_rejected = false;
  try { (void)discover_alpha_plus_result(EventLog{}); }
  catch (const std::invalid_argument&) { no_traces_rejected = true; }
  assert(no_traces_rejected);
  alpha_plus_boundary_loops_use_unmarked_separate_places();
  alpha_plus_keeps_removed_initial_places_in_convenience_results();
  alpha_plus_creates_a_separate_interior_loop_place();
  alpha_plus_remove_unconnected_is_explicit();
  alpha_plus_uses_last_predecessor_and_aggregated_successors();
  alpha_uses_a_strict_custom_activity_field();
  alpha_custom_first_class_projection_is_authoritative();
  alpha_canonical_projection_precedes_retained_attribute();
  alpha_loop_matches_an_empty_predecessor_difference();
  alpha_plus_leaves_adjacent_distinct_loop_runs_unconnected();
  alpha_custom_projection_drives_model_decoration();
  alpha_default_projection_remains_compatible();
  return 0;
}
