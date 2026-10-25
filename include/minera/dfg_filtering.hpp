#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

}  // namespace minera
