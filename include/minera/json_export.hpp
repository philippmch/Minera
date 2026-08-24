#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/decision_table.hpp"
#include "minera/decision_mining.hpp"
#include "minera/dfg_playout.hpp"
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
#include "minera/batches.hpp"
#include "minera/chaotic_activities.hpp"
#include "minera/attribute_statistics.hpp"
#include "minera/attribute_distribution.hpp"
#include "minera/attribute_kde.hpp"
#include "minera/boundary_activities.hpp"
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/cycle_time.hpp"
#include "minera/petri_net.hpp"
#include "minera/playout.hpp"
#include "minera/powl.hpp"
#include "minera/process_tree.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/transition_system.hpp"
#include "minera/variant_statistics.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/concept_drift.hpp"
#include "minera/log_to_target.hpp"
#include "minera/trie.hpp"
#include "minera/interval_tree.hpp"
#include "minera/case_relations.hpp"
#include "minera/random_tree.hpp"
#include "minera/log_to_features.hpp"
#include "minera/embeddings.hpp"
#include "minera/label_splitting.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/trace_variant_query.hpp"
#include "minera/pripel.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/network_analysis.hpp"
#include "minera/overlap.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/rework.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"
#include "minera/local_diagnostics.hpp"

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
                             correlation_mining = nullptr,
                         const BatchDiscovery* batch_discovery = nullptr,
                         const DfgAlignmentLogResult* dfg_alignment = nullptr,
                         const std::vector<EditDistanceAlignment>* edit_distance = nullptr,
                         bool edit_anti = false,
                         const DfgAlignmentProjection* dfg_projection = nullptr,
                         const EventLog* dfg_projected_log = nullptr,
                         const EventLog* edit_projected_log = nullptr,
                         const ProcessTreeAlignmentResult* process_tree_alignment = nullptr,
                         const DecomposedAlignmentResult* decomposed_alignment = nullptr,
                         const PetriAntiAlignmentResult* petri_anti_alignment = nullptr,
                         const PetriMultiAlignmentResult* petri_multi_alignment = nullptr,
                         const ProcessTree* reduced_process_tree = nullptr,
                         std::size_t reduced_trace_index = 0,
                         const std::vector<ModelEvaluation>* evaluation = nullptr,
                         const EmdResult* earth_movers_distance = nullptr,
                         const IntervalEventOverlapCounts*
                             interval_event_overlap_counts = nullptr,
                         const CaseOverlapCounts* case_overlap_counts = nullptr,
                         const ConcurrentActivityOverlapResult*
                             concurrent_activity_overlap = nullptr,
                         const DecisionTablesResult* decision_tables = nullptr,
                         MinimumSelfDistanceVariant minimum_self_distance_variant =
                             MinimumSelfDistanceVariant::kLog);

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
    const CorrelationVariantsAnalysis* correlation_mining = nullptr,
    const BatchDiscovery* batch_discovery = nullptr,
    const DfgAlignmentLogResult* dfg_alignment = nullptr,
    const std::vector<EditDistanceAlignment>* edit_distance = nullptr,
    bool edit_anti = false,
    const DfgAlignmentProjection* dfg_projection = nullptr,
    const EventLog* dfg_projected_log = nullptr,
    const EventLog* edit_projected_log = nullptr,
    const ProcessTreeAlignmentResult* process_tree_alignment = nullptr,
    const DecomposedAlignmentResult* decomposed_alignment = nullptr,
    const PetriAntiAlignmentResult* petri_anti_alignment = nullptr,
    const PetriMultiAlignmentResult* petri_multi_alignment = nullptr,
    const ProcessTree* reduced_process_tree = nullptr,
    std::size_t reduced_trace_index = 0,
    const std::vector<ModelEvaluation>* evaluation = nullptr,
    const EmdResult* earth_movers_distance = nullptr,
    const IntervalEventOverlapCounts* interval_event_overlap_counts = nullptr,
    const CaseOverlapCounts* case_overlap_counts = nullptr,
    const ConcurrentActivityOverlapResult* concurrent_activity_overlap = nullptr,
    const DecisionTablesResult* decision_tables = nullptr,
    MinimumSelfDistanceVariant minimum_self_distance_variant =
        MinimumSelfDistanceVariant::kLog);

[[nodiscard]] std::string replay_fitness_json(
    const ReplayFitnessResult& result,
    const DirectlyFollowsGraph* graph = nullptr);
[[nodiscard]] std::string precision_json(
    const PrecisionResult& result,
    const DirectlyFollowsGraph* graph = nullptr);
[[nodiscard]] std::string generalization_json(
    const GeneralizationResult& result);
[[nodiscard]] std::string simplicity_json(
    const SimplicityResult& result);
[[nodiscard]] std::string emd_json(const EmdResult& result);
[[nodiscard]] std::string model_evaluation_json(
    const ModelEvaluation& result,
    const DirectlyFollowsGraph* graph = nullptr);

[[nodiscard]] std::string process_tree_alignment_json(
    const ProcessTreeAlignmentResult& result);

[[nodiscard]] std::string decomposed_alignment_json(
    const DecomposedAlignmentResult& result);
[[nodiscard]] std::string petri_anti_alignment_json(
    const PetriAntiAlignmentResult& result);
[[nodiscard]] std::string petri_multi_alignment_json(
    const PetriMultiAlignmentResult& result);
[[nodiscard]] std::string process_tree_reduction_json(
    const ProcessTree& result, const DirectlyFollowsGraph& graph,
    std::size_t trace_index);

[[nodiscard]] std::string dfg_alignment_json(
    const DfgAlignmentLogResult& result,
    const DfgAlignmentProjection* projection = nullptr,
    const EventLog* projected_log = nullptr);

// Serializes the deterministic sequence/count result returned by classic DFG
// playout when variant output is requested.
[[nodiscard]] std::string dfg_playout_json(const DfgPlayoutResult& result);

[[nodiscard]] std::string edit_distance_json(
    const std::vector<EditDistanceAlignment>& result,
    bool anti_alignment = false,
    const EventLog* projected_log = nullptr);

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

[[nodiscard]] std::string petri_usage_comparison_json(
    const PetriUsageComparison& result);
[[nodiscard]] std::string serialize_playout_visited_json(
    const PetriNet& net, const PlayoutVisitedResult& result);

[[nodiscard]] std::string concept_drift_json(const ConceptDriftResult& result);
[[nodiscard]] std::string log_to_target_json(const LogToTargetResult& result);
[[nodiscard]] std::string log_to_features_json(const LogToFeaturesResult& result);
[[nodiscard]] std::string serialize_embeddings_json(
    const EmbeddingResult& result);
[[nodiscard]] std::string serialize_embedding_similarity_json(
    std::string_view target_text, const EmbeddingResult& result,
    const std::vector<EmbeddingSimilarity>& matches, std::size_t top_k);
[[nodiscard]] std::string label_splitting_json(const LabelSplittingResult& result);
[[nodiscard]] std::string trie_json(const TrieModel& result);
[[nodiscard]] std::string interval_tree_json(
    const std::vector<OpenPathInterval>& result);
[[nodiscard]] std::string interval_tree_json(
    const IntervalTreeQueryResult& result);
[[nodiscard]] std::string case_relations_json(
    const EventLog& result,
    CaseRelationsVariant variant = CaseRelationsVariant::kPandas);
// The one-argument overload is the legacy BASIC serializer. Pass the
// variant explicitly for PT-and-log results.
[[nodiscard]] std::string random_tree_json(const ProcessTree& result);
[[nodiscard]] std::string random_tree_json(const ProcessTree& result,
                                            RandomTreeVariant variant);
[[nodiscard]] std::string random_tree_json(
    const std::vector<ProcessTree>& results, RandomTreeVariant variant,
    const std::vector<std::string>& activities = {});

// Standalone deterministic JSON for the classic bounded ILP miner.
[[nodiscard]] std::string ilp_miner_json(const ILPMinerResult& result);

// Standalone deterministic JSON for the bounded classic genetic miner.
[[nodiscard]] std::string genetic_miner_json(const GeneticMinerResult& result);

// Standalone deterministic JSON for trace-variant query anonymization.
[[nodiscard]] std::string trace_variant_query_json(
    const TraceVariantQueryResult& result);

// Standalone deterministic JSON for PRIPEL-style contextual anonymization.
[[nodiscard]] std::string pripel_json(const PripelResult& result);
[[nodiscard]] std::string chaotic_activities_json(
    const ChaoticActivitiesResult& result);
[[nodiscard]] std::string passed_time_json(const PassedTimeResult& result);
[[nodiscard]] std::string service_time_json(const ServiceTimeResult& result);
[[nodiscard]] std::string rework_json(const ReworkResult& result);
[[nodiscard]] std::string cycle_time_json(const CycleTimeResult& result);
[[nodiscard]] std::string case_arrival_json(const CaseArrivalResult& result);
[[nodiscard]] std::string case_statistics_json(
    const CaseStatisticsResult& result);
[[nodiscard]] std::string attribute_statistics_json(
    const AttributeStatisticsResult& result);
[[nodiscard]] std::string attribute_distribution_json(
    const AttributeDistributionResult& result);
[[nodiscard]] std::string attribute_kde_json(const AttributeKdeResult& result);
[[nodiscard]] std::string boundary_activities_json(
    const BoundaryActivitiesResult& result);
[[nodiscard]] std::string variant_statistics_json(
    const VariantStatisticsResult& result);
[[nodiscard]] std::string organizational_sna_json(
    const OrganizationalSnaResult& result);
[[nodiscard]] std::string network_analysis_json(
    const NetworkAnalysisResult& result);
[[nodiscard]] std::string resource_profiles_json(
    const ResourceProfilesResult& result);
[[nodiscard]] std::string role_discovery_json(const RoleDiscoveryResult& result);
[[nodiscard]] std::string local_diagnostics_json(const LocalDiagnosticsResult& result);

}  // namespace minera
