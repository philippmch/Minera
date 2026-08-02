#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/powl_discovery.hpp"
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
#include "minera/process_tree_playout.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/playout.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/concept_drift.hpp"
#include "minera/log_to_target.hpp"
#include "minera/trie.hpp"
#include "minera/interval_tree.hpp"
#include "minera/case_relations.hpp"
#include "minera/random_tree.hpp"
#include "minera/log_to_features.hpp"
#include "minera/label_splitting.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/trace_variant_query.hpp"
#include "minera/pripel.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/network_analysis.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"
#include "minera/local_diagnostics.hpp"

// Pure request-parsing helpers shared by the local server (tools/minera_server)
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
struct MineParams {
  CsvImportOptions csv;
  LogFilterOptions filter;
  InductiveMinerOptions inductive;
  AlphaMinerOptions alpha;
  HeuristicMinerOptions heuristic;
  PowlDiscoveryOptions powl;
  TransitionSystemOptions transition_system;
  TokenReplayOptions token_replay;
  LogSkeletonOptions log_skeleton;
  DeclareDiscoveryOptions declare_options;
  FootprintsOptions footprints;
  // Empty vectors keep these analyses opt-in. String ids map to the native
  // enum values.
  std::vector<std::string> dfg_statistical_variants;
  std::vector<std::string> dfg_causal_variants;
  DfgStatisticalOptions dfg_statistics;
  // Correlation mining is strictly opt-in; an empty list preserves the
  // historical request behaviour and avoids requiring timestamps by default.
  std::vector<std::string> correlation_variants;
  CorrelationMinerOptions correlation;
  // Opt-in temporal/behavioral discovery options.
  std::vector<std::string> spectrum_activities;
  PerformanceSpectrumOptions spectrum;
  bool batches = false;
  BatchDiscoveryOptions batch_options;
};

using ConceptDriftParams = ConceptDriftOptions;
using LogToTargetParams = LogToTargetOptions;
using TrieParams = TrieOptions;
using IntervalTreeParams = OpenPathIntervalOptions;
using CaseRelationsParams = CaseRelationsOptions;
using RandomTreeParams = RandomTreeOptions;
using LogToFeaturesParams = LogToFeaturesOptions;
using LabelSplittingParams = LabelSplittingOptions;

enum class EvaluationFamily {
  kReplayFitness,
  kPrecision,
  kGeneralization,
  kSimplicity,
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
};

enum class PetriAnalysisFamily {
  kMarkingEquation,
  kExtendedMarkingEquation,
  kWorkflowNet,
  kSoundness,
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
  bool diagnostics = false;
};

// Strict wire contract for process-tree simulation. Fields use the
// `playout_` prefix when parsed from request/CLI adapters.
using ProcessTreePlayoutParams = ProcessTreePlayoutOptions;
using DfgPlayoutParams = DfgPlayoutOptions;
using StochasticPlayoutParams = StochasticPlayoutOptions;

// A lookup returning a form field's value, or "" when the field is absent.
using FieldLookup = std::function<std::string(std::string_view)>;
// Optional companion lookup that distinguishes an omitted field from a
// present field whose value is empty. This matters for options where an
// explicit empty list has different semantics from using the default.
using FieldPresence = std::function<bool(std::string_view)>;

// Strict request/CLI adapter for the bounded classic ILP miner. Wire names
// are `ilp_activity_key`, `ilp_noise_threshold`, and `ilp_max_nodes`.
[[nodiscard]] ILPMinerOptions parse_ilp_miner_params(const FieldLookup& field);
[[nodiscard]] ILPMinerOptions parse_ilp_miner_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict request/CLI adapter for the bounded classic genetic miner. Wire
// names use the `genetic_` prefix and include the reproducibility seed and
// evolutionary safety bounds.
[[nodiscard]] GeneticMinerOptions parse_genetic_miner_params(
    const FieldLookup& field);
[[nodiscard]] GeneticMinerOptions parse_genetic_miner_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict wire parser for trace-variant query anonymization. Fields use the
// `tvq_` prefix: variant, epsilon, k, p, seed, and output safety bounds.
[[nodiscard]] TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field);
[[nodiscard]] TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field, const FieldPresence& present);

// Strict PRIPEL-style anonymization parser. Fields use the `pripel_` prefix.
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

// Parses DFG playout settings. Wire fields use the `dfg_playout_` prefix;
// every supplied value is validated and unsafe/unbounded values are rejected
// by the adapters rather than silently falling back to native defaults.
[[nodiscard]] DfgPlayoutParams parse_dfg_playout_params(
    const FieldLookup& field);
[[nodiscard]] DfgPlayoutParams parse_dfg_playout_params(
    const FieldLookup& field, const FieldPresence& present);

// Parses stochastic Petri-net playout settings. Wire fields use the
// `stochastic_` prefix. Transition weights are comma-separated finite,
// non-negative values; the native simulator validates that their count
// matches the selected net's transition count.
[[nodiscard]] StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field);
[[nodiscard]] StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field, const FieldPresence& present);

[[nodiscard]] ConceptDriftParams parse_concept_drift_params(
    const FieldLookup& field);
[[nodiscard]] ConceptDriftParams parse_concept_drift_params(
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

[[nodiscard]] LabelSplittingParams parse_label_splitting_params(
    const FieldLookup& field);
[[nodiscard]] LabelSplittingParams parse_label_splitting_params(
    const FieldLookup& field, const FieldPresence& present);

}  // namespace minera
