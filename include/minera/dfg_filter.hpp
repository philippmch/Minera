#pragma once

#include <cstdint>
#include <map>
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

[[nodiscard]] DfgFilterData filter_dfg_to_activity(
    const DfgFilterData& data, const std::string& activity);

[[nodiscard]] DfgFilterData filter_dfg_from_activity(
    const DfgFilterData& data, const std::string& activity);

[[nodiscard]] DfgFilterData filter_dfg_contain_activity(
    const DfgFilterData& data, const std::string& activity);

}  // namespace minera
