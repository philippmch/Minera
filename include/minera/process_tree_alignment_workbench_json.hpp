#pragma once

#include <string>
#include <string_view>

#include "minera/process_tree_alignment.hpp"

namespace minera {

// Complete constructor state, matching serialize_inductive_process_tree_json:
// {activities:string[],root:{op,op_index,activity,children:[node]}}.
// Every field is required. The canonical op must match its native enum index;
// UNKNOWN represents any unrecognized index in the enum's signed int range.
// Duplicate/unused activity labels, latent indices and all child shapes are
// preserved. Semantic alignment/export validation belongs to those consumers.
// Input is bounded to 32 MiB, 64 JSON nesting levels and one million values.
// Strings preserve controls, Unicode and escaped lone UTF-16 surrogates;
// malformed raw UTF-8, duplicate decoded keys and trailing input are rejected.
[[nodiscard]] ProcessTree parse_process_tree_alignment_input_json(
    std::string_view input);
[[nodiscard]] std::string serialize_process_tree_alignment_input_json(
    const ProcessTree& tree);

// All native stored fields remain present for incomplete/uncomputed results.
// variant is SEARCH_GRAPH_PT/DYNAMIC_PROGRAMMING/MILP/APPROXIMATED_ORIGINAL
// (UNKNOWN for unrecognized enums); variant_index retains its underlying int.
// Moves retain type sync/log/model/UNKNOWN, type_index and all three activity
// strings, including unused empty sides. All size_t/uint64 values are exact
// integer tokens. Finite doubles use max_digits10, including -0.0; nonfinite
// doubles use "NaN", "INF" and "-INF". Output has no input transport limits.
// Internal WTF-8 surrogate code points are written as valid JSON escapes.
[[nodiscard]] std::string serialize_process_tree_alignment_result_json(
    const ProcessTreeAlignmentResult& result);
// All twelve actual native option fields, plus variant_index.
[[nodiscard]] std::string serialize_process_tree_alignment_options_json(
    const ProcessTreeAlignmentOptions& options);

}  // namespace minera
