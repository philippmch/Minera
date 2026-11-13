#include "minera/ocel_transform.hpp"
#include "minera/xes_export.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

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
  assert(result.candidate_count == 2 && !result.truncated);
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

// The single-pass component filter must equal filtering the whole log once
// per component, including duplicate ids, cross-component e2e and objects
// removed by the degree-centrality bound. The reference component keeps no
// O2O rows and no object changes (round BY).
minera::Ocel per_component_reference(const minera::Ocel& log,
                                     const std::set<std::string>& selected) {
  minera::Ocel result;
  result.globals = log.globals;
  result.event_type_attributes = log.event_type_attributes;
  result.object_type_attributes = log.object_type_attributes;
  std::set<std::string> event_ids;
  for (const auto& object : log.objects)
    if (selected.contains(object.id)) result.objects.push_back(object);
  for (const auto& relation : log.relations) {
    if (!selected.contains(relation.object_id)) continue;
    result.relations.push_back(relation);
    event_ids.insert(relation.event_id);
  }
  for (const auto& event : log.events)
    if (event_ids.contains(event.id)) result.events.push_back(event);
  for (const auto& relation : log.e2e)
    if (event_ids.contains(relation.source_id) && event_ids.contains(relation.target_id))
      result.e2e.push_back(relation);
  return result;
}

void connected_components_single_pass_contract() {
  std::mt19937 random(20261103);
  for (int round = 0; round < 400; ++round) {
    const auto pick = [&](int bound) {
      return static_cast<int>(random() % static_cast<unsigned>(bound));
    };
    const int objects = 1 + pick(12), events = 1 + pick(12);
    const auto object_id = [&] { return "o" + std::to_string(pick(objects + 1)); };
    const auto event_id = [&] { return "e" + std::to_string(pick(events + 1)); };
    minera::Ocel log;
    for (int index = 0; index < objects; ++index)
      log.objects.push_back({"o" + std::to_string(pick(objects)),
                             index % 2 ? "order" : "item", {}});
    for (int index = 0; index < events; ++index)
      log.events.push_back({"e" + std::to_string(pick(events)), "A",
                            "2026-01-01T00:00:00Z", {}});
    for (int index = pick(30); index > 0; --index)
      log.relations.push_back({event_id(), object_id(), ""});
    for (int index = pick(8); index > 0; --index)
      log.o2o.push_back({object_id(), object_id(), std::to_string(index)});
    for (int index = pick(8); index > 0; --index)
      log.e2e.push_back({event_id(), event_id(), std::to_string(index)});
    for (int index = pick(6); index > 0; --index)
      log.object_changes.push_back({object_id(), "order", "2026-01-02T00:00:00Z",
                                    "state", std::string("x")});
    minera::OcelSplitOptions options;
    options.variant = minera::OcelSplitVariant::kConnectedComponents;
    if (round % 3 == 0) options.max_degree_centrality = 0.25 * pick(5);
    const auto result = minera::split_ocel(log, options);
    std::set<std::string> covered;
    for (const auto& part : result.logs) {
      std::set<std::string> selected;
      for (const auto& relation : part.relations) selected.insert(relation.object_id);
      assert(!selected.empty());
      for (const auto& id : selected) assert(covered.insert(id).second);
      assert(minera::serialize_ocel_json(part) ==
             minera::serialize_ocel_json(per_component_reference(log, selected)));
    }
  }
}

void ancestors_descendants_contract() {
  minera::OcelSplitOptions options;
  options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "order";
  options.max_objects = 1;
  const auto result = minera::split_ocel(sample(), options);
  assert(result.logs.size() == 1 && result.central_objects.size() == 1);
  assert(result.candidate_count == 2 && result.truncated);
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
  options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "order";
  options.max_objects = 0;
  threw = false;
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
  // Captured from pm4py 2.7.22 ocel_o2o_enrichment on the same log: the
  // original row comes first; interaction has both orientations, the others
  // one; the inheritance label repeats the descendants edges (reference quirk).
  const std::vector<std::tuple<std::string, std::string, std::string>> expected_o2o = {
      {"o1", "o2", "contains"},
      {"i1", "o1", "object_cobirth_graph"},
      {"i1", "o1", "object_interaction_graph"},
      {"i1", "o2", "object_descendants_graph"},
      {"i1", "o2", "object_inheritance_graph"},
      {"i1", "o2", "object_interaction_graph"},
      {"i2", "o2", "object_codeath_graph"},
      {"i2", "o2", "object_interaction_graph"},
      {"i3", "i4", "object_cobirth_graph"},
      {"i3", "i4", "object_codeath_graph"},
      {"i3", "i4", "object_interaction_graph"},
      {"i4", "i3", "object_interaction_graph"},
      {"o1", "i1", "object_interaction_graph"},
      {"o2", "i1", "object_interaction_graph"},
      {"o2", "i2", "object_descendants_graph"},
      {"o2", "i2", "object_inheritance_graph"},
      {"o2", "i2", "object_interaction_graph"}};
  assert(result.o2o.size() == expected_o2o.size());
  for (std::size_t row = 0; row < expected_o2o.size(); ++row) {
    const auto& [source, target, qualifier] = expected_o2o[row];
    assert(result.o2o[row].source_id == source && result.o2o[row].target_id == target &&
           result.o2o[row].qualifier == qualifier);
  }
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

void flatten_contract() {
  minera::Ocel log;
  log.objects = {
      {"o1", "order",
       {{"region", std::string("EU")},
        {"amount", std::int64_t{7}},
        {"active", true},
        {"ratio", 1.25},
        {"missing", std::nullptr_t{}}}},
      {"o2", "order", {}},
      {"i1", "item", {}}};
  log.events = {
      {"e1", "Create", "2026-01-01T00:00:00Z",
       {{"priority", std::int64_t{3}}, {"flag", false},
        {"note", std::string("first")},
        {"missing", std::nullptr_t{}}}},
      {"e2", "Pack", "2026-01-02T00:00:00Z", {}},
      {"unused", "Ignore", "2026-01-03T00:00:00Z", {}}};
  // Relation order is intentionally different from event order, and the
  // repeated e1/o1 row must survive as a repeated event occurrence.
  log.relations = {{"e2", "o1", "first"}, {"e1", "o1", "second"},
                   {"e1", "o1", "duplicate"}, {"e2", "o2", ""},
                   {"unused", "i1", ""}};

  const auto flattened = minera::flatten_ocel_to_event_log(log, "order");
  assert(flattened.traces.size() == 2);
  assert(flattened.traces[0].case_id == "o1");
  assert(flattened.traces[1].case_id == "o2");
  assert(flattened.traces[0].events.size() == 3);
  assert(flattened.traces[1].events.size() == 1);
  // The source-event order drives the left join. Relation order remains the
  // stable tie-breaker, so both e1/o1 duplicates precede e2/o1.
  assert(flattened.traces[0].events[0].activity == "Create");
  assert(flattened.traces[0].events[1].activity == "Create");
  assert(flattened.traces[0].events[2].activity == "Pack");
  assert(flattened.traces[1].events[0].activity == "Pack");
  assert(flattened.traces[0].events[0].input_order == 0);
  assert(flattened.traces[0].events[1].input_order == 1);
  assert(flattened.traces[0].events[2].input_order == 2);
  assert(flattened.traces[1].events[0].input_order == 3);
  assert(flattened.traces[0].events[0].attributes.size() == 4);
  assert(flattened.traces[0].events[0].attributes[0].key == "ocel:eid");
  assert(flattened.traces[0].events[0].attributes[0].value == "e1");
  assert(flattened.traces[0].events[0].attributes[1].key == "priority");
  assert(flattened.traces[0].events[0].attributes[1].kind ==
         minera::AttributeKind::kInt);
  assert(flattened.traces[0].events[0].attributes[2].kind ==
         minera::AttributeKind::kBoolean);
  assert(flattened.traces[0].events[0].attributes[3].kind ==
         minera::AttributeKind::kString);
  assert(flattened.traces[0].events[2].attributes.size() == 1);
  assert(flattened.traces[0].events[2].attributes[0].key == "ocel:eid");
  assert(flattened.traces[0].events[2].attributes[0].value == "e2");
  assert(flattened.traces[1].events[0].attributes[0].value == "e2");
  assert(flattened.traces[0].attributes.size() == 5);
  assert(flattened.traces[0].attributes[0].key == "ocel:type");
  assert(flattened.traces[0].attributes[1].key == "region");
  assert(flattened.traces[0].attributes[2].key == "amount");
  assert(flattened.traces[0].attributes[3].key == "active");
  assert(flattened.traces[0].attributes[4].key == "ratio");
  assert(flattened.traces[0].attribute("missing") == nullptr);
  assert(flattened.traces[0].attribute("ocel:type") != nullptr);
  assert(flattened.event_count() == 4);
  const auto xes = minera::to_xes(flattened, "ocel flatten");
  assert(xes.find("value=\"o1\"") != std::string::npos);
  assert(xes.find("key=\"ocel:type\" value=\"order\"") !=
         std::string::npos);
  assert(xes.find("key=\"amount\" value=\"7\"") !=
         std::string::npos);
  assert(xes.find("key=\"ocel:eid\" value=\"e1\"") !=
         std::string::npos);
  assert(xes.find("key=\"case:ocel:type\"") == std::string::npos);
  assert(xes.find("key=\"case:amount\"") == std::string::npos);

  bool threw = false;
  try {
    (void)minera::flatten_ocel_to_event_log(log, "");
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  const auto unknown = minera::flatten_ocel_to_event_log(log, "missing");
  assert(unknown.traces.empty());
  assert(unknown.event_count() == 0);
  assert(minera::to_xes(unknown, "ocel flatten").find("<trace>") ==
         std::string::npos);

  threw = false;
  try {
    (void)minera::flatten_ocel_to_event_log(log, "   ");
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto spaced = log;
  spaced.objects[2].type = " order ";
  minera::OcelFlattenOptions spaced_options;
  spaced_options.object_type = " order ";
  const auto exact =
      minera::flatten_ocel_to_event_log(spaced, spaced_options);
  assert(exact.traces.size() == 1);
  assert(exact.traces[0].case_id == "i1");
  assert(exact.traces[0].attribute("ocel:type") != nullptr);
  assert(*exact.traces[0].attribute("ocel:type") == " order ");

  auto custom = log;
  custom.events[0].attributes.push_back(
      {"display_activity", std::string("Created")});
  custom.events[0].attributes.push_back(
      {"event_time", std::int64_t{42}});
  custom.events[1].attributes.push_back(
      {"display_activity", std::string("Packed")});
  custom.events[1].attributes.push_back(
      {"event_time", std::int64_t{43}});
  minera::OcelFlattenOptions custom_options;
  custom_options.object_type = "order";
  custom_options.event_activity_attribute = "display_activity";
  custom_options.event_timestamp_attribute = "event_time";
  const auto custom_flattened =
      minera::flatten_ocel_to_event_log(custom, custom_options);
  assert(custom_flattened.traces[0].events[0].activity == "Created");
  assert(custom_flattened.traces[0].events[0].timestamp == "42");
  assert(custom_flattened.traces[0].events[0].attribute(
             "display_activity") == nullptr);
  assert(custom_flattened.traces[0].events[0].attribute("event_time") ==
         nullptr);
  assert(custom_flattened.traces[0].events[2].activity == "Packed");
  assert(custom_flattened.traces[0].events[2].timestamp == "43");
  assert(custom_flattened.traces[0].events[2].attribute(
             "display_activity") == nullptr);
  assert(custom_flattened.traces[0].events[2].attribute("event_time") ==
         nullptr);

  auto null_selector = custom;
  null_selector.events[0].attributes.push_back(
      {"null_time", std::nullptr_t{}});
  custom_options.event_timestamp_attribute = "null_time";
  threw = false;
  try {
    (void)minera::flatten_ocel_to_event_log(null_selector, custom_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto nonfinite_selector = custom;
  nonfinite_selector.events[0].attributes.push_back(
      {"nonfinite_time", std::numeric_limits<double>::infinity()});
  custom_options.event_timestamp_attribute = "nonfinite_time";
  threw = false;
  try {
    (void)minera::flatten_ocel_to_event_log(nonfinite_selector,
                                            custom_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  custom_options.event_activity_attribute = " ";
  threw = false;
  try {
    (void)minera::flatten_ocel_to_event_log(custom, custom_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

// Captured from pm4py 2.7.22 split_ocel ANCESTORS_DESCENDANTS: objects first
// seen at the same instant are a tie (relation order is not a tie-break), and
// ties are oriented lexically smaller -> larger for the selected type.
void ancestors_descendants_tie_contract() {
  minera::Ocel log;
  log.objects = {{"o1", "order", {}}, {"o2", "order", {}}, {"o3", "order", {}}};
  log.events = {{"e1", "a", "2024-01-01T00:00:00Z", {}},
                {"e2", "a", "2024-01-01T00:00:00Z", {}}};
  log.relations = {{"e1", "o2", ""}, {"e1", "o1", ""},
                   {"e2", "o2", ""}, {"e2", "o3", ""}};
  minera::OcelSplitOptions options;
  options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "order";
  const auto result = minera::split_ocel(log, options);
  assert(result.logs.size() == 3);
  assert((result.central_objects == std::vector<std::string>{"o1", "o2", "o3"}));
  for (const auto& projection : result.logs) {
    assert(projection.objects.size() == 3);
    assert(projection.events.size() == 2);
  }
}

int main() {
  ancestors_descendants_tie_contract();
  connected_components_contract();
  connected_components_single_pass_contract();
  ancestors_descendants_contract();
  validation_contract();
  enrichment_contract();
  object_graph_contract();
  heterogeneous_graph_contract();
  flatten_contract();
}
