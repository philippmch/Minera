#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "minera/dfg_playout.hpp"

namespace minera {

// Declared native maps; labels and explicit zero counts retain their identity.
// A null performance pointer is distinct from an engaged, empty map.
struct DfgPlayoutInput {
  DfgFrequency frequency;
  DfgActivityCounts start_activities;
  DfgActivityCounts end_activities;
  std::optional<DfgPerformance> performance;
};

// Exact uint64 integer tokens, IEEE double tags (NaN/INF/-INF), and escaped
// lone UTF-16 surrogates are preserved. Unknown/duplicate fields and duplicate
// map keys are rejected. Omitted maps are empty, performance is null; record
// labels are required, while omitted values use their native zero defaults.
// Transport is bounded to 32 MiB, one million JSON values, and 64 levels.
// Native playout, rather than this carrier, validates simulation semantics.
[[nodiscard]] DfgPlayoutInput parse_dfg_playout_input_json(std::string_view input);
// Complete stored state, with no transport/output limits or omitted fields.
[[nodiscard]] std::string serialize_dfg_playout_input_json(const DfgPlayoutInput& input);

}  // namespace minera
