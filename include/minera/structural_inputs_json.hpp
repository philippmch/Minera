#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "minera/discovery_workbench_json.hpp"
#include "minera/trie.hpp"
#include "minera/interval_tree.hpp"
namespace minera {
// Structural-only native constructor codecs; selected native functions own semantics.
[[nodiscard]] TrieModel parse_structural_input_trie_json(std::string_view);
[[nodiscard]] std::vector<OpenPathInterval> parse_structural_input_intervals_json(std::string_view);
[[nodiscard]] IntervalTreeQueryOptions parse_structural_input_query_options_json(std::string_view);
[[nodiscard]] std::string serialize_structural_input_json(const TrieModel&);
[[nodiscard]] std::string serialize_structural_input_json(const std::vector<OpenPathInterval>&);
[[nodiscard]] std::string serialize_structural_input_json(const IntervalTreeQueryOptions&);
[[nodiscard]] std::string serialize_structural_input_json(const IntervalTreeQueryResult&);
struct StructuralInputsResponse {int status=200;std::string json;};
[[nodiscard]] StructuralInputsResponse execute_structural_inputs(const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] StructuralInputsResponse execute_structural_inputs_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query={},bool complete=true);
} // namespace minera
