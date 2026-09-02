#pragma once

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/windows_event_log.hpp"
#include "minera/repository_history.hpp"
#include "minera/workflow_history.hpp"
#include "minera/enterprise_history.hpp"
#include "minera/communication_history.hpp"
#include "minera/knowledge_injection.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/log_filter.hpp"
#include "minera/ltl_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/transition_system.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/petri_net_reduction.hpp"
#include "minera/process_tree_playout.hpp"
#include "minera/declare_playout.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/playout.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/stochastic_petri.hpp"
#include "minera/stochastic_map.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/concept_drift.hpp"
#include "minera/log_to_target.hpp"
#include "minera/trie.hpp"
#include "minera/interval_tree.hpp"
#include "minera/case_relations.hpp"
#include "minera/random_tree.hpp"
#include "minera/log_to_features.hpp"
#include "minera/embeddings.hpp"
#include "minera/label_splitting.hpp"
#include "minera/link_analysis.hpp"
#include "minera/timestamp_interleavings.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/trace_variant_query.hpp"
#include "minera/pripel.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/network_analysis.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"
#include "minera/local_diagnostics.hpp"
#include "minera/clustering.hpp"
#include "minera/segment_filter.hpp"
#include "minera/path_filter.hpp"
#include "minera/overlap.hpp"
#include "minera/chaotic_activities.hpp"
#include "minera/attribute_statistics.hpp"
#include "minera/attribute_distribution.hpp"
#include "minera/attribute_kde.hpp"
#include "minera/boundary_activities.hpp"
#include "minera/browser_history.hpp"
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/variant_statistics.hpp"
#include "minera/cycle_time.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/rework.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/log_conversion.hpp"

// Pure request-parsing helpers shared by the local server
// (src/backend/tools/minera_server)
// and its tests. Nothing here depends on the HTTP layer: form fields arrive
// through a plain string-keyed lookup, so the parsing can be exercised directly.
namespace minera {

// Case-insensitive suffix match (the suffix is assumed already lowercase).
[[nodiscard]] bool ends_with_ci(std::string_view value, std::string_view suffix);

// Decides whether an upload is XES rather than CSV: a ".xes" name, or content
// that begins (after an optional UTF-8 BOM and whitespace) with "<?xml" or
// "<log" so extension-less uploads still route to the XES importer.
[[nodiscard]] bool looks_like_xes(std::string_view filename,
                                  std::string_view content);

// Splits a comma-separated field into trimmed, non-empty parts. Spaces
// surrounding each part are dropped (so "a , b" yields "a","b"); empty parts
// (e.g. from "a,,b" or a trailing comma) are skipped entirely.
[[nodiscard]] std::vector<std::string> split_csv(std::string_view value);

// Serializes a message as a one-key JSON error object, escaping quotes and
// backslashes so the message can never break the JSON.
[[nodiscard]] std::string error_json(std::string_view message);

// The discovery options parsed from a mine request's form fields.
// Operation selected by the dataframe-backed DFG transport. The native table
// options remain separate from the established event-log statistical options
// so callers can request either surface without changing defaults.
enum class DfgTableOperation {
  kGraph,
  kPartialOrder,
  kConcurrentEvents,

  GRAPH = kGraph,
  PARTIAL_ORDER = kPartialOrder,
  CONCURRENT_EVENTS = kConcurrentEvents,
};

struct MineParams {
  CsvImportOptions csv;
  XesImportOptions xes;
  LogFilterOptions filter;
  InductiveMinerOptions inductive;
  AlphaMinerOptions alpha;
  HeuristicMinerOptions heuristic;
  PowlDiscoveryOptions powl;
  // The standalone temporal adapters are strict by default. Mining bundles
  // this optional diagnostic into every analysis, so sparse/timestamp-less
  // logs retain the established permissive behavior unless explicitly opted
  // into strict validation.
  TemporalProfileOptions temporal_profile = [] {
    TemporalProfileOptions options;
    options.strict_attributes = false;
    return options;
  }();
  TransitionSystemOptions transition_system;
  TokenReplayOptions token_replay;
  // Whether the token-replay activity field was explicitly supplied. This
  // lets model-specific selectors provide defaults without masking a caller's
  // more specific replay choice.
  bool token_replay_activity_attribute_supplied = false;
  LogSkeletonOptions log_skeleton;
  DeclareDiscoveryOptions declare_options;
  FootprintsOptions footprints;
  // Empty vectors keep these analyses opt-in. String ids map to the native
  // enum values.
  std::vector<std::string> dfg_statistical_variants;
  std::vector<std::string> dfg_causal_variants;
  DfgStatisticalOptions dfg_statistics;
  // Dataframe/lazy-table DFG transport settings. `dfg_dataframe_requested`
  // is true when any dataframe-specific field was supplied, allowing the
  // server and CLI to keep this operation opt-in.
  DfgTableOptions dfg_table_options;
  DfgTableOperation dfg_operation = DfgTableOperation::kGraph;
  DfgTableBackend dfg_backend = DfgTableBackend::kPandas;
  bool dfg_dataframe_requested = false;
  // Correlation mining is strictly opt-in; an empty list preserves the
  // historical request behaviour and avoids requiring timestamps by default.
  std::vector<std::string> correlation_variants;
  CorrelationMinerOptions correlation;
  // Opt-in temporal/behavioral discovery options.
  std::vector<std::string> spectrum_activities;
  PerformanceSpectrumOptions spectrum;
  bool batches = false;
  BatchDiscoveryOptions batch_options;
  // Optional bounded temporal predicate, applied after traditional filters.
  std::optional<LtlFilterOptions> ltl;
  bool ltl_enabled = false;
  // Optional segment operations, applied after ordinary log filters and
  // before the bounded LTL predicate.
  std::optional<SegmentFilterOptions> segment;
  bool segment_enabled = false;
  // Optional adjacent predecessor/successor path predicate, applied after
  // ordinary filters and segment extraction and before LTL.
  std::optional<AdjacentPathFilterOptions> path;
  bool path_enabled = false;
  // Optional overlap statistics, computed after all log filters.
  bool overlap = false;
  OverlapOptions overlap_options;
  MinimumSelfDistanceOptions minimum_self_distance;
};

using ConceptDriftParams = ConceptDriftOptions;
using LogToTargetParams = LogToTargetOptions;
using TrieParams = TrieOptions;
using IntervalTreeParams = OpenPathIntervalOptions;
using CaseRelationsParams = CaseRelationsOptions;
using LinkAnalysisParams = LinkAnalysisOptions;
using TimestampInterleavingsParams = TimestampInterleavingOptions;
using RandomTreeParams = RandomTreeOptions;
using LogToFeaturesParams = LogToFeaturesOptions;
using EmbeddingParams = EmbeddingOptions;
using LabelSplittingParams = LabelSplittingOptions;
using BrowserHistoryParams = BrowserHistoryOptions;
using WindowsEventLogParams = WindowsEventLogOptions;

enum class EvaluationFamily {
  kReplayFitness,
  kPrecision,
  kGeneralization,
  kSimplicity,
  kComposite,
};

struct EvaluationParams {
  EvaluationFamily family = EvaluationFamily::kReplayFitness;
  ReplayFitnessVariant replay_fitness_variant = ReplayFitnessVariant::AUTO;
  PrecisionVariant precision_variant = PrecisionVariant::AUTO;
  PrecisionOptions precision_options;
  GeneralizationVariant generalization_variant =
      GeneralizationVariant::GENERALIZATION_TOKEN;
  SimplicityVariant simplicity_variant = SimplicityVariant::ARC_DEGREE;
  SimplicityOptions simplicity_options;
  CompositeEvaluationOptions composite_options;
};

enum class PetriAnalysisFamily {
  kMarkingEquation,
  kExtendedMarkingEquation,
  kWorkflowNet,
  kSoundness,
  kWoflan,
};

struct PetriAnalysisParams {
  PetriAnalysisFamily family = PetriAnalysisFamily::kMarkingEquation;
  std::string miner = "alpha";
  std::size_t trace_index = 0;
  Marking initial_marking;
  Marking final_marking;
  std::vector<double> transition_costs;
  std::vector<double> model_transition_costs;
  std::vector<double> synchronous_transition_costs;
  std::vector<double> log_move_costs;
  std::vector<std::size_t> split_indices;
  std::size_t max_k_value = 5;
  bool full_bootstrap_required = true;
  std::size_t max_states = 200000;
  std::size_t invariant_search_limit = 100000;
  bool diagnostics = false;
  bool return_asap_when_unsound = false;
};

// Strict wire contract for process-tree simulation. Fields use the
// `playout_` prefix when parsed from request/CLI adapters.
using ProcessTreePlayoutParams = ProcessTreePlayoutOptions;
using ExtensivePlayoutParams = ExtensivePlayoutOptions;
using DfgPlayoutParams = DfgPlayoutOptions;
using StochasticPlayoutParams = StochasticPlayoutOptions;
using OcpnPlayoutParams = OcpnPlayoutOptions;
using StochasticPetriParams = StochasticPetriOptions;
using MonteCarloParams = MonteCarloOptions;

// Conservative bound for a supplied transition map when no target net is
// available to provide a tighter limit. Server callers should pass the
// selected net's transition count, capped by this value.
inline constexpr std::size_t kMonteCarloRequestMapMaxRecords = 100'000;

enum class StochasticMapSource { kLog, kPerformanceDfg };

struct StochasticMapParams {
  StochasticMapSource source = StochasticMapSource::kLog;
  StochasticMapFitOptions fit;
  StochasticMapPerformanceOptions performance;
  std::vector<std::uint32_t> initial_marking;
  std::size_t max_states = kStochasticPetriMaxGeneratorStates;
};

// A lookup returning a form field's value, or "" when the field is absent.
using FieldLookup = std::function<std::string(std::string_view)>;
// Optional companion lookup that distinguishes an omitted field from a
// present field whose value is empty. This matters for options where an
// explicit empty list has different semantics from using the default.
using FieldPresence = std::function<bool(std::string_view)>;

// Strict request/CLI adapter for the bounded classic ILP miner. Wire names
// use the `ilp_` prefix for activity selection, artificial boundaries,
// sequence-encoding threshold, optional causal edges, noise, and safety.
[[nodiscard]] ILPMinerOptions parse_ilp_miner_params(const FieldLookup& field);
[[nodiscard]] ILPMinerOptions parse_ilp_miner_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict request/CLI adapter for the bounded classic genetic miner. Wire
// names use the `genetic_` prefix and include activity, timestamp, and case
// selectors, reproducibility seed, and evolutionary safety bounds.
[[nodiscard]] GeneticMinerOptions parse_genetic_miner_params(
    const FieldLookup& field);
[[nodiscard]] GeneticMinerOptions parse_genetic_miner_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict wire parser for trace-variant query anonymization. Fields use the
// `tvq_` prefix: variant, epsilon, k, p, seed, secure randomness, and output
// safety bounds.
[[nodiscard]] TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field);
[[nodiscard]] TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict PRIPEL-style anonymization parser. Fields use the `pripel_` prefix,
// including the opt-in secure randomness switch.
[[nodiscard]] PripelOptions parse_pripel_params(const FieldLookup& field);
[[nodiscard]] PripelOptions parse_pripel_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict organizational SNA parser. Fields use the `sna_` prefix.
[[nodiscard]] OrganizationalSnaOptions parse_organizational_sna_params(
    const FieldLookup& field);
[[nodiscard]] OrganizationalSnaOptions parse_organizational_sna_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] RoleDiscoveryOptions parse_role_discovery_params(
    const FieldLookup& field);
[[nodiscard]] RoleDiscoveryOptions parse_role_discovery_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] ResourceProfilesOptions parse_resource_profiles_params(
    const FieldLookup& field);
[[nodiscard]] ResourceProfilesOptions parse_resource_profiles_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] LocalDiagnosticsOptions parse_local_diagnostics_params(
    const FieldLookup& field);
[[nodiscard]] LocalDiagnosticsOptions parse_local_diagnostics_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] TraceAttributeClusteringOptions parse_clustering_params(
    const FieldLookup& field);
[[nodiscard]] TraceAttributeClusteringOptions parse_clustering_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] ProfileClusteringOptions parse_profile_clustering_params(
    const FieldLookup& field);
[[nodiscard]] ProfileClusteringOptions parse_profile_clustering_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ChaoticActivitiesOptions parse_chaotic_activities_params(
    const FieldLookup& field);
[[nodiscard]] ChaoticActivitiesOptions parse_chaotic_activities_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] PassedTimeOptions parse_passed_time_params(
    const FieldLookup& field);
[[nodiscard]] PassedTimeOptions parse_passed_time_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ServiceTimeOptions parse_service_time_params(
    const FieldLookup& field);
[[nodiscard]] ServiceTimeOptions parse_service_time_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ReworkOptions parse_rework_params(const FieldLookup& field);
[[nodiscard]] ReworkOptions parse_rework_params(const FieldLookup& field,
                                                const FieldPresence& present);

[[nodiscard]] CycleTimeOptions parse_cycle_time_params(
    const FieldLookup& field);
[[nodiscard]] CycleTimeOptions parse_cycle_time_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] CaseArrivalOptions parse_case_arrival_params(
    const FieldLookup& field);
[[nodiscard]] CaseArrivalOptions parse_case_arrival_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] CaseStatisticsOptions parse_case_statistics_params(
    const FieldLookup& field);
[[nodiscard]] CaseStatisticsOptions parse_case_statistics_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] AttributeStatisticsOptions parse_attribute_statistics_params(
    const FieldLookup& field);
[[nodiscard]] AttributeStatisticsOptions parse_attribute_statistics_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] AttributeDistributionOptions parse_attribute_distribution_params(
    const FieldLookup& field);
[[nodiscard]] AttributeDistributionOptions parse_attribute_distribution_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] AttributeKdeOptions parse_attribute_kde_params(
    const FieldLookup& field);
[[nodiscard]] AttributeKdeOptions parse_attribute_kde_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] BoundaryActivitiesOptions parse_boundary_activities_params(
    const FieldLookup& field);
[[nodiscard]] BoundaryActivitiesOptions parse_boundary_activities_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] VariantStatisticsOptions parse_variant_statistics_params(
    const FieldLookup& field);
[[nodiscard]] VariantStatisticsOptions parse_variant_statistics_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] NetworkAnalysisOptions parse_network_analysis_params(
    const FieldLookup& field);
[[nodiscard]] NetworkAnalysisOptions parse_network_analysis_params(
    const FieldLookup& field, const FieldPresence& present);

// Parse the escaped activity=cost grammar shared by HTTP requests and CLI
// adapters. Commas, separators, backslashes, and whitespace may be escaped.
[[nodiscard]] std::map<std::string, double> parse_activity_costs(
    std::string_view value, std::string_view name = "activity_costs");

// Standalone conformance option parsers. Wire fields use the dfg_* and
// edit_* prefixes; map values are comma-separated `activity=value` pairs
// (a colon is accepted as a compatibility separator). Explicitly present
// empty values are rejected for every option.
[[nodiscard]] DfgAlignmentOptions parse_dfg_alignment_params(
    const FieldLookup& field);
[[nodiscard]] DfgAlignmentOptions parse_dfg_alignment_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] EditDistanceOptions parse_edit_distance_params(
    const FieldLookup& field);
[[nodiscard]] EditDistanceOptions parse_edit_distance_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses alignment search settings shared by the HTTP server, CLI and tests.
// Fields use the wire names documented by /api/align (alignment_variant,
// max_states, max_align_time, ...).  Invalid or dependent settings throw
// std::invalid_argument rather than silently falling back to defaults.
[[nodiscard]] AlignmentOptions parse_alignment_params(const FieldLookup& field);
[[nodiscard]] AlignmentOptions parse_alignment_params(const FieldLookup& field,
                                                       const FieldPresence& present);

// Parses process-tree alignment settings. Wire fields use the canonical
// `pt_alignment_` prefix (for example `pt_alignment_variant` and
// `pt_alignment_max_time_ms`). Time limits are expressed in milliseconds on
// the wire and converted to the core's seconds representation. Zero means
// unlimited where supported by the core.
[[nodiscard]] ProcessTreeAlignmentOptions parse_process_tree_alignment_params(
    const FieldLookup& field);
[[nodiscard]] ProcessTreeAlignmentOptions parse_process_tree_alignment_params(
    const FieldLookup& field, const FieldPresence& present);

// Builds the discovery options from a mine request's form fields. The
// single-callback overload treats absent and empty fields alike, preserving the
// original defaults. With FieldPresence, explicitly present empty Declare list
// fields become engaged empty filters, while footprint and DFG variant lists
// can reject an explicit empty list; omitted fields retain their defaults. A
// `time_mode` value that is not one of the three known modes throws
// std::invalid_argument, so the caller can report it instead of silently mining
// with the wrong filter.
[[nodiscard]] MineParams parse_mine_params(const FieldLookup& field);
[[nodiscard]] MineParams parse_mine_params(const FieldLookup& field,
                                           const FieldPresence& present);

[[nodiscard]] DecomposedAlignmentOptions parse_decomposed_alignment_params(
    const FieldLookup& field);
[[nodiscard]] DecomposedAlignmentOptions parse_decomposed_alignment_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] PetriAntiAlignmentOptions parse_petri_anti_alignment_params(
    const FieldLookup& field);
[[nodiscard]] PetriAntiAlignmentOptions parse_petri_anti_alignment_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] PetriMultiAlignmentOptions parse_petri_multi_alignment_params(
    const FieldLookup& field);
[[nodiscard]] PetriMultiAlignmentOptions parse_petri_multi_alignment_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] ProcessTreeReductionOptions parse_process_tree_reduction_params(
    const FieldLookup& field);
[[nodiscard]] ProcessTreeReductionOptions parse_process_tree_reduction_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] PetriNetReductionOptions parse_petri_net_reduction_params(
    const FieldLookup& field);
[[nodiscard]] PetriNetReductionOptions parse_petri_net_reduction_params(
    const FieldLookup& field, const FieldPresence& present);

// Parse the model-quality evaluation request contract. Every supplied option
// is validated, even when its family is not selected, so malformed values
// cannot be hidden by an unrelated evaluation family.
[[nodiscard]] EvaluationParams parse_evaluation_params(
    const FieldLookup& field);
[[nodiscard]] EvaluationParams parse_evaluation_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] PetriAnalysisParams parse_petri_analysis_params(
    const FieldLookup& field);
[[nodiscard]] PetriAnalysisParams parse_petri_analysis_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ProcessTreePlayoutParams parse_process_tree_playout_params(
    const FieldLookup& field);
[[nodiscard]] ProcessTreePlayoutParams parse_process_tree_playout_params(
    const FieldLookup& field, const FieldPresence& present);

using DeclarePlayoutParams = DeclarePlayoutOptions;
[[nodiscard]] DeclarePlayoutParams parse_declare_playout_params(
    const FieldLookup& field);
[[nodiscard]] DeclarePlayoutParams parse_declare_playout_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ExtensivePlayoutParams parse_extensive_playout_params(
    const FieldLookup& field);
[[nodiscard]] ExtensivePlayoutParams parse_extensive_playout_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses the optional OCPN playout trace-detail switch. The preferred wire
// field is `ocpn_return_traces`; `return_traces` is accepted for the HTTP
// endpoint's unprefixed query contract.
[[nodiscard]] OcpnPlayoutParams parse_ocpn_playout_params(
    const FieldLookup& field);
[[nodiscard]] OcpnPlayoutParams parse_ocpn_playout_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses DFG playout settings. Wire fields use the `dfg_playout_` prefix;
// every supplied value is validated and unsafe/unbounded values are rejected
// by the adapters rather than silently falling back to native defaults.
[[nodiscard]] DfgPlayoutParams parse_dfg_playout_params(
    const FieldLookup& field);
[[nodiscard]] DfgPlayoutParams parse_dfg_playout_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses stochastic Petri-net playout settings. Wire fields use the
// `stochastic_` prefix. Transition weights are comma-separated finite,
// non-negative values; `stochastic_fit_from_log=true` asks the adapters to fit
// weights from the cached/source event log when no explicit vector is supplied.
// The native simulator validates that explicit vector counts match the net.
[[nodiscard]] StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field);
[[nodiscard]] StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] StochasticPetriParams parse_stochastic_petri_params(
    const FieldLookup& field);
[[nodiscard]] StochasticPetriParams parse_stochastic_petri_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] StochasticMapParams parse_stochastic_map_params(
    const FieldLookup& field);
[[nodiscard]] StochasticMapParams parse_stochastic_map_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses bounded virtual-time Monte Carlo settings. Wire fields use the
// `monte_carlo_` prefix. Transition durations are comma-separated
// `distribution:param[;param...]` entries for deterministic, exponential,
// constant0, immediate, normal, uniform, lognormal, and gamma families;
// resource capacities use `place=count` pairs. A provided stochastic map is
// encoded as `index:timing:distribution:p1:p2:p3:weight:priority` records.
[[nodiscard]] MonteCarloParams parse_monte_carlo_params(
    const FieldLookup& field);
[[nodiscard]] MonteCarloParams parse_monte_carlo_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] MonteCarloParams parse_monte_carlo_params(
    const FieldLookup& field, const FieldPresence& present,
    std::size_t max_stochastic_map_records);

[[nodiscard]] ConceptDriftParams parse_concept_drift_params(
    const FieldLookup& field);
[[nodiscard]] ConceptDriftParams parse_concept_drift_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] BrowserHistoryParams parse_browser_history_params(
    const FieldLookup& field);
[[nodiscard]] BrowserHistoryParams parse_browser_history_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] WindowsEventLogParams parse_windows_event_log_params(
    const FieldLookup& field);
[[nodiscard]] WindowsEventLogParams parse_windows_event_log_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] RepositoryHistoryOptions parse_repository_history_params(
    const FieldLookup& field);
[[nodiscard]] RepositoryHistoryOptions parse_repository_history_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] WorkflowHistoryOptions parse_workflow_history_params(
    const FieldLookup& field);
[[nodiscard]] WorkflowHistoryOptions parse_workflow_history_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] EnterpriseHistoryOptions parse_enterprise_history_params(
    const FieldLookup& field);
[[nodiscard]] EnterpriseHistoryOptions parse_enterprise_history_params(
    const FieldLookup& field, const FieldPresence& present);
[[nodiscard]] CommunicationHistoryOptions parse_communication_history_params(
    const FieldLookup& field);
[[nodiscard]] CommunicationHistoryOptions parse_communication_history_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] KnowledgeInjectionOptions parse_knowledge_injection_params(
    const FieldLookup& field);
[[nodiscard]] KnowledgeInjectionOptions parse_knowledge_injection_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] LogToTargetParams parse_log_to_target_params(
    const FieldLookup& field);
[[nodiscard]] LogToTargetParams parse_log_to_target_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] TrieParams parse_trie_params(const FieldLookup& field);
[[nodiscard]] TrieParams parse_trie_params(const FieldLookup& field,
                                           const FieldPresence& present);

[[nodiscard]] IntervalTreeParams parse_interval_tree_params(
    const FieldLookup& field);
[[nodiscard]] IntervalTreeParams parse_interval_tree_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] CaseRelationsParams parse_case_relations_params(
    const FieldLookup& field);
[[nodiscard]] CaseRelationsParams parse_case_relations_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] LinkAnalysisParams parse_link_analysis_params(
    const FieldLookup& field);
[[nodiscard]] LinkAnalysisParams parse_link_analysis_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] TimestampInterleavingsParams
parse_timestamp_interleavings_params(const FieldLookup& field);
[[nodiscard]] TimestampInterleavingsParams parse_timestamp_interleavings_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] RandomTreeParams parse_random_tree_params(
    const FieldLookup& field);
[[nodiscard]] RandomTreeParams parse_random_tree_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses the native log-to-features projection. Wire fields use the
// `features_` prefix (the singular `feature_` spelling is accepted for
// compatibility). Lists are comma-separated and explicitly present empty
// lists are rejected.
[[nodiscard]] LogToFeaturesParams parse_log_to_features_params(
    const FieldLookup& field);
[[nodiscard]] LogToFeaturesParams parse_log_to_features_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict parser for deterministic case/event embedding transformations. Wire
// fields use the `embedding_` prefix.
[[nodiscard]] EmbeddingParams parse_embedding_params(
    const FieldLookup& field);
[[nodiscard]] EmbeddingParams parse_embedding_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] LabelSplittingParams parse_label_splitting_params(
    const FieldLookup& field);
[[nodiscard]] LabelSplittingParams parse_label_splitting_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict request/CLI adapter for EventLog conversion selectors. Wire fields
// use the `log_conversion_` prefix and preserve explicit empty values where
// the conversion contract permits them (for example, an empty case prefix).
[[nodiscard]] LogConversionOptions parse_log_conversion_params(
    const FieldLookup& field);
[[nodiscard]] LogConversionOptions parse_log_conversion_params(
    const FieldLookup& field, const FieldPresence& present);

}  // namespace minera
