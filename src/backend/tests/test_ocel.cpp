#include "minera/ocel.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/saw_nets.hpp"
#include "../reference_tree_net.hpp"

namespace {

constexpr std::size_t kOcelOverLimitPadding = 128u * 1024u * 1024u;

std::string policy_net_signature(const minera::PetriNet& net) {
  std::ostringstream out;
  out << net.place_count() << ':' << net.source() << ':' << net.sink() << ':';
  for (const auto& activity : net.activities()) out << activity << ',';
  for (const auto& transition : net.transitions()) {
    out << '|';
    if (transition.activity) out << 'a' << *transition.activity;
    else out << 't';
    out << '[';
    for (const auto place : transition.inputs) out << place << ',';
    out << "]>[";
    for (const auto place : transition.outputs) out << place << ',';
    out << ']';
  }
  return out.str();
}

minera::Ocel policy_ocel_from_traces(
    const std::vector<std::vector<std::string>>& traces) {
  minera::Ocel result;
  for (std::size_t trace = 0; trace < traces.size(); ++trace) {
    const auto suffix = std::to_string(trace);
    const auto order_id = "order-" + suffix;
    const auto item_id = "item-" + suffix;
    result.objects.push_back({order_id, "order", {}});
    result.objects.push_back({item_id, "item", {}});
    for (std::size_t event = 0; event < traces[trace].size(); ++event) {
      const auto id = "event-" + suffix + "-" + std::to_string(event);
      const auto timestamp = "2026-01-01T00:00:0" +
                             std::to_string(event) + "Z";
      result.events.push_back({id, traces[trace][event], timestamp, {}});
      result.relations.push_back({id, order_id, ""});
      result.relations.push_back({id, item_id, ""});
    }
  }
  return result;
}

void ocpn_miner_policy_contract() {
  // These are existing inductive-miner policy counterexamples, lifted to
  // shared-event OCELs. Both object types must receive each selected policy.
  const std::vector<std::vector<std::vector<std::string>>> fixtures = {
      {{"a"}, {"a", "b", "c"}},
      {{"a", "c", "b", "b"}, {"c", "b", "a"}},
      {{"A", "D", "B"}, {"C", "A", "C"}, {"C", "D", "D", "B"}}};
  for (std::size_t fixture = 0; fixture < fixtures.size(); ++fixture) {
    const auto log = policy_ocel_from_traces(fixtures[fixture]);
    const auto default_model = minera::discover_ocpn(log);
    minera::OcpnDiscoveryOptions explicit_defaults;
    // The public discovery mines without the strict sequence cut or
    // fall-throughs.
    explicit_defaults.disable_strict_sequence_cut = true;
    explicit_defaults.disable_fallthroughs = true;
    assert(minera::serialize_ocpn_json(default_model) ==
           minera::serialize_ocpn_json(
               minera::discover_ocpn(log, explicit_defaults)));

    for (const auto variant : {minera::OcpnInductiveVariant::kLog,
                               minera::OcpnInductiveVariant::kDfg}) {
      std::vector<std::string> policy_signatures;
      for (const bool disable_strict : {false, true}) {
        for (const bool disable_fallthroughs : {false, true}) {
          minera::OcpnDiscoveryOptions options;
          options.variant = variant;
          options.disable_strict_sequence_cut = disable_strict;
          options.disable_fallthroughs = disable_fallthroughs;
          const auto model = minera::discover_ocpn(log, options);
          const auto causal = minera::discover_oc_causal_net(log, options);
          assert(minera::serialize_ocpn_json(model) ==
                 minera::serialize_ocpn_json(causal.workflow_projection));

          // Compare each complete discovered net with a separately configured
          // direct inductive-miner invocation, including invisible routing.
          minera::InductiveMinerVariantLog variants;
          for (const auto& trace : fixtures[fixture])
            variants.push_back({trace, 1});
          minera::InductiveMinerOptions miner;
          miner.variant = variant == minera::OcpnInductiveVariant::kLog
                              ? minera::InductiveMinerVariant::kLog
                              : minera::InductiveMinerVariant::kDfg;
          miner.disable_strict_sequence_cut = disable_strict;
          miner.disable_fallthroughs = disable_fallthroughs;
          const auto expected = policy_net_signature(minera::process_tree_reference_net(
              minera::discover_process_tree(variants, miner)));
          assert(model.petri_nets.size() == 2);
          for (const auto& [type, net] : model.petri_nets) {
            assert(type == "order" || type == "item");
            assert(policy_net_signature(net) == expected);
          }
          policy_signatures.push_back(expected);
        }
      }
      // Defaults vs one enabled option: [false,false], [false,true],
      // [true,false], [true,true]. Compare actual executable topology.
      if (fixture == 0 && variant == minera::OcpnInductiveVariant::kLog)
        assert(policy_signatures[0] != policy_signatures[2]);
      if (fixture == 1 && variant == minera::OcpnInductiveVariant::kLog)
        assert(policy_signatures[0] != policy_signatures[1]);
      if (fixture == 2 && variant == minera::OcpnInductiveVariant::kDfg)
        assert(policy_signatures[0] != policy_signatures[2]);
    }
  }
}

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

// Captured from pm4py 2.7.22 read_ocel2_json: a value that does not match its
// declared type is coerced like _parse_attr_value or kept raw, never rejected
// (pm4py's own writer declares partially missing boolean columns "string").
void standard_json_declared_type_fallback_contract() {
  const std::string input = R"({"objectTypes":[{"name":"order","attributes":[{"name":"w","type":"float"},{"name":"n","type":"integer"},{"name":"ok","type":"boolean"}]}],
"eventTypes":[{"name":"a","attributes":[{"name":"flag","type":"string"},{"name":"b0","type":"boolean"},{"name":"bs","type":"boolean"},{"name":"bt","type":"boolean"},{"name":"i","type":"int"},{"name":"is","type":"int"},{"name":"f","type":"float"},{"name":"d","type":"date"}]}],
"objects":[{"id":"o1","type":"order","attributes":[{"name":"w","time":"1970-01-01T00:00:00Z","value":"n/a"},{"name":"n","time":"1970-01-01T00:00:00Z","value":"x1"},{"name":"ok","time":"1970-01-01T00:00:00Z","value":0}]}],
"events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[{"name":"flag","value":true},{"name":"b0","value":"0"},{"name":"bs","value":"no"},{"name":"bt","value":" TRUE "},{"name":"i","value":3.7},{"name":"is","value":" +12 "},{"name":"f","value":true},{"name":"d","value":5}],"relationships":[{"objectId":"o1","qualifier":""}]}]})";
  const auto log = minera::parse_ocel_json(input);
  const auto value = [](const std::vector<minera::OcelAttribute>& attributes,
                        const std::string& key) {
    const auto found = std::find_if(attributes.begin(), attributes.end(),
                                    [&](const auto& item) { return item.key == key; });
    assert(found != attributes.end());
    return found->value;
  };
  const auto& event = log.events.at(0).attributes;
  assert(std::get<bool>(value(event, "flag")) == true);
  assert(std::get<bool>(value(event, "b0")) == true);   // bool("0")
  assert(std::get<bool>(value(event, "bs")) == true);   // bool("no")
  assert(std::get<bool>(value(event, "bt")) == true);
  assert(std::get<std::int64_t>(value(event, "i")) == 3);  // int(3.7)
  assert(std::get<std::int64_t>(value(event, "is")) == 12);
  assert(std::get<double>(value(event, "f")) == 1.0);       // float(True)
  assert(std::get<std::int64_t>(value(event, "d")) == 5);   // raw value
  const auto& object = log.objects.at(0).attributes;
  assert(std::get<std::string>(value(object, "w")) == "n/a");
  assert(std::get<std::string>(value(object, "n")) == "x1");
  assert(std::get<bool>(value(object, "ok")) == false);
}

// Reference capture (pm4py 2.7.22 read_ocel2_json / read_ocel): events and
// objects without event-object relations are dropped, with the O2O links and
// object changes that touch a dropped object; a log without relations reads as
// empty.
void readers_drop_unrelated_entries_contract() {
  const auto standard = minera::parse_ocel_json(R"json({"objectTypes":[{"name":"order","attributes":[{"name":"st","type":"string"}]}],"eventTypes":[{"name":"A","attributes":[]},{"name":"B","attributes":[]}],
"objects":[{"id":"o1","type":"order","attributes":[{"name":"st","time":"2024-01-01T00:00:00Z","value":"x"}],"relationships":[{"objectId":"o2","qualifier":"has"}]},
{"id":"o2","type":"order","attributes":[{"name":"st","time":"2024-01-01T00:00:00Z","value":"y"},{"name":"st","time":"2024-01-02T00:00:00Z","value":"z"}],"relationships":[{"objectId":"o1","qualifier":"rev"}]}],
"events":[{"id":"e1","type":"A","time":"2024-01-01T00:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":""}]},
{"id":"e2","type":"B","time":"2024-01-01T01:00:00Z","attributes":[],"relationships":[]}]}
)json");
  assert(standard.events.size() == 1 && standard.events[0].id == "e1");
  assert(standard.objects.size() == 1 && standard.objects[0].id == "o1");
  assert(standard.relations.size() == 1);
  assert(standard.o2o.empty() && standard.object_changes.empty());

  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}},)"
      R"("e2":{"ocel:activity":"B","ocel:timestamp":"2024-01-01T01:00:00","ocel:omap":[],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}},"o2":{"ocel:type":"order","ocel:ovmap":{}}}})");
  assert(classic.events.size() == 1 && classic.events[0].id == "e1");
  assert(classic.objects.size() == 1 && classic.objects[0].id == "o1");

  const auto unrelated = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00","ocel:omap":[],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})");
  assert(unrelated.events.empty() && unrelated.objects.empty());

  // Native callers that build an OCEL in memory keep their entries until a
  // reader sees them.
  minera::Ocel in_memory;
  in_memory.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
  in_memory.objects.push_back({"o1", "order", {}});
  minera::drop_unrelated_ocel_entries(in_memory);
  assert(in_memory.events.empty() && in_memory.objects.empty());
}

// OCEL JSON size limits: the default limit follows the server's payload
// limit, the message names the limit instead of calling the file invalid,
// and the scanner reports what the parser would.
void json_limits_contract() {
  assert(minera::ocel_limit_text(128u * 1024u * 1024u) == "128 MiB");
  assert(minera::ocel_limit_text(1000) == "1000 bytes");
  const std::string small =
      R"({"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})";
  assert(!minera::ocel_json_limit_violation(small));
  minera::OcelJsonLimits tight;
  tight.max_bytes = 64;
  const auto violation = minera::ocel_json_limit_violation(small, tight);
  assert(violation && violation->find("exceeds the 64 bytes OCEL limit") != std::string::npos);
  try {
    (void)minera::parse_ocel_json(small, tight);
    assert(false);
  } catch (const std::invalid_argument& error) {
    assert(std::string(error.what()) == "OCEL JSON file exceeds the 64 bytes OCEL limit");
  }
  minera::OcelJsonLimits few_nodes;
  few_nodes.max_nodes = 5;
  assert(minera::ocel_json_limit_violation(small, few_nodes));
  bool node_limit = false;
  try { (void)minera::parse_ocel_json(small, few_nodes); } catch (const std::invalid_argument&) { node_limit = true; }
  assert(node_limit);
  minera::OcelJsonLimits shallow;
  shallow.max_depth = 3;
  assert(minera::ocel_json_limit_violation(small, shallow));

  // A document over the former 16 MiB limit reads.
  std::string large = R"({"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{"pad":")";
  large.append(17u * 1024u * 1024u, ' ');
  large += R"("}}},"ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})";
  minera::OcelJsonLimits long_strings;
  long_strings.max_string_bytes = 32u * 1024u * 1024u;
  assert(!minera::ocel_json_limit_violation(large, long_strings));
  assert(minera::parse_ocel_json(large, long_strings).events.size() == 1);
  std::string over(kOcelOverLimitPadding, ' ');
  over.insert(0, small);
  try {
    (void)minera::parse_ocel_json(over);
    assert(false);
  } catch (const std::invalid_argument& error) {
    assert(std::string(error.what()) == "OCEL JSON file exceeds the 128 MiB OCEL limit");
  }
}

// Reference capture (pm4py 2.7.22 read_ocel / write_ocel): qualifiers come
// from typedOmap, attributes keep their document order, and the writers keep
// the log's event, object and attribute order and print floats as Python's
// repr.
void json_order_contract() {
  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e2":{"ocel:activity":"B","ocel:timestamp":"2024-01-02T00:00:00","ocel:omap":["o2","o1"],)"
      R"("ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":"q1"},{"ocel:oid":"o2","ocel:qualifier":"q2"}],"ocel:vmap":{"z":0.1,"a":1e20}},)"
      R"("e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o2":{"ocel:type":"item","ocel:ovmap":{"w":2,"b":true}},"o1":{"ocel:type":"order","ocel:ovmap":{}}}})");
  assert(classic.events.size() == 2 && classic.events[1].id == "e2");
  assert(classic.events[1].attributes[0].key == "z" && classic.events[1].attributes[1].key == "a");
  assert(classic.objects[0].id == "o2" && classic.objects[0].attributes[0].key == "w");
  std::size_t qualified = 0;
  for (const auto& relation : classic.relations)
    if (relation.event_id == "e2")
      qualified += (relation.object_id == "o1" && relation.qualifier == "q1") ||
                   (relation.object_id == "o2" && relation.qualifier == "q2");
  assert(qualified == 2);
  const auto written = minera::serialize_ocel_json(classic, minera::OcelJsonFormat::kClassic);
  assert(written.find(R"("z":0.1,"a":1e+20)") != std::string::npos);
  assert(written.find(R"("w":2,"b":true)") != std::string::npos);
  assert(written.find(R"("e1":)") < written.find(R"("e2":)"));
  assert(written.find(R"("o2":)") < written.find(R"("o1":)"));
  assert(written.find("minera:keepUnrelated") == std::string::npos);
}

// The reference CSV reader keeps orphan events and objects; the file
// readers of the other formats drop them. An OCEL that Minera serializes
// carries a marker, so a CSV import keeps them through every later route.
void csv_orphans_survive_routes_contract() {
  const auto csv = minera::parse_ocel_csv(
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order,ocel:type:item\n"
      "e1,create,2023-01-01 10:00:00,['o1'],['i1']\n"
      "e2,orphan,2023-01-01 11:00:00,[],[]\n"
      "e3,pay,2023-01-01 12:00:00,['o1'],[]\n",
      "ocel:oid,ocel:type,color\n"
      "o1,order,red\n"
      "i1,item,blue\n"
      "g1,ghost,green\n");
  assert(csv.events.size() == 3 && csv.objects.size() == 3);
  for (const auto format : {minera::OcelJsonFormat::kClassic, minera::OcelJsonFormat::kStandard}) {
    const auto json = minera::serialize_ocel_json(csv, format);
    assert(json.find(R"("minera:keepUnrelated":true)") != std::string::npos);
    const auto routed = minera::parse_ocel_json(json);
    assert(routed.events.size() == 3 && routed.objects.size() == 3);
    const auto graph = minera::discover_ocdfg_classic(routed);
    assert((graph.activities == std::vector<std::string>{"create", "orphan", "pay"}));
    auto types = graph.object_types;
    std::sort(types.begin(), types.end());
    assert((types == std::vector<std::string>{"ghost", "item", "order"}));
    // Twice through a route keeps them too.
    assert(minera::parse_ocel_json(minera::serialize_ocel_json(routed, format)).events.size() == 3);
  }
  // A file without the marker reads as the reference reader reads it.
  auto unmarked = minera::serialize_ocel_json(csv, minera::OcelJsonFormat::kStandard);
  const std::string marker = R"(,"minera:keepUnrelated":true)";
  unmarked.erase(unmarked.find(marker), marker.size());
  const auto dropped = minera::parse_ocel_json(unmarked);
  assert(dropped.events.size() == 2 && dropped.objects.size() == 2);
  bool bad_marker = false;
  try {
    auto wrong = minera::serialize_ocel_json(csv, minera::OcelJsonFormat::kStandard);
    wrong.replace(wrong.find(marker), marker.size(), R"(,"minera:keepUnrelated":1)");
    (void)minera::parse_ocel_json(wrong);
  } catch (const std::invalid_argument&) {
    bad_marker = true;
  }
  assert(bad_marker);
}

// An object id listed twice, under two types, in a JSON document (observed
// black-box on pm4py 2.7.22). OCEL 2.0 JSON: one object in the place of the
// first listing, with the type, attribute values and object relationships
// of the last; the attribute changes of every listing stay, in document
// order, read with their own listing's declared types and under the last
// type. Classic JSON: json.load keeps the last entry of a repeated key in the
// place of the first; a change has its object's type whatever its own
// "ocel:type" says.
void repeated_object_ids_contract() {
  const auto standard = minera::parse_ocel_json(
      R"({"objectTypes":[{"name":"order","attributes":[{"name":"p","type":"float"}]},)"
      R"({"name":"item","attributes":[{"name":"p","type":"string"},{"name":"r","type":"string"}]}],)"
      R"("eventTypes":[{"name":"A","attributes":[]}],"objects":[)"
      R"({"id":"o1","type":"order","attributes":[{"name":"p","value":"1","time":"1970-01-01T00:00:00Z"},)"
      R"({"name":"p","value":"2","time":"2024-01-09T00:00:00Z"}],"relationships":[{"objectId":"o2","qualifier":"r1"}]},)"
      R"({"id":"o2","type":"item","relationships":[{"objectId":"o1","qualifier":"r2"}]},)"
      R"({"id":"o3","type":"item"},)"
      R"({"id":"o1","type":"item","attributes":[{"name":"r","value":"R","time":"1970-01-01T00:00:00Z"},)"
      R"({"name":"p","value":"7","time":"1970-01-01T00:00:00Z"},{"name":"p","value":"8","time":"2024-01-06T00:00:00Z"}],)"
      R"("relationships":[{"objectId":"o3","qualifier":"r3"}]},)"
      R"({"id":"o3","type":"order","relationships":[{"objectId":"o2","qualifier":"r4"}]}],)"
      R"("events":[{"id":"e1","type":"A","time":"2024-01-01T00:00:00Z","relationships":[)"
      R"({"objectId":"o1","qualifier":"x"},{"objectId":"o2","qualifier":"y"},{"objectId":"o3","qualifier":"y"}]}]})");
  assert(standard.objects.size() == 3);
  assert(standard.objects[0].id == "o1" && standard.objects[0].type == "item");
  assert(standard.objects[0].attributes.size() == 2 && standard.objects[0].attributes[0].key == "r" &&
         std::get<std::string>(standard.objects[0].attributes[1].value) == "7");
  assert(standard.objects[1].id == "o2" && standard.objects[1].type == "item");
  assert(standard.objects[2].id == "o3" && standard.objects[2].type == "order");
  assert(standard.o2o.size() == 3);
  assert(standard.o2o[0].source_id == "o1" && standard.o2o[0].target_id == "o3" && standard.o2o[0].qualifier == "r3");
  assert(standard.o2o[1].qualifier == "r2" && standard.o2o[2].qualifier == "r4");
  assert(standard.object_changes.size() == 2);
  assert(standard.object_changes[0].object_type == "item" &&
         std::get<double>(standard.object_changes[0].value) == 2.0 &&
         standard.object_changes[0].timestamp == "2024-01-09T00:00:00Z");
  assert(standard.object_changes[1].object_type == "item" &&
         std::get<std::string>(standard.object_changes[1].value) == "8");

  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:timestamp":"2024-01-01T00:00:00Z","ocel:activity":"A","ocel:vmap":{},"ocel:omap":["o1","o2"]}},)"
      R"("ocel:objectChanges":[{"ocel:oid":"o1","ocel:type":"order","ocel:timestamp":"2024-01-02T00:00:00Z","ocel:field":"p","p":5}],)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{"p":1}},"o2":{"ocel:type":"item","ocel:ovmap":{}},)"
      R"("o1":{"ocel:type":"item","ocel:ovmap":{"q":2}}}})");
  assert(classic.objects.size() == 2 && classic.objects[0].id == "o1" && classic.objects[0].type == "item");
  assert(classic.objects[0].attributes.size() == 1 && classic.objects[0].attributes[0].key == "q");
  assert(classic.object_changes.size() == 1 && classic.object_changes[0].object_type == "item");
  assert(classic.object_type_attributes.count("order") == 0);
}

// Naive JSON timestamps are stored as UTC, as every reference JSON and XML
// reader stores them (datetime64[us, UTC]): an event time is stamped "Z" and
// a change time "+00:00", the spellings of the reference JSON writers, so a
// naive document written back as JSON reads like theirs. The epoch time of a
// static value stays as written.
void naive_timestamps_read_as_utc_contract() {
  const auto log = minera::parse_ocel_json(
      R"({"objectTypes":[{"name":"order","attributes":[{"name":"p","type":"string"}]}],)"
      R"("eventTypes":[{"name":"A","attributes":[]}],"objects":[{"id":"o1","type":"order","attributes":[)"
      R"({"name":"p","value":"P1","time":"1970-01-01T00:00:00"},{"name":"p","value":"P2","time":"2024-01-05T12:00:00"}]}],)"
      R"("events":[{"id":"e2","type":"A","time":"2024-01-02T10:30:00.123456","relationships":[{"objectId":"o1"}]},)"
      R"({"id":"e1","type":"A","time":"2024-01-01T00:00:00","relationships":[{"objectId":"o1"}]}]})");
  assert(log.events.size() == 2 && log.events[0].id == "e1");
  assert(log.events[0].timestamp == "2024-01-01T00:00:00Z");
  assert(log.events[1].timestamp == "2024-01-02T10:30:00.123456Z");
  assert(log.object_changes.size() == 1 && log.object_changes[0].timestamp == "2024-01-05T12:00:00+00:00");
  const auto standard = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  assert(standard.find(R"("time":"2024-01-01T00:00:00Z")") != std::string::npos);
  assert(standard.find(R"("time":"2024-01-05T12:00:00+00:00")") != std::string::npos);
  assert(standard.find(R"("time":"1970-01-01T00:00:00Z")") != std::string::npos);
  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:timestamp":"2024-01-01 08:00","ocel:activity":"A","ocel:vmap":{},"ocel:omap":["o1"]}},)"
      R"("ocel:objectChanges":[{"ocel:oid":"o1","ocel:timestamp":"2024-01-02T00:00:00","ocel:field":"p","p":5}],)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})");
  assert(classic.events[0].timestamp == "2024-01-01T08:00:00Z");
  assert(classic.object_changes[0].timestamp == "2024-01-02T00:00:00+00:00");
  // An aware text is kept as written.
  const auto aware = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:timestamp":"2024-01-01T08:00:00+00:00","ocel:activity":"A","ocel:omap":["o1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})");
  assert(aware.events[0].timestamp == "2024-01-01T08:00:00+00:00");
}

// Reference (pm4py 2.7.22, black-box): write_ocel_json and write_ocel_xml
// fill an event or object global scope the OCEL does not declare with
// {"ocel:activity": "__INVALID__"} / {"ocel:type": "__INVALID__"}. Only
// read_ocel2_json (every scope, empty) and read_ocel_json (the scopes its
// file lists) declare scopes; OCEL 2.0 XML, SQLite, CSV and OCEL 1.0 XML
// reads declare none.
void global_scope_defaults_contract() {
  minera::Ocel log;
  log.objects.push_back({"o1", "order", {}});
  log.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
  log.relations.push_back({"e1", "o1", ""});
  const auto classic = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kClassic, false);
  assert(classic.find(R"("ocel:global-event":{"ocel:activity":"__INVALID__"})") != std::string::npos);
  assert(classic.find(R"("ocel:global-object":{"ocel:type":"__INVALID__"})") != std::string::npos);
  // read_ocel_json keeps them; the OCEL 2.0 JSON writer leaves them out and
  // its reader declares empty scopes, so they come back empty.
  const auto classic_read = minera::parse_ocel_json(classic);
  assert(classic_read.global_event_scope && classic_read.global_object_scope);
  assert(classic_read.globals.at("ocel:global-event.ocel:activity") ==
         minera::OcelValue(std::string("__INVALID__")));
  const auto standard = minera::serialize_ocel_json(classic_read, minera::OcelJsonFormat::kStandard);
  assert(standard.find("__INVALID__") == std::string::npos);
  const auto standard_read = minera::parse_ocel_json(standard);
  assert(standard_read.global_event_scope && standard_read.global_object_scope);
  const auto empty_scopes = minera::serialize_ocel_json(standard_read, minera::OcelJsonFormat::kClassic, false);
  assert(empty_scopes.find(R"("ocel:global-event":{},"ocel:global-object":{})") != std::string::npos);
  // A file's declared empty scope stays empty; an absent one gets its default.
  const auto one_scope = minera::parse_ocel_json(
      R"({"ocel:global-event":{},"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}}})");
  const auto one_written = minera::serialize_ocel_json(one_scope, minera::OcelJsonFormat::kClassic, false);
  assert(one_written.find(R"("ocel:global-event":{},"ocel:global-object":{"ocel:type":"__INVALID__"})") !=
         std::string::npos);
  // The defaults and the computed log scope carry nothing a CSV loses: the
  // CSV writer no longer refuses them without the loss allowance.
  auto generated = classic_read;
  generated.globals["ocel:global-log.ocel:version"] = std::string("1.0");
  generated.globals["ocel:global-log.ocel:ordering"] = std::string("timestamp");
  std::ostringstream csv;
  minera::write_ocel_csv(csv, generated);
  assert(csv.str().find("e1") != std::string::npos);
  generated.globals["ocel:global-event.owner"] = std::string("x");
  bool refused = false;
  try {
    std::ostringstream lossy;
    minera::write_ocel_csv(lossy, generated);
  } catch (const std::invalid_argument&) {
    refused = true;
  }
  assert(refused);
}

// Round AI: pm4py 2.7.22 write_ocel2_json declares each attribute as its
// frame column holds it (reference capture): integer and boolean columns
// every object has are "string", an integer column some object lacks is
// "float", a boolean one "string", timestamps "date"; listed per type in
// column order. Its reader casts values to the declared type, so this is what
// makes pm4py read the file back as it reads its own export.
void standard_json_declares_frame_column_types_contract() {
  const auto log = minera::parse_ocel_json(R"json({"objectTypes":[
{"name":"Order","attributes":[{"name":"q","type":"integer"},{"name":"ok","type":"boolean"},{"name":"at","type":"time"},{"name":"n","type":"integer"}]},
{"name":"Item","attributes":[{"name":"q","type":"integer"},{"name":"ok","type":"boolean"},{"name":"at","type":"time"},{"name":"m","type":"boolean"}]}],
"eventTypes":[{"name":"E","attributes":[{"name":"k","type":"integer"},{"name":"s","type":"string"}]}],
"objects":[{"id":"o1","type":"Order","attributes":[{"name":"q","time":"1970-01-01T00:00:00Z","value":1},{"name":"ok","time":"1970-01-01T00:00:00Z","value":true},{"name":"at","time":"1970-01-01T00:00:00Z","value":"2024-01-05T00:00:00Z"},{"name":"n","time":"1970-01-01T00:00:00Z","value":4},{"name":"q","time":"2024-01-02T00:00:00Z","value":2}]},
{"id":"i1","type":"Item","attributes":[{"name":"q","time":"1970-01-01T00:00:00Z","value":3},{"name":"ok","time":"1970-01-01T00:00:00Z","value":false},{"name":"at","time":"1970-01-01T00:00:00Z","value":"2024-01-06T00:00:00Z"},{"name":"m","time":"1970-01-01T00:00:00Z","value":true}]}],
"events":[{"id":"e1","type":"E","time":"2024-01-01T00:00:00Z","attributes":[{"name":"k","value":7},{"name":"s","value":"x"}],"relationships":[{"objectId":"o1","qualifier":""},{"objectId":"i1","qualifier":""}]},
{"id":"e2","type":"E","time":"2024-01-02T00:00:00Z","attributes":[{"name":"k","value":8},{"name":"s","value":"y"}],"relationships":[{"objectId":"o1","qualifier":""}]}]})json");
  const auto written = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  assert(written.find(R"("objectTypes":[{"name":"Item","attributes":[{"name":"q","type":"string"},{"name":"ok","type":"string"},{"name":"at","type":"date"},{"name":"m","type":"string"}]},{"name":"Order","attributes":[{"name":"q","type":"string"},{"name":"ok","type":"string"},{"name":"at","type":"date"},{"name":"n","type":"float"}]}])") != std::string::npos);
  assert(written.find(R"("eventTypes":[{"name":"E","attributes":[{"name":"k","type":"string"},{"name":"s","type":"string"}]}])") != std::string::npos);
  // Minera reads its own export back with the same values.
  const auto again = minera::parse_ocel_json(written);
  assert(again.objects.size() == 2 && again.object_changes.size() == 1);
  assert(std::get<std::int64_t>(again.object_changes[0].value) == 2);
  for (const auto& attribute : again.objects[0].attributes)
    if (attribute.key == "n") assert(std::get<double>(attribute.value) == 4.0);
}

// A relation, object link, qualifier or object change that names an object
// the document does not list is dropped, as every reference JSON reader drops
// it (observed black-box); an event left without relations goes too. The
// readers used to refuse the whole log.
void readers_drop_unknown_object_references_contract() {
  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1","zz"],)"
      R"("ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":"q"},{"ocel:oid":"zz","ocel:qualifier":"w"},{"ocel:oid":"o2","ocel:qualifier":"v"}],"ocel:vmap":{}},)"
      R"("e2":{"ocel:activity":"B","ocel:timestamp":"2024-01-01T00:01:00Z","ocel:omap":["o2"],"ocel:vmap":{}},)"
      R"("e3":{"ocel:activity":"C","ocel:timestamp":"2024-01-01T00:02:00Z","ocel:omap":["zz"],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{},"ocel:o2o":[{"ocel:oid":"zz","ocel:qualifier":"x"},{"ocel:oid":"o2","ocel:qualifier":"y"}]},)"
      R"("o2":{"ocel:type":"order","ocel:ovmap":{}}},)"
      R"("ocel:objectChanges":[{"ocel:oid":"zz","ocel:type":"order","ocel:field":"st","st":"a","ocel:timestamp":"2024-01-02T00:00:00Z"},)"
      R"({"ocel:oid":"o1","ocel:type":"order","ocel:field":"st","st":"b","ocel:timestamp":"2024-01-02T00:00:00Z"}]})");
  assert(classic.events.size() == 2 && classic.events[0].id == "e1" && classic.events[1].id == "e2");
  assert(classic.relations.size() == 2);
  assert(classic.relations[0].event_id == "e1" && classic.relations[0].object_id == "o1" &&
         classic.relations[0].qualifier == "q");
  assert(classic.relations[1].event_id == "e2" && classic.relations[1].object_id == "o2" &&
         classic.relations[1].qualifier.empty());
  assert(classic.o2o.size() == 1 && classic.o2o[0].target_id == "o2");
  assert(classic.object_changes.size() == 1 && classic.object_changes[0].object_id == "o1");

  const auto standard = minera::parse_ocel_json(R"json({"objectTypes":[{"name":"order","attributes":[]}],"eventTypes":[{"name":"A","attributes":[]},{"name":"B","attributes":[]}],
"objects":[{"id":"o1","type":"order","attributes":[],"relationships":[{"objectId":"zz8","qualifier":"x"}]},{"id":"o2","type":"order","attributes":[]}],
"events":[{"id":"e1","type":"A","time":"2024-01-01T00:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":""},{"objectId":"zz9","qualifier":"q"}]},
{"id":"e2","type":"B","time":"2024-01-01T00:01:00Z","attributes":[],"relationships":[{"objectId":"zz9","qualifier":""}]}]})json");
  assert(standard.events.size() == 1 && standard.events[0].id == "e1");
  assert(standard.objects.size() == 1 && standard.objects[0].id == "o1");
  assert(standard.relations.size() == 1 && standard.relations[0].object_id == "o1");
  assert(standard.o2o.empty());
}

// An event with an empty id or activity and an object with an empty id or
// type are dropped with their relations, as both reference JSON readers drop
// them (observed black-box); an event left without relations goes too. A
// literal-names document (exact filter input) keeps them. The OCEL 2.0 reader
// also takes a repeated event id from its last listing, in the place of the
// first, and reads empty attribute names and repeated declarations.
void readers_drop_empty_names_contract() {
  const auto classic = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]},)"
      R"("":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T00:01:00Z","ocel:omap":["o1"]},)"
      R"("e3":{"ocel:activity":"b","ocel:timestamp":"2024-01-01T00:02:00Z","ocel:omap":["o1","p1",""]},)"
      R"("e4":{"ocel:activity":"c","ocel:timestamp":"2024-01-01T00:03:00Z","ocel:omap":["p1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"o","ocel:o2o":[{"ocel:oid":"p1"},{"ocel:oid":""}]},"p1":{"ocel:type":""},"":{"ocel:type":"o"}}})");
  assert(classic.events.size() == 1 && classic.events[0].id == "e3");
  assert(classic.objects.size() == 1 && classic.objects[0].id == "o1");
  assert(classic.relations.size() == 1 && classic.relations[0].object_id == "o1");
  assert(classic.o2o.empty());
  assert(!classic.event_type_attributes.contains("") && !classic.object_type_attributes.contains(""));
  const auto literal = minera::parse_ocel_json_literal_names(
      R"({"ocel:events":{"e1":{"ocel:activity":"","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":""}}})");
  assert(literal.events.size() == 1 && literal.objects.size() == 1);

  const auto standard = minera::parse_ocel_json(R"json({"objectTypes":[{"name":"o","attributes":[{"name":"","type":"integer"}]},{"name":"","attributes":[]}],
"eventTypes":[{"name":"a","attributes":[{"name":"x","type":"integer"},{"name":"x","type":"string"},{"name":"y","type":""}]},{"name":"","attributes":[]}],
"objects":[{"id":"o1","type":"o","attributes":[{"name":"","value":"7"}]},{"id":"o2","type":"","attributes":[]},{"id":"","type":"o","attributes":[]}],
"events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[{"name":"x","value":"5"},{"name":"y","value":"6"}],"relationships":[{"objectId":"o2"}]},
{"id":"e2","type":"","time":"2024-01-01T00:00:00Z","relationships":[{"objectId":"o1"}]},
{"id":"e3","type":"b","time":"2024-01-01T00:00:00Z","relationships":[{"objectId":"o1"}]},
{"id":"e1","type":"c","time":"2024-01-01T00:00:00Z","attributes":[{"name":"x","value":"5"}],"relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":""}]}]})json");
  assert(standard.events.size() == 2 && standard.events[0].id == "e1" &&
         standard.events[0].activity == "c" && standard.events[1].id == "e3");
  assert(standard.objects.size() == 1 && standard.objects[0].id == "o1");
  assert(standard.objects[0].attributes.size() == 1 && standard.objects[0].attributes[0].key.empty() &&
         std::get<std::int64_t>(standard.objects[0].attributes[0].value) == 7);
  assert(standard.relations.size() == 2 && standard.relations[0].event_id == "e1" &&
         standard.relations[0].qualifier == "q" && standard.relations[1].event_id == "e3");
  assert(!standard.event_type_attributes.contains("") && !standard.object_type_attributes.contains(""));
  assert(standard.event_type_attributes.at("a").at("x") == "string" &&
         !standard.event_type_attributes.at("a").contains("y"));
  const auto typed = minera::parse_ocel_json(R"json({"eventTypes":[{"name":"a","attributes":[{"name":"x","type":"integer"},{"name":"x","type":"string"},{"name":"y","type":""}]}],
"objects":[{"id":"o1","type":"o"}],
"events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[{"name":"x","value":"5"},{"name":"y","value":"6"}],"relationships":[{"objectId":"o1"}]}]})json");
  assert(std::get<std::string>(typed.events[0].attributes[0].value) == "5" &&
         std::get<std::string>(typed.events[0].attributes[1].value) == "6");
  // The classic transport carries the empty attribute name of a catalog.
  const auto round = minera::parse_ocel_json(minera::serialize_ocel_json(standard));
  assert(round.object_type_attributes.at("o").contains(""));
}

// OCDFG edge keys are labels: "a" -> "b->c" and "a->b" -> "c" both read
// "a->b->c", so the second gets "#2" and the JSON object keeps both edges.
void ocdfg_edge_keys_stay_unique_contract() {
  const auto log = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]},)"
      R"("e2":{"ocel:activity":"b->c","ocel:timestamp":"2024-01-01T00:01:00Z","ocel:omap":["o1"]},)"
      R"("e3":{"ocel:activity":"a->b","ocel:timestamp":"2024-01-01T00:02:00Z","ocel:omap":["o2"]},)"
      R"("e4":{"ocel:activity":"c","ocel:timestamp":"2024-01-01T00:03:00Z","ocel:omap":["o2"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order"},"o2":{"ocel:type":"order"}}})");
  const auto graph = minera::discover_ocdfg_classic(log);
  assert(graph.edges.at("order").size() == 2);
  const auto json = minera::serialize_ocdfg_json(graph);
  assert(json.find(R"("a->b->c":{"source_activity":"a","target_activity":"b->c")") != std::string::npos);
  assert(json.find(R"("a->b->c#2":{"source_activity":"a->b","target_activity":"c")") != std::string::npos);
  const auto reread = minera::parse_ocdfg_json(json);
  assert(reread.edges.at("order").size() == 2);
  // An empty activity (a literal-names log) reads back from the endpoint
  // fields; the key "->b" alone could not name it.
  const auto literal = minera::parse_ocel_json_literal_names(
      R"({"ocel:events":{"e1":{"ocel:activity":"","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]},)"
      R"("e2":{"ocel:activity":"b","ocel:timestamp":"2024-01-01T00:01:00Z","ocel:omap":["o1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order"}}})");
  const auto empty_round =
      minera::parse_ocdfg_json(minera::serialize_ocdfg_json(minera::discover_ocdfg_classic(literal)));
  assert(empty_round.edges.at("order").count({"", "b"}) == 1);
}

int main() {
  readers_drop_unknown_object_references_contract();
  readers_drop_empty_names_contract();
  ocdfg_edge_keys_stay_unique_contract();
  global_scope_defaults_contract();
  standard_json_declares_frame_column_types_contract();
  repeated_object_ids_contract();
  naive_timestamps_read_as_utc_contract();
  json_limits_contract();
  json_order_contract();
  csv_orphans_survive_routes_contract();
  readers_drop_unrelated_entries_contract();
  standard_json_declared_type_fallback_contract();
  ocpn_miner_policy_contract();
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

  {
    // Repeated co-occurrence and duplicate qualifiers retain one interaction
    // per object pair, while distinct pairs sharing types still add up.
    minera::Ocel repeated;
    repeated.objects = {{"o0", "T", {}}, {"o1", "U", {}}, {"o2", "T", {}}};
    for (int i = 0; i < 3; ++i) {
      const auto id = "e" + std::to_string(i);
      repeated.events.push_back({id, i == 1 ? "B" : "A", "", {}});
      for (const auto& object : repeated.objects)
        repeated.relations.push_back({id, object.id, ""});
      repeated.relations.push_back({id, "o0", "other-role"});
    }
    const auto graph = minera::discover_otg_classic(repeated);
    assert(graph.edges.size() == 9);
    for (const auto& edge : graph.edges) assert(edge.frequency == 1);
    const auto direct = minera::compare_otg(repeated, graph);
    assert(direct.fitness == 1.0 && direct.non_conforming_edges.empty());

    for (auto& object : repeated.objects) object.type = "T";
    const auto same_type = minera::discover_otg_classic(repeated);
    assert(same_type.edges.size() == 3);
    for (const auto& edge : same_type.edges) assert(edge.frequency == 3);

    repeated.events.push_back({"unlinked", "C", "", {}});
    const auto expect_missing_event = [&](auto discover_or_compare) {
      bool failed = false;
      try {
        discover_or_compare();
      } catch (const std::out_of_range& error) {
        failed = std::string(error.what()) == "'unlinked'";
      }
      assert(failed);
    };
    expect_missing_event([&] { (void)minera::discover_otg_classic(repeated); });
    expect_missing_event([&] { (void)minera::compare_otg(repeated, same_type); });
    const minera::Ocel empty;
    assert(minera::discover_otg_classic(empty).object_types.empty());
    auto objects_only = empty;
    objects_only.objects = repeated.objects;
    assert(minera::discover_otg_classic(objects_only).edges.empty());
  }

  {
    const auto escaped = minera::parse_ocel_json(
        R"({"events":[{"id":"e\ud83d\ude42","type":"A\ud83d\ude42","time":"2026-01-01T00:00:00Z","relationships":[{"objectId":"o\ud83d\ude42","qualifier":"q\ud83d\ude42"}]}],"objects":[{"id":"o\ud83d\ude42","type":"T\ud83d\ude42","attributes":[{"name":"label","value":"v\ud83d\ude42"}]}]})");
    const auto utf8 = minera::parse_ocel_json(
        R"({"events":[{"id":"e🙂","type":"A🙂","time":"2026-01-01T00:00:00Z","relationships":[{"objectId":"o🙂","qualifier":"q🙂"}]}],"objects":[{"id":"o🙂","type":"T🙂","attributes":[{"name":"label","value":"v🙂"}]}]})");
    assert(escaped.events[0].id == "e🙂" &&
           escaped.objects[0].type == "T🙂" &&
           escaped.relations[0].qualifier == "q🙂" &&
           std::get<std::string>(escaped.objects[0].attributes[0].value) == "v🙂");
    assert(minera::serialize_ocel_json(escaped) == minera::serialize_ocel_json(utf8));
    assert(minera::etot_json(minera::discover_etot_classic(escaped)) ==
           minera::etot_json(minera::discover_etot_classic(utf8)));
    assert(minera::otg_json(minera::discover_otg_classic(escaped)) ==
           minera::otg_json(minera::discover_otg_classic(utf8)));
    const auto boundaries = minera::parse_etot_json(
        R"({"activities":["\ud800\udc00","\udbff\udfff"],"object_types":[],"edges":[]})");
    assert(boundaries.activities[0] == "\xf0\x90\x80\x80" &&
           boundaries.activities[1] == "\xf4\x8f\xbf\xbf");
    for (const auto* invalid : {R"(\ud800)", R"(\ud800\u0041)",
                                R"(\udc00)", R"(\ud800\ud800)"}) {
      bool rejected = false;
      try {
        (void)minera::parse_etot_json(std::string("{\"activities\":[\"") +
                                    invalid + "\"],\"object_types\":[],\"edges\":[]}");
      } catch (const std::invalid_argument&) {
        rejected = true;
      }
      assert(rejected);
    }
  }

  const auto round =
      minera::parse_ocel_json(minera::serialize_ocel_json(log));
  // The object scope is undeclared and empty: write_ocel_json fills it with
  // its default, which the classic reader then keeps (declared).
  auto round_globals = log.globals;
  round_globals["ocel:global-object.ocel:type"] = std::string("__INVALID__");
  assert(round.events.size() == log.events.size() &&
         round.relations.size() == log.relations.size() &&
         round.o2o.size() == log.o2o.size() &&
         round.e2e.size() == log.e2e.size() &&
         round.object_changes.size() == log.object_changes.size() &&
         round.globals == round_globals &&
         round.global_event_scope && round.global_object_scope);
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
         standard_round.globals == log.globals);
  // The status change of o1, which has no static status, is written alone
  // (as write_ocel2_json writes it) and reads back as the base value, as
  // pm4py reads its own export.
  assert(standard.find("\"value\":null") == std::string::npos);
  assert(standard_round.object_changes.empty());
  assert(std::any_of(standard_round.objects[0].attributes.begin(),
                     standard_round.objects[0].attributes.end(), [](const auto& attribute) {
                       return attribute.key == "status" &&
                              attribute.value == minera::OcelValue(std::string("shipped"));
                     }));

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
    "eventTypes":[{"name":"Create","attributes":[]}],
    "events":[{"id":"e1","type":"Create","time":"2024-01-01T00:00:00Z","attributes":[],
      "relationships":[{"objectId":"o1","qualifier":""}]}]
  })json");
  // The event keeps o1: the reader drops objects without relations.
  assert(repeated_epoch.objects.size() == 1);
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

  // A type listed twice is its last listing (pm4py.read_ocel2_json types
  // the value by it; it used to be refused).
  const auto duplicate_type = minera::parse_ocel_json(
      R"json({"objectTypes":[{"name":"Order","attributes":[{"name":"n","type":"integer"}]},{"name":"Order"}],)json"
      R"json("eventTypes":[{"name":"A"}],"objects":[{"id":"o1","type":"Order","attributes":[{"name":"n","value":"5"}]}],)json"
      R"json("events":[{"id":"e1","type":"A","time":"2024-01-01T00:00:00Z","relationships":[{"objectId":"o1"}]}]})json");
  assert(duplicate_type.objects.size() == 1 &&
         std::get<std::string>(duplicate_type.objects[0].attributes.at(0).value) == "5");

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

  // A repeated key is the last entry in the place of the first, as
  // json.load reads it (pm4py.read_ocel_json keeps event "e" as "b").
  const auto repeated = minera::parse_ocel_json(
      R"({"ocel:objects":{"o":{"ocel:type":"x"}},"ocel:events":{"e":{"ocel:activity":"a","ocel:timestamp":"2024-01-01","ocel:omap":["o"]},)"
      R"("f":{"ocel:activity":"c","ocel:timestamp":"2024-01-03","ocel:omap":["o"]},"e":{"ocel:activity":"b","ocel:timestamp":"2024-01-02","ocel:omap":["o"]}}})");
  assert(repeated.events.size() == 2 && repeated.events[0].id == "e" && repeated.events[0].activity == "b");
  assert(repeated.relations.size() == 2);

  // An object link to an unlisted object is dropped, as the reference
  // reader drops it (it used to refuse the log).
  const auto bad_o2o = minera::parse_ocel_json(
      R"({"ocel:objects":{"o":{"ocel:type":"x","ocel:o2o":[{"ocel:oid":"missing"}]}},)"
      R"("ocel:events":{"e":{"ocel:activity":"a","ocel:timestamp":"2024-01-01","ocel:omap":["o"]}}})");
  assert(bad_o2o.objects.size() == 1 && bad_o2o.o2o.empty());
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
  const std::vector<std::pair<std::string, std::string>> repeated_associations{
      {"e1", "o1"}, {"e1", "o2"}, {"e1", "o2"}};
  assert(activity_metric.total_objects == repeated_associations);
  const auto& repeated_flow = metric_graph.edges.at("item").at({"A", "A"});
  assert((repeated_flow.event_couples ==
          std::vector<std::pair<std::string, std::string>>{{"e1", "e1"}}));
  assert(repeated_flow.total_objects.size() == 1 &&
         repeated_flow.unique_objects == std::vector<std::string>{"o2"});
  assert(repeated_flow.performance_event_couples == std::vector<double>{0.0});
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

  // Nonfinite weights also affect empty components through IEEE multiplication.
  // OCDFG fitness clips the resulting NaN to positive zero while diagnostics
  // continue to describe the original graph differences.
  const minera::OcdfgGraph empty_ocdfg;
  auto overflow_norm = ocdfg_norm;
  overflow_norm.edges["order"][{"Ship", "Create"}].event_couples = {
      {"e3", "e1"}};
  for (const auto weight : {&minera::OcdfgConformanceOptions::alpha,
                            &minera::OcdfgConformanceOptions::beta,
                            &minera::OcdfgConformanceOptions::gamma,
                            &minera::OcdfgConformanceOptions::delta}) {
    for (const auto value : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()}) {
      minera::OcdfgConformanceOptions options;
      options.*weight = value;
      const auto identical = minera::compare_ocdfg(ocdfg_norm, ocdfg_norm, options);
      assert(identical.fitness == 0.0 && !std::signbit(identical.fitness));
      assert(identical.missing_activities.empty() && identical.missing_flows.empty());
      const auto missing = minera::compare_ocdfg(ocdfg_real, ocdfg_norm, options);
      assert(missing.fitness == 0.0 && !std::signbit(missing.fitness));
      assert(missing.missing_activities == ocdfg_bad.missing_activities);
      assert(missing.activity_measure_differences == ocdfg_bad.activity_measure_differences);
      assert(missing.flow_measure_differences == ocdfg_bad.flow_measure_differences);
      assert(minera::compare_ocdfg(empty_ocdfg, empty_ocdfg, options).fitness == 0.0);
    }
    minera::OcdfgConformanceOptions options;
    options.*weight = std::numeric_limits<double>::max();
    const auto overflow = minera::compare_ocdfg(empty_ocdfg, overflow_norm, options);
    assert(overflow.fitness == 0.0 && !std::signbit(overflow.fitness));
    assert(minera::compare_ocdfg(overflow_norm, overflow_norm, options).fitness == 1.0);
  }

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
  assert(minera::ocdfg_conformance_json(selected_conformance) ==
         minera::ocdfg_conformance_json(unselected_conformance));
  const auto projected_conformance = minera::compare_ocdfg(
      selected_ocdfg, selected_ocdfg, selected_conformance_options);
  assert(projected_conformance.fitness == 1.0 &&
         projected_conformance.missing_activities.empty() &&
         projected_conformance.additional_activities.empty() &&
         projected_conformance.missing_flows.empty() &&
         projected_conformance.additional_flows.empty());
  auto ignored_selector = selected_conformance_options;
  ignored_selector.object_type_attribute = "missing_type";
  assert(minera::ocdfg_conformance_json(minera::compare_ocdfg(
             selected_log, selected_ocdfg, ignored_selector)) ==
         minera::ocdfg_conformance_json(unselected_conformance));

  {
    minera::Ocel empty_tables;
    empty_tables.table_columns.events = std::vector<std::string>{
        "ocel:eid", "ocel:activity", "ocel:timestamp", "selected_activity"};
    empty_tables.table_columns.objects = std::vector<std::string>{
        "ocel:oid", "ocel:type", "selected_type"};
    empty_tables.table_columns.relations = std::vector<std::string>{
        "ocel:eid", "ocel:activity", "ocel:timestamp", "selected_activity",
        "ocel:oid", "ocel:type", "selected_type", "ocel:qualifier"};
    const auto empty_graph = minera::discover_ocdfg_classic_with_options(
        empty_tables, ocdfg_selected_options);
    assert(empty_graph.activities.empty() && empty_graph.edges.empty());
    for (const auto format : {minera::OcelJsonFormat::kClassic,
                              minera::OcelJsonFormat::kStandard}) {
      const auto decoded = minera::parse_ocel_json(
          minera::serialize_ocel_json(empty_tables, format));
      assert(decoded.table_columns.events == empty_tables.table_columns.events &&
             decoded.table_columns.objects == empty_tables.table_columns.objects &&
             decoded.table_columns.relations == empty_tables.table_columns.relations);
      assert(minera::discover_ocdfg_classic_with_options(decoded,
                 ocdfg_selected_options).activities.empty());
    }
    const auto expect_missing_column = [&](const minera::Ocel& input,
                                            const std::string& selector,
                                            bool activity) {
      auto options = ocdfg_selected_options;
      if (activity) options.activity_attribute = selector;
      else options.object_type_attribute = selector;
      bool missing = false;
      try { (void)minera::discover_ocdfg_classic_with_options(input, options); }
      catch (const std::out_of_range& error) {
        missing = error.what() == std::string("'") + selector + "'";
      }
      assert(missing);
    };
    expect_missing_column(empty_tables, "absent_activity", true);
    expect_missing_column(empty_tables, "absent_type", false);
    expect_missing_column(selected_log, "absent_activity", true);
    expect_missing_column(selected_log, "absent_type", false);
    empty_tables.table_columns.events = std::vector<std::string>{};
    expect_missing_column(empty_tables, "selected_activity", true);
  }
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
  } catch (const std::out_of_range& error) {
    ocdfg_missing_selector_rejected = std::string(error.what()) == "'missing_type'";
  }
  assert(ocdfg_missing_selector_rejected);

  minera::SawNetDiscoveryOptions saw_selected_options;
  saw_selected_options.activity_attribute = "selected_activity";
  saw_selected_options.object_type_attribute = "selected_type";
  const auto selected_saw =
      minera::discover_saw_nets(selected_log, saw_selected_options);
  assert(selected_saw.ot_saw_nets.contains("order-main") &&
         selected_saw.ot_saw_nets.contains("item-main"));
  // A selected type label names the net, but the flattening matches the
  // canonical type, so no object feeds it: the net is a single silent step.
  const auto& selected_order_net = selected_saw.ot_saw_nets.at("order-main").net;
  assert(selected_order_net.transitions().size() == 1 &&
         !selected_order_net.transitions().front().activity);
  assert(selected_saw.multi_place_object_types.size() > 0);
  for (const auto& [place, type] : selected_saw.multi_place_object_types) {
    (void)place;
    assert(type == "order-main" || type == "item-main");
  }
  assert(selected_saw.ot_saw_nets.at("order-main").transition_weights.empty());
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
  // An attribute with values is declared as its frame column holds it, as
  // write_ocel2_json does (round AI): a full integer column is "string".
  assert(catalog_round.event_type_attributes.at("Create").at("count") ==
         "string");
  assert(std::get<std::int64_t>(catalog_round.events.at(0).attributes.at(0).value) == 3);
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
