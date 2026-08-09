#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "minera/directly_follows_graph.hpp"

namespace minera {

// A compact, deterministic DFG view used by the connected filtering
// variations. Boundary frequencies are kept separately because they are not
// represented by ordinary directly-follows edges.
struct DfgFilterData {
  std::map<std::pair<std::string, std::string>, std::uint64_t> edges;
  std::map<std::string, std::uint64_t> start_activities;
  std::map<std::string, std::uint64_t> end_activities;
  std::map<std::string, std::uint64_t> activities;
};

[[nodiscard]] DfgFilterData make_dfg_filter_data(
    const DirectlyFollowsGraph& graph);

[[nodiscard]] DfgFilterData filter_dfg_on_activities_percentage(
    const DfgFilterData& data, double percentage);

[[nodiscard]] DfgFilterData filter_dfg_on_paths_percentage(
    const DfgFilterData& data, double percentage,
    bool keep_all_activities = false);

[[nodiscard]] DfgFilterData filter_dfg_keep_connected(
    const DfgFilterData& data, double dependency_threshold,
    bool keep_all_activities = false);

// Removes low-support directly-following edges using the strongest incoming
// or outgoing edge touching each endpoint as its local reference. An edge is
// retained when its count is at least
// min(max_endpoint_source, max_endpoint_target) * noise_threshold. Explicit
// most-common paths are retained even when they fall below that boundary.
// Activity, start, and end tables are preserved; only ordinary edges can be
// cleaned.
[[nodiscard]] DfgFilterData clean_dfg_based_on_noise_threshold(
    const DfgFilterData& data, double noise_threshold,
    const std::set<std::pair<std::string, std::string>>& most_common_paths =
        {});

// Short spelling retained for callers that use the classic filter name.
[[nodiscard]] inline DfgFilterData clean_dfg_based_on_noise_thresh(
    const DfgFilterData& data, double noise_threshold,
    const std::set<std::pair<std::string, std::string>>& most_common_paths =
        {}) {
  return clean_dfg_based_on_noise_threshold(data, noise_threshold,
                                             most_common_paths);
}

[[nodiscard]] DfgFilterData filter_dfg_to_activity(
    const DfgFilterData& data, const std::string& activity);

[[nodiscard]] DfgFilterData filter_dfg_from_activity(
    const DfgFilterData& data, const std::string& activity);

[[nodiscard]] DfgFilterData filter_dfg_contain_activity(
    const DfgFilterData& data, const std::string& activity);

}  // namespace minera
