#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Trace-attribute-driven hierarchical clustering variants.
enum class TraceAttributeClusteringVariant {
  kDmmLeven, kAvgLeven, kDmmVec, kAvgVec, kDfg,
  DMM_LEVEN = kDmmLeven, AVG_LEVEN = kAvgLeven, DMM_VEC = kDmmVec,
  AVG_VEC = kAvgVec, DFG = kDfg,
};

struct TraceAttributeClusteringOptions {
  std::string trace_attribute;
  TraceAttributeClusteringVariant variant = TraceAttributeClusteringVariant::kDmmLeven;
  double alpha = 0.5;
  double percent = 1.0;
  std::size_t max_groups = 10000;
  std::size_t max_output_nodes = 20000;
};

struct TraceAttributeClusteringNode {
  std::string name;
  std::vector<TraceAttributeClusteringNode> children;
  double distance = 0.0;
  std::size_t size = 0;
  bool leaf = false;
};

struct TraceAttributeClusteringDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0;
  std::size_t group_count = 0;
  std::size_t node_count = 0;
  std::string message;
};

struct TraceAttributeClusteringResult {
  TraceAttributeClusteringNode tree;
  std::vector<std::string> leaf_names;
  TraceAttributeClusteringDiagnostics diagnostics;
};

[[nodiscard]] TraceAttributeClusteringResult discover_trace_attribute_clustering(
    const EventLog& log,
    const TraceAttributeClusteringOptions& options = {});

[[nodiscard]] inline TraceAttributeClusteringResult cluster_traces_by_attribute(
    const EventLog& log, const TraceAttributeClusteringOptions& options = {}) {
  return discover_trace_attribute_clustering(log, options);
}

[[nodiscard]] std::string trace_attribute_clustering_json(
    const TraceAttributeClusteringResult& result);

}  // namespace minera
