#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

enum class NetworkAnalysisVariant {
  kDataframe,
  DATAFRAME = kDataframe,
};

enum class NetworkAnalysisEdgeReference { kOut, kIn };

struct NetworkAnalysisOptions {
  NetworkAnalysisVariant variant = NetworkAnalysisVariant::kDataframe;
  std::string sorting_attribute = "time:timestamp";
  std::string timestamp_attribute = "time:timestamp";
  std::string source_node_attribute = "org:resource";
  std::string target_node_attribute = "org:resource";
  std::string edge_attribute = "concept:name";
  NetworkAnalysisEdgeReference edge_reference = NetworkAnalysisEdgeReference::kOut;
  std::string out_relation_attribute = "case:concept:name";
  std::string in_relation_attribute = "case:concept:name";
  bool include_performance = false;
  bool business_hours = false;
  std::vector<BusinessHourSlot> business_hour_slots;
  std::size_t max_rows = 10000;
  std::size_t max_values = 100000;
};

struct NetworkAnalysisEdge {
  std::string source;
  std::string target;
  std::string activity;
  std::uint64_t count = 0;
  std::vector<double> performance;
};

struct NetworkAnalysisDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0;
  std::size_t event_count = 0;
  std::size_t link_count = 0;
  std::size_t row_count = 0;
  std::size_t performance_value_count = 0;
  std::string message;
};

struct NetworkAnalysisResult {
  NetworkAnalysisVariant variant = NetworkAnalysisVariant::kDataframe;
  std::vector<NetworkAnalysisEdge> edges;
  NetworkAnalysisDiagnostics diagnostics;
};

[[nodiscard]] NetworkAnalysisResult discover_network_analysis(
    const EventLog& log, const NetworkAnalysisOptions& options = {});

[[nodiscard]] inline NetworkAnalysisResult mine_network_analysis(
    const EventLog& log, const NetworkAnalysisOptions& options = {}) {
  return discover_network_analysis(log, options);
}

}  // namespace minera
