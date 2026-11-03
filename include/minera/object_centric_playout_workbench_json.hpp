#pragma once

#include <set>
#include <string>
#include <string_view>

#include "minera/oc_causal_net.hpp"
#include "minera/ocel_playout.hpp"

namespace minera {
// Full literal native storage. These codecs do not invoke algorithm/export
// semantic validators. Every std::string (including map keys) uses the existing
// byte-safe NativeString carrier; arbitrary integer tokens are never rounded.
[[nodiscard]] OcpnGraph parse_oc_playout_workbench_ocpn_json(std::string_view);
[[nodiscard]] std::string serialize_oc_playout_workbench_ocpn_json(const OcpnGraph&);
[[nodiscard]] OcCausalNet parse_oc_playout_workbench_causal_net_json(std::string_view);
[[nodiscard]] std::string serialize_oc_playout_workbench_causal_net_json(const OcCausalNet&);
[[nodiscard]] OcpnPlayoutOptions parse_oc_playout_workbench_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_oc_playout_workbench_options_json(const OcpnPlayoutOptions&);
[[nodiscard]] Ocel parse_oc_playout_workbench_ocel_json(std::string_view);
[[nodiscard]] std::string serialize_oc_playout_workbench_ocel_json(const Ocel&);
[[nodiscard]] OcpnPlayoutResult parse_oc_playout_workbench_result_json(std::string_view);
[[nodiscard]] std::string serialize_oc_playout_workbench_result_json(const OcpnPlayoutResult&);
[[nodiscard]] PetriNet parse_oc_playout_workbench_petri_net_json(std::string_view);
// Writes the nine PetriNetLiteralStorage fields the workbench clients validate;
// native place names are not part of the carrier (the parser still accepts
// an optional place_names field).
[[nodiscard]] std::string serialize_oc_playout_workbench_petri_net_json(const PetriNet&);
}  // namespace minera
