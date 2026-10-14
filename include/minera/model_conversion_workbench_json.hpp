#pragma once

#include <string>

#include "minera/petri_net_reduction.hpp"
#include "minera/powl.hpp"
#include "minera/request_params.hpp"

namespace minera {

// Every stored PowlNode getter is represented recursively:
// {kind,kind_index,label,frequency:{minimum,maximum:null|int},children,order}.
// Kinds use TRANSITION/SILENT/XOR/LOOP/STRICT_PARTIAL_ORDER (or UNKNOWN).
// The order array contains exact [source,target] pairs in stored closure order.
// Empty transition labels remain visible; SILENT is a separate native kind.
// Serialization does not reparse or validate the native POWL text grammar.
[[nodiscard]] std::string serialize_model_conversion_powl_json(
    const PowlNode& node);

// Scoped reduction input preserves the full signed int64 marking domain.
// Each legacy petri_reduction_{initial,final}_marking comma field conflicts
// by presence with its corresponding *_marking_json array, including [].
// Empty JSON arrays retain the native unspecified-vector meaning. Marking
// width and reduction semantics remain the native algorithm's responsibility.
// Boolean fields reuse the existing true/false/1/0 request contract, including
// petri_reduction_preserve_join_bridges. JSON integer grammar is exact: no
// fractions, exponent notation, coercion or overflowing signed integers.
// Inputs are bounded to 32 MiB and one million values including the array;
// only one array level and scalar integers are permitted by this schema.
[[nodiscard]] PetriNetReductionOptions parse_model_reduction_workbench_params(
    const FieldLookup& field, const FieldPresence& present);

// All five native option fields, and all native result fields respectively.
// Results use the complete executable-net snapshot and preserve signed
// markings, both raw size_t mapping arrays (including removed sentinels),
// and changed. No completion flags or model identity assumptions are added.
[[nodiscard]] std::string serialize_model_reduction_options_json(
    const PetriNetReductionOptions& options);
[[nodiscard]] std::string serialize_model_reduction_result_json(
    const PetriNetReductionResult& result);

// Output has no input size/depth/value caps. Exact UTF/WTF-8 labels use valid
// JSON escaping, including controls and escaped lone UTF-16 surrogates.
// The reused complete-net writer retains exact native numeric storage.

}  // namespace minera
