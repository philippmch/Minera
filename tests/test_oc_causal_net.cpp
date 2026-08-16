#include "minera/oc_causal_net.hpp"

#include <cassert>
#include <iostream>

namespace {
minera::Ocel sample() {
  minera::Ocel log;
  log.events = {
      {"e1", "Create", "2026-01-01T00:00:00Z", {}},
      {"e2", "Ship", "2026-01-01T00:00:01Z", {}}};
  log.objects = {{"o1", "order", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""}};
  return log;
}
}

int main() {
  const auto model = minera::discover_oc_causal_net(sample());
  assert(model.workflow_projection.object_types.size() == 1);
  const auto model_json = minera::serialize_oc_causal_net_json(model);
  assert(model_json.find("\"family\":\"OC_CAUSAL_NET\"") != std::string::npos);
  minera::OcpnPlayoutOptions options;
  options.max_traces = 1;
  const auto log = minera::oc_causal_net_extensive_playout(model, options);
  assert(!log.events.empty());
  const auto json = minera::serialize_oc_causal_net_playout_json(log);
  assert(json.find("\"family\":\"OC_CAUSAL_NET_PLAYOUT\"") != std::string::npos);
  const auto bound_json = minera::serialize_oc_causal_net_playout_json(
      log, minera::OcpnEventBinding::kSharedActivity);
  assert(bound_json.find("\"event_binding\":\"shared_activity\"") !=
         std::string::npos);
  std::cout << "OC causal-net tests passed\n";
}
