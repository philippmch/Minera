#pragma once

#include <cstddef>
#include <cstdint>
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

// Deterministic native equivalent of profile-based trace clustering. The
// default representation contains activity and adjacent-activity one-hot
// features; explicit attribute lists add trace/event profile dimensions.
struct ProfileClusteringOptions {
  std::size_t cluster_count = 2;
  std::size_t max_iterations = 100;
  std::size_t max_traces = 100000;
  std::size_t max_features = 10000;
  std::uint64_t seed = 0;
  double tolerance = 1e-6;
  bool include_activity_representation = true;
  bool include_succession_representation = true;
  std::vector<std::string> trace_string_attributes;
  std::vector<std::string> event_string_attributes;
  std::vector<std::string> trace_numeric_attributes;
  std::vector<std::string> event_numeric_attributes;
  std::vector<std::string> feature_names;
};

struct ProfileClusteringDiagnostics {
  bool computed = false;
  bool converged = false;
  std::size_t trace_count = 0;
  std::size_t feature_count = 0;
  std::size_t cluster_count = 0;
  std::size_t iterations = 0;
  std::string message;
};

struct ProfileClusteringResult {
  std::vector<int> labels;
  std::vector<std::vector<std::size_t>> clusters;
  std::vector<std::vector<double>> centroids;
  std::vector<std::string> feature_names;
  double inertia = 0.0;
  ProfileClusteringDiagnostics diagnostics;
};

[[nodiscard]] ProfileClusteringResult discover_profile_clustering(
    const EventLog& log, const ProfileClusteringOptions& options = {});

[[nodiscard]] std::string profile_clustering_json(
    const ProfileClusteringResult& result);

}  // namespace minera
