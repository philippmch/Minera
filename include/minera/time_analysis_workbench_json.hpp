#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/discovery_workbench_json.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/cycle_time.hpp"
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
namespace minera {
// Full structural storage codecs. Actual public consumers own semantic validation.
[[nodiscard]] PassedTimeOptions parse_time_analysis_workbench_passed_time_options_json(std::string_view bytes,std::set<std::string>* supplied=nullptr);
[[nodiscard]] PassedTimeResult parse_time_analysis_workbench_passed_time_result_json(std::string_view bytes);
[[nodiscard]] std::string serialize_time_analysis_workbench_options_json(const PassedTimeOptions& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(const PassedTimeResult& value);
[[nodiscard]] ServiceTimeOptions parse_time_analysis_workbench_service_time_options_json(std::string_view bytes,std::set<std::string>* supplied=nullptr);
[[nodiscard]] ServiceTimeResult parse_time_analysis_workbench_service_time_result_json(std::string_view bytes);
[[nodiscard]] std::string serialize_time_analysis_workbench_options_json(const ServiceTimeOptions& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(const ServiceTimeResult& value);
[[nodiscard]] CycleTimeOptions parse_time_analysis_workbench_cycle_time_options_json(std::string_view bytes,std::set<std::string>* supplied=nullptr);
[[nodiscard]] CycleTimeResult parse_time_analysis_workbench_cycle_time_result_json(std::string_view bytes);
[[nodiscard]] std::string serialize_time_analysis_workbench_options_json(const CycleTimeOptions& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(const CycleTimeResult& value);
[[nodiscard]] CaseArrivalOptions parse_time_analysis_workbench_case_arrival_options_json(std::string_view bytes,std::set<std::string>* supplied=nullptr);
[[nodiscard]] CaseArrivalResult parse_time_analysis_workbench_case_arrival_result_json(std::string_view bytes);
[[nodiscard]] std::string serialize_time_analysis_workbench_options_json(const CaseArrivalOptions& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(const CaseArrivalResult& value);
[[nodiscard]] CaseStatisticsOptions parse_time_analysis_workbench_case_statistics_options_json(std::string_view bytes,std::set<std::string>* supplied=nullptr);
[[nodiscard]] CaseStatisticsResult parse_time_analysis_workbench_case_statistics_result_json(std::string_view bytes);
[[nodiscard]] std::string serialize_time_analysis_workbench_options_json(const CaseStatisticsOptions& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(const CaseStatisticsResult& value);
[[nodiscard]] std::string serialize_time_analysis_workbench_result_json(double);
struct TimeAnalysisWorkbenchResponse{int status=200;std::string json;};
[[nodiscard]] TimeAnalysisWorkbenchResponse execute_time_analysis_workbench(const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] TimeAnalysisWorkbenchResponse execute_time_analysis_workbench_multipart(std::string_view,std::string_view,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
