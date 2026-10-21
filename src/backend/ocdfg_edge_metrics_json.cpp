#include "minera/ocdfg_edge_metrics_json.hpp"

#include <locale>
#include <sstream>

#include "minera/embedding_workbench_json.hpp"

namespace minera {

std::string serialize_ocdfg_edge_metrics_json(const OcdfgEdgeAssociations& associations) {
  const auto couples = aggregate_ocdfg_event_couples(associations);
  const auto total_objects = aggregate_ocdfg_total_objects(associations);
  const auto unique_objects = aggregate_ocdfg_unique_objects(associations);
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto text = [&](const std::string& value) {
    out << serialize_embedding_workbench_string_json(value);
  };
  const auto triples = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& [from, to, object] : values) {
      if (!first) out << ',';
      first = false;
      out << '['; text(from); out << ','; text(to); out << ','; text(object); out << ']';
    }
    out << ']';
  };
  out << "{\"object_types\":[";
  bool first_type = true;
  for (const auto& [type, edges] : associations) {
    if (!first_type) out << ',';
    first_type = false;
    out << "{\"object_type\":"; text(type); out << ",\"edges\":[";
    bool first_edge = true;
    for (const auto& [edge, ordered] : edges) {
      if (!first_edge) out << ',';
      first_edge = false;
      out << "{\"source_activity\":"; text(edge.first);
      out << ",\"target_activity\":"; text(edge.second);
      out << ",\"associations\":"; triples(ordered);
      out << ",\"event_couples\":[";
      bool first_pair = true;
      for (const auto& [from, to] : couples.at(type).at(edge)) {
        if (!first_pair) out << ',';
        first_pair = false;
        out << '['; text(from); out << ','; text(to); out << ']';
      }
      out << "],\"total_objects\":"; triples(total_objects.at(type).at(edge));
      out << ",\"unique_objects\":[";
      bool first_object = true;
      for (const auto& object : unique_objects.at(type).at(edge)) {
        if (!first_object) out << ',';
        first_object = false;
        text(object);
      }
      out << "]}";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
