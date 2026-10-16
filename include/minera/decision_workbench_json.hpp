#pragma once

#include <set>
#include <string>
#include <string_view>

#include "minera/data_petri_net.hpp"

namespace minera {

// Exact native fields and defaults. Integers are JSON integer tokens in their
// native storage domain; doubles also accept NaN/INF/-INF string tags. Literal
// strings, including escaped lone surrogates, are never trimmed or split.
// Unknown/duplicate fields and type/storage errors reject; native algorithms
// own semantic validation. Input limits: 32 MiB, one million values, 64 levels.
// supplied is replaced transactionally with actual top-level names plus dotted
// nested names, e.g. mining and mining.max_depth. Nested {} supplies only its
// parent; omission supplies nothing. Table mining and outer fields are independent.
[[nodiscard]] DecisionMiningOptions parse_decision_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DecisionTableOptions parse_decision_table_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DataPetriNetOptions parse_data_petri_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);

[[nodiscard]] std::string serialize_decision_workbench_options_json(
    const DecisionMiningOptions& options);
[[nodiscard]] std::string serialize_decision_workbench_options_json(
    const DecisionTableOptions& options);
[[nodiscard]] std::string serialize_decision_workbench_options_json(
    const DataPetriNetOptions& options);

// Complete stored state without presentation constants or synthesized miner
// names. Inactive split fields, signed child sentinels, null features and raw
// IEEE values survive. DataPetriNet keeps the complete native net separately
// from its ordered guarded_transitions, even when a guard index is unused.
[[nodiscard]] std::string serialize_decision_workbench_result_json(
    const DecisionMiningResult& result);
[[nodiscard]] std::string serialize_decision_workbench_result_json(
    const DecisionTablesResult& result);
[[nodiscard]] std::string serialize_decision_workbench_result_json(
    const DataPetriNet& result);

}  // namespace minera
