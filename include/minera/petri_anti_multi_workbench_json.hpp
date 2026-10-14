#pragma once

#include <string>

#include "minera/pnml.hpp"
#include "minera/request_params.hpp"

namespace minera {

// Scoped original-input parsers; existing cached request contracts are intact.
// Scalars retain strtod syntax (including hexadecimal and subnormal values)
// and literal activity_attribute strings. Exponent/epsilon and the effective
// search-bound domains are validated by the native algorithm, not this codec.
// JSON marking arrays contain exact unsigned size_t integer tokens, including
// []; their presence cannot coexist with the corresponding legacy comma field.
// JSON input is bounded to 32 MiB, 64 levels and one million values. Duplicate
// decoded object keys, malformed UTF/JSON and integer overflow are rejected.
[[nodiscard]] PetriAntiAlignmentOptions parse_petri_anti_workbench_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] PetriMultiAlignmentOptions parse_petri_multi_workbench_params(
    const FieldLookup& field, const FieldPresence& present);

// All eight/seven native fields plus variant_index. Canonical variant is
// VERSION_DISCOUNTED_A_STAR, or UNKNOWN for an unrecognized stored enum.
[[nodiscard]] std::string serialize_petri_anti_workbench_options_json(
    const PetriAntiAlignmentOptions& options);
[[nodiscard]] std::string serialize_petri_multi_workbench_options_json(
    const PetriMultiAlignmentOptions& options);

// Exactly the six stored result fields, including every diagnostic field.
// No native variant, completion/time flags or per-trace state are invented.
[[nodiscard]] std::string serialize_petri_anti_workbench_result_json(
    const PetriAntiAlignmentResult& result);
[[nodiscard]] std::string serialize_petri_multi_workbench_result_json(
    const PetriMultiAlignmentResult& result);

// Complete PnmlDocument: frozen51 net snapshot, raw_source/raw_sink, arbitrary
// exact TokenInteger markings, all ID/key vectors, optional ordered complete
// arc occurrences and optional full stochastic records with raw enum indexes.
// Null and engaged empty optional vectors/maps remain distinct. No document
// export/inference validation or mutation is invoked by this serializer.
[[nodiscard]] std::string serialize_petri_workbench_document_json(
    const PnmlDocument& document);

// All output doubles use max_digits10/-0.0 or "NaN"/"INF"/"-INF" tags;
// exact integers are JSON integer tokens. UTF/WTF-8 strings retain identity,
// with lone surrogate code points escaped. Output has no input transport caps.

}  // namespace minera
