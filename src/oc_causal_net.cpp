#include "minera/oc_causal_net.hpp"

#include <sstream>

namespace minera {

OcCausalNet discover_oc_causal_net(const Ocel& log,
                                   const OcpnDiscoveryOptions& options) {
  return {discover_ocpn(log, options)};
}

std::string serialize_oc_causal_net_json(const OcCausalNet& net) {
  return std::string(
             "{\"family\":\"OC_CAUSAL_NET\",\"variant\":\"WORKFLOW_PROJECTION\",\"model\":") +
         serialize_ocpn_json(net.workflow_projection) + "}";
}

Ocel oc_causal_net_extensive_playout(const OcCausalNet& net,
                                     const OcpnPlayoutOptions& options) {
  return ocpn_extensive_playout(net.workflow_projection, options);
}

OcpnPlayoutResult oc_causal_net_extensive_playout_result(
    const OcCausalNet& net, const OcpnPlayoutOptions& options) {
  return ocpn_extensive_playout_result(net.workflow_projection, options);
}

std::string serialize_oc_causal_net_playout_json(const Ocel& log) {
  return serialize_oc_causal_net_playout_json(log,
                                              OcpnEventBinding::kIndependent);
}

std::string serialize_oc_causal_net_playout_json(const Ocel& log,
                                                 OcpnEventBinding binding) {
  return std::string(
             "{\"family\":\"OC_CAUSAL_NET_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"event_binding\":\"") +
         ocpn_event_binding_name(binding) + "\",\"log\":" +
         serialize_ocel_json(log) + "}";
}

std::string serialize_oc_causal_net_playout_json(
    const OcpnPlayoutResult& result, OcpnEventBinding binding) {
  auto json = serialize_ocpn_playout_json(result, binding);
  const auto family = std::string("\"family\":\"OCEL_PLAYOUT\"");
  const auto replacement = std::string("\"family\":\"OC_CAUSAL_NET_PLAYOUT\"");
  const auto position = json.find(family);
  if (position != std::string::npos) json.replace(position, family.size(), replacement);
  return json;
}

}  // namespace minera
