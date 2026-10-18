#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/discovery_workbench_json.hpp"
#include "minera/root_cause.hpp"

namespace minera {
// Structural storage parsing only. The selected native public function owns
// attribute, balancing and float32 validation and their validation order.
[[nodiscard]] RootCauseOptions parse_root_cause_workbench_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_root_cause_workbench_options_json(const RootCauseOptions&);
// Complete 4/8/7 stored records; optional null remains disengaged. NativeFloat64
// uses existing semantic NaN canonicalization, not payload/sign/signalling bits.
[[nodiscard]] std::string serialize_root_cause_workbench_result_json(const RootCauseRepresentation&);
[[nodiscard]] std::string serialize_root_cause_workbench_result_json(const RootCauseModel&);
[[nodiscard]] std::string serialize_root_cause_workbench_result_json(const std::optional<RootCauseModel>&);

struct RootCauseWorkbenchResponse { int status = 200; std::string json; };
// Physical role names own input independently of filename attribute presence.
// Finite request-local graphs retain true shared_ptr aliases, nulls and cycles.
[[nodiscard]] RootCauseWorkbenchResponse execute_root_cause_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query = {});
[[nodiscard]] RootCauseWorkbenchResponse execute_root_cause_workbench_multipart(
    std::string_view content_type, std::string_view body,
    const std::vector<DiscoveryWorkbenchPart>& query = {}, bool transport_complete = true);
}  // namespace minera
