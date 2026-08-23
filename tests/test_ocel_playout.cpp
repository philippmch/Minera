#include "minera/ocel_playout.hpp"

#include <cassert>
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

}  // namespace

int main() {
  extensive_contract();
  unique_objects_and_validation();
  shared_activity_binding_coalesces_relations();
  synchronized_binding_fires_one_global_activity();
  synchronized_exists_trace_reports_reachability();
  synchronized_variable_slots_bind_multiple_objects();
  explicit_type_markers_drive_both_bindings();
}
