#pragma once

#include <set>
#include <string>
#include <string_view>

#include "minera/monte_carlo.hpp"

namespace minera {

// The object accepts the 27 native MonteCarloOptions field names. Omitted
// fields retain native defaults; empty vectors and null/engaged-empty maps
// remain distinct. Unknown/duplicate fields and type/coercion errors reject.
// Integers are exact JSON integer tokens in their native storage domains.
// Double values accept numeric tokens or "NaN", "INF", and "-INF" tags.
// Native simulation/fitting owns semantic validation, including whether a
// stored duration parameter or name vector is actually used.
//
// Enums accept canonical names or {"name":NAME,"index":INT}; output always
// emits this object so an unknown native enum value remains inspectable.
// Known names must agree with their index; unknown indexes use "UNKNOWN".
// Duration names: deterministic/exponential/constant0/immediate/normal/
// uniform/lognormal/gamma. Replay names: forward/backwards. Timing names:
// timed/immediate. force_distribution also accepts null.
//
// resources_per_place is an object with canonical decimal size_t keys and
// size_t values. stochastic_map is null or {"transitions":[native records]}.
// Durations/map records accept their native field names with native defaults.
// Strings preserve literal controls, Unicode and escaped lone surrogates.
// Input limits are 32 MiB, one million JSON values, and 64 container levels;
// serialization is not subject to input limits.
[[nodiscard]] MonteCarloOptions parse_monte_carlo_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);

[[nodiscard]] std::string serialize_monte_carlo_workbench_options_json(
    const MonteCarloOptions& options);

// Every stored result field is included. log uses the complete active-source
// native-state/legacy-view carrier, not a generated-log count summary. All
// diagnostics and ordered interval records retain exact integer and IEEE
// storage, without imposing new native-result validation at serialization.
[[nodiscard]] std::string serialize_monte_carlo_workbench_result_json(
    const MonteCarloResult& result);

}  // namespace minera
