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
  const auto write_ids = [&](const std::vector<std::size_t>& locals) {
    out << '[';
    for (std::size_t i = 0; i < locals.size(); ++i) {
      out << resolve(locals[i]) << (i + 1 < locals.size() ? ", " : "");
    }
    out << ']';
  };

  out << "{\"edges\": [";
  for (std::size_t i = 0; i < net.edges.size(); ++i) {
    const auto& edge = net.edges[i];
    out << "{\"source\": " << resolve(edge.source)
        << ", \"target\": " << resolve(edge.target)
        << ", \"dependency\": " << edge.dependency
        << ", \"frequency\": " << edge.frequency << '}'
        << (i + 1 < net.edges.size() ? ", " : "");
  }
  out << "], \"start_activities\": ";
  write_ids(net.start_activities);
  out << ", \"end_activities\": ";
  write_ids(net.end_activities);
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
                         const TransitionSystem* transition_system) {
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
                          const TransitionSystem* transition_system) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph, tree, net, heuristics,
                      conformance, performance, resources, variant_durations,
                      eventually_follows, decisions, petri_decoration,
                      spectrum, inductive_net, inductive_decoration,
                      heuristic_petri, heuristic_decoration, temporal_profile,
                      minimum_self_distances, transition_system);
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
