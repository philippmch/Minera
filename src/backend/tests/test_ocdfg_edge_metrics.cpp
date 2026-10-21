#include "minera/ocdfg_edge_metrics.hpp"

#include <cassert>
#include <iostream>

namespace {
using namespace minera;

template <class Action>
void error_is(Action action, const std::string& type, const std::string& message) {
  bool thrown = false;
  try { action(); }
  catch (const OcdfgEdgeMetricsError& error) {
    thrown = true;
    assert(error.type == type && error.what() == message);
  }
  assert(thrown);
}

#include "ocdfg_edge_metrics_public_vectors.inc"

void practical_association_and_set_projections() {
  OcdfgAssociationTable table;
  table.columns = {"ocel:eid", "ocel:activity", "ocel:oid", "ocel:type"};
  const auto row = [](const char* event, const char* activity, const char* object) {
    return std::map<std::string, OcelValue>{{"ocel:eid", std::string(event)},
        {"ocel:activity", std::string(activity)}, {"ocel:oid", std::string(object)},
        {"ocel:type", std::string("Order")}};
  };
  table.rows = {row("e0", "A", "o0"), row("e0", "A", "o1"),
                row("e1", "B", "o1"), row("e1", "B", "o0"),
                row("e1", "B", "o1"), row("e2", "C", "o0")};
  const auto before_rows = table.rows;
  const auto before_columns = table.columns;
  const auto associations = find_ocdfg_edge_associations(table);
  const OcdfgEdgeAssociations expected = {{"Order", {
      {{"A", "B"}, {{"e0", "e1", "o1"}, {"e0", "e1", "o0"}}},
      {{"B", "B"}, {{"e1", "e1", "o1"}}},
      {{"B", "C"}, {{"e1", "e2", "o0"}}}}}};
  assert(associations == expected);
  auto raw = associations;
  raw["Order"][{"A", "B"}].push_back({"e0", "e1", "o1"});
  raw["empty group"] = {};
  raw["Order"][{"empty", "edge"}] = {};
  const auto before_raw = raw;
  const auto pairs = aggregate_ocdfg_event_couples(raw);
  const auto triples = aggregate_ocdfg_total_objects(raw);
  const auto objects = aggregate_ocdfg_unique_objects(raw);
  assert(pairs.at("Order").at({"A", "B"}).size() == 1);
  assert(triples.at("Order").at({"A", "B"}).size() == 2);
  assert(objects.at("Order").at({"A", "B"}).size() == 2);
  assert(pairs.at("empty group").empty() && triples.at("empty group").empty() && objects.at("empty group").empty());
  assert(pairs.at("Order").at({"empty", "edge"}).empty());
  assert(triples.at("Order").at({"empty", "edge"}).empty());
  assert(objects.at("Order").at({"empty", "edge"}).empty());
  assert(raw == before_raw);
  assert(table.rows == before_rows && table.columns == before_columns);
}

void literal_keys_and_native_rows() {
  OcdfgAssociationTable table;
  table.columns = {"", "object id", "type", "activity"};
  table.rows = {{{"", std::string()}, {"object id", std::string()},
                 {"type", std::string()}, {"activity", std::string()}},
                {{"", std::string(" next ")}, {"object id", std::string()},
                 {"type", std::string()}, {"activity", std::string()}}};
  OcdfgAssociationOptions options;
  options.event_id_attribute = "";
  options.object_id_attribute = "object id";
  options.object_type_attribute = "type";
  options.activity_attribute = "activity";
  options.timestamp_attribute = "not present and inactive";
  const OcdfgEdgeAssociations expected = {{"", {{{"", ""}, {{"", " next ", ""}}}}}};
  assert(find_ocdfg_edge_associations(table, options) == expected);
  Ocel log;
  log.events = {{"", "", "", {}}, {" next ", "", "", {}}};
  log.objects = {{"", "", {}}};
  log.relations = {{"", "", ""}, {" next ", "", ""}};
  const auto before = serialize_ocel_json(log);
  assert(find_ocdfg_edge_associations(log) == expected);
  assert(serialize_ocel_json(log) == before);
  table.rows.clear();
  assert(find_ocdfg_edge_associations(table, options).empty());
}

}  // namespace

int main() {
  public_aggregation_vectors();
  public_association_vectors();
  practical_association_and_set_projections();
  literal_keys_and_native_rows();
  std::cout << "OCDFG edge helpers: all nine retained public outcomes, native adapters, ordered associations and true set projections passed\n";
}
