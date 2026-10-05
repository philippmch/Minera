#include "minera/ocel.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/conformance.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/saw_nets.hpp"

namespace {

void variant1_text_contract(const minera::Ocel& log) {
  const auto text = minera::describe_ocel_text(log);
  assert(text.find("\n\nevents:\n") != std::string::npos);
  assert(text.find("Create ( related objects: o1 )  timestamp: "
                   "2024-01-01T00:00:00Z") != std::string::npos);
  assert(text.find("Ship ( related objects: o1, o2 )  timestamp: "
                   "2024-01-02T00:00:00Z") != std::string::npos);
  assert(text.find("\n\nobjects:\n") != std::string::npos);
  assert(text.find("o1 object type: order ( lifecycle start: "
                   "2024-01-01T00:00:00Z ; lifecycle end: "
                   "2024-01-02T00:00:00Z ; lifecycle duration: 86400") !=
         std::string::npos);
  assert(text.find("o2 object type: item") != std::string::npos);

  const auto variant1 = minera::serialize_ocel_description_json(
      log, minera::OcelDescriptionVariant::kVariant1);
  assert(variant1.find("\"family\":\"OCEL_DESCRIPTION\"") !=
         std::string::npos);
  assert(variant1.find("\"variant\":\"VARIANT1\"") !=
         std::string::npos);
  assert(variant1.find("\"text\":") != std::string::npos);
  assert(variant1.find("\"event_count\":2") != std::string::npos &&
         variant1.find("\"object_count\":2") != std::string::npos);
}

void variant2_tuple_contract(const minera::Ocel& log) {
  const auto tuple_description = minera::describe_ocel_tuples(log);
  assert(tuple_description.includes_timestamps);
  assert(tuple_description.events.size() == 2 &&
         tuple_description.objects.size() == 2);
  assert(tuple_description.events[0].activity == "Create");
  assert(tuple_description.events[0].related_objects.size() == 1 &&
         tuple_description.events[0].related_objects[0] == "o1");
  assert(tuple_description.events[0].timestamp == "2024-01-01T00:00:00Z");
  assert(tuple_description.events[1].activity == "Ship");
  assert(tuple_description.events[1].related_objects.size() == 2 &&
         tuple_description.events[1].related_objects[0] == "o1" &&
         tuple_description.events[1].related_objects[1] == "o2");
  assert(tuple_description.objects[0].id == "o1" &&
         tuple_description.objects[0].has_lifecycle &&
         tuple_description.objects[0].lifecycle_start ==
             "2024-01-01T00:00:00Z" &&
         tuple_description.objects[0].lifecycle_end ==
             "2024-01-02T00:00:00Z" &&
         tuple_description.objects[0].lifecycle_seconds == 86400.0);
  const auto tuple_json =
      minera::serialize_ocel_description_json(tuple_description);
  assert(tuple_json.find("includes_timestamps") != std::string::npos &&
         tuple_json.find("lifecycle_seconds") != std::string::npos);

  minera::OcelDescriptionOptions no_timestamps;
  no_timestamps.include_timestamps = false;
  const auto without_timestamps =
      minera::describe_ocel_tuples(log, no_timestamps);
  assert(!without_timestamps.includes_timestamps &&
         without_timestamps.events[0].timestamp.empty());
  assert(minera::serialize_ocel_description_json(without_timestamps).find(
             "\"timestamp\"") == std::string::npos);

  const auto variant2 = minera::serialize_ocel_description_json(
      log, minera::OcelDescriptionVariant::kVariant2);
  assert(variant2.find("\"variant\":\"VARIANT2\"") !=
         std::string::npos);
  assert(variant2.find("\"events\":[[\"Create\",\"o1\",") !=
         std::string::npos);
  assert(variant2.find("\"event_object_counts\":[1,2]") !=
         std::string::npos);
  assert(variant2.find("\"objects\":[[") != std::string::npos);
}

}  // namespace

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

  const auto repeated_epoch = minera::parse_ocel_json(R"json({
    "objectTypes":[{"name":"Order","attributes":[{"name":"state","type":"string"}]}],
    "objects":[{"id":"o1","type":"Order","attributes":[
      {"name":"state","time":"1970-01-01T00:00:00Z","value":"new"},
      {"name":"state","time":"1970-01-01T00:00:00Z","value":"reopened"},
      {"name":"state","time":"0","value":"queued"}]}],
    "events":[]
  })json");
  assert(repeated_epoch.objects.front().attributes.size() == 1);
  assert(repeated_epoch.object_changes.size() == 2);
  assert(repeated_epoch.object_changes[0].timestamp ==
         "1970-01-01T00:00:00Z");
  assert(repeated_epoch.object_changes[1].timestamp == "0");
  assert(std::get<std::string>(repeated_epoch.object_changes[1].value) ==
         "queued");
  bool missing_change_time = false;
  try {
    (void)minera::parse_ocel_json(R"json({
      "objects":[{"id":"o1","type":"Order","attributes":[
        {"name":"state","value":"new"},
        {"name":"state","value":"queued"}]}],
      "events":[]
    })json");
  } catch (...) {
    missing_change_time = true;
  }
  assert(missing_change_time);

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
  const auto etot_round = minera::parse_etot_json(minera::etot_json(etot));
  assert(minera::etot_json(etot_round) == minera::etot_json(etot));
  assert(minera::otg_json(otg).find("\"object_interaction\"") !=
         std::string::npos);
  const auto otg_round = minera::parse_otg_json(minera::otg_json(otg));
  assert(minera::otg_json(otg_round) == minera::otg_json(otg));
  const auto ocdfg = minera::discover_ocdfg_classic(log, true);
  assert(ocdfg.activities.size() == 2 && ocdfg.object_types.size() == 2);
  assert(ocdfg.activities_indep.at("Create").events.size() == 1);
  const auto legacy_braced_ocdfg = minera::discover_ocdfg_classic(log, {});
  assert(legacy_braced_ocdfg.activities == ocdfg.activities);
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
  assert(ocdfg_json.find("total_objects") != std::string::npos);
  assert(ocdfg_json.find("performance_event_couples") != std::string::npos);
  assert(ocdfg_json.find("performance_total_objects") != std::string::npos);
  assert(ocdfg_json.find("performance_computed") != std::string::npos);
  assert(!minera::discover_ocdfg_classic(log, false).performance_computed);
  const auto ocdfg_round = minera::parse_ocdfg_json(ocdfg_json);
  assert(minera::serialize_ocdfg_json(ocdfg_round) == ocdfg_json);
  bool bad_graph = false;
  try {
    (void)minera::parse_etot_json(
        R"({"activities":[],"object_types":[],"edges":[{"activity":"A","object_type":"T","frequency":-1}]})");
  } catch (...) {
    bad_graph = true;
  }
  assert(bad_graph);
  bad_graph = false;
  try {
    (void)minera::parse_ocdfg_json(R"({"activities":[],"object_types":[]})");
  } catch (...) {
    bad_graph = true;
  }
  assert(bad_graph);
  bad_graph = false;
  try {
    (void)minera::parse_ocdfg_json(
        R"({"activities":[],"object_types":[],"edges":{"T":{"A->B":{"source_activity":"A","target_activity":"B","event_couples":[["e"]]}}}})");
  } catch (...) {
    bad_graph = true;
  }
  assert(bad_graph);
  bad_graph = false;
  try {
    (void)minera::parse_otg_json(
        R"({"activities":[],"object_types":[],"edges":[]})");
  } catch (...) {
    bad_graph = true;
  }
  assert(bad_graph);
  bad_graph = false;
  try {
    (void)minera::parse_etot_json(
        R"({"activities":[],"object_types":[],"activities_indep":{},"edges":{}})");
  } catch (...) {
    bad_graph = true;
  }
  assert(bad_graph);

  // Associations are keyed by event/object identity, not relation qualifier.
  // The event-couple performance projection is unique per pair, while the
  // total-object projection retains one sample for each participating object.
  minera::Ocel metric_log;
  metric_log.events = {
      {"e1", "A", "2024-01-01T00:00:00Z", {}},
      {"e2", "B", "2024-01-01T01:00:00Z", {}},
      {"e3", "C", "2024-01-01T03:00:00Z", {}},
  };
  metric_log.objects = {
      {"o1", "item", {}}, {"o2", "item", {}}, {"o3", "other", {}},
  };
  metric_log.relations = {
      {"e1", "o1", "source"}, {"e1", "o2", "source"},
      {"e1", "o2", "duplicate-qualifier"}, {"e2", "o1", "target"},
      {"e2", "o2", "target"}, {"e3", "o2", "target"},
  };
  const auto metric_graph = minera::discover_ocdfg_classic(metric_log, true);
  const auto& activity_metric = metric_graph.activities_indep.at("A");
  assert(activity_metric.events == std::vector<std::string>({"e1"}));
  assert(activity_metric.unique_objects ==
         std::vector<std::string>({"o1", "o2"}));
  assert(activity_metric.event_objects == activity_metric.total_objects);
  const auto& metric_edge = metric_graph.edges.at("item").at({"A", "B"});
  const std::vector<std::pair<std::string, std::string>> expected_metric_couples{
      {"e1", "e2"}};
  assert(metric_edge.event_couples == expected_metric_couples);
  assert(metric_edge.event_objects == metric_edge.total_objects);
  assert(metric_edge.total_objects.size() == 2);
  assert(metric_edge.performance_event_couples.size() == 1);
  assert(metric_edge.performance_total_objects.size() == 2);
  assert(metric_edge.performance == metric_edge.performance_total_objects);
  assert(metric_edge.performance_event_couples.front() == 3600.0);
  const auto metric_without_performance =
      minera::discover_ocdfg_classic(metric_log, false);
  const auto& no_perf_edge =
      metric_without_performance.edges.at("item").at({"A", "B"});
  assert(no_perf_edge.performance.empty() &&
         no_perf_edge.performance_event_couples.empty() &&
         no_perf_edge.performance_total_objects.empty());
  assert(minera::serialize_ocdfg_json(metric_graph).find(
             "\"total_objects\":[[\"e1\",\"o1\"]") !=
         std::string::npos);

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
  const auto etot_direct = minera::compare_etot(log, minera::discover_etot_classic(log));
  assert(etot_direct.fitness == 1.0);
  // Undefined ETOT normalization and zero normative edge frequencies must
  // remain errors rather than reporting a perfect comparison.
  const auto expect_division_by_zero = [](const auto& compare) {
    bool rejected = false;
    try {
      compare();
    } catch (const std::domain_error& error) {
      rejected = std::string(error.what()) == "division by zero";
    }
    assert(rejected);
  };
  expect_division_by_zero([] { (void)minera::compare_etot(
      minera::EtotGraph{}, minera::EtotGraph{}); });
  const minera::EtotGraph zero_frequency_etot{
      {"A"}, {"T"}, {{"A", "T", 0}}};
  expect_division_by_zero([&] { (void)minera::compare_etot(
      zero_frequency_etot, zero_frequency_etot); });
  minera::EtotConformanceOptions zero_weights_etot;
  zero_weights_etot.alpha = zero_weights_etot.beta = zero_weights_etot.gamma = 0;
  expect_division_by_zero([&] { (void)minera::compare_etot(
      etot_equal, etot_norm, zero_weights_etot); });

  // Signed weights and negative/NaN/infinite thresholds are accepted by
  // the comparator. IEEE comparisons determine whether a delta is flagged.
  auto unrestricted_etot = minera::EtotConformanceOptions{};
  unrestricted_etot.alpha = -1;
  unrestricted_etot.theta_relative = std::numeric_limits<double>::quiet_NaN();
  assert(minera::compare_etot(etot_equal, etot_norm, unrestricted_etot).fitness == 1);
  unrestricted_etot.theta_relative = -1;
  assert(minera::compare_etot(etot_equal, etot_norm, unrestricted_etot).fitness == -1);
  unrestricted_etot.theta_relative = std::numeric_limits<double>::infinity();
  assert(minera::compare_etot(etot_equal, etot_norm, unrestricted_etot).fitness == 1);

  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t wide_count = std::uint64_t{1} << 53;
    const minera::EtotGraph wide_real{{"A"}, {"T"}, {{"A", "T", wide_count}}};
    const minera::EtotGraph wide_norm{{"A"}, {"T"}, {{"A", "T", wide_count + 1}}};
    minera::EtotConformanceOptions exact_threshold;
    exact_threshold.theta_relative = 0;
    const auto wide_result = minera::compare_etot(wide_real, wide_norm, exact_threshold);
    assert(wide_result.relative_frequency_delta.at({"A", "T"}) == 0x1.fffffffffffffp-54);
    assert(wide_result.fitness == 0.75);
    auto maximum_real = wide_real;
    maximum_real.edges.front().frequency = std::numeric_limits<std::size_t>::max();
    auto one_norm = wide_norm;
    one_norm.edges.front().frequency = 1;
    assert(minera::compare_etot(maximum_real, one_norm)
               .relative_frequency_delta.at({"A", "T"}) == 0x1p64);
    // Serialized graph counters retain all unsigned bits across the wire.
    for (const auto count : {std::uint64_t{1} << 63,
                             (std::uint64_t{1} << 63) + 1,
                             std::numeric_limits<std::uint64_t>::max()}) {
      maximum_real.edges.front().frequency = count;
      const auto json = minera::etot_json(maximum_real);
      const auto round_trip = minera::parse_etot_json(json);
      assert(round_trip.edges.front().frequency == count);
      assert(minera::etot_json(round_trip) == json);
    }
    bool oversized_count = false;
    try {
      (void)minera::parse_etot_json(
          R"({"activities":["A"],"object_types":["T"],"edges":[{"activity":"A","object_type":"T","frequency":18446744073709551616}]})");
    } catch (const std::invalid_argument&) {
      oversized_count = true;
    }
    assert(oversized_count);
    bool ocel_scalar_unchanged = false;
    try {
      (void)minera::parse_ocel_json(
          R"({"ocel:objects":{},"ocel:events":{},"ocel:global-log":{"counter":18446744073709551615}})");
    } catch (const std::invalid_argument&) {
      ocel_scalar_unchanged = true;
    }
    assert(ocel_scalar_unchanged);
  }
  assert(etot_direct.missing_edges.empty() &&
         etot_direct.additional_edges.empty());

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
  const auto otg_direct = minera::compare_otg(log, minera::discover_otg_classic(log));
  assert(otg_direct.fitness == 1.0);
  expect_division_by_zero([] { (void)minera::compare_otg(
      minera::OtgGraph{}, minera::OtgGraph{}); });
  const minera::OtgGraph zero_frequency_otg{
      {"T", "U"}, {{"T", "object_interaction", "U", 0}}};
  const auto zero_otg_result = minera::compare_otg(zero_frequency_otg, zero_frequency_otg);
  assert(zero_otg_result.fitness == 0.75);
  assert(std::isinf(zero_otg_result.non_conforming_edges.at(
      {"T", "object_interaction", "U"})));
  auto unrestricted_otg = minera::OtgConformanceOptions{};
  unrestricted_otg.theta_by_relation["object_interaction"] =
      std::numeric_limits<double>::quiet_NaN();
  assert(minera::compare_otg(zero_frequency_otg, zero_frequency_otg, unrestricted_otg)
             .non_conforming_edges.empty());
  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t wide_count = std::uint64_t{1} << 53;
    const minera::OtgGraph wide_real{{"T", "U"}, {{"T", "object_interaction", "U", wide_count}}};
    const minera::OtgGraph wide_norm{{"T", "U"}, {{"T", "object_interaction", "U", wide_count + 1}}};
    minera::OtgConformanceOptions exact_threshold;
    exact_threshold.theta = 0;
    const auto wide_result = minera::compare_otg(wide_real, wide_norm, exact_threshold);
    assert(wide_result.non_conforming_edges.at({"T", "object_interaction", "U"}) ==
           0x1.fffffffffffffp-54);
    assert(wide_result.fitness == 0.75);
    auto maximum_graph = wide_norm;
    maximum_graph.edges.front().frequency = std::numeric_limits<std::size_t>::max();
    const auto json = minera::otg_json(maximum_graph);
    const auto round_trip = minera::parse_otg_json(json);
    assert(round_trip.edges.front().frequency == maximum_graph.edges.front().frequency);
    assert(minera::otg_json(round_trip) == json);
  }
  assert(otg_direct.missing_edges.empty() && otg_direct.additional_edges.empty());

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
  const auto ocdfg_direct =
      minera::compare_ocdfg(log, minera::discover_ocdfg_classic(log, false));
  assert(ocdfg_direct.fitness == 1.0);
  assert(ocdfg_direct.missing_activities.empty() &&
         ocdfg_direct.missing_flows.empty());
  minera::OcdfgConformanceOptions unrestricted_ocdfg;
  unrestricted_ocdfg.theta_activity = -1;
  unrestricted_ocdfg.theta_flow = std::numeric_limits<double>::quiet_NaN();
  const auto signed_ocdfg = minera::compare_ocdfg(ocdfg_norm, ocdfg_norm, unrestricted_ocdfg);
  assert(signed_ocdfg.non_conforming_activities_in_measure.size() == 2);
  assert(signed_ocdfg.non_conforming_flows_in_measure.empty());
  unrestricted_ocdfg.alpha = unrestricted_ocdfg.beta =
      unrestricted_ocdfg.gamma = unrestricted_ocdfg.delta = 0;
  assert(minera::compare_ocdfg(ocdfg_norm, ocdfg_norm, unrestricted_ocdfg).fitness == 1);

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

  // OCPN discovery can project labels from typed event/object attributes while
  // retaining the canonical IDs and relation graph. The source fields are
  // deliberately different so the selector, rather than fallback behavior,
  // determines every model family label.
  auto selected_log = log;
  selected_log.events[0].activity = "legacy-create";
  selected_log.events[0].attributes.push_back(
      {"selected_activity", std::string("Assemble")});
  selected_log.events[1].activity = "legacy-ship";
  selected_log.events[1].attributes.push_back(
      {"selected_activity", std::string("Deliver")});
  selected_log.objects[0].type = "legacy-order";
  selected_log.objects[0].attributes.push_back(
      {"selected_type", std::string("order-main")});
  selected_log.objects[1].type = "legacy-item";
  selected_log.objects[1].attributes.push_back(
      {"selected_type", std::string("item-main")});
  minera::OcpnDiscoveryOptions selected_options;
  selected_options.activity_attribute = "selected_activity";
  selected_options.object_type_attribute = "selected_type";
  const auto selected_ocpn =
      minera::discover_ocpn(selected_log, selected_options);
  assert(selected_ocpn.activities ==
         std::vector<std::string>({"Assemble", "Deliver"}));
  assert(selected_ocpn.object_types ==
         std::vector<std::string>({"item-main", "order-main"}));
  assert(selected_ocpn.petri_nets.contains("order-main") &&
         selected_ocpn.petri_nets.contains("item-main"));
  assert(selected_ocpn.annotations.activities_indep.at("Assemble").events ==
         std::vector<std::string>({"e1"}));
  const auto selected_causal =
      minera::discover_oc_causal_net(selected_log, selected_options);
  assert(selected_causal.workflow_projection.activities ==
         selected_ocpn.activities);
  assert(std::any_of(
      selected_causal.activities.begin(), selected_causal.activities.end(),
      [](const auto& activity) { return activity.activity == "Assemble"; }));

  // Scalar numeric selectors are rendered with a stable textual spelling.
  auto numeric_log = selected_log;
  numeric_log.events[0].attributes.push_back(
      {"numeric_activity", std::int64_t{7}});
  numeric_log.events[1].attributes.push_back(
      {"numeric_activity", std::int64_t{8}});
  numeric_log.objects[0].attributes.push_back(
      {"numeric_type", std::int64_t{3}});
  numeric_log.objects[1].attributes.push_back(
      {"numeric_type", std::int64_t{4}});
  minera::OcpnDiscoveryOptions numeric_options;
  numeric_options.activity_attribute = "numeric_activity";
  numeric_options.object_type_attribute = "numeric_type";
  const auto numeric_ocpn =
      minera::discover_ocpn(numeric_log, numeric_options);
  assert(numeric_ocpn.activities ==
         std::vector<std::string>({"7", "8"}));
  assert(numeric_ocpn.object_types ==
         std::vector<std::string>({"3", "4"}));

  bool missing_selector_rejected = false;
  try {
    auto invalid_options = selected_options;
    invalid_options.activity_attribute = "not_present";
    (void)minera::discover_ocpn(selected_log, invalid_options);
  } catch (const std::invalid_argument&) {
    missing_selector_rejected = true;
  }
  assert(missing_selector_rejected);

  minera::OcdfgDiscoveryOptions ocdfg_selected_options;
  ocdfg_selected_options.activity_attribute = "selected_activity";
  ocdfg_selected_options.object_type_attribute = "selected_type";
  const auto selected_ocdfg =
      minera::discover_ocdfg_classic_with_options(selected_log,
                                                  ocdfg_selected_options);
  assert(selected_ocdfg.activities ==
         std::vector<std::string>({"Assemble", "Deliver"}));
  assert(selected_ocdfg.object_types ==
         std::vector<std::string>({"item-main", "order-main"}));
  assert(selected_ocdfg.edges.at("order-main").at({"Assemble", "Deliver"})
             .performance_event_couples.size() == 1);
  const auto unselected_conformance =
      minera::compare_ocdfg(selected_log, selected_ocdfg);
  assert(unselected_conformance.fitness < 1.0);
  minera::OcdfgConformanceOptions selected_conformance_options;
  selected_conformance_options.activity_attribute = "selected_activity";
  selected_conformance_options.object_type_attribute = "selected_type";
  const auto selected_conformance = minera::compare_ocdfg(
      selected_log, selected_ocdfg, selected_conformance_options);
  assert(selected_conformance.fitness == 1.0);
  assert(selected_conformance.missing_activities.empty() &&
         selected_conformance.additional_activities.empty() &&
         selected_conformance.missing_flows.empty() &&
         selected_conformance.additional_flows.empty());
  bool conformance_missing_selector_rejected = false;
  try {
    auto invalid = selected_conformance_options;
    invalid.object_type_attribute = "missing_type";
    (void)minera::compare_ocdfg(selected_log, selected_ocdfg, invalid);
  } catch (const std::invalid_argument&) {
    conformance_missing_selector_rejected = true;
  }
  assert(conformance_missing_selector_rejected);
  ocdfg_selected_options.compute_edges_performance = false;
  const auto selected_ocdfg_plain =
      minera::discover_ocdfg_classic_with_options(selected_log,
                                                  ocdfg_selected_options);
  assert(!selected_ocdfg_plain.performance_computed);
  assert(selected_ocdfg_plain.activities == selected_ocdfg.activities);
  assert(selected_ocdfg_plain.edges.at("order-main")
             .at({"Assemble", "Deliver"})
             .performance_event_couples.empty());
  bool ocdfg_missing_selector_rejected = false;
  try {
    auto invalid = ocdfg_selected_options;
    invalid.object_type_attribute = "missing_type";
    (void)minera::discover_ocdfg_classic_with_options(selected_log, invalid);
  } catch (const std::invalid_argument&) {
    ocdfg_missing_selector_rejected = true;
  }
  assert(ocdfg_missing_selector_rejected);

  minera::SawNetDiscoveryOptions saw_selected_options;
  saw_selected_options.activity_attribute = "selected_activity";
  saw_selected_options.object_type_attribute = "selected_type";
  const auto selected_saw =
      minera::discover_saw_nets(selected_log, saw_selected_options);
  assert(selected_saw.ot_saw_nets.contains("order-main") &&
         selected_saw.ot_saw_nets.contains("item-main"));
  assert(selected_saw.ot_saw_nets.at("order-main").net.activities() ==
         std::vector<std::string>({"Assemble", "Deliver"}));
  assert(selected_saw.multi_place_object_types.size() > 0);
  for (const auto& [place, type] : selected_saw.multi_place_object_types) {
    (void)place;
    assert(type == "order-main" || type == "item-main");
  }
  assert(!selected_saw.ot_saw_nets.at("order-main").transition_weights.empty());
  bool saw_missing_selector_rejected = false;
  try {
    auto invalid = saw_selected_options;
    invalid.activity_attribute = "missing_activity";
    (void)minera::discover_saw_nets(selected_log, invalid);
  } catch (const std::invalid_argument&) {
    saw_missing_selector_rejected = true;
  }
  assert(saw_missing_selector_rejected);

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
  minera::TokenInteger total_missing = 0;
  for (const auto& place : replay_diagnostics.places)
    total_missing += place.missing;
  assert(total_missing > 0 && replay_diagnostics.transition_activations[1] == 1);

  variant1_text_contract(log);
  variant2_tuple_contract(log);

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

  const std::string classic_metadata = R"json({
    "ocel:eventTypes":{"Create":{"approved":"boolean","when":"datetime"},"EmptyEvent":{}},
    "ocel:objectTypes":{"Order":{"amount":"decimal"},"EmptyObject":{}},
    "ocel:objects":{"o":{"ocel:type":"Order","ocel:ovmap":{"amount":2.5}}},
    "ocel:events":{"e":{"ocel:activity":"Create","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o"],"ocel:vmap":{"approved":true}}}
  })json";
  const auto classic_metadata_log = minera::parse_ocel_json(classic_metadata);
  assert(classic_metadata_log.event_type_attributes.at("Create").at("approved") ==
         "boolean");
  assert(classic_metadata_log.event_type_attributes.at("Create").at("when") ==
         "datetime");
  assert(classic_metadata_log.event_type_attributes.at("EmptyEvent").empty());
  assert(classic_metadata_log.object_type_attributes.at("Order").at("amount") ==
         "decimal");
  assert(classic_metadata_log.object_type_attributes.at("EmptyObject").empty());

  const auto classic_catalog_json =
      minera::serialize_ocel_json(catalog, minera::OcelJsonFormat::kClassic);
  assert(classic_catalog_json.find("\"ocel:eventTypes\"") !=
             std::string::npos &&
         classic_catalog_json.find("\"ocel:objectTypes\"") !=
             std::string::npos);
  const auto classic_catalog_round =
      minera::parse_ocel_json(classic_catalog_json);
  assert(classic_catalog_round.event_type_attributes.at("EmptyEvent").at(
             "planned_at") == "datetime");
  assert(classic_catalog_round.event_type_attributes.at("Create").at("count") ==
         "long");
  assert(classic_catalog_round.object_type_attributes.at("EmptyObject").at(
             "label") == "string");
  assert(classic_catalog_round.object_type_attributes.at("Order").at("amount") ==
         "decimal");

  bool malformed_classic_catalog = false;
  try {
    (void)minera::parse_ocel_json(
        R"json({"ocel:eventTypes":[],"ocel:objects":{},"ocel:events":{}})json");
  } catch (...) {
    malformed_classic_catalog = true;
  }
  assert(malformed_classic_catalog);
  return 0;
}
