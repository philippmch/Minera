#include "minera/ocel_transform.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.objects = { {"o1", "order", {}}, {"o2", "order", {}},
                  {"i1", "item", {}}, {"i2", "item", {}},
                  {"i3", "item", {}}, {"i4", "item", {}},
                  {"isolated", "item", {}} };
  log.events = { {"e1", "Create", "2026-01-01T00:00:00Z", {}},
                 {"e2", "Pack", "2026-01-02T00:00:00Z", {}},
                 {"e3", "Ship", "2026-01-03T00:00:00Z", {}},
                 {"e4", "Pair", "2026-01-04T00:00:00Z", {}} };
  log.relations = { {"e1", "o1", ""}, {"e1", "i1", ""},
                    {"e2", "i1", ""}, {"e2", "o2", ""},
                    {"e3", "o2", ""}, {"e3", "i2", ""},
                    {"e4", "i3", ""}, {"e4", "i4", ""} };
  log.o2o = { {"o1", "o2", "contains"} };
  log.e2e = { {"e1", "e2", "precedes"} };
  log.object_changes = { {"o1", "order", "2026-01-04T00:00:00Z",
                          "state", std::string("done")} };
  return log;
}

void connected_components_contract() {
  minera::OcelSplitOptions options;
  options.variant = minera::OcelSplitVariant::kConnectedComponents;
  const auto result = minera::split_ocel(sample(), options);
  assert(result.logs.size() == 2);
  assert(result.central_objects.empty());
  assert(result.logs[0].objects.size() == 4);
  assert(result.logs[1].objects.size() == 2);
  assert(result.logs[0].events.size() == 3);
  assert(result.logs[1].events.size() == 1);
  assert(result.logs[0].object_changes.empty() ||
         result.logs[1].object_changes.empty());
  const auto json = minera::serialize_ocel_split_json(result);
  assert(json.find("\"family\":\"OCEL_SPLIT\"") != std::string::npos);
  assert(json.find("CONNECTED_COMPONENTS") != std::string::npos);
}

void ancestors_descendants_contract() {
  minera::OcelSplitOptions options;
  options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "order";
  options.max_objects = 1;
  const auto result = minera::split_ocel(sample(), options);
  assert(result.logs.size() == 1 && result.central_objects.size() == 1);
  assert(result.central_objects[0] == "o1");
  assert(result.logs[0].objects.size() == 4);
  assert(result.logs[0].o2o.size() == 1);
  assert(result.logs[0].e2e.size() == 1 &&
         result.logs[0].e2e.front().qualifier == "precedes");
  assert(result.logs[0].object_changes.size() == 1);
  const auto json = minera::serialize_ocel_split_json(result);
  assert(json.find("ANCESTORS_DESCENDANTS") != std::string::npos);
  assert(json.find("central_object") != std::string::npos);
}

void validation_contract() {
  minera::OcelSplitOptions options;
  options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
  bool threw = false;
  try {
    (void)minera::split_ocel(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  options.variant = minera::OcelSplitVariant::kConnectedComponents;
  options.max_degree_centrality = 1.1;
  threw = false;
  try {
    (void)minera::split_ocel(sample(), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void enrichment_contract() {
  const auto result = minera::enrich_ocel(sample());
  // Existing O2O records are preserved; each selected graph contributes its
  // deterministic relation family without duplicate triples.
  assert(result.o2o.size() == 22);
  bool interaction = false, descendant = false, inheritance = false;
  bool cobirth = false, codeath = false;
  for (const auto& relation : result.o2o) {
    interaction = interaction || relation.qualifier == "object_interaction_graph";
    descendant = descendant || relation.qualifier == "object_descendants_graph";
    inheritance = inheritance || relation.qualifier == "object_inheritance_graph";
    cobirth = cobirth || relation.qualifier == "object_cobirth_graph";
    codeath = codeath || relation.qualifier == "object_codeath_graph";
  }
  assert(interaction && descendant && inheritance && cobirth && codeath);
  assert(result.relations[0].qualifier == "creation");
  assert(result.relations[1].qualifier == "creation");
  assert(result.relations[2].qualifier == "termination");
  assert(result.relations[4].qualifier == "termination");
  assert(result.relations[6].qualifier == "creation");
  const auto json = minera::serialize_ocel_enrichment_json(result);
  assert(json.find("\"family\":\"OCEL_ENRICHMENT\"") != std::string::npos);
  assert(json.find("object_codeath_graph") != std::string::npos);
  assert(json.find("\"lifecycle_qualifiers\":true") != std::string::npos);
  assert(json.find("\"ocel:o2o\"") != std::string::npos);

  minera::OcelEnrichmentOptions limited;
  limited.include_interaction = false;
  limited.include_descendants = false;
  limited.include_inheritance = false;
  limited.include_cobirth = false;
  limited.include_codeath = false;
  limited.include_lifecycle_qualifiers = false;
  const auto unchanged = minera::enrich_ocel(sample(), limited);
  assert(unchanged.o2o.size() == 1 && unchanged.relations[0].qualifier.empty());
  assert(minera::serialize_ocel_enrichment_json(unchanged, limited).find(
             "\"graphs\":[]") != std::string::npos);
}

void object_graph_contract() {
  const auto log = sample();
  const auto interaction = minera::discover_ocel_object_graph(
      log, minera::OcelEnrichmentGraph::kInteraction);
  assert(interaction.object_ids.size() == log.objects.size());
  assert(interaction.object_ids.back() == "o2");
  const std::vector<std::pair<std::string, std::string>> expected_interaction = {
      {"i1", "o1"}, {"i1", "o2"}, {"i2", "o2"}, {"i3", "i4"}};
  assert(interaction.edges == expected_interaction);
  const auto descendants = minera::discover_ocel_object_graph(
      log, minera::OcelEnrichmentGraph::kDescendants);
  const std::vector<std::pair<std::string, std::string>> expected_descendants = {
      {"i1", "o2"}, {"o2", "i2"}};
  assert(descendants.edges == expected_descendants);
  const auto inheritance = minera::discover_ocel_object_graph(
      log, minera::OcelEnrichmentGraph::kInheritance);
  const std::vector<std::pair<std::string, std::string>> expected_inheritance = {
      {"i1", "o2"}, {"o1", "i1"}, {"o2", "i2"}};
  assert(inheritance.edges == expected_inheritance);
  const auto cobirth = minera::discover_ocel_object_graph(
      log, minera::OcelEnrichmentGraph::kCobirth);
  const std::vector<std::pair<std::string, std::string>> expected_cobirth = {
      {"i1", "o1"}, {"i3", "i4"}};
  assert(cobirth.edges == expected_cobirth);
  const auto codeath = minera::discover_ocel_object_graph(
      log, minera::OcelEnrichmentGraph::kCodeath);
  const std::vector<std::pair<std::string, std::string>> expected_codeath = {
      {"i2", "o2"}, {"i3", "i4"}};
  assert(codeath.edges == expected_codeath);
  const auto json = minera::serialize_ocel_object_graph_json(interaction);
  assert(json.find("\"family\":\"OCEL_OBJECT_GRAPH\"") !=
         std::string::npos);
  assert(json.find("\"object_ids\"") != std::string::npos);
  assert(json.find("[\"i1\",\"o1\"]") != std::string::npos);

  minera::Ocel malformed = log;
  malformed.objects.push_back({"o1", "duplicate", {}});
  bool threw = false;
  try {
    (void)minera::discover_ocel_object_graph(
        malformed, minera::OcelEnrichmentGraph::kInteraction);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void heterogeneous_graph_contract() {
  auto log = sample();
  log.events.front().attributes.push_back({"priority", std::int64_t{3}});
  log.objects.front().attributes.push_back({"active", true});
  const auto graph = minera::discover_ocel_heterogeneous_graph(log);
  assert(graph.include_df && graph.include_object_changes);
  assert(graph.nodes.size() == log.events.size() + log.objects.size() +
         log.object_changes.size());
  assert(graph.nodes[0].id == "e1" && graph.nodes[0].type == "EVENT");
  assert(graph.nodes[0].activity == "Create");
  assert(graph.nodes[4].id == "o1" && graph.nodes[4].type == "OBJECT");
  assert(graph.nodes[4].object_type == "order");
  assert(graph.nodes.back().id == "@@change##0" &&
         graph.nodes.back().type == "CHANGE");
  assert(graph.nodes.back().value.has_value());

  assert(graph.edges.size() == log.relations.size() + log.o2o.size() + 2 +
         log.object_changes.size());
  for (std::size_t index = 0; index < log.relations.size(); ++index) {
    assert(graph.edges[index].type == "E2O");
    assert(graph.edges[index].source == log.relations[index].event_id);
    assert(graph.edges[index].target == log.relations[index].object_id);
  }
  assert(graph.edges[8].type == "O2O");
  assert(graph.edges[9].type == "DF" && graph.edges[9].source == "e1" &&
         graph.edges[9].target == "e2" && graph.edges[9].object == "i1");
  assert(graph.edges[10].type == "DF" && graph.edges[10].source == "e2" &&
         graph.edges[10].target == "e3" && graph.edges[10].object == "o2");
  assert(graph.edges[11].type == "CHANGE" &&
         graph.edges[11].source == "@@change##0" &&
         graph.edges[11].target == "o1");

  const auto json = minera::serialize_ocel_heterogeneous_graph_json(graph);
  assert(json.find("\"family\":\"OCEL_GRAPH\"") != std::string::npos);
  assert(json.find("\"variant\":\"HETEROGENEOUS\"") !=
         std::string::npos);
  assert(json.find("\"type\":\"DF\"") != std::string::npos);
  assert(json.find("\"priority\":3") != std::string::npos);
  assert(json.find("\"value\":\"done\"") != std::string::npos);
  assert(json == minera::serialize_ocel_heterogeneous_graph_json(graph));

  minera::OcelHeterogeneousGraphOptions limited;
  limited.include_df = false;
  limited.include_object_changes = false;
  const auto without_optional =
      minera::discover_ocel_heterogeneous_graph(log, limited);
  assert(!without_optional.include_df &&
         !without_optional.include_object_changes);
  assert(without_optional.nodes.size() ==
         log.events.size() + log.objects.size());
  assert(without_optional.edges.size() == log.relations.size() + log.o2o.size());
  for (const auto& edge : without_optional.edges) {
    assert(edge.type == "E2O" || edge.type == "O2O");
  }

  // Event/object namespaces share the graph key space, and synthetic change
  // identifiers use that same key space. Later records update the keyed node.
  auto colliding = log;
  colliding.events.push_back(
      {"o1", "Shadow", "2026-01-05T00:00:00Z", {}});
  colliding.events.push_back(
      {"@@change##0", "ShadowChange", "2026-01-06T00:00:00Z", {}});
  const auto keyed = minera::discover_ocel_heterogeneous_graph(colliding);
  assert(keyed.nodes.size() == colliding.events.size() + colliding.objects.size() +
         colliding.object_changes.size() - 2);
  const auto find_node = [&](const std::string& id) -> const minera::OcelGraphNode* {
    for (const auto& node : keyed.nodes) {
      if (node.id == id) return &node;
    }
    return nullptr;
  };
  const auto* o1 = find_node("o1");
  assert(o1 && o1->type == "OBJECT" && o1->object_type == "order");
  const auto* change = find_node("@@change##0");
  assert(change && change->type == "CHANGE" && change->object_id == "o1");
}

}  // namespace

int main() {
  connected_components_contract();
  ancestors_descendants_contract();
  validation_contract();
  enrichment_contract();
  object_graph_contract();
  heterogeneous_graph_contract();
}
