#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "minera/decomposed_alignment.hpp"
#include "minera/dfg_alignment.hpp"

namespace minera {

// A raw trace is a JSON array of strings. Repeated words, empty names, NUL,
// whitespace and escaped lone UTF-16 surrogates retain their exact identity.
// Raw malformed UTF-8, non-string members and trailing input are rejected.
// Input is bounded to 32 MiB and one million JSON values (including the array).
// Its schema permits only the array and scalar strings, so nesting is bounded.
[[nodiscard]] std::vector<std::string> parse_alignment_trace_words_json(
    std::string_view input);
[[nodiscard]] std::string serialize_alignment_trace_words_json(
    const std::vector<std::string>& words);

// Complete native state: no fields are discarded when computed/complete is
// false. Finite doubles use max_digits10 and a decimal point when integral;
// nonfinite doubles use the strings "NaN", "INF" and "-INF". In particular,
// raw DFG trace fitness/best_worst_cost remain "NaN", without fabricated scores.
// Unsigned integers are exact JSON integer tokens, including the decomposed
// transition's size_t(-1) sentinel. Optional move labels are JSON null.
// All outputs are UTF-8 JSON; internal WTF-8 surrogates become \uXXXX escapes.
// Output serializers have no input transport size, depth or node limits.
[[nodiscard]] std::string serialize_dfg_alignment_trace_json(
    const DfgAlignmentResult& result);
// {dfg:[{source,target,sync,mm}],activities:[{activity,sync,mm,lm}]}
[[nodiscard]] std::string serialize_dfg_alignment_projection_json(
    const DfgAlignmentProjection& projection);
// {alignments:[complete trace records],unique_variants,projection:null|object}
[[nodiscard]] std::string serialize_dfg_alignment_log_json(
    const DfgAlignmentLogResult& result,
    const DfgAlignmentProjection* projection = nullptr);
// Native fields retain their names; moves use the field "alignment".
[[nodiscard]] std::string serialize_decomposed_alignment_trace_json(
    const DecomposedAlignmentTrace& trace);
[[nodiscard]] std::string serialize_decomposed_alignment_result_json(
    const DecomposedAlignmentResult& result);

// All actual configured option fields, including empty cost functions and
// their independent DFG supplied flags. Cost maps are JSON objects with exact
// label keys; markings/bounds retain exact native unsigned integers. "variant"
// is CLASSIC/RECOMPOS_MAXIMAL (UNKNOWN for an unrecognized native enum), with
// "variant_index" retaining the original underlying integer in every case.
[[nodiscard]] std::string serialize_dfg_alignment_options_json(
    const DfgAlignmentOptions& options);
[[nodiscard]] std::string serialize_decomposed_alignment_options_json(
    const DecomposedAlignmentOptions& options);

}  // namespace minera
