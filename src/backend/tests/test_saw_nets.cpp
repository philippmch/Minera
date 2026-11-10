#include "minera/saw_nets.hpp"
#include "minera/inductive_miner.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.objects = {{"o1", "order", {}}, {"i1", "item", {}}};
  log.events = {{"e1", "Create", "2026-01-01T00:00:00Z", {}},
                {"e2", "Ship", "2026-01-01T00:00:04Z", {}},
                {"e3", "Create", "2026-01-01T00:00:00Z", {}},
                {"e4", "Ship", "2026-01-01T00:00:04Z", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""},
                   {"e3", "i1", ""}, {"e4", "i1", ""}};
  return log;
}

minera::Ocel repeated_activity_sample() {
  minera::Ocel log;
  log.objects = {{"o1", "order", {}}};
  log.events = {{"e1", "Create", "2026-01-01T00:00:00Z", {}},
                {"e2", "Create", "2026-01-01T00:00:01Z", {}},
                {"e3", "Ship", "2026-01-01T00:00:02Z", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""},
                   {"e3", "o1", ""}};
  return log;
}

minera::Ocel shared_event_sample() {
  minera::Ocel log;
  log.objects = {{"o1", "order", {}}, {"o2", "order", {}}};
  log.events = {{"e1", "Create", "2026-01-01T00:00:00Z", {}}};
  log.relations = {{"e1", "o1", ""}, {"e1", "o2", ""}};
  return log;
}

minera::Ocel policy_from_traces(
    const std::vector<std::vector<std::string>>& traces) {
  minera::Ocel log;
  for (std::size_t object = 0; object < traces.size(); ++object) {
    const auto object_id = "o" + std::to_string(object);
    log.objects.push_back({object_id, "order", {}});
    for (std::size_t index = 0; index < traces[object].size(); ++index) {
      const auto event_id = object_id + "e" + std::to_string(index);
      log.events.push_back({event_id, traces[object][index],
                            "2026-01-01T00:00:00Z", {}});
      log.relations.push_back({event_id, object_id, ""});
    }
  }
  return log;
}

minera::Ocel policy_sample() {
  return policy_from_traces({{"A", "B", "D"}, {"A", "C", "D"}});
}

std::string net_signature(const minera::PetriNet& net) {
  std::ostringstream out;
  out << net.place_count() << ':' << net.source() << ':' << net.sink() << ':';
  for (const auto& activity : net.activities()) out << activity << ',';
  out << '|';
  for (const auto& transition : net.transitions()) {
    if (transition.activity) {
      out << 'a' << *transition.activity;
    } else {
      out << 't';
    }
    out << '[';
    for (const auto place : transition.inputs) out << place << ',';
    out << "]->[";
    for (const auto place : transition.outputs) out << place << ',';
    out << "]|";
  }
  return out.str();
}

minera::EventLog event_log_from_traces(
    const std::vector<std::vector<std::string>>& traces) {
  minera::EventLog log;
  for (std::size_t case_index = 0; case_index < traces.size(); ++case_index) {
    minera::Trace trace;
    trace.case_id = "c" + std::to_string(case_index);
    for (std::size_t index = 0; index < traces[case_index].size(); ++index) {
      trace.events.push_back({trace.case_id, traces[case_index][index],
                              "2026-01-01T00:00:00Z", {}, index, {}, {}});
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::string miner_signature(
    const std::vector<std::vector<std::string>>& traces, bool strict,
    minera::InductiveMinerVariant variant = minera::InductiveMinerVariant::kLog) {
  minera::InductiveMinerOptions options;
  options.disable_strict_sequence_cut = !strict;
  options.variant = variant;
  const auto tree = minera::discover_process_tree(event_log_from_traces(traces),
                                                  options);
  return net_signature(minera::to_petri_net(tree));
}

}  // namespace

int main() {
  const std::vector<std::vector<std::string>> policy_sensitive = {
      {"A", "D", "B"}, {"C", "A", "C"}, {"C", "D", "D", "B"}};
  const auto strict_policy = miner_signature(
      policy_sensitive, true, minera::InductiveMinerVariant::kDfg);
  const auto normal_policy = miner_signature(
      policy_sensitive, false, minera::InductiveMinerVariant::kDfg);
  assert(strict_policy != normal_policy);

  // The second sequence group contains globally non-contiguous activities
  // (B and C are interned around D).  Recursing into that projected group must
  // still use compact local ids.
  const auto sparse_projection =
      miner_signature({{"A", "B", "D"}, {"A", "C", "D"}}, true);
  assert(!sparse_projection.empty());
  const auto sparse_projection_dfg = miner_signature(
      {{"A", "B", "D"}, {"A", "C", "D"}}, true,
      minera::InductiveMinerVariant::kDfg);
  assert(!sparse_projection_dfg.empty());

  const auto policy = policy_sample();
  const auto saw_policy = minera::discover_saw_nets(policy);
  minera::OcpnDiscoveryOptions saw_options;
  saw_options.disable_fallthroughs = true;
  saw_options.disable_strict_sequence_cut = true;
  const auto expected = minera::discover_ocpn(policy, saw_options);
  const auto& expected_net = expected.petri_nets.at("order");
  const auto& saw_net = saw_policy.ot_saw_nets.at("order").net;
  // The SAW projection must use the same fixed miner policy as an explicitly
  // configured object-centric discovery, including transition topology.
  assert(net_signature(saw_net) == net_signature(expected_net));
  const auto result = minera::discover_saw_nets(sample());
  assert(result.ot_saw_nets.size() == 2);
  assert(result.ot_saw_nets.at("order").net.activities().size() == 2);
  assert(!result.ot_saw_nets.at("order").transition_weights.empty());
  assert(!result.multi_saw_net.net.activities().empty());
  const auto json = minera::saw_nets_json(result);
  assert(json.find("\"family\":\"SAW_NETS\"") != std::string::npos);
  assert(json.find("weight_distribution") != std::string::npos);
  assert(!result.multi_arc_object_types.empty());
  for (const auto& [arc, type] : result.multi_arc_object_types) {
    assert(arc.find(":input:") != std::string::npos ||
           arc.find(":output:") != std::string::npos);
    assert(type == "item" || type == "order");
  }
  assert(json.find("\"multi_arc_object_types\":{") != std::string::npos);
  assert(json == minera::saw_nets_json(result));
  // An OCEL without events has no object type: the merged net is empty, no
  // places, as the reference's multi SAW net is.
  const auto none = minera::discover_saw_nets(minera::Ocel{});
  assert(none.ot_saw_nets.empty() && none.multi_saw_net.net.place_count() == 0 &&
         none.multi_saw_net.net.transitions().empty());
  assert(minera::saw_nets_json(none).find("\"multi_saw_net\":{\"object_type\":\"multi\",\"place_count\":0,") !=
         std::string::npos);

  const auto repeated = minera::discover_saw_nets(repeated_activity_sample());
  const auto& repeated_saw = repeated.ot_saw_nets.at("order");
  bool found_repeated_create = false;
  for (std::size_t i = 0; i < repeated_saw.net.transitions().size(); ++i) {
    const auto& transition = repeated_saw.net.transitions()[i];
    if (transition.activity &&
        repeated_saw.net.activities()[*transition.activity] == "Create") {
      const auto weights = repeated_saw.transition_weights.find(i);
      assert(weights != repeated_saw.transition_weights.end());
      // Two Create events each fire the same model transition once.  The
      // distribution therefore records two observations of multiplicity 1,
      // rather than one collapsed observation of multiplicity 2.
      assert(weights->second.size() == 1);
      assert(weights->second.at(1) == 2);
      found_repeated_create = true;
    }
  }
  assert(found_repeated_create);

  const auto shared = minera::discover_saw_nets(shared_event_sample());
  const auto& shared_saw = shared.ot_saw_nets.at("order");
  bool found_shared_create = false;
  for (std::size_t i = 0; i < shared_saw.net.transitions().size(); ++i) {
    const auto& transition = shared_saw.net.transitions()[i];
    if (transition.activity &&
        shared_saw.net.activities()[*transition.activity] == "Create") {
      const auto weights = shared_saw.transition_weights.find(i);
      assert(weights != shared_saw.transition_weights.end());
      // The same event is related to two order objects, so it contributes one
      // observation with multiplicity 2 rather than two observations of 1.
      assert(weights->second.size() == 1);
      assert(weights->second.at(2) == 1);
      found_shared_create = true;
    }
  }
  assert(found_shared_create);
}
