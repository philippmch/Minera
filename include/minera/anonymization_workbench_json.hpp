#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/discovery_workbench_json.hpp"
#include "minera/pripel.hpp"
#include "minera/trace_variant_query.hpp"

namespace minera {
// Storage parsing only: the selected native callable owns semantic validation.
// NativeFloat64 follows existing semantic NaN canonicalization, not raw bits.
[[nodiscard]] TraceVariantQueryOptions parse_anonymization_tvq_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] PripelOptions parse_anonymization_pripel_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] PripelDiagnostics parse_anonymization_pripel_diagnostics_json(std::string_view);
[[nodiscard]] std::string serialize_anonymization_workbench_options_json(const TraceVariantQueryOptions&);
[[nodiscard]] std::string serialize_anonymization_workbench_options_json(const PripelOptions&);
[[nodiscard]] std::string serialize_anonymization_workbench_diagnostics_json(const TraceVariantQueryDiagnostics&);
[[nodiscard]] std::string serialize_anonymization_workbench_diagnostics_json(const PripelDiagnostics&);
[[nodiscard]] std::string serialize_anonymization_workbench_result_json(const TraceVariantQueryResult&);
[[nodiscard]] std::string serialize_anonymization_workbench_result_json(const PripelResult&);
struct AnonymizationWorkbenchResponse { int status = 200; std::string json; };
// Role names are independent of filename-attribute presence. Raw logs go to the
// existing complete public parser on their original spans, without conversion.
[[nodiscard]] AnonymizationWorkbenchResponse execute_anonymization_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query = {});
[[nodiscard]] AnonymizationWorkbenchResponse execute_anonymization_workbench_multipart(
    std::string_view content_type, std::string_view body,
    const std::vector<DiscoveryWorkbenchPart>& query = {}, bool transport_complete = true);
}  // namespace minera
