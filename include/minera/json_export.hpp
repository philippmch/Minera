#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Serializes an analysis (the imported event log plus its discovered
// directly-follows graph) into the JSON document consumed by the frontend.
// When `tree` is non-null its process tree is embedded under "process_tree";
// when `net` is non-null the Alpha Miner's Petri net is embedded under
// "petri_net"; when `heuristics` is non-null the Heuristic Miner's dependency
// graph is embedded under "heuristics_net"; when `conformance` is non-null each
// model's conformance is embedded under "conformance", keyed by miner id, with
// a "token_replay" and an "alignments" result per miner.
void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& event_log,
                         const DirectlyFollowsGraph& graph,
                         const ProcessTree* tree = nullptr,
                         const PetriNet* net = nullptr,
                         const HeuristicsNet* heuristics = nullptr,
                         const std::vector<ModelConformance>* conformance = nullptr);

[[nodiscard]] std::string analysis_json(
    std::string_view dataset,
    const EventLog& event_log,
    const DirectlyFollowsGraph& graph,
    const ProcessTree* tree = nullptr,
    const PetriNet* net = nullptr,
    const HeuristicsNet* heuristics = nullptr,
    const std::vector<ModelConformance>* conformance = nullptr);

}  // namespace minera
