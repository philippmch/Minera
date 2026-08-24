#include "minera/ocel_transform.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace minera {
namespace {

using ObjectIds = std::set<std::string>;

void write_json_string(std::ostringstream& out, const std::string& value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      default:
        if (c < 0x20) {
          const char* hex = "0123456789abcdef";
          out << "\\u00" << hex[(c >> 4) & 0xf] << hex[c & 0xf];
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  out << '"';
}

std::string variant_name(OcelSplitVariant variant) {
  switch (variant) {
    case OcelSplitVariant::kConnectedComponents:
      return "CONNECTED_COMPONENTS";
    case OcelSplitVariant::kAncestorsDescendants:
      return "ANCESTORS_DESCENDANTS";
  }
  throw std::invalid_argument("unknown OCEL split variant");
}

std::map<std::string, std::string> object_types(const Ocel& log) {
  std::map<std::string, std::string> result;
  for (const auto& object : log.objects) result.emplace(object.id, object.type);
  return result;
}

std::map<std::string, const OcelEvent*> events_by_id(const Ocel& log) {
  std::map<std::string, const OcelEvent*> result;
  for (const auto& event : log.events) result[event.id] = &event;
  return result;
}

std::map<std::string, std::vector<std::string>> objects_by_event(
    const Ocel& log, const std::map<std::string, std::string>& types) {
  std::map<std::string, std::vector<std::string>> result;
  for (const auto& relation : log.relations) {
    if (!types.contains(relation.object_id)) continue;
    auto& objects = result[relation.event_id];
    objects.push_back(relation.object_id);
  }
  for (auto& [event_id, objects] : result) {
    (void)event_id;
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  return result;
}

std::set<std::pair<std::string, std::string>> interaction_edges(
    const Ocel& log, const std::map<std::string, std::string>& types) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& [event_id, objects] : objects_by_event(log, types)) {
    (void)event_id;
    for (std::size_t left = 0; left < objects.size(); ++left) {
      for (std::size_t right = left + 1; right < objects.size(); ++right) {
        result.emplace(objects[left], objects[right]);
      }
    }
  }
  return result;
}

Ocel filter_objects(const Ocel& log, const ObjectIds& selected) {
  Ocel result;
  result.globals = log.globals;
  result.event_type_attributes = log.event_type_attributes;
  result.object_type_attributes = log.object_type_attributes;
  std::set<std::string> event_ids;
  for (const auto& object : log.objects) {
    if (selected.contains(object.id)) result.objects.push_back(object);
  }
  for (const auto& relation : log.relations) {
    if (!selected.contains(relation.object_id)) continue;
    result.relations.push_back(relation);
    event_ids.insert(relation.event_id);
  }
  for (const auto& event : log.events) {
    if (event_ids.contains(event.id)) result.events.push_back(event);
  }
  for (const auto& relation : log.o2o) {
    if (selected.contains(relation.source_id) &&
        selected.contains(relation.target_id)) {
      result.o2o.push_back(relation);
    }
  }
  for (const auto& relation : log.e2e) {
    if (event_ids.contains(relation.source_id) &&
        event_ids.contains(relation.target_id)) {
      result.e2e.push_back(relation);
    }
  }
  for (const auto& change : log.object_changes) {
    if (selected.contains(change.object_id)) result.object_changes.push_back(change);
  }
  return result;
}

std::vector<ObjectIds> connected_components(
    const std::set<std::pair<std::string, std::string>>& edges,
    const std::optional<double>& max_degree_centrality) {
  std::map<std::string, std::set<std::string>> adjacency;
  for (const auto& [left, right] : edges) {
    adjacency[left].insert(right);
    adjacency[right].insert(left);
  }
  ObjectIds removed;
  if (max_degree_centrality) {
    const double denominator = adjacency.size() <= 1
                                   ? 1.0
                                   : static_cast<double>(adjacency.size() - 1);
    for (const auto& [object, neighbours] : adjacency) {
      const double centrality =
          static_cast<double>(neighbours.size()) / denominator;
      if (centrality > *max_degree_centrality) removed.insert(object);
    }
    for (const auto& object : removed) adjacency.erase(object);
    for (auto& [object, neighbours] : adjacency) {
      (void)object;
      for (const auto& gone : removed) neighbours.erase(gone);
    }
  }

  std::vector<ObjectIds> result;
  ObjectIds seen;
  for (const auto& [start, neighbours] : adjacency) {
    (void)neighbours;
    if (seen.contains(start)) continue;
    ObjectIds component;
    std::vector<std::string> stack{start};
    seen.insert(start);
    while (!stack.empty()) {
      const auto object = stack.back();
      stack.pop_back();
      component.insert(object);
      for (const auto& next : adjacency.at(object)) {
        if (!seen.contains(next)) {
          seen.insert(next);
          stack.push_back(next);
        }
      }
    }
    if (!component.empty()) result.push_back(std::move(component));
  }
  return result;
}

struct StartOrder {
  bool known = false;
  double timestamp = 0.0;
  std::size_t event_index = 0;
};

std::map<std::string, StartOrder> object_start_order(
    const Ocel& log, const std::map<std::string, std::string>& types,
    const std::map<std::string, const OcelEvent*>& events) {
  std::map<std::string, StartOrder> result;
  for (std::size_t index = 0; index < log.relations.size(); ++index) {
    const auto& relation = log.relations[index];
    if (!types.contains(relation.object_id)) continue;
    const auto event_it = events.find(relation.event_id);
    if (event_it == events.end()) continue;
    const auto parsed = parse_timestamp(event_it->second->timestamp);
    StartOrder candidate;
    candidate.known = parsed.has_value() && std::isfinite(*parsed);
    candidate.timestamp = candidate.known ? *parsed : 0.0;
    candidate.event_index = index;
    const auto old = result.find(relation.object_id);
    const auto earlier = [&](const StartOrder& left, const StartOrder& right) {
      if (left.known && right.known && left.timestamp != right.timestamp)
        return left.timestamp < right.timestamp;
      if (left.known != right.known) return left.known;
      return left.event_index < right.event_index;
    };
    if (old == result.end() || earlier(candidate, old->second))
      result[relation.object_id] = candidate;
  }
  return result;
}

std::vector<std::pair<std::string, ObjectIds>> ancestors_descendants(
    const Ocel& log, const OcelSplitOptions& options) {
  const auto types = object_types(log);
  const auto events = events_by_id(log);
  const auto edges = interaction_edges(log, types);
  const auto starts = object_start_order(log, types, events);
  ObjectIds centers;
  for (const auto& [object, type] : types) {
    if (type == options.object_type) centers.insert(object);
  }
  std::map<std::string, std::set<std::string>> directed;
  for (const auto& [left, right] : edges) {
    const auto left_it = starts.find(left);
    const auto right_it = starts.find(right);
    if (left_it == starts.end() || right_it == starts.end()) continue;
    const auto& a = left_it->second;
    const auto& b = right_it->second;
    auto before = [&](const StartOrder& first, const StartOrder& second) {
      if (first.known && second.known && first.timestamp != second.timestamp)
        return first.timestamp < second.timestamp;
      if (first.known != second.known) return first.known;
      return first.event_index < second.event_index;
    };
    if (before(a, b)) {
      directed[left].insert(right);
    } else if (before(b, a)) {
      directed[right].insert(left);
    } else if (centers.contains(left)) {
      directed[left].insert(right);
    } else if (centers.contains(right)) {
      directed[right].insert(left);
    }
  }

  std::vector<std::pair<std::string, ObjectIds>> result;
  for (const auto& center : centers) {
    if (result.size() >= options.max_objects) break;
    ObjectIds selected{center};
    std::vector<std::string> forward{center};
    while (!forward.empty()) {
      const auto current = forward.back();
      forward.pop_back();
      for (const auto& next : directed[current]) {
        if (selected.insert(next).second) forward.push_back(next);
      }
    }
    std::map<std::string, std::set<std::string>> reverse;
    for (const auto& [source, targets] : directed)
      for (const auto& target : targets) reverse[target].insert(source);
    std::vector<std::string> backward{center};
    while (!backward.empty()) {
      const auto current = backward.back();
      backward.pop_back();
      for (const auto& previous : reverse[current]) {
        if (selected.insert(previous).second) backward.push_back(previous);
      }
    }
    result.emplace_back(center, std::move(selected));
  }
  return result;
}

struct EnrichmentIndex {
  std::vector<std::string> event_order;
  std::map<std::string, std::size_t> event_index;
  std::map<std::string, std::vector<std::string>> objects_by_event;
  std::map<std::string, std::vector<std::size_t>> events_by_object;
};

EnrichmentIndex make_enrichment_index(const Ocel& log) {
  EnrichmentIndex result;
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    if (log.events[index].id.empty() ||
        !result.event_index.emplace(log.events[index].id, index).second) {
      throw std::invalid_argument(
          "OCEL enrichment events require unique non-empty ids");
    }
    result.event_order.push_back(log.events[index].id);
  }
  std::set<std::string> object_ids;
  for (const auto& object : log.objects) {
    if (object.id.empty() || !object_ids.insert(object.id).second)
      throw std::invalid_argument(
          "OCEL enrichment objects require unique non-empty ids");
  }

  std::map<std::string, std::set<std::string>> objects_by_event;
  for (const auto& relation : log.relations) {
    if (!result.event_index.contains(relation.event_id) ||
        !object_ids.contains(relation.object_id)) {
      throw std::invalid_argument(
          "OCEL enrichment relation references an unknown id");
    }
    objects_by_event[relation.event_id].insert(relation.object_id);
  }
  for (const auto& event_id : result.event_order) {
    const auto found = objects_by_event.find(event_id);
    if (found == objects_by_event.end()) continue;
    auto& objects = result.objects_by_event[event_id];
    objects.assign(found->second.begin(), found->second.end());
    for (const auto& object_id : objects)
      result.events_by_object[object_id].push_back(result.event_index.at(event_id));
  }
  return result;
}

using ObjectPair = std::pair<std::string, std::string>;

std::set<ObjectPair> enrichment_interaction_graph(const EnrichmentIndex& index) {
  std::set<ObjectPair> result;
  for (const auto& [event_id, objects] : index.objects_by_event) {
    (void)event_id;
    for (std::size_t left = 0; left < objects.size(); ++left)
      for (std::size_t right = left + 1; right < objects.size(); ++right)
        result.emplace(objects[left], objects[right]);
  }
  return result;
}

std::set<ObjectPair> enrichment_descendants_graph(const EnrichmentIndex& index) {
  std::set<ObjectPair> result;
  std::set<std::string> seen;
  for (const auto& event_id : index.event_order) {
    const auto found = index.objects_by_event.find(event_id);
    if (found == index.objects_by_event.end()) continue;
    std::vector<std::string> unseen;
    for (const auto& object : found->second) {
      if (!seen.contains(object)) unseen.push_back(object);
    }
    for (const auto& object : found->second) {
      if (!seen.contains(object)) continue;
      for (const auto& child : unseen) result.emplace(object, child);
    }
    seen.insert(unseen.begin(), unseen.end());
  }
  return result;
}

std::set<ObjectPair> enrichment_cobirth_graph(const EnrichmentIndex& index) {
  std::set<ObjectPair> result;
  std::set<std::string> seen;
  for (const auto& event_id : index.event_order) {
    const auto found = index.objects_by_event.find(event_id);
    if (found == index.objects_by_event.end()) continue;
    std::vector<std::string> unseen;
    for (const auto& object : found->second)
      if (!seen.contains(object)) unseen.push_back(object);
    for (std::size_t left = 0; left < unseen.size(); ++left)
      for (std::size_t right = left + 1; right < unseen.size(); ++right)
        result.emplace(unseen[left], unseen[right]);
    seen.insert(unseen.begin(), unseen.end());
  }
  return result;
}

std::set<ObjectPair> enrichment_codeath_graph(const EnrichmentIndex& index) {
  std::set<ObjectPair> result;
  std::set<std::string> seen;
  for (auto event = index.event_order.rbegin(); event != index.event_order.rend();
       ++event) {
    const auto found = index.objects_by_event.find(*event);
    if (found == index.objects_by_event.end()) continue;
    std::vector<std::string> unseen;
    for (const auto& object : found->second)
      if (!seen.contains(object)) unseen.push_back(object);
    for (std::size_t left = 0; left < unseen.size(); ++left)
      for (std::size_t right = left + 1; right < unseen.size(); ++right)
        result.emplace(unseen[left], unseen[right]);
    seen.insert(unseen.begin(), unseen.end());
  }
  return result;
}

std::set<ObjectPair> enrichment_inheritance_graph(const EnrichmentIndex& index) {
  std::set<ObjectPair> result;
  std::map<std::string, std::size_t> first, last;
  for (const auto& [object, events] : index.events_by_object) {
    if (events.empty()) continue;
    first[object] = events.front();
    last[object] = events.back();
  }
  for (const auto& event_id : index.event_order) {
    const auto found = index.objects_by_event.find(event_id);
    if (found == index.objects_by_event.end()) continue;
    const auto event_index = index.event_index.at(event_id);
    for (const auto& source : found->second) {
      if (last[source] != event_index) continue;
      for (const auto& target : found->second) {
        if (source != target && first[target] == event_index)
          result.emplace(source, target);
      }
    }
  }
  std::set<ObjectPair> asymmetric;
  for (const auto& edge : result)
    if (!result.contains({edge.second, edge.first})) asymmetric.insert(edge);
  return asymmetric;
}

void append_enrichment_edge(Ocel& log, std::set<std::tuple<std::string,
                                                           std::string,
                                                           std::string>>& seen,
                            const std::string& source, const std::string& target,
                            const std::string& qualifier) {
  if (source == target || !seen.emplace(source, target, qualifier).second) return;
  log.o2o.push_back({source, target, qualifier});
}

void apply_lifecycle_qualifiers(Ocel& log, const EnrichmentIndex& index) {
  std::map<std::pair<std::size_t, std::string>, std::string> qualifiers;
  for (const auto& [object, events] : index.events_by_object) {
    if (events.empty()) continue;
    for (std::size_t offset = 0; offset < events.size(); ++offset) {
      const auto event = events[offset];
      const auto qualifier = events.size() == 1 || offset == 0
                                 ? "creation"
                                 : offset + 1 == events.size()
                                       ? "termination"
                                       : "other";
      qualifiers[{event, object}] = qualifier;
    }
  }
  for (auto& relation : log.relations) {
    const auto event = index.event_index.find(relation.event_id);
    if (event == index.event_index.end()) continue;
    const auto qualifier = qualifiers.find({event->second, relation.object_id});
    if (qualifier != qualifiers.end()) relation.qualifier = qualifier->second;
  }
}

const char* enrichment_graph_name(OcelEnrichmentGraph graph) {
  switch (graph) {
    case OcelEnrichmentGraph::kInteraction: return "object_interaction_graph";
    case OcelEnrichmentGraph::kDescendants: return "object_descendants_graph";
    case OcelEnrichmentGraph::kInheritance: return "object_inheritance_graph";
    case OcelEnrichmentGraph::kCobirth: return "object_cobirth_graph";
    case OcelEnrichmentGraph::kCodeath: return "object_codeath_graph";
  }
  throw std::invalid_argument("unknown OCEL enrichment graph");
}

}  // namespace

Ocel enrich_ocel(const Ocel& log, const OcelEnrichmentOptions& options) {
  const auto index = make_enrichment_index(log);
  Ocel result = log;
  std::set<std::tuple<std::string, std::string, std::string>> seen;
  for (const auto& relation : result.o2o)
    seen.emplace(relation.source_id, relation.target_id, relation.qualifier);

  const auto add_undirected = [&](const std::set<ObjectPair>& edges,
                                  const char* qualifier) {
    for (const auto& [source, target] : edges) {
      append_enrichment_edge(result, seen, source, target, qualifier);
      append_enrichment_edge(result, seen, target, source, qualifier);
    }
  };
  const auto add_directed = [&](const std::set<ObjectPair>& edges,
                                const char* qualifier) {
    for (const auto& [source, target] : edges)
      append_enrichment_edge(result, seen, source, target, qualifier);
  };
  if (options.include_interaction)
    add_undirected(enrichment_interaction_graph(index),
                   enrichment_graph_name(OcelEnrichmentGraph::kInteraction));
  if (options.include_descendants)
    add_directed(enrichment_descendants_graph(index),
                 enrichment_graph_name(OcelEnrichmentGraph::kDescendants));
  if (options.include_inheritance)
    add_directed(enrichment_inheritance_graph(index),
                 enrichment_graph_name(OcelEnrichmentGraph::kInheritance));
  if (options.include_cobirth)
    add_undirected(enrichment_cobirth_graph(index),
                   enrichment_graph_name(OcelEnrichmentGraph::kCobirth));
  if (options.include_codeath)
    add_undirected(enrichment_codeath_graph(index),
                   enrichment_graph_name(OcelEnrichmentGraph::kCodeath));
  if (options.include_lifecycle_qualifiers)
    apply_lifecycle_qualifiers(result, index);
  return result;
}

std::string serialize_ocel_enrichment_json(const Ocel& log,
                                           const OcelEnrichmentOptions& options) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_ENRICHMENT\",\"variant\":\"CLASSIC\",\"graphs\":[";
  bool first = true;
  const auto add = [&](OcelEnrichmentGraph graph, bool enabled) {
    if (!enabled) return;
    if (!first) out << ',';
    first = false;
    write_json_string(out, enrichment_graph_name(graph));
  };
  add(OcelEnrichmentGraph::kInteraction, options.include_interaction);
  add(OcelEnrichmentGraph::kDescendants, options.include_descendants);
  add(OcelEnrichmentGraph::kInheritance, options.include_inheritance);
  add(OcelEnrichmentGraph::kCobirth, options.include_cobirth);
  add(OcelEnrichmentGraph::kCodeath, options.include_codeath);
  out << "],\"lifecycle_qualifiers\":"
      << (options.include_lifecycle_qualifiers ? "true" : "false")
      << ",\"log\":" << serialize_ocel_json(log) << '}';
  return out.str();
}

OcelSplitResult split_ocel(const Ocel& log, const OcelSplitOptions& options) {
  if (options.variant != OcelSplitVariant::kConnectedComponents &&
      options.variant != OcelSplitVariant::kAncestorsDescendants) {
    throw std::invalid_argument("unknown OCEL split variant");
  }
  if (options.max_degree_centrality &&
      (!std::isfinite(*options.max_degree_centrality) ||
       *options.max_degree_centrality < 0.0 ||
       *options.max_degree_centrality > 1.0)) {
    throw std::invalid_argument(
        "OCEL split max_degree_centrality must be in [0,1]");
  }
  OcelSplitResult result;
  result.variant = options.variant;
  if (options.variant == OcelSplitVariant::kConnectedComponents) {
    const auto types = object_types(log);
    for (const auto& component : connected_components(
             interaction_edges(log, types), options.max_degree_centrality)) {
      result.logs.push_back(filter_objects(log, component));
    }
  } else {
    if (options.object_type.empty())
      throw std::invalid_argument(
          "OCEL ancestors/descendants split requires object_type");
    for (const auto& [center, component] : ancestors_descendants(log, options)) {
      if (component.empty()) continue;
      result.central_objects.push_back(center);
      result.logs.push_back(filter_objects(log, component));
    }
  }
  return result;
}

std::string serialize_ocel_split_json(const OcelSplitResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_SPLIT\",\"variant\":\""
      << variant_name(result.variant) << "\",\"logs\":[";
  for (std::size_t index = 0; index < result.logs.size(); ++index) {
    if (index != 0) out << ',';
    if (result.variant == OcelSplitVariant::kAncestorsDescendants) {
      out << "{\"central_object\":";
      const auto& center = result.central_objects.at(index);
      write_json_string(out, center);
      out << ",\"log\":" << serialize_ocel_json(result.logs[index]);
      out << '}';
    } else {
      out << serialize_ocel_json(result.logs[index]);
    }
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
