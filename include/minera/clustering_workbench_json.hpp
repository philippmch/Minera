#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/clustering.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// These scoped codecs preserve complete stored public constructor values.
// Selected public clustering functions, including result serializers, own all
// semantic validation. Canonical storage output is never the native text view.
[[nodiscard]] TraceAttributeClusteringOptions parse_clustering_hierarchy_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] ProfileClusteringOptions parse_clustering_profile_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_clustering_workbench_options_json(const TraceAttributeClusteringOptions&);
[[nodiscard]] std::string serialize_clustering_workbench_options_json(const ProfileClusteringOptions&);
[[nodiscard]] TraceAttributeClusteringResult parse_clustering_workbench_hierarchy_result_json(std::string_view);
[[nodiscard]] ProfileClusteringResult parse_clustering_workbench_profile_result_json(std::string_view);
[[nodiscard]] std::string serialize_clustering_workbench_result_json(const TraceAttributeClusteringResult&);
[[nodiscard]] std::string serialize_clustering_workbench_result_json(const ProfileClusteringResult&);
[[nodiscard]] std::string serialize_clustering_workbench_groups_json(const std::vector<TraceAttributeClusteringGroup>&);
struct ClusteringWorkbenchResponse { int status=200; std::string json; };
[[nodiscard]] ClusteringWorkbenchResponse execute_clustering_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] ClusteringWorkbenchResponse execute_clustering_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
