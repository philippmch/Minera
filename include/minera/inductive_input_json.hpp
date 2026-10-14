#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "minera/inductive_miner.hpp"

namespace minera {

// Bare ordered [{activities:[string],count:uint64}]. Repeated records/words,
// empty words and zero counts are native carriers, not deduplicated or expanded.
[[nodiscard]] InductiveMinerVariantLog parse_inductive_variant_log_json(
    std::string_view input);
[[nodiscard]] std::string serialize_inductive_variant_log_json(
    const InductiveMinerVariantLog& variants);

// Exact public DirectlyFollowsGraph::from_frequencies constructor carrier.
// All four fields are required; indexes and all counts are JSON unsigned
// integer tokens. Registry and edge order, zero entries and explicit empties
// are retained. The existing native constructor validates registry uniqueness,
// edge uniqueness/index bounds and boundary-vector widths.
struct InductiveDfgInput {
  std::vector<std::string> activities;
  std::vector<DirectlyFollowsEdge> edges;
  std::vector<std::uint64_t> start_counts;
  std::vector<std::uint64_t> end_counts;
};
[[nodiscard]] InductiveDfgInput parse_inductive_dfg_input_json(
    std::string_view input);
[[nodiscard]] std::string serialize_inductive_dfg_input_json(
    const InductiveDfgInput& input);

// Complete public native observables: constructor fields plus activity_counts,
// rework_case_counts and ordered variants. Declared frequencies do not invent
// occurrences, rework or observed variants, so constructor graphs retain zeros.
[[nodiscard]] std::string serialize_inductive_native_dfg_json(
    const DirectlyFollowsGraph& graph);

// Complete stored ProcessTree: {activities:[string],root:{op,op_index,activity,
// children:[node]}}. Every node's raw activity index is retained even on tau or
// operator nodes, along with all children and unused activity-registry entries.
// No native display-name/optional activity/node-pool fields exist in this type.
[[nodiscard]] std::string serialize_inductive_process_tree_json(
    const ProcessTree& tree);

// All configured native fields, canonical variant IM/IMF/IMD and its original
// variant_index. Unknown native enum values retain UNKNOWN plus their integer.
// Finite floats use max_digits10; nonfinite values use NaN/INF/-INF string tags.
[[nodiscard]] std::string serialize_inductive_options_json(
    const InductiveMinerOptions& options);

// Inputs reject duplicate decoded object keys, unknown/missing/wrong-type
// fields, malformed UTF-8 and integer overflow; 32 MiB/64 levels/1M JSON values
// bound input only. Escaped lone surrogates retain native WTF-8 internally;
// every output emits valid UTF-8 JSON with those code points escaped as \uXXXX.
// Output serialization never reapplies input size/depth/node limits.
}  // namespace minera
