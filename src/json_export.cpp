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

}  // namespace

void write_analysis_json(std::ostream& out,
                         std::string_view dataset,
                         const EventLog& log,
                         const DirectlyFollowsGraph& graph) {
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
  out << "  ]\n";
  out << "}\n";
}

std::string analysis_json(std::string_view dataset,
                          const EventLog& log,
                          const DirectlyFollowsGraph& graph) {
  std::ostringstream out;
  write_analysis_json(out, dataset, log, graph);
  return out.str();
}

}  // namespace minera
