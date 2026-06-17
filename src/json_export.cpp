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

}  // namespace

void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& log,
                         const DirectlyFollowsGraph& graph,
                         const ProcessTree* tree) {
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
    out << '\n';
  } else {
    out << '\n';
  }
  out << "}\n";
}

std::string analysis_json(std::string_view dataset,
                          const EventLog& log,
                          const DirectlyFollowsGraph& graph,
                          const ProcessTree* tree) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph, tree);
  return out.str();
}

}  // namespace minera
