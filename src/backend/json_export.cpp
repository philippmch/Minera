#include "minera/json_export.hpp"
#include "minera/log_conversion.hpp"
#include "minera/pnml.hpp"
#include "root_cause_trace_key.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <limits>
#include <locale>
#include <numeric>
#include <optional>
#include <sstream>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "minera/version.hpp"

namespace minera {
namespace {

void write_attribute_detail(std::ostream& out, const EventAttribute& attribute);

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
        } else {
          out << c;
        }
    }
  }
  out << '"';
}

std::string_view operator_name(ProcessTreeOperator op) {
  switch (op) {
    case ProcessTreeOperator::kActivity: return "activity";
    case ProcessTreeOperator::kTau: return "tau";
    case ProcessTreeOperator::kSequence: return "sequence";
    case ProcessTreeOperator::kExclusive: return "exclusive";
    case ProcessTreeOperator::kInclusiveChoice: return "inclusive_or";
    case ProcessTreeOperator::kParallel: return "parallel";
    case ProcessTreeOperator::kInterleaving: return "interleaving";
    case ProcessTreeOperator::kLoop: return "loop";
  }
  return "tau";
}

void write_process_tree_node(std::ostream& out,
                             const ProcessTreeNode& node,
                             const ProcessTree& tree,
                             const DirectlyFollowsGraph& graph) {
  out << "{\"operator\": \"" << operator_name(node.op) << '"';
  if (node.op == ProcessTreeOperator::kActivity) {
    // Re-resolve the activity id against the graph so it matches the ids used
    // by the rest of the document regardless of interning order.
    const auto& name = tree.activities()[node.activity];
    const auto id = graph.activity_id(name);
    out << ", \"activity\": " << (id ? *id : node.activity);
  }
  if (!node.children.empty()) {
    out << ", \"children\": [";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      write_process_tree_node(out, node.children[i], tree, graph);
      out << (i + 1 < node.children.size() ? ", " : "");
    }
    out << ']';
  }
  out << '}';
}

std::string_view process_tree_alignment_variant_name(ProcessTreeAlignmentVariant variant) {
  switch (variant) {
    case ProcessTreeAlignmentVariant::kSearchGraphPt: return "SEARCH_GRAPH_PT";
    case ProcessTreeAlignmentVariant::kDynamicProgramming: return "DYNAMIC_PROGRAMMING";
    case ProcessTreeAlignmentVariant::kMilp: return "MILP";
    case ProcessTreeAlignmentVariant::kApproximatedOriginal: return "APPROXIMATED_ORIGINAL";
  }
  return "SEARCH_GRAPH_PT";
}

std::string_view process_tree_alignment_move_type_name(ProcessTreeAlignmentMoveType type) {
  switch (type) {
    case ProcessTreeAlignmentMoveType::kSynchronous: return "sync";
    case ProcessTreeAlignmentMoveType::kLog: return "log";
    case ProcessTreeAlignmentMoveType::kModel: return "model";
  }
  return "sync";
}

void write_process_tree_alignment(std::ostream& out,
                                  const ProcessTreeAlignmentResult& result) {
  out << "{\"family\": \"PROCESS_TREE\", \"variant\": ";
  write_json_string(out, process_tree_alignment_variant_name(result.variant));
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"completed\": " << (result.completed ? "true" : "false")
      << ", \"timed_out\": " << (result.timed_out ? "true" : "false")
      << ", \"exhausted\": " << (result.exhausted ? "true" : "false")
      << ", \"total_cost\": " << result.total_cost
      << ", \"fitness\": " << result.fitness
      << ", \"diagnostics\": {\"visited_states\": "
      << result.diagnostics.visited_states
      << ", \"queued_states\": " << result.diagnostics.queued_states
      << ", \"traversed_arcs\": " << result.diagnostics.traversed_arcs
      << ", \"lp_solved\": " << result.diagnostics.lp_solved
      << ", \"traces_aligned\": " << result.diagnostics.traces_aligned
      << ", \"message\": ";
  write_json_string(out, result.diagnostics.message);
  out << "}, \"traces\": [";
  for (std::size_t i = 0; i < result.traces.size(); ++i) {
    const auto& trace = result.traces[i];
    if (i != 0) out << ", ";
    out << "{\"trace\": [";
    for (std::size_t j = 0; j < trace.trace.size(); ++j) {
      if (j != 0) out << ", ";
      write_json_string(out, trace.trace[j]);
    }
    out << "], \"cost\": " << trace.cost
        << ", \"fitness\": " << trace.fitness
        << ", \"completed\": " << (trace.completed ? "true" : "false")
        << ", \"timed_out\": " << (trace.timed_out ? "true" : "false")
        << ", \"moves\": [";
    for (std::size_t j = 0; j < trace.moves.size(); ++j) {
      const auto& move = trace.moves[j];
      if (j != 0) out << ", ";
      out << "{\"type\": ";
      write_json_string(out, process_tree_alignment_move_type_name(move.type));
      out << ", \"log\": ";
      write_json_string(out, move.log_activity);
      out << ", \"model\": ";
      write_json_string(out, move.model_activity);
      out << ", \"activity\": ";
      write_json_string(out, move.activity);
      out << '}';
    }
    out << "]}";
  }
  out << "]}";
}

void write_powl_node(std::ostream& out, const PowlNode& node,
                     const DirectlyFollowsGraph& graph) {
  switch (node.kind()) {
    case PowlNodeKind::kTransition: {
      const auto activity = graph.activity_id(node.label());
      if (!activity) {
        throw std::invalid_argument(
            "POWL transition activity is not registered: " + node.label());
      }
      out << "{\"kind\": \"transition\", \"activity\": " << *activity
          << ", \"frequency\": {\"minimum\": " << node.frequency().minimum
          << ", \"maximum\": ";
      if (node.frequency().maximum) out << *node.frequency().maximum;
      else out << "null";
      out << "}}";
      return;
    }
    case PowlNodeKind::kSilent:
      out << "{\"kind\": \"silent\"}";
      return;
    case PowlNodeKind::kXor:
      out << "{\"kind\": \"xor\"";
      break;
    case PowlNodeKind::kLoop:
      out << "{\"kind\": \"loop\"";
      break;
    case PowlNodeKind::kStrictPartialOrder:
      out << "{\"kind\": \"strict_partial_order\"";
      break;
  }
  out << ", \"children\": [";
  for (std::size_t i = 0; i < node.children().size(); ++i) {
    write_powl_node(out, node.children()[i], graph);
    out << (i + 1 < node.children().size() ? ", " : "");
  }
  out << ']';
  if (node.kind() == PowlNodeKind::kStrictPartialOrder) {
    out << ", \"order\": [";
    for (std::size_t i = 0; i < node.order().size(); ++i) {
      out << "{\"source\": " << node.order()[i].first
          << ", \"target\": " << node.order()[i].second << '}'
          << (i + 1 < node.order().size() ? ", " : "");
    }
    out << ']';
  }
  out << '}';
}

void write_log_skeleton_pairs(std::ostream& out,
                              const std::vector<LogSkeletonPair>& pairs,
                              const DirectlyFollowsGraph& graph) {
  out << '[';
  for (std::size_t i = 0; i < pairs.size(); ++i) {
    const auto source = graph.activity_id(pairs[i].source);
    const auto target = graph.activity_id(pairs[i].target);
    if (!source || !target) {
      throw std::invalid_argument("log skeleton activity is not registered");
    }
    out << "{\"source\": " << *source << ", \"target\": " << *target
        << '}' << (i + 1 < pairs.size() ? ", " : "");
  }
  out << ']';
}

void write_log_skeleton_model(std::ostream& out,
                              const LogSkeletonModel& model,
                              const DirectlyFollowsGraph& graph) {
  out << "{\"equivalence\": ";
  write_log_skeleton_pairs(out, model.equivalence, graph);
  out << ", \"always_after\": ";
  write_log_skeleton_pairs(out, model.always_after, graph);
  out << ", \"always_before\": ";
  write_log_skeleton_pairs(out, model.always_before, graph);
  out << ", \"never_together\": ";
  write_log_skeleton_pairs(out, model.never_together, graph);
  out << ", \"directly_follows\": ";
  write_log_skeleton_pairs(out, model.directly_follows, graph);
  out << ", \"activ_freq\": {";
  std::size_t emitted = 0;
  for (const auto& [activity, frequencies] : model.activ_freq) {
    const auto id = graph.activity_id(activity);
    if (!id) {
      throw std::invalid_argument("log skeleton activity is not registered: " +
                                  activity);
    }
    if (emitted++ != 0) out << ", ";
    write_json_string(out, std::to_string(*id));
    out << ": [";
    for (std::size_t i = 0; i < frequencies.size(); ++i) {
      out << frequencies[i] << (i + 1 < frequencies.size() ? ", " : "");
    }
    out << ']';
  }
  out << "}}";
}

void write_log_skeleton_conformance(
    std::ostream& out, const EventLog& log,
    const std::vector<LogSkeletonConformance>& conformance,
    const DirectlyFollowsGraph& graph) {
  out << '[';
  for (std::size_t i = 0; i < conformance.size(); ++i) {
    const auto& result = conformance[i];
    out << "{\"case_id\": ";
    write_json_string(out, log.traces[i].case_id);
    out << ", \"deviations\": [";
    for (std::size_t j = 0; j < result.deviations.size(); ++j) {
      const auto& deviation = result.deviations[j];
      out << "{\"constraint\": ";
      write_json_string(out, deviation.constraint);
      if (deviation.constraint == "activ_freq") {
        const auto id = graph.activity_id(deviation.activity);
        if (!id) {
          throw std::invalid_argument(
              "log skeleton activity is not registered: " +
              deviation.activity);
        }
        out << ", \"activity\": " << *id
            << ", \"observed_frequency\": "
            << deviation.observed_frequency << ", \"allowed_frequencies\": [";
        for (std::size_t k = 0; k < deviation.allowed_frequencies.size(); ++k) {
          out << deviation.allowed_frequencies[k]
              << (k + 1 < deviation.allowed_frequencies.size() ? ", " : "");
        }
        out << ']';
      } else {
        const auto source = graph.activity_id(deviation.relation.source);
        const auto target = graph.activity_id(deviation.relation.target);
        if (!source || !target) {
          throw std::invalid_argument("log skeleton activity is not registered");
        }
        out << ", \"source\": " << *source << ", \"target\": "
            << *target;
      }
      out << '}' << (j + 1 < result.deviations.size() ? ", " : "");
    }
    out << "], \"no_dev_total\": " << result.no_dev_total
        << ", \"no_constr_total\": " << result.no_constr_total
        << ", \"dev_fitness\": " << result.dev_fitness
        << ", \"is_fit\": " << (result.is_fit ? "true" : "false") << '}'
        << (i + 1 < conformance.size() ? ", " : "");
  }
  out << ']';
}

bool is_log_skeleton_pair_constraint(std::string_view constraint) {
  return constraint == "equivalence" || constraint == "always_after" ||
         constraint == "always_before" || constraint == "never_together" ||
         constraint == "directly_follows";
}

void validate_log_skeleton_pair(const LogSkeletonPair& pair,
                                const DirectlyFollowsGraph& graph) {
  if (pair.source.empty() || pair.target.empty() ||
      !graph.activity_id(pair.source) || !graph.activity_id(pair.target)) {
    throw std::invalid_argument("log skeleton pair activity is not registered");
  }
}

void validate_log_skeleton_model(const LogSkeletonModel& model,
                                 const DirectlyFollowsGraph& graph) {
  const auto validate_pairs = [&](const std::vector<LogSkeletonPair>& pairs) {
    for (const auto& pair : pairs) validate_log_skeleton_pair(pair, graph);
  };
  validate_pairs(model.equivalence);
  validate_pairs(model.always_after);
  validate_pairs(model.always_before);
  validate_pairs(model.never_together);
  validate_pairs(model.directly_follows);
  for (const auto& [activity, frequencies] : model.activ_freq) {
    if (activity.empty() || !graph.activity_id(activity)) {
      throw std::invalid_argument(
          "log skeleton activity is not registered: " + activity);
    }
    (void)frequencies;
  }
}

void validate_log_skeleton_conformance(
    const EventLog& log, const std::vector<LogSkeletonConformance>& conformance,
    const DirectlyFollowsGraph& graph) {
  if (conformance.size() != log.traces.size()) {
    throw std::invalid_argument(
        "log skeleton conformance must contain one result per trace");
  }
  for (const auto& result : conformance) {
    for (const auto& deviation : result.deviations) {
      if (deviation.constraint == "activ_freq") {
        if (deviation.activity.empty() ||
            !graph.activity_id(deviation.activity)) {
          throw std::invalid_argument(
              "log skeleton activity is not registered: " +
              deviation.activity);
        }
      } else if (is_log_skeleton_pair_constraint(deviation.constraint)) {
        validate_log_skeleton_pair(deviation.relation, graph);
      } else {
        throw std::invalid_argument(
            "unknown log skeleton deviation constraint: " +
            deviation.constraint);
      }
    }
  }
}

constexpr std::string_view kDeclareFamilies[] = {
    kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
    kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
    kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
    kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
    kDeclareChainSuccession, kDeclareAbsence, kDeclareCoexistence,
    kDeclareNonCoexistence, kDeclareNonSuccession,
    kDeclareNonChainSuccession};

bool declare_family_is_unary(std::string_view family) {
  return family == kDeclareExistence || family == kDeclareExactlyOne ||
         family == kDeclareInit || family == kDeclareAbsence;
}

bool declare_family_known(std::string_view family) {
  return std::find(std::begin(kDeclareFamilies), std::end(kDeclareFamilies),
                   family) != std::end(kDeclareFamilies);
}

void validate_declare_model(const DeclareModel& model,
                            const DirectlyFollowsGraph& graph) {
  for (const auto& [family, rules] : model.rules) {
    if (!declare_family_known(family)) {
      throw std::invalid_argument("unknown Declare template family: " + family);
    }
    const bool unary = declare_family_is_unary(family);
    for (const auto& rule : rules) {
      if (!graph.activity_id(rule.first)) {
        throw std::invalid_argument("Declare rule activity is not registered");
      }
      if (unary) {
        if (rule.binary) {
          throw std::invalid_argument("Declare unary rule has binary fields");
        }
      } else {
        if (!rule.binary || !graph.activity_id(rule.second)) {
          throw std::invalid_argument(
              "Declare binary rule has invalid activity fields");
        }
      }
    }
  }
}

void validate_declare_conformance(
    const EventLog& log, const std::vector<DeclareConformance>& conformance,
    const DirectlyFollowsGraph& graph) {
  if (conformance.size() != log.traces.size()) {
    throw std::invalid_argument(
        "Declare conformance must contain one result per trace");
  }
  for (const auto& result : conformance) {
    if (!std::isfinite(result.dev_fitness)) {
      throw std::invalid_argument("Declare conformance fitness must be finite");
    }
    for (const auto& deviation : result.deviations) {
      if (!declare_family_known(deviation.template_name)) {
        throw std::invalid_argument("unknown Declare deviation template: " +
                                    deviation.template_name);
      }
      const bool unary = declare_family_is_unary(deviation.template_name);
      if (!graph.activity_id(deviation.first)) {
        throw std::invalid_argument(
            "Declare deviation activity is not registered");
      }
      if (unary) {
        if (deviation.binary) {
          throw std::invalid_argument(
              "Declare unary deviation has binary fields");
        }
      } else if (!deviation.binary || !graph.activity_id(deviation.second)) {
        throw std::invalid_argument(
            "Declare binary deviation has invalid activity fields");
      }
    }
  }
}

void write_declare_model(std::ostream& out, const DeclareModel& model,
                         const DirectlyFollowsGraph& graph) {
  out << '{';
  for (std::size_t i = 0; i < std::size(kDeclareFamilies); ++i) {
    if (i != 0) out << ", ";
    const auto family = kDeclareFamilies[i];
    write_json_string(out, family);
    out << ": [";
    const auto found = model.rules.find(std::string(family));
    if (found != model.rules.end()) {
      std::vector<const DeclareConstraint*> ordered;
      ordered.reserve(found->second.size());
      for (const auto& rule : found->second) ordered.push_back(&rule);
      std::stable_sort(ordered.begin(), ordered.end(),
                       [](const auto* lhs, const auto* rhs) {
                         return std::tie(lhs->first, lhs->second, lhs->binary,
                                         lhs->support, lhs->confidence) <
                                std::tie(rhs->first, rhs->second, rhs->binary,
                                         rhs->support, rhs->confidence);
                       });
      for (std::size_t j = 0; j < ordered.size(); ++j) {
        const auto& rule = *ordered[j];
        if (j != 0) out << ", ";
        out << '{';
        if (rule.binary) {
          out << "\"source\": " << *graph.activity_id(rule.first)
              << ", \"target\": " << *graph.activity_id(rule.second);
        } else {
          out << "\"activity\": " << *graph.activity_id(rule.first);
        }
        out << ", \"support\": " << rule.support
            << ", \"confidence\": " << rule.confidence << '}';
      }
    }
    out << ']';
  }
  out << '}';
}

void write_declare_conformance(
    std::ostream& out, const EventLog& log,
    const std::vector<DeclareConformance>& conformance,
    const DirectlyFollowsGraph& graph) {
  out << '[';
  for (std::size_t i = 0; i < conformance.size(); ++i) {
    const auto& result = conformance[i];
    if (i != 0) out << ", ";
    out << "{\"trace_index\": " << i << ", \"case_id\": ";
    write_json_string(out, log.traces[i].case_id);
    out << ", \"deviations\": [";
    for (std::size_t j = 0; j < result.deviations.size(); ++j) {
      const auto& deviation = result.deviations[j];
      if (j != 0) out << ", ";
      out << "{\"template\": ";
      write_json_string(out, deviation.template_name);
      if (deviation.binary) {
        out << ", \"source\": " << *graph.activity_id(deviation.first)
            << ", \"target\": " << *graph.activity_id(deviation.second);
      } else {
        out << ", \"activity\": " << *graph.activity_id(deviation.first);
      }
      out << '}';
    }
    out << "], \"no_constr_total\": " << result.no_constr_total
        << ", \"no_dev_total\": " << result.no_dev_total
        << ", \"dev_fitness\": " << result.dev_fitness
        << ", \"is_fit\": " << (result.is_fit ? "true" : "false") << '}';
  }
  out << ']';
}

std::size_t footprint_activity_id(const DirectlyFollowsGraph& graph,
                                  const std::string& activity) {
  const auto id = graph.activity_id(activity);
  if (!id) {
    throw std::invalid_argument("footprint activity is not registered: " +
                                activity);
  }
  return *id;
}

void validate_footprint_pair(const FootprintPair& pair,
                             const DirectlyFollowsGraph& graph) {
  (void)footprint_activity_id(graph, pair.first);
  (void)footprint_activity_id(graph, pair.second);
}

void validate_footprint(const Footprint& footprint,
                        const DirectlyFollowsGraph& graph) {
  for (const auto& [pair, count] : footprint.dfg) {
    validate_footprint_pair(pair, graph);
    (void)count;
  }
  for (const auto& pair : footprint.sequence) {
    validate_footprint_pair(pair, graph);
  }
  for (const auto& pair : footprint.parallel) {
    validate_footprint_pair(pair, graph);
  }
  const auto validate_activities = [&](const auto& activities) {
    for (const auto& activity : activities) {
      (void)footprint_activity_id(graph, activity);
    }
  };
  validate_activities(footprint.start_activities);
  validate_activities(footprint.end_activities);
  validate_activities(footprint.activities);
  if (footprint.activities_always_happening) {
    validate_activities(*footprint.activities_always_happening);
  }
  if (footprint.trace) validate_activities(*footprint.trace);
}

void validate_footprints_extensive(
    const FootprintsExtensiveResult& result,
    const DirectlyFollowsGraph& graph) {
  for (const auto& pair : result.footprints) {
    validate_footprint_pair(pair, graph);
  }
  for (const auto& activity : result.start_activities) {
    (void)footprint_activity_id(graph, activity);
  }
  for (const auto& activity : result.end_activities) {
    (void)footprint_activity_id(graph, activity);
  }
  for (const auto& activity : result.activities_always_happening) {
    (void)footprint_activity_id(graph, activity);
  }
}

void validate_footprints_analysis(const FootprintsAnalysis& analysis,
                                  const EventLog& log,
                                  const DirectlyFollowsGraph& graph) {
  if (analysis.trace_case_ids.size() != 0 &&
      analysis.trace_case_ids.size() != log.traces.size()) {
    throw std::invalid_argument(
        "footprints diagnostic case ids must contain one value per trace");
  }
  if (analysis.activity_attribute.empty() ||
      analysis.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "footprints diagnostic selectors must not be empty");
  }
  if (analysis.entire_event_log) {
    validate_footprint(*analysis.entire_event_log, graph);
  }
  if (analysis.entire_dataframe) {
    validate_footprint(*analysis.entire_dataframe, graph);
  }
  if (analysis.polars_lazyframes) {
    validate_footprint(*analysis.polars_lazyframes, graph);
  }
  if (analysis.trace_by_trace) {
    if (analysis.trace_by_trace->size() != log.traces.size()) {
      throw std::invalid_argument(
          "trace-by-trace footprints must contain one result per trace");
    }
    for (const auto& footprint : *analysis.trace_by_trace) {
      validate_footprint(footprint, graph);
    }
  }
  if (analysis.dfg) validate_footprint(*analysis.dfg, graph);
  if (analysis.process_tree) validate_footprint(*analysis.process_tree, graph);
  if (analysis.powl) validate_footprint(*analysis.powl, graph);
  if (analysis.petri_reach_graph) {
    validate_footprint(*analysis.petri_reach_graph, graph);
  }
  validate_footprint(analysis.model_footprint, graph);
  static const std::set<std::string> models = {
      "process_tree", "powl", "inductive", "alpha", "heuristic", "dfg"};
  if (!models.contains(analysis.model)) {
    throw std::invalid_argument("unknown footprints model: " + analysis.model);
  }
  if (analysis.log_model) {
    for (const auto& pair : *analysis.log_model) {
      validate_footprint_pair(pair, graph);
    }
  }
  if (analysis.log_extensive) {
    validate_footprints_extensive(*analysis.log_extensive, graph);
  }
  if (analysis.trace_extensive) {
    if (analysis.trace_extensive->size() != log.traces.size()) {
      throw std::invalid_argument(
          "trace-extensive footprints must contain one result per trace");
    }
    for (const auto& result : *analysis.trace_extensive) {
      validate_footprints_extensive(result, graph);
    }
  }
  if (!std::isfinite(analysis.precision) ||
      !std::isfinite(analysis.log_fitness) ||
      !std::isfinite(analysis.perc_fit_traces)) {
    throw std::invalid_argument("footprints metrics must be finite");
  }
}

std::vector<std::size_t> resolve_footprint_activities(
    const std::set<std::string>& activities,
    const DirectlyFollowsGraph& graph) {
  std::vector<std::size_t> resolved;
  resolved.reserve(activities.size());
  for (const auto& activity : activities) {
    resolved.push_back(footprint_activity_id(graph, activity));
  }
  std::sort(resolved.begin(), resolved.end());
  return resolved;
}

void write_footprint_ids(std::ostream& out,
                         const std::set<std::string>& activities,
                         const DirectlyFollowsGraph& graph) {
  const auto resolved = resolve_footprint_activities(activities, graph);
  out << '[';
  for (std::size_t i = 0; i < resolved.size(); ++i) {
    out << resolved[i] << (i + 1 < resolved.size() ? ", " : "");
  }
  out << ']';
}

void write_footprint_pairs(std::ostream& out,
                           const std::set<FootprintPair>& pairs,
                           const DirectlyFollowsGraph& graph) {
  std::vector<std::pair<std::size_t, std::size_t>> resolved;
  resolved.reserve(pairs.size());
  for (const auto& pair : pairs) {
    resolved.emplace_back(footprint_activity_id(graph, pair.first),
                          footprint_activity_id(graph, pair.second));
  }
  std::sort(resolved.begin(), resolved.end());
  out << '[';
  for (std::size_t i = 0; i < resolved.size(); ++i) {
    out << "{\"source\": " << resolved[i].first
        << ", \"target\": " << resolved[i].second << '}'
        << (i + 1 < resolved.size() ? ", " : "");
  }
  out << ']';
}

void write_footprint(std::ostream& out, const Footprint& footprint,
                     const DirectlyFollowsGraph& graph) {
  out << '{';
  bool emitted = false;
  const auto field = [&](std::string_view name) {
    if (emitted) out << ", ";
    emitted = true;
    write_json_string(out, name);
    out << ": ";
  };
  if (footprint.dfg_present) {
    field("dfg");
    struct ResolvedDfg {
      std::size_t source;
      std::size_t target;
      std::uint64_t count;
    };
    std::vector<ResolvedDfg> resolved;
    resolved.reserve(footprint.dfg.size());
    for (const auto& [pair, count] : footprint.dfg) {
      resolved.push_back({footprint_activity_id(graph, pair.first),
                          footprint_activity_id(graph, pair.second), count});
    }
    std::sort(resolved.begin(), resolved.end(),
              [](const auto& lhs, const auto& rhs) {
                return std::tie(lhs.source, lhs.target, lhs.count) <
                       std::tie(rhs.source, rhs.target, rhs.count);
              });
    out << '[';
    for (std::size_t i = 0; i < resolved.size(); ++i) {
      out << "{\"source\": " << resolved[i].source
          << ", \"target\": " << resolved[i].target
          << ", \"count\": " << resolved[i].count << '}'
          << (i + 1 < resolved.size() ? ", " : "");
    }
    out << ']';
  }
  field("sequence");
  write_footprint_pairs(out, footprint.sequence, graph);
  field("parallel");
  write_footprint_pairs(out, footprint.parallel, graph);
  field("atomic_interleaving");
  write_footprint_pairs(out, footprint.atomic_interleaving, graph);
  if (footprint.start_activities_present) {
    field("start_activities");
    write_footprint_ids(out, footprint.start_activities, graph);
  }
  if (footprint.end_activities_present) {
    field("end_activities");
    write_footprint_ids(out, footprint.end_activities, graph);
  }
  field("activities");
  write_footprint_ids(out, footprint.activities, graph);
  if (footprint.skippable) {
    field("skippable");
    out << (*footprint.skippable ? "true" : "false");
  }
  if (footprint.activities_always_happening) {
    field("activities_always_happening");
    write_footprint_ids(out, *footprint.activities_always_happening, graph);
  }
  if (footprint.min_trace_length_present) {
    field("min_trace_length");
    if (footprint.min_trace_length) {
      out << *footprint.min_trace_length;
    } else {
      out << "null";
    }
  }
  if (footprint.max_trace_length_without_loops) {
    field("max_trace_length_without_loops");
    out << *footprint.max_trace_length_without_loops;
  }
  if (footprint.trace) {
    field("trace");
    out << '[';
    for (std::size_t i = 0; i < footprint.trace->size(); ++i) {
      out << footprint_activity_id(graph, (*footprint.trace)[i])
          << (i + 1 < footprint.trace->size() ? ", " : "");
    }
    out << ']';
  }
  out << '}';
}

void write_footprints_extensive_fields(
    std::ostream& out, const FootprintsExtensiveResult& result,
    const DirectlyFollowsGraph& graph) {
  out << "\"footprints\": ";
  write_footprint_pairs(out, result.footprints, graph);
  out << ", \"start_activities\": ";
  write_footprint_ids(out, result.start_activities, graph);
  out << ", \"end_activities\": ";
  write_footprint_ids(out, result.end_activities, graph);
  out << ", \"activities_always_happening\": ";
  write_footprint_ids(out, result.activities_always_happening, graph);
  out << ", \"min_length_fit\": "
      << (result.min_length_fit ? "true" : "false")
      << ", \"is_footprints_fit\": "
      << (result.is_footprints_fit ? "true" : "false")
      << ", \"is_fit\": " << (result.is_fit ? "true" : "false");
}

void write_footprints_extensive(std::ostream& out,
                                const FootprintsExtensiveResult& result,
                                const DirectlyFollowsGraph& graph) {
  out << '{';
  write_footprints_extensive_fields(out, result, graph);
  out << '}';
}

void write_footprints_analysis(std::ostream& out,
                               const FootprintsAnalysis& analysis,
                               const EventLog& log,
                               const DirectlyFollowsGraph& graph) {
  const auto diagnostic_case_id = [&](std::size_t index) -> const std::string& {
    if (analysis.trace_case_ids.size() == log.traces.size())
      return analysis.trace_case_ids[index];
    return log.traces[index].case_id;
  };
  out << "{\"discovery\": {";
  bool emitted = false;
  const auto discovery = [&](std::string_view name,
                             const Footprint& footprint) {
    if (emitted) out << ", ";
    emitted = true;
    write_json_string(out, name);
    out << ": ";
    write_footprint(out, footprint, graph);
  };
  if (analysis.entire_event_log) {
    discovery("entire_event_log", *analysis.entire_event_log);
  }
  if (analysis.entire_dataframe) {
    discovery("entire_dataframe", *analysis.entire_dataframe);
  }
  if (analysis.polars_lazyframes) {
    discovery("polars_lazyframes", *analysis.polars_lazyframes);
  }
  if (analysis.trace_by_trace) {
    if (emitted) out << ", ";
    emitted = true;
    out << "\"trace_by_trace\": [";
    for (std::size_t i = 0; i < analysis.trace_by_trace->size(); ++i) {
      out << "{\"trace_index\": " << i << ", \"case_id\": ";
      write_json_string(out, diagnostic_case_id(i));
      out << ", \"footprint\": ";
      write_footprint(out, (*analysis.trace_by_trace)[i], graph);
      out << '}' << (i + 1 < analysis.trace_by_trace->size() ? ", " : "");
    }
    out << ']';
  }
  if (analysis.dfg) discovery("dfg", *analysis.dfg);
  if (analysis.process_tree) discovery("process_tree", *analysis.process_tree);
  if (analysis.powl) discovery("powl", *analysis.powl);
  if (analysis.petri_reach_graph) {
    discovery("petri_reach_graph", *analysis.petri_reach_graph);
  }
  out << "}, \"conformance\": {\"model\": ";
  write_json_string(out, analysis.model);
  if (analysis.log_model) {
    out << ", \"log_model\": {\"strict\": "
        << (analysis.strict ? "true" : "false")
        << ", \"violations\": ";
    write_footprint_pairs(out, *analysis.log_model, graph);
    out << ", \"is_fit\": "
        << (analysis.log_model->empty() ? "true" : "false") << '}';
  }
  if (analysis.log_extensive) {
    out << ", \"log_extensive\": ";
    write_footprints_extensive(out, *analysis.log_extensive, graph);
  }
  if (analysis.trace_extensive) {
    out << ", \"trace_extensive\": [";
    for (std::size_t i = 0; i < analysis.trace_extensive->size(); ++i) {
      out << "{\"trace_index\": " << i << ", \"case_id\": ";
      write_json_string(out, diagnostic_case_id(i));
      out << ", ";
      write_footprints_extensive_fields(
          out, (*analysis.trace_extensive)[i], graph);
      out << '}' << (i + 1 < analysis.trace_extensive->size() ? ", " : "");
    }
    out << ']';
  }
  out << "}, \"metrics\": {\"precision\": " << analysis.precision
      << ", \"log_fitness\": " << analysis.log_fitness
      << ", \"perc_fit_traces\": " << analysis.perc_fit_traces << '}';
  if (analysis.activity_attribute != "concept:name") {
    out << ", \"activity_attribute\": ";
    write_json_string(out, analysis.activity_attribute);
  }
  if (analysis.case_id_attribute != "case:concept:name") {
    out << ", \"case_id_attribute\": ";
    write_json_string(out, analysis.case_id_attribute);
  }
  out << '}';
}

void write_place_list(std::ostream& out, const std::vector<std::size_t>& places) {
  out << '[';
  for (std::size_t i = 0; i < places.size(); ++i) {
    out << places[i] << (i + 1 < places.size() ? ", " : "");
  }
  out << ']';
}

void write_petri_net(std::ostream& out,
                     const PetriNet& net,
                     const DirectlyFollowsGraph& graph) {
  out << "{\"place_count\": " << net.place_count()
      << ", \"source\": " << net.source() << ", \"sink\": " << net.sink()
      << ", \"transitions\": [";
  const auto& transitions = net.transitions();
  for (std::size_t i = 0; i < transitions.size(); ++i) {
    const auto& transition = transitions[i];
    out << "{\"activity\": ";
    if (transition.silent()) {
      out << "null";
    } else {
      // Re-resolve through the activity name so the id matches the rest of the
      // document regardless of the net's own interning order.
      const auto& name = net.activities()[*transition.activity];
      const auto id = graph.activity_id(name);
      out << (id ? *id : *transition.activity);
    }
    out << ", \"inputs\": ";
    write_place_list(out, transition.inputs);
    out << ", \"outputs\": ";
    write_place_list(out, transition.outputs);
    out << ", \"reset_inputs\": ";
    write_place_list(out, transition.reset_inputs);
    out << ", \"inhibitor_inputs\": ";
    write_place_list(out, transition.inhibitor_inputs);
    out << '}' << (i + 1 < transitions.size() ? ", " : "");
  }
  out << "]}";
}

void write_heuristic_performance_summary(
    std::ostream& out, const HeuristicPerformanceSummary& summary) {
  // Match PM4Py's public all-aggregation object keys.  The native scalar
  // representation remains available separately for compatibility callers.
  out << "{\"median\": " << summary.median
      << ", \"min\": " << summary.minimum
      << ", \"max\": " << summary.maximum
      << ", \"stdev\": " << summary.stdev
      << ", \"sum\": " << summary.sum
      << ", \"mean\": " << summary.mean << '}';
}

[[noreturn]] void throw_non_finite_heuristic_performance_value() {
  throw std::invalid_argument(
      "heuristic performance values must contain only finite numbers");
}

void validate_heuristic_structured_value(
    const HeuristicStructuredValue& value) {
  std::visit(
      [&](const auto& selected) {
        using Value = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<Value, double>) {
          if (!std::isfinite(selected))
            throw_non_finite_heuristic_performance_value();
        } else if constexpr (std::is_same_v<
                                 Value, HeuristicStructuredValue::Array>) {
          for (const auto& item : selected)
            validate_heuristic_structured_value(item);
        } else if constexpr (std::is_same_v<
                                 Value, HeuristicStructuredValue::Object>) {
          for (const auto& [_, item] : selected)
            validate_heuristic_structured_value(item);
        }
      },
      value.value);
}

void validate_heuristic_performance_value(
    const HeuristicPerformanceValue& value) {
  std::visit(
      [](const auto& selected) {
        using Value = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<Value, double>) {
          if (!std::isfinite(selected))
            throw_non_finite_heuristic_performance_value();
        } else if constexpr (std::is_same_v<Value, std::vector<double>>) {
          for (const double item : selected) {
            if (!std::isfinite(item))
              throw_non_finite_heuristic_performance_value();
          }
        } else if constexpr (std::is_same_v<Value,
                                            HeuristicPerformanceSummary>) {
          for (const double item : {selected.mean, selected.median,
                                    selected.minimum, selected.maximum,
                                    selected.stdev, selected.sum}) {
            if (!std::isfinite(item))
              throw_non_finite_heuristic_performance_value();
          }
        } else {
          validate_heuristic_structured_value(selected);
        }
      },
      value);
}

void write_heuristic_structured_value(
    std::ostream& out, const HeuristicStructuredValue& value) {
  std::visit(
      [&](const auto& selected) {
        using Value = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          out << "null";
        } else if constexpr (std::is_same_v<Value, bool>) {
          out << (selected ? "true" : "false");
        } else if constexpr (std::is_same_v<Value, std::int64_t> ||
                             std::is_same_v<Value, std::uint64_t> ||
                             std::is_same_v<Value, double>) {
          out << selected;
        } else if constexpr (std::is_same_v<Value, std::string>) {
          write_json_string(out, selected);
        } else if constexpr (std::is_same_v<
                                 Value, HeuristicStructuredValue::Array>) {
          out << '[';
          for (std::size_t i = 0; i < selected.size(); ++i) {
            if (i != 0) out << ", ";
            write_heuristic_structured_value(out, selected[i]);
          }
          out << ']';
        } else {
          out << '{';
          std::size_t i = 0;
          for (const auto& [key, item] : selected) {
            if (i != 0) out << ", ";
            write_json_string(out, key);
            out << ": ";
            write_heuristic_structured_value(out, item);
            ++i;
          }
          out << '}';
        }
      },
      value.value);
}

void write_heuristic_performance_value(
    std::ostream& out, const HeuristicPerformanceValue& value) {
  validate_heuristic_performance_value(value);
  std::visit(
      [&](const auto& selected) {
        using Value = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<Value, double>) {
          out << selected;
        } else if constexpr (std::is_same_v<Value, std::vector<double>>) {
          out << '[';
          for (std::size_t i = 0; i < selected.size(); ++i)
            out << selected[i] << (i + 1 < selected.size() ? ", " : "");
          out << ']';
        } else if constexpr (std::is_same_v<Value,
                                            HeuristicPerformanceSummary>) {
          write_heuristic_performance_summary(out, selected);
        } else {
          write_heuristic_structured_value(out, selected);
        }
      },
      value);
}

void write_heuristics_net(std::ostream& out,
                          const HeuristicsNet& net,
                          const DirectlyFollowsGraph& graph) {
  struct RestorePrecision {
    std::ostream& stream;
    std::streamsize precision;
    ~RestorePrecision() { stream.precision(precision); }
  } restore_precision{
      out, out.precision(std::numeric_limits<double>::max_digits10)};
  // Validate every typed performance payload before emitting the first byte of
  // this net.  This keeps non-finite values from producing invalid JSON even
  // when they are nested inside an already-aggregated DFG object or a
  // representation retained on an edge/relation statistic.
  if (net.performance_values) {
    for (const auto& [_, value] : *net.performance_values)
      validate_heuristic_performance_value(value);
  }
  for (const auto& edge : net.edges) {
    if (edge.representation_value)
      validate_heuristic_performance_value(*edge.representation_value);
  }
  for (const auto& stat : net.relation_statistics) {
    if (stat.representation_value)
      validate_heuristic_performance_value(*stat.representation_value);
  }
  // PLUSPLUS can retain raw statistics for activities filtered out of the
  // native node table.  The exported local-id table must still identify those
  // name-based records, while graph ids and metadata keep their native shape.
  std::vector<std::string> export_activities = net.activities;
  std::unordered_map<std::string, std::size_t> export_ids;
  if (net.activity_ids_are_local) {
    for (std::size_t i = 0; i < export_activities.size(); ++i)
      export_ids.emplace(export_activities[i], i);
    const auto append_name = [&](const std::string& name) {
      const auto result = export_ids.emplace(name, export_activities.size());
      if (result.second) export_activities.push_back(name);
    };
    for (const auto& stat : net.raw_activity_statistics)
      append_name(stat.activity);
    for (const auto& stat : net.raw_concurrency_statistics) {
      append_name(stat.first);
      append_name(stat.second);
    }
    const auto append_dfg_names = [&](const HeuristicFrequencyDfg& dfg) {
      for (const auto& [pair, _] : dfg) {
        append_name(pair.first);
        append_name(pair.second);
      }
    };
    append_dfg_names(net.raw_dfg);
    append_dfg_names(net.cleaned_dfg);
    append_dfg_names(net.window_two_dfg);
    for (const auto& [triple, _] : net.frequency_triples) {
      const auto& [first, middle, last] = triple;
      append_name(first);
      append_name(middle);
      append_name(last);
    }
    if (net.performance_values) {
      for (const auto& [pair, _] : *net.performance_values) {
        append_name(pair.first);
        append_name(pair.second);
      }
    }
  }
  // Canonical nets share the document-wide activity ids; nets built from a
  // generic activity field keep a self-contained local id space.
  const auto resolve = [&](std::size_t local) {
    if (net.activity_ids_are_local) return local;
    if (local >= net.activities.size()) return local;
    const auto id = graph.activity_id(net.activities[local]);
    return id ? *id : local;
  };
  const auto resolve_name = [&](const std::string& name) {
    if (net.activity_ids_are_local) {
      return export_ids.at(name);
    }
    const auto id = graph.activity_id(name);
    return id ? *id : std::size_t{0};
  };
  const auto write_ids = [&](const std::vector<std::size_t>& locals) {
    out << '[';
    for (std::size_t i = 0; i < locals.size(); ++i) {
      out << resolve(locals[i]) << (i + 1 < locals.size() ? ", " : "");
    }
    out << ']';
  };

  out << "{\"variant\": \""
      << (net.variant == HeuristicMinerVariant::kPlusPlus ? "plusplus"
                                                          : "classic")
      << "\", \"decoration\": \""
      << (net.decoration == HeuristicNetDecoration::kPerformance
              ? "performance"
              : "frequency")
      << "\", \"performance_aggregation\": \"";
  switch (net.performance_aggregation) {
    case HeuristicPerformanceAggregation::kMean: out << "mean"; break;
    case HeuristicPerformanceAggregation::kMedian: out << "median"; break;
    case HeuristicPerformanceAggregation::kMinimum: out << "min"; break;
    case HeuristicPerformanceAggregation::kMaximum: out << "max"; break;
    case HeuristicPerformanceAggregation::kStandardDeviation:
      out << "stdev";
      break;
    case HeuristicPerformanceAggregation::kSum: out << "sum"; break;
    case HeuristicPerformanceAggregation::kRawValues:
      out << "raw_values";
      break;
    case HeuristicPerformanceAggregation::kAll: out << "all"; break;
  }
  out << "\", \"activity_attribute\": ";
  write_json_string(out, net.activity_attribute.empty()
                             ? "concept:name"
                             : net.activity_attribute);
  out << ", \"timestamp_attribute\": ";
  write_json_string(out, net.timestamp_attribute.empty()
                             ? "time:timestamp"
                             : net.timestamp_attribute);
  out << ", \"activity_ids_are_local\": "
      << (net.activity_ids_are_local ? "true" : "false")
      << ", \"activities\": [";
  for (std::size_t i = 0; i < export_activities.size(); ++i) {
    write_json_string(out, export_activities[i]);
    if (i + 1 < export_activities.size()) out << ", ";
  }
  out << "], \"edges\": [";
  for (std::size_t i = 0; i < net.edges.size(); ++i) {
    const auto& edge = net.edges[i];
    out << "{\"source\": " << resolve(edge.source)
        << ", \"target\": " << resolve(edge.target)
        << ", \"dependency\": " << edge.dependency
        << ", \"frequency\": " << edge.frequency
        << ", \"representation\": ";
    if (edge.representation_value)
      write_heuristic_performance_value(out, *edge.representation_value);
    else
      out << edge.representation;
    out << ", \"length_two_loop\": "
        << (edge.length_two_loop ? "true" : "false") << '}'
        << (i + 1 < net.edges.size() ? ", " : "");
  }
  if (net.performance_values) {
    out << "], \"performance_dfg\": [";
    std::size_t index = 0;
    for (const auto& [pair, value] : *net.performance_values) {
      out << "{\"source\": " << resolve_name(pair.first)
          << ", \"target\": " << resolve_name(pair.second)
          << ", \"value\": ";
      write_heuristic_performance_value(out, value);
      out << '}' << (++index < net.performance_values->size() ? ", " : "");
    }
  }
  out << "], \"dependency_measures\": [";
  for (std::size_t i = 0; i < net.dependency_measures.size(); ++i) {
    const auto& measure = net.dependency_measures[i];
    out << "{\"source\": " << resolve(measure.source)
        << ", \"target\": " << resolve(measure.target)
        << ", \"value\": " << measure.value << '}'
        << (i + 1 < net.dependency_measures.size() ? ", " : "");
  }
  out << "], \"loop_length_two\": [";
  for (std::size_t i = 0; i < net.loop_length_two.size(); ++i) {
    const auto& loop = net.loop_length_two[i];
    out << "{\"source\": " << resolve(loop.source)
        << ", \"target\": " << resolve(loop.target)
        << ", \"frequency\": " << loop.frequency << '}'
        << (i + 1 < net.loop_length_two.size() ? ", " : "");
  }
  out << "], \"start_activities\": ";
  write_ids(net.start_activities);
  out << ", \"end_activities\": ";
  write_ids(net.end_activities);
  out << ", \"active_activities\": ";
  write_ids(net.active_activities);
  out << ", \"activity_statistics\": [";
  for (std::size_t i = 0; i < net.activity_statistics.size(); ++i) {
    const auto& stat = net.activity_statistics[i];
    out << "{\"activity\": " << resolve(stat.activity)
        << ", \"occurrences\": " << stat.occurrences.to_string()
        << ", \"start_occurrences\": " << stat.start_occurrences
        << ", \"end_occurrences\": " << stat.end_occurrences
        << ", \"mean_sojourn_seconds\": " << stat.mean_sojourn_seconds << '}'
        << (i + 1 < net.activity_statistics.size() ? ", " : "");
  }
  out << "], \"relation_statistics\": [";
  for (std::size_t i = 0; i < net.relation_statistics.size(); ++i) {
    const auto& stat = net.relation_statistics[i];
    out << "{\"source\": " << resolve(stat.source)
        << ", \"target\": " << resolve(stat.target)
        << ", \"eventually_follows_frequency\": "
        << stat.eventually_follows_frequency
        << ", \"direct_adjacency_frequency\": "
        << stat.direct_adjacency_frequency
        << ", \"mean_wait_seconds\": " << stat.mean_wait_seconds
        << ", \"raw_frequency\": " << stat.raw_frequency
        << ", \"cleaned_frequency\": " << stat.cleaned_frequency
        << ", \"representation\": ";
    if (stat.representation_value)
      write_heuristic_performance_value(out, *stat.representation_value);
    else
      out << stat.representation;
    out << ", \"length_two_loop\": "
        << (stat.length_two_loop ? "true" : "false")
        << ", \"wait_samples\": [";
    for (std::size_t j = 0; j < stat.wait_samples.size(); ++j) {
      out << stat.wait_samples[j]
          << (j + 1 < stat.wait_samples.size() ? ", " : "");
    }
    out << "], \"median_wait_seconds\": " << stat.median_wait_seconds
        << ", \"min_wait_seconds\": " << stat.min_wait_seconds
        << ", \"max_wait_seconds\": " << stat.max_wait_seconds
        << ", \"stdev_wait_seconds\": " << stat.stdev_wait_seconds
        << ", \"sum_wait_seconds\": " << stat.sum_wait_seconds << '}'
        << (i + 1 < net.relation_statistics.size() ? ", " : "");
  }
  out << "], \"concurrency_statistics\": [";
  for (std::size_t i = 0; i < net.concurrency_statistics.size(); ++i) {
    const auto& stat = net.concurrency_statistics[i];
    out << "{\"first\": " << resolve(stat.first)
        << ", \"second\": " << resolve(stat.second)
        << ", \"frequency\": " << stat.frequency << '}'
        << (i + 1 < net.concurrency_statistics.size() ? ", " : "");
  }
  out << "], \"raw_activity_statistics\": [";
  for (std::size_t i = 0; i < net.raw_activity_statistics.size(); ++i) {
    const auto& stat = net.raw_activity_statistics[i];
    out << "{\"activity\": " << resolve_name(stat.activity)
        << ", \"occurrences\": " << stat.occurrences
        << ", \"mean_sojourn_seconds\": " << stat.mean_sojourn_seconds << '}'
        << (i + 1 < net.raw_activity_statistics.size() ? ", " : "");
  }
  out << "], \"raw_concurrency_statistics\": [";
  for (std::size_t i = 0; i < net.raw_concurrency_statistics.size(); ++i) {
    const auto& stat = net.raw_concurrency_statistics[i];
    out << "{\"first\": " << resolve_name(stat.first)
        << ", \"second\": " << resolve_name(stat.second)
        << ", \"frequency\": " << stat.frequency << '}'
        << (i + 1 < net.raw_concurrency_statistics.size() ? ", " : "");
  }
  const auto write_frequency_dfg = [&](const HeuristicFrequencyDfg& dfg) {
    out << '[';
    std::size_t index = 0;
    for (const auto& [pair, count] : dfg) {
      out << "{\"source\": " << resolve_name(pair.first)
          << ", \"target\": " << resolve_name(pair.second)
          << ", \"count\": " << count << '}'
          << (++index < dfg.size() ? ", " : "");
    }
    out << ']';
  };
  const auto write_and_measures =
      [&](const std::vector<HeuristicAndMeasure>& measures) {
        out << '[';
        for (std::size_t i = 0; i < measures.size(); ++i) {
          const auto& measure = measures[i];
          out << "{\"activity\": " << resolve(measure.activity)
              << ", \"first\": " << resolve(measure.first)
              << ", \"second\": " << resolve(measure.second)
              << ", \"value\": " << measure.value << '}'
              << (i + 1 < measures.size() ? ", " : "");
        }
        out << ']';
      };
  out << "], \"raw_dfg\": ";
  write_frequency_dfg(net.raw_dfg);
  out << ", \"cleaned_dfg\": ";
  write_frequency_dfg(net.cleaned_dfg);
  out << ", \"window_two_dfg\": ";
  write_frequency_dfg(net.window_two_dfg);
  out << ", \"frequency_triples\": [";
  {
    std::size_t index = 0;
    for (const auto& [triple, count] : net.frequency_triples) {
      const auto& [first, middle, last] = triple;
      out << "{\"first\": " << resolve_name(first)
          << ", \"middle\": " << resolve_name(middle)
          << ", \"last\": " << resolve_name(last)
          << ", \"count\": " << count << '}'
          << (++index < net.frequency_triples.size() ? ", " : "");
    }
  }
  out << "], \"and_measures_in\": ";
  write_and_measures(net.and_measures_in);
  out << ", \"and_measures_out\": ";
  write_and_measures(net.and_measures_out);
  const auto write_bundles = [&](const auto& by_activity) {
    out << '[';
    for (std::size_t activity = 0; activity < by_activity.size(); ++activity) {
      out << "{\"activity\": " << resolve(activity) << ", \"bundles\": [";
      for (std::size_t i = 0; i < by_activity[activity].size(); ++i) {
        write_ids(by_activity[activity][i]);
        out << (i + 1 < by_activity[activity].size() ? ", " : "");
      }
      out << "]}" << (activity + 1 < by_activity.size() ? ", " : "");
    }
    out << ']';
  };
  out << ", \"output_bundles\": ";
  write_bundles(net.output_bundles);
  out << ", \"input_bundles\": ";
  write_bundles(net.input_bundles);
  if (net.metadata) {
    const auto write_boundary = [&](const HeuristicBoundaryActivities& value) {
      // Public boundary values are represented as a one-element outer array,
      // preserving whether the selected inner value was a list or map.
      out << '[';
      std::visit(
          [&](const auto& selected) {
            using Value = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<Value, std::vector<std::string>>) {
              out << '[';
              for (std::size_t i = 0; i < selected.size(); ++i) {
                write_json_string(out, selected[i]);
                if (i + 1 < selected.size()) out << ", ";
              }
              out << ']';
            } else {
              out << '{';
              std::size_t i = 0;
              for (const auto& [name, count] : selected) {
                write_json_string(out, name);
                out << ": " << count;
                if (++i < selected.size()) out << ", ";
              }
              out << '}';
            }
          },
          value);
      out << ']';
    };
    out << ", \"metadata\": {\"activities\": [";
    for (std::size_t i = 0; i < net.metadata->activities.size(); ++i) {
      write_json_string(out, net.metadata->activities[i]);
      if (i + 1 < net.metadata->activities.size()) out << ", ";
    }
    out << "], \"activities_occurrences\": {";
    std::size_t occurrence_index = 0;
    for (const auto& [name, count] : net.metadata->activities_occurrences) {
      write_json_string(out, name);
      out << ": " << count.to_string();
      if (++occurrence_index < net.metadata->activities_occurrences.size())
        out << ", ";
    }
    out << "}, \"start_activities\": ";
    write_boundary(net.metadata->start_activities);
    out << ", \"end_activities\": ";
    write_boundary(net.metadata->end_activities);
    out << '}';
  }
  out << '}';
}

void write_token_replay_trace(std::ostream& out,
                              const TokenReplayTraceDiagnostics& trace) {
  if (!trace.reached_marking_keys.empty() &&
      trace.reached_marking_keys.size() != trace.reached_marking.size())
    throw std::invalid_argument("reached marking key count must match marking size");
  const auto previous_precision = out.precision(std::numeric_limits<double>::max_digits10);
  out << "{\"trace_index\": " << trace.trace_index << ", \"case_id\": ";
  write_json_string(out, trace.case_id);
  out << ", \"trace_is_fit\": " << (trace.trace_is_fit ? "true" : "false")
      << ", \"trace_fitness\": " << trace.trace_fitness
      << ", \"produced\": " << trace.produced
      << ", \"consumed\": " << trace.consumed
      << ", \"missing\": " << trace.missing
      << ", \"remaining\": " << trace.remaining
      << ", \"replay_stopped\": " << (trace.replay_stopped ? "true" : "false")
      << ", \"replayed_events\": " << trace.replayed_events;
  const auto numbers = [&](std::string_view name, const auto& values) {
    out << ", ";
    write_json_string(out, name);
    out << ": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ", ";
      out << values[i];
    }
    out << ']';
  };
  numbers("activated_transitions", trace.activated_transitions);
  numbers("transitions_with_problems", trace.transitions_with_problems);
  numbers("enabled_transitions", trace.enabled_transitions);
  numbers("reached_marking", trace.reached_marking);
  if (!trace.reached_marking_keys.empty()) {
    out << ", \"reached_marking_keys\": [";
    for (std::size_t i = 0; i < trace.reached_marking_keys.size(); ++i) {
      if (i) out << ", ";
      out << (trace.reached_marking_keys[i] ? "true" : "false");
    }
    out << ']';
  }
  numbers("unknown_event_indices", trace.unknown_event_indices);
  out << ", \"event_diagnostics\": [";
  for (std::size_t i = 0; i < trace.event_diagnostics.size(); ++i) {
    if (i) out << ", ";
    const auto& event = trace.event_diagnostics[i];
    out << "{\"event_index\": " << event.event_index;
    numbers("activated_transitions", event.activated_transitions);
    numbers("transitions_with_problems", event.transitions_with_problems);
    out << '}';
  }
  out << ']';
  out << ", \"unknown_activities\": [";
  for (std::size_t i = 0; i < trace.unknown_activities.size(); ++i) {
    if (i) out << ", ";
    write_json_string(out, trace.unknown_activities[i]);
  }
  out << "], \"unknown_activity_is_null\": [";
  for (std::size_t i = 0; i < trace.unknown_activity_is_null.size(); ++i) {
    if (i) out << ", ";
    out << (trace.unknown_activity_is_null[i] ? "true" : "false");
  }
  out << "], \"places\": [";
  for (std::size_t i = 0; i < trace.places.size(); ++i) {
    if (i) out << ", ";
    const auto& place = trace.places[i];
    out << "{\"missing\": " << place.missing
        << ", \"remaining\": " << place.remaining
        << ", \"consumed\": " << place.consumed
        << ", \"produced\": " << place.produced << '}';
  }
  out << "]}";
  out.precision(previous_precision);
}

void write_token_replay_duration_comparison(
    std::ostream& out, const TokenReplayDurationComparison& comparison) {
  const auto previous_precision = out.precision(std::numeric_limits<double>::max_digits10);
  const auto indices = [&](std::string_view name, const auto& values) {
    out << ", ";
    write_json_string(out, name);
    out << ": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ", ";
      out << values[i];
    }
    out << ']';
  };
  out << ", \"fit_count\": " << comparison.fit_count
      << ", \"problem_count\": " << comparison.problem_count
      << ", \"fit_median_seconds\": " << comparison.fit_median_seconds
      << ", \"problem_median_seconds\": " << comparison.problem_median_seconds
      << ", \"relative_throughput\": " << comparison.relative_throughput;
  indices("fit_trace_indices", comparison.fit_trace_indices);
  indices("problem_trace_indices", comparison.problem_trace_indices);
  out.precision(previous_precision);
}

void write_token_replay_duration(std::ostream& out,
                                const TokenReplayDurationDiagnostics& diagnostics) {
  out << "{\"timestamp_attribute\": ";
  write_json_string(out, diagnostics.timestamp_attribute);
  out << ", \"transitions\": [";
  for (std::size_t i = 0; i < diagnostics.transitions.size(); ++i) {
    if (i) out << ", ";
    const auto& row = diagnostics.transitions[i];
    out << "{\"transition_index\": " << row.transition_index;
    write_token_replay_duration_comparison(out, row);
    out << '}';
  }
  out << "], \"unknown_activities\": [";
  for (std::size_t i = 0; i < diagnostics.unknown_activities.size(); ++i) {
    if (i) out << ", ";
    const auto& row = diagnostics.unknown_activities[i];
    out << "{\"activity\": ";
    write_json_string(out, row.activity);
    out << ", \"activity_is_null\": " << (row.activity_is_null ? "true" : "false");
    write_token_replay_duration_comparison(out, row);
    out << '}';
  }
  out << "]}";
}

void write_root_cause_model(std::ostream& out, const RootCauseModel& model) {
  const auto previous_precision = out.precision(std::numeric_limits<double>::max_digits10);
  const auto strings = [&](std::string_view name, const auto& values) {
    out << ", ";
    write_json_string(out, name);
    out << ": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ", ";
      write_json_string(out, values[i]);
    }
    out << ']';
  };
  const auto numbers = [&](std::string_view name, const auto& values) {
    out << ", ";
    write_json_string(out, name);
    out << ": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ", ";
      out << values[i];
    }
    out << ']';
  };
  out << ", \"fit_count\": " << model.fit_count
      << ", \"problem_count\": " << model.problem_count;
  strings("feature_names", model.feature_names);
  strings("classes", model.classes);
  out << ", \"data\": [";
  for (std::size_t row = 0; row < model.data.size(); ++row) {
    if (row) out << ", ";
    out << '[';
    for (std::size_t column = 0; column < model.data[row].size(); ++column) {
      if (column) out << ", ";
      if (std::isfinite(model.data[row][column])) out << model.data[row][column];
      else out << "null";
    }
    out << ']';
  }
  out << ']';
  numbers("target", model.target);
  numbers("source_trace_indices", model.source_trace_indices);
  out << ", \"tree\": [";
  for (std::size_t i = 0; i < model.tree.size(); ++i) {
    if (i) out << ", ";
    const auto& node = model.tree[i];
    out << "{\"feature_index\": " << node.feature_index
        << ", \"threshold\": " << node.threshold
        << ", \"left\": " << node.left << ", \"right\": " << node.right
        << ", \"class_counts\": [" << node.class_counts[0] << ", " << node.class_counts[1]
        << "], \"missing_go_to_left\": " << (node.missing_go_to_left ? "true" : "false")
        << ", \"missing_only\": " << (node.missing_only ? "true" : "false") << '}';
  }
  out << ']';
  out.precision(previous_precision);
}

void write_token_replay_root_cause(
    std::ostream& out, const TokenReplayRootCauseDiagnostics& diagnostics) {
  out << "{\"string_attributes\": [";
  for (std::size_t i = 0; i < diagnostics.options.string_attributes.size(); ++i) {
    if (i) out << ", ";
    write_json_string(out, diagnostics.options.string_attributes[i]);
  }
  out << "], \"numeric_attributes\": [";
  for (std::size_t i = 0; i < diagnostics.options.numeric_attributes.size(); ++i) {
    if (i) out << ", ";
    write_json_string(out, diagnostics.options.numeric_attributes[i]);
  }
  out << "], \"enable_multiplier\": " << (diagnostics.options.enable_multiplier ? "true" : "false")
      << ", \"transitions\": [";
  for (std::size_t i = 0; i < diagnostics.transitions.size(); ++i) {
    if (i) out << ", ";
    const auto& row = diagnostics.transitions[i];
    out << "{\"transition_index\": " << row.transition_index;
    write_root_cause_model(out, row);
    out << '}';
  }
  out << "], \"unknown_activities\": [";
  for (std::size_t i = 0; i < diagnostics.unknown_activities.size(); ++i) {
    if (i) out << ", ";
    const auto& row = diagnostics.unknown_activities[i];
    out << "{\"activity\": ";
    write_json_string(out, row.activity);
    out << ", \"activity_is_null\": " << (row.activity_is_null ? "true" : "false");
    write_root_cause_model(out, row);
    if (row.activity_attribute) {
      out << ", \"activity_attribute\": ";
      write_attribute_detail(out, *row.activity_attribute);
    }
    out << '}';
  }
  out << "]}";
}

void write_conformance(std::ostream& out,
                       const ConformanceResult& conformance,
                       const DirectlyFollowsGraph& graph) {
  out << "{\"fitness\": " << conformance.fitness
      << ", \"precision\": " << conformance.precision
      << ", \"generalization\": " << conformance.generalization
      << ", \"simplicity\": " << conformance.simplicity
      << ", \"fitting_traces\": " << conformance.fitting_traces
      << ", \"trace_count\": " << conformance.trace_count
      << ", \"produced\": " << conformance.produced
      << ", \"consumed\": " << conformance.consumed
      << ", \"missing\": " << conformance.missing
      << ", \"remaining\": " << conformance.remaining
      << ", \"activity_attribute\": ";
  write_json_string(out, conformance.activity_attribute.empty()
                             ? "concept:name"
                             : conformance.activity_attribute);
  out << ", \"variants\": [";
  const auto& variants = conformance.variants;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    out << "{\"activities\": [";
    for (std::size_t j = 0; j < variants[i].activities.size(); ++j) {
      // Resolve names to the activity ids used elsewhere in the document.
      const bool is_null = std::find(variants[i].null_activity_indices.begin(),
                                    variants[i].null_activity_indices.end(), j) !=
                           variants[i].null_activity_indices.end();
      const auto id = is_null ? std::nullopt : graph.activity_id(variants[i].activities[j]);
      if (id) out << *id;
      else out << "null";
      out << (j + 1 < variants[i].activities.size() ? ", " : "");
    }
    out << "], \"activity_names\": [";
    for (std::size_t j = 0; j < variants[i].activities.size(); ++j) {
      write_json_string(out, variants[i].activities[j]);
      out << (j + 1 < variants[i].activities.size() ? ", " : "");
    }
    out << ']';
    if (!variants[i].null_activity_indices.empty()) {
      out << ", \"null_activity_indices\": [";
      for (std::size_t j = 0; j < variants[i].null_activity_indices.size(); ++j) {
        if (j) out << ", ";
        out << variants[i].null_activity_indices[j];
      }
      out << ']';
    }
    out << ", \"count\": " << variants[i].count
        << ", \"fitness\": " << variants[i].fitness << '}'
        << (i + 1 < variants.size() ? ", " : "");
  }
  out << ']';
  if (conformance.trace_diagnostics) {
    out << ", \"trace_diagnostics\": [";
    for (std::size_t i = 0; i < conformance.trace_diagnostics->size(); ++i) {
      if (i) out << ", ";
      write_token_replay_trace(out, (*conformance.trace_diagnostics)[i]);
    }
    out << ']';
  }
  if (conformance.duration_diagnostics) {
    out << ", \"duration_diagnostics\": ";
    write_token_replay_duration(out, *conformance.duration_diagnostics);
  }
  if (conformance.root_cause_diagnostics) {
    out << ", \"root_cause_diagnostics\": ";
    write_token_replay_root_cause(out, *conformance.root_cause_diagnostics);
  }
  out << '}';
}

std::string_view move_type_name(AlignmentMoveType type) {
  switch (type) {
    case AlignmentMoveType::kSynchronous: return "sync";
    case AlignmentMoveType::kLog: return "log";
    case AlignmentMoveType::kModel: return "model";
  }
  return "sync";
}

void write_alignments(std::ostream& out,
                      const AlignmentResult& alignments,
                      const DirectlyFollowsGraph& graph) {
  const auto variant_name = [](AlignmentVariant variant) {
    switch (variant) {
      case AlignmentVariant::kStateEquationAStar: return "state_equation_a_star";
      case AlignmentVariant::kDijkstraNoHeuristics: return "dijkstra_no_heuristics";
      case AlignmentVariant::kDijkstraLessMemory: return "dijkstra_less_memory";
      case AlignmentVariant::kDiscountedAStar: return "discounted_a_star";
      case AlignmentVariant::kGeneratorDijkstraNoHeuristics: return "generator_dijkstra_no_heuristics";
      case AlignmentVariant::kGeneratorDijkstraLessMemory: return "generator_dijkstra_less_memory";
    }
    return "dijkstra_less_memory";
  };
  const auto write_move = [&](const AlignmentMove& move) {
    const auto id = graph.activity_id(move.activity);
    out << "{\"type\": \"" << move_type_name(move.type) << "\"";
    if (move.activity.empty()) {
      out << ", \"activity\": null, \"silent\": true";
    } else {
      out << ", \"activity\": ";
      if (id) out << *id;
      else out << "null";
      out << ", \"activity_name\": ";
      write_json_string(out, move.activity);
      out << ", \"silent\": false";
    }
    out << '}';
  };
  const auto write_moves = [&](const std::vector<AlignmentMove>& moves) {
    out << '[';
    for (std::size_t j = 0; j < moves.size(); ++j) {
      write_move(moves[j]);
      out << (j + 1 < moves.size() ? ", " : "");
    }
    out << ']';
  };
  out << "{\"computed\": " << (alignments.computed ? "true" : "false")
      << ", \"exhausted\": " << (alignments.exhausted ? "true" : "false")
      << ", \"fitness\": " << alignments.fitness
      << ", \"precision\": " << alignments.precision
      << ", \"fitting_traces\": " << alignments.fitting_traces
      << ", \"trace_count\": " << alignments.trace_count
      << ", \"total_cost\": " << alignments.total_cost
      << ", \"variant\": \"" << variant_name(alignments.variant) << "\""
      << ", \"activity_attribute\": ";
  write_json_string(out, alignments.activity_attribute.empty()
                             ? "concept:name"
                             : alignments.activity_attribute);
  out
      << ", \"diagnostics\": {\"visited_states\": "
      << alignments.diagnostics.visited_states
      << ", \"queued_states\": " << alignments.diagnostics.queued_states
      << ", \"traversed_arcs\": " << alignments.diagnostics.traversed_arcs
      << ", \"lp_solved\": " << alignments.diagnostics.lp_solved
      << ", \"closed_set_length\": " << alignments.diagnostics.closed_set_length
      << ", \"num_visited_markings\": " << alignments.diagnostics.num_visited_markings
      << "}, \"variants\": [";
  const auto& variants = alignments.variants;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    const auto& variant = variants[i];
    out << "{\"activities\": [";
    for (std::size_t j = 0; j < variant.activities.size(); ++j) {
      const auto id = graph.activity_id(variant.activities[j]);
      if (id) out << *id;
      else out << "null";
      out << (j + 1 < variant.activities.size() ? ", " : "");
    }
    out << "], \"activity_names\": [";
    for (std::size_t j = 0; j < variant.activities.size(); ++j) {
      write_json_string(out, variant.activities[j]);
      out << (j + 1 < variant.activities.size() ? ", " : "");
    }
    out << "], \"count\": " << variant.count << ", \"cost\": " << variant.cost
        << ", \"fitness\": " << variant.fitness
        << ", \"diagnostics\": {\"visited_states\": "
        << variant.diagnostics.visited_states
        << ", \"queued_states\": " << variant.diagnostics.queued_states
        << ", \"traversed_arcs\": " << variant.diagnostics.traversed_arcs
        << ", \"lp_solved\": " << variant.diagnostics.lp_solved
        << ", \"closed_set_length\": " << variant.diagnostics.closed_set_length
        << ", \"num_visited_markings\": " << variant.diagnostics.num_visited_markings
        << "}, \"moves\": ";
    write_moves(variant.moves);
    out << ", \"optimal_alignments\": [";
    for (std::size_t k = 0; k < variant.optimal_alignments.size(); ++k) {
      write_moves(variant.optimal_alignments[k]);
      if (k + 1 < variant.optimal_alignments.size()) out << ", ";
    }
    out << "]}" << (i + 1 < variants.size() ? ", " : "");
  }
  out << "], \"activity_deviations\": [";
  // The log-level aggregate of the moves above: which activities the model
  // gets wrong, and how, weighted by trace counts.
  const auto deviations = summarize_alignment_moves(alignments);
  for (std::size_t i = 0; i < deviations.size(); ++i) {
    const auto& entry = deviations[i];
    const auto id = graph.activity_id(entry.activity);
    out << "{\"activity\": ";
    if (id) out << *id;
    else out << "null";
    out << ", \"activity_name\": ";
    write_json_string(out, entry.activity);
    out << ", \"synchronous\": " << entry.synchronous
        << ", \"log_moves\": " << entry.log_moves
        << ", \"model_moves\": " << entry.model_moves << '}'
        << (i + 1 < deviations.size() ? ", " : "");
  }
  out << "]}";
}

// Serializes the log↔model footprint (directly-follows relation) comparison:
// the two headline ratios, the relation counts, and the deviating pairs as
// activity-id pairs so the frontend can name them from the shared registry.
void write_footprints(std::ostream& out, const FootprintResult& footprints,
                      const DirectlyFollowsGraph& graph) {
  const auto pairs = [&](const std::vector<FootprintRelation>& relations) {
    out << '[';
    for (std::size_t i = 0; i < relations.size(); ++i) {
      const auto source = graph.activity_id(relations[i].source);
      const auto target = graph.activity_id(relations[i].target);
      out << "{\"source\": " << (source ? *source : 0)
          << ", \"target\": " << (target ? *target : 0) << '}'
          << (i + 1 < relations.size() ? ", " : "");
    }
    out << ']';
  };
  out << "{\"fitness\": " << footprints.fitness
      << ", \"precision\": " << footprints.precision
      << ", \"log_relations\": " << footprints.log_relations
      << ", \"model_relations\": " << footprints.model_relations
      << ", \"shared_relations\": " << footprints.shared_relations
      << ", \"exhausted\": " << (footprints.exhausted ? "true" : "false")
      << ", \"log_only\": ";
  pairs(footprints.log_only);
  out << ", \"model_only\": ";
  pairs(footprints.model_only);
  out << '}';
}

// Writes one model's conformance object: whether its net is sound, its
// token-replay result, its footprint comparison, and — when present — its
// alignment result.
void write_model_conformance(std::ostream& out, const ModelConformance& model,
                             const DirectlyFollowsGraph& graph) {
  out << "{\"sound\": " << (model.sound ? "true" : "false")
      << ", \"token_replay\": ";
  write_conformance(out, model.token_replay, graph);
  out << ", \"footprints\": ";
  write_footprints(out, model.footprints, graph);
  if (model.alignments) {
    out << ", \"alignments\": ";
    write_alignments(out, *model.alignments, graph);
  }
  out << '}';
}

// Writes the per-miner conformance map, keyed by miner id. (The server omits
// alignments here and serves them on demand; the CLI embeds them.) This lets
// the frontend offer any miner against any method.
void write_conformance_models(std::ostream& out,
                              const std::vector<ModelConformance>& models,
                              const DirectlyFollowsGraph& graph) {
  out << '{';
  for (std::size_t i = 0; i < models.size(); ++i) {
    write_json_string(out, models[i].miner);
    out << ": ";
    write_model_conformance(out, models[i], graph);
    out << (i + 1 < models.size() ? ", " : "");
  }
  out << '}';
}

// Writes the activity registry: one stats object per activity, indexed by the
// ids every other part of the document uses.
void write_activity_stats(std::ostream& out,
                          const DirectlyFollowsGraph& graph) {
  out << "[\n";
  const auto& activities = graph.activities();
  for (std::size_t i = 0; i < activities.size(); ++i) {
    const auto& name = activities[i];
    out << "    {\"name\": ";
    write_json_string(out, name);
    out << ", \"count\": " << graph.activity_count(name)
        << ", \"start_count\": " << graph.start_activity_count(name)
        << ", \"end_count\": " << graph.end_activity_count(name)
        << ", \"rework_cases\": " << graph.rework_case_count(name) << '}'
        << (i + 1 < activities.size() ? "," : "") << '\n';
  }
  out << "  ]";
}

// Writes a list of generic attributes as one JSON object; a duplicate key
// (legal in XES, rare in practice) keeps the last value.
void write_attribute_object(std::ostream& out,
                            const std::vector<EventAttribute>& attributes) {
  out << '{';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    validate_native_na_attribute(attributes[i], "log export JSON");
    write_json_string(out, attributes[i].key);
    out << ": ";
    if (attributes[i].is_null)
      out << "null";
    else
      write_json_string(out, event_attribute_lexical_value(attributes[i]));
    out << (i + 1 < attributes.size() ? ", " : "");
  }
  out << '}';
}

// Per-key tally of a generic attribute: how many carriers (events or cases)
// have it, and how often each value occurs.
struct AttributeKeyStats {
  std::uint64_t carriers = 0;
  std::unordered_map<std::string, std::uint64_t> value_counts;
};

// Writes one aggregated attribute field (including its leading separator):
// per key (alphabetical), how many carriers have it — under `carrier_name`,
// "events" or "cases" — and its values by frequency. The listed values are
// capped so a high-cardinality attribute (an amount, a note) cannot bloat the
// export; `distinct_values` still reports the full count. Writes nothing when
// there are no keys, so old-style documents stay unchanged.
void write_attribute_stats(std::ostream& out,
                           std::string_view field_name,
                           std::string_view carrier_name,
                           const std::map<std::string, AttributeKeyStats>& keys) {
  if (keys.empty()) return;

  constexpr std::size_t kMaxListedValues = 25;
  out << ",\n  \"" << field_name << "\": [\n";
  std::size_t written = 0;
  for (const auto& [key, stats] : keys) {
    std::vector<std::pair<std::string, std::uint64_t>> values(
        stats.value_counts.begin(), stats.value_counts.end());
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
      return a.second != b.second ? a.second > b.second : a.first < b.first;
    });

    out << "    {\"key\": ";
    write_json_string(out, key);
    out << ", \"" << carrier_name << "\": " << stats.carriers
        << ", \"distinct_values\": " << values.size() << ", \"values\": [";
    const auto listed = std::min(values.size(), kMaxListedValues);
    for (std::size_t i = 0; i < listed; ++i) {
      out << "{\"value\": ";
      write_json_string(out, values[i].first);
      out << ", \"count\": " << values[i].second << '}'
          << (i + 1 < listed ? ", " : "");
    }
    out << "]}" << (++written < keys.size() ? "," : "") << '\n';
  }
  out << "  ]";
}

// Aggregates the log's generic event attributes into an `"event_attributes"`
// field: per key, how many events carry it and its values by frequency.
void write_event_attribute_stats(std::ostream& out, const EventLog& log) {
  std::map<std::string, AttributeKeyStats> keys;  // ordered, stable export
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      for (const auto& attribute : event.attributes) {
        if (attribute.is_null) continue;
        auto& stats = keys[attribute.key];
        ++stats.carriers;
        ++stats.value_counts[attribute.value];
      }
    }
  }
  write_attribute_stats(out, "event_attributes", "events", keys);
}

// Aggregates the log's case attributes into a `"case_attributes"` field: per
// key, how many cases carry it and its values by frequency.
void write_case_attribute_stats(std::ostream& out, const EventLog& log) {
  std::map<std::string, AttributeKeyStats> keys;  // ordered, stable export
  for (const auto& trace : log.traces) {
    for (const auto& attribute : trace.attributes) {
      if (attribute.is_null) continue;
      auto& stats = keys[attribute.key];
      ++stats.carriers;
      ++stats.value_counts[attribute.value];
    }
  }
  write_attribute_stats(out, "case_attributes", "cases", keys);
}

void write_decision_counts(std::ostream& out,
                           const std::vector<std::uint64_t>& counts) {
  out << '[';
  for (std::size_t i = 0; i < counts.size(); ++i) {
    out << counts[i] << (i + 1 < counts.size() ? ", " : "");
  }
  out << ']';
}

// One decision-tree node, recursively: an internal node carries its split test
// and both subtrees, a leaf its predicted branch; both carry the per-branch
// observation counts the node saw.
void write_decision_node(std::ostream& out,
                         const std::vector<DecisionNode>& nodes,
                         std::size_t index) {
  const auto& node = nodes[index];
  out << "{\"counts\": ";
  write_decision_counts(out, node.branch_counts);
  if (node.then_child < 0) {
    const auto majority = static_cast<std::size_t>(
        std::max_element(node.branch_counts.begin(), node.branch_counts.end()) -
        node.branch_counts.begin());
    out << ", \"branch\": " << majority << '}';
    return;
  }
  out << ", \"key\": ";
  write_json_string(out, node.split.key);
  if (node.split.numeric) {
    out << ", \"threshold\": " << node.split.threshold;
  } else {
    out << ", \"value\": ";
    write_json_string(out, node.split.value);
  }
  out << ", \"then\": ";
  write_decision_node(out, nodes, static_cast<std::size_t>(node.then_child));
  out << ", \"else\": ";
  write_decision_node(out, nodes, static_cast<std::size_t>(node.else_child));
  out << '}';
}

void write_decisions(std::ostream& out, const DecisionMiningResult& decisions) {
  out << '{';
  if (!decisions.miner.empty()) {
    out << "\"miner\": ";
    write_json_string(out, decisions.miner);
    out << ", ";
  }
  out << "\"activity_attribute\": ";
  write_json_string(out, decisions.activity_attribute.empty()
                             ? "concept:name"
                             : decisions.activity_attribute);
  out << ", \"observed_traces\": " << decisions.observed_traces
      << ", \"replayed_traces\": " << decisions.replayed_traces
      << ", \"aligned_traces\": " << decisions.aligned_traces
      << ", \"alignment_failures\": " << decisions.alignment_failures
      << ", \"trace_count\": " << decisions.trace_count
      << ", \"decision_points\": [";
  for (std::size_t i = 0; i < decisions.decision_points.size(); ++i) {
    const auto& point = decisions.decision_points[i];
    const auto& root = point.nodes.front().branch_counts;
    out << "{\"place\": " << point.place
        << ", \"observations\": " << point.observations
        << ", \"accuracy\": " << point.accuracy
        << ", \"majority_share\": " << point.majority_share
        << ", \"branches\": [";
    for (std::size_t b = 0; b < point.branches.size(); ++b) {
      out << "{\"label\": ";
      write_json_string(out, point.branches[b]);
      out << ", \"count\": " << root[b] << '}'
          << (b + 1 < point.branches.size() ? ", " : "");
    }
    out << "], \"tree\": ";
    write_decision_node(out, point.nodes, 0);
    out << '}' << (i + 1 < decisions.decision_points.size() ? ", " : "");
  }
  out << "]}";
}

void write_duration_stats(std::ostream& out, const DurationStats& stats) {
  out << "{\"count\": " << stats.count << ", \"total\": " << stats.total
      << ", \"mean\": " << stats.mean << ", \"median\": " << stats.median
      << ", \"min\": " << stats.min << ", \"max\": " << stats.max << '}';
}

// The replay decoration of the embedded Petri net: transition and place
// indices match the "petri_net" object it sits next to.
void write_model_decoration(std::ostream& out,
                            const ModelDecoration& decoration) {
  out << "{\"decorated_traces\": " << decoration.decorated_traces
      << ", \"trace_count\": " << decoration.trace_count
      << ", \"transitions\": [";
  for (std::size_t i = 0; i < decoration.transition_counts.size(); ++i) {
    out << decoration.transition_counts[i]
        << (i + 1 < decoration.transition_counts.size() ? ", " : "");
  }
  out << "], \"places\": [";
  for (std::size_t i = 0; i < decoration.place_counts.size(); ++i) {
    out << "{\"count\": " << decoration.place_counts[i] << ", \"wait\": ";
    write_duration_stats(out, decoration.place_waits[i]);
    out << '}' << (i + 1 < decoration.place_counts.size() ? ", " : "");
  }
  out << "]}";
}

// The spectrum: the pattern's activity names, then one row of instants per
// observation (epoch seconds, one per pattern step, with the case id).
void write_performance_spectrum(std::ostream& out,
                                const PerformanceSpectrum& spectrum) {
  const auto variant_name = [&]() -> std::string_view {
    switch (spectrum.variant) {
      case PerformanceSpectrumVariant::kConnected: return "connected";
      case PerformanceSpectrumVariant::kDisconnected: return "disconnected";
      case PerformanceSpectrumVariant::kDataframe: return "dataframe";
      case PerformanceSpectrumVariant::kLog: return "log";
      case PerformanceSpectrumVariant::kDataframeDisconnected:
        return "dataframe_disconnected";
      case PerformanceSpectrumVariant::kLogDisconnected:
        return "log_disconnected";
      case PerformanceSpectrumVariant::kLazyframe:
        return "lazyframe";
      case PerformanceSpectrumVariant::kLazyframeDisconnected:
        return "lazyframe_disconnected";
    }
    throw std::invalid_argument("performance spectrum has unknown variant");
  }();
  out << "{\"variant\": ";
  write_json_string(out, variant_name);
  out << ", \"activities\": [";
  for (std::size_t i = 0; i < spectrum.activities.size(); ++i) {
    write_json_string(out, spectrum.activities[i]);
    out << (i + 1 < spectrum.activities.size() ? ", " : "");
  }
  out << "], \"activity_attribute\": ";
  write_json_string(out, spectrum.activity_attribute);
  out << ", \"timestamp_attribute\": ";
  write_json_string(out, spectrum.timestamp_attribute);
  out << ", \"case_id_attribute\": ";
  write_json_string(out, spectrum.case_id_attribute);
  out << ", \"observations\": [";
  for (std::size_t i = 0; i < spectrum.observations.size(); ++i) {
    const auto& observation = spectrum.observations[i];
    out << "{\"case_id\": ";
    write_json_string(out, observation.case_id);
    out << ", \"pattern_offset\": " << observation.pattern_offset
        << ", \"instants\": [";
    for (std::size_t k = 0; k < observation.instants.size(); ++k) {
      out << observation.instants[k]
          << (k + 1 < observation.instants.size() ? ", " : "");
    }
    out << "], \"activities\": [";
    for (std::size_t k = 0; k < observation.activities.size(); ++k) {
      write_json_string(out, observation.activities[k]);
      out << (k + 1 < observation.activities.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < spectrum.observations.size() ? ", " : "");
  }
  out << "]}";
}

const char* replay_fitness_variant_name(ReplayFitnessVariant variant) {
  switch (variant) {
    case ReplayFitnessVariant::AUTO: return "auto";
    case ReplayFitnessVariant::TOKEN_BASED: return "token_based";
    case ReplayFitnessVariant::ALIGNMENT_BASED: return "alignment_based";
  }
  return "auto";
}

const char* precision_variant_name(PrecisionVariant variant) {
  switch (variant) {
    case PrecisionVariant::AUTO: return "auto";
    case PrecisionVariant::ETCONFORMANCE_TOKEN: return "etconformance_token";
    case PrecisionVariant::ALIGN_ETCONFORMANCE: return "align_etconformance";
    case PrecisionVariant::AUTOMATON_AFTER_ALIGN:
      return "automaton_after_align";
  }
  return "auto";
}

const char* generalization_variant_name(GeneralizationVariant variant) {
  switch (variant) {
    case GeneralizationVariant::GENERALIZATION_TOKEN:
      return "generalization_token";
  }
  return "generalization_token";
}

const char* simplicity_variant_name(SimplicityVariant variant) {
  switch (variant) {
    case SimplicityVariant::ARC_DEGREE: return "arc_degree";
    case SimplicityVariant::EXTENDED_CARDOSO: return "extended_cardoso";
    case SimplicityVariant::EXTENDED_CYCLOMATIC:
      return "extended_cyclomatic";
  }
  return "arc_degree";
}

void write_finite_or_null(std::ostream& out, double value) {
  if (std::isfinite(value)) out << value;
  else out << "null";
}

void write_replay_fitness(std::ostream& out, const ReplayFitnessResult& result,
                          const DirectlyFollowsGraph* graph) {
  out << "{\"variant\": ";
  write_json_string(out, replay_fitness_variant_name(result.variant));
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"exhausted\": " << (result.exhausted ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  out << ", \"fitness\": ";
  write_finite_or_null(out, result.fitness);
  out << ", \"log_fitness\": ";
  write_finite_or_null(out, result.log_fitness);
  out << ", \"average_trace_fitness\": ";
  write_finite_or_null(out, result.average_trace_fitness);
  out << ", \"percentage_of_fitting_traces\": ";
  write_finite_or_null(out, result.percentage_fit_traces);
  // Keep each evaluator's established compatibility spelling additive to the
  // stable Minera fields. Token replay uses snake_case; alignment evaluation
  // uses camelCase for its two historical summary keys.
  if (result.variant == ReplayFitnessVariant::TOKEN_BASED) {
    out << ", \"perc_fit_traces\": ";
    write_finite_or_null(out, result.percentage_fit_traces);
  } else if (result.variant == ReplayFitnessVariant::ALIGNMENT_BASED) {
    out << ", \"percFitTraces\": ";
    write_finite_or_null(out, result.percentage_fit_traces);
    out << ", \"averageFitness\": ";
    write_finite_or_null(out, result.average_trace_fitness);
  }
  out << ", \"fitting_traces\": " << result.fitting_traces
      << ", \"trace_count\": " << result.trace_count
      << ", \"total_cost\": ";
  write_finite_or_null(out, result.total_cost);
  if (result.variant == ReplayFitnessVariant::ALIGNMENT_BASED &&
      result.decomposed_alignment_result) {
    out << ", \"alignments\": "
        << decomposed_alignment_json(*result.decomposed_alignment_result);
  } else if (graph != nullptr &&
             result.variant == ReplayFitnessVariant::TOKEN_BASED) {
    out << ", \"token_replay\": ";
    write_conformance(out, result.token_result, *graph);
  } else if (graph != nullptr &&
             result.variant == ReplayFitnessVariant::ALIGNMENT_BASED) {
    out << ", \"alignments\": ";
    write_alignments(out, result.alignment_result, *graph);
  }
  out << '}';
}

// The compatibility object above is intentionally compact, but cannot retain
// duplicate keys or nested/typed XES data. The detailed representation is
// additive so existing consumers keep working while lossless consumers can
// use the ordered recursive array.
const char* first_class_state_name(FirstClassAttributeState state);
FirstClassAttributeState effective_first_class_state(
    FirstClassAttributeState state, std::string_view value);
void write_attribute_details(std::ostream& out,
                             const std::vector<EventAttribute>& attributes);
void write_optional_attribute_detail(
    std::ostream& out, std::string_view field,
    const std::optional<EventAttribute>& attribute);
void write_projected_metadata(std::ostream& out,
                              const EventDataMetadata& metadata);

void write_precision(std::ostream& out, const PrecisionResult& result,
                     const DirectlyFollowsGraph* graph) {
  out << "{\"variant\": ";
  write_json_string(out, precision_variant_name(result.variant));
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"exhausted\": " << (result.exhausted ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  out << ", \"precision\": ";
  write_finite_or_null(out, result.precision);
  if (graph != nullptr &&
      result.variant == PrecisionVariant::ETCONFORMANCE_TOKEN) {
    out << ", \"token_replay\": ";
    write_conformance(out, result.token_result, *graph);
  } else if (graph != nullptr &&
             (result.variant == PrecisionVariant::ALIGN_ETCONFORMANCE ||
              result.variant == PrecisionVariant::AUTOMATON_AFTER_ALIGN) &&
             result.alignment_detail_present) {
    out << ", \"alignments\": ";
    write_alignments(out, result.alignment_result, *graph);
  }
  out << '}';
}

void write_generalization(std::ostream& out,
                          const GeneralizationResult& result) {
  out << "{\"variant\": ";
  write_json_string(out, generalization_variant_name(result.variant));
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  out << ", \"score\": ";
  write_finite_or_null(out, result.generalization);
  out << ", \"executed_transitions\": " << result.executed_transitions
      << ", \"transition_count\": " << result.transition_count
      << ", \"transition_frequencies\": [";
  for (std::size_t i = 0; i < result.transition_frequencies.size(); ++i) {
    if (i != 0) out << ", ";
    out << result.transition_frequencies[i];
  }
  out << "]}";
}

void write_simplicity(std::ostream& out, const SimplicityResult& result) {
  out << "{\"variant\": ";
  write_json_string(out, simplicity_variant_name(result.variant));
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"exhausted\": " << (result.exhausted ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  out << ", \"simplicity\": ";
  write_finite_or_null(out, result.simplicity);
  out << ", \"complexity\": ";
  write_finite_or_null(out, result.complexity);
  out << ", \"value\": ";
  write_finite_or_null(out, result.value);
  out << ", \"vertices\": " << result.vertices
      << ", \"edges\": " << result.edges
      << ", \"strongly_connected_components\": "
      << result.strongly_connected_components << '}';
}

void write_composite(std::ostream& out, const CompositeEvaluationResult& result,
                     const DirectlyFollowsGraph* graph) {
  out << "{\"computed\": " << (result.computed ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  // The public composite contract keeps the complete replay-fitness record
  // under fitness and exposes the historical scalar separately.
  out << ", \"fitness\": ";
  write_replay_fitness(out, result.fitness_details, graph);
  out << ", \"fitness_score\": ";
  write_finite_or_null(out, result.fitness);
  out << ", \"precision\": ";
  write_finite_or_null(out, result.precision);
  out << ", \"generalization\": ";
  write_finite_or_null(out, result.generalization);
  out << ", \"simplicity\": ";
  write_finite_or_null(out, result.simplicity);
  out << ", \"metrics_average_weight\": ";
  write_finite_or_null(out, result.metrics_average_weight);
  out << ", \"metricsAverageWeight\": ";
  write_finite_or_null(out, result.metrics_average_weight);
  out << ", \"fscore\": ";
  write_finite_or_null(out, result.fscore);
  out << ", \"weights\": {\"fitness\": ";
  write_finite_or_null(out, result.fitness_weight);
  out << ", \"precision\": ";
  write_finite_or_null(out, result.precision_weight);
  out << ", \"simplicity\": ";
  write_finite_or_null(out, result.simplicity_weight);
  out << ", \"generalization\": ";
  write_finite_or_null(out, result.generalization_weight);
  out << "}}";
}

void write_emd(std::ostream& out, const EmdResult& result) {
  const char* variant = "exact";
  switch (result.variant) {
    case EmdVariant::EXACT: variant = "exact"; break;
    case EmdVariant::PYEMD: variant = "pyemd"; break;
    default: variant = "unknown"; break;
  }
  out << "{\"variant\": ";
  write_json_string(out, variant);
  out << ", \"distance\": ";
  write_finite_or_null(out, result.distance);
  out << ", \"computed\": " << (result.computed ? "true" : "false")
      << ", \"normalized\": " << (result.normalized ? "true" : "false")
      << ", \"use_fast_emd\": " << (result.use_fast_emd ? "true" : "false")
      << ", \"error\": ";
  if (result.error.empty()) out << "null";
  else write_json_string(out, result.error);
  out << '}';
}

void write_model_evaluation(std::ostream& out, const ModelEvaluation& result,
                            const DirectlyFollowsGraph* graph,
                            bool include_miner) {
  out << '{';
  bool first = true;
  const auto field = [&](std::string_view name, const auto& value) {
    if (!first) out << ", ";
    first = false;
    write_json_string(out, name);
    out << ": ";
    value(out);
  };
  if (include_miner) {
    field("miner", [&](std::ostream& stream) {
      write_json_string(stream, result.miner);
    });
  }
  if (result.replay_fitness) {
    field("replay_fitness", [&](std::ostream& stream) {
      write_replay_fitness(stream, *result.replay_fitness, graph);
    });
  }
  if (result.precision) {
    field("precision", [&](std::ostream& stream) {
      write_precision(stream, *result.precision, graph);
    });
  }
  if (result.generalization) {
    field("generalization", [&](std::ostream& stream) {
      write_generalization(stream, *result.generalization);
    });
  }
  if (result.simplicity) {
    field("simplicity", [&](std::ostream& stream) {
      write_simplicity(stream, *result.simplicity);
    });
  }
  if (result.composite) {
    field("composite", [&](std::ostream& stream) {
      write_composite(stream, *result.composite, graph);
    });
  }
  out << '}';
}

void validate_performance_spectrum(const PerformanceSpectrum& spectrum,
                                   const DirectlyFollowsGraph& graph) {
  if (spectrum.activities.size() < 2) {
    throw std::invalid_argument("performance spectrum pattern must have at least two activities");
  }
  if (spectrum.activity_attribute.empty() ||
      spectrum.timestamp_attribute.empty() ||
      spectrum.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "performance spectrum attribute names must not be empty");
  }
  const bool built_in_activity = spectrum.activity_attribute == "activity" ||
                                 spectrum.activity_attribute == "concept:name";
  if (built_in_activity) {
    for (const auto& activity : spectrum.activities) {
      if (!graph.activity_id(activity)) {
        throw std::invalid_argument("performance spectrum activity is not registered: " + activity);
      }
    }
  }
  const auto is_disconnected = [](PerformanceSpectrumVariant variant) {
    return variant == PerformanceSpectrumVariant::kDisconnected ||
           variant == PerformanceSpectrumVariant::kDataframeDisconnected ||
           variant == PerformanceSpectrumVariant::kLogDisconnected ||
           variant == PerformanceSpectrumVariant::kLazyframeDisconnected;
  };
  if (spectrum.variant != PerformanceSpectrumVariant::kConnected &&
      spectrum.variant != PerformanceSpectrumVariant::kDisconnected &&
      spectrum.variant != PerformanceSpectrumVariant::kDataframe &&
      spectrum.variant != PerformanceSpectrumVariant::kLog &&
      spectrum.variant != PerformanceSpectrumVariant::kDataframeDisconnected &&
      spectrum.variant != PerformanceSpectrumVariant::kLogDisconnected &&
      spectrum.variant != PerformanceSpectrumVariant::kLazyframe &&
      spectrum.variant != PerformanceSpectrumVariant::kLazyframeDisconnected) {
    throw std::invalid_argument("performance spectrum has unknown variant");
  }
  for (const auto& observation : spectrum.observations) {
    if (observation.instants.size() != observation.activities.size()) {
      throw std::invalid_argument("performance spectrum observation vectors have different lengths");
    }
    if (observation.instants.empty()) {
      throw std::invalid_argument("performance spectrum observation cannot be empty");
    }
    if (observation.pattern_offset > spectrum.activities.size() ||
        observation.activities.size() >
            spectrum.activities.size() - observation.pattern_offset) {
      throw std::invalid_argument("performance spectrum observation exceeds pattern bounds");
    }
    if (!is_disconnected(spectrum.variant) &&
        observation.pattern_offset != 0) {
      throw std::invalid_argument("connected performance spectrum observation must have offset zero");
    }
    if (is_disconnected(spectrum.variant) &&
        observation.activities.size() < 2) {
      throw std::invalid_argument("disconnected performance spectrum observation must have at least two activities");
    }
    const auto pattern_begin = spectrum.activities.begin() +
                               static_cast<std::ptrdiff_t>(observation.pattern_offset);
    if (!std::equal(observation.activities.begin(), observation.activities.end(),
                    pattern_begin)) {
      throw std::invalid_argument("performance spectrum observation labels do not match pattern offset");
    }
    for (const double instant : observation.instants) {
      if (!std::isfinite(instant)) {
        throw std::invalid_argument("performance spectrum contains a non-finite instant");
      }
    }
    if (!is_disconnected(spectrum.variant)) {
      if (observation.activities != spectrum.activities) {
        throw std::invalid_argument("connected performance spectrum observation does not match pattern");
      }
    } else if (observation.activities.size() < 2 ||
               observation.activities.size() > spectrum.activities.size()) {
      throw std::invalid_argument("disconnected performance spectrum observation is longer than pattern");
    } else {
      bool contiguous = false;
      for (std::size_t start = 0; start + observation.activities.size() <= spectrum.activities.size(); ++start) {
        if (start != observation.pattern_offset) continue;
        if (std::equal(observation.activities.begin(), observation.activities.end(),
                       spectrum.activities.begin() + static_cast<std::ptrdiff_t>(start))) {
          contiguous = true;
          break;
        }
      }
      if (!contiguous) {
        throw std::invalid_argument("disconnected performance spectrum observation is not a contiguous sub-pattern");
      }
    }
  }
}

std::string_view batch_type_name(BatchType type) {
  switch (type) {
    case BatchType::kSimultaneous: return "simultaneous";
    case BatchType::kBatchingOnStart: return "batching_on_start";
    case BatchType::kBatchingOnEnd: return "batching_on_end";
    case BatchType::kSequential: return "sequential";
    case BatchType::kConcurrent: return "concurrent";
  }
  throw std::invalid_argument("batch discovery contains an unknown batch type");
}

std::string_view batch_variant_name(BatchDiscoveryVariant variant) {
  switch (variant) {
    case BatchDiscoveryVariant::kLog: return "log";
    case BatchDiscoveryVariant::kPandas: return "pandas";
    case BatchDiscoveryVariant::kPolars: return "polars";
  }
  throw std::invalid_argument("batch discovery contains an unknown variant");
}

const std::string* batch_event_value(const Event& event, std::string_view key,
                                     std::string_view kind) {
  if (kind == "activity" &&
      (key.empty() || key == "activity" || key == "concept:name" ||
       key == event.activity_source_key)) {
    return &event.activity;
  }
  if (kind == "resource" &&
      (key.empty() || key == "resource" || key == "org:resource" ||
       key == event.resource_source_key)) {
    return &event.resource;
  }
  if (kind == "timestamp" &&
      (key.empty() || key == "timestamp" || key == "time:timestamp" ||
       key == event.timestamp_source_key)) {
    return &event.timestamp;
  }
  if (kind == "case" &&
      (key.empty() || key == "case" || key == "case_id" ||
       key == "case:id" || key == "case:concept:name" ||
       key == "concept:name" || key == event.case_id_source_key)) {
    return &event.case_id;
  }
  return event.attribute(key);
}

const std::string* batch_case_value(const Trace& trace, const Event& event,
                                    std::string_view key) {
  if (key.empty() || key == "case_id" || key == "case:id" ||
      key == "case:concept:name" || key == "concept:name" || key == "case") {
    return &trace.case_id;
  }
  // A non-standard case selector is resolved against the exact source event,
  // with trace metadata as a fallback.  This mirrors LOG discovery and is
  // important when events in one trace carry different case labels.
  if (const auto* value = batch_event_value(event, key, "case")) return value;
  if (const auto* value = trace.attribute(key)) return value;
  return nullptr;
}

BatchType classify_batch_events(const std::vector<BatchEvent>& events) {
  const double first_start = events.front().start;
  const double first_complete = events.front().complete;
  bool equal_starts = true, equal_ends = true, simultaneous = true;
  for (const auto& event : events) {
    equal_starts = equal_starts && event.start == first_start;
    equal_ends = equal_ends && event.complete == first_complete;
    simultaneous = simultaneous && event.start == first_start &&
                   event.complete == first_complete;
  }
  if (simultaneous) return BatchType::kSimultaneous;
  if (equal_starts) return BatchType::kBatchingOnStart;
  if (equal_ends) return BatchType::kBatchingOnEnd;
  bool sequential = true;
  for (std::size_t i = 1; i < events.size(); ++i) {
    if (events[i - 1].complete != events[i].start) {
      sequential = false;
      break;
    }
  }
  return sequential ? BatchType::kSequential : BatchType::kConcurrent;
}

void validate_batch_discovery(const BatchDiscovery& discovery,
                              const EventLog& log,
                              const DirectlyFollowsGraph& graph) {
  if (discovery.variant != BatchDiscoveryVariant::kLog &&
      discovery.variant != BatchDiscoveryVariant::kPandas &&
      discovery.variant != BatchDiscoveryVariant::kPolars) {
    throw std::invalid_argument("batch discovery has an unknown variant");
  }
  if (!std::isfinite(discovery.options.merge_distance_seconds) ||
      discovery.options.merge_distance_seconds < 0.0 ||
      discovery.options.min_batch_size < 2 ||
      discovery.options.activity_attribute.empty() ||
      discovery.options.resource_attribute.empty() ||
      discovery.options.timestamp_attribute.empty() ||
      discovery.options.case_id_attribute.empty()) {
    throw std::invalid_argument("batch discovery options are invalid");
  }
  std::set<std::pair<std::size_t, std::size_t>> identities;
  std::size_t previous_group_batch_count = std::numeric_limits<std::size_t>::max();
  std::string previous_group_activity;
  std::string previous_group_resource;
  bool first_group = true;
  for (const auto& group : discovery.groups) {
    if (discovery.options.activity_attribute == "activity" ||
        discovery.options.activity_attribute == "concept:name") {
      if (!graph.activity_id(group.activity)) {
      throw std::invalid_argument("batch discovery activity is not registered: " + group.activity);
      }
    }
    std::size_t total = 0;
    if (group.batch_count != group.batches.size()) {
      throw std::invalid_argument("batch discovery group batch_count does not match batches");
    }
    if (!first_group &&
        (group.batch_count > previous_group_batch_count ||
         (group.batch_count == previous_group_batch_count &&
          std::tie(group.activity, group.resource) >=
              std::tie(previous_group_activity, previous_group_resource)))) {
      throw std::invalid_argument("batch discovery groups are not ordered");
    }
    first_group = false;
    previous_group_batch_count = group.batch_count;
    previous_group_activity = group.activity;
    previous_group_resource = group.resource;
    double previous_start = -std::numeric_limits<double>::infinity();
    for (const auto& batch : group.batches) {
      (void)batch_type_name(batch.type);
      if (!std::isfinite(batch.start) || !std::isfinite(batch.complete) ||
          batch.start > batch.complete) {
        throw std::invalid_argument("batch discovery has invalid batch range");
      }
      if (batch.events.size() < discovery.options.min_batch_size) {
        throw std::invalid_argument("batch discovery batch must contain at least two events");
      }
      if (batch.type != classify_batch_events(batch.events)) {
        throw std::invalid_argument("batch discovery type does not match event intervals");
      }
      if (batch.start < previous_start) {
        throw std::invalid_argument("batch discovery batches are not ordered");
      }
      previous_start = batch.start;
      double previous_event_start = -std::numeric_limits<double>::infinity();
      for (const auto& event : batch.events) {
        if (!std::isfinite(event.start) || !std::isfinite(event.complete) ||
            event.start > event.complete || event.start < batch.start ||
            event.complete > batch.complete) {
          throw std::invalid_argument("batch discovery has invalid event range");
        }
        if (discovery.variant == BatchDiscoveryVariant::kLog) {
          if (event.trace_index >= log.traces.size() ||
              event.event_index >= log.traces[event.trace_index].events.size()) {
            throw std::invalid_argument(
                "batch discovery event index is out of range");
          }
          const auto& source =
              log.traces[event.trace_index].events[event.event_index];
          const auto* source_activity = batch_event_value(
              source, discovery.options.activity_attribute, "activity");
          const auto* source_resource = batch_event_value(
              source, discovery.options.resource_attribute, "resource");
          if (source_activity == nullptr || source_resource == nullptr ||
              *source_activity != group.activity ||
              *source_resource != group.resource) {
            throw std::invalid_argument(
                "batch discovery event does not match its group");
          }
          const auto* source_case = batch_case_value(
              log.traces[event.trace_index], source,
              discovery.options.case_id_attribute);
          if (source_case == nullptr || event.case_id != *source_case) {
            throw std::invalid_argument(
                "batch discovery event case_id does not match source event");
          }
        }
        if (event.start < previous_event_start) {
          throw std::invalid_argument("batch discovery events are not ordered");
        }
        previous_event_start = event.start;
        if (!identities.emplace(event.trace_index, event.event_index).second) {
          throw std::invalid_argument("batch discovery contains a duplicate event identity");
        }
        ++total;
      }
    }
    if (group.event_count != total) {
      throw std::invalid_argument("batch discovery group event_count does not match batches");
    }
  }
}

void write_batch_discovery(std::ostream& out, const BatchDiscovery& discovery,
                           const DirectlyFollowsGraph& graph) {
  out << "{\"groups\": [";
  for (std::size_t i = 0; i < discovery.groups.size(); ++i) {
    const auto& group = discovery.groups[i];
    const auto activity = graph.activity_id(group.activity);
    out << "{\"activity\": ";
    if (activity)
      out << *activity;
    else
      out << "null";
    out << ", \"activity_label\": ";
    write_json_string(out, group.activity);
    out << ", \"resource\": ";
    write_json_string(out, group.resource);
    out << ", \"event_count\": " << group.event_count
        << ", \"batch_count\": " << group.batch_count << ", \"batches\": [";
    for (std::size_t j = 0; j < group.batches.size(); ++j) {
      const auto& batch = group.batches[j];
      out << "{\"type\": ";
      write_json_string(out, batch_type_name(batch.type));
      out << ", \"start\": " << batch.start << ", \"complete\": "
          << batch.complete << ", \"events\": [";
      for (std::size_t k = 0; k < batch.events.size(); ++k) {
        const auto& event = batch.events[k];
        out << "{\"start\": " << event.start << ", \"complete\": "
            << event.complete << ", \"case_id\": ";
        write_json_string(out, event.case_id);
        out << ", \"trace_index\": " << event.trace_index
            << ", \"event_index\": " << event.event_index;
        if (!event.event_id.empty()) {
          out << ", \"event_id\": ";
          write_json_string(out, event.event_id);
        }
        out << "}" << (k + 1 < batch.events.size() ? ", " : "");
      }
      out << "]}" << (j + 1 < group.batches.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < discovery.groups.size() ? ", " : "");
  }
  out << "], \"variant\": ";
  write_json_string(out, batch_variant_name(discovery.variant));
  out << ", \"activity_attribute\": ";
  write_json_string(out, discovery.options.activity_attribute);
  out << ", \"resource_attribute\": ";
  write_json_string(out, discovery.options.resource_attribute);
  out << ", \"timestamp_attribute\": ";
  write_json_string(out, discovery.options.timestamp_attribute);
  out << ", \"case_id_attribute\": ";
  write_json_string(out, discovery.options.case_id_attribute);
  out << ", \"start_timestamp_attribute\": ";
  write_json_string(out, discovery.options.start_timestamp_attribute);
  out << ", \"event_id_attribute\": ";
  write_json_string(out, discovery.options.event_id_attribute);
  out << "}";
}

void write_performance(std::ostream& out, const PerformanceMetrics& performance) {
  out << "{\"timed_traces\": " << performance.timed_traces
      << ", \"case_duration\": ";
  write_duration_stats(out, performance.case_duration);
  out << ", \"case_arrival\": ";
  write_duration_stats(out, performance.case_arrival);
  out << ", \"edges\": [";
  const auto& edges = performance.edges;
  for (std::size_t i = 0; i < edges.size(); ++i) {
    out << "{\"source\": " << edges[i].source
        << ", \"target\": " << edges[i].target << ", \"stats\": ";
    write_duration_stats(out, edges[i].stats);
    out << '}' << (i + 1 < edges.size() ? ", " : "");
  }
  out << "], \"activities\": [";
  // Service times from lifecycle start/complete pairs; sparse, so a log of
  // atomic events emits an empty list.
  const auto& service = performance.activity_service;
  for (std::size_t i = 0; i < service.size(); ++i) {
    out << "{\"activity\": " << service[i].activity << ", \"stats\": ";
    write_duration_stats(out, service[i].stats);
    out << '}' << (i + 1 < service.size() ? ", " : "");
  }
  out << "]}";
}

void write_resources(std::ostream& out, const ResourceNetwork& resources) {
  out << "{\"total_handovers\": " << resources.total_handovers
      << ", \"resources\": [";
  for (std::size_t i = 0; i < resources.resources.size(); ++i) {
    out << "{\"name\": ";
    write_json_string(out, resources.resources[i]);
    out << ", \"event_count\": " << resources.event_counts[i] << '}'
        << (i + 1 < resources.resources.size() ? ", " : "");
  }
  out << "], \"handovers\": [";
  const auto& handovers = resources.handovers;
  for (std::size_t i = 0; i < handovers.size(); ++i) {
    out << "{\"source\": " << handovers[i].source
        << ", \"target\": " << handovers[i].target
        << ", \"count\": " << handovers[i].count << '}'
        << (i + 1 < handovers.size() ? ", " : "");
  }
  out << "]}";
}

struct ResolvedTemporalProfileEntry {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t count = 0;
  double mean_seconds = 0.0;
  double standard_deviation_seconds = 0.0;
};

struct ResolvedMinimumSelfDistance {
  std::size_t activity = 0;
  std::uint64_t distance = 0;
  std::vector<std::size_t> witnesses;
};

std::vector<ResolvedMinimumSelfDistance> resolve_minimum_self_distances(
    const MinimumSelfDistances& distances, const DirectlyFollowsGraph& graph) {
  std::vector<ResolvedMinimumSelfDistance> resolved;
  resolved.reserve(distances.size());
  std::map<std::size_t, bool> seen_activities;
  std::string previous_activity;
  bool first = true;
  for (const auto& entry : distances) {
    if (!first && entry.activity <= previous_activity) {
      throw std::invalid_argument(
          "minimum self-distance JSON activities must be strictly sorted");
    }
    first = false;
    previous_activity = entry.activity;
    const auto activity = graph.activity_id(entry.activity);
    if (!activity) {
      throw std::invalid_argument(
          "minimum self-distance activity is not registered in the directly-follows graph: " +
          entry.activity);
    }
    if (!seen_activities.emplace(*activity, true).second) {
      throw std::invalid_argument(
          "minimum self-distance JSON contains a duplicate activity");
    }
    if (entry.distance == 0 && !entry.witnesses.empty()) {
      throw std::invalid_argument(
          "minimum self-distance zero entries must have empty witnesses");
    }
    if (entry.distance > 0 && entry.witnesses.empty()) {
      throw std::invalid_argument(
          "positive minimum self-distance entries must have witnesses");
    }
    std::vector<std::size_t> witnesses;
    witnesses.reserve(entry.witnesses.size());
    std::string previous_witness;
    bool first_witness = true;
    for (const auto& witness_name : entry.witnesses) {
      if (!first_witness && witness_name <= previous_witness) {
        throw std::invalid_argument(
            "minimum self-distance witnesses must be strictly sorted");
      }
      first_witness = false;
      previous_witness = witness_name;
      const auto witness = graph.activity_id(witness_name);
      if (!witness) {
        throw std::invalid_argument(
            "minimum self-distance witness is not registered in the directly-follows graph: " +
            witness_name);
      }
      if (*witness == *activity) {
        throw std::invalid_argument(
            "minimum self-distance witness cannot equal its activity");
      }
      witnesses.push_back(*witness);
    }
    resolved.push_back({*activity, entry.distance, std::move(witnesses)});
  }
  return resolved;
}

void write_minimum_self_distances(
    std::ostream& out,
    const std::vector<ResolvedMinimumSelfDistance>& distances) {
  out << '[';
  for (std::size_t i = 0; i < distances.size(); ++i) {
    const auto& entry = distances[i];
    out << "{\"activity\": " << entry.activity
        << ", \"distance\": " << entry.distance << ", \"witnesses\": [";
    for (std::size_t j = 0; j < entry.witnesses.size(); ++j) {
      out << entry.witnesses[j]
          << (j + 1 < entry.witnesses.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < distances.size() ? ", " : "");
  }
  out << ']';
}

std::string_view minimum_self_distance_variant_name(
    MinimumSelfDistanceVariant variant) {
  switch (variant) {
    case MinimumSelfDistanceVariant::kLog: return "log";
    case MinimumSelfDistanceVariant::kPandas: return "pandas";
    case MinimumSelfDistanceVariant::kPolars: return "polars";
  }
  throw std::invalid_argument(
      "minimum self-distance JSON contains an unknown variant");
}

std::vector<ResolvedTemporalProfileEntry> resolve_temporal_profile(
    const TemporalProfile& profile, const DirectlyFollowsGraph& graph) {
  std::vector<ResolvedTemporalProfileEntry> resolved;
  resolved.reserve(profile.entries.size());
  std::map<std::pair<std::size_t, std::size_t>, bool> seen;
  std::pair<std::string, std::string> previous_names;
  bool first = true;
  for (const auto& entry : profile.entries) {
    const auto names = std::make_pair(entry.source, entry.target);
    if (!first && names <= previous_names) {
      throw std::invalid_argument(
          "temporal profile JSON entries must be strictly sorted by source and target");
    }
    first = false;
    previous_names = names;
    const auto source = graph.activity_id(entry.source);
    const auto target = graph.activity_id(entry.target);
    if (!source || !target) {
      throw std::invalid_argument(
          "temporal profile activity is not registered in the directly-follows graph: " +
          entry.source + " -> " + entry.target);
    }
    if (entry.count == 0 || !std::isfinite(entry.mean_seconds) ||
        entry.mean_seconds < 0.0 ||
        !std::isfinite(entry.standard_deviation_seconds) ||
        entry.standard_deviation_seconds < 0.0) {
      throw std::invalid_argument(
          "temporal profile JSON requires a positive count and finite, non-negative timing statistics");
    }
    if (!seen.emplace(std::make_pair(*source, *target), true).second) {
      throw std::invalid_argument(
          "temporal profile JSON contains a duplicate source/target pair");
    }
    resolved.push_back({*source, *target, entry.count, entry.mean_seconds,
                        entry.standard_deviation_seconds});
  }
  return resolved;
}

void write_temporal_profile(
    std::ostream& out,
    const std::vector<ResolvedTemporalProfileEntry>& profile) {
  out << "[";
  for (std::size_t i = 0; i < profile.size(); ++i) {
    const auto& entry = profile[i];
    out << "{\"source\": " << entry.source << ", \"target\": " << entry.target
        << ", \"count\": " << entry.count
        << ", \"mean_seconds\": " << entry.mean_seconds
        << ", \"standard_deviation_seconds\": "
        << entry.standard_deviation_seconds << '}'
        << (i + 1 < profile.size() ? ", " : "");
  }
  out << "]";
}

struct ResolvedTransitionSystem {
  std::vector<std::vector<std::size_t>> payload;
};

const char* transition_view_name(TransitionView view) {
  switch (view) {
    case TransitionView::kSequence: return "sequence";
    case TransitionView::kSet: return "set";
    case TransitionView::kMultiset: return "multiset";
  }
  throw std::invalid_argument("transition system has an invalid view");
}

const char* transition_direction_name(TransitionDirection direction) {
  switch (direction) {
    case TransitionDirection::kForward: return "forward";
    case TransitionDirection::kBackward: return "backward";
  }
  throw std::invalid_argument("transition system has an invalid direction");
}

ResolvedTransitionSystem resolve_transition_system(
    const TransitionSystem& system, const EventLog& log,
    const DirectlyFollowsGraph& graph) {
  (void)transition_view_name(system.options().view);
  (void)transition_direction_name(system.options().direction);
  ResolvedTransitionSystem resolved;
  const auto& states = system.states();
  resolved.payload.reserve(states.size());
  for (std::size_t i = 0; i < states.size(); ++i) {
    const auto& state = states[i];
    if (state.id != i) throw std::invalid_argument("transition system state ids must be contiguous and ordered");
    std::vector<std::size_t> ids;
    ids.reserve(state.payload.size());
    std::string previous;
    bool first = true;
    for (const auto& activity : state.payload) {
      if (!first && (system.options().view == TransitionView::kSet ||
                     system.options().view == TransitionView::kMultiset) &&
          activity < previous) {
        throw std::invalid_argument("transition system set/multiset payloads must be sorted");
      }
      if (!first && system.options().view == TransitionView::kSet && activity == previous) {
        throw std::invalid_argument("transition system set payloads must be unique");
      }
      first = false;
      previous = activity;
      const auto id = graph.activity_id(activity);
      if (!id) throw std::invalid_argument("transition system activity is not registered: " + activity);
      ids.push_back(*id);
    }
    resolved.payload.push_back(std::move(ids));
    const auto validate_occurrence = [&](const TransitionOccurrence& occurrence) {
      if (occurrence.trace_index >= log.traces.size() ||
          occurrence.event_index >= log.traces[occurrence.trace_index].events.size()) {
        throw std::invalid_argument("transition system occurrence is outside the event log");
      }
    };
    const auto validate_occurrences = [&](const std::vector<TransitionOccurrence>& occurrences) {
      TransitionOccurrence previous{};
      bool first_occurrence = true;
      for (const auto& occurrence : occurrences) {
        validate_occurrence(occurrence);
        if (!first_occurrence && (occurrence.trace_index < previous.trace_index ||
                                  (occurrence.trace_index == previous.trace_index &&
                                   occurrence.event_index <= previous.event_index))) {
          throw std::invalid_argument("transition system occurrence references must be strictly sorted");
        }
        first_occurrence = false;
        previous = occurrence;
      }
    };
    validate_occurrences(state.incoming_occurrences);
    validate_occurrences(state.outgoing_occurrences);
  }
  std::set<std::tuple<std::size_t, std::size_t, std::string>> seen;
  std::tuple<std::size_t, std::size_t, std::string> previous_transition;
  bool first_transition = true;
  for (const auto& transition : system.transitions()) {
    if (transition.source >= states.size() || transition.target >= states.size())
      throw std::invalid_argument("transition system transition references an unknown state");
    if (transition.count == 0) throw std::invalid_argument("transition system transition counts must be positive");
    if (!seen.emplace(transition.source, transition.target, transition.label).second)
      throw std::invalid_argument("transition system contains duplicate transitions");
    const auto transition_key = std::make_tuple(transition.source, transition.target,
                                                transition.label);
    if (!first_transition && transition_key <= previous_transition)
      throw std::invalid_argument("transition system transitions must be strictly sorted");
    first_transition = false;
    previous_transition = transition_key;
    if (!graph.activity_id(transition.label))
      throw std::invalid_argument("transition system transition activity is not registered: " + transition.label);
    TransitionOccurrence previous{};
    bool first_occurrence = true;
    for (const auto& occurrence : transition.occurrences) {
      if (occurrence.trace_index >= log.traces.size() ||
          occurrence.event_index >= log.traces[occurrence.trace_index].events.size())
        throw std::invalid_argument("transition system occurrence is outside the event log");
      if (log.traces[occurrence.trace_index].events[occurrence.event_index].activity != transition.label)
        throw std::invalid_argument("transition system occurrence activity does not match its transition label");
      if (!first_occurrence && (occurrence.trace_index < previous.trace_index ||
                                (occurrence.trace_index == previous.trace_index &&
                                 occurrence.event_index <= previous.event_index)))
        throw std::invalid_argument("transition system occurrence references must be strictly sorted");
      first_occurrence = false;
      previous = occurrence;
    }
    if (system.options().include_data && transition.occurrences.size() != transition.count)
      throw std::invalid_argument("transition system transition occurrences must match count when include_data is enabled");
  }
  return resolved;
}

void write_transition_occurrences(std::ostream& out,
                                  const std::vector<TransitionOccurrence>& occurrences) {
  out << "[";
  for (std::size_t i = 0; i < occurrences.size(); ++i) {
    out << "{\"trace_index\": " << occurrences[i].trace_index
        << ", \"event_index\": " << occurrences[i].event_index << "}"
        << (i + 1 < occurrences.size() ? ", " : "");
  }
  out << "]";
}

void write_transition_system(std::ostream& out, const TransitionSystem& system,
                             const ResolvedTransitionSystem& resolved,
                             const DirectlyFollowsGraph& graph) {
  const auto& options = system.options();
  out << "{\"view\": \"" << transition_view_name(options.view)
      << "\", \"direction\": \"" << transition_direction_name(options.direction)
      << "\", \"window\": " << options.window
      << ", \"include_data\": " << (options.include_data ? "true" : "false")
      << ", \"states\": [";
  for (std::size_t i = 0; i < system.states().size(); ++i) {
    const auto& state = system.states()[i];
    out << "{\"id\": " << state.id << ", \"payload\": ";
    if (options.view == TransitionView::kMultiset) {
      out << "[";
      for (std::size_t j = 0; j < resolved.payload[i].size();) {
        std::size_t k = j + 1;
        while (k < resolved.payload[i].size() && resolved.payload[i][k] == resolved.payload[i][j]) ++k;
        out << "{\"activity\": " << resolved.payload[i][j] << ", \"count\": " << (k - j) << "}"
            << (k < resolved.payload[i].size() ? ", " : "");
        j = k;
      }
      out << "]";
    } else {
      out << "[";
      for (std::size_t j = 0; j < resolved.payload[i].size(); ++j)
        out << resolved.payload[i][j] << (j + 1 < resolved.payload[i].size() ? ", " : "");
      out << "]";
    }
    out << ", \"start_count\": " << state.start_count
        << ", \"end_count\": " << state.end_count;
    if (options.include_data) {
      out << ", \"incoming_occurrences\": "; write_transition_occurrences(out, state.incoming_occurrences);
      out << ", \"outgoing_occurrences\": "; write_transition_occurrences(out, state.outgoing_occurrences);
    }
    out << "}" << (i + 1 < system.states().size() ? ", " : "");
  }
  out << "], \"transitions\": [";
  for (std::size_t i = 0; i < system.transitions().size(); ++i) {
    const auto& transition = system.transitions()[i];
    out << "{\"source\": " << transition.source << ", \"target\": " << transition.target
        << ", \"activity\": " << *graph.activity_id(transition.label)
        << ", \"count\": " << transition.count;
    if (options.include_data) {
      out << ", \"occurrences\": ";
      write_transition_occurrences(out, transition.occurrences);
    }
    out << "}" << (i + 1 < system.transitions().size() ? ", " : "");
  }
  out << "]}";
}

std::size_t dfg_variant_activity_id(
    const DirectlyFollowsGraph& graph, const std::string& activity) {
  const auto id = graph.activity_id(activity);
  if (!id) {
    throw std::invalid_argument(
        "DFG variant activity is not registered: " + activity);
  }
  return *id;
}

// JSON numbers are consumed as IEEE-754 doubles by the frontend. Keep every
// statistical DFG count within the exactly representable integer range before
// writing the document.
constexpr std::uint64_t kMaxJsonSafeInteger = 9007199254740991ULL;

void validate_dfg_count(std::uint64_t count, std::string_view field) {
  if (count > kMaxJsonSafeInteger) {
    throw std::invalid_argument("DFG " + std::string(field) +
                                " exceeds the JSON safe-integer limit");
  }
}

void validate_dfg_performance_statistics(
    const DfgPerformanceStatistics& statistics) {
  validate_dfg_count(statistics.summary.count, "performance summary count");
  const auto finite = [](double value) { return std::isfinite(value); };
  const auto& summary = statistics.summary;
  if (!finite(summary.mean) || !finite(summary.median) ||
      !finite(summary.minimum) || !finite(summary.maximum) ||
      !finite(summary.standard_deviation) || !finite(summary.sum)) {
    throw std::invalid_argument(
        "DFG performance statistics must be finite");
  }
  if (!std::all_of(statistics.raw_values.begin(),
                   statistics.raw_values.end(), finite)) {
    throw std::invalid_argument("DFG performance samples must be finite");
  }
  std::visit(
      [&](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, double>) {
          if (!finite(value)) {
            throw std::invalid_argument(
                "DFG selected performance value must be finite");
          }
        } else if constexpr (std::is_same_v<Value, std::vector<double>>) {
          if (!std::all_of(value.begin(), value.end(), finite)) {
            throw std::invalid_argument(
                "DFG selected performance samples must be finite");
          }
        } else {
          validate_dfg_count(value.count,
                             "selected performance summary count");
          if (!finite(value.mean) || !finite(value.median) ||
              !finite(value.minimum) || !finite(value.maximum) ||
              !finite(value.standard_deviation) || !finite(value.sum)) {
            throw std::invalid_argument(
                "DFG selected performance summary must be finite");
          }
        }
      },
      statistics.value);
}

void validate_dfg_variants(const DfgVariantsAnalysis& variants,
                           const DirectlyFollowsGraph& graph) {
  const auto statistical_variant_for_name =
      [](std::string_view name) -> std::optional<DfgStatisticalVariant> {
    if (name == "native" || name == "frequency" ||
        name == "frequency_greedy") {
      return DfgStatisticalVariant::kNative;
    }
    if (name == "frequency_triples")
      return DfgStatisticalVariant::kFrequencyTriples;
    if (name == "performance" || name == "performance_greedy")
      return DfgStatisticalVariant::kPerformance;
    if (name == "case_attributes")
      return DfgStatisticalVariant::kCaseAttributes;
    if (name == "clean") return DfgStatisticalVariant::kClean;
    if (name == "clean_polars") return DfgStatisticalVariant::kCleanPolars;
    if (name == "clean_time") return DfgStatisticalVariant::kCleanTime;
    return std::nullopt;
  };
  const auto causal_variant_for_name =
      [](std::string_view name) -> std::optional<DfgCausalVariant> {
    if (name == "alpha") return DfgCausalVariant::kAlpha;
    if (name == "heuristic") return DfgCausalVariant::kHeuristic;
    return std::nullopt;
  };
  const auto validate_pair =
      [&](const DfgActivityPair& pair) {
        (void)dfg_variant_activity_id(graph, pair.first);
        (void)dfg_variant_activity_id(graph, pair.second);
      };
  for (const auto& named : variants.statistical) {
    const auto expected_variant = statistical_variant_for_name(named.name);
    if (!expected_variant || *expected_variant != named.analysis.variant) {
      throw std::invalid_argument(
          "DFG statistical variant name and result do not agree");
    }
    const auto& analysis = named.analysis;
    for (const auto& [pair, count] : analysis.frequency_dfg) {
      validate_dfg_count(count, "frequency count");
      validate_pair(pair);
    }
    for (const auto& [triple, count] : analysis.frequency_triples) {
      validate_dfg_count(count, "frequency triple count");
      (void)dfg_variant_activity_id(graph, std::get<0>(triple));
      (void)dfg_variant_activity_id(graph, std::get<1>(triple));
      (void)dfg_variant_activity_id(graph, std::get<2>(triple));
    }
    for (const auto& [pair, statistics] : analysis.performance_dfg) {
      validate_pair(pair);
      validate_dfg_performance_statistics(statistics);
    }
    for (const auto& [pair, attributes] : analysis.case_attributes.edges) {
      validate_pair(pair);
      for (const auto& [attribute, values] : attributes) {
        (void)attribute;
        for (const auto& [value, count] : values) {
          (void)value;
          validate_dfg_count(count, "case-attribute value count");
        }
      }
    }
    for (const auto& [activity, attributes] :
         analysis.case_attributes.nodes) {
      (void)dfg_variant_activity_id(graph, activity);
      for (const auto& [attribute, values] : attributes) {
        (void)attribute;
        for (const auto& [value, count] : values) {
          (void)value;
          validate_dfg_count(count, "case-attribute value count");
        }
      }
    }
    for (const auto& [pair, count] : analysis.clean.graph) {
      validate_dfg_count(count, "clean DFG count");
      validate_pair(pair);
    }
    for (const auto& [activity, count] :
         analysis.clean.start_activities) {
      validate_dfg_count(count, "clean start-activity count");
      (void)dfg_variant_activity_id(graph, activity);
    }
    for (const auto& [activity, count] : analysis.clean.end_activities) {
      validate_dfg_count(count, "clean end-activity count");
      (void)dfg_variant_activity_id(graph, activity);
    }
    for (const auto& [activity, value] : analysis.clean_time) {
      (void)dfg_variant_activity_id(graph, activity);
      if (!std::isfinite(value)) {
        throw std::invalid_argument("DFG clean-time value must be finite");
      }
    }
  }
  for (const auto& named : variants.causal) {
    const auto expected_variant = causal_variant_for_name(named.name);
    if (!expected_variant || *expected_variant != named.analysis.variant) {
      throw std::invalid_argument(
          "DFG causal variant name and result do not agree");
    }
    for (const auto& [pair, value] : named.analysis.relations) {
      validate_pair(pair);
      if (!std::isfinite(value)) {
        throw std::invalid_argument("DFG causal value must be finite");
      }
    }
  }
}

void write_dfg_frequency(std::ostream& out, const DfgFrequency& dfg,
                         const DirectlyFollowsGraph& graph) {
  out << '[';
  std::size_t emitted = 0;
  for (const auto& [pair, count] : dfg) {
    if (emitted++ != 0) out << ", ";
    out << "{\"source\": " << dfg_variant_activity_id(graph, pair.first)
        << ", \"target\": " << dfg_variant_activity_id(graph, pair.second)
        << ", \"count\": " << count << '}';
  }
  out << ']';
}

void write_dfg_performance_summary(
    std::ostream& out, const DfgPerformanceSummary& summary) {
  out << "{\"count\": " << summary.count << ", \"mean\": " << summary.mean
      << ", \"median\": " << summary.median
      << ", \"min\": " << summary.minimum
      << ", \"max\": " << summary.maximum
      << ", \"stdev\": " << summary.standard_deviation
      << ", \"sum\": " << summary.sum << '}';
}

void write_double_vector(std::ostream& out,
                         const std::vector<double>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    out << values[i] << (i + 1 < values.size() ? ", " : "");
  }
  out << ']';
}

void write_dfg_performance_value(std::ostream& out,
                                 const DfgPerformanceValue& value) {
  std::visit(
      [&](const auto& selected) {
        using Value = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<Value, double>) {
          out << selected;
        } else if constexpr (std::is_same_v<Value, std::vector<double>>) {
          write_double_vector(out, selected);
        } else {
          write_dfg_performance_summary(out, selected);
        }
      },
      value);
}

void write_dfg_named_attribute_counts(
    std::ostream& out, const DfgNamedAttributeCounts& attributes) {
  out << '[';
  std::size_t attribute_index = 0;
  for (const auto& [attribute, values] : attributes) {
    if (attribute_index++ != 0) out << ", ";
    out << "{\"attribute\": ";
    write_json_string(out, attribute);
    out << ", \"values\": [";
    std::size_t value_index = 0;
    for (const auto& [value, count] : values) {
      if (value_index++ != 0) out << ", ";
      out << "{\"value\": ";
      write_json_string(out, value);
      out << ", \"count\": " << count << '}';
    }
    out << "]}";
  }
  out << ']';
}

void write_dfg_statistical_analysis(
    std::ostream& out, const DfgNamedStatisticalAnalysis& named,
    const DirectlyFollowsGraph& graph) {
  out << "{\"variant\": ";
  write_json_string(out, named.name);
  const auto& analysis = named.analysis;
  switch (analysis.variant) {
    case DfgStatisticalVariant::kNative:
      out << ", \"frequency_dfg\": ";
      write_dfg_frequency(out, analysis.frequency_dfg, graph);
      break;
    case DfgStatisticalVariant::kFrequencyTriples: {
      out << ", \"frequency_triples\": [";
      std::size_t emitted = 0;
      for (const auto& [triple, count] : analysis.frequency_triples) {
        if (emitted++ != 0) out << ", ";
        out << "{\"first\": "
            << dfg_variant_activity_id(graph, std::get<0>(triple))
            << ", \"middle\": "
            << dfg_variant_activity_id(graph, std::get<1>(triple))
            << ", \"last\": "
            << dfg_variant_activity_id(graph, std::get<2>(triple))
            << ", \"count\": " << count << '}';
      }
      out << ']';
      break;
    }
    case DfgStatisticalVariant::kPerformance: {
      out << ", \"performance_dfg\": [";
      std::size_t emitted = 0;
      for (const auto& [pair, statistics] : analysis.performance_dfg) {
        if (emitted++ != 0) out << ", ";
        out << "{\"source\": "
            << dfg_variant_activity_id(graph, pair.first)
            << ", \"target\": "
            << dfg_variant_activity_id(graph, pair.second)
            << ", \"summary\": ";
        write_dfg_performance_summary(out, statistics.summary);
        out << ", \"raw_values\": ";
        write_double_vector(out, statistics.raw_values);
        out << ", \"value\": ";
        write_dfg_performance_value(out, statistics.value);
        out << '}';
      }
      out << ']';
      break;
    }
    case DfgStatisticalVariant::kCaseAttributes: {
      out << ", \"case_attributes\": {\"edges\": [";
      std::size_t emitted = 0;
      for (const auto& [pair, attributes] : analysis.case_attributes.edges) {
        if (emitted++ != 0) out << ", ";
        out << "{\"source\": "
            << dfg_variant_activity_id(graph, pair.first)
            << ", \"target\": "
            << dfg_variant_activity_id(graph, pair.second)
            << ", \"attributes\": ";
        write_dfg_named_attribute_counts(out, attributes);
        out << '}';
      }
      out << "], \"nodes\": [";
      emitted = 0;
      for (const auto& [activity, attributes] :
           analysis.case_attributes.nodes) {
        if (emitted++ != 0) out << ", ";
        out << "{\"activity\": "
            << dfg_variant_activity_id(graph, activity)
            << ", \"attributes\": ";
        write_dfg_named_attribute_counts(out, attributes);
        out << '}';
      }
      out << "]}";
      break;
    }
    case DfgStatisticalVariant::kClean:
    case DfgStatisticalVariant::kCleanPolars:
      out << ", \"clean\": {\"dfg\": ";
      write_dfg_frequency(out, analysis.clean.graph, graph);
      out << ", \"start_activities\": [";
      {
        std::size_t emitted = 0;
        for (const auto& [activity, count] :
             analysis.clean.start_activities) {
          if (emitted++ != 0) out << ", ";
          out << "{\"activity\": "
              << dfg_variant_activity_id(graph, activity)
              << ", \"count\": " << count << '}';
        }
      }
      out << "], \"end_activities\": [";
      {
        std::size_t emitted = 0;
        for (const auto& [activity, count] : analysis.clean.end_activities) {
          if (emitted++ != 0) out << ", ";
          out << "{\"activity\": "
              << dfg_variant_activity_id(graph, activity)
              << ", \"count\": " << count << '}';
        }
      }
      out << "]}";
      break;
    case DfgStatisticalVariant::kCleanTime: {
      out << ", \"clean_time\": [";
      std::size_t emitted = 0;
      for (const auto& [activity, value] : analysis.clean_time) {
        if (emitted++ != 0) out << ", ";
        out << "{\"activity\": "
            << dfg_variant_activity_id(graph, activity)
            << ", \"mean_seconds_from_case_start\": " << value << '}';
      }
      out << ']';
      break;
    }
  }
  out << '}';
}

void write_dfg_variants(std::ostream& out,
                        const DfgVariantsAnalysis& variants,
                        const DirectlyFollowsGraph& graph) {
  out << "{\"statistical\": [";
  for (std::size_t i = 0; i < variants.statistical.size(); ++i) {
    write_dfg_statistical_analysis(out, variants.statistical[i], graph);
    out << (i + 1 < variants.statistical.size() ? ", " : "");
  }
  out << "], \"causal\": [";
  for (std::size_t i = 0; i < variants.causal.size(); ++i) {
    const auto& named = variants.causal[i];
    out << "{\"variant\": ";
    write_json_string(out, named.name);
    out << ", \"relations\": [";
    std::size_t emitted = 0;
    for (const auto& [pair, value] : named.analysis.relations) {
      if (emitted++ != 0) out << ", ";
      out << "{\"source\": " << dfg_variant_activity_id(graph, pair.first)
          << ", \"target\": " << dfg_variant_activity_id(graph, pair.second)
          << ", \"value\": "
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << value << '}';
    }
    out << "]}";
    out << (i + 1 < variants.causal.size() ? ", " : "");
  }
  out << "]}";
}

void validate_correlation_mining(
    const CorrelationVariantsAnalysis& variants,
    const DirectlyFollowsGraph& graph) {
  const auto validate_pair = [&](const DfgActivityPair& pair) {
    (void)dfg_variant_activity_id(graph, pair.first);
    (void)dfg_variant_activity_id(graph, pair.second);
  };
  for (const auto& named : variants.variants) {
    std::optional<CorrelationVariant> named_variant;
    if (named.name == "classic") {
      named_variant = CorrelationVariant::kClassic;
    } else if (named.name == "classic_split") {
      named_variant = CorrelationVariant::kClassicSplit;
    } else if (named.name == "trace_based") {
      named_variant = CorrelationVariant::kTraceBased;
    }
    if (!named_variant || *named_variant != named.analysis.variant) {
      throw std::invalid_argument(
          "correlation-miner variant name and result do not agree");
    }
    const auto& analysis = named.analysis;
    if (analysis.frequency_dfg.size() != analysis.performance_dfg.size()) {
      throw std::invalid_argument(
          "correlation frequency/performance DFGs must have the same edges");
    }
    for (const auto& [activity, count] : analysis.activity_counts) {
      (void)dfg_variant_activity_id(graph, activity);
      if (count == 0) {
        throw std::invalid_argument(
            "correlation activity counts must be positive");
      }
    }
    const auto validate_observed_pair = [&](const DfgActivityPair& pair) {
      validate_pair(pair);
      if (!analysis.activity_counts.contains(pair.first) ||
          !analysis.activity_counts.contains(pair.second)) {
        throw std::invalid_argument(
            "correlation edge references an unobserved activity");
      }
    };
    std::map<std::string, std::uint64_t> outgoing;
    std::map<std::string, std::uint64_t> incoming;
    const auto accumulate = [](std::uint64_t& total, std::uint64_t value) {
      if (value > std::numeric_limits<std::uint64_t>::max() - total) {
        throw std::invalid_argument("correlation frequency count overflow");
      }
      total += value;
    };
    for (const auto& [pair, count] : analysis.frequency_dfg) {
      validate_observed_pair(pair);
      if (count == 0 || !analysis.performance_dfg.contains(pair)) {
        throw std::invalid_argument(
            "correlation frequency edge is invalid");
      }
      accumulate(outgoing[pair.first], count);
      accumulate(incoming[pair.second], count);
    }
    for (const auto& [pair, value] : analysis.performance_dfg) {
      validate_observed_pair(pair);
      if (!analysis.frequency_dfg.contains(pair) || !std::isfinite(value) ||
          value < 0.0) {
        throw std::invalid_argument(
            "correlation performance edge is invalid");
      }
    }
    for (const auto& [activity, count] : analysis.activity_counts) {
      if (outgoing[activity] != count || incoming[activity] != count) {
        throw std::invalid_argument(
            "correlation frequency marginals must match activity counts");
      }
    }
    for (const auto& [pair, value] :
         analysis.precede_succeed_probability) {
      validate_observed_pair(pair);
      if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            "correlation precede/succeed probability must be finite");
      }
    }
    for (const auto& [pair, value] : analysis.duration_matrix) {
      validate_observed_pair(pair);
      if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            "correlation duration value must be finite");
      }
    }
    const auto matrix_keys =
        [](const std::map<DfgActivityPair, double>& matrix) {
          std::set<DfgActivityPair> keys;
          for (const auto& [pair, value] : matrix) {
            (void)value;
            keys.insert(pair);
          }
          return keys;
        };
    std::set<DfgActivityPair> expected_matrix_pairs;
    for (const auto& [source, source_count] : analysis.activity_counts) {
      (void)source_count;
      for (const auto& [target, target_count] : analysis.activity_counts) {
        (void)target_count;
        expected_matrix_pairs.emplace(source, target);
      }
    }
    if (matrix_keys(analysis.precede_succeed_probability) !=
            expected_matrix_pairs ||
        matrix_keys(analysis.duration_matrix) != expected_matrix_pairs) {
      throw std::invalid_argument(
          "correlation diagnostic matrices must contain every activity pair");
    }
  }
}

void write_correlation_matrix(
    std::ostream& out, const std::map<DfgActivityPair, double>& matrix,
    const DirectlyFollowsGraph& graph, std::string_view value_key) {
  out << '[';
  std::size_t emitted = 0;
  for (const auto& [pair, value] : matrix) {
    if (emitted++ != 0) out << ", ";
    out << "{\"source\": " << dfg_variant_activity_id(graph, pair.first)
        << ", \"target\": " << dfg_variant_activity_id(graph, pair.second)
        << ", ";
    write_json_string(out, value_key);
    out << ": " << value << '}';
  }
  out << ']';
}

void write_correlation_mining(
    std::ostream& out, const CorrelationVariantsAnalysis& variants,
    const DirectlyFollowsGraph& graph) {
  out << "{\"variants\": [";
  for (std::size_t i = 0; i < variants.variants.size(); ++i) {
    const auto& named = variants.variants[i];
    out << "{\"variant\": ";
    write_json_string(out, named.name);
    out << ", \"activity_counts\": [";
    std::size_t activity_index = 0;
    for (const auto& [activity, count] : named.analysis.activity_counts) {
      if (activity_index++ != 0) out << ", ";
      out << "{\"activity\": "
          << dfg_variant_activity_id(graph, activity)
          << ", \"count\": " << count << '}';
    }
    out << "], \"frequency_dfg\": ";
    write_dfg_frequency(out, named.analysis.frequency_dfg, graph);
    out << ", \"performance_dfg\": [";
    std::size_t emitted = 0;
    for (const auto& [pair, value] : named.analysis.performance_dfg) {
      if (emitted++ != 0) out << ", ";
      out << "{\"source\": " << dfg_variant_activity_id(graph, pair.first)
          << ", \"target\": " << dfg_variant_activity_id(graph, pair.second)
          << ", \"seconds\": " << value << '}';
    }
    out << "], \"precede_succeed_probability\": ";
    write_correlation_matrix(
        out, named.analysis.precede_succeed_probability, graph, "value");
    out << ", \"duration_matrix\": ";
    write_correlation_matrix(
        out, named.analysis.duration_matrix, graph, "value");
    out << '}';
    out << (i + 1 < variants.variants.size() ? ", " : "");
  }
  out << "]}";
}

template <typename Counts>
void validate_overlap_vector(const Counts& counts, const char* name) {
  using Value = typename Counts::value_type;
  for (const auto value : counts) {
    // Native overlap counters are unsigned integers, but retain these checks
    // so the export contract remains safe if a floating-point counter type is
    // supplied by a downstream adapter.
    const auto numeric = static_cast<long double>(value);
    if (!std::isfinite(numeric) ||
        (std::is_signed_v<Value> && value < static_cast<Value>(0))) {
      throw std::invalid_argument(std::string(name) +
                                  " must contain finite non-negative counts");
    }
  }
}

void validate_overlap_counts(
    const EventLog& log,
    const IntervalEventOverlapCounts* interval_event_overlap_counts,
    const CaseOverlapCounts* case_overlap_counts,
    const ConcurrentActivityOverlapResult*
        concurrent_activity_overlap = nullptr) {
  if (interval_event_overlap_counts != nullptr &&
      interval_event_overlap_counts->size() != log.event_count()) {
    throw std::invalid_argument(
        "interval event overlap counts must match event count");
  }
  if (interval_event_overlap_counts != nullptr) {
    validate_overlap_vector(*interval_event_overlap_counts,
                            "interval event overlap counts");
  }
  if (case_overlap_counts != nullptr &&
      case_overlap_counts->size() != log.trace_count()) {
    throw std::invalid_argument("case overlap counts must match trace count");
  }
  if (case_overlap_counts != nullptr) {
    validate_overlap_vector(*case_overlap_counts, "case overlap counts");
  }
  if (concurrent_activity_overlap != nullptr) {
    for (const auto& [pair, count] : *concurrent_activity_overlap) {
      if (pair.first > pair.second) {
        throw std::invalid_argument(
            "concurrent activity overlap pair must be lexicographically ordered");
      }
      const auto numeric = static_cast<long double>(count);
      if (!std::isfinite(numeric) || count < 0) {
        throw std::invalid_argument(
            "concurrent activity overlap counts must be finite non-negative counts");
      }
    }
  }
}

void write_overlap(
    std::ostream& out,
    const IntervalEventOverlapCounts* interval_event_overlap_counts,
    const CaseOverlapCounts* case_overlap_counts,
    const ConcurrentActivityOverlapResult* concurrent_activity_overlap) {
  out << '{';
  bool first = true;
  if (interval_event_overlap_counts != nullptr) {
    out << "\"interval_events\": [";
    for (std::size_t i = 0; i < interval_event_overlap_counts->size(); ++i) {
      if (i != 0) out << ", ";
      out << (*interval_event_overlap_counts)[i];
    }
    out << ']';
    first = false;
  }
  if (case_overlap_counts != nullptr) {
    if (!first) out << ", ";
    out << "\"cases\": [";
    for (std::size_t i = 0; i < case_overlap_counts->size(); ++i) {
      if (i != 0) out << ", ";
      out << (*case_overlap_counts)[i];
    }
    out << ']';
    first = false;
  }
  if (concurrent_activity_overlap != nullptr) {
    if (!first) out << ", ";
    out << "\"concurrent_activities\": [";
    std::size_t emitted = 0;
    for (const auto& [pair, count] : *concurrent_activity_overlap) {
      if (emitted++ != 0) out << ", ";
      out << "{\"first\": ";
      write_json_string(out, pair.first);
      out << ", \"second\": ";
      write_json_string(out, pair.second);
      out << ", \"count\": " << count << '}';
    }
    out << ']';
  }
  out << '}';
}

}  // namespace

std::string token_replay_traces_json(
    const std::vector<TokenReplayTraceDiagnostics>& traces,
    TokenReplayVariant variant) {
  if (variant != TokenReplayVariant::kForward && variant != TokenReplayVariant::kBackwards)
    throw std::invalid_argument("unsupported token replay variant");
  std::ostringstream out;
  out << "{\"family\":\"TOKEN_REPLAY\",\"variant\":\""
      << (variant == TokenReplayVariant::kForward ? "forward" : "backwards")
      << "\",\"traces\":[";
  for (std::size_t i = 0; i < traces.size(); ++i) {
    if (i) out << ',';
    write_token_replay_trace(out, traces[i]);
  }
  out << "]}";
  return out.str();
}

void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& log,
                         const DirectlyFollowsGraph& graph,
                         const ProcessTree* tree,
                         const PetriNet* net,
                         const HeuristicsNet* heuristics,
                         const std::vector<ModelConformance>* conformance,
                         const PerformanceMetrics* performance,
                         const ResourceNetwork* resources,
                         const std::vector<DurationStats>* variant_durations,
                         const std::vector<EventuallyFollowsEdge>* eventually_follows,
                         const DecisionMiningResult* decisions,
                         const ModelDecoration* petri_decoration,
                         const PerformanceSpectrum* spectrum,
                         const PetriNet* inductive_net,
                         const ModelDecoration* inductive_decoration,
                         const PetriNet* heuristic_petri,
                         const ModelDecoration* heuristic_decoration,
                         const TemporalProfile* temporal_profile,
                         const MinimumSelfDistances* minimum_self_distances,
                         const TransitionSystem* transition_system,
                         const PowlNode* powl,
                         const PetriNet* powl_petri,
                         const ModelDecoration* powl_decoration,
                         const LogSkeletonModel* log_skeleton,
                         const std::vector<LogSkeletonConformance>*
                             log_skeleton_conformance,
                         const DeclareModel* declare,
                         const std::vector<DeclareConformance>*
                             declare_conformance,
                         const FootprintsAnalysis* footprints,
                         const DfgVariantsAnalysis* dfg_variants,
                         const CorrelationVariantsAnalysis*
                             correlation_mining,
                         const BatchDiscovery* batch_discovery,
                         const DfgAlignmentLogResult* dfg_alignment,
                         const std::vector<EditDistanceAlignment>* edit_distance,
                         bool edit_anti,
                         const DfgAlignmentProjection* dfg_projection,
                         const EventLog* dfg_projected_log,
                         const EventLog* edit_projected_log,
                         const ProcessTreeAlignmentResult* process_tree_alignment,
                         const DecomposedAlignmentResult* decomposed_alignment,
                         const PetriAntiAlignmentResult* petri_anti_alignment,
                         const PetriMultiAlignmentResult* petri_multi_alignment,
                         const ProcessTree* reduced_process_tree,
                         std::size_t reduced_trace_index,
                         const std::vector<ModelEvaluation>* evaluation,
                         const EmdResult* earth_movers_distance,
                         const IntervalEventOverlapCounts*
                             interval_event_overlap_counts,
                         const CaseOverlapCounts* case_overlap_counts,
                         const ConcurrentActivityOverlapResult*
                             concurrent_activity_overlap,
                         const DecisionTablesResult* decision_tables,
                         MinimumSelfDistanceVariant minimum_self_distance_variant,
                         const std::vector<EditDistanceTypedAlignment>*
                             typed_edit_distance) {
  // Resolve and validate the optional profile before writing any document
  // bytes, so a bad manually assembled profile cannot leave partial JSON in
  // the caller's stream.
  const auto resolved_temporal_profile =
      temporal_profile != nullptr
          ? resolve_temporal_profile(*temporal_profile, graph)
          : std::vector<ResolvedTemporalProfileEntry>{};
  const auto resolved_minimum_self_distances =
      minimum_self_distances != nullptr
          ? resolve_minimum_self_distances(*minimum_self_distances, graph)
          : std::vector<ResolvedMinimumSelfDistance>{};
  if (minimum_self_distances != nullptr) {
    (void)minimum_self_distance_variant_name(minimum_self_distance_variant);
  }
  const auto resolved_transition_system =
      transition_system != nullptr
          ? resolve_transition_system(*transition_system, log, graph)
          : ResolvedTransitionSystem{};
  if (log_skeleton != nullptr) {
    validate_log_skeleton_model(*log_skeleton, graph);
  }
  if (log_skeleton_conformance != nullptr) {
    validate_log_skeleton_conformance(log, *log_skeleton_conformance, graph);
  }
  if (declare != nullptr) validate_declare_model(*declare, graph);
  if (declare_conformance != nullptr) {
    validate_declare_conformance(log, *declare_conformance, graph);
  }
  if (footprints != nullptr) {
    validate_footprints_analysis(*footprints, log, graph);
  }
  if (dfg_variants != nullptr) {
    validate_dfg_variants(*dfg_variants, graph);
  }
  if (correlation_mining != nullptr) {
    validate_correlation_mining(*correlation_mining, graph);
  }
  if (spectrum != nullptr) validate_performance_spectrum(*spectrum, graph);
  if (batch_discovery != nullptr) {
    validate_batch_discovery(*batch_discovery, log, graph);
  }
  if (evaluation != nullptr) {
    std::set<std::string> miners;
    for (const auto& result : *evaluation) {
      if (result.miner.empty()) {
        throw std::invalid_argument(
            "evaluation result requires a non-empty miner name");
      }
      if (!miners.insert(result.miner).second) {
        throw std::invalid_argument("duplicate evaluation miner: " +
                                    result.miner);
      }
      const std::size_t families =
          static_cast<std::size_t>(result.replay_fitness.has_value()) +
          static_cast<std::size_t>(result.precision.has_value()) +
          static_cast<std::size_t>(result.generalization.has_value()) +
          static_cast<std::size_t>(result.simplicity.has_value()) +
          static_cast<std::size_t>(result.composite.has_value());
      if (families != 1) {
        throw std::invalid_argument(
            "evaluation result must contain exactly one family");
      }
    }
  }
  validate_overlap_counts(log, interval_event_overlap_counts,
                          case_overlap_counts, concurrent_activity_overlap);
  out << "{\n";
  out << "  \"dataset\": ";
  write_json_string(out, dataset);
  out << ",\n";
  out << "  \"generator\": \"minera " << version_major << '.' << version_minor
      << '.' << version_patch << "\",\n";
  out << "  \"trace_count\": " << log.trace_count() << ",\n";
  out << "  \"event_count\": " << log.event_count() << ",\n";

  out << "  \"log_attributes\": ";
  write_attribute_object(out, log.attributes);
  out << ",\n  \"log_attribute_details\": ";
  write_attribute_details(out, log.attributes);
  out << ",\n  \"log_metadata\": ";
  write_projected_metadata(out, log.metadata);
  out << ",\n";

  out << "  \"traces\": [\n";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& trace = log.traces[i];
    out << "    {\"case_id\": ";
    write_json_string(out, trace.case_id);
    out << ", \"case_id_state\": ";
    write_json_string(
        out, first_class_state_name(
                 effective_first_class_state(trace.case_id_state,
                                             trace.case_id)));
    out << ", \"case_id_source_key\": ";
    write_json_string(out, trace.case_id_source_key);
    write_optional_attribute_detail(out, "case_id_attribute",
                                    trace.case_id_attribute);
    if (!trace.attributes.empty()) {
      out << ", \"attributes\": ";
      write_attribute_object(out, trace.attributes);
      out << ", \"attribute_details\": ";
      write_attribute_details(out, trace.attributes);
    }
    if (!trace.properties.empty()) {
      out << ", \"properties\": ";
      write_attribute_object(out, trace.properties);
      out << ", \"property_details\": ";
      write_attribute_details(out, trace.properties);
    }
    out << ", \"events\": [";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      const auto& event = trace.events[j];
      out << "{\"activity\": " << *graph.activity_id(event.activity)
          << ", \"timestamp\": ";
      write_json_string(out, event.timestamp);
      out << ", \"input_order\": " << event.input_order;
      out << ", \"case_id_state\": ";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(event.case_id_state,
                                               event.case_id)));
      out << ", \"case_id_source_key\": ";
      write_json_string(out, event.case_id_source_key);
      out << ", \"activity_state\": ";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(event.activity_state,
                                               event.activity)));
      out << ", \"activity_source_key\": ";
      write_json_string(out, event.activity_source_key);
      write_optional_attribute_detail(out, "activity_attribute",
                                      event.activity_attribute);
      out << ", \"timestamp_state\": ";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(event.timestamp_state,
                                               event.timestamp)));
      out << ", \"timestamp_source_key\": ";
      write_json_string(out, event.timestamp_source_key);
      write_optional_attribute_detail(out, "timestamp_attribute",
                                      event.timestamp_attribute);
      if (effective_first_class_state(event.resource_state, event.resource) !=
          FirstClassAttributeState::kMissing) {
        out << ", \"resource\": ";
        write_json_string(out, event.resource);
      }
      out << ", \"resource_state\": ";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(event.resource_state,
                                               event.resource)));
      out << ", \"resource_source_key\": ";
      write_json_string(out, event.resource_source_key);
      write_optional_attribute_detail(out, "resource_attribute",
                                      event.resource_attribute);
      if (effective_first_class_state(event.lifecycle_state,
                                      event.lifecycle) !=
          FirstClassAttributeState::kMissing) {
        out << ", \"lifecycle\": ";
        write_json_string(out, event.lifecycle);
      }
      out << ", \"lifecycle_state\": ";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(event.lifecycle_state,
                                               event.lifecycle)));
      out << ", \"lifecycle_source_key\": ";
      write_json_string(out, event.lifecycle_source_key);
      write_optional_attribute_detail(out, "lifecycle_attribute",
                                      event.lifecycle_attribute);
      if (!event.attributes.empty()) {
        out << ", \"attributes\": ";
        write_attribute_object(out, event.attributes);
        out << ", \"attribute_details\": ";
        write_attribute_details(out, event.attributes);
      }
      out << '}' << (j + 1 < trace.events.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < log.traces.size() ? "," : "") << '\n';
  }
  out << "  ],\n";

  out << "  \"activities\": ";
  write_activity_stats(out, graph);
  out << ",\n";

  out << "  \"edges\": [\n";
  const auto& edges = graph.edges();
  for (std::size_t i = 0; i < edges.size(); ++i) {
    out << "    {\"source\": " << edges[i].source
        << ", \"target\": " << edges[i].target
        << ", \"count\": " << edges[i].count << '}'
        << (i + 1 < edges.size() ? "," : "") << '\n';
  }
  out << "  ],\n";

  // Sort an index permutation so any aligned per-variant data (durations) stays
  // matched to its variant after the most-frequent-first reordering.
  const auto& variants = graph.variants();
  std::vector<std::size_t> variant_order(variants.size());
  std::iota(variant_order.begin(), variant_order.end(), std::size_t{0});
  std::stable_sort(variant_order.begin(), variant_order.end(),
                   [&](std::size_t a, std::size_t b) {
                     return variants[a].count > variants[b].count;
                   });
  out << "  \"variants\": [\n";
  for (std::size_t i = 0; i < variant_order.size(); ++i) {
    const auto idx = variant_order[i];
    const auto& variant = variants[idx];
    out << "    {\"activities\": [";
    for (std::size_t j = 0; j < variant.activities.size(); ++j) {
      out << *graph.activity_id(variant.activities[j])
          << (j + 1 < variant.activities.size() ? ", " : "");
    }
    out << "], \"count\": " << variant.count;
    if (variant_durations != nullptr && idx < variant_durations->size()) {
      out << ", \"duration\": ";
      write_duration_stats(out, (*variant_durations)[idx]);
    }
    out << '}' << (i + 1 < variant_order.size() ? "," : "") << '\n';
  }
  out << "  ]";

  if (temporal_profile != nullptr) {
    out << ",\n  \"temporal_profile\": ";
    write_temporal_profile(out, resolved_temporal_profile);
  }
  if (minimum_self_distances != nullptr) {
    out << ",\n  \"minimum_self_distance_variant\": \""
        << minimum_self_distance_variant_name(minimum_self_distance_variant)
        << "\",\n  \"minimum_self_distances\": ";
    write_minimum_self_distances(out, resolved_minimum_self_distances);
  }
  if (transition_system != nullptr) {
    out << ",\n  \"transition_system\": ";
    write_transition_system(out, *transition_system, resolved_transition_system, graph);
  }

  write_event_attribute_stats(out, log);
  write_case_attribute_stats(out, log);

  if (tree != nullptr) {
    out << ",\n  \"process_tree\": ";
    write_process_tree_node(out, tree->root(), *tree, graph);
  }
  if (powl != nullptr) {
    out << ",\n  \"powl\": ";
    write_powl_node(out, *powl, graph);
  }
  if (net != nullptr) {
    out << ",\n  \"petri_net\": ";
    write_petri_net(out, *net, graph);
  }
  if (net != nullptr && petri_decoration != nullptr) {
    out << ",\n  \"petri_net_decoration\": ";
    write_model_decoration(out, *petri_decoration);
  }
  if (inductive_net != nullptr) {
    out << ",\n  \"inductive_petri_net\": ";
    write_petri_net(out, *inductive_net, graph);
    if (inductive_decoration != nullptr) {
      out << ",\n  \"inductive_petri_net_decoration\": ";
      write_model_decoration(out, *inductive_decoration);
    }
  }
  if (heuristic_petri != nullptr) {
    out << ",\n  \"heuristic_petri_net\": ";
    write_petri_net(out, *heuristic_petri, graph);
    if (heuristic_decoration != nullptr) {
      out << ",\n  \"heuristic_petri_net_decoration\": ";
      write_model_decoration(out, *heuristic_decoration);
    }
  }
  if (powl_petri != nullptr) {
    out << ",\n  \"powl_petri_net\": ";
    write_petri_net(out, *powl_petri, graph);
    if (powl_decoration != nullptr) {
      out << ",\n  \"powl_petri_net_decoration\": ";
      write_model_decoration(out, *powl_decoration);
    }
  }
  if (log_skeleton != nullptr) {
    out << ",\n  \"log_skeleton\": ";
    write_log_skeleton_model(out, *log_skeleton, graph);
  }
  if (log_skeleton_conformance != nullptr) {
    out << ",\n  \"log_skeleton_conformance\": ";
    write_log_skeleton_conformance(out, log, *log_skeleton_conformance, graph);
  }
  if (declare != nullptr) {
    out << ",\n  \"declare\": ";
    write_declare_model(out, *declare, graph);
  }
  if (declare_conformance != nullptr) {
    out << ",\n  \"declare_conformance\": ";
    write_declare_conformance(out, log, *declare_conformance, graph);
  }
  if (footprints != nullptr) {
    out << ",\n  \"footprints\": ";
    write_footprints_analysis(out, *footprints, log, graph);
  }
  if (dfg_variants != nullptr) {
    out << ",\n  \"dfg_variants\": ";
    write_dfg_variants(out, *dfg_variants, graph);
  }
  if (correlation_mining != nullptr) {
    out << ",\n  \"correlation_mining\": ";
    write_correlation_mining(out, *correlation_mining, graph);
  }
  if (heuristics != nullptr) {
    out << ",\n  \"heuristics_net\": ";
    write_heuristics_net(out, *heuristics, graph);
  }
  if (conformance != nullptr) {
    out << ",\n  \"conformance\": ";
    write_conformance_models(out, *conformance, graph);
  }
  if (performance != nullptr) {
    out << ",\n  \"performance\": ";
    write_performance(out, *performance);
  }
  if (resources != nullptr) {
    out << ",\n  \"resources\": ";
    write_resources(out, *resources);
  }
  if (eventually_follows != nullptr) {
    out << ",\n  \"eventually_follows\": [\n";
    for (std::size_t i = 0; i < eventually_follows->size(); ++i) {
      const auto& edge = (*eventually_follows)[i];
      out << "    {\"source\": " << edge.source
          << ", \"target\": " << edge.target << ", \"count\": " << edge.count
          << '}' << (i + 1 < eventually_follows->size() ? "," : "") << '\n';
    }
    out << "  ]";
  }
  if (decisions != nullptr) {
    out << ",\n  \"decisions\": ";
    write_decisions(out, *decisions);
  }
  if (spectrum != nullptr) {
    out << ",\n  \"performance_spectrum\": ";
    write_performance_spectrum(out, *spectrum);
  }
  if (batch_discovery != nullptr) {
    out << ",\n  \"batch_discovery\": ";
    write_batch_discovery(out, *batch_discovery, graph);
  }
  if (dfg_alignment != nullptr) {
    out << ",\n  \"dfg_alignment\": ";
    // The embedded shape intentionally matches the standalone writer.
    out << dfg_alignment_json(*dfg_alignment, dfg_projection, dfg_projected_log);
  }
  if (typed_edit_distance != nullptr) {
    out << ",\n  \"edit_distance\": ";
    out << typed_edit_distance_json(*typed_edit_distance, edit_anti,
                              edit_projected_log);
  } else if (edit_distance != nullptr) {
    out << ",\n  \"edit_distance\": ";
    out << edit_distance_json(*edit_distance, edit_anti, edit_projected_log);
  }
  if (process_tree_alignment != nullptr) {
    out << ",\n  \"process_tree_alignment\": ";
    write_process_tree_alignment(out, *process_tree_alignment);
  }
  if (decomposed_alignment != nullptr) {
    out << ",\n  \"decomposed_alignment\": " << decomposed_alignment_json(*decomposed_alignment);
  }
  if (petri_anti_alignment != nullptr) {
    out << ",\n  \"petri_anti_alignment\": " << petri_anti_alignment_json(*petri_anti_alignment);
  }
  if (petri_multi_alignment != nullptr) {
    out << ",\n  \"petri_multi_alignment\": " << petri_multi_alignment_json(*petri_multi_alignment);
  }
  if (reduced_process_tree != nullptr) {
    out << ",\n  \"process_tree_reduction\": "
        << process_tree_reduction_json(*reduced_process_tree, graph,
                                       reduced_trace_index);
  }
  if (evaluation != nullptr) {
    out << ",\n  \"evaluation\": {";
    for (std::size_t i = 0; i < evaluation->size(); ++i) {
      if (i != 0) out << ", ";
      write_json_string(out, (*evaluation)[i].miner);
      out << ": ";
      write_model_evaluation(out, (*evaluation)[i], &graph, false);
    }
    out << '}';
  }
  if (earth_movers_distance != nullptr) {
    out << ",\n  \"earth_movers_distance\": ";
    write_emd(out, *earth_movers_distance);
  }
  if (interval_event_overlap_counts != nullptr || case_overlap_counts != nullptr ||
      concurrent_activity_overlap != nullptr) {
    out << ",\n  \"overlap\": ";
    write_overlap(out, interval_event_overlap_counts, case_overlap_counts,
                  concurrent_activity_overlap);
  }
  if (decision_tables != nullptr) {
    out << ",\n  \"decision_tables\": "
        << serialize_decision_tables_json(*decision_tables);
  }
  out << '\n';
  out << "}\n";
}

std::string analysis_json(std::string_view dataset,
                          const EventLog& log,
                          const DirectlyFollowsGraph& graph,
                          const ProcessTree* tree,
                          const PetriNet* net,
                          const HeuristicsNet* heuristics,
                          const std::vector<ModelConformance>* conformance,
                          const PerformanceMetrics* performance,
                          const ResourceNetwork* resources,
                          const std::vector<DurationStats>* variant_durations,
                          const std::vector<EventuallyFollowsEdge>* eventually_follows,
                          const DecisionMiningResult* decisions,
                          const ModelDecoration* petri_decoration,
                          const PerformanceSpectrum* spectrum,
                          const PetriNet* inductive_net,
                          const ModelDecoration* inductive_decoration,
                          const PetriNet* heuristic_petri,
                          const ModelDecoration* heuristic_decoration,
                          const TemporalProfile* temporal_profile,
                          const MinimumSelfDistances* minimum_self_distances,
                          const TransitionSystem* transition_system,
                          const PowlNode* powl,
                          const PetriNet* powl_petri,
                          const ModelDecoration* powl_decoration,
                          const LogSkeletonModel* log_skeleton,
                          const std::vector<LogSkeletonConformance>*
                              log_skeleton_conformance,
                          const DeclareModel* declare,
                          const std::vector<DeclareConformance>*
                              declare_conformance,
                          const FootprintsAnalysis* footprints,
                          const DfgVariantsAnalysis* dfg_variants,
                          const CorrelationVariantsAnalysis*
                              correlation_mining,
                          const BatchDiscovery* batch_discovery,
                          const DfgAlignmentLogResult* dfg_alignment,
                          const std::vector<EditDistanceAlignment>* edit_distance,
                          bool edit_anti,
                          const DfgAlignmentProjection* dfg_projection,
                          const EventLog* dfg_projected_log,
                          const EventLog* edit_projected_log,
                          const ProcessTreeAlignmentResult* process_tree_alignment,
                          const DecomposedAlignmentResult* decomposed_alignment,
                          const PetriAntiAlignmentResult* petri_anti_alignment,
                          const PetriMultiAlignmentResult* petri_multi_alignment,
                          const ProcessTree* reduced_process_tree,
                          std::size_t reduced_trace_index,
                          const std::vector<ModelEvaluation>* evaluation,
                          const EmdResult* earth_movers_distance,
                          const IntervalEventOverlapCounts*
                              interval_event_overlap_counts,
                          const CaseOverlapCounts* case_overlap_counts,
                          const ConcurrentActivityOverlapResult*
                              concurrent_activity_overlap,
                          const DecisionTablesResult* decision_tables,
                          MinimumSelfDistanceVariant minimum_self_distance_variant,
                          const std::vector<EditDistanceTypedAlignment>*
                              typed_edit_distance) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph, tree, net, heuristics,
                      conformance, performance, resources, variant_durations,
                      eventually_follows, decisions, petri_decoration,
                      spectrum, inductive_net, inductive_decoration,
                      heuristic_petri, heuristic_decoration, temporal_profile,
                      minimum_self_distances, transition_system, powl,
                      powl_petri, powl_decoration, log_skeleton,
                      log_skeleton_conformance, declare, declare_conformance,
                      footprints, dfg_variants, correlation_mining,
                      batch_discovery, dfg_alignment, edit_distance, edit_anti,
                      dfg_projection, dfg_projected_log, edit_projected_log,
                      process_tree_alignment, decomposed_alignment,
                      petri_anti_alignment, petri_multi_alignment,
                      reduced_process_tree, reduced_trace_index, evaluation,
                      earth_movers_distance, interval_event_overlap_counts,
                      case_overlap_counts, concurrent_activity_overlap,
                      decision_tables, minimum_self_distance_variant,
                      typed_edit_distance);
  return out.str();
}

std::string replay_fitness_json(const ReplayFitnessResult& result,
                                const DirectlyFollowsGraph* graph) {
  std::ostringstream out;
  write_replay_fitness(out, result, graph);
  return out.str();
}

std::string precision_json(const PrecisionResult& result,
                           const DirectlyFollowsGraph* graph) {
  std::ostringstream out;
  write_precision(out, result, graph);
  return out.str();
}

std::string generalization_json(const GeneralizationResult& result) {
  std::ostringstream out;
  write_generalization(out, result);
  return out.str();
}

std::string simplicity_json(const SimplicityResult& result) {
  std::ostringstream out;
  write_simplicity(out, result);
  return out.str();
}

std::string emd_json(const EmdResult& result) {
  std::ostringstream out;
  write_emd(out, result);
  return out.str();
}

std::string model_evaluation_json(const ModelEvaluation& result,
                                  const DirectlyFollowsGraph* graph) {
  const std::size_t families =
      static_cast<std::size_t>(result.replay_fitness.has_value()) +
      static_cast<std::size_t>(result.precision.has_value()) +
      static_cast<std::size_t>(result.generalization.has_value()) +
      static_cast<std::size_t>(result.simplicity.has_value()) +
      static_cast<std::size_t>(result.composite.has_value());
  if (families != 1)
    throw std::invalid_argument(
        "evaluation result must contain exactly one family");
  std::ostringstream out;
  write_model_evaluation(out, result, graph, true);
  return out.str();
}

std::string decisions_json(const DecisionMiningResult& decisions) {
  std::ostringstream out;
  write_decisions(out, decisions);
  return out.str();
}

std::string alignment_json(const AlignmentResult& alignment,
                           const DirectlyFollowsGraph& graph) {
  std::ostringstream out;
  write_alignments(out, alignment, graph);
  return out.str();
}

std::string process_tree_alignment_json(const ProcessTreeAlignmentResult& result) {
  std::ostringstream out;
  write_process_tree_alignment(out, result);
  return out.str();
}

std::string decomposed_alignment_json(const DecomposedAlignmentResult& result) {
  std::ostringstream out;
  out << "{\"algorithm\":\"decomposed_alignment\",\"variant\":\"recompos_maximal\",\"complete\":" << (result.complete?"true":"false")
      << ",\"timed_out\":" << (result.timed_out?"true":"false") << ",\"exhausted\":" << (result.exhausted?"true":"false")
      << ",\"fitness\":" << result.fitness << ",\"best_worst_cost\":" << result.best_worst_cost << ",\"trace_count\":" << result.trace_count << ",\"total_cost\":" << result.total_cost
      << ",\"diagnostics\":{\"decompositions\":" << result.diagnostics.decompositions << ",\"components\":" << result.diagnostics.components << ",\"merges\":" << result.diagnostics.merges << ",\"cache_hits\":" << result.diagnostics.cache_hits << ",\"cache_misses\":" << result.diagnostics.cache_misses << ",\"border_disagreements\":" << result.diagnostics.border_disagreements << ",\"agreement_iterations\":" << result.diagnostics.agreement_iterations << ",\"visited_states\":" << result.diagnostics.visited_states << ",\"queued_states\":" << result.diagnostics.queued_states << ",\"traversed_arcs\":" << result.diagnostics.traversed_arcs << ",\"whole_net_fallback\":" << (result.diagnostics.whole_net_fallback?"true":"false") << ",\"note\":"; write_json_string(out,result.diagnostics.note); out << "},\"traces\":[";
  for (std::size_t i=0;i<result.traces.size();++i) { if(i) out<<','; const auto& t=result.traces[i]; out<<"{\"activities\":["; for(std::size_t j=0;j<t.activities.size();++j){if(j)out<<',';write_json_string(out,t.activities[j]);} out<<"],\"count\":"<<t.count<<",\"complete\":"<<(t.complete?"true":"false")<<",\"timed_out\":"<<(t.timed_out?"true":"false")<<",\"exhausted\":"<<(t.exhausted?"true":"false")<<",\"cost\":"<<t.cost<<",\"fitness\":"<<t.fitness<<",\"best_worst_cost\":"<<t.best_worst_cost<<",\"moves\":["; for(std::size_t j=0;j<t.alignment.size();++j){if(j)out<<',';const auto&m=t.alignment[j];out<<"{\"log\":";if(m.log)write_json_string(out,*m.log);else out<<"null";out<<",\"model\":";if(m.model)write_json_string(out,*m.model);else out<<"null";out<<",\"transition\":"<<(m.transition==static_cast<std::size_t>(-1)?-1:static_cast<long long>(m.transition))<<"}";} out<<"]}"; }
  out << "]}"; return out.str();
}

std::string petri_anti_alignment_json(const PetriAntiAlignmentResult& result) {
  std::ostringstream out; out << "{\"algorithm\":\"petri_anti_alignment\",\"variant\":\"version_discounted_a_star\",\"activities\":["; for(std::size_t i=0;i<result.activities.size();++i){if(i)out<<',';write_json_string(out,result.activities[i]);} out<<"],\"transitions\":["; for(std::size_t i=0;i<result.transitions.size();++i){if(i)out<<',';out<<result.transitions[i];} out<<"],\"cost\":"<<result.cost<<",\"objective\":"<<result.objective<<",\"precision\":"<<result.precision<<",\"diagnostics\":{\"visited_states\":"<<result.diagnostics.visited_states<<",\"queued_states\":"<<result.diagnostics.queued_states<<",\"traversed_arcs\":"<<result.diagnostics.traversed_arcs<<",\"exhausted\":"<<(result.diagnostics.exhausted?"true":"false")<<"}}"; return out.str();
}

std::string petri_multi_alignment_json(const PetriMultiAlignmentResult& result) {
  std::ostringstream out; out << "{\"algorithm\":\"petri_multi_alignment\",\"variant\":\"version_discounted_a_star\",\"activities\":["; for(std::size_t i=0;i<result.activities.size();++i){if(i)out<<',';write_json_string(out,result.activities[i]);} out<<"],\"transitions\":["; for(std::size_t i=0;i<result.transitions.size();++i){if(i)out<<',';out<<result.transitions[i];} out<<"],\"cost\":"<<result.cost<<",\"objective\":"<<result.objective<<",\"max_distance_to_log\":"<<result.max_distance_to_log<<",\"diagnostics\":{\"visited_states\":"<<result.diagnostics.visited_states<<",\"queued_states\":"<<result.diagnostics.queued_states<<",\"traversed_arcs\":"<<result.diagnostics.traversed_arcs<<",\"exhausted\":"<<(result.diagnostics.exhausted?"true":"false")<<"}}"; return out.str();
}

std::string process_tree_reduction_json(const ProcessTree& result,
                                         const DirectlyFollowsGraph& graph,
                                         std::size_t trace_index) {
  std::ostringstream out; out << "{\"algorithm\":\"process_tree_reduction\",\"variant\":\"tree_tr_based\",\"trace_index\":" << trace_index << ",\"process_tree\":"; write_process_tree_node(out,result.root(),result,graph); out << '}'; return out.str();
}

namespace {
const char* attribute_kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
    case AttributeKind::kBytes: return "bytes";
    case AttributeKind::kNA: return "na";
  }
  throw std::invalid_argument("projected log contains an invalid attribute kind");
}

const char* attribute_container_name(AttributeContainerKind container) {
  switch (container) {
    case AttributeContainerKind::kScalar: return "scalar";
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "container";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
  }
  throw std::invalid_argument(
      "projected log contains an invalid attribute container");
}

const char* first_class_state_name(FirstClassAttributeState state) {
  switch (state) {
    case FirstClassAttributeState::kMissing: return "missing";
    case FirstClassAttributeState::kPresent: return "present";
    case FirstClassAttributeState::kNull: return "null";
  }
  throw std::invalid_argument(
      "projected log contains an invalid first-class attribute state");
}

const char* first_class_role_name(FirstClassAttributeRole role) {
  switch (role) {
    case FirstClassAttributeRole::kNone: return "none";
    case FirstClassAttributeRole::kCaseId: return "case_id";
    case FirstClassAttributeRole::kActivity: return "activity";
    case FirstClassAttributeRole::kTimestamp: return "timestamp";
    case FirstClassAttributeRole::kResource: return "resource";
    case FirstClassAttributeRole::kLifecycle: return "lifecycle";
  }
  throw std::invalid_argument(
      "projected table contains an invalid first-class attribute role");
}

FirstClassAttributeState effective_first_class_state(
    FirstClassAttributeState state, std::string_view value) {
  return state == FirstClassAttributeState::kMissing && !value.empty()
             ? FirstClassAttributeState::kPresent
             : state;
}

void write_typed_attribute_value(std::ostream& out,
                                 const NativeAttributeValue& value) {
  if (const auto* integer = std::get_if<std::int64_t>(&value)) {
    out << *integer;
  } else if (const auto* unsigned_integer = std::get_if<std::uint64_t>(&value)) {
    out << *unsigned_integer;
  } else if (const auto* number = std::get_if<double>(&value)) {
    if (std::isfinite(*number))
      out << std::setprecision(17) << *number;
    else if (std::isnan(*number))
      write_json_string(out, "NaN");
    else
      write_json_string(out, *number < 0 ? "-INF" : "INF");
  } else if (const auto* boolean = std::get_if<bool>(&value)) {
    out << (*boolean ? "true" : "false");
  } else if (const auto* date = std::get_if<NativeDate>(&value)) {
    write_json_string(out, date->value);
  } else if (std::holds_alternative<NativeNA>(value)) {
    write_json_string(out, "NA");
  } else {
    out << "null";
  }
}

void write_attribute_details(std::ostream& out,
                             const std::vector<EventAttribute>& attributes);

void write_attribute_detail(std::ostream& out,
                            const EventAttribute& attribute) {
  validate_native_na_attribute(attribute, "log export JSON");
  out << "{\"key\":";
  write_json_string(out, attribute.key);
  out << ",\"value\":";
  write_json_string(out, event_attribute_lexical_value(attribute));
  out << ",\"kind\":";
  write_json_string(out, attribute_kind_name(attribute.kind));
  out << ",\"container\":";
  write_json_string(out, attribute_container_name(attribute.container));
  out << ",\"null\":" << (attribute.is_null ? "true" : "false");
  if (!std::holds_alternative<std::monostate>(attribute.typed_value) ||
      attribute.kind == AttributeKind::kNA) {
    out << ",\"typed_value\":";
    if (attribute.kind == AttributeKind::kNA &&
        std::holds_alternative<std::monostate>(attribute.typed_value))
      write_json_string(out, "NA");
    else
      write_typed_attribute_value(out, attribute.typed_value);
  }
  if (attribute.scalar_wrapper) out << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list) out << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    out << ",\"children\":";
    write_attribute_details(out, attribute.children);
  }
  out << '}';
}

void write_attribute_details(std::ostream& out,
                             const std::vector<EventAttribute>& attributes) {
  out << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i) out << ',';
    write_attribute_detail(out, attributes[i]);
  }
  out << ']';
}

void write_optional_attribute_detail(
    std::ostream& out, std::string_view field,
    const std::optional<EventAttribute>& attribute) {
  if (!attribute) return;
  out << ", \"" << field << "\": ";
  write_attribute_detail(out, *attribute);
}

void write_attribute_kinds(std::ostream& out,
                           const std::vector<EventAttribute>& attributes) {
  out << '{';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, attributes[i].key);
    out << ':';
    write_json_string(out, attribute_kind_name(attributes[i].kind));
  }
  out << '}';
}

void write_projected_metadata(std::ostream& out,
                              const EventDataMetadata& metadata) {
  out << "{\"extensions\":[";
  for (std::size_t i = 0; i < metadata.extensions.size(); ++i) {
    if (i) out << ',';
    const auto& extension = metadata.extensions[i];
    out << "{\"name\":";
    write_json_string(out, extension.name);
    out << ",\"prefix\":";
    write_json_string(out, extension.prefix);
    out << ",\"uri\":";
    write_json_string(out, extension.uri);
    out << '}';
  }
  out << "],\"classifiers\":[";
  for (std::size_t i = 0; i < metadata.classifiers.size(); ++i) {
    if (i) out << ',';
    const auto& classifier = metadata.classifiers[i];
    out << "{\"name\":";
    write_json_string(out, classifier.name);
    out << ",\"keys\":[";
    for (std::size_t j = 0; j < classifier.keys.size(); ++j) {
      if (j) out << ',';
      write_json_string(out, classifier.keys[j]);
    }
    out << "]}";
  }
  out << "],\"trace_globals\":";
  write_attribute_object(out, metadata.trace_globals);
  out << ",\"trace_global_details\":";
  write_attribute_details(out, metadata.trace_globals);
  out << ",\"event_globals\":";
  write_attribute_object(out, metadata.event_globals);
  out << ",\"event_global_details\":";
  write_attribute_details(out, metadata.event_globals);
  out << ",\"properties\":";
  write_attribute_object(out, metadata.properties);
  out << ",\"property_details\":";
  write_attribute_details(out, metadata.properties);
  out << ",\"globals\":[";
  for (std::size_t i = 0; i < metadata.globals.size(); ++i) {
    if (i) out << ',';
    out << "{\"scope\":";
    write_json_string(out, metadata.globals[i].scope);
    out << ",\"attributes\":";
    write_attribute_details(out, metadata.globals[i].attributes);
    out << '}';
  }
  out << ']';
  out << '}';
}

void write_projected_log(std::ostream& out, const EventLog& log) {
  out << "{";
  if (!log.attributes.empty()) {
    out << "\"attributes\":";
    write_attribute_object(out, log.attributes);
    out << ",\"attribute_kinds\":";
    write_attribute_kinds(out, log.attributes);
    out << ",\"attribute_details\":";
    write_attribute_details(out, log.attributes);
    out << ',';
  }
  out << "\"metadata\":";
  write_projected_metadata(out, log.metadata);
  out << ',';
  out << "\"traces\":[";
  for (std::size_t i=0;i<log.traces.size();++i) {
    if (i) out << ',';
    const auto& t=log.traces[i];
    out << "{\"case_id\":"; write_json_string(out,t.case_id);
    out << ",\"case_id_state\":";
    write_json_string(
        out, first_class_state_name(
                 effective_first_class_state(t.case_id_state, t.case_id)));
    out << ",\"case_id_source_key\":";
    write_json_string(out, t.case_id_source_key);
    write_optional_attribute_detail(out, "case_id_attribute",
                                    t.case_id_attribute);
    if(!t.attributes.empty()) {
      out<<",\"attributes\":";
      write_attribute_object(out, t.attributes);
      out<<",\"attribute_kinds\":";
      write_attribute_kinds(out, t.attributes);
      out<<",\"attribute_details\":";
      write_attribute_details(out, t.attributes);
    }
    if(!t.properties.empty()) {
      out<<",\"properties\":";
      write_attribute_object(out, t.properties);
      out<<",\"property_kinds\":";
      write_attribute_kinds(out, t.properties);
      out<<",\"property_details\":";
      write_attribute_details(out, t.properties);
    }
    out << ",\"events\":[";
    for (std::size_t j=0;j<t.events.size();++j) {
      if(j)out<<',';
      const auto& e=t.events[j];
      out<<"{\"activity\":"; write_json_string(out,e.activity);
      out << ",\"timestamp\":"; write_json_string(out,e.timestamp);
      out << ",\"resource\":"; write_json_string(out,e.resource);
      out << ",\"lifecycle\":"; write_json_string(out,e.lifecycle);
      out << ",\"input_order\":" << e.input_order;
      out << ",\"case_id_state\":";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(e.case_id_state, e.case_id)));
      out << ",\"case_id_source_key\":";
      write_json_string(out, e.case_id_source_key);
      out << ",\"activity_state\":";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(e.activity_state, e.activity)));
      out << ",\"activity_source_key\":";
      write_json_string(out, e.activity_source_key);
      write_optional_attribute_detail(out, "activity_attribute",
                                      e.activity_attribute);
      out << ",\"timestamp_state\":";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(e.timestamp_state, e.timestamp)));
      out << ",\"timestamp_source_key\":";
      write_json_string(out, e.timestamp_source_key);
      write_optional_attribute_detail(out, "timestamp_attribute",
                                      e.timestamp_attribute);
      out << ",\"resource_state\":";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(e.resource_state, e.resource)));
      out << ",\"resource_source_key\":";
      write_json_string(out, e.resource_source_key);
      write_optional_attribute_detail(out, "resource_attribute",
                                      e.resource_attribute);
      out << ",\"lifecycle_state\":";
      write_json_string(
          out, first_class_state_name(
                   effective_first_class_state(e.lifecycle_state, e.lifecycle)));
      out << ",\"lifecycle_source_key\":";
      write_json_string(out, e.lifecycle_source_key);
      write_optional_attribute_detail(out, "lifecycle_attribute",
                                      e.lifecycle_attribute);
      if(!e.attributes.empty()) {
        out<<",\"attributes\":";
        write_attribute_object(out, e.attributes);
        out<<",\"attribute_kinds\":";
        write_attribute_kinds(out, e.attributes);
        out<<",\"attribute_details\":";
        write_attribute_details(out, e.attributes);
      }
      out<<'}';
    }
    out << "]}";
  }
  out << "]}";
}

void write_case_relations_table_attribute(
    std::ostream& out, const EventAttribute& attribute);

void write_case_relations_table_attributes(
    std::ostream& out, const std::vector<EventAttribute>& attributes) {
  out << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i) out << ',';
    write_case_relations_table_attribute(out, attributes[i]);
  }
  out << ']';
}

void write_case_relations_table_attribute(
    std::ostream& out, const EventAttribute& attribute) {
  validate_native_na_attribute(attribute, "case relations JSON");
  out << "{\"key\":";
  write_json_string(out, attribute.key);
  out << ",\"value\":";
  write_json_string(out, event_attribute_lexical_value(attribute));
  out << ",\"kind\":";
  write_json_string(out, attribute_kind_name(attribute.kind));
  out << ",\"container\":";
  write_json_string(out, attribute_container_name(attribute.container));
  out << ",\"null\":" << (attribute.is_null ? "true" : "false");
  if (!std::holds_alternative<std::monostate>(attribute.typed_value) ||
      attribute.kind == AttributeKind::kNA) {
    out << ",\"typed_value\":";
    if (attribute.kind == AttributeKind::kNA &&
        std::holds_alternative<std::monostate>(attribute.typed_value))
      write_json_string(out, "NA");
    else
      write_typed_attribute_value(out, attribute.typed_value);
  }
  if (attribute.scalar_wrapper) out << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list)
    out << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    out << ",\"children\":";
    write_case_relations_table_attributes(out, attribute.children);
  }
  if (attribute.first_class_role != FirstClassAttributeRole::kNone) {
    out << ",\"first_class_role\":";
    write_json_string(out, first_class_role_name(attribute.first_class_role));
    out << ",\"first_class_state\":";
    write_json_string(out, first_class_state_name(attribute.first_class_state));
    out << ",\"first_class_source_key\":";
    write_json_string(out, attribute.first_class_source_key);
  }
  out << '}';
}

void write_case_relations_table_index_label(
    std::ostream& out, const EventTableIndexLabel& label) {
  out << "{\"kind\":";
  if (const auto* scalar = std::get_if<EventAttribute>(&label.value)) {
    out << "\"scalar\",\"value\":";
    write_case_relations_table_attribute(out, *scalar);
  } else if (const auto* tuple =
                 std::get_if<std::vector<EventTableIndexLabel>>(&label.value)) {
    out << "\"tuple\",\"items\":[";
    for (std::size_t index = 0; index < tuple->size(); ++index) {
      if (index != 0) out << ',';
      write_case_relations_table_index_label(out, (*tuple)[index]);
    }
    out << ']';
  } else {
    throw std::invalid_argument(
        "case relations table contains an invalid index label");
  }
  out << '}';
}

void write_case_relations_table_metadata(std::ostream& out,
                                         const EventTable& table) {
  if (!table.dtypes.empty()) {
    out << ",\"dtypes\":[";
    for (std::size_t index = 0; index < table.dtypes.size(); ++index) {
      if (index != 0) out << ',';
      write_json_string(out, table.dtypes[index]);
    }
    out << ']';
  }
  if (table.index) {
    out << ",\"index\":{\"class\":";
    write_json_string(out, table.index->kind);
    out << ",\"dtype\":";
    write_json_string(out, table.index->dtype);
    out << ",\"names\":[";
    for (std::size_t index = 0; index < table.index->names.size(); ++index) {
      if (index != 0) out << ',';
      if (table.index->names[index])
        write_json_string(out, *table.index->names[index]);
      else
        out << "null";
    }
    out << "],\"labels\":[";
    for (std::size_t index = 0; index < table.index->labels.size(); ++index) {
      if (index != 0) out << ',';
      write_case_relations_table_index_label(out, table.index->labels[index]);
    }
    out << "]}";
  }
  if (!table.categorical_dtypes.empty()) {
    out << ",\"categorical_dtypes\":[";
    for (std::size_t column = 0; column < table.categorical_dtypes.size();
         ++column) {
      if (column != 0) out << ',';
      const auto& descriptor = table.categorical_dtypes[column];
      if (!descriptor) {
        out << "null";
        continue;
      }
      out << "{\"categories_dtype\":";
      write_json_string(out, descriptor->categories_dtype);
      out << ",\"ordered\":"
          << (descriptor->ordered ? "true" : "false")
          << ",\"categories\":[";
      for (std::size_t category = 0;
           category < descriptor->categories.size(); ++category) {
        if (category != 0) out << ',';
        write_case_relations_table_attribute(out,
                                             descriptor->categories[category]);
      }
      out << "]}";
    }
    out << ']';
  }
}

void write_case_relations_table(std::ostream& out, const EventTable& table) {
  table.validate();
  out << "{\"columns\":[";
  for (std::size_t i = 0; i < table.columns.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, table.columns[i]);
  }
  out << ']';
  write_case_relations_table_metadata(out, table);
  out << ",\"attributes\":";
  write_case_relations_table_attributes(out, table.attributes);
  out << ",\"metadata\":";
  write_projected_metadata(out, table.metadata);
  out << ",\"rows\":[";
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    if (row) out << ',';
    out << '[';
    for (std::size_t column = 0; column < table.rows[row].size(); ++column) {
      if (column) out << ',';
      if (!table.rows[row][column]) {
        out << "null";
      } else {
        write_case_relations_table_attribute(out, *table.rows[row][column]);
      }
    }
    out << ']';
  }
  out << "]}";
}

void clear_first_class_role(EventAttribute& attribute) {
  attribute.first_class_role = FirstClassAttributeRole::kNone;
  attribute.first_class_state = FirstClassAttributeState::kMissing;
  attribute.first_class_source_key.clear();
}

void retag_projection_cell(EventAttribute& attribute,
                           FirstClassAttributeRole role,
                           std::string_view source_key) {
  attribute.first_class_role = role;
  attribute.first_class_state =
      attribute.is_null ? FirstClassAttributeState::kNull
                        : FirstClassAttributeState::kPresent;
  attribute.first_class_source_key = std::string(source_key);
}

EventTable case_relations_log_projection_table(
    const EventTable& source, const CaseRelationsOptions& options) {
  source.validate();
  EventTable projected = source;

  // The merge table retains source adapter roles, including the original
  // CSV case/timestamp roles.  Those roles describe import configuration,
  // while the compatibility log must describe the selectors used by this
  // merge.  Normalize this private copy only; the public table remains a
  // faithful source/result table.
  const auto case_column = std::find(
      projected.columns.begin(), projected.columns.end(), options.case_id_key);
  const auto timestamp_column = std::find(projected.columns.begin(),
                                           projected.columns.end(),
                                           options.timestamp_key);
  std::optional<std::size_t> case_index;
  if (case_column != projected.columns.end())
    case_index = static_cast<std::size_t>(
        std::distance(projected.columns.begin(), case_column));
  std::optional<std::size_t> timestamp_index;
  if (timestamp_column != projected.columns.end())
    timestamp_index = static_cast<std::size_t>(
        std::distance(projected.columns.begin(), timestamp_column));
  if (!timestamp_index) {
    // Compatibility EventLog reconstruction needs an explicit role marker
    // for a selector that is absent from every source row. Keep this
    // synthetic column local to the projection; the public merge table
    // remains unchanged. A declared dtype vector must stay aligned with the
    // temporary schema, including nullable categorical descriptors.
    projected.columns.push_back(options.timestamp_key);
    if (!projected.dtypes.empty()) projected.dtypes.push_back("object");
    if (!projected.categorical_dtypes.empty())
      projected.categorical_dtypes.push_back(std::nullopt);
    for (auto& row : projected.rows) row.push_back(std::nullopt);
    timestamp_index = projected.columns.size() - 1;
  }

  for (auto& row : projected.rows) {
    for (std::size_t index = 0; index < row.size(); ++index) {
      auto& cell = row[index];
      if (!cell) continue;
      if (cell->first_class_role == FirstClassAttributeRole::kCaseId) {
        if (case_index && index == *case_index) {
          retag_projection_cell(*cell, FirstClassAttributeRole::kCaseId,
                                projected.columns[index]);
        } else {
          clear_first_class_role(*cell);
        }
        continue;
      }
      if (cell->first_class_role == FirstClassAttributeRole::kTimestamp) {
        if (timestamp_index && index == *timestamp_index) {
          retag_projection_cell(*cell, FirstClassAttributeRole::kTimestamp,
                                projected.columns[index]);
        } else {
          clear_first_class_role(*cell);
        }
        continue;
      }
      if (case_index && index == *case_index) {
        retag_projection_cell(*cell, FirstClassAttributeRole::kCaseId,
                              projected.columns[index]);
      } else if (timestamp_index && index == *timestamp_index) {
        retag_projection_cell(*cell, FirstClassAttributeRole::kTimestamp,
                              projected.columns[index]);
      }
    }
  }
  return projected;
}

EventLog case_relations_compatibility_log(
    const EventTable& source, const CaseRelationsOptions& options) {
  LogConversionOptions conversion_options;
  conversion_options.case_id_key = options.case_id_key;
  const auto projection_table =
      case_relations_log_projection_table(source, options);
  auto stream = event_table_to_event_stream(projection_table,
                                            conversion_options);

  // event_table_to_event_log has a canonical timestamp fallback for legacy
  // tables.  A custom selector with a missing cell must suppress that
  // fallback for the individual record; a private missing role marker does
  // so without adding a cell to the returned table.
  for (auto& record : stream.events) {
    const auto has_timestamp_role = std::any_of(
        record.fields.begin(), record.fields.end(), [](const auto& field) {
          return field.first_class_role == FirstClassAttributeRole::kTimestamp;
        });
    if (has_timestamp_role) continue;
    EventAttribute missing{options.timestamp_key, "", AttributeKind::kDate};
    missing.first_class_role = FirstClassAttributeRole::kTimestamp;
    missing.first_class_state = FirstClassAttributeState::kMissing;
    missing.first_class_source_key = options.timestamp_key;
    record.fields.push_back(std::move(missing));
  }
  return event_stream_to_event_log(stream, conversion_options);
}
}

std::string dfg_alignment_json(const DfgAlignmentLogResult& result,
                               const DfgAlignmentProjection* projection,
                               const EventLog* projected_log) {
  if (result.unique_variants > result.alignments.size())
    throw std::invalid_argument("DFG alignment unique variants exceed trace count");
  std::ostringstream out;
  out << "{\"family\": \"DFG\", \"variant\": \"CLASSIC\", \"unique_variants\": "
      << result.unique_variants << ", \"alignments\": [";
  for (std::size_t i = 0; i < result.alignments.size(); ++i) {
    const auto& a = result.alignments[i];
    if (!a.computed) {
      if (projection != nullptr || projected_log != nullptr)
        throw std::invalid_argument("DFG projection requires computed alignments");
      out << "{\"computed\": false}";
      if (i + 1 < result.alignments.size()) out << ',';
      continue;
    }
    if (!std::isfinite(a.cost) || a.cost < 0 ||
        !std::isfinite(a.internal_cost) || a.internal_cost < 0 ||
        !std::isfinite(a.fitness) || !std::isfinite(a.best_worst_cost) ||
        a.best_worst_cost < 0)
      throw std::invalid_argument("DFG alignment result has invalid numeric values");
    out << "{\"computed\": true, \"cost\": " << a.cost << ", \"internal_cost\": " << a.internal_cost
        << ", \"fitness\": " << a.fitness << ", \"best_worst_cost\": "
        << a.best_worst_cost << ", \"visited_states\": " << a.visited_states
        << ", \"closed\": " << a.closed << ", \"closed_states\": " << a.closed_states
        << ", \"moves\": [";
    for (std::size_t j = 0; j < a.alignment.size(); ++j) {
      const auto& m = a.alignment[j];
      out << "{\"log\": "; write_json_string(out, m.log);
      out << ", \"model\": "; write_json_string(out, m.model); out << '}';
      if (j + 1 < a.alignment.size()) out << ',';
    }
    out << "]}";
    if (i + 1 < result.alignments.size()) out << ',';
  }
  out << ']';
  if (projection != nullptr) {
    out << ", \"projected_dfg\": {\"edges\": [";
    std::size_t n = 0;
    for (const auto& kv : projection->dfg) {
      if (n++) out << ',';
      out << "{\"source\": "; write_json_string(out, kv.first.first);
      out << ", \"target\": "; write_json_string(out, kv.first.second);
      out << ", \"sync\": " << kv.second.sync << ", \"model_move\": " << kv.second.mm << '}';
    }
    out << "], \"activities\": ["; n = 0;
    for (const auto& kv : projection->activities) {
      if (n++) out << ',';
      out << "{\"activity\": "; write_json_string(out, kv.first);
      out << ", \"sync\": " << kv.second.sync << ", \"model_move\": " << kv.second.mm
          << ", \"log_move\": " << kv.second.lm << '}';
    }
    out << "]}";
  }
  if (projected_log != nullptr) {
    out << ", \"projected_log\": ";
    write_projected_log(out, *projected_log);
  }
  out << '}';
  return out.str();
}

std::string dfg_playout_json(const DfgPlayoutResult& result) {
  const char* variant = result.variant == DfgPlayoutVariant::kPerformance
                            ? "PERFORMANCE"
                            : "CLASSIC";
  std::ostringstream out;
  out << "{\"family\":\"DFG_PLAYOUT\",\"variant\":";
  write_json_string(out, variant);
  out << ",\"return_variants\":true,\"complete\":"
      << (result.complete ? "true" : "false")
      << ",\"interrupted\":"
      << (result.interrupted ? "true" : "false")
      << ",\"completed_traces\":" << result.completed_traces
      << ",\"truncated_traces\":" << result.truncated_traces
      << ",\"variants\":[";
  std::size_t index = 0;
  for (const auto& [activities, occurrences] : result.counts) {
    if (index++ != 0) out << ',';
    out << "{\"activities\":[";
    for (std::size_t i = 0; i < activities.size(); ++i) {
      if (i != 0) out << ',';
      write_json_string(out, activities[i]);
    }
    out << "],\"occurrences\":" << occurrences << '}';
  }
  out << "]}";
  return out.str();
}

std::string edit_distance_json(const std::vector<EditDistanceAlignment>& result,
                               bool anti_alignment,
                               const EventLog* projected_log) {
  std::ostringstream out;
  std::size_t source_events = 0, target_events = 0;
  out << "{\"family\": \"EDIT_DISTANCE\", \"variant\": \"EDIT_DISTANCE\", \"anti\": "
      << (anti_alignment ? "true" : "false") << ", \"source_count\": " << result.size()
      << ", \"alignments\": [";
  for (std::size_t i = 0; i < result.size(); ++i) {
    const auto& a = result[i]; source_events += a.source_variant.size(); target_events += a.target_variant.size();
    if (i) out << ',';
    out << "{\"source_variant\": [";
    for (std::size_t j=0;j<a.source_variant.size();++j){if(j)out<<',';write_json_string(out,a.source_variant[j]);}
    out << "], \"target_variant\": [";
    for (std::size_t j=0;j<a.target_variant.size();++j){if(j)out<<',';write_json_string(out,a.target_variant[j]);}
    out << "], \"target_index\": " << a.target_index << ", \"cost\": " << a.cost
        << ", \"fitness\": " << a.fitness << ", \"best_worst_cost\": " << a.best_worst_cost
        << ", \"bwc\": " << a.bwc << ", \"denominator\": " << a.denominator << ", \"moves\": [";
    for (std::size_t j=0;j<a.alignment.size();++j) { const auto& m=a.alignment[j]; if(j)out<<','; out<<"{\"type\": ";
      const char* type = m.type==EditDistanceMoveType::kSynchronous?"sync":(m.type==EditDistanceMoveType::kLog?"log":"model"); write_json_string(out,type);
      out << ", \"log\": ";
      if (m.type == EditDistanceMoveType::kModel) write_json_string(out, ">>");
      else write_json_string(out, m.log);
      out << ", \"model\": ";
      if (m.type == EditDistanceMoveType::kLog) write_json_string(out, ">>");
      else write_json_string(out, m.model);
      out << '}'; }
    out << "]}";
  }
  out << "], \"source_event_count\": " << source_events
      << ", \"target_event_count\": " << target_events;
  if (projected_log) { out << ", \"projected_log\": "; write_projected_log(out,*projected_log); }
  out << '}'; return out.str();
}

namespace {

void write_edit_distance_double(std::ostream& out, double value) {
  if (std::isnan(value)) {
    out << "{\"kind\":\"float\",\"value\":\"nan\"}";
  } else if (std::isinf(value)) {
    out << "{\"kind\":\"float\",\"value\":\""
        << (value < 0 ? "-inf" : "inf") << "\"}";
  } else {
    const auto flags = out.flags();
    const auto precision = out.precision();
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << std::showpoint << value;
    out.flags(flags);
    out.precision(precision);
  }
}

void write_edit_distance_typed_activity(std::ostream& out,
                                        const EventAttribute& attribute);

void write_edit_distance_typed_items(
    std::ostream& out, std::string_view kind,
    const std::vector<EventAttribute>& children, bool sort_values) {
  std::vector<std::string> items;
  items.reserve(children.size());
  for (const auto& child : children) {
    std::ostringstream item;
    write_edit_distance_typed_activity(item, child);
    items.push_back(item.str());
  }
  if (sort_values) std::sort(items.begin(), items.end());
  out << "{\"kind\":";
  write_json_string(out, kind);
  out << ",\"items\":[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) out << ',';
    out << items[i];
  }
  out << "]}";
}

void write_edit_distance_typed_activity(std::ostream& out,
                                        const EventAttribute& attribute) {
  validate_native_na_attribute(attribute, "edit-distance JSON");
  if (is_native_na_attribute(attribute)) {
    out << "{\"kind\":\"na\"}";
    return;
  }
  if (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull) {
    out << "null";
    return;
  }

  if (attribute.kind == AttributeKind::kBytes) {
    const auto bytes = decode_bytes_attribute(attribute);
    const auto canonical = make_bytes_attribute("", bytes);
    out << "{\"kind\":\"bytes\",\"hex\":";
    write_json_string(out, canonical.value);
    out << '}';
    return;
  }

  if (attribute.container != AttributeContainerKind::kScalar ||
      !attribute.children.empty() || attribute.scalar_wrapper ||
      attribute.scalar_children_list) {
    switch (attribute.container) {
      case AttributeContainerKind::kList:
        write_edit_distance_typed_items(out, "list", attribute.children, false);
        return;
      case AttributeContainerKind::kTuple:
        write_edit_distance_typed_items(out, "tuple", attribute.children, false);
        return;
      case AttributeContainerKind::kSet:
        write_edit_distance_typed_items(out, "set", attribute.children, true);
        return;
      case AttributeContainerKind::kFrozenSet:
        write_edit_distance_typed_items(out, "frozenset", attribute.children, true);
        return;
      case AttributeContainerKind::kContainer:
        out << "{\"kind\":\"mapping\",\"entries\":[";
        for (std::size_t i = 0; i < attribute.children.size(); ++i) {
          if (i) out << ',';
          out << '[';
          write_json_string(out, attribute.children[i].key);
          out << ',';
          write_edit_distance_typed_activity(out, attribute.children[i]);
          out << ']';
        }
        out << "]}";
        return;
      case AttributeContainerKind::kScalar:
        if (attribute.scalar_children_list) {
          write_edit_distance_typed_items(out, "list", attribute.children, false);
        } else if (!attribute.children.empty()) {
          out << "{\"kind\":\"scalar_wrapper\",\"value\":";
          EventAttribute scalar = attribute;
          scalar.children.clear();
          scalar.scalar_wrapper = false;
          scalar.scalar_children_list = false;
          write_edit_distance_typed_activity(out, scalar);
          out << ",\"children\":";
          write_edit_distance_typed_items(out, "list", attribute.children, false);
          out << '}';
        } else {
          // An empty wrapper still carries its scalar value; it falls through
          // to the scalar representation below.
        }
        if (attribute.scalar_children_list) return;
        if (!attribute.children.empty()) return;
        break;
    }
  }

  if (const auto* integer = std::get_if<std::int64_t>(&attribute.typed_value)) {
    out << *integer;
    return;
  }
  if (const auto* unsigned_integer =
          std::get_if<std::uint64_t>(&attribute.typed_value)) {
    out << *unsigned_integer;
    return;
  }
  if (const auto* number = std::get_if<double>(&attribute.typed_value)) {
    write_edit_distance_double(out, *number);
    return;
  }
  if (const auto* boolean = std::get_if<bool>(&attribute.typed_value)) {
    out << (*boolean ? "true" : "false");
    return;
  }
  if (const auto* date = std::get_if<NativeDate>(&attribute.typed_value)) {
    write_json_string(out, date->value);
    return;
  }

  const auto lexical = event_attribute_lexical_value(attribute);
  switch (attribute.kind) {
    case AttributeKind::kInt: {
      std::int64_t signed_value = 0;
      const auto signed_result = std::from_chars(
          lexical.data(), lexical.data() + lexical.size(), signed_value);
      if (signed_result.ec == std::errc{} &&
          signed_result.ptr == lexical.data() + lexical.size()) {
        out << signed_value;
        return;
      }
      if (!lexical.empty() && lexical.front() != '-') {
        std::uint64_t unsigned_value = 0;
        const auto unsigned_result = std::from_chars(
            lexical.data(), lexical.data() + lexical.size(), unsigned_value);
        if (unsigned_result.ec == std::errc{} &&
            unsigned_result.ptr == lexical.data() + lexical.size()) {
          out << unsigned_value;
          return;
        }
      }
      throw std::invalid_argument("edit-distance JSON: invalid integer activity");
    }
    case AttributeKind::kFloat: {
      std::istringstream input{std::string(lexical)};
      input.imbue(std::locale::classic());
      double value = 0.0;
      input >> value;
      input >> std::ws;
      if (!input || !input.eof())
        throw std::invalid_argument("edit-distance JSON: invalid float activity");
      write_edit_distance_double(out, value);
      return;
    }
    case AttributeKind::kBoolean:
      if (lexical == "true" || lexical == "True" || lexical == "1") out << "true";
      else if (lexical == "false" || lexical == "False" || lexical == "0") out << "false";
      else throw std::invalid_argument("edit-distance JSON: invalid boolean activity");
      return;
    case AttributeKind::kNA:
      out << "{\"kind\":\"na\"}";
      return;
    case AttributeKind::kString:
    case AttributeKind::kDate:
    case AttributeKind::kId:
      write_json_string(out, lexical);
      return;
    case AttributeKind::kBytes:
      break;
  }
  throw std::invalid_argument("edit-distance JSON: invalid typed activity kind");
}

void write_edit_distance_optional_activity(
    std::ostream& out, const std::optional<EventAttribute>& activity) {
  if (!activity) {
    write_json_string(out, ">>");
    return;
  }
  write_edit_distance_typed_activity(out, *activity);
}

}  // namespace

std::string typed_edit_distance_json(
    const std::vector<EditDistanceTypedAlignment>& result,
    bool anti_alignment, const EventLog* projected_log) {
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  std::size_t source_events = 0;
  std::size_t target_events = 0;
  out << "{\"family\": \"EDIT_DISTANCE\", \"variant\": \"EDIT_DISTANCE\", \"anti\": "
      << (anti_alignment ? "true" : "false")
      << ", \"source_count\": " << result.size() << ", \"alignments\": [";
  for (std::size_t i = 0; i < result.size(); ++i) {
    const auto& alignment = result[i];
    source_events += alignment.source_variant.size();
    target_events += alignment.target_variant.size();
    if (i) out << ',';
    out << "{\"source_variant\":[";
    for (std::size_t j = 0; j < alignment.source_variant.size(); ++j) {
      if (j) out << ',';
      write_edit_distance_typed_activity(out, alignment.source_variant[j]);
    }
    out << "],\"target_variant\":[";
    for (std::size_t j = 0; j < alignment.target_variant.size(); ++j) {
      if (j) out << ',';
      write_edit_distance_typed_activity(out, alignment.target_variant[j]);
    }
    out << "],\"target_index\":" << alignment.target_index
        << ",\"cost\":" << alignment.cost << ",\"fitness\":";
    write_edit_distance_double(out, alignment.fitness);
    out << ",\"best_worst_cost\":" << alignment.best_worst_cost
        << ",\"bwc\":" << alignment.bwc
        << ",\"denominator\":" << alignment.denominator
        << ",\"moves\":[";
    for (std::size_t j = 0; j < alignment.alignment.size(); ++j) {
      const auto& move = alignment.alignment[j];
      if (j) out << ',';
      out << "{\"type\":";
      const char* type = "sync";
      switch (move.type) {
        case EditDistanceMoveType::kSynchronous: type = "sync"; break;
        case EditDistanceMoveType::kLog: type = "log"; break;
        case EditDistanceMoveType::kModel: type = "model"; break;
      }
      write_json_string(out, type);
      out << ",\"log\":";
      write_edit_distance_optional_activity(out, move.log_activity);
      out << ",\"model\":";
      write_edit_distance_optional_activity(out, move.model_activity);
      out << '}';
    }
    out << "]}";
  }
  out << "],\"source_event_count\":" << source_events
      << ",\"target_event_count\":" << target_events;
  if (projected_log) {
    out << ",\"projected_log\":";
    write_projected_log(out, *projected_log);
  }
  out << '}';
  return out.str();
}

std::string imported_model_json(const ModelConformance& model,
                                const DirectlyFollowsGraph& graph) {
  std::ostringstream out;
  out << "{\"activities\": ";
  write_activity_stats(out, graph);
  out << ", \"model\": ";
  write_model_conformance(out, model, graph);
  out << '}';
  return out.str();
}

std::string petri_usage_comparison_json(const PetriUsageComparison& result) {
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "{\"family\":\"PETRI_USAGE_COMPARISON\",\"places\":[";
  for (std::size_t i = 0; i < result.places.size(); ++i) {
    if (i) out << ',';
    const auto& r = result.places[i];
    out << "{\"id\":" << r.id << ",\"log1_occurrences\":"
        << r.log1_occurrences << ",\"log2_occurrences\":"
        << r.log2_occurrences << ",\"total_occurrences\":"
        << r.total_occurrences << ",\"direction\":" << r.direction
        << ",\"label\":";
    write_json_string(out, r.label);
    out << ",\"color\":";
    write_json_string(out, r.color);
    out << '}';
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < result.transitions.size(); ++i) {
    if (i) out << ',';
    const auto& r = result.transitions[i];
    out << "{\"id\":" << r.id << ",\"log1_occurrences\":"
        << r.log1_occurrences << ",\"log2_occurrences\":"
        << r.log2_occurrences << ",\"total_occurrences\":"
        << r.total_occurrences << ",\"direction\":" << r.direction
        << ",\"label\":";
    write_json_string(out, r.label);
    out << ",\"color\":";
    write_json_string(out, r.color);
    out << '}';
  }
  out << "],\"arcs\":[";
  for (std::size_t i = 0; i < result.arcs.size(); ++i) {
    if (i) out << ',';
    const auto& r = result.arcs[i];
    const char* kind = "input";
    switch (r.kind) {
      case PetriArcUsageKind::kInput: kind = "input"; break;
      case PetriArcUsageKind::kOutput: kind = "output"; break;
      case PetriArcUsageKind::kReset: kind = "reset"; break;
      case PetriArcUsageKind::kInhibitor: kind = "inhibitor"; break;
    }
    out << "{\"id\":" << r.id << ",\"transition\":" << r.transition
        << ",\"place\":" << r.place << ",\"kind\":";
    write_json_string(out, kind);
    out << ",\"input\":"
        << (r.input ? "true" : "false") << ",\"log1_occurrences\":"
        << r.log1_occurrences << ",\"log2_occurrences\":"
        << r.log2_occurrences << ",\"total_occurrences\":"
        << r.total_occurrences << ",\"direction\":" << r.direction
        << ",\"label\":";
    write_json_string(out, r.label);
    out << ",\"color\":";
    write_json_string(out, r.color);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string petri_net_reduction_json(const PetriNetReductionResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"PETRI_NET_REDUCTION\",\"variant\":\"complete\""
      << ",\"changed\":" << (result.changed ? "true" : "false")
      << ",\"place_count\":" << result.net.place_count()
      << ",\"transition_count\":" << result.net.transitions().size()
      << ",\"source\":" << result.net.source()
      << ",\"sink\":" << result.net.sink()
      << ",\"initial_marking\":[";
  for (std::size_t i = 0; i < result.markings.initial.size(); ++i) {
    if (i) out << ',';
    out << result.markings.initial[i];
  }
  out << "],\"final_marking\":[";
  for (std::size_t i = 0; i < result.markings.final.size(); ++i) {
    if (i) out << ',';
    out << result.markings.final[i];
  }
  out << "],\"old_to_new_place\":[";
  for (std::size_t i = 0; i < result.old_to_new_place.size(); ++i) {
    if (i) out << ',';
    if (result.old_to_new_place[i] == kRemovedPlace) out << "null";
    else out << result.old_to_new_place[i];
  }
  out << "],\"pnml\":";
  if (result.markings.initial.empty() && result.markings.final.empty()) {
    write_json_string(out, to_pnml(result.net, "reduced net"));
  } else {
    write_json_string(out, to_pnml(result.net, result.markings.initial,
                                   result.markings.final, "reduced net"));
  }
  out << '}';
  return out.str();
}

std::string pnml_document_json(const PnmlDocument& document) {
  std::ostringstream out;
  out << "{\"family\":\"PNML_DOCUMENT\",\"variant\":\"pnml\""
      << ",\"place_count\":" << document.net.place_count()
      << ",\"transition_count\":" << document.net.transitions().size()
      << ",\"source\":" << document.net.source()
      << ",\"sink\":" << document.net.sink()
      << ",\"initial_marking\":[";
  for (std::size_t i = 0; i < document.initial_marking.size(); ++i) {
    if (i) out << ',';
    out << document.initial_marking[i];
  }
  out << "],\"final_marking\":[";
  for (std::size_t i = 0; i < document.final_marking.size(); ++i) {
    if (i) out << ',';
    out << document.final_marking[i];
  }
  out << ']';
  const auto names = [&](const char* key, const std::vector<std::string>& values) {
    if (values.empty()) return;
    out << ",\"" << key << "\":[";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ',';
      write_json_string(out, values[i]);
    }
    out << ']';
  };
  const auto keys = [&](const char* key, const std::vector<bool>& values) {
    if (values.empty()) return;
    out << ",\"" << key << "\":[";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ',';
      out << (values[i] ? "true" : "false");
    }
    out << ']';
  };
  names("place_ids", document.place_ids);
  names("transition_ids", document.transition_ids);
  keys("initial_marking_keys", document.initial_marking_keys);
  keys("final_marking_keys", document.final_marking_keys);
  out << ",\"transitions\":[";
  for (std::size_t i = 0; i < document.net.transitions().size(); ++i) {
    if (i) out << ',';
    const auto& transition = document.net.transitions()[i];
    out << "{\"activity\":";
    if (transition.activity) {
      write_json_string(out, document.net.activities()[*transition.activity]);
    } else {
      out << "null";
    }
    const auto places = [&](const char* name,
                            const std::vector<std::size_t>& values) {
      out << ",\"" << name << "\":[";
      for (std::size_t j = 0; j < values.size(); ++j) {
        if (j) out << ',';
        out << values[j];
      }
      out << ']';
    };
    places("inputs", transition.inputs);
    places("outputs", transition.outputs);
    places("reset_inputs", transition.reset_inputs);
    places("inhibitor_inputs", transition.inhibitor_inputs);
    out << '}';
  }
  out << "],\"stochastic_map\":";
  if (document.stochastic_map)
    out << serialize_stochastic_map_json(*document.stochastic_map);
  else
    out << "null";
  out << '}';
  return out.str();
}

namespace {

std::string serialize_playout_visited_json_variant(
    const PetriNet& net, const PlayoutVisitedResult& result,
    std::string_view variant) {
  std::ostringstream out;
  out << "{\"family\":\"PETRI_PLAYOUT_VISITED\",\"variant\":";
  write_json_string(out, variant);
  out << ",\"truncated\":" << (result.truncated ? "true" : "false")
      << ",\"trace_limit_reached\":"
      << (result.trace_limit_reached ? "true" : "false")
      << ",\"state_limit_reached\":"
      << (result.state_limit_reached ? "true" : "false")
      << ",\"traces\":[";
  for (std::size_t i = 0; i < result.traces.size(); ++i) {
    if (i) out << ',';
    out << '[';
    const auto& trace = result.traces[i];
    for (std::size_t j = 0; j < trace.size(); ++j) {
      if (j) out << ',';
      const auto& element = trace[j];
      if (element.kind == PlayoutVisitedElementKind::MARKING) {
        out << "{\"kind\":\"marking\",\"marking\":[";
        for (std::size_t p = 0; p < element.marking.size(); ++p) {
          if (p) out << ',';
          out << element.marking[p];
        }
        out << "]}";
      } else {
        out << "{\"kind\":\"transition\",\"transition\":"
            << element.transition_index;
        if (element.transition_index < net.transitions().size()) {
          const auto& transition = net.transitions()[element.transition_index];
          out << ",\"activity\":";
          if (transition.activity &&
              *transition.activity < net.activities().size()) {
            write_json_string(out, net.activities()[*transition.activity]);
          } else {
            out << "null";
          }
        }
        out << '}';
      }
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

}  // namespace

std::string serialize_playout_visited_json(
    const PetriNet& net, const PlayoutVisitedResult& result) {
  return serialize_playout_visited_json_variant(net, result, "BASIC_PLAYOUT");
}

std::string serialize_stochastic_playout_visited_json(
    const PetriNet& net, const PlayoutVisitedResult& result) {
  return serialize_playout_visited_json_variant(net, result,
                                                "STOCHASTIC_PLAYOUT");
}

std::string serialize_extensive_playout_visited_json(
    const PetriNet& net, const PlayoutVisitedResult& result) {
  return serialize_playout_visited_json_variant(net, result,
                                                "EXTENSIVE_PLAYOUT");
}

std::string serialize_stochastic_petri_json(
    const StochasticPetriResult& result) {
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "{\"family\":\"STOCHASTIC_PETRI\",\"variant\":\"TANGIBLE_CTMC\",\"exhausted\":"
      << (result.exhausted ? "true" : "false") << ",\"diagnostic\":";
  write_json_string(out, result.diagnostic);
  out << ",\"initial_state\":";
  if (result.initial_state)
    out << *result.initial_state;
  else
    out << "null";
  out << ",\"initial_probabilities\":[";
  for (std::size_t index = 0; index < result.initial_probabilities.size();
       ++index) {
    if (index) out << ',';
    write_finite_or_null(out, result.initial_probabilities[index]);
  }
  out << ']';
  out << ",\"states\":[";
  for (std::size_t i = 0; i < result.states.size(); ++i) {
    if (i) out << ',';
    out << "[";
    for (std::size_t p = 0; p < result.states[i].marking.size(); ++p) {
      if (p) out << ',';
      out << result.states[i].marking[p];
    }
    out << "]";
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < result.edges.size(); ++i) {
    if (i) out << ',';
    const auto& edge = result.edges[i];
    out << "{\"source\":" << edge.source << ",\"target\":"
        << edge.target << ",\"transition\":" << edge.transition
        << ",\"rate\":" << edge.rate << '}';
  }
  out << "],\"generator\":[";
  for (std::size_t i = 0; i < result.generator.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.generator[i].size(); ++j) {
      if (j) out << ',';
      out << result.generator[i][j];
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

std::string serialize_stochastic_petri_distribution_json(
    const StochasticPetriDistributionResult& result) {
  const auto variant = [&]() -> std::string_view {
    switch (result.variant) {
      case StochasticPetriAnalysisVariant::kTransient:
        return "TRANSIENT";
      case StochasticPetriAnalysisVariant::kSteadyState:
        return "STEADY_STATE";
      case StochasticPetriAnalysisVariant::kReachability:
        return "REACHABILITY";
    }
    return "UNKNOWN";
  }();
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "{\"family\":\"STOCHASTIC_PETRI_DISTRIBUTION\",\"variant\":\""
      << variant << "\",\"computed\":" << (result.computed ? "true" : "false")
      << ",\"unique\":" << (result.unique ? "true" : "false")
      << ",\"time_seconds\":";
  write_finite_or_null(out, result.time_seconds);
  out << ",\"diagnostic\":";
  write_json_string(out, result.diagnostic);
  out << ",\"initial_state\":";
  if (result.initial_state)
    out << *result.initial_state;
  else
    out << "null";
  out << ",\"probabilities\":[";
  for (std::size_t index = 0; index < result.probabilities.size(); ++index) {
    if (index) out << ',';
    write_finite_or_null(out, result.probabilities[index]);
  }
  out << "]}";
  return out.str();
}

std::string serialize_process_tree_playout_json(
    const ProcessTreePlayoutResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"PROCESS_TREE_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"return_set_strings\":true,\"truncated\":"
      << (result.truncated ? "true" : "false") << ",\"traces\":[";
  for (std::size_t i = 0; i < result.trace_strings.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.trace_strings[i].size(); ++j) {
      if (j) out << ',';
      write_json_string(out, result.trace_strings[i][j]);
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

std::string concept_drift_json(const ConceptDriftResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  const std::string_view variant =
      result.variant == ConceptDriftVariant::kBose ? "bose" : "unknown";
  out << "{\"family\":\"CONCEPT_DRIFT\",\"variant\":";
  write_json_string(out, variant);
  out << ",\"activity_attribute\":";
  write_json_string(out, result.activity_attribute);
  out << ",\"timestamp_attribute\":";
  write_json_string(out, result.timestamp_attribute);
  out << ",\"case_id_attribute\":";
  write_json_string(out, result.case_id_attribute);
  out << ",\"activities\":[";
  for (std::size_t i = 0; i < result.activities.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, result.activities[i]);
  }
  out << "],\"feature_vectors\":[";
  for (std::size_t i = 0; i < result.feature_vectors.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.feature_vectors[i].size(); ++j) {
      if (j) out << ',';
      out << result.feature_vectors[i][j];
    }
    out << ']';
  }
  out << "],\"sublogs\":[";
  for (std::size_t i = 0; i < result.sublogs.size(); ++i) {
    if (i) out << ',';
    out << "{\"trace_count\":" << result.sublogs[i].trace_count()
        << ",\"event_count\":" << result.sublogs[i].event_count() << '}';
  }
  out << "],\"returned_sublogs\":[";
  for (std::size_t i = 0; i < result.returned_sublogs.size(); ++i) {
    if (i) out << ',';
    out << "{\"trace_count\":" << result.returned_sublogs[i].trace_count()
        << ",\"event_count\":" << result.returned_sublogs[i].event_count()
        << '}';
  }
  out << "],\"cumulative_sublogs\":[";
  for (std::size_t i = 0; i < result.cumulative_sublogs.size(); ++i) {
    if (i) out << ',';
    out << "{\"trace_count\":" << result.cumulative_sublogs[i].trace_count()
        << ",\"event_count\":" << result.cumulative_sublogs[i].event_count() << '}';
  }
  out << "],\"sublog_start_timestamps\":[";
  for (std::size_t i = 0; i < result.sublog_start_timestamps.size(); ++i) {
    if (i) out << ',';
    out << result.sublog_start_timestamps[i];
  }
  out << "],\"change_points\":[";
  for (std::size_t i = 0; i < result.change_points.size(); ++i) {
    if (i) out << ',';
    out << "{\"sublog_index\":" << result.change_points[i].sublog_index
        << ",\"p_value\":" << result.change_points[i].p_value << '}';
  }
  out << "],\"change_timestamps\":[";
  for (std::size_t i = 0; i < result.change_timestamps.size(); ++i) {
    if (i) out << ',';
    out << result.change_timestamps[i];
  }
  out << "]}";
  return out.str();
}

std::string log_to_target_json(const LogToTargetResult& result) {
  const auto variant_name = [&]() -> std::string_view {
    switch (result.variant) {
      case LogToTargetVariant::NEXT_ACTIVITY: return "next_activity";
      case LogToTargetVariant::NEXT_TIME: return "next_time";
      case LogToTargetVariant::REMAINING_TIME: return "remaining_time";
    }
    throw std::invalid_argument("log-to-target result has an unknown variant");
  };
  std::ostringstream out;
  out << "{\"family\":\"LOG_TO_TARGET\",\"variant\":";
  write_json_string(out, variant_name());
  out << ",\"trace_count\":" << result.trace_count
      << ",\"target_width\":" << result.target_width
      << ",\"padded\":" << (result.padded ? "true" : "false")
      << ",\"classes\":[";
  for (std::size_t i = 0; i < result.classes.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, result.classes[i]);
  }
  out << "],\"rows\":[";
  for (std::size_t i = 0; i < result.rows.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < result.rows[i].size(); ++j) {
      if (j) out << ',';
      out << '[';
      for (std::size_t k = 0; k < result.rows[i][j].size(); ++k) {
        if (k) out << ',';
        out << result.rows[i][j][k];
      }
      out << ']';
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

std::string log_to_features_json(const LogToFeaturesResult& result) {
  const auto variant_name = [&]() -> std::string_view {
    switch (result.variant) {
      case LogToFeaturesVariant::EVENT_BASED: return "event_based";
      case LogToFeaturesVariant::TRACE_BASED: return "trace_based";
      case LogToFeaturesVariant::TEMPORAL: return "temporal";
    }
    throw std::invalid_argument("log_to_features: unknown result variant");
  };
  std::set<std::string> unique_features;
  for (const auto& name : result.feature_names)
    if (result.variant == LogToFeaturesVariant::TEMPORAL &&
        (name.empty() || !unique_features.insert(name).second))
      throw std::invalid_argument("log_to_features: invalid result feature schema");
  const auto valid_row = [&](const auto& row) {
    return row.size() == result.feature_names.size();
  };
  if (result.variant == LogToFeaturesVariant::EVENT_BASED) {
    if (!result.trace_rows.empty() || !result.temporal_rows.empty())
      throw std::invalid_argument("log_to_features: mixed result row families");
    for (const auto& trace : result.event_rows)
      for (const auto& row : trace)
        if (!valid_row(row))
          throw std::invalid_argument("log_to_features: invalid event result row");
  } else if (result.variant == LogToFeaturesVariant::TRACE_BASED) {
    if (!result.event_rows.empty() || !result.temporal_rows.empty() ||
        (!result.case_ids.empty() && result.case_ids.size() != result.trace_rows.size()))
      throw std::invalid_argument("log_to_features: inconsistent trace result");
    for (const auto& row : result.trace_rows) {
      if (!valid_row(row))
        throw std::invalid_argument("log_to_features: invalid trace result row");
      for (std::size_t i = 0; i < row.size(); ++i)
        if (std::holds_alternative<EventAttribute>(row[i]) &&
            (i != 0 || result.feature_names[i] != "@@case_id_column"))
          throw std::invalid_argument("log_to_features: typed cell outside identifier column");
    }
  } else if (result.variant == LogToFeaturesVariant::TEMPORAL) {
    if (!result.event_rows.empty() || !result.trace_rows.empty() ||
        !result.case_ids.empty())
      throw std::invalid_argument("log_to_features: mixed temporal result");
    for (const auto& row : result.temporal_rows) {
      if (!std::isfinite(row.timestamp) || !valid_row(row.values))
        throw std::invalid_argument("log_to_features: invalid temporal result row");
      if (row.timestamp_ns) {
        const auto& exact = *row.timestamp_ns;
        const auto first = !exact.empty() && exact.front() == '-' ? std::size_t{1} : std::size_t{0};
        if (first == exact.size() ||
            (exact[first] == '0' && (first != 0 || exact.size() != 1)) ||
            !std::all_of(exact.begin() + first, exact.end(), [](char digit) { return digit >= '0' && digit <= '9'; }))
          throw std::invalid_argument("log_to_features: invalid exact temporal timestamp");
      }
      for (std::size_t i = 0; i < row.values.size(); ++i)
        if (const auto* value = std::get_if<std::string>(&row.values[i]); value &&
            (!value->empty() || result.feature_names[i] != "total_number_of_reworked_activities"))
          throw std::invalid_argument("log_to_features: invalid temporal string cell");
    }
  }
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  const auto write_number = [&](double value) {
    if (std::isnan(value)) write_json_string(out, "NaN");
    else if (std::isinf(value)) write_json_string(out, value < 0 ? "-Infinity" : "Infinity");
    else out << value;
  };
  const auto safe_integer_text = [](std::string_view canonical) {
    const auto magnitude = canonical.front() == '-' ? canonical.substr(1) : canonical;
    constexpr std::string_view limit = "9007199254740991";
    return magnitude.size() < limit.size() ||
        (magnitude.size() == limit.size() && magnitude <= limit);
  };
  const auto normalize_identifier_detail = [&](auto&& self, EventAttribute& attribute) -> void {
    if (!attribute.is_null && attribute.container == AttributeContainerKind::kScalar &&
        attribute.kind != AttributeKind::kBytes &&
        (std::holds_alternative<std::int64_t>(attribute.typed_value) ||
         std::holds_alternative<std::uint64_t>(attribute.typed_value) ||
         (attribute.kind == AttributeKind::kInt &&
          std::holds_alternative<std::monostate>(attribute.typed_value)))) {
      const auto canonical = root_cause_attribute_category(attribute);
      if (!safe_integer_text(canonical)) {
        attribute.kind = AttributeKind::kInt;
        attribute.value = canonical;
        attribute.typed_value = std::monostate{};
      }
    }
    for (auto& child : attribute.children) self(self, child);
  };
  const auto write_identifier = [&](const EventAttribute& attribute) {
    if (is_native_na_attribute(attribute)) {
      validate_native_na_attribute(attribute, "log_to_features");
      write_attribute_detail(out, attribute);
      return;
    }
    if (attribute.is_null) { out << "null"; return; }
    if (attribute.container != AttributeContainerKind::kScalar ||
        attribute.kind == AttributeKind::kBytes || !attribute.children.empty()) {
      auto exact = attribute;
      normalize_identifier_detail(normalize_identifier_detail, exact);
      write_attribute_detail(out, exact);
      return;
    }
    const auto write_integer = [&] {
      const auto canonical = root_cause_attribute_category(attribute);
      if (safe_integer_text(canonical)) {
        out << canonical;
      } else {
        auto exact = attribute;
        exact.kind = AttributeKind::kInt;
        exact.value = canonical;
        exact.typed_value = std::monostate{};
        write_attribute_detail(out, exact);
      }
    };
    if (std::holds_alternative<std::int64_t>(attribute.typed_value)) {
      write_integer();
      return;
    }
    if (const auto* value = std::get_if<std::uint64_t>(&attribute.typed_value)) {
      const auto canonical = std::to_string(*value);
      if (safe_integer_text(canonical)) {
        out << canonical;
      } else {
        auto exact = attribute;
        exact.kind = AttributeKind::kInt;
        exact.value = canonical;
        exact.typed_value = std::monostate{};
        write_attribute_detail(out, exact);
      }
      return;
    }
    if (const auto* value = std::get_if<double>(&attribute.typed_value)) {
      write_number(*value);
      return;
    }
    if (const auto* value = std::get_if<bool>(&attribute.typed_value)) {
      out << (*value ? "true" : "false");
      return;
    }
    if (const auto* value = std::get_if<NativeDate>(&attribute.typed_value)) {
      write_json_string(out, value->value);
      return;
    }
    switch (attribute.kind) {
      case AttributeKind::kInt: {
        write_integer();
        return;
      }
      case AttributeKind::kFloat: {
        write_number(native_attribute_number(attribute));
        return;
      }
      case AttributeKind::kBoolean:
        if (attribute.value == "true" || attribute.value == "1") out << "true";
        else if (attribute.value == "false" || attribute.value == "0") out << "false";
        else throw std::invalid_argument("log_to_features: invalid boolean identifier");
        return;
      case AttributeKind::kString:
      case AttributeKind::kDate:
      case AttributeKind::kId:
        write_json_string(out, attribute.value);
        return;
      case AttributeKind::kBytes: break;
    }
    throw std::invalid_argument("log_to_features: invalid identifier attribute kind");
  };
  out << "{\"family\":\"LOG_TO_FEATURES\",\"variant\":";
  write_json_string(out, variant_name());
  out << ",\"feature_names\":[";
  for (std::size_t i = 0; i < result.feature_names.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, result.feature_names[i]);
  }
  out << "]";
  if (!result.case_ids.empty()) {
    out << ",\"case_ids\":[";
    for (std::size_t i = 0; i < result.case_ids.size(); ++i) {
      if (i) out << ',';
      write_json_string(out, result.case_ids[i]);
    }
    out << ']';
  }
  if (result.variant == LogToFeaturesVariant::EVENT_BASED) {
    out << ",\"event_rows\":[";
    for (std::size_t i = 0; i < result.event_rows.size(); ++i) {
      if (i) out << ',';
      out << '[';
      for (std::size_t j = 0; j < result.event_rows[i].size(); ++j) {
        if (j) out << ',';
        out << '[';
        for (std::size_t k = 0; k < result.event_rows[i][j].size(); ++k) {
          if (k) out << ',';
          write_number(result.event_rows[i][j][k]);
        }
        out << ']';
      }
      out << ']';
    }
    out << ']';
  } else if (result.variant == LogToFeaturesVariant::TRACE_BASED) {
    out << ",\"trace_rows\":[";
    for (std::size_t i = 0; i < result.trace_rows.size(); ++i) {
      if (i) out << ',';
      out << '[';
      for (std::size_t j = 0; j < result.trace_rows[i].size(); ++j) {
        if (j) out << ',';
        if (const auto* value = std::get_if<double>(&result.trace_rows[i][j]))
          write_number(*value);
        else
          write_identifier(std::get<EventAttribute>(result.trace_rows[i][j]));
      }
      out << ']';
    }
    out << ']';
  } else {
    out << ",\"temporal_rows\":[";
    for (std::size_t i = 0; i < result.temporal_rows.size(); ++i) {
      if (i) out << ',';
      const auto& row = result.temporal_rows[i];
      out << "{\"timestamp\":" << row.timestamp;
      if (row.timestamp_ns) {
        out << ",\"timestamp_ns\":";
        write_json_string(out, *row.timestamp_ns);
      }
      out << ",\"values\":[";
      for (std::size_t j = 0; j < row.values.size(); ++j) {
        if (j) out << ',';
        if (const auto* value = std::get_if<double>(&row.values[j])) write_number(*value);
        else write_json_string(out, std::get<std::string>(row.values[j]));
      }
      out << "]}";
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string label_splitting_json(const LabelSplittingResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"LABEL_SPLITTING\",\"variant\":\"contextual\",\"log\":";
  write_projected_log(out, result.log);
  out << '}';
  return out.str();
}

namespace {
void write_trie_node_json(std::ostream& out, const TrieNode& node) {
  out << "{\"label\":";
  write_json_string(out, node.label);
  out << ",\"depth\":" << node.depth << ",\"final\":"
      << (node.final ? "true" : "false") << ",\"children\":[";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    if (i) out << ',';
    write_trie_node_json(out, node.children[i]);
  }
  out << "]}";
}
}  // namespace

std::string trie_json(const TrieModel& result) {
  std::ostringstream out;
  out << "{\"family\":\"LOG_TO_TRIE\",\"input_traces\":"
      << result.input_traces << ",\"distinct_variants\":"
      << result.distinct_variants << ",\"node_count\":" << result.node_count
      << ",\"truncated\":" << (result.truncated ? "true" : "false")
      << ",\"root\":";
  write_trie_node_json(out, result.root);
  out << '}';
  return out.str();
}

namespace {
void write_interval_tree_intervals(std::ostream& out,
                                   const std::vector<OpenPathInterval>& result) {
  for (std::size_t i = 0; i < result.size(); ++i) {
    if (i) out << ',';
    const auto& interval = result[i];
    out << "{\"start\":" << interval.start << ",\"end\":"
        << interval.end << ",\"case_id\":";
    write_json_string(out, interval.case_id);
    out << ",\"source_activity\":";
    write_json_string(out, interval.source_activity);
    out << ",\"target_activity\":";
    write_json_string(out, interval.target_activity);
    out << ",\"trace_index\":" << interval.trace_index
        << ",\"source_event_index\":" << interval.source_event_index
        << ",\"target_event_index\":" << interval.target_event_index
        << '}';
  }
}

void write_index_array(std::ostream& out, const std::vector<std::size_t>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  out << ']';
}
}  // namespace

std::string interval_tree_json(const std::vector<OpenPathInterval>& result) {
  const auto query_result = query_open_path_intervals(result);
  return interval_tree_json(query_result);
}

std::string interval_tree_json(const IntervalTreeQueryResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"LOG_TO_INTERVAL_TREE\",\"interval_count\":"
      << result.interval_count << ",\"intervals\":[";
  write_interval_tree_intervals(out, result.intervals);
  out << ']';
  if (result.point) {
    out << ",\"point_query\":" << *result.point
        << ",\"point_matches\":";
    write_index_array(out, result.point_matches);
  }
  if (result.range) {
    out << ",\"range_query\":[" << result.range->first << ','
        << result.range->second << "],\"range_matches\":";
    write_index_array(out, result.range_matches);
  }
  out << '}';
  return out.str();
}

std::string case_relations_json(const EventLog& result,
                                CaseRelationsVariant variant) {
  if (variant != CaseRelationsVariant::kPandas)
    throw std::invalid_argument("case relations result has an unknown variant");
  std::ostringstream out;
  out << "{\"family\":\"MERGE_CASE_RELATIONS\",\"variant\":\"pandas\",\"log\":";
  write_projected_log(out, result);
  out << '}';
  return out.str();
}

std::string case_relations_json(const EventTable& result,
                                const CaseRelationsOptions& options) {
  if (options.variant != CaseRelationsVariant::kPandas)
    throw std::invalid_argument("case relations result has an unknown variant");

  std::optional<EventLog> projected_log;
  std::string projection_error;
  try {
    projected_log = case_relations_compatibility_log(result, options);
  } catch (const std::exception& error) {
    projection_error = error.what();
  }

  std::ostringstream out;
  out << "{\"family\":\"MERGE_CASE_RELATIONS\",\"variant\":\"pandas\"";
  if (projected_log) {
    out << ",\"log\":";
    write_projected_log(out, *projected_log);
  } else {
    out << ",\"log\":null,\"log_projection_error\":";
    write_json_string(out, projection_error);
  }
  out << ",\"table\":";
  write_case_relations_table(out, result);
  out << '}';
  return out.str();
}

namespace {
void write_random_tree_node_json(std::ostream& out,
                                 const ProcessTreeNode& node,
                                 const ProcessTree& tree) {
  out << "{\"operator\":";
  write_json_string(out, operator_name(node.op));
  if (node.op == ProcessTreeOperator::kActivity) {
    out << ",\"activity\":";
    write_json_string(out, tree.activities().at(node.activity));
  }
  out << ",\"children\":[";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    if (i) out << ',';
    write_random_tree_node_json(out, node.children[i], tree);
  }
  out << "]}";
}
}  // namespace

std::string random_tree_json(const ProcessTree& result) {
  return random_tree_json(result, RandomTreeVariant::kBasic);
}

std::string random_tree_json(const ProcessTree& result,
                             RandomTreeVariant variant) {
  return random_tree_json(std::vector<ProcessTree>{result}, variant);
}

std::string random_tree_json(const std::vector<ProcessTree>& results,
                             RandomTreeVariant variant,
                             const std::vector<std::string>& activities) {
  std::ostringstream out;
  const auto variant_name = variant == RandomTreeVariant::kBasic
                                ? "BASIC"
                                : "PTANDLOGGENERATOR";
  const auto* shared_activities = &activities;
  if (shared_activities->empty() && !results.empty())
    shared_activities = &results.front().activities();
  for (const auto& result : results) {
    if (result.activities() != *shared_activities)
      throw std::invalid_argument(
          "random tree: all generated models must share activities");
  }
  out << "{\"family\":\"RANDOM_TREE\",\"variant\":\""
      << variant_name << "\",\"activities\":[";
  for (std::size_t i = 0; i < shared_activities->size(); ++i) {
    if (i) out << ',';
    write_json_string(out, (*shared_activities)[i]);
  }
  if (results.size() == 1) {
    out << "],\"tree\":";
    write_random_tree_node_json(out, results.front().root(), results.front());
  } else {
    out << "],\"trees\":[";
    for (std::size_t i = 0; i < results.size(); ++i) {
      if (i) out << ',';
      write_random_tree_node_json(out, results[i].root(), results[i]);
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string ilp_miner_json(const ILPMinerResult& result) {
  const auto& net = result.net;
  if (result.markings.initial.size() != net.place_count() ||
      result.markings.final.size() != net.place_count() ||
      result.diagnostics.lp_solved > result.diagnostics.visited_nodes ||
      result.diagnostics.branch_and_bound_nodes > result.diagnostics.visited_nodes)
    throw std::invalid_argument("ILP miner result is inconsistent");
  for (const auto& transition : net.transitions())
    for (const auto place : transition.inputs)
      if (place >= net.place_count())
        throw std::invalid_argument("ILP miner transition input is invalid");
  for (const auto& transition : net.transitions())
    for (const auto place : transition.outputs)
      if (place >= net.place_count())
        throw std::invalid_argument("ILP miner transition output is invalid");
  std::ostringstream out;
  out << "{\"family\":\"ILP\",\"net\":{\"place_count\":"
      << net.place_count() << ",\"source\":" << net.source()
      << ",\"sink\":" << net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i) out << ',';
    write_json_string(out, net.activities()[i]);
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"activity\":";
    if (transition.silent()) out << "null";
    else out << *transition.activity;
    out << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.inputs[j];
    }
    out << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j) out << ',';
      out << transition.outputs[j];
    }
    out << "],\"reset_inputs\":[";
    for (std::size_t j = 0; j < transition.reset_inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.reset_inputs[j];
    }
    out << "],\"inhibitor_inputs\":[";
    for (std::size_t j = 0; j < transition.inhibitor_inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.inhibitor_inputs[j];
    }
    out << "]}";
  }
  out << "]},\"markings\":{\"initial\":[";
  for (std::size_t i = 0; i < result.markings.initial.size(); ++i) {
    if (i) out << ',';
    out << result.markings.initial[i];
  }
  out << "],\"final\":[";
  for (std::size_t i = 0; i < result.markings.final.size(); ++i) {
    if (i) out << ',';
    out << result.markings.final[i];
  }
  const auto& d = result.diagnostics;
  out << "]},\"diagnostics\":{\"computed\":"
      << (d.computed ? "true" : "false") << ",\"exhausted\":"
      << (d.exhausted ? "true" : "false") << ",\"visited_nodes\":"
      << d.visited_nodes << ",\"lp_solved\":" << d.lp_solved
      << ",\"branch_and_bound_nodes\":" << d.branch_and_bound_nodes
      << ",\"message\":";
  write_json_string(out, d.message);
  out << "}}";
  return out.str();
}

std::string genetic_miner_json(const GeneticMinerResult& result) {
  const auto& net = result.net;
  const auto activity_count = result.matrix.activities.size();
  if (result.matrix.input_map.size() != activity_count ||
      result.matrix.output_map.size() != activity_count ||
      result.markings.initial.size() != net.place_count() ||
      result.markings.final.size() != net.place_count() ||
      result.diagnostics.activity_count != activity_count ||
      !std::isfinite(result.diagnostics.best_fitness) ||
      result.diagnostics.best_fitness < 0.0 ||
      result.diagnostics.best_fitness > 1.0)
    throw std::invalid_argument("genetic miner result is inconsistent");
  std::set<std::string> activities;
  for (const auto& activity : result.matrix.activities)
    if (activity.empty() || !activities.insert(activity).second)
      throw std::invalid_argument("genetic miner activities are invalid");
  const auto validate_map = [&](const GeneticMatrixMap& map) {
    for (const auto& groups : map)
      for (const auto& group : groups)
        for (const auto member : group)
          if (member >= activity_count)
            throw std::invalid_argument("genetic miner matrix member is invalid");
  };
  validate_map(result.matrix.input_map);
  validate_map(result.matrix.output_map);
  std::ostringstream out;
  const auto write_matrix_map = [&](const GeneticMatrixMap& map) {
    out << '[';
    for (std::size_t activity = 0; activity < map.size(); ++activity) {
      if (activity) out << ',';
      out << '[';
      for (std::size_t group = 0; group < map[activity].size(); ++group) {
        if (group) out << ',';
        out << '[';
        for (std::size_t member = 0; member < map[activity][group].size();
             ++member) {
          if (member) out << ',';
          out << map[activity][group][member];
        }
        out << ']';
      }
      out << ']';
    }
    out << ']';
  };
  out << "{\"family\":\"GENETIC\",\"matrix\":{\"activities\":[";
  for (std::size_t i = 0; i < result.matrix.activities.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, result.matrix.activities[i]);
  }
  out << "],\"input_map\":";
  write_matrix_map(result.matrix.input_map);
  out << ",\"output_map\":";
  write_matrix_map(result.matrix.output_map);
  out << "},\"net\":{\"place_count\":" << net.place_count()
      << ",\"source\":" << net.source()
      << ",\"sink\":" << net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i) out << ',';
    write_json_string(out, net.activities()[i]);
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"activity\":";
    if (transition.silent()) out << "null";
    else out << *transition.activity;
    out << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.inputs[j];
    }
    out << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j) out << ',';
      out << transition.outputs[j];
    }
    out << "],\"reset_inputs\":[";
    for (std::size_t j = 0; j < transition.reset_inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.reset_inputs[j];
    }
    out << "],\"inhibitor_inputs\":[";
    for (std::size_t j = 0; j < transition.inhibitor_inputs.size(); ++j) {
      if (j) out << ',';
      out << transition.inhibitor_inputs[j];
    }
    out << "]}";
  }
  out << "]},\"markings\":{\"initial\":[";
  for (std::size_t i = 0; i < result.markings.initial.size(); ++i) {
    if (i) out << ',';
    out << result.markings.initial[i];
  }
  out << "],\"final\":[";
  for (std::size_t i = 0; i < result.markings.final.size(); ++i) {
    if (i) out << ',';
    out << result.markings.final[i];
  }
  const auto& d = result.diagnostics;
  out << "]},\"diagnostics\":{\"computed\":"
      << (d.computed ? "true" : "false") << ",\"exhausted\":"
      << (d.exhausted ? "true" : "false")
      << ",\"generations_completed\":" << d.generations_completed
      << ",\"candidates_evaluated\":" << d.candidates_evaluated
      << ",\"population_size\":" << d.population_size
      << ",\"activity_count\":" << d.activity_count
      << ",\"best_fitness\":" << std::setprecision(17)
      << d.best_fitness << ",\"seed\":" << d.seed << ",\"message\":";
  write_json_string(out, d.message);
  out << "}}";
  return out.str();
}

std::string trace_variant_query_json(const TraceVariantQueryResult& result) {
  const auto variant_name = [&]() -> std::string_view {
    switch (result.diagnostics.variant) {
      case TraceVariantQueryVariant::kLaplace: return "laplace";
      case TraceVariantQueryVariant::kSacofa: return "sacofa";
    }
    throw std::invalid_argument("trace variant query result has invalid variant");
  }();
  if (!std::isfinite(result.diagnostics.epsilon) ||
      result.diagnostics.epsilon <= 0.0 ||
      result.diagnostics.retained_variant_count != result.frequencies.size() ||
      result.diagnostics.released_trace_count != result.log.trace_count())
    throw std::invalid_argument("trace variant query result is inconsistent");
  std::uint64_t released = 0;
  for (const auto& [prefix, count] : result.frequencies) {
    if (count > std::numeric_limits<std::uint64_t>::max() - released)
      throw std::invalid_argument("trace variant query frequency total overflows");
    released += count;
    for (const auto& activity : prefix)
      if (activity.empty())
        throw std::invalid_argument("trace variant query contains an empty activity");
  }
  if (!result.diagnostics.output_truncated &&
      released != result.diagnostics.released_trace_count)
    throw std::invalid_argument("trace variant query frequencies do not match the log");
  std::ostringstream out;
  out << "{\"family\":\"TRACE_VARIANT_QUERY\",\"variant\":";
  write_json_string(out, variant_name);
  out << ",\"epsilon\":" << result.diagnostics.epsilon
      << ",\"k\":" << result.diagnostics.max_prefix_length
      << ",\"p\":" << result.diagnostics.prune_threshold
      << ",\"seed\":" << result.diagnostics.seed
      << ",\"secure_random\":"
      << (result.diagnostics.secure_random ? "true" : "false")
      << ",\"frequencies\":[";
  std::size_t index = 0;
  for (const auto& [prefix, count] : result.frequencies) {
    if (index++) out << ',';
    out << "{\"activities\":[";
    for (std::size_t i = 0; i < prefix.size(); ++i) {
      if (i) out << ',';
      write_json_string(out, prefix[i]);
    }
    out << "],\"count\":" << count << '}';
  }
  const auto& d = result.diagnostics;
  out << "],\"diagnostics\":{\"candidate_count\":"
      << d.candidate_count << ",\"selected_violation_count\":"
      << d.selected_violation_count << ",\"retained_variant_count\":"
      << d.retained_variant_count << ",\"released_trace_count\":"
      << d.released_trace_count << ",\"output_truncated\":"
      << (d.output_truncated ? "true" : "false")
      << ",\"secure_random\":"
      << (d.secure_random ? "true" : "false") << ",\"message\":";
  write_json_string(out, d.message);
  out << "},\"log\":";
  write_projected_log(out, result.log);
  out << '}';
  return out.str();
}

std::string pripel_json(const PripelResult& result) {
  const auto& diagnostics = result.diagnostics;
  if (!std::isfinite(diagnostics.epsilon) || diagnostics.epsilon <= 0.0 ||
      diagnostics.released_trace_count != result.log.trace_count() ||
      diagnostics.exact_matches + diagnostics.nearest_matches +
              diagnostics.synthesized_traces !=
          diagnostics.released_trace_count ||
      diagnostics.synthesized_events > result.log.event_count())
    throw std::invalid_argument("pripel result is inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"PRIPEL\",\"diagnostics\":"
      << result.diagnostics.to_json() << ",\"log\":";
  write_projected_log(out, result.log);
  out << '}';
  return out.str();
}

std::string chaotic_activities_json(const ChaoticActivitiesResult& result) {
  if (!std::isfinite(result.diagnostics.alpha) ||
      result.diagnostics.alpha < 0.0)
    throw std::invalid_argument("chaotic activity alpha must be finite and non-negative");
  for (const auto& row : result.activities) {
    if (!std::isfinite(row.entropy) || !std::isfinite(row.entropy_smooth) ||
        !std::isfinite(row.entropy_gain) ||
        !std::isfinite(row.chaotic_score))
      throw std::invalid_argument("chaotic activity metrics must be finite");
  }
  std::ostringstream out;
  out << "{\"family\":\"CHAOTIC_ACTIVITIES\",\"variant\":\"NIEK_SIDOROVA\",\"alpha\":"
      << std::setprecision(17) << result.diagnostics.alpha
      << ",\"activities\":[";
  for (std::size_t i = 0; i < result.activities.size(); ++i) {
    if (i) out << ',';
    const auto& row = result.activities[i];
    out << "{\"activity\":";
    write_json_string(out, row.activity);
    out << ",\"frequency\":" << row.frequency
        << ",\"entropy\":" << std::setprecision(17) << row.entropy
        << ",\"entropy_smooth\":" << row.entropy_smooth
        << ",\"entropy_gain\":" << row.entropy_gain
        << ",\"chaotic_score\":" << row.chaotic_score << '}';
  }
  const auto& diagnostics = result.diagnostics;
  out << "],\"diagnostics\":{\"activity_count\":"
      << diagnostics.activity_count << ",\"event_count\":"
      << diagnostics.event_count << ",\"output_truncated\":"
      << (diagnostics.output_truncated ? "true" : "false")
      << ",\"message\":";
  write_json_string(out, diagnostics.message);
  out << "}}";
  return out.str();
}

std::string passed_time_json(const PassedTimeResult& result) {
  const auto write_entries = [](std::ostringstream& out,
                                const std::vector<PassedTimeEntry>& entries) {
    out << '[';
    for (std::size_t i = 0; i < entries.size(); ++i) {
      if (i) out << ',';
      const auto& entry = entries[i];
      out << "{\"activity\":";
      write_json_string(out, entry.activity);
      out << ",\"performance\":" << std::setprecision(17)
          << entry.performance << ",\"frequency\":" << entry.frequency
          << '}';
    }
    out << ']';
  };
  if (!std::isfinite(result.pre_avg_perf) ||
      !std::isfinite(result.post_avg_perf)) {
    throw std::invalid_argument("passed time averages must be finite");
  }
  for (const auto& entry : result.pre) {
    if (!std::isfinite(entry.performance))
      throw std::invalid_argument("passed time performance must be finite");
  }
  for (const auto& entry : result.post) {
    if (!std::isfinite(entry.performance))
      throw std::invalid_argument("passed time performance must be finite");
  }
  std::ostringstream out;
  out << "{\"family\":\"PASSED_TIME\",\"variant\":\""
      << passed_time_variant_name(result.variant) << "\",\"activity\":";
  write_json_string(out, result.activity);
  out << ",\"pre\":";
  write_entries(out, result.pre);
  out << ",\"post\":";
  write_entries(out, result.post);
  out << ",\"pre_avg_perf\":" << std::setprecision(17)
      << result.pre_avg_perf << ",\"post_avg_perf\":"
      << result.post_avg_perf << '}';
  return out.str();
}

std::string service_time_json(const ServiceTimeResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"SERVICE_TIME\",\"variant\":\"CLASSIC\",\"aggregation\":\""
      << service_time_aggregation_name(result.aggregation)
      << "\",\"values\":{";
  std::size_t index = 0;
  for (const auto& [activity, value] : result.values) {
    if (!std::isfinite(value))
      throw std::invalid_argument("service time values must be finite");
    if (index++) out << ',';
    write_json_string(out, activity);
    out << ':' << std::setprecision(17) << value;
  }
  out << "}}";
  return out.str();
}

std::string rework_json(const ReworkResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"REWORK\",\"variant\":\""
      << rework_variant_name(result.variant) << '\"';
  if (result.variant == ReworkVariant::kActivities) {
    out << ",\"activity_cases\":{";
    std::size_t index = 0;
    for (const auto& [activity, count] : result.activity_cases) {
      if (index++) out << ',';
      write_json_string(out, activity);
      out << ':' << count;
    }
    out << '}';
  } else if (result.variant == ReworkVariant::kCases) {
    out << ",\"cases\":{";
    std::size_t index = 0;
    for (const auto& [case_id, stats] : result.cases) {
      if (index++) out << ',';
      write_json_string(out, case_id);
      out << ":{\"number_activities\":" << stats.number_activities
          << ",\"rework\":" << stats.rework << '}';
    }
    out << '}';
  } else {
    throw std::invalid_argument("rework: unknown variant");
  }
  out << '}';
  return out.str();
}

std::string cycle_time_json(const CycleTimeResult& result) {
  if (!std::isfinite(result.cycle_time) || result.cycle_time < 0.0)
    throw std::invalid_argument("cycle time must be finite and non-negative");
  std::ostringstream out;
  out << "{\"family\":\"CYCLE_TIME\",\"variant\":\"CLASSIC\","
      << "\"cycle_time\":" << result.cycle_time
      << ",\"instances\":" << result.instances
      << ",\"timed_events\":" << result.timed_events << '}';
  return out.str();
}

std::string case_arrival_json(const CaseArrivalResult& result) {
  if (!std::isfinite(result.arrival_average) ||
      !std::isfinite(result.dispersion_average) ||
      result.arrival_average < 0.0 || result.dispersion_average < 0.0) {
    throw std::invalid_argument(
        "case arrival averages must be finite and non-negative");
  }
  std::ostringstream out;
  out << "{\"family\":\"CASE_ARRIVAL\",\"variant\":\"CLASSIC\","
      << "\"arrival_average\":" << result.arrival_average
      << ",\"dispersion_average\":" << result.dispersion_average
      << ",\"timed_starts\":" << result.timed_starts
      << ",\"timed_ends\":" << result.timed_ends << '}';
  return out.str();
}

std::string case_statistics_json(const CaseStatisticsResult& result) {
  (void)case_statistics_sort_name(result.options.sort_by);
  for (const auto& entry : result.cases) {
    if (!std::isfinite(entry.start_time) || !std::isfinite(entry.end_time) ||
        !std::isfinite(entry.case_duration) || entry.case_duration < 0.0) {
      throw std::invalid_argument(
          "case statistics values must be finite and non-negative");
    }
  }
  std::ostringstream out;
  out << "{\"family\":\"CASE_STATISTICS\",\"variant\":\"CLASSIC\""
      << ",\"case_id_key\":";
  write_json_string(out, result.options.case_id_key);
  out << ",\"timestamp_key\":";
  write_json_string(out, result.options.timestamp_key);
  out << ",\"enable_sort\":"
      << (result.options.enable_sort ? "true" : "false")
      << ",\"sort_by\":";
  write_json_string(out, case_statistics_sort_name(result.options.sort_by));
  out << ",\"sort_ascending\":"
      << (result.options.sort_ascending ? "true" : "false")
      << ",\"max_cases\":" << result.options.max_cases
      << ",\"use_business_hours\":"
      << (result.options.use_business_hours ? "true" : "false")
      << ",\"cases\":[";
  for (std::size_t i = 0; i < result.cases.size(); ++i) {
    if (i != 0) out << ',';
    const auto& entry = result.cases[i];
    out << "{\"case_id\":";
    write_json_string(out, entry.case_id);
    out << ",\"start_time\":" << std::setprecision(17)
        << entry.start_time << ",\"end_time\":" << entry.end_time
        << ",\"case_duration\":" << entry.case_duration << '}';
  }
  out << "]}";
  return out.str();
}

std::string attribute_statistics_json(
    const AttributeStatisticsResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"ATTRIBUTE_STATISTICS\",\"scope\":\""
      << attribute_statistics_scope_name(result.scope)
      << "\",\"attribute_key\":";
  write_json_string(out, result.attribute_key);
  out << ",\"keep_once_per_case\":"
      << (result.keep_once_per_case ? "true" : "false")
      << ",\"values\":{";
  std::size_t index = 0;
  for (const auto& [value, count] : result.values) {
    if (index++) out << ',';
    write_json_string(out, value);
    out << ':' << count;
  }
  out << "}}";
  return out.str();
}

std::string attribute_distribution_json(
    const AttributeDistributionResult& result) {
  if (result.labels.size() != result.counts.size())
    throw std::invalid_argument(
        "attribute distribution labels and counts must have equal length");
  (void)event_distribution_name(result.options.distribution);
  std::ostringstream out;
  out << "{\"family\":\"ATTRIBUTE_DISTRIBUTION\",\"variant\":\""
      << event_distribution_name(result.options.distribution)
      << "\",\"timestamp_key\":";
  write_json_string(out, result.options.timestamp_key);
  out << ",\"labels\":[";
  for (std::size_t i = 0; i < result.labels.size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, result.labels[i]);
  }
  out << "],\"counts\":[";
  for (std::size_t i = 0; i < result.counts.size(); ++i) {
    if (i != 0) out << ',';
    out << result.counts[i];
  }
  out << "],\"timed_events\":" << result.timed_events << '}';
  return out.str();
}

std::string attribute_kde_json(const AttributeKdeResult& result) {
  if (result.x.size() != result.y.size())
    throw std::invalid_argument("attribute KDE x and y must have equal length");
  const auto variant = attribute_kde_variant_name(result.variant);
  for (std::size_t i = 0; i < result.x.size(); ++i) {
    if (!std::isfinite(result.x[i]) || !std::isfinite(result.y[i]) ||
        result.y[i] < 0.0 ||
        (i != 0 && result.x[i] < result.x[i - 1])) {
      throw std::invalid_argument(
          "attribute KDE points must be finite, ordered, and non-negative");
    }
  }
  if (result.attribute_key.empty())
    throw std::invalid_argument("attribute KDE attribute key is empty");
  std::ostringstream out;
  out << "{\"family\":\"ATTRIBUTE_KDE\",\"variant\":\""
      << variant << "\",\"attribute_key\":";
  write_json_string(out, result.attribute_key);
  out << ",\"x\":[";
  for (std::size_t i = 0; i < result.x.size(); ++i) {
    if (i != 0) out << ',';
    out << std::setprecision(17) << result.x[i];
  }
  out << "],\"y\":[";
  for (std::size_t i = 0; i < result.y.size(); ++i) {
    if (i != 0) out << ',';
    out << std::setprecision(17) << result.y[i];
  }
  out << "]}";
  return out.str();
}

std::string boundary_activities_json(const BoundaryActivitiesResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"BOUNDARY_ACTIVITIES\",\"variant\":\""
      << boundary_activities_variant_name(result.variant)
      << "\",\"starts\":{";
  std::size_t index = 0;
  for (const auto& [activity, count] : result.starts) {
    if (index++) out << ',';
    write_json_string(out, activity);
    out << ':' << count;
  }
  out << "},\"ends\":{";
  index = 0;
  for (const auto& [activity, count] : result.ends) {
    if (index++) out << ',';
    write_json_string(out, activity);
    out << ':' << count;
  }
  out << "}}";
  return out.str();
}

std::string variant_statistics_json(const VariantStatisticsResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"VARIANT_STATISTICS\",\"variant\":\""
      << variant_statistics_variant_name() << "\",\"activity_key\":";
  write_json_string(out, result.activity_key);
  out << ",\"variants\":[";
  for (std::size_t i = 0; i < result.variants.size(); ++i) {
    if (i) out << ',';
    const auto& entry = result.variants[i];
    out << "{\"activities\":[";
    for (std::size_t j = 0; j < entry.activities.size(); ++j) {
      if (j) out << ',';
      write_json_string(out, entry.activities[j]);
    }
    out << "],\"count\":" << entry.count
        << ",\"probability\":" << std::setprecision(17)
        << entry.probability << '}';
  }
  out << "]}";
  return out.str();
}

std::string organizational_sna_json(const OrganizationalSnaResult& result) {
  const char* variant = nullptr;
  switch (result.variant) {
    case OrganizationalSnaVariant::kWorkingTogether: variant = "working_together"; break;
    case OrganizationalSnaVariant::kSubcontracting: variant = "subcontracting"; break;
    case OrganizationalSnaVariant::kJointActivities: variant = "jointactivities"; break;
    case OrganizationalSnaVariant::kHandover: variant = "handover"; break;
    case OrganizationalSnaVariant::kHandoverPandas: variant = "handover_pandas"; break;
    case OrganizationalSnaVariant::kWorkingTogetherPandas: variant = "working_together_pandas"; break;
    case OrganizationalSnaVariant::kSubcontractingPandas: variant = "subcontracting_pandas"; break;
    case OrganizationalSnaVariant::kJointActivitiesPandas: variant = "jointactivities_pandas"; break;
  }
  if (variant == nullptr)
    throw std::invalid_argument("organizational SNA result has an unknown variant");
  const bool directed = result.variant == OrganizationalSnaVariant::kHandover ||
      result.variant == OrganizationalSnaVariant::kHandoverPandas ||
      result.variant == OrganizationalSnaVariant::kSubcontracting ||
      result.variant == OrganizationalSnaVariant::kSubcontractingPandas;
  if (result.directed != directed ||
      result.diagnostics.resource_count != result.resources.size() ||
      result.diagnostics.edge_count != result.edges.size())
    throw std::invalid_argument("organizational SNA result is inconsistent");
  if (std::set<std::string>(result.resources.begin(), result.resources.end()).size() !=
      result.resources.size())
    throw std::invalid_argument("organizational SNA resources must be unique");
  for (const auto& edge : result.edges)
    if (edge.source >= result.resources.size() || edge.target >= result.resources.size() ||
        !std::isfinite(edge.weight))
      throw std::invalid_argument("organizational SNA edge is invalid");
  std::ostringstream out;
  out << "{\"family\":\"ORGANIZATIONAL_SNA\",\"variant\":";
  write_json_string(out, variant);
  out << ",\"directed\":" << (result.directed ? "true" : "false")
      << ",\"resources\":[";
  for (std::size_t i = 0; i < result.resources.size(); ++i) {
    if (i) out << ',';
    write_json_string(out, result.resources[i]);
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < result.edges.size(); ++i) {
    if (i) out << ',';
    const auto& edge = result.edges[i];
    out << "{\"source\":" << edge.source << ",\"target\":"
        << edge.target << ",\"weight\":" << std::setprecision(17)
        << edge.weight << '}';
  }
  const auto& d = result.diagnostics;
  out << "],\"diagnostics\":{\"computed\":"
      << (d.computed ? "true" : "false") << ",\"output_truncated\":"
      << (d.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << d.trace_count
      << ",\"event_count\":" << d.event_count
      << ",\"resource_count\":" << d.resource_count
      << ",\"edge_count\":" << d.edge_count << ",\"message\":";
  write_json_string(out, d.message);
  out << "}}";
  return out.str();
}

std::string network_analysis_json(const NetworkAnalysisResult& result) {
  if (result.variant != NetworkAnalysisVariant::kDataframe)
    throw std::invalid_argument("network analysis result has an unknown variant");
  std::size_t performance_values = 0;
  std::uint64_t links = 0;
  for (const auto& edge : result.edges) {
    if (edge.source.empty() || edge.target.empty() || edge.activity.empty() || edge.count == 0 ||
        std::any_of(edge.performance.begin(), edge.performance.end(),
                    [](double value) { return !std::isfinite(value); }))
      throw std::invalid_argument("network analysis edge is invalid");
    links += edge.count;
    performance_values += edge.performance.size();
  }
  if (result.diagnostics.row_count != result.edges.size() ||
      result.diagnostics.link_count != links ||
      result.diagnostics.performance_value_count != performance_values)
    throw std::invalid_argument("network analysis diagnostics are inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"NETWORK_ANALYSIS\",\"variant\":\"dataframe\",\"edges\":[";
  for (std::size_t i = 0; i < result.edges.size(); ++i) {
    if (i) out << ',';
    const auto& e = result.edges[i];
    out << "{\"source\":"; write_json_string(out, e.source);
    out << ",\"target\":"; write_json_string(out, e.target);
    out << ",\"activity\":"; write_json_string(out, e.activity);
    out << ",\"count\":" << e.count << ",\"performance\":[";
    for (std::size_t j = 0; j < e.performance.size(); ++j) {
      if (j) out << ',';
      out << std::setprecision(17) << e.performance[j];
    }
    out << "]}";
  }
  const auto& d = result.diagnostics;
  out << "],\"diagnostics\":{\"computed\":" << (d.computed ? "true" : "false")
      << ",\"output_truncated\":" << (d.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << d.trace_count << ",\"event_count\":" << d.event_count
      << ",\"link_count\":" << d.link_count << ",\"row_count\":" << d.row_count
      << ",\"performance_value_count\":" << d.performance_value_count << ",\"message\":";
  write_json_string(out, d.message);
  out << "}}";
  return out.str();
}

std::string role_discovery_json(const RoleDiscoveryResult& result) {
  const char* variant = nullptr;
  switch (result.variant) {
    case RoleDiscoveryVariant::kLog: variant = "log"; break;
    case RoleDiscoveryVariant::kPandas: variant = "pandas"; break;
  }
  if (variant == nullptr)
    throw std::invalid_argument("role discovery result has an unknown variant");
  std::set<std::string> activities;
  std::set<std::string> resources;
  for (const auto& role : result.roles) {
    if (role.activities.empty() || role.resources.empty())
      throw std::invalid_argument("role discovery contains an empty role");
    for (const auto& activity : role.activities)
      if (activity.empty() || !activities.insert(activity).second)
        throw std::invalid_argument("role discovery activities are invalid");
    for (const auto& resource : role.resources) {
      if (resource.name.empty() || resource.count == 0)
        throw std::invalid_argument("role discovery resource is invalid");
      resources.insert(resource.name);
    }
  }
  if (result.diagnostics.role_count != result.roles.size() ||
      result.diagnostics.activity_count != activities.size() ||
      result.diagnostics.resource_count != resources.size())
    throw std::invalid_argument("role discovery diagnostics are inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"ROLE_DISCOVERY\",\"variant\":\""
      << variant << "\",\"roles\":[";
  for (std::size_t i = 0; i < result.roles.size(); ++i) {
    if (i) out << ',';
    const auto& role = result.roles[i];
    out << "{\"activities\":[";
    for (std::size_t j = 0; j < role.activities.size(); ++j) {
      if (j) out << ','; write_json_string(out, role.activities[j]);
    }
    out << "],\"resources\":[";
    for (std::size_t j = 0; j < role.resources.size(); ++j) {
      if (j) out << ',';
      out << "{\"name\":";
      write_json_string(out, role.resources[j].name);
      out << ",\"count\":" << role.resources[j].count << '}';
    }
    out << "]}";
  }
  const auto& d = result.diagnostics;
  out << "],\"diagnostics\":{\"computed\":" << (d.computed ? "true" : "false")
      << ",\"output_truncated\":" << (d.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << d.trace_count << ",\"event_count\":" << d.event_count
      << ",\"activity_count\":" << d.activity_count << ",\"resource_count\":" << d.resource_count
      << ",\"role_count\":" << d.role_count << ",\"message\":";
  write_json_string(out, d.message); out << "}}";
  return out.str();
}

std::string temporal_conformance_json(
    const TemporalConformanceResult& result) {
  std::size_t deviation_count = 0;
  for (const auto& trace : result) {
    for (const auto& deviation : trace) {
      if (!std::isfinite(deviation.elapsed_seconds) ||
          !std::isfinite(deviation.standard_deviation_distance) ||
          deviation.elapsed_seconds < 0.0 ||
          deviation.standard_deviation_distance < 0.0) {
        throw std::invalid_argument(
            "temporal conformance contains an invalid deviation");
      }
      ++deviation_count;
    }
  }
  std::ostringstream out;
  out << "{\"family\":\"TEMPORAL_PROFILE_CONFORMANCE\",\"trace_count\":"
      << result.size() << ",\"deviation_count\":" << deviation_count
      << ",\"traces\":[";
  for (std::size_t trace_index = 0; trace_index < result.size();
       ++trace_index) {
    if (trace_index) out << ',';
    out << '[';
    const auto& trace = result[trace_index];
    for (std::size_t index = 0; index < trace.size(); ++index) {
      if (index) out << ',';
      const auto& deviation = trace[index];
      out << "{\"case_id\":";
      write_json_string(out, deviation.case_id);
      out << ",\"source\":";
      write_json_string(out, deviation.source);
      out << ",\"target\":";
      write_json_string(out, deviation.target);
      out << ",\"elapsed_seconds\":" << std::setprecision(17)
          << deviation.elapsed_seconds
          << ",\"standard_deviation_distance\":" << std::setprecision(17)
          << deviation.standard_deviation_distance << '}';
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
