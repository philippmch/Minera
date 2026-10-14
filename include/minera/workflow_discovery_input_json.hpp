#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/heuristic_miner.hpp"

namespace minera {

// Discriminates the three native Alpha graph input contracts. Sparse maps
// omit both boundaries for inference or supply both (including empty maps).
// Clean maps supply both boundaries. Graph inputs additionally declare an
// ordered, unique activity registry containing every edge/boundary label.
struct AlphaDfgDiscoveryInput {
  enum class Kind { kSparse, kClean, kGraph };
  Kind kind = Kind::kSparse;
  DfgFrequency dfg;
  std::optional<std::map<std::string, std::uint64_t>> start_activities;
  std::optional<std::map<std::string, std::uint64_t>> end_activities;
  std::vector<std::string> activities;
};

// Bare JSON uses kind=sparse|clean|graph and dfg=[{source,target,count}].
// Counts are exact JSON uint64 integers; boundary maps use exact string keys.
// Auxiliary Heuristic fields mirror HeuristicClassicDfgInput; triples use
// {source,via,target,count}, samples use {source,target,samples}, and ordered
// paths use {source,target}. Nonfinite floats use "NaN"/"INF"/"-INF" tags.
// performance_values entries use value={type:scalar|samples|summary|structured,
// value:...}; summary has mean/median/minimum/maximum/stdev/sum. Structured
// values recursively use {type:null|boolean|int64|uint64|float|string|array|object,
// value:...}, retaining numeric storage and typed nested containers.
// Duplicate decoded keys/paths, unknown fields, wrong types and overflow fail.
// Input limits are 32 MiB, 64 levels and one million JSON values. Output
// serializers retain complete finite native carriers without those caps.
[[nodiscard]] AlphaDfgDiscoveryInput parse_alpha_dfg_input_json(
    std::string_view input);
[[nodiscard]] std::string serialize_alpha_dfg_input_json(
    const AlphaDfgDiscoveryInput& input);
[[nodiscard]] HeuristicClassicDfgInput parse_heuristic_dfg_input_json(
    std::string_view input);
[[nodiscard]] std::string serialize_heuristic_dfg_input_json(
    const HeuristicClassicDfgInput& input);
[[nodiscard]] HeuristicStructuredValue parse_workflow_structured_value_json(
    std::string_view input);
[[nodiscard]] std::string serialize_workflow_structured_value_json(
    const HeuristicStructuredValue& input);
[[nodiscard]] std::string serialize_workflow_performance_value_json(
    const HeuristicPerformanceValue& input);

// Literal selector overlay. Exact keys and values remain unchanged, including
// empty strings, whitespace, Unicode, NUL and delimiter characters.
[[nodiscard]] std::map<std::string, std::string>
parse_workflow_selector_strings_json(std::string_view input);

}  // namespace minera
