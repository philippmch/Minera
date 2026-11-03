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
  std::string activity_attribute = "concept:name";
  TraceAttributeClusteringVariant variant = TraceAttributeClusteringVariant::kDmmLeven;
  double alpha = 0.5;
  double percent = 1.0;
  std::size_t max_groups = 10000;
  std::size_t max_output_nodes = 20000;
  // The reference DFG selector names a module rather than a callable and
  // raises TypeError for every input, so kDfg raises unless this Minera
  // extension enables the native DFG distance.
  bool dfg_extension = false;
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
  // Condensed pairwise group distances in group order (0-1, 0-2, ..., 1-2, ...).
  std::vector<double> distances;
  TraceAttributeClusteringDiagnostics diagnostics;
};

// Source trace positions, independent of possibly repeated case identifiers.
// Shares the discovery algorithm's exact selector/alias and first-seen order.
struct TraceAttributeClusteringGroup {
  std::string name;
  std::vector<std::size_t> trace_indices;
};

[[nodiscard]] std::vector<TraceAttributeClusteringGroup>
trace_attribute_clustering_groups(const EventLog& log,
                                 const std::string& trace_attribute);

[[nodiscard]] TraceAttributeClusteringResult discover_trace_attribute_clustering(
    const EventLog& log,
    const TraceAttributeClusteringOptions& options = {});

[[nodiscard]] inline TraceAttributeClusteringResult cluster_traces_by_attribute(
    const EventLog& log, const TraceAttributeClusteringOptions& options = {}) {
  return discover_trace_attribute_clustering(log, options);
}

[[nodiscard]] std::string trace_attribute_clustering_json(
    const TraceAttributeClusteringResult& result);

// Native profile-based trace clustering (a Minera extension: its own one-hot
// activity/adjacent-activity profiles and seeded k-means). The server and
// CLI run reference_profile_clustering_result below unless the caller asks
// for this engine explicitly (profile_engine=native).
enum class ProfileClusteringVariant {
  kSklearnProfiles,
  SKLEARN_PROFILES = kSklearnProfiles,
};

struct ProfileClusteringOptions {
  ProfileClusteringVariant variant = ProfileClusteringVariant::kSklearnProfiles;
  std::string activity_attribute = "concept:name";
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
  ProfileClusteringVariant variant = ProfileClusteringVariant::kSklearnProfiles;
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

// The public SKLEARN_PROFILES default: the trace rows of
// trace_based_log_features (default parameters) clustered by k-means on
// mean-centred features, with k-means++ seeding (2 + floor(ln k) local trials)
// drawn from a Mersenne Twister seeded like numpy's legacy RandomState(seed),
// one initialisation, and Lloyd iterations that stop when labels repeat or the
// squared centre shift falls to tolerance times the mean feature variance.
struct ReferenceProfileClusteringOptions {
  std::size_t cluster_count = 2;
  std::uint32_t seed = 0;
  std::size_t max_iterations = 300;
  double tolerance = 1e-4;
};

struct ReferenceKMeansResult {
  std::vector<std::size_t> labels;
  // Lloyd iterations run, and whether they stopped on repeated labels or the
  // tolerance rather than at max_iterations.
  std::size_t iterations = 0;
  bool converged = false;
  // Some decision (a nearest centre, a seeding draw or candidate, the stop
  // test) fell within rounding distance of a tie. The reference then decides
  // by its BLAS kernel's rounding, which varies across machines.
  bool tie_dependent = false;
};

struct ReferenceProfileClusteringResult {
  // Trace indices per k-means label in ascending label order, empty labels
  // omitted.
  std::vector<std::vector<std::size_t>> clusters;
  bool tie_dependent = false;
};

// Throws std::invalid_argument when the log has fewer traces than clusters,
// where the reference raises ValueError.
[[nodiscard]] ReferenceProfileClusteringResult reference_profile_clustering(
    const EventLog& log, const ReferenceProfileClusteringOptions& options = {});

// The SKLEARN_PROFILES pipeline of the reference (what the server and CLI run
// by default) in the native result shape. The options' representation and
// attribute flags are the reference parameters it hands to TRACE_BASED
// log-to-features (include_activity_representation:
// enable_activity_def_representation, include_succession_representation:
// enable_succ_def_representation, trace/event string/numeric attributes:
// str_tr_attr / str_ev_attr / num_tr_attr / num_ev_attr, feature_names,
// activity_attribute: the activity key); max_traces and max_features are
// Minera safety bounds. `kmeans` is the clusterer: the reference default is
// KMeans(n_clusters=2, random_state=0) with scikit-learn's max_iter=300 and
// tol=1e-4; other values stand for that KMeans passed as sklearn_clusterer.
// clusters are the reference sublogs (ascending label, empty omitted),
// labels index them, centroids are the cluster means and inertia the summed
// squared distances to them. Throws std::invalid_argument where the
// reference raises (fewer traces than clusters, non-finite features, missing
// numeric attributes).
[[nodiscard]] ProfileClusteringResult reference_profile_clustering_result(
    const EventLog& log, const ProfileClusteringOptions& options,
    const ReferenceProfileClusteringOptions& kmeans = {});

// The k-means labels behind reference_profile_clustering for explicit rows.
[[nodiscard]] ReferenceKMeansResult reference_kmeans_labels(
    std::vector<std::vector<double>> rows, const ReferenceProfileClusteringOptions& options = {});

}  // namespace minera
