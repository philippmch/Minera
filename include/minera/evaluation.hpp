#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/event_data.hpp"

namespace minera {

struct PnmlDocument;

// Native evaluation variants. Upper-case enumerators are retained for
// compatibility with the public evaluator vocabulary.
enum class ReplayFitnessVariant { AUTO, TOKEN_BASED, ALIGNMENT_BASED,
                                  kTokenBased = TOKEN_BASED,
                                  kAlignmentBased = ALIGNMENT_BASED,
                                  kAuto = AUTO };
enum class PrecisionVariant { AUTO, ETCONFORMANCE_TOKEN, ALIGN_ETCONFORMANCE,
                              AUTOMATON_AFTER_ALIGN,
                              kEtconformanceToken = ETCONFORMANCE_TOKEN,
                              kAlignEtconformance = ALIGN_ETCONFORMANCE,
                              kAutomatonAfterAlign = AUTOMATON_AFTER_ALIGN,
                              kAuto = AUTO };
enum class GeneralizationVariant { GENERALIZATION_TOKEN,
                                   kGeneralizationToken = GENERALIZATION_TOKEN };
enum class SimplicityVariant { ARC_DEGREE, EXTENDED_CARDOSO,
                               EXTENDED_CYCLOMATIC,
                               kArcDegree = ARC_DEGREE,
                               kExtendedCardoso = EXTENDED_CARDOSO,
                               kExtendedCyclomatic = EXTENDED_CYCLOMATIC };

using ReplayFitnessMethod = ReplayFitnessVariant;
using PrecisionMethod = PrecisionVariant;
using GeneralizationMethod = GeneralizationVariant;
using SimplicityMethod = SimplicityVariant;

struct ReplayFitnessResult {
  ReplayFitnessVariant variant = ReplayFitnessVariant::TOKEN_BASED;
  double fitness = 0.0;
  double log_fitness = 0.0;  // alias for fitness
  double average_trace_fitness = 0.0;
  double percentage_fit_traces = 0.0;
  std::uint64_t fitting_traces = 0;
  std::uint64_t trace_count = 0;
  double total_cost = 0.0;
  bool computed = true;
  bool exhausted = false;
  std::string error;
  ConformanceResult token_result;
  AlignmentResult alignment_result;
  std::optional<DecomposedAlignmentResult> decomposed_alignment_result;
  // The exact token-result aggregate retains the public evaluator's integer
  // zero when either aggregate denominator is nonpositive or input is empty.
  bool log_fitness_is_integer_zero = false;
  // Fitness-only replay does not evaluate the bundle's other quality scores.
  bool token_result_is_fitness_only = false;
};

// Evaluator-only replay controls. Alignment evaluation uses standard move
// costs and the less-memory Dijkstra engine unless explicitly configured.
struct ReplayFitnessOptions {
  // Forward replay can count labels absent from the model as deviations. The
  // legacy backwards variation always ignores them.
  bool consider_activities_not_in_model_in_fitness = false;
  std::string activity_attribute = "concept:name";
  TokenReplayVariant token_replay_variant = TokenReplayVariant::kForward;
  AlignmentOptions alignment_options{
      AlignmentVariant::kStateEquationAStar, AlignmentCostProfile::kStandard};
  // Engaging this option selects decomposed recomposition instead of the
  // ordinary Petri-net alignment engine.
  std::optional<DecomposedAlignmentOptions> decomposed_alignment_options;
  // Common marked-net contract applied to the selected replay engine. Empty
  // vectors retain that engine's workflow-net defaults.
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  // Case field used by flat event-data adapters. It is ignored for an
  // already grouped EventLog, whose Trace boundaries are authoritative.
  std::string case_id_attribute = "case:concept:name";
  // Optional hard cap for grouped or flat input materialization. Exceeding it
  // is a deterministic length error rather than silent truncation.
  std::optional<std::size_t> max_rows_to_materialize;
  bool cleaning_token_flood = false;
  // Exact forward token markings override the legacy vectors. Sparse key
  // membership retains explicit zero entries; names identify supplied models.
  std::optional<TokenMarking> exact_initial_marking;
  std::optional<TokenMarking> exact_final_marking;
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  // Token replay's per-place silent-path tie order (see TokenReplayOptions).
  // PNML document entry points default it to the imported arc order.
  std::optional<std::vector<std::vector<std::size_t>>> place_outgoing_transition_order;
};

struct PrecisionResult {
  PrecisionVariant variant = PrecisionVariant::ETCONFORMANCE_TOKEN;
  double precision = 1.0;
  bool computed = true;
  bool exhausted = false;
  std::string error;
  ConformanceResult token_result;
  AlignmentResult alignment_result;
  // ALIGN_ETCONFORMANCE computes prefix alignments only. In that variation
  // alignment_result is deliberately not an ordinary full-log payload.
  bool alignment_detail_present = false;
  bool token_result_is_precision_only = false;
};
struct PrecisionOptions {
  bool use_task_ids = false;
  // Maximum number of markings/product states explored by exact precision
  // searches.  Zero is invalid and reports an exhausted computation.
  std::size_t max_states = 100000;
  std::string activity_attribute = "concept:name";
  TokenReplayVariant token_replay_variant = TokenReplayVariant::kForward;
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  std::string case_id_attribute = "case:concept:name";
  std::optional<std::size_t> max_rows_to_materialize;
  std::optional<TokenMarking> exact_initial_marking;
  std::optional<TokenMarking> exact_final_marking;
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  // Token replay's per-place silent-path tie order (see TokenReplayOptions).
  // PNML document entry points default it to the imported arc order.
  std::optional<std::vector<std::vector<std::size_t>>> place_outgoing_transition_order;
};

struct GeneralizationOptions {
  std::string activity_attribute = "concept:name";
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  std::string case_id_attribute = "case:concept:name";
  std::optional<std::size_t> max_rows_to_materialize;
  std::optional<TokenMarking> exact_initial_marking;
  std::optional<TokenMarking> exact_final_marking;
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  // Token replay's per-place silent-path tie order (see TokenReplayOptions).
  // PNML document entry points default it to the imported arc order.
  std::optional<std::vector<std::vector<std::size_t>>> place_outgoing_transition_order;
};

struct GeneralizationResult {
  GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN;
  double generalization = 0.0;
  std::uint64_t executed_transitions = 0;
  std::uint64_t transition_count = 0;
  std::vector<std::uint64_t> transition_frequencies;
  bool computed = true;
  std::string error;
};

struct SimplicityOptions {
  // Optional caller-provided initial marking. For extended cyclomatic,
  // empty reconstructs the default: one token in every empty-preset place.
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  std::size_t max_states = 100000;
  // Arc-degree accepts every double baseline, including NaN and infinities.
  double arc_degree_k = 2.0;
  // Full presentations/documents can retain sparse signed tokens, explicit
  // zero entries and marking key order. IDs refer to presented place IDs.
  // When supplied, this overrides initial_marking, including an empty marking.
  std::optional<std::vector<PetriSparseMarkingEntry>> presentation_initial_marking;
};

struct SimplicityResult {
  SimplicityVariant variant = SimplicityVariant::ARC_DEGREE;
  double simplicity = 0.0;
  double complexity = 0.0;
  double value = 0.0;
  std::size_t vertices = 0;
  // Cyclomatic graph arcs retain transition names and marking endpoints;
  // executable transitions have distinct implicit names.
  std::size_t edges = 0;
  // Graph metadata; the pinned public scalar is reported separately in value.
  std::size_t strongly_connected_components = 0;
  bool computed = true;
  bool exhausted = false;
  std::string error;
};

struct CompositeEvaluationOptions {
  // Public weight normalization accepts negative, infinite and NaN values.
  // A zero total is undefined and reports an uncomputed result.
  double fitness_weight = 0.25;
  double precision_weight = 0.25;
  double simplicity_weight = 0.25;
  double generalization_weight = 0.25;
  std::string activity_attribute = "concept:name";
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  std::string case_id_attribute = "case:concept:name";
  std::optional<std::size_t> max_rows_to_materialize;
  std::optional<TokenMarking> exact_initial_marking;
  std::optional<TokenMarking> exact_final_marking;
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  // Token replay's per-place silent-path tie order (see TokenReplayOptions).
  // PNML document entry points default it to the imported arc order.
  std::optional<std::vector<std::vector<std::size_t>>> place_outgoing_transition_order;
};

struct CompositeEvaluationResult {
  double fitness = 0.0;
  double precision = 0.0;
  double generalization = 0.0;
  double simplicity = 0.0;
  double metrics_average_weight = 0.0;
  double fscore = 0.0;
  double fitness_weight = 0.25;
  double precision_weight = 0.25;
  double simplicity_weight = 0.25;
  double generalization_weight = 0.25;
  bool computed = true;
  std::string error;
  // The composite evaluator performs one token replay and retains its full
  // result so callers do not need to run the replay a second time. `fitness`
  // remains the historical scalar score; `fitness_score` is an explicit alias
  // for serializers that expose the detailed result under `fitness`.
  ReplayFitnessResult fitness_details;
  double fitness_score = 0.0;
};

struct WeightedTrace {
  std::vector<std::string> activities;
  double weight = 0.0;
};
struct StochasticLanguage {
  std::vector<WeightedTrace> traces;
  double total_mass = 0.0;
};
using DistanceFunction = std::function<double(const std::vector<std::string>&,
                                              const std::vector<std::string>&)>;
// Callback receiving the one-character-per-activity strings used by the
// stochastic-language transport calculation. The union of both input
// languages is encoded in sorted activity order before the callback runs.
using EncodedDistanceFunction =
    std::function<double(const std::u32string&, const std::u32string&)>;
using Utf8EncodedDistanceFunction =
    std::function<double(const std::string&, const std::string&)>;
// EMD variants. PYEMD is the reference-compatible default. EXACT remains an
// explicit native compatibility extension and is never selected implicitly.
enum class EmdVariant {
  EXACT,
  PYEMD,
  kExact = EXACT,
  kPyemd = PYEMD,
};
struct EmdOptions {
  bool normalize_masses = true;  // default for unequal mass
  bool strict_equal_mass = false;
  DistanceFunction distance;
  // Appended to preserve positional initialization of the legacy options.
  EmdVariant variant = EmdVariant::PYEMD;
  // Selects the accelerated implementation when one is available. Native
  // builds retain the same deterministic transportation fallback either way.
  bool use_fast_emd = true;
  // Reference-style callback over encoded Unicode strings. encoded_distance
  // wins over string_distance when both are supplied.
  EncodedDistanceFunction encoded_distance;
  // Readable alias for callers porting a string-distance callback.
  EncodedDistanceFunction string_distance;
  // Optional UTF-8 callback extension for clients whose string-distance
  // implementation cannot consume UTF-32 code points directly.
  Utf8EncodedDistanceFunction utf8_string_distance;
};
struct EmdResult {
  double distance = std::numeric_limits<double>::quiet_NaN();
  bool computed = true;
  bool normalized = false;
  std::string error;
  // Appended to preserve positional initialization of the legacy result.
  EmdVariant variant = EmdVariant::PYEMD;
  bool use_fast_emd = true;
};

// One already-computed token replay record. Metrics use every supplied row;
// computed/exhausted/error are compatibility metadata, not row filters.
struct PrecomputedTokenReplay {
  bool computed = true;
  bool exhausted = false;
  bool trace_is_fit = false;
  double trace_fitness = 0.0;
  std::optional<TokenInteger> missing_tokens;
  std::optional<TokenInteger> consumed_tokens;
  std::optional<TokenInteger> remaining_tokens;
  std::optional<TokenInteger> produced_tokens;
  std::vector<std::uint64_t> transition_activations;
  std::string error;
  // Short aliases mirror the aggregate fields exposed by ConformanceResult.
  // An absent *_tokens value selects the short alias. An explicit zero (or
  // negative value) always retains precedence over the alias.
  TokenInteger missing = 0;
  TokenInteger consumed = 0;
  TokenInteger remaining = 0;
  TokenInteger produced = 0;
};

// One already-computed alignment replay record. `best_worst_cost` is the
// denominator used by the cost-based log fitness; a zero denominator follows
// the reference evaluator and yields a unit log fitness for nonempty
// computable input.
struct PrecomputedAlignmentReplay {
  bool computed = true;
  bool exhausted = false;
  bool trace_is_fit = false;
  double fitness = 0.0;
  double cost = 0.0;
  double best_worst_cost = 0.0;
  std::string error;
  // Common serialized spelling for the best-worst denominator. A zero
  // best_worst_cost is the unspecified sentinel when bwc is nonzero.
  double bwc = 0.0;
};

// Descriptive aliases for clients that name the source algorithm explicitly.
using TokenReplayEvaluationResult = PrecomputedTokenReplay;
using AlignmentReplayEvaluationResult = PrecomputedAlignmentReplay;

// A request-scoped container for the selected model-quality evaluation.  Only
// the result corresponding to the requested family is populated.
struct ModelEvaluation {
  std::string miner;
  std::optional<ReplayFitnessResult> replay_fitness;
  std::optional<PrecisionResult> precision;
  std::optional<GeneralizationResult> generalization;
  std::optional<SimplicityResult> simplicity;
  std::optional<CompositeEvaluationResult> composite;
};

[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventLog&, const PetriNet&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventLog&, const ProcessTree&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventLog&, const PetriNet&, ReplayFitnessVariant,
    const ReplayFitnessOptions&);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventLog&, const ProcessTree&, ReplayFitnessVariant,
    const ReplayFitnessOptions&);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventLog&, const PnmlDocument&,
    ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED,
    const ReplayFitnessOptions& = {});

// Flat event-data adapters. EventStream, EventTable, LazyEventTable, and the
// tagged EventDataView all preserve the grouped EventLog contract after
// deterministic case/activity selector projection.
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView&, const PnmlDocument&,
    ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED,
    const ReplayFitnessOptions& = {});
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView&, const PetriNet&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& = {});
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const EventDataView&, const ProcessTree&, ReplayFitnessVariant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& = {});

[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const EventStream& data, const PetriNet& net,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const EventTable& data, const PetriNet& net,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const LazyEventTable& data, const PetriNet& net,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const EventStream& data, const ProcessTree& tree,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const EventTable& data, const ProcessTree& tree,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness(
    const LazyEventTable& data, const ProcessTree& tree,
    ReplayFitnessVariant variant = ReplayFitnessVariant::AUTO,
    const ReplayFitnessOptions& options = {}) {
  return evaluate_replay_fitness(EventDataView(data), tree, variant, options);
}
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<PrecomputedTokenReplay>&,
    ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED);
// Aggregate full replay rows without narrowing signed/arbitrary-size counts.
// Trace fitness is used as supplied, including values outside [0, 1], NaN and
// infinities. Fit classification comes from trace_is_fit, independently.
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<TokenReplayTraceDiagnostics>&,
    ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const TokenReplayResult&,
    ReplayFitnessVariant = ReplayFitnessVariant::TOKEN_BASED);
[[nodiscard]] ReplayFitnessResult evaluate_replay_fitness(
    const std::vector<PrecomputedAlignmentReplay>&,
    ReplayFitnessVariant = ReplayFitnessVariant::ALIGNMENT_BASED);

[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness_from_token_results(
    const std::vector<PrecomputedTokenReplay>& results) {
  return evaluate_replay_fitness(results, ReplayFitnessVariant::TOKEN_BASED);
}
[[nodiscard]] inline ReplayFitnessResult evaluate_replay_fitness_from_alignment_results(
    const std::vector<PrecomputedAlignmentReplay>& results) {
  return evaluate_replay_fitness(results, ReplayFitnessVariant::ALIGNMENT_BASED);
}
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const PetriNet&, PrecisionVariant = PrecisionVariant::AUTO);
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const PetriNet&, PrecisionVariant, const PrecisionOptions&);
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const PnmlDocument&, PrecisionVariant,
    const PrecisionOptions& = {});
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventDataView&, const PnmlDocument&, PrecisionVariant,
    const PrecisionOptions& = {});
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventDataView&, const PetriNet&, PrecisionVariant = PrecisionVariant::AUTO,
    const PrecisionOptions& = {});
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventDataView&, const ProcessTree&, PrecisionVariant = PrecisionVariant::AUTO,
    const PrecisionOptions& = {});
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const EventStream& data, const PetriNet& net,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const EventTable& data, const PetriNet& net,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const LazyEventTable& data, const PetriNet& net,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const EventStream& data, const ProcessTree& tree,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const EventTable& data, const ProcessTree& tree,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline PrecisionResult evaluate_precision(
    const LazyEventTable& data, const ProcessTree& tree,
    PrecisionVariant variant = PrecisionVariant::AUTO,
    const PrecisionOptions& options = {}) {
  return evaluate_precision(EventDataView(data), tree, variant, options);
}
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const ProcessTree&, PrecisionVariant = PrecisionVariant::AUTO);
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const ProcessTree&, PrecisionVariant, const PrecisionOptions&);
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventLog&, const PetriNet&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN);
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventLog&, const ProcessTree&, GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN);
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventLog&, const PetriNet&, GeneralizationVariant,
    const GeneralizationOptions&);
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventLog&, const PnmlDocument&,
    GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& = {});
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventDataView&, const PnmlDocument&,
    GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& = {});
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventLog&, const ProcessTree&, GeneralizationVariant,
    const GeneralizationOptions&);
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventDataView&, const PetriNet&,
    GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& = {});
[[nodiscard]] GeneralizationResult evaluate_generalization(
    const EventDataView&, const ProcessTree&,
    GeneralizationVariant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& = {});
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const EventStream& data, const PetriNet& net,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const EventTable& data, const PetriNet& net,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const LazyEventTable& data, const PetriNet& net,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), net, variant, options);
}
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const EventStream& data, const ProcessTree& tree,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const EventTable& data, const ProcessTree& tree,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), tree, variant, options);
}
[[nodiscard]] inline GeneralizationResult evaluate_generalization(
    const LazyEventTable& data, const ProcessTree& tree,
    GeneralizationVariant variant = GeneralizationVariant::GENERALIZATION_TOKEN,
    const GeneralizationOptions& options = {}) {
  return evaluate_generalization(EventDataView(data), tree, variant, options);
}
// Default cyclomatic graph construction uses classic weighted arc behavior,
// including on reset/inhibitor nets. Other execution consumers retain arc types.
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const PetriNet&, SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});
// Full presentations preserve distinct arc occurrences and node identities.
// Cyclomatic reachability enables each input occurrence against its own weight,
// fires their combined consumption, and retains supplied marking/transition names.
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const PetriNetPresentation&,
    SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});
// Imported document arcs use the retained presentation for all variants.
// Default cyclomatic graph construction treats reset/inhibitor arcs as ordinary
// weighted arcs, as observed on the pinned public default. Stale metadata rejects.
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const PnmlDocument&,
    SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});

[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventLog&, const PetriNet&,
    const CompositeEvaluationOptions& = {});
// Uses executable replay metrics and retained document arc occurrences for
// simplicity, without repeating token replay or projecting away parallel arcs.
// Omitted option markings use complete document markings when present; explicit
// option vectors override them. Replay markings must fit the native int range.
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventLog&, const PnmlDocument&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventLog&, const ProcessTree&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventDataView&, const PetriNet&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventDataView&, const PnmlDocument&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventDataView&, const ProcessTree&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventStream& data, const PetriNet& net,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), net, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventTable& data, const PetriNet& net,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), net, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const LazyEventTable& data, const PetriNet& net,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), net, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventStream& data, const PnmlDocument& document,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), document, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventTable& data, const PnmlDocument& document,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), document, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const LazyEventTable& data, const PnmlDocument& document,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), document, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventStream& data, const ProcessTree& tree,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), tree, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const EventTable& data, const ProcessTree& tree,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), tree, options);
}
[[nodiscard]] inline CompositeEvaluationResult evaluate_composite(
    const LazyEventTable& data, const ProcessTree& tree,
    const CompositeEvaluationOptions& options = {}) {
  return evaluate_composite(EventDataView(data), tree, options);
}
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const ProcessTree&, SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});

[[nodiscard]] StochasticLanguage derive_stochastic_language(const EventLog&, bool normalize = false);
[[nodiscard]] EmdResult evaluate_emd(const StochasticLanguage&, const StochasticLanguage&, const EmdOptions& = {});
[[nodiscard]] double earth_movers_distance(const StochasticLanguage&, const StochasticLanguage&, const EmdOptions& = {});
[[nodiscard]] double normalized_levenshtein_distance(const std::vector<std::string>&,
                                                     const std::vector<std::string>&);
[[nodiscard]] double normalized_levenshtein_distance(const std::u32string&,
                                                     const std::u32string&);

// Short aliases used by clients that model evaluator modules as
// free functions rather than selecting an evaluator object.
inline ReplayFitnessResult replay_fitness(const EventLog& l, const PetriNet& n,
                                          ReplayFitnessVariant v = ReplayFitnessVariant::AUTO) {
  return evaluate_replay_fitness(l, n, v);
}

inline ReplayFitnessResult replay_fitness(const EventDataView& d,
                                          const PetriNet& n,
                                          ReplayFitnessVariant v = ReplayFitnessVariant::AUTO,
                                          const ReplayFitnessOptions& o = {}) {
  return evaluate_replay_fitness(d, n, v, o);
}
inline ReplayFitnessResult replay_fitness(const EventLog& l, const PetriNet& n,
                                          ReplayFitnessVariant v,
                                          const ReplayFitnessOptions& o) {
  return evaluate_replay_fitness(l, n, v, o);
}
inline PrecisionResult precision(const EventLog& l, const PetriNet& n,
                                 PrecisionVariant v = PrecisionVariant::AUTO) {
  return evaluate_precision(l, n, v);
}
inline PrecisionResult precision(const EventLog& l, const PetriNet& n,
                                 PrecisionVariant v,
                                 const PrecisionOptions& o) {
  return evaluate_precision(l, n, v, o);
}
inline GeneralizationResult generalization(const EventLog& l, const PetriNet& n,
                                           GeneralizationVariant v = GeneralizationVariant::GENERALIZATION_TOKEN) {
  return evaluate_generalization(l, n, v);
}
inline GeneralizationResult generalization(const EventLog& l, const PetriNet& n,
                                           GeneralizationVariant v,
                                           const GeneralizationOptions& o) {
  return evaluate_generalization(l, n, v, o);
}
inline SimplicityResult simplicity(const PetriNet& n,
                                  SimplicityVariant v = SimplicityVariant::ARC_DEGREE,
                                  const SimplicityOptions& o = {}) {
  return evaluate_simplicity(n, v, o);
}

}  // namespace minera
