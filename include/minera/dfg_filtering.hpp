#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/filtering_common.hpp"

namespace minera {

// algo.filtering.dfg.dfg_filtering over insertion-ordered DFG dictionaries:
// the directly-follows counts, start and end activity counts and activity
// counts. Results keep the reference's dictionary order. Activity names never
// equal the reference's reserved "_S_START_"/"_S_END_" node names; public
// failures raise FilteringCommonError with the Python class and message.
using DfgFilteringEdge = std::pair<std::string, std::string>;
using DfgFilteringEdges = std::vector<std::pair<DfgFilteringEdge, std::int64_t>>;
using DfgFilteringCounts = std::vector<std::pair<std::string, std::int64_t>>;

struct DfgFilteringData {
  DfgFilteringEdges dfg;
  DfgFilteringCounts start_activities;
  DfgFilteringCounts end_activities;
  DfgFilteringCounts activities;
  friend bool operator==(const DfgFilteringData&, const DfgFilteringData&) = default;
};

inline constexpr double kDfgFilteringDefaultNoiseThreshDf = kFilteringDefaultNoiseThreshDf;
inline constexpr const char* kDfgFilteringStartNode = "_S_START_";
inline constexpr const char* kDfgFilteringEndNode = "_S_END_";

// Keeps the ceil((|activities| - 1) * percentage) + 1 most frequent
// activities (count, then name, descending); the others are tried for
// removal from the least frequent up and dropped when every kept activity
// stays reachable from the start and can still reach the end. Activities
// that end up disconnected are dropped last. Inputs with at most one activity
// or one edge are returned unchanged.
[[nodiscard]] DfgFilteringData filter_dfg_on_activities_percentage(const DfgFilteringData& data, double percentage);

// The same over edges, start and end arcs included (count, then edge,
// descending); keep_all_activities requires every activity and boundary
// activity to stay connected.
[[nodiscard]] DfgFilteringData filter_dfg_on_paths_percentage(const DfgFilteringData& data, double percentage,
                                                              bool keep_all_activities = false);

// Arcs scored by (c - c_inverse) / (c + c_inverse + 1), boundary arcs by 1;
// arcs scoring below the threshold are tried for removal from the lowest up.
[[nodiscard]] DfgFilteringData filter_dfg_keep_connected(const DfgFilteringData& data, double threshold,
                                                         bool keep_all_activities = false);

// Drops every edge outside most_common_paths whose count is below
// min(max_a * threshold, max_b * threshold), where max_x is the largest count
// of an edge touching activity x, or zero when that is larger. An edge
// activity missing from `activities` raises KeyError.
[[nodiscard]] DfgFilteringEdges clean_dfg_based_on_noise_thresh(
    const DfgFilteringEdges& dfg, const std::vector<std::string>& activities, double noise_threshold,
    const std::optional<std::vector<DfgFilteringEdge>>& most_common_paths = std::nullopt);

[[nodiscard]] DfgFilteringData filter_dfg_to_activity(const DfgFilteringData& data, const std::string& target_activity);
[[nodiscard]] DfgFilteringData filter_dfg_from_activity(const DfgFilteringData& data,
                                                        const std::string& source_activity);
[[nodiscard]] DfgFilteringData filter_dfg_contain_activity(const DfgFilteringData& data, const std::string& activity);

struct DfgFilteringAdjacency {
  std::map<std::string, std::set<std::string>> adjacency;
  std::map<std::string, std::set<std::string>> reverse_adjacency;
  std::string start_node = kDfgFilteringStartNode;
  std::string end_node = kDfgFilteringEndNode;
};
[[nodiscard]] DfgFilteringAdjacency build_adjacency_structures(const DfgFilteringEdges& dfg,
                                                               const DfgFilteringCounts& start_activities,
                                                               const DfgFilteringCounts& end_activities);

// Successor lists by node; a visited node without an entry raises KeyError.
using DfgFilteringSuccessors = std::map<std::string, std::vector<std::string>>;
[[nodiscard]] std::set<std::string> bfs_reachable(const std::string& start, const DfgFilteringSuccessors& adjacency);

// Removes every activity that is not both reachable from start_node over
// adjacency and able to reach end_node over reverse_adjacency (the two node
// names count as unreachable). The activity, start and end dictionaries lose
// those activities in place, like the reference's; the returned data carries
// them with the edges between remaining activities.
[[nodiscard]] DfgFilteringData remove_unreachable_nodes(DfgFilteringData& data, const DfgFilteringSuccessors& adjacency,
                                                        const DfgFilteringSuccessors& reverse_adjacency,
                                                        const std::string& start_node, const std::string& end_node);

// The networkx DiGraph with a fresh uuid4 start and end node: nodes in
// insertion order (start, end, activities, then edge, start-arc and end-arc
// endpoints) and edges by node, successors in insertion order.
struct DfgFilteringGraph {
  std::vector<std::string> nodes;
  std::vector<DfgFilteringEdge> edges;
  std::string start_node;
  std::string end_node;
};
[[nodiscard]] DfgFilteringGraph generate_nx_graph_from_dfg(const DfgFilteringData& data);

class DirectlyFollowsGraph;

// The DFG export filters of GET /api/dfg, minera_dfg and the frontend's DFG
// export, applied in this order through the functions above: noise cleaning
// (edges only), activities percentage, paths percentage, keep connected, to,
// from and contain activity. Any step may raise FilteringCommonError.
struct DfgFilteringRequest {
  std::optional<double> noise_threshold;
  std::optional<std::vector<DfgFilteringEdge>> most_common_paths;
  std::optional<double> activities_percentage;
  std::optional<double> paths_percentage;
  std::optional<double> keep_connected;
  bool keep_all_activities = false;
  std::optional<std::string> to_activity;
  std::optional<std::string> from_activity;
  std::optional<std::string> contain_activity;
};

// A mined log's DFG with its real activity counts, the inputs a reference
// user passes as discover_dfg(log) and get_event_attribute_values(log,
// activity): edges, start and end activities and activities in the graph's
// order, zero counts omitted.
[[nodiscard]] DfgFilteringData dfg_filtering_data_from_graph(const DirectlyFollowsGraph& graph);

// A DFG read from a .dfg file, which carries no activity counts. Each
// activity that occurs on an edge counts max(sum of incoming edge counts, sum
// of outgoing edge counts); activities occurring only as start or end
// activities get no entry. This is the count the reference's public
// filter_dfg_activities_percentage / filter_dfg_paths_percentage wrappers use
// for (dfg, start, end) alone (black-box: 3,000 random DFGs). Activities are
// ordered by first occurrence over the edges (source, then target).
[[nodiscard]] DfgFilteringData dfg_filtering_data_from_dfg(const DfgCleanAnalysis& dfg);

[[nodiscard]] DfgFilteringData apply_dfg_filtering_request(DfgFilteringData data, const DfgFilteringRequest& request);

// Back to the .dfg payload (edges, start and end activities).
[[nodiscard]] DfgCleanAnalysis to_dfg_clean_analysis(const DfgFilteringData& data);

}  // namespace minera
