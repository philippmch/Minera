#pragma once
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/organizational_mining.hpp"
#include "minera/role_discovery.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/network_analysis.hpp"
#include "minera/local_diagnostics.hpp"
#include "minera/json_export.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// Complete stored-value codecs; native public functions own semantic validation.
[[nodiscard]] OrganizationalSnaOptions parse_organizational_sna_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_organizational_workbench_options_json(const OrganizationalSnaOptions&);
[[nodiscard]] OrganizationalSnaResult parse_organizational_sna_result_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_result_json(const OrganizationalSnaResult&);
[[nodiscard]] RoleDiscoveryOptions parse_organizational_roles_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_organizational_workbench_options_json(const RoleDiscoveryOptions&);
[[nodiscard]] RoleDiscoveryResult parse_organizational_roles_result_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_result_json(const RoleDiscoveryResult&);
[[nodiscard]] ResourceProfilesOptions parse_organizational_profiles_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_organizational_workbench_options_json(const ResourceProfilesOptions&);
[[nodiscard]] ResourceProfilesResult parse_organizational_profiles_result_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_result_json(const ResourceProfilesResult&);
[[nodiscard]] NetworkAnalysisOptions parse_organizational_network_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_organizational_workbench_options_json(const NetworkAnalysisOptions&);
[[nodiscard]] NetworkAnalysisResult parse_organizational_network_result_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_result_json(const NetworkAnalysisResult&);
[[nodiscard]] LocalDiagnosticsOptions parse_organizational_local_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_organizational_workbench_options_json(const LocalDiagnosticsOptions&);
[[nodiscard]] LocalDiagnosticsResult parse_organizational_local_result_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_result_json(const LocalDiagnosticsResult&);
using OrganizationalWorkbenchGroups=std::map<std::string,std::vector<std::string>>;
[[nodiscard]] OrganizationalWorkbenchGroups parse_organizational_workbench_groups_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_groups_json(const OrganizationalWorkbenchGroups&);
[[nodiscard]] ResourceProfileMetric parse_organizational_workbench_metric_json(std::string_view);
[[nodiscard]] std::string serialize_organizational_workbench_metric_json(ResourceProfileMetric);
[[nodiscard]] std::string parse_organizational_workbench_name_json(std::string_view);
struct OrganizationalWorkbenchResponse {int status=200;std::string json;};
[[nodiscard]] OrganizationalWorkbenchResponse execute_organizational_workbench(const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] OrganizationalWorkbenchResponse execute_organizational_workbench_multipart(std::string_view,std::string_view,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
