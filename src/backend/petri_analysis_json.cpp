#include "minera/petri_analysis_json.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {

std::string quote(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const auto character : value) {
    switch (character) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(character) < 0x20)
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(character))
              << std::dec << std::setfill(' ');
        else
          out << character;
    }
  }
  out << '"';
  return out.str();
}

template <typename T>
std::string numbers(const std::vector<T>& values) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << values[i];
  }
  out << ']';
  return out.str();
}

std::string markings(const std::vector<Marking>& values) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << numbers(values[i]);
  }
  out << ']';
  return out.str();
}

std::string optional_index_vectors(
    const std::optional<std::vector<std::vector<std::size_t>>>& values) {
  if (!values) return "null";
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values->size(); ++i) {
    if (i != 0) out << ',';
    out << numbers((*values)[i]);
  }
  out << ']';
  return out.str();
}

std::string optional_number(const std::optional<std::size_t>& value) {
  return value ? std::to_string(*value) : "null";
}

std::string optional_bool(const std::optional<bool>& value) {
  if (!value) return "null";
  return *value ? "true" : "false";
}

std::string index_vectors(
    const std::vector<std::vector<std::size_t>>& values) {
  std::ostringstream out;
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << numbers(values[i]);
  }
  out << ']';
  return out.str();
}

std::string doubles(const std::vector<double>& values) {
  std::ostringstream out;
  out << std::setprecision(17) << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << values[i];
  }
  out << ']';
  return out.str();
}

void validate_finite(const std::vector<double>& values, const char* name) {
  if (std::any_of(values.begin(), values.end(),
                  [](double value) { return !std::isfinite(value); }))
    throw std::invalid_argument(std::string(name) + " must be finite");
}

void validate_equation_result(const MarkingEquationResult& result) {
  if (!std::isfinite(result.objective) || !std::isfinite(result.lower_bound) ||
      result.objective < 0.0 || result.lower_bound < 0.0 ||
      result.objective + 1e-7 < result.lower_bound)
    throw std::invalid_argument("marking equation objective is invalid");
  validate_finite(result.transition_counts, "marking equation transition counts");
  validate_finite(result.residual, "marking equation residual");
  if (!result.residual.empty() && result.computed && result.feasible &&
      result.transition_counts.empty())
    throw std::invalid_argument("marking equation dimensions are inconsistent");
  if (std::any_of(result.activated_transitions.begin(),
                  result.activated_transitions.end(), [&](std::size_t index) {
                    return index >= result.transition_counts.size();
                  }) ||
      std::any_of(result.firing_sequence.begin(), result.firing_sequence.end(),
                  [&](std::size_t index) {
                    return index >= result.transition_counts.size();
                  }))
    throw std::invalid_argument("marking equation transition index is invalid");
}

void validate_equation_result(const ExtendedMarkingEquationResult& result) {
  if (!std::isfinite(result.objective) || !std::isfinite(result.lower_bound) ||
      result.objective < 0.0 || result.lower_bound < 0.0 ||
      result.objective + 1e-7 < result.lower_bound)
    throw std::invalid_argument("extended marking equation objective is invalid");
  validate_finite(result.transition_counts,
                  "extended marking equation transition counts");
  if (result.segment_count != result.retained_splits.size() + 1 ||
      !std::is_sorted(result.retained_splits.begin(),
                      result.retained_splits.end()) ||
      std::adjacent_find(result.retained_splits.begin(),
                         result.retained_splits.end()) !=
          result.retained_splits.end())
    throw std::invalid_argument("extended marking equation splits are inconsistent");
  if (std::any_of(result.firing_sequence.begin(), result.firing_sequence.end(),
                  [&](std::size_t index) {
                    return index >= result.transition_counts.size();
                  }))
    throw std::invalid_argument(
        "extended marking equation transition index is invalid");
}

const char* status(SoundnessStatus value) {
  switch (value) {
    case SoundnessStatus::SOUND: return "sound";
    case SoundnessStatus::UNSOUND: return "unsound";
    case SoundnessStatus::UNKNOWN: return "unknown";
  }
  return "unknown";
}

const char* soundness_variant(SoundnessVariant value) {
  switch (value) {
    case SoundnessVariant::kSoundness: return "soundness";
    case SoundnessVariant::kWoflan: return "woflan";
  }
  return "soundness";
}

std::string workflow(const WorkflowNetResult& result) {
  std::ostringstream out;
  out << "{\"variant\":\"petri_net\",\"valid\":"
      << (result.valid ? "true" : "false")
      << ",\"source_candidates\":" << numbers(result.source_candidates)
      << ",\"sink_candidates\":" << numbers(result.sink_candidates)
      << ",\"offending_places\":" << numbers(result.offending_places)
      << ",\"offending_transitions\":"
      << numbers(result.offending_transitions)
      << ",\"unreachable_places\":" << numbers(result.unreachable_places)
      << ",\"unreachable_transitions\":"
      << numbers(result.unreachable_transitions)
      << ",\"reason\":" << quote(result.reason) << '}';
  return out.str();
}

void validate_workflow_result(const WorkflowNetResult& result) {
  const auto sorted_unique = [](const auto& values) {
    return std::is_sorted(values.begin(), values.end()) &&
           std::adjacent_find(values.begin(), values.end()) == values.end();
  };
  if (!sorted_unique(result.source_candidates) ||
      !sorted_unique(result.sink_candidates) ||
      !sorted_unique(result.offending_places) ||
      !sorted_unique(result.offending_transitions) ||
      !sorted_unique(result.unreachable_places) ||
      !sorted_unique(result.unreachable_transitions))
    throw std::invalid_argument(
        "workflow-net diagnostics must be sorted and unique");
  if (result.valid &&
      (result.source_candidates.size() != 1 ||
       result.sink_candidates.size() != 1 || !result.offending_places.empty() ||
       !result.offending_transitions.empty() ||
       !result.unreachable_places.empty() ||
       !result.unreachable_transitions.empty() || !result.reason.empty()))
    throw std::invalid_argument("valid workflow-net result is inconsistent");
  if (!result.valid && result.reason.empty())
    throw std::invalid_argument("invalid workflow-net result requires a reason");
}

void validate_soundness_result(const SoundnessResult& result) {
  validate_workflow_result(result.workflow);
  if (result.status == SoundnessStatus::SOUND &&
      (!result.computed || result.exhausted || !result.workflow.valid ||
       !result.bounded || !result.final_reachable ||
       !result.option_to_complete || !result.proper_completion ||
       !result.dead_tasks.empty()))
    throw std::invalid_argument("sound result is internally inconsistent");
  if (result.returned_asap && !result.computed)
    throw std::invalid_argument("early soundness result must be computed");
}

}  // namespace

std::string petri_analysis_json(const MarkingEquationResult& result) {
  validate_equation_result(result);
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"marking_equation\",\"variant\":\"classic\","
         "\"computed\":" << (result.computed ? "true" : "false")
      << ",\"feasible\":" << (result.feasible ? "true" : "false")
      << ",\"exhausted\":" << (result.exhausted ? "true" : "false")
      << ",\"solver_nodes\":" << result.solver_nodes
      << ",\"objective\":" << result.objective
      << ",\"lower_bound\":" << result.lower_bound
      << ",\"transition_counts\":" << doubles(result.transition_counts)
      << ",\"residual\":" << doubles(result.residual)
      << ",\"activated_transitions\":"
      << numbers(result.activated_transitions)
      << ",\"firing_sequence\":" << numbers(result.firing_sequence)
      << ",\"recovery_attempted\":"
      << (result.recovery_attempted ? "true" : "false")
      << ",\"reaches_final\":" << (result.reaches_final ? "true" : "false")
      << ",\"error\":" << quote(result.error) << '}';
  return out.str();
}

std::string petri_analysis_json(const ExtendedMarkingEquationResult& result) {
  validate_equation_result(result);
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"extended_marking_equation\",\"variant\":\"classic\","
         "\"computed\":" << (result.computed ? "true" : "false")
      << ",\"feasible\":" << (result.feasible ? "true" : "false")
      << ",\"exhausted\":" << (result.exhausted ? "true" : "false")
      << ",\"solver_nodes\":" << result.solver_nodes
      << ",\"objective\":" << result.objective
      << ",\"lower_bound\":" << result.lower_bound
      << ",\"retained_splits\":" << numbers(result.retained_splits)
      << ",\"segment_count\":" << result.segment_count
      << ",\"transition_counts\":" << doubles(result.transition_counts)
      << ",\"firing_sequence\":" << numbers(result.firing_sequence)
      << ",\"reaches_final\":" << (result.reaches_final ? "true" : "false")
      << ",\"error\":" << quote(result.error) << '}';
  return out.str();
}

std::string petri_analysis_json(const WorkflowNetResult& result) {
  validate_workflow_result(result);
  return "{\"family\":\"workflow_net\",\"result\":" +
         workflow(result) + '}';
}

std::string petri_analysis_json(const SoundnessResult& result) {
  validate_soundness_result(result);
  std::ostringstream out;
  const char* family = result.variant == SoundnessVariant::kWoflan
                           ? "woflan"
                           : "soundness";
  out << std::setprecision(17)
      << "{\"family\":\"" << family << "\",\"variant\":\""
      << soundness_variant(result.variant) << "\",\"status\":\""
      << status(result.status)
      << "\",\"computed\":" << (result.computed ? "true" : "false")
      << ",\"exhausted\":" << (result.exhausted ? "true" : "false")
      << ",\"returned_asap\":"
      << (result.returned_asap ? "true" : "false")
      << ",\"workflow\":" << workflow(result.workflow)
      << ",\"bounded\":" << (result.bounded ? "true" : "false")
      << ",\"final_reachable\":"
      << (result.final_reachable ? "true" : "false")
      << ",\"option_to_complete\":"
      << (result.option_to_complete ? "true" : "false")
      << ",\"proper_completion\":"
      << (result.proper_completion ? "true" : "false")
      << ",\"dead_transitions\":" << numbers(result.dead_transitions)
      << ",\"dead_tasks\":" << numbers(result.dead_tasks)
      << ",\"deadlocks\":" << markings(result.deadlocks)
      << ",\"deadlock_witnesses\":[";
  for (std::size_t i = 0; i < result.deadlock_witnesses.size(); ++i) {
    if (i != 0) out << ',';
    out << numbers(result.deadlock_witnesses[i]);
  }
  out
      << "]"
      << ",\"improper_markings\":" << markings(result.improper_markings)
      << ",\"improper_witnesses\":[";
  for (std::size_t i = 0; i < result.improper_witnesses.size(); ++i) {
    if (i != 0) out << ',';
    out << numbers(result.improper_witnesses[i]);
  }
  out
      << "]"
      << ",\"completion_witness\":" << numbers(result.completion_witness)
      << ",\"locking_witness\":" << numbers(result.locking_witness)
      << ",\"locking_scenarios\":[";
  for (std::size_t i = 0; i < result.locking_scenarios.size(); ++i) {
    if (i != 0) out << ',';
    out << numbers(result.locking_scenarios[i]);
  }
  out
      << "]"
      << ",\"unbounded_witness\":" << numbers(result.unbounded_witness)
      << ",\"reachable_marking_count\":" << result.reachable_marking_count
      << ",\"coverability_node_count\":" << result.coverability_node_count
      << ",\"uncovered_places\":" << numbers(result.uncovered_places)
      << ",\"diagnostic_messages\":[";
  for (std::size_t i = 0; i < result.diagnostic_messages.size(); ++i) {
    if (i != 0) out << ',';
    out << quote(result.diagnostic_messages[i]);
  }
  out << "]"
      << ",\"place_invariant_basis\":[";
  for (std::size_t i = 0; i < result.place_invariant_basis.size(); ++i) {
    if (i != 0) out << ',';
    out << doubles(result.place_invariant_basis[i]);
  }
  out << "]"
      << ",\"uniform_place_invariants\":[";
  for (std::size_t i = 0; i < result.uniform_place_invariants.size(); ++i) {
    if (i != 0) out << ',';
    out << doubles(result.uniform_place_invariants[i]);
  }
  out << "]"
      << ",\"weighted_place_invariants\":[";
  for (std::size_t i = 0; i < result.weighted_place_invariants.size(); ++i) {
    if (i != 0) out << ',';
    out << doubles(result.weighted_place_invariants[i]);
  }
  out << "]"
      << ",\"uncovered_places_uniform\":"
      << numbers(result.uncovered_places_uniform)
      << ",\"uniform_invariant_search_exhausted\":"
      << (result.uniform_invariant_search_exhausted ? "true" : "false")
      << ",\"uncovered_places_weighted\":"
      << numbers(result.uncovered_places_weighted)
      << ",\"uncovered_places_s_component\":"
      << numbers(result.uncovered_places_s_component)
      << ",\"left\":" << optional_bool(result.left)
      << ",\"coverability_markings\":"
      << markings(result.coverability_markings)
      << ",\"coverability_edges\":"
      << index_vectors(result.coverability_edges)
      << ",\"restricted_coverability_tree_exhausted\":"
      << (result.restricted_coverability_tree_exhausted ? "true" : "false")
      << ",\"restricted_coverability_markings\":"
      << markings(result.restricted_coverability_markings)
      << ",\"restricted_coverability_edges\":"
      << index_vectors(result.restricted_coverability_edges)
      << ",\"reachability_markings\":"
      << markings(result.reachability_markings)
      << ",\"reachability_edges\":"
      << index_vectors(result.reachability_edges)
      << ",\"s_components\":" << optional_index_vectors(result.s_components)
      << ",\"handled_pairs\":" << optional_index_vectors(result.handled_pairs)
      << ",\"not_well_handled_pairs\":"
      << optional_index_vectors(result.not_well_handled_pairs)
      << ",\"structural_node_labels\":[";
  for (std::size_t i = 0; i < result.structural_node_labels.size(); ++i) {
    if (i != 0) out << ',';
    out << quote(result.structural_node_labels[i]);
  }
  out
      << "]"
      << ",\"short_circuit_valid\":"
      << (result.short_circuit_valid ? "true" : "false")
      << ",\"short_circuit_strongly_connected\":"
      << (result.short_circuit_strongly_connected ? "true" : "false")
      << ",\"short_circuit_transition\":"
      << optional_number(result.short_circuit_transition)
      << ",\"short_circuit_reachability_exhausted\":"
      << (result.short_circuit_reachability_exhausted ? "true" : "false")
      << ",\"short_circuit_reachability_strongly_connected\":"
      << (result.short_circuit_reachability_strongly_connected ? "true"
                                                               : "false")
      << ",\"short_circuit_diagnostic_messages\":[";
  for (std::size_t i = 0;
       i < result.short_circuit_diagnostic_messages.size(); ++i) {
    if (i != 0) out << ',';
    out << quote(result.short_circuit_diagnostic_messages[i]);
  }
  out << "]"
      << ",\"short_circuit_reachability_markings\":"
      << markings(result.short_circuit_reachability_markings)
      << ",\"short_circuit_reachability_edges\":"
      << index_vectors(result.short_circuit_reachability_edges)
      << ",\"error\":" << quote(result.error) << '}';
  return out.str();
}

}  // namespace minera
