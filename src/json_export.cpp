#include "minera/json_export.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
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

}  // namespace

void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& log,
                         const DirectlyFollowsGraph& graph,
                         const ProcessTree* tree,
                         const ConformanceResult* conformance,
                         const PetriNet* net,
                         const HeuristicsNet* heuristics) {
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
    out << ", \"events\": [";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      const auto& event = trace.events[j];
      out << "{\"activity\": " << *graph.activity_id(event.activity)
          << ", \"timestamp\": ";
      write_json_string(out, event.timestamp);
      out << '}' << (j + 1 < trace.events.size() ? ", " : "");
    }
    out << "]}" << (i + 1 < log.traces.size() ? "," : "") << '\n';
  }
  out << "  ],\n";

  out << "  \"activities\": [\n";
  const auto& activities = graph.activities();
  for (std::size_t i = 0; i < activities.size(); ++i) {
    const auto& name = activities[i];
    out << "    {\"name\": ";
    write_json_string(out, name);
    out << ", \"count\": " << graph.activity_count(name)
        << ", \"start_count\": " << graph.start_activity_count(name)
        << ", \"end_count\": " << graph.end_activity_count(name) << '}'
        << (i + 1 < activities.size() ? "," : "") << '\n';
  }
  out << "  ],\n";

  out << "  \"edges\": [\n";
  const auto& edges = graph.edges();
  for (std::size_t i = 0; i < edges.size(); ++i) {
    out << "    {\"source\": " << edges[i].source
        << ", \"target\": " << edges[i].target
        << ", \"count\": " << edges[i].count << '}'
        << (i + 1 < edges.size() ? "," : "") << '\n';
  }
  out << "  ],\n";

  auto variants = graph.variants();
  std::stable_sort(variants.begin(), variants.end(),
                   [](const auto& a, const auto& b) { return a.count > b.count; });
  out << "  \"variants\": [\n";
  for (std::size_t i = 0; i < variants.size(); ++i) {
    out << "    {\"activities\": [";
    for (std::size_t j = 0; j < variants[i].activities.size(); ++j) {
      out << *graph.activity_id(variants[i].activities[j])
          << (j + 1 < variants[i].activities.size() ? ", " : "");
    }
    out << "], \"count\": " << variants[i].count << '}'
        << (i + 1 < variants.size() ? "," : "") << '\n';
  }
  out << "  ]";

  if (tree != nullptr) {
    out << ",\n  \"process_tree\": ";
    write_process_tree_node(out, tree->root(), *tree, graph);
  }
  if (conformance != nullptr) {
    out << ",\n  \"conformance\": ";
    write_conformance(out, *conformance, graph);
  }
  if (net != nullptr) {
    out << ",\n  \"petri_net\": ";
    write_petri_net(out, *net, graph);
  }
  if (heuristics != nullptr) {
    out << ",\n  \"heuristics_net\": ";
    write_heuristics_net(out, *heuristics, graph);
  }
  out << '\n';
  out << "}\n";
}

std::string analysis_json(std::string_view dataset,
                          const EventLog& log,
                          const DirectlyFollowsGraph& graph,
                          const ProcessTree* tree,
                          const ConformanceResult* conformance,
                          const PetriNet* net,
                          const HeuristicsNet* heuristics) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph, tree, conformance, net,
                      heuristics);
  return out.str();
}

}  // namespace minera
