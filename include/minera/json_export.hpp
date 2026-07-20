#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/decision_mining.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/model_decoration.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/performance.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/petri_net.hpp"
#include "minera/powl.hpp"
#include "minera/process_tree.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/transition_system.hpp"

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
// When `decisions` is non-null the decision-mining result is embedded under
// "decisions". When both `net` and `petri_decoration` are non-null the net's
// replay decoration (transition firing counts and place waiting times, indices
// matching "petri_net") is embedded under "petri_net_decoration". When
// `spectrum` is non-null the performance spectrum (the pattern and one
// instants row per observation) is embedded under "performance_spectrum".
// The inductive and heuristic models' workflow nets travel the same way the
// alpha net does: when `inductive_net` / `heuristic_petri` are non-null they
// are embedded under "inductive_petri_net" / "heuristic_petri_net", each with
// its replay decoration under the matching "..._decoration" key when given —
// so every miner's model can be drawn (and annotated) as a Petri net, not
// just exported as one.
// When `temporal_profile` is non-null its entries are embedded under
// "temporal_profile", with source and target activity ids resolved against
// the directly-follows graph.
// When `powl` is non-null it is embedded recursively under "powl". Its
// converted workflow net and replay decoration use "powl_petri_net" and
// "powl_petri_net_decoration", matching the other discovered miners.
// `log_skeleton` and `log_skeleton_conformance` add the classic declarative
// constraint model and one diagnostic result per trace.
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
                         const std::vector<EventuallyFollowsEdge>* eventually_follows = nullptr,
                         const DecisionMiningResult* decisions = nullptr,
                         const ModelDecoration* petri_decoration = nullptr,
                         const PerformanceSpectrum* spectrum = nullptr,
                         const PetriNet* inductive_net = nullptr,
                         const ModelDecoration* inductive_decoration = nullptr,
                         const PetriNet* heuristic_petri = nullptr,
                         const ModelDecoration* heuristic_decoration = nullptr,
                         const TemporalProfile* temporal_profile = nullptr,
                         const MinimumSelfDistances* minimum_self_distances = nullptr,
                         const TransitionSystem* transition_system = nullptr,
                         const PowlNode* powl = nullptr,
                         const PetriNet* powl_petri = nullptr,
                         const ModelDecoration* powl_decoration = nullptr,
                         const LogSkeletonModel* log_skeleton = nullptr,
                         const std::vector<LogSkeletonConformance>*
                             log_skeleton_conformance = nullptr,
                         const DeclareModel* declare = nullptr,
                         const std::vector<DeclareConformance>*
                             declare_conformance = nullptr,
                         const FootprintsAnalysis* footprints = nullptr,
                         const DfgVariantsAnalysis* dfg_variants = nullptr,
                         const CorrelationVariantsAnalysis*
                             correlation_mining = nullptr);

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
    const std::vector<EventuallyFollowsEdge>* eventually_follows = nullptr,
    const DecisionMiningResult* decisions = nullptr,
    const ModelDecoration* petri_decoration = nullptr,
    const PerformanceSpectrum* spectrum = nullptr,
    const PetriNet* inductive_net = nullptr,
    const ModelDecoration* inductive_decoration = nullptr,
    const PetriNet* heuristic_petri = nullptr,
    const ModelDecoration* heuristic_decoration = nullptr,
    const TemporalProfile* temporal_profile = nullptr,
    const MinimumSelfDistances* minimum_self_distances = nullptr,
    const TransitionSystem* transition_system = nullptr,
    const PowlNode* powl = nullptr,
    const PetriNet* powl_petri = nullptr,
    const ModelDecoration* powl_decoration = nullptr,
    const LogSkeletonModel* log_skeleton = nullptr,
    const std::vector<LogSkeletonConformance>* log_skeleton_conformance =
        nullptr,
    const DeclareModel* declare = nullptr,
    const std::vector<DeclareConformance>* declare_conformance = nullptr,
    const FootprintsAnalysis* footprints = nullptr,
    const DfgVariantsAnalysis* dfg_variants = nullptr,
    const CorrelationVariantsAnalysis* correlation_mining = nullptr);

// Serializes a decision-mining result — per decision point, the branch
// frequencies and the mined rule tree — as one standalone object: the same
// shape embedded under "decisions", for the server's on-demand endpoint.
[[nodiscard]] std::string decisions_json(const DecisionMiningResult& decisions);

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
