#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/performance.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"
#include "minera/resource_analysis.hpp"

namespace minera {

// Serializes an analysis (the imported event log plus its discovered
// directly-follows graph) into the JSON document consumed by the frontend.
// When `tree` is non-null its process tree is embedded under "process_tree";
// when `net` is non-null the Alpha Miner's Petri net is embedded under
// "petri_net"; when `heuristics` is non-null the Heuristic Miner's dependency
// graph is embedded under "heuristics_net"; when `conformance` is non-null each
// model's conformance is embedded under "conformance", keyed by miner id, with
// a "token_replay" and an "alignments" result per miner. When `performance` is
// non-null the time perspective (per-edge waiting times and case throughput) is
// embedded under "performance"; when `resources` is non-null the handover-of-
// work network is embedded under "resources". When `variant_durations` is
// non-null its entries (aligned to `graph.variants()` order) add a "duration"
// object to each emitted variant — the variant's case-throughput statistics.
// When `eventually_follows` is non-null the eventually-follows graph is
// embedded under "eventually_follows", shaped like "edges".
void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& event_log,
                         const DirectlyFollowsGraph& graph,
                         const ProcessTree* tree = nullptr,
                         const PetriNet* net = nullptr,
                         const HeuristicsNet* heuristics = nullptr,
                         const std::vector<ModelConformance>* conformance = nullptr,
                         const PerformanceMetrics* performance = nullptr,
                         const ResourceNetwork* resources = nullptr,
                         const std::vector<DurationStats>* variant_durations = nullptr,
                         const std::vector<EventuallyFollowsEdge>* eventually_follows = nullptr);

[[nodiscard]] std::string analysis_json(
    std::string_view dataset,
    const EventLog& event_log,
    const DirectlyFollowsGraph& graph,
    const ProcessTree* tree = nullptr,
    const PetriNet* net = nullptr,
    const HeuristicsNet* heuristics = nullptr,
    const std::vector<ModelConformance>* conformance = nullptr,
    const PerformanceMetrics* performance = nullptr,
    const ResourceNetwork* resources = nullptr,
    const std::vector<DurationStats>* variant_durations = nullptr,
    const std::vector<EventuallyFollowsEdge>* eventually_follows = nullptr);

// Serializes a single alignment result (the object the frontend reads as one
// model's `alignments`), for the server's on-demand alignment endpoint.
[[nodiscard]] std::string alignment_json(const AlignmentResult& alignment,
                                         const DirectlyFollowsGraph& graph);

// Serializes the conformance of an imported (user-supplied) model for the
// server's model-checking endpoint: `model` holds the same object written per
// miner under "conformance", and `activities` re-sends the id registry, which
// may have grown — the imported net can name activities the log never shows,
// so the frontend needs the extended registry to label them.
[[nodiscard]] std::string imported_model_json(const ModelConformance& model,
                                              const DirectlyFollowsGraph& graph);

}  // namespace minera
