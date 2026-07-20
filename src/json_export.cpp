#include "minera/json_export.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <sstream>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/version.hpp"

namespace minera {
namespace {

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
    case ProcessTreeOperator::kParallel: return "parallel";
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
  if (analysis.entire_event_log) {
    validate_footprint(*analysis.entire_event_log, graph);
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
  if (analysis.trace_by_trace) {
    if (emitted) out << ", ";
    emitted = true;
    out << "\"trace_by_trace\": [";
    for (std::size_t i = 0; i < analysis.trace_by_trace->size(); ++i) {
      out << "{\"trace_index\": " << i << ", \"case_id\": ";
      write_json_string(out, log.traces[i].case_id);
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
      write_json_string(out, log.traces[i].case_id);
      out << ", ";
      write_footprints_extensive_fields(
          out, (*analysis.trace_extensive)[i], graph);
      out << '}' << (i + 1 < analysis.trace_extensive->size() ? ", " : "");
    }
    out << ']';
  }
  out << "}, \"metrics\": {\"precision\": " << analysis.precision
      << ", \"log_fitness\": " << analysis.log_fitness
      << ", \"perc_fit_traces\": " << analysis.perc_fit_traces << "}}";
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
    out << '}' << (i + 1 < transitions.size() ? ", " : "");
  }
  out << "]}";
}

void write_heuristics_net(std::ostream& out,
                          const HeuristicsNet& net,
                          const DirectlyFollowsGraph& graph) {
  // Resolve the net's internal activity ids to the ids used everywhere else in
  // the document by going through the activity name.
  const auto resolve = [&](std::size_t local) {
    const auto id = graph.activity_id(net.activities[local]);
    return id ? *id : local;
  };
  const auto resolve_name = [&](const std::string& name) {
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
  out << "\", \"edges\": [";
  for (std::size_t i = 0; i < net.edges.size(); ++i) {
    const auto& edge = net.edges[i];
    out << "{\"source\": " << resolve(edge.source)
        << ", \"target\": " << resolve(edge.target)
        << ", \"dependency\": " << edge.dependency
        << ", \"frequency\": " << edge.frequency
        << ", \"representation\": " << edge.representation
        << ", \"length_two_loop\": "
        << (edge.length_two_loop ? "true" : "false") << '}'
        << (i + 1 < net.edges.size() ? ", " : "");
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
        << ", \"occurrences\": " << stat.occurrences
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
        << ", \"representation\": " << stat.representation
        << ", \"length_two_loop\": "
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
  out << "], \"output_bundles\": ";
  write_bundles(net.output_bundles);
  out << ", \"input_bundles\": ";
  write_bundles(net.input_bundles);
  out << '}';
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
      << ", \"remaining\": " << conformance.remaining << ", \"variants\": [";
  const auto& variants = conformance.variants;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    out << "{\"activities\": [";
    for (std::size_t j = 0; j < variants[i].activities.size(); ++j) {
      // Resolve names to the activity ids used elsewhere in the document.
      const auto id = graph.activity_id(variants[i].activities[j]);
      out << (id ? *id : 0) << (j + 1 < variants[i].activities.size() ? ", " : "");
    }
    out << "], \"count\": " << variants[i].count
        << ", \"fitness\": " << variants[i].fitness << '}'
        << (i + 1 < variants.size() ? ", " : "");
  }
  out << "]}";
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
  out << "{\"computed\": " << (alignments.computed ? "true" : "false")
      << ", \"exhausted\": " << (alignments.exhausted ? "true" : "false")
      << ", \"fitness\": " << alignments.fitness
      << ", \"precision\": " << alignments.precision
      << ", \"fitting_traces\": " << alignments.fitting_traces
      << ", \"trace_count\": " << alignments.trace_count
      << ", \"total_cost\": " << alignments.total_cost << ", \"variants\": [";
  const auto& variants = alignments.variants;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    const auto& variant = variants[i];
    out << "{\"activities\": [";
    for (std::size_t j = 0; j < variant.activities.size(); ++j) {
      const auto id = graph.activity_id(variant.activities[j]);
      out << (id ? *id : 0) << (j + 1 < variant.activities.size() ? ", " : "");
    }
    out << "], \"count\": " << variant.count << ", \"cost\": " << variant.cost
        << ", \"fitness\": " << variant.fitness << ", \"moves\": [";
    for (std::size_t j = 0; j < variant.moves.size(); ++j) {
      const auto& move = variant.moves[j];
      const auto id = graph.activity_id(move.activity);
      out << "{\"type\": \"" << move_type_name(move.type)
          << "\", \"activity\": " << (id ? *id : 0) << '}'
          << (j + 1 < variant.moves.size() ? ", " : "");
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
    out << "{\"activity\": " << (id ? *id : 0)
        << ", \"synchronous\": " << entry.synchronous
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
    write_json_string(out, attributes[i].key);
    out << ": ";
    write_json_string(out, attributes[i].value);
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
  out << "\"observed_traces\": " << decisions.observed_traces
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
  out << "{\"activities\": [";
  for (std::size_t i = 0; i < spectrum.activities.size(); ++i) {
    write_json_string(out, spectrum.activities[i]);
    out << (i + 1 < spectrum.activities.size() ? ", " : "");
  }
  out << "], \"observations\": [";
  for (std::size_t i = 0; i < spectrum.observations.size(); ++i) {
    const auto& observation = spectrum.observations[i];
    out << "{\"case_id\": ";
    write_json_string(out, observation.case_id);
    out << ", \"instants\": [";
    for (std::size_t k = 0; k < observation.instants.size(); ++k) {
      out << observation.instants[k]
          << (k + 1 < observation.instants.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < spectrum.observations.size() ? ", " : "");
  }
  out << "]}";
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

void validate_dfg_performance_statistics(
    const DfgPerformanceStatistics& statistics) {
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
  const auto validate_pair =
      [&](const DfgActivityPair& pair) {
        (void)dfg_variant_activity_id(graph, pair.first);
        (void)dfg_variant_activity_id(graph, pair.second);
      };
  for (const auto& named : variants.statistical) {
    if (named.name.empty()) {
      throw std::invalid_argument("DFG statistical variant name is empty");
    }
    const auto& analysis = named.analysis;
    for (const auto& [pair, count] : analysis.frequency_dfg) {
      (void)count;
      validate_pair(pair);
    }
    for (const auto& [triple, count] : analysis.frequency_triples) {
      (void)count;
      (void)dfg_variant_activity_id(graph, std::get<0>(triple));
      (void)dfg_variant_activity_id(graph, std::get<1>(triple));
      (void)dfg_variant_activity_id(graph, std::get<2>(triple));
    }
    for (const auto& [pair, statistics] : analysis.performance_dfg) {
      validate_pair(pair);
      validate_dfg_performance_statistics(statistics);
    }
    for (const auto& [pair, attributes] : analysis.case_attributes.edges) {
      (void)attributes;
      validate_pair(pair);
    }
    for (const auto& [activity, attributes] :
         analysis.case_attributes.nodes) {
      (void)attributes;
      (void)dfg_variant_activity_id(graph, activity);
    }
    for (const auto& [pair, count] : analysis.clean.graph) {
      (void)count;
      validate_pair(pair);
    }
    for (const auto& [activity, count] :
         analysis.clean.start_activities) {
      (void)count;
      (void)dfg_variant_activity_id(graph, activity);
    }
    for (const auto& [activity, count] : analysis.clean.end_activities) {
      (void)count;
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
    if (named.name.empty()) {
      throw std::invalid_argument("DFG causal variant name is empty");
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
          << ", \"value\": " << value << '}';
    }
    out << "]}";
    out << (i + 1 < variants.causal.size() ? ", " : "");
  }
  out << "]}";
}

}  // namespace

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
                         const DfgVariantsAnalysis* dfg_variants) {
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
  out << "{\n";
  out << "  \"dataset\": ";
  write_json_string(out, dataset);
  out << ",\n";
  out << "  \"generator\": \"minera " << version_major << '.' << version_minor
      << '.' << version_patch << "\",\n";
  out << "  \"trace_count\": " << log.trace_count() << ",\n";
  out << "  \"event_count\": " << log.event_count() << ",\n";

  out << "  \"traces\": [\n";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& trace = log.traces[i];
    out << "    {\"case_id\": ";
    write_json_string(out, trace.case_id);
    if (!trace.attributes.empty()) {
      out << ", \"attributes\": ";
      write_attribute_object(out, trace.attributes);
    }
    out << ", \"events\": [";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      const auto& event = trace.events[j];
      out << "{\"activity\": " << *graph.activity_id(event.activity)
          << ", \"timestamp\": ";
      write_json_string(out, event.timestamp);
      if (!event.resource.empty()) {
        out << ", \"resource\": ";
        write_json_string(out, event.resource);
      }
      if (!event.lifecycle.empty()) {
        out << ", \"lifecycle\": ";
        write_json_string(out, event.lifecycle);
      }
      if (!event.attributes.empty()) {
        out << ", \"attributes\": ";
        write_attribute_object(out, event.attributes);
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
    out << ",\n  \"minimum_self_distances\": ";
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
                          const DfgVariantsAnalysis* dfg_variants) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph, tree, net, heuristics,
                      conformance, performance, resources, variant_durations,
                      eventually_follows, decisions, petri_decoration,
                      spectrum, inductive_net, inductive_decoration,
                      heuristic_petri, heuristic_decoration, temporal_profile,
                      minimum_self_distances, transition_system, powl,
                      powl_petri, powl_decoration, log_skeleton,
                      log_skeleton_conformance, declare, declare_conformance,
                      footprints, dfg_variants);
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

}  // namespace minera
