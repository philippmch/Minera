#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/decomposed_alignment.hpp"

namespace minera {

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
      AlignmentVariant::kDijkstraLessMemory, AlignmentCostProfile::kStandard};
  // Engaging this option selects decomposed recomposition instead of the
  // ordinary Petri-net alignment engine.
  std::optional<DecomposedAlignmentOptions> decomposed_alignment_options;
};

struct PrecisionResult {
  PrecisionVariant variant = PrecisionVariant::ETCONFORMANCE_TOKEN;
  double precision = 1.0;
  bool computed = true;
  bool exhausted = false;
  std::string error;
  ConformanceResult token_result;
  AlignmentResult alignment_result;
};
struct PrecisionOptions {
  bool use_task_ids = false;
  // Maximum number of markings/product states explored by exact precision
  // searches.  Zero is invalid and reports an exhausted computation.
  std::size_t max_states = 100000;
  std::string activity_attribute = "concept:name";
  TokenReplayVariant token_replay_variant = TokenReplayVariant::kForward;
};

struct GeneralizationOptions {
  std::string activity_attribute = "concept:name";
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
  double arc_degree_k = 2.0;
};

struct SimplicityResult {
  SimplicityVariant variant = SimplicityVariant::ARC_DEGREE;
  double simplicity = 0.0;
  double complexity = 0.0;
  double value = 0.0;
  std::size_t vertices = 0;
  std::size_t edges = 0;
  std::size_t strongly_connected_components = 0;
  bool computed = true;
  bool exhausted = false;
  std::string error;
};

struct CompositeEvaluationOptions {
  double fitness_weight = 0.25;
  double precision_weight = 0.25;
  double simplicity_weight = 0.25;
  double generalization_weight = 0.25;
  std::string activity_attribute = "concept:name";
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
// EMD variants. EXACT is the historical native implementation. PYEMD uses
// the same dependency-free transportation solver with the standard normalized
// trace distance, while retaining the legacy variant as the default.
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
  EmdVariant variant = EmdVariant::EXACT;
  // Selects the accelerated implementation when one is available. Native
  // builds retain the same deterministic transportation fallback either way.
  bool use_fast_emd = true;
};
struct EmdResult {
  double distance = std::numeric_limits<double>::quiet_NaN();
  bool computed = true;
  bool normalized = false;
  std::string error;
  // Appended to preserve positional initialization of the legacy result.
  EmdVariant variant = EmdVariant::EXACT;
  bool use_fast_emd = true;
};

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
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const PetriNet&, PrecisionVariant = PrecisionVariant::AUTO);
[[nodiscard]] PrecisionResult evaluate_precision(
    const EventLog&, const PetriNet&, PrecisionVariant, const PrecisionOptions&);
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
    const EventLog&, const ProcessTree&, GeneralizationVariant,
    const GeneralizationOptions&);
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const PetriNet&, SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});

[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventLog&, const PetriNet&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] CompositeEvaluationResult evaluate_composite(
    const EventLog&, const ProcessTree&,
    const CompositeEvaluationOptions& = {});
[[nodiscard]] SimplicityResult evaluate_simplicity(
    const ProcessTree&, SimplicityVariant = SimplicityVariant::ARC_DEGREE,
    const SimplicityOptions& = {});

[[nodiscard]] StochasticLanguage derive_stochastic_language(const EventLog&, bool normalize = false);
[[nodiscard]] EmdResult evaluate_emd(const StochasticLanguage&, const StochasticLanguage&, const EmdOptions& = {});
[[nodiscard]] double earth_movers_distance(const StochasticLanguage&, const StochasticLanguage&, const EmdOptions& = {});
[[nodiscard]] double normalized_levenshtein_distance(const std::vector<std::string>&,
                                                     const std::vector<std::string>&);

// Short aliases used by clients that model evaluator modules as
// free functions rather than selecting an evaluator object.
inline ReplayFitnessResult replay_fitness(const EventLog& l, const PetriNet& n,
                                          ReplayFitnessVariant v = ReplayFitnessVariant::AUTO) {
  return evaluate_replay_fitness(l, n, v);
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
