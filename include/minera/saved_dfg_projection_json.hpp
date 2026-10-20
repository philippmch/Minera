#pragma once
#include <string>
#include <string_view>
#include "minera/dfg_alignment.hpp"
#include "minera/workflow_discovery_input_json.hpp"
namespace minera {
// Complete supplied state, never alignment search. NativeString labels accept
// text or {native_bytes:"hex-v1",data:"..."}; counters accept exact unsigned
// decimal tokens or strings. Nonfinite scores use NaN/INF/-INF strings.
// Explicit shapes: native_result, workbench.result, report.native_result.result.
// Input grammar limits: 32 MiB, 64 levels, one million values; duplicate keys
// and unknown native fields fail. The complete uploaded envelope stays with
// its caller; projection does not authenticate its source, model or fitness.
[[nodiscard]] DfgAlignmentLogResult parse_saved_dfg_alignment_result_json(std::string_view, std::string_view shape="native_result");
[[nodiscard]] std::string serialize_saved_dfg_alignment_result_json(const DfgAlignmentLogResult&);
// All original Alpha graph forms with required explicit boundaries are
// accepted; byte-safe boundary arrays [{activity,count}] extend object maps.
[[nodiscard]] AlphaDfgDiscoveryInput parse_saved_dfg_projection_model_json(std::string_view);
[[nodiscard]] std::string serialize_saved_dfg_projection_model_json(const AlphaDfgDiscoveryInput&);
[[nodiscard]] std::map<std::string,std::string> parse_saved_dfg_projection_selectors_json(std::string_view);
[[nodiscard]] std::string serialize_saved_dfg_projection_options_json(const DfgAlignmentOptions&);
[[nodiscard]] std::string serialize_saved_dfg_projection_metrics_json(const DfgAlignmentProjection&);
}
