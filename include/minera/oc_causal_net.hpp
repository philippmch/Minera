#pragma once

#include <string>

#include "minera/ocel.hpp"
#include "minera/ocel_playout.hpp"

namespace minera {

// A deterministic object-centric causal model. The workflow projection keeps
// one net per object type; this is the bounded causal projection used by the
// extensive playout adapter.
struct OcCausalNet {
  OcpnGraph workflow_projection;
};

[[nodiscard]] OcCausalNet discover_oc_causal_net(
    const Ocel& log, const OcpnDiscoveryOptions& options = {});

[[nodiscard]] std::string serialize_oc_causal_net_json(
    const OcCausalNet& net);

[[nodiscard]] Ocel oc_causal_net_extensive_playout(
    const OcCausalNet& net,
    const OcpnPlayoutOptions& options = {});

[[nodiscard]] OcpnPlayoutResult oc_causal_net_extensive_playout_result(
    const OcCausalNet& net,
    const OcpnPlayoutOptions& options = {});

[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const Ocel& log);
[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const Ocel& log, OcpnEventBinding binding);
[[nodiscard]] std::string serialize_oc_causal_net_playout_json(
    const OcpnPlayoutResult& result, OcpnEventBinding binding);

}  // namespace minera
