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

}  // namespace

int main() {
  connected_components_contract();
  ancestors_descendants_contract();
  validation_contract();
  enrichment_contract();
}
