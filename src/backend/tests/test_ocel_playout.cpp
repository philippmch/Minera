#include "minera/ocel_playout.hpp"

#include <cassert>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {

minera::OcpnGraph graph() {
  minera::OcpnGraph result;
  result.object_types = {"order"};
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"Create", "Ship"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  result.petri_nets.emplace("order", std::move(net));
  return result;
}

minera::OcpnGraph shared_graph() {
  auto result = graph();
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"Create", "Ship"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  result.object_types.push_back("item");
  result.petri_nets.emplace("item", std::move(net));
  result.activity_object_types["Create"] = {"item", "order"};
  result.activity_object_types["Ship"] = {"item", "order"};
  return result;
}

minera::OcpnGraph branching_graph() {
  minera::OcpnGraph result;
  result.object_types = {"order"};
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  result.petri_nets.emplace("order", std::move(net));
  return result;
}

minera::OcpnGraph shared_branching_graph() {
  auto result = branching_graph();
  minera::PetriNet item;
  const auto source = item.add_place();
  const auto sink = item.add_place();
  item.set_source(source);
  item.set_sink(sink);
  item.set_activities({"A", "B"});
  item.add_transition(0, {source}, {sink});
  item.add_transition(1, {source}, {sink});
  result.object_types.push_back("item");
  result.petri_nets.emplace("item", std::move(item));
  return result;
}

minera::OcpnGraph dead_branching_graph() {
  minera::OcpnGraph result;
  result.object_types = {"order"};
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto dead = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {dead});
  result.petri_nets.emplace("order", std::move(net));
  return result;
}

minera::OcpnGraph duplicate_label_graph() {
  minera::OcpnGraph result;
  result.object_types = {"order"};
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  result.petri_nets.emplace("order", std::move(net));
  return result;
}

std::string activity_signature(const minera::Ocel& log) {
  std::string result;
  for (const auto& event : log.events) result += event.activity + ",";
  return result;
}

std::map<std::string, std::string> per_object_activities(
    const minera::Ocel& log) {
  std::map<std::string, std::string> activities;
  std::map<std::string, std::string> event_activities;
  for (const auto& event : log.events) event_activities[event.id] = event.activity;
  for (const auto& relation : log.relations)
    activities[relation.object_id] = event_activities[relation.event_id];
  return activities;
}

std::string per_object_activity_signature(const minera::Ocel& log) {
  const auto activities = per_object_activities(log);
  std::string result;
  for (const auto& [object_id, activity] : activities)
    result += object_id + '=' + activity + ';';
  return result;
}

std::string sequence_signature(
    const std::vector<std::vector<minera::OcpnBinding>>& sequences) {
  std::string result;
  for (const auto& sequence : sequences) {
    result += '[';
    for (const auto& binding : sequence) {
      result += binding.activity + ':';
      for (const auto& [type, id] : binding.objects)
        result += type + '=' + id + ',';
      result += ';';
    }
    result += ']';
  }
  return result;
}

void extensive_contract() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 4;
  options.objects["order"] = {"o1", "o2"};
  const auto log = minera::ocpn_extensive_playout(graph(), options);
  assert(log.objects.size() == 2);
  assert(log.events.size() == 4);
  assert(log.relations.size() == 4);
  assert(log.events[0].activity == "Create");
  assert(log.events[1].activity == "Create");
  assert(log.events[2].activity == "Ship");
  assert(log.relations[0].object_id == "o1");
  assert(log.relations[1].object_id == "o2");
  const auto json = minera::serialize_ocpn_playout_json(log);
  assert(json.find("\"family\":\"OCEL_PLAYOUT\"") != std::string::npos);
  assert(json.find("EXTENSIVE") != std::string::npos);
  const auto shared_json = minera::serialize_ocpn_playout_json(
      log, minera::OcpnEventBinding::kSharedActivity);
  assert(shared_json.find("\"event_binding\":\"shared_activity\"") !=
         std::string::npos);
}

void unique_objects_and_validation() {
  minera::OcpnPlayoutOptions options;
  options.objects_unique_per_trace = true;
  options.max_traces = 1;
  const auto log = minera::ocpn_extensive_playout(graph(), options);
  assert(log.objects.size() == 1);
  assert(log.objects[0].id == "order_1_1");

  options.max_bindings_per_activity = 0;
  bool threw = false;
  try {
    (void)minera::ocpn_extensive_playout(graph(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void shared_activity_binding_coalesces_relations() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.objects["order"] = {"o1"};
  options.objects["item"] = {"i1"};
  options.event_binding = minera::OcpnEventBinding::kSharedActivity;
  const auto log = minera::ocpn_extensive_playout(shared_graph(), options);
  assert(log.events.size() == 2);
  assert(log.relations.size() == 4);
  assert(log.events[0].activity == "Create");
  assert(log.events[1].activity == "Ship");
  assert(log.relations[0].event_id == log.events[0].id);
  assert(log.relations[1].event_id == log.events[0].id);
  assert(log.relations[2].event_id == log.events[1].id);
  assert(log.relations[3].event_id == log.events[1].id);
  assert(log.relations[0].object_id == "i1");
  assert(log.relations[1].object_id == "o1");
}

void synchronized_binding_fires_one_global_activity() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.max_states = 100;
  options.objects["order"] = {"o1"};
  options.objects["item"] = {"i1"};
  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  options.return_sequences = true;
  const auto result = minera::ocpn_extensive_playout_result(shared_graph(), options);
  assert(result.trace_exists);
  assert(result.log.events.size() == 2);
  assert(result.log.relations.size() == 4);
  assert(result.log.events[0].activity == "Create");
  assert(result.log.events[1].activity == "Ship");
  assert(result.sequences.size() == 1);
  assert(result.sequences[0].size() == 2);
  assert(result.sequences[0][0].objects.size() == 2);
  assert(result.sequences[0][0].objects[0] ==
         std::make_pair(std::string("item"), std::string("i1")));
  assert(result.sequences[0][0].objects[1] ==
         std::make_pair(std::string("order"), std::string("o1")));
  const auto json = minera::serialize_ocpn_playout_json(
      result, options.event_binding);
  assert(json.find("\"event_binding\":\"synchronized\"") !=
         std::string::npos);
  assert(json.find("\"sequences\"") != std::string::npos);
}

void synchronized_exists_trace_reports_reachability() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_states = 100;
  options.objects["order"] = {"o1"};
  options.objects["item"] = {"i1"};
  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  options.exists_trace = true;
  const auto result = minera::ocpn_extensive_playout_result(shared_graph(), options);
  assert(result.trace_exists);
  assert(result.log.events.empty());
  const auto json = minera::serialize_ocpn_playout_json(
      result, options.event_binding);
  assert(json.find("\"exists_trace\":true") != std::string::npos);
}

void synchronized_variable_slots_bind_multiple_objects() {
  auto variable_graph = shared_graph();
  variable_graph.activity_object_types["Create"] = {"order"};
  variable_graph.activity_object_types["Ship"] = {"order"};
  variable_graph.activity_variable_object_types["Create"] = {"item"};
  variable_graph.activity_variable_object_types["Ship"] = {"item"};

  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.max_states = 1000;
  options.objects["order"] = {"o1"};
  options.objects["item"] = {"i1", "i2"};
  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  options.return_sequences = true;
  const auto result = minera::ocpn_extensive_playout_result(variable_graph, options);
  assert(result.trace_exists);
  assert(result.log.events.size() == 2);
  assert(result.log.relations.size() == 6);
  assert(result.sequences.size() == 1);
  assert(result.sequences[0][0].objects.size() == 3);
}

void explicit_type_markers_drive_both_bindings() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.objects["order"] = {"o1"};
  options.initial_markings["order"] = {0, 1, 0};
  options.final_markings["order"] = {0, 0, 1};
  options.return_sequences = true;

  const auto legacy = minera::ocpn_extensive_playout_result(graph(), options);
  assert(legacy.trace_exists);
  assert(legacy.log.events.size() == 1);
  assert(legacy.log.events.front().activity == "Ship");
  assert(legacy.sequences.size() == 1 && legacy.sequences.front().size() == 1);

  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  const auto synchronized =
      minera::ocpn_extensive_playout_result(graph(), options);
  assert(synchronized.trace_exists);
  assert(synchronized.log.events.size() == 1);
  assert(synchronized.log.events.front().activity == "Ship");
  assert(synchronized.sequences.size() == 1 &&
         synchronized.sequences.front().size() == 1);

  options.final_markings["unknown"] = {0, 0, 1};
  bool rejected = false;
  try {
    (void)minera::ocpn_extensive_playout(graph(), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void return_traces_preserves_duplicate_transition_identity() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 4;
  options.max_states = 100;
  options.objects["order"] = {"o1"};
  options.return_traces = true;

  auto duplicate_graph = duplicate_label_graph();
  auto regular_options = options;
  regular_options.return_traces = false;
  const auto regular =
      minera::ocpn_extensive_playout(duplicate_graph, regular_options);
  assert(regular.events.size() == 2);
  const auto result = minera::ocpn_extensive_playout_result(
      duplicate_graph, options);
  assert(result.trace_exists);
  assert(result.sequences.empty());
  assert(result.traces.size() == 2);
  assert(result.traces[0].size() == 2);
  assert(result.traces[1].size() == 2);
  assert(result.traces[0][0].activity == "A");
  assert(result.traces[0][0].transitions.size() == 1);
  assert(result.traces[0][0].transitions[0].object_id == "o1");
  assert(result.traces[0][0].transitions[0].transition_index == 0);
  assert(result.traces[1][0].transitions[0].transition_index == 1);
  assert(result.traces[0][1].transitions[0].transition_index == 2);
  assert(result.traces[1][1].transitions[0].transition_index == 2);

  const auto json = minera::serialize_ocpn_playout_json(
      result, minera::OcpnEventBinding::kIndependent);
  assert(json.find("\"traces\"") != std::string::npos);
  assert(json.find("\"transition_index\":1") != std::string::npos);

  options.max_traces = 1;
  const auto bounded = minera::ocpn_extensive_playout_result(
      duplicate_label_graph(), options);
  assert(bounded.traces.size() == 1);

  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  options.max_traces = 4;
  const auto synchronized = minera::ocpn_extensive_playout_result(
      duplicate_label_graph(), options);
  assert(synchronized.traces.size() == 2);
  assert(synchronized.traces[0][0].transitions.size() == 1);
  assert(synchronized.traces[1][0].transitions.size() == 1);
  assert(synchronized.traces[0][0].transitions[0].transition_index == 0);
  assert(synchronized.traces[1][0].transitions[0].transition_index == 1);
}

void synchronized_branching_is_seeded_and_fractional() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.max_states = 100;
  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  options.return_sequences = true;
  options.branching_factor_transitions = 1.0;
  options.seed = 42;
  const auto first = minera::ocpn_extensive_playout_result(branching_graph(), options);
  const auto second = minera::ocpn_extensive_playout_result(branching_graph(), options);
  assert(first.trace_exists);
  assert(first.log.events.size() == 1);
  assert(first.log.events.front().activity == "A" ||
         first.log.events.front().activity == "B");
  assert(minera::serialize_ocpn_playout_json(first, options.event_binding) ==
         minera::serialize_ocpn_playout_json(second, options.event_binding));

  options.branching_factor_transitions = 0.5;
  const auto fractional_first =
      minera::ocpn_extensive_playout_result(branching_graph(), options);
  const auto fractional_second =
      minera::ocpn_extensive_playout_result(branching_graph(), options);
  assert(fractional_first.trace_exists == fractional_second.trace_exists);
  assert(minera::serialize_ocpn_playout_json(
             fractional_first, options.event_binding) ==
         minera::serialize_ocpn_playout_json(
             fractional_second, options.event_binding));

  options.branching_factor_transitions = -1.0;
  bool rejected = false;
  try {
    (void)minera::ocpn_extensive_playout_result(branching_graph(), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void legacy_branching_is_seeded_for_independent_and_shared_modes() {
  for (const auto binding : {minera::OcpnEventBinding::kIndependent,
                             minera::OcpnEventBinding::kSharedActivity}) {
    const auto graph = binding == minera::OcpnEventBinding::kIndependent
                           ? branching_graph()
                           : shared_branching_graph();
    minera::OcpnPlayoutOptions options;
    options.max_bindings_per_activity = 1;
    options.max_traces = 4;
    options.event_binding = binding;
    options.branching_factor_transitions = 1.0;
    options.return_sequences = true;
    options.seed = 19;
    const auto first = minera::ocpn_extensive_playout_result(graph, options);
    const auto second = minera::ocpn_extensive_playout_result(graph, options);
    assert(first.trace_exists);
    assert(activity_signature(first.log) == activity_signature(second.log));
    assert(sequence_signature(first.sequences) ==
           sequence_signature(second.sequences));

    std::set<std::string> transition_choices;
    for (std::uint64_t seed = 1; seed <= 64; ++seed) {
      options.seed = seed;
      transition_choices.insert(activity_signature(
          minera::ocpn_extensive_playout_result(graph, options).log));
    }
    assert(transition_choices.size() >= 2);

    // A binding cap samples the per-object trace choices after exhaustive
    // transition expansion; it is independently seeded for each object.
    options.seed = 19;
    options.branching_factor_transitions = 0.0;
    options.branching_factor_bindings = 1.0;
    const auto bound_first =
        minera::ocpn_extensive_playout_result(graph, options);
    const auto bound_second =
        minera::ocpn_extensive_playout_result(graph, options);
    assert(activity_signature(bound_first.log) ==
           activity_signature(bound_second.log));
    assert(sequence_signature(bound_first.sequences) ==
           sequence_signature(bound_second.sequences));

    options.branching_factor_transitions = 0.5;
    options.branching_factor_bindings = 0.5;
    const auto fractional_first =
        minera::ocpn_extensive_playout_result(graph, options);
    const auto fractional_second =
        minera::ocpn_extensive_playout_result(graph, options);
    assert(activity_signature(fractional_first.log) ==
           activity_signature(fractional_second.log));
  }
}

void legacy_transition_sampling_is_per_object() {
  auto graph = branching_graph();
  minera::OcpnPlayoutOptions options;
  options.objects["order"] = {"o1", "o2"};
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.branching_factor_transitions = 1.0;
  options.seed = 1;

  std::set<std::string> per_seed_choices;
  bool saw_different_object_choices = false;
  for (std::uint64_t seed = 1; seed <= 64; ++seed) {
    options.seed = seed;
    const auto log = minera::ocpn_extensive_playout(graph, options);
    per_seed_choices.insert(per_object_activity_signature(log));
    const auto activities = per_object_activities(log);
    if (activities.size() == 2 && activities.at("o1") != activities.at("o2"))
      saw_different_object_choices = true;
  }
  assert(per_seed_choices.size() >= 2);
  assert(saw_different_object_choices);

  options.seed = 17;
  const auto first = minera::ocpn_extensive_playout(graph, options);
  const auto second = minera::ocpn_extensive_playout(graph, options);
  assert(per_object_activity_signature(first) ==
         per_object_activity_signature(second));
}

void legacy_branching_rejects_incomplete_object_products() {
  minera::OcpnPlayoutOptions options;
  options.max_bindings_per_activity = 1;
  options.max_traces = 1;
  options.branching_factor_transitions = 1.0;

  bool found_mixed_feasibility = false;
  for (std::uint64_t seed = 1; seed <= 128; ++seed) {
    options.seed = seed;
    options.objects["order"] = {"o1"};
    const auto first = minera::ocpn_extensive_playout(dead_branching_graph(), options);
    options.objects["order"] = {"o2"};
    const auto second = minera::ocpn_extensive_playout(dead_branching_graph(), options);
    if (first.events.empty() == second.events.empty()) continue;

    options.objects["order"] = {"o1", "o2"};
    const auto combined =
        minera::ocpn_extensive_playout(dead_branching_graph(), options);
    assert(combined.events.empty());
    found_mixed_feasibility = true;
    break;
  }
  assert(found_mixed_feasibility);
}

void malformed_result_is_rejected() {
  minera::OcpnPlayoutResult result;
  result.sequences.push_back({});
  bool threw = false;
  try {
    (void)minera::serialize_ocpn_playout_json(
        result, minera::OcpnEventBinding::kIndependent);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  result.return_sequences_requested = true;
  result.sequences = {{{"", {{"order", "o1"}}}}};
  threw = false;
  try {
    (void)minera::serialize_ocpn_playout_json(
        result, minera::OcpnEventBinding::kIndependent);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)minera::serialize_ocpn_playout_json(
        result.log, static_cast<minera::OcpnEventBinding>(99));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  extensive_contract();
  unique_objects_and_validation();
  shared_activity_binding_coalesces_relations();
  synchronized_binding_fires_one_global_activity();
  synchronized_exists_trace_reports_reachability();
  synchronized_variable_slots_bind_multiple_objects();
  explicit_type_markers_drive_both_bindings();
  return_traces_preserves_duplicate_transition_identity();
  synchronized_branching_is_seeded_and_fractional();
  legacy_branching_is_seeded_for_independent_and_shared_modes();
  legacy_transition_sampling_is_per_object();
  legacy_branching_rejects_incomplete_object_products();
  malformed_result_is_rejected();
}
