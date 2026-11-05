#pragma once

#include <set>
#include <string>
#include <string_view>

#include "minera/playout.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/process_tree_playout.hpp"

namespace minera {

// Exact native option fields; omitted values retain the native defaults.
// Unknown/duplicate fields and type/storage-domain errors reject. Native
// algorithms retain ownership of semantic validation and overload behavior.
// Integers use exact JSON tokens. Doubles also accept NaN/INF/-INF string tags.
// Enums accept canonical names or {"name":NAME,"index":INT}; snapshots always
// emit both, with UNKNOWN for unrecognized stored native enum indexes.
// Escaped lone surrogates, literal controls and Unicode survive round trips.
// Input limits are 32 MiB, one million values and 64 container levels.
// supplied is replaced only after successful parsing, including for {}.
[[nodiscard]] PlayoutOptions parse_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] StochasticPlayoutOptions parse_stochastic_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] ExtensivePlayoutOptions parse_extensive_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DfgPlayoutOptions parse_dfg_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] ProcessTreePlayoutOptions parse_process_tree_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);

[[nodiscard]] std::string serialize_playout_workbench_options_json(
    const PlayoutOptions& options);
[[nodiscard]] std::string serialize_playout_workbench_options_json(
    const StochasticPlayoutOptions& options);
[[nodiscard]] std::string serialize_playout_workbench_options_json(
    const ExtensivePlayoutOptions& options);
[[nodiscard]] std::string serialize_playout_workbench_options_json(
    const DfgPlayoutOptions& options);
[[nodiscard]] std::string serialize_playout_workbench_options_json(
    const ProcessTreePlayoutOptions& options);

// Complete stored result snapshots, including inactive visitation payloads,
// raw enum indexes, exact counts, and the complete active-source log carrier.
[[nodiscard]] std::string serialize_playout_workbench_result_json(
    const PlayoutVisitedResult& result);
[[nodiscard]] std::string serialize_playout_workbench_result_json(
    const DfgPlayoutResult& result);
[[nodiscard]] std::string serialize_playout_workbench_result_json(
    const ProcessTreePlayoutResult& result);
// The exact size of serialize_playout_workbench_result_json(result)'s
// document, counted without building it.
[[nodiscard]] std::size_t playout_workbench_result_json_bytes(const PlayoutVisitedResult& result);

}  // namespace minera
