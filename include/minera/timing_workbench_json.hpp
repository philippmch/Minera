#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// Full structural storage codecs; the selected public consumer owns semantics.
[[nodiscard]] PerformanceSpectrumOptions parse_timing_workbench_spectrum_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] BatchDiscoveryOptions parse_timing_workbench_batch_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] PerformanceSpectrum parse_timing_workbench_spectrum_json(std::string_view);
[[nodiscard]] BatchDiscovery parse_timing_workbench_batches_json(std::string_view);
[[nodiscard]] std::string serialize_timing_workbench_options_json(const PerformanceSpectrumOptions&);
[[nodiscard]] std::string serialize_timing_workbench_options_json(const BatchDiscoveryOptions&);
[[nodiscard]] std::string serialize_timing_workbench_result_json(const PerformanceSpectrum&);
[[nodiscard]] std::string serialize_timing_workbench_result_json(const BatchDiscovery&);
struct TimingWorkbenchResponse {int status=200;std::string json;};
[[nodiscard]] TimingWorkbenchResponse execute_timing_workbench(const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] TimingWorkbenchResponse execute_timing_workbench_multipart(std::string_view,std::string_view,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
