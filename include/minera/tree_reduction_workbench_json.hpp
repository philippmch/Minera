#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "minera/discovery_workbench_json.hpp"
#include "minera/process_tree_reduction.hpp"
namespace minera {
// Full ProcessTree constructor shape, without semantic validation. Each node
// requires op/op_index/activity/children; UNKNOWN retains unrecognized indices.
// Stored strings use NativeString (plain UTF8/WTF8 or hex-v1 byte carriers).
[[nodiscard]] ProcessTree parse_tree_reduction_tree_json(std::string_view);
[[nodiscard]] std::string serialize_tree_reduction_tree_json(const ProcessTree&);
// Complete existing DeclareTrace/Event/attribute schema, scoped independently:
// case_id,case_id_state_index,case_id_source_key,case_id_attribute,attributes,
// properties,events. Every nested stored string supports NativeString; omitted
// constructor fields retain defaults. Ordered duplicate attributes, typed native
// values, latent fields and all first-class states remain literal native state.
// Private scalar alias identity renews on construction, as with existing native
// input carriers; it is not an assignable stored constructor field.
[[nodiscard]] Trace parse_tree_reduction_trace_json(std::string_view);
[[nodiscard]] std::string serialize_tree_reduction_trace_json(const Trace&);
// Exactly {variant_index:int,activity_key:NativeString}. Unknown stored enum
// indices are passed to the native reducer, preserving its exception precedence.
[[nodiscard]] ProcessTreeReductionOptions parse_tree_reduction_options_json(std::string_view);
[[nodiscard]] std::string serialize_tree_reduction_options_json(const ProcessTreeReductionOptions&);
// No codec byte/node/depth limits narrow valid constructor state. HTTP retains
// its existing 32 MiB body limit. The native reducer alone owns semantics.
[[nodiscard]] DiscoveryWorkbenchResponse execute_tree_reduction_workbench(
 const std::vector<DiscoveryWorkbenchPart>& parts,
 const std::vector<DiscoveryWorkbenchPart>& query = {},
 std::string_view input_order = "provided_order",std::string_view transport_error = {});
}  // namespace minera
