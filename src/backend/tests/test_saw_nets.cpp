#include "minera/saw_nets.hpp"

#include <cassert>
#include <string>

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

}  // namespace

int main() {
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
