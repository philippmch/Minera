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
  std::vector<Marking> deadlocks;
  std::vector<Marking> improper_markings;
  std::vector<std::size_t> locking_witness;
  std::vector<std::size_t> unbounded_witness;
  std::size_t reachable_marking_count = 0;
  std::size_t coverability_node_count = 0;
  std::vector<std::string> diagnostic_messages;
  std::vector<std::vector<double>> place_invariant_basis;
  std::vector<std::size_t> uncovered_places;
  std::optional<std::vector<std::vector<std::size_t>>> s_components;
  std::optional<std::vector<std::vector<std::size_t>>> handled_pairs;
  std::string error;
};

[[nodiscard]] SoundnessResult analyze_soundness(
    const PetriNet& net, const SoundnessOptions& options = {});

}  // namespace minera
