#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {

using Marking = std::vector<std::int64_t>;

struct WorkflowMarkings {
  Marking initial;
  Marking final;
};

[[nodiscard]] WorkflowMarkings default_workflow_markings(const PetriNet& net);

enum class MarkingEquationVariant { CLASSIC };

struct MarkingEquationOptions {
  Marking initial_marking;
  Marking final_marking;
  std::vector<double> transition_costs;
  std::size_t realizability_limit = 200000;
};

struct MarkingEquationResult {
  MarkingEquationVariant variant = MarkingEquationVariant::CLASSIC;
  bool computed = false;
  bool feasible = false;
  bool exhausted = false;
  double objective = 0.0;
  double lower_bound = 0.0;
  std::vector<double> transition_counts;
  std::vector<double> residual;
  std::vector<std::size_t> activated_transitions;
  std::vector<std::size_t> firing_sequence;
  bool recovery_attempted = false;
  bool reaches_final = false;
  std::string error;
};

[[nodiscard]] MarkingEquationResult solve_marking_equation(
    const PetriNet& net, const MarkingEquationOptions& options = {});

enum class ExtendedMarkingEquationVariant { CLASSIC };

struct ExtendedMarkingEquationOptions {
  Marking initial_marking;
  Marking final_marking;
  std::vector<double> model_transition_costs;
  std::vector<double> synchronous_transition_costs;
  std::vector<double> log_move_costs;
  std::vector<std::size_t> split_indices;
  std::size_t max_k_value = 5;
  bool full_bootstrap_required = true;
  std::size_t realizability_limit = 200000;
};

struct ExtendedMarkingEquationResult {
  ExtendedMarkingEquationVariant variant = ExtendedMarkingEquationVariant::CLASSIC;
  bool computed = false;
  bool feasible = false;
  bool exhausted = false;
  double objective = 0.0;
  double lower_bound = 0.0;
  std::vector<std::size_t> retained_splits;
  std::size_t segment_count = 0;
  std::vector<double> transition_counts;
  std::vector<std::size_t> firing_sequence;
  bool reaches_final = false;
  std::string error;
};

[[nodiscard]] ExtendedMarkingEquationResult solve_extended_marking_equation(
    const std::vector<std::string>& trace, const PetriNet& net,
    const ExtendedMarkingEquationOptions& options = {});

enum class WorkflowNetVariant { PETRI_NET };

struct WorkflowNetResult {
  WorkflowNetVariant variant = WorkflowNetVariant::PETRI_NET;
  bool valid = false;
  std::vector<std::size_t> source_candidates;
  std::vector<std::size_t> sink_candidates;
  std::vector<std::size_t> offending_places;
  std::vector<std::size_t> offending_transitions;
  std::vector<std::size_t> unreachable_places;
  std::vector<std::size_t> unreachable_transitions;
  std::string reason;
};

[[nodiscard]] WorkflowNetResult check_workflow_net(const PetriNet& net);
[[nodiscard]] bool is_workflow_net(const PetriNet& net);

enum class SoundnessStatus { SOUND, UNSOUND, UNKNOWN };

struct SoundnessOptions {
  Marking initial_marking;
  Marking final_marking;
  std::size_t max_states = 200000;
  bool diagnostics = false;
};

struct SoundnessResult {
  SoundnessStatus status = SoundnessStatus::UNKNOWN;
  bool computed = false;
  bool exhausted = false;
  WorkflowNetResult workflow;
  bool bounded = false;
  bool final_reachable = false;
  bool option_to_complete = false;
  bool proper_completion = false;
  std::vector<std::size_t> dead_transitions;
  // Alias used by structural soundness diagnostics: transition ids that are
  // never enabled from the resolved initial marking.
  std::vector<std::size_t> dead_tasks;
  std::vector<Marking> deadlocks;
  std::vector<std::vector<std::size_t>> deadlock_witnesses;
  std::vector<Marking> improper_markings;
  std::vector<std::vector<std::size_t>> improper_witnesses;
  std::vector<std::size_t> completion_witness;
  std::vector<std::size_t> locking_witness;
  // Deterministic shortest firing paths to bounded markings that cannot reach
  // the resolved final marking. The single locking_witness is retained as a
  // compact compatibility field containing the first such path.
  std::vector<std::vector<std::size_t>> locking_scenarios;
  std::vector<std::size_t> unbounded_witness;
  std::size_t reachable_marking_count = 0;
  std::size_t coverability_node_count = 0;
  std::vector<std::string> diagnostic_messages;
  // Deterministic non-negative P-invariants, normalized so their largest
  // coefficient is one.  A place is covered when it has a positive
  // coefficient in at least one vector.
  std::vector<std::vector<double>> place_invariant_basis;
  std::vector<std::size_t> uncovered_places;
  // Uniform invariants keep only the support of each positive invariant.
  // Weighted invariants retain relative positive coefficients, normalized so
  // the smallest positive coefficient is one.  Both families are emitted in
  // deterministic order and are useful for independent coverage diagnostics.
  std::vector<std::vector<double>> uniform_place_invariants;
  std::vector<std::size_t> uncovered_places_uniform;
  std::vector<std::vector<double>> weighted_place_invariants;
  std::vector<std::size_t> uncovered_places_weighted;
  std::vector<std::size_t> uncovered_places_s_component;
  // Compact graph projections used by soundness diagnostics.  Coverability
  // markings use -1 for an omega coordinate; graph edges are
  // [source_node, target_node, transition_id].
  std::vector<Marking> coverability_markings;
  std::vector<std::vector<std::size_t>> coverability_edges;
  std::vector<Marking> reachability_markings;
  std::vector<std::vector<std::size_t>> reachability_edges;
  // For workflow nets, supports of the positive P-invariants above.  The
  // optional is disengaged when the input is not a structurally valid
  // workflow net; an engaged empty value means no positive S-component was
  // found.
  std::optional<std::vector<std::vector<std::size_t>>> s_components;
  // Each pair is [source_node, target_node] in a deterministic node-split
  // graph: element n has input node 2*n and output node 2*n+1. The vector is
  // engaged for a structurally valid workflow net, including when empty.
  std::optional<std::vector<std::vector<std::size_t>>> handled_pairs;
  // Human-readable labels for the node coordinates used by handled_pairs.
  std::vector<std::string> structural_node_labels;
  std::string error;
};

[[nodiscard]] SoundnessResult analyze_soundness(
    const PetriNet& net, const SoundnessOptions& options = {});

}  // namespace minera
