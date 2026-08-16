#include "minera/petri_analysis_json.hpp"

#include <iomanip>
#include <sstream>

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

}  // namespace

std::string petri_analysis_json(const MarkingEquationResult& result) {
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"marking_equation\",\"variant\":\"classic\","
         "\"computed\":" << (result.computed ? "true" : "false")
      << ",\"feasible\":" << (result.feasible ? "true" : "false")
      << ",\"exhausted\":" << (result.exhausted ? "true" : "false")
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
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"extended_marking_equation\",\"variant\":\"classic\","
         "\"computed\":" << (result.computed ? "true" : "false")
      << ",\"feasible\":" << (result.feasible ? "true" : "false")
      << ",\"exhausted\":" << (result.exhausted ? "true" : "false")
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
  return "{\"family\":\"workflow_net\",\"result\":" +
         workflow(result) + '}';
}

std::string petri_analysis_json(const SoundnessResult& result) {
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
      << ",\"uncovered_places_weighted\":"
      << numbers(result.uncovered_places_weighted)
      << ",\"uncovered_places_s_component\":"
      << numbers(result.uncovered_places_s_component)
      << ",\"coverability_markings\":"
      << markings(result.coverability_markings)
      << ",\"coverability_edges\":"
      << index_vectors(result.coverability_edges)
      << ",\"reachability_markings\":"
      << markings(result.reachability_markings)
      << ",\"reachability_edges\":"
      << index_vectors(result.reachability_edges)
      << ",\"s_components\":" << optional_index_vectors(result.s_components)
      << ",\"handled_pairs\":" << optional_index_vectors(result.handled_pairs)
      << ",\"not_well_handled_pairs\":"
      << optional_index_vectors(result.handled_pairs)
      << ",\"structural_node_labels\":[";
  for (std::size_t i = 0; i < result.structural_node_labels.size(); ++i) {
    if (i != 0) out << ',';
    out << quote(result.structural_node_labels[i]);
  }
  out
      << "]"
      << ",\"error\":" << quote(result.error) << '}';
  return out.str();
}

}  // namespace minera
