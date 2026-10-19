#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/temporal_profile.hpp"
#include "minera/temporal_profile_json.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// Complete structural stored-value codecs. Only selected public native
// operations validate temporal meaning; these helpers never call them.
[[nodiscard]] TemporalProfileOptions parse_temporal_workbench_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_temporal_workbench_options_json(const TemporalProfileOptions&);
[[nodiscard]] TemporalProfile parse_temporal_workbench_profile_json(std::string_view);
[[nodiscard]] TemporalProfileEntry parse_temporal_workbench_entry_json(std::string_view);
[[nodiscard]] TemporalDeviation parse_temporal_workbench_deviation_json(std::string_view);
[[nodiscard]] std::vector<TemporalDeviation> parse_temporal_workbench_deviations_json(std::string_view);
[[nodiscard]] TemporalConformanceResult parse_temporal_workbench_conformance_json(std::string_view);
[[nodiscard]] std::string serialize_temporal_workbench_result_json(const TemporalProfile&);
[[nodiscard]] std::string serialize_temporal_workbench_result_json(const TemporalProfileEntry&);
[[nodiscard]] std::string serialize_temporal_workbench_result_json(const TemporalDeviation&);
[[nodiscard]] std::string serialize_temporal_workbench_result_json(const std::vector<TemporalDeviation>&);
[[nodiscard]] std::string serialize_temporal_workbench_result_json(const TemporalConformanceResult&);
[[nodiscard]] std::string serialize_temporal_workbench_scalar_json(double);
struct TemporalWorkbenchResponse {int status=200;std::string json;};
[[nodiscard]] TemporalWorkbenchResponse execute_temporal_workbench(const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] TemporalWorkbenchResponse execute_temporal_workbench_multipart(std::string_view,std::string_view,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
