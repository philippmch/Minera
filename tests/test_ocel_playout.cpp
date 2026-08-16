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

}  // namespace

int main() {
  extensive_contract();
  unique_objects_and_validation();
  shared_activity_binding_coalesces_relations();
}
