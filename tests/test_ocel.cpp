#include "minera/ocel.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>

#include "minera/conformance.hpp"

int main() {
  const std::string input =
      R"({"ocel:events":{"e2":{"ocel:activity":"Ship","ocel:timestamp":"2024-01-02T00:00:00Z","ocel:omap":["o1","o2"],"ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":"source"},{"ocel:oid":"o2","ocel:qualifier":"target"}],"ocel:vmap":{"count":2}},"e1":{"ocel:activity":"Create","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]}},"ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{"amount":12.5}},"o2":{"ocel:type":"item","ocel:ovmap":{}}}})";
  auto log = minera::parse_ocel_json(input);
  assert(log.events.size() == 2 && log.objects.size() == 2 &&
         log.relations.size() == 3);
  assert(log.relations[1].qualifier == "source");
  assert(std::get<double>(log.objects[0].attributes[0].value) == 12.5);

  log.o2o.push_back({"o1", "o2", "contains"});
  log.e2e.push_back({"e1", "e2", "precedes"});
  log.object_changes.push_back(
      {"o1", "order", "2024-01-03T00:00:00Z", "status",
       std::string("shipped")});
  log.globals["ocel:global-log.version"] = std::int64_t{2};
  log.globals["ocel:global-event.owner"] = std::string("a\"b\\c");

  const auto etot = minera::discover_etot_classic(log);
  assert(etot.edges.size() == 3);
  assert(etot.edges[0].activity == "Create" &&
         etot.edges[0].object_type == "order" &&
         etot.edges[0].frequency == 1);

  const auto otg = minera::discover_otg_classic(log);
  assert(otg.object_types.size() == 2 && otg.edges.size() == 4);
  bool saw_interaction = false;
  bool saw_descendant = false;
  bool saw_inheritance = false;
  for (const auto& edge : otg.edges) {
    if (edge.relation == "object_interaction") saw_interaction = true;
    if (edge.relation == "object_descendants") saw_descendant = true;
    if (edge.relation == "object_inheritance") saw_inheritance = true;
  }
  assert(saw_interaction && saw_descendant && saw_inheritance);

  const auto round =
      minera::parse_ocel_json(minera::serialize_ocel_json(log));
  assert(round.events.size() == log.events.size() &&
         round.relations.size() == log.relations.size() &&
         round.o2o.size() == log.o2o.size() &&
         round.e2e.size() == log.e2e.size() &&
         round.object_changes.size() == log.object_changes.size() &&
         round.globals == log.globals);
  assert(round.o2o.back().qualifier == "contains");
  assert(round.e2e.front().source_id == "e1" &&
         round.e2e.front().target_id == "e2" &&
         round.e2e.front().qualifier == "precedes");
  assert(std::get<std::string>(round.object_changes.front().value) ==
         "shipped");

  const auto standard =
      minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  assert(standard.find("\"eventTypes\":[{\"name\":\"Create\"") !=
         std::string::npos);
  assert(standard.find("\"time\":\"1970-01-01T00:00:00Z\"") !=
         std::string::npos);
  const auto standard_round = minera::parse_ocel_json(standard);
  assert(standard_round.objects.size() == 2 &&
         standard_round.events.size() == 2 &&
         standard_round.relations.size() == log.relations.size() &&
         standard_round.o2o.size() == log.o2o.size() &&
         standard_round.e2e.size() == log.e2e.size() &&
         standard_round.object_changes.size() == log.object_changes.size() &&
         standard_round.globals == log.globals);

  const std::string standard_fixture = R"json({
    "ocelVersion":"2.0",
    "objectTypes":[{"name":"Order","attributes":[
      {"name":"amount","type":"int"},
      {"name":"status","type":"string"}]}],
    "eventTypes":[{"name":"Create","attributes":[
      {"name":"approved","type":"boolean"},
      {"name":"score","type":"float"}]}],
    "objects":[{"id":"o1","type":"Order","attributes":[
      {"name":"amount","time":"1970-01-01T00:00:00Z","value":"7"},
      {"name":"status","time":"1970-01-01T00:00:00Z","value":"new"},
      {"name":"status","time":"2024-01-02T00:00:00Z","value":"paid"}],
      "relationships":[{"objectId":"o1","qualifier":"self"}]}],
    "events":[{"id":"e1","type":"Create","time":"2024-01-01T00:00:00Z",
      "attributes":[{"name":"approved","value":"true"},{"name":"score","value":"2.5"}],
      "relationships":[{"objectId":"o1","qualifier":"created"}]}],
    "eventRelationships":[{"sourceEventId":"e1","targetEventId":"e1","qualifier":"self"}],
    "globals":{"version":2}
  })json";
  const auto typed_standard = minera::parse_ocel_json(standard_fixture);
  assert(std::get<std::int64_t>(typed_standard.objects.front().attributes[0].value) == 7);
  assert(std::get<bool>(typed_standard.events.front().attributes[0].value));
  assert(std::get<double>(typed_standard.events.front().attributes[1].value) == 2.5);
  assert(typed_standard.object_changes.size() == 1 &&
         typed_standard.object_changes.front().field == "status" &&
         std::get<std::string>(typed_standard.object_changes.front().value) ==
             "paid");
  assert(typed_standard.e2e.size() == 1 &&
         typed_standard.e2e.front().qualifier == "self");

  bool duplicate_type = false;
  try {
    (void)minera::parse_ocel_json(
        R"json({"objectTypes":[{"name":"Order"},{"name":"Order"}],"objects":[],"events":[]})json");
  } catch (...) {
    duplicate_type = true;
  }
  assert(duplicate_type);

  minera::Ocel escaped;
  escaped.objects.push_back(
      {"o\"1", "type\\x", {{"k\"\\", std::string("v\n")}}});
  escaped.events.push_back(
      {"e\"1", "act\\x", "t", {{"a", std::string("b\\c")}}});
  escaped.relations.push_back({"e\"1", "o\"1", "q"});
  const auto escaped_round =
      minera::parse_ocel_json(minera::serialize_ocel_json(escaped));
  assert(escaped_round.objects[0].id == "o\"1" &&
         escaped_round.objects[0].type == "type\\x" &&
         escaped_round.events[0].activity == "act\\x" &&
         escaped_round.relations[0].qualifier == "q");

  bool malformed = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{},"ocel:events":{"e":{}}})");
  } catch (...) {
    malformed = true;
  }
  assert(malformed);

  bool duplicate = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{"o":{"ocel:type":"x"}},"ocel:events":{"e":{"ocel:activity":"a","ocel:timestamp":"t","ocel:omap":["o"]},"e":{"ocel:activity":"a","ocel:timestamp":"t","ocel:omap":["o"]}}})");
  } catch (...) {
    duplicate = true;
  }
  assert(duplicate);

  bool bad_o2o = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{"o":{"ocel:type":"x","ocel:o2o":[{"ocel:oid":"missing"}]}},"ocel:events":{}})");
  } catch (...) {
    bad_o2o = true;
  }
  assert(bad_o2o);
  bool bad_e2e = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{},"ocel:events":{"e":{"ocel:activity":"a","ocel:timestamp":"t","ocel:omap":[]}},"ocel:e2e":[{"ocel:eid":"e","ocel:eid_2":"missing"}]})");
  } catch (...) {
    bad_e2e = true;
  }
  assert(bad_e2e);
  assert(minera::etot_json(etot).find("\"frequency\":1") !=
         std::string::npos);
  assert(minera::otg_json(otg).find("\"object_interaction\"") !=
         std::string::npos);
  const auto ocdfg = minera::discover_ocdfg_classic(log, true);
  assert(ocdfg.activities.size() == 2 && ocdfg.object_types.size() == 2);
  assert(ocdfg.activities_indep.at("Create").events.size() == 1);
  assert(ocdfg.activities_ot.at("order").at("Ship").unique_objects.size() == 1);
  assert(ocdfg.start_activities.at("order").at("Create").events.size() == 1);
  assert(ocdfg.end_activities.at("order").at("Ship").events.size() == 1);
  assert(ocdfg.edges.at("order").at({"Create", "Ship"}).event_couples.size() == 1);
  const auto ocdfg_json = minera::serialize_ocdfg_json(ocdfg);
  assert(ocdfg_json.find("activities_indep") != std::string::npos);
  assert(ocdfg_json.find("activities_ot") != std::string::npos);
  assert(ocdfg_json.find("start_activities") != std::string::npos);
  assert(ocdfg_json.find("end_activities") != std::string::npos);
  assert(ocdfg_json.find("event_objects") != std::string::npos);
  assert(ocdfg_json.find("performance_computed") != std::string::npos);
  assert(!minera::discover_ocdfg_classic(log, false).performance_computed);

  const minera::EtotGraph etot_norm{
      {"Create", "Ship"}, {"order"}, {{"Create", "order", 10}, {"Ship", "order", 5}}};
  const minera::EtotGraph etot_equal{
      {"Create", "Ship"}, {"order"}, {{"Create", "order", 11}, {"Ship", "order", 5}}};
  const auto etot_at_threshold = minera::compare_etot(etot_equal, etot_norm);
  assert(etot_at_threshold.missing_edges.empty());
  assert(etot_at_threshold.relative_frequency_delta.size() == 2);
  assert(etot_at_threshold.fitness == 1.0);
  const minera::EtotGraph etot_extra{
      {"Create", "Ship", "Cancel"}, {"order"},
      {{"Create", "order", 10}, {"Ship", "order", 5}, {"Cancel", "order", 1}}};
  assert(minera::compare_etot(etot_extra, etot_norm).fitness == 1.0);
  const minera::EtotGraph etot_missing{
      {"Create"}, {"order"}, {{"Create", "order", 10}}};
  const auto etot_bad = minera::compare_etot(etot_missing, etot_norm);
  assert(etot_bad.missing_activities.size() == 1 &&
         etot_bad.missing_edges.size() == 1 && etot_bad.fitness < 1.0);
  assert(minera::etot_conformance_json(etot_bad).find("relative_frequency_delta") !=
         std::string::npos);

  const minera::OtgGraph otg_norm{
      {"order", "item"},
      {{"order", "object_interaction", "item", 10},
       {"order", "object_descendants", "item", 10}}};
  const minera::OtgGraph otg_equal{
      {"order", "item"},
      {{"order", "object_interaction", "item", 12},
       {"order", "object_descendants", "item", 12}}};
  minera::OtgConformanceOptions otg_options;
  otg_options.theta_by_relation["object_interaction"] = 0.1;
  otg_options.theta_by_relation["object_descendants"] = 0.3;
  const auto otg_bad = minera::compare_otg(otg_equal, otg_norm, otg_options);
  assert(otg_bad.non_conforming_edges.size() == 1);
  assert(std::get<1>(otg_bad.non_conforming_edges.begin()->first) ==
         "object_interaction");
  assert(otg_bad.fitness < 1.0);
  minera::OtgConformanceOptions otg_scalar;
  otg_scalar.theta = 0.3;
  assert(minera::compare_otg(otg_equal, otg_norm, otg_scalar)
             .non_conforming_edges.empty());
  assert(minera::otg_conformance_json(otg_bad).find("non_conforming_edges") !=
         std::string::npos);

  minera::OcdfgGraph ocdfg_norm;
  ocdfg_norm.activities = {"Create", "Ship"};
  ocdfg_norm.object_types = {"order", "item"};
  ocdfg_norm.activities_indep["Create"].events = {"e1", "e2"};
  ocdfg_norm.activities_indep["Ship"].events = {"e3"};
  ocdfg_norm.edges["order"][{"Create", "Ship"}].event_couples = {
      {"e1", "e3"}};
  ocdfg_norm.edges["item"][{"Create", "Ship"}].event_couples = {
      {"e2", "e4"}};
  minera::OcdfgGraph ocdfg_real = ocdfg_norm;
  ocdfg_real.edges["item"].clear();
  ocdfg_real.activities = {"Create"};
  ocdfg_real.activities_indep["Ship"].events.clear();
  const auto ocdfg_bad = minera::compare_ocdfg(ocdfg_real, ocdfg_norm);
  assert(ocdfg_bad.missing_activities.size() == 1 &&
         ocdfg_bad.missing_flows.size() == 0);
  assert(ocdfg_bad.flow_measure_differences.at({"Create", "Ship"}) == 1);
  assert(ocdfg_bad.fitness < 1.0);
  minera::OcdfgConformanceOptions ocdfg_threshold;
  ocdfg_threshold.theta_activity = 1.0;
  ocdfg_threshold.theta_flow = 1.0;
  const auto ocdfg_at_threshold =
      minera::compare_ocdfg(ocdfg_real, ocdfg_norm, ocdfg_threshold);
  assert(ocdfg_at_threshold.non_conforming_activities_in_measure.empty());
  assert(ocdfg_at_threshold.non_conforming_flows_in_measure.empty());
  assert(minera::ocdfg_conformance_json(ocdfg_bad).find("activity_measure_differences") !=
         std::string::npos);

  minera::OcpnDiscoveryOptions ocpn_options;
  ocpn_options.double_arc_threshold = 0.8;
  const auto ocpn = minera::discover_ocpn(log, ocpn_options);
  assert(ocpn.object_types.size() == 2 && ocpn.petri_nets.size() == 2);
  assert(ocpn.petri_nets.at("order").activities().size() >= 1);
  assert(ocpn.double_arcs_on_activity.at("order").at("Create") == false);
  assert(ocpn.annotations.activities_indep.at("Create").events.size() == 1);
  assert(ocpn.annotations.activities_ot.at("order").at("Ship")
             .unique_objects.size() == 1);
  assert(ocpn.annotations.start_activities.at("order").at("Create")
             .events.size() == 1);
  assert(ocpn.annotations.end_activities.at("order").at("Ship")
             .events.size() == 1);
  assert(ocpn.annotations.edges.at("order").at({"Create", "Ship"})
             .event_couples.size() == 1);
  assert(ocpn.tbr_results.empty());
  const auto ocpn_json = minera::serialize_ocpn_json(ocpn);
  assert(ocpn_json.find("double_arcs_on_activity") != std::string::npos);
  assert(ocpn_json.find("activity_object_types") != std::string::npos);
  assert(ocpn_json.find("activity_variable_object_types") !=
         std::string::npos);
  assert(ocpn_json.find("\"annotations\"") != std::string::npos);
  assert(ocpn_json.find("\"event_pairs\"") != std::string::npos);
  assert(ocpn_json.find("\"total_objects\":[[") != std::string::npos);
  assert(ocpn_json.find("\"tbr_results\":{}") != std::string::npos);
  ocpn_options.diagnostics_with_token_based_replay = true;
  const auto ocpn_with_tbr = minera::discover_ocpn(log, ocpn_options);
  assert(ocpn_with_tbr.tbr_results.size() == 2);
  for (const auto& [type, diagnostics] : ocpn_with_tbr.tbr_results) {
    (void)type;
    assert(diagnostics.enabled && diagnostics.trace_count == 1 &&
           diagnostics.fitting_traces == 1 &&
           diagnostics.places.size() ==
               ocpn_with_tbr.petri_nets.at(type).place_count() &&
           diagnostics.transition_activations.size() ==
               ocpn_with_tbr.petri_nets.at(type).transitions().size());
    assert(std::any_of(diagnostics.transition_activations.begin(),
                       diagnostics.transition_activations.end(),
                       [](std::uint64_t count) { return count != 0; }));
  }
  const auto ocpn_with_tbr_json = minera::serialize_ocpn_json(ocpn_with_tbr);
  assert(ocpn_with_tbr_json.find("\"tbr_results\":{\"item\"") !=
         std::string::npos);
  assert(ocpn_with_tbr_json.find("\"places\":[{\"m\":") !=
         std::string::npos);
  assert(ocpn_with_tbr_json ==
         minera::serialize_ocpn_json(ocpn_with_tbr));
  auto variable_log = log;
  variable_log.objects.push_back({"o3", "item", {}});
  variable_log.relations.push_back({"e2", "o3", "target"});
  const auto variable_ocpn = minera::discover_ocpn(variable_log);
  assert(variable_ocpn.activity_object_types.at("Ship") ==
         std::vector<std::string>({"order"}));
  assert(variable_ocpn.activity_variable_object_types.at("Ship") ==
         std::vector<std::string>({"item"}));
  assert(minera::serialize_ocpn_json(variable_ocpn).find(
             "\"activity_variable_object_types\":{\"Ship\":[\"item\"]}") !=
         std::string::npos);
  ocpn_options.variant = minera::OcpnInductiveVariant::kDfg;
  const auto ocpn_dfg = minera::discover_ocpn(log, ocpn_options);
  assert(ocpn_dfg.petri_nets.size() == 2);
  ocpn_options.annotated = false;
  const auto ocpn_plain = minera::discover_ocpn(log, ocpn_options);
  assert(!ocpn_plain.annotated && ocpn_plain.annotations.activities.empty());
  assert(ocpn_plain.tbr_results.at("order").enabled &&
         ocpn_plain.tbr_results.at("item").enabled);
  assert(minera::serialize_ocpn_json(ocpn_plain).find("\"annotations\":null") !=
         std::string::npos);

  minera::PetriNet replay_net;
  const auto replay_source = replay_net.add_place();
  const auto replay_middle = replay_net.add_place();
  const auto replay_sink = replay_net.add_place();
  replay_net.set_source(replay_source);
  replay_net.set_sink(replay_sink);
  replay_net.set_activities({"A", "B"});
  replay_net.add_transition(0, {replay_source}, {replay_middle});
  replay_net.add_transition(1, {replay_middle}, {replay_sink});
  minera::EventLog replay_log;
  replay_log.traces.push_back({"deviating", {{"", "B", "", "", 0, {}, {}}}, {}});
  const auto replay_diagnostics =
      minera::replay_token_diagnostics(replay_log, replay_net);
  assert(replay_diagnostics.trace_count == 1 &&
         replay_diagnostics.fitting_traces == 0 &&
         replay_diagnostics.transition_activations.size() == 2);
  std::uint64_t total_missing = 0;
  for (const auto& place : replay_diagnostics.places)
    total_missing += place.missing;
  assert(total_missing > 0 && replay_diagnostics.transition_activations[1] == 1);

  const auto tuple_description = minera::describe_ocel_tuples(log);
  assert(tuple_description.includes_timestamps);
  assert(tuple_description.events.size() == 2 && tuple_description.objects.size() == 2);
  assert(tuple_description.events[0].activity == "Create");
  assert(tuple_description.events[0].related_objects.size() == 1 &&
         tuple_description.events[0].related_objects[0] == "o1");
  assert(tuple_description.events[0].timestamp == "2024-01-01T00:00:00Z");
  assert(tuple_description.objects[0].id == "o1" &&
         tuple_description.objects[0].has_lifecycle &&
         tuple_description.objects[0].lifecycle_start == "2024-01-01T00:00:00Z" &&
         tuple_description.objects[0].lifecycle_end == "2024-01-02T00:00:00Z" &&
         tuple_description.objects[0].lifecycle_seconds == 86400.0);
  assert(minera::describe_ocel_text(log).find("events:") != std::string::npos);
  const auto tuple_json = minera::serialize_ocel_description_json(tuple_description);
  assert(tuple_json.find("includes_timestamps") != std::string::npos &&
         tuple_json.find("lifecycle_seconds") != std::string::npos);
  minera::OcelDescriptionOptions no_timestamps;
  no_timestamps.include_timestamps = false;
  const auto without_timestamps = minera::describe_ocel_tuples(log, no_timestamps);
  assert(!without_timestamps.includes_timestamps &&
         without_timestamps.events[0].timestamp.empty());
  assert(minera::serialize_ocel_description_json(without_timestamps).find(
             "\"timestamp\"") == std::string::npos);
  const auto variant1 = minera::serialize_ocel_description_json(
      log, minera::OcelDescriptionVariant::kVariant1);
  assert(variant1.find("\"family\":\"OCEL_DESCRIPTION\"") !=
         std::string::npos);
  assert(variant1.find("\"variant\":\"VARIANT1\"") !=
         std::string::npos);
  assert(variant1.find("\"text\":") != std::string::npos);
  const auto variant2 = minera::serialize_ocel_description_json(
      log, minera::OcelDescriptionVariant::kVariant2);
  assert(variant2.find("\"variant\":\"VARIANT2\"") !=
         std::string::npos);
  assert(variant2.find("\"events\":[[\"Create\",\"o1\",") !=
         std::string::npos);
  assert(variant2.find("\"objects\":[[") != std::string::npos);

  minera::Ocel catalog;
  catalog.event_type_attributes["EmptyEvent"] = {{"planned_at", "datetime"}};
  catalog.event_type_attributes["Create"] = {{"count", "long"}};
  catalog.object_type_attributes["EmptyObject"] = {{"label", "string"}};
  catalog.object_type_attributes["Order"] = {{"amount", "decimal"}};
  catalog.events.push_back({"ce1", "Create", "2024-01-01T00:00:00Z",
                            {{"count", std::int64_t(3)}}});
  catalog.objects.push_back({"co1", "Order", {}});
  const auto catalog_json =
      minera::serialize_ocel_json(catalog, minera::OcelJsonFormat::kStandard);
  assert(catalog_json.find("EmptyEvent") != std::string::npos &&
         catalog_json.find("EmptyObject") != std::string::npos);
  const auto catalog_round = minera::parse_ocel_json(catalog_json);
  assert(catalog_round.event_type_attributes.at("EmptyEvent").at("planned_at") ==
         "datetime");
  assert(catalog_round.event_type_attributes.at("Create").at("count") ==
         "long");
  assert(catalog_round.object_type_attributes.at("EmptyObject").at("label") ==
         "string");
  assert(catalog_round.object_type_attributes.at("Order").at("amount") ==
         "decimal");
  return 0;
}
