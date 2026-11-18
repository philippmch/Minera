// Review EF1: pm4py 2.7.22 on OCEL logs holding events or objects without
// relations, and on the objects frame of an OCEL CSV without an object
// table. Observed black-box (review-harness round-ef, un/un1.py-un6.py
// probes and rt/rsw.py route sweeps, 0 differences over 19,000 route calls
// on 500 fuzzed classic JSON files besides declared ones):
//
// - a classic JSON whose objects frame drops an event's only object (an
//   ovmap ocel:type "") or whose events frame drops an object's only event
//   (a vmap ocel:activity ""), and a CSV row naming no object, leave an
//   event or an object without relations;
// - both descriptions raise KeyError on the first such event in (time,
//   activity, id) order, and with timestamps on the first such object by id,
//   after the events;
// - one instant written two ways ("Z" and "+00:00") is one time when the
//   description orders its events;
// - the compatibility event features raise KeyError on the first such event
//   for the relation counts, else on the first such object (objects-frame
//   order) for the endpoint types; the object features raise it on the first
//   event for descendants, cobirth and the connected-object graph, on the
//   last for inheritance and codeath; enrichment with the object graphs
//   fails too;
// - the flattening leaves a NaN attribute out (pandas' missing value) and
//   keeps an infinity a float;
// - read_ocel_csv without an object table lists the objects type by type in
//   the order of the type columns (set iteration within a type, so native
//   keeps the first appearance there), which orders the per-type feature
//   columns.
//
// See docs/reference-2.7.22-parity.md, Review EF1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_transform.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

std::string event(const std::string& id, const std::string& activity, const std::string& time,
                  const std::string& omap, const std::string& vmap = "{}") {
  return "\"" + id + "\":{\"ocel:activity\":\"" + activity + "\",\"ocel:timestamp\":\"" + time +
         "\",\"ocel:omap\":[" + omap + "],\"ocel:vmap\":" + vmap + "}";
}

std::string object(const std::string& id, const std::string& type,
                   const std::string& ovmap = "{}") {
  return "\"" + id + "\":{\"ocel:type\":\"" + type + "\",\"ocel:ovmap\":" + ovmap + "}";
}

Ocel classic(const std::vector<std::string>& events, const std::vector<std::string>& objects) {
  std::string json =
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{},"
      "\"ocel:events\":{";
  for (std::size_t i = 0; i < events.size(); ++i) json += (i ? "," : "") + events[i];
  json += "},\"ocel:objects\":{";
  for (std::size_t i = 0; i < objects.size(); ++i) json += (i ? "," : "") + objects[i];
  return parse_ocel_json(json + "}}");
}

std::string describe_error(const Ocel& log, bool timestamps) {
  OcelDescriptionOptions options;
  options.include_timestamps = timestamps;
  return error_of([&] { (void)describe_ocel_tuples(log, options); });
}

void descriptions() {
  // e2's only object o2 is dropped by the objects frame; e4 too, later.
  const auto two = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                            event("e4", "b", "2024-01-01T13:00:00", "\"o2\""),
                            event("e2", "b", "2024-01-01T11:00:00", "\"o2\""),
                            event("e3", "c", "2024-01-01T12:00:00", "\"o1\"")},
                           {object("o1", "t"), object("o2", "t", "{\"ocel:type\":\"\"}")});
  expect(two.events.size() == 4 && two.objects.size() == 1 && two.relations.size() == 2,
         "the classic reader keeps e2 and e4 without relations");
  expect(describe_error(two, true) == "KeyError: 'e2'", "description: " + describe_error(two, true));
  expect(describe_error(two, false) == "KeyError: 'e2'",
         "description without timestamps: " + describe_error(two, false));
  // A time tie goes to the activity, then the id; file order does not count.
  const auto tie = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                            event("e2", "z", "2024-01-01T11:00:00", "\"o2\""),
                            event("e9", "b", "2024-01-01T11:00:00", "\"o2\"")},
                           {object("o1", "t"), object("o2", "t", "{\"ocel:type\":\"\"}")});
  expect(describe_error(tie, true) == "KeyError: 'e9'", "tie on time: " + describe_error(tie, true));
  const auto later_id = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                                 event("e9", "b", "2024-01-01T11:00:00", "\"o2\""),
                                 event("e2", "b", "2024-01-01T11:00:00", "\"o2\"")},
                                {object("o1", "t"), object("o2", "t", "{\"ocel:type\":\"\"}")});
  expect(describe_error(later_id, true) == "KeyError: 'e2'",
         "tie on time and activity: " + describe_error(later_id, true));
  // o2's only event e2 is dropped by the events frame; o2 stays, unrelated.
  const auto objects = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                                event("e2", "b", "2024-01-01T11:00:00", "\"o9\",\"o2\"",
                                      "{\"ocel:activity\":\"\"}")},
                               {object("o1", "t"), object("o9", "u"), object("o2", "u")});
  expect(describe_error(objects, true) == "KeyError: 'o2'",
         "the first object without events by id: " + describe_error(objects, true));
  expect(describe_error(objects, false).empty(),
         "without timestamps objects without events are listed: " + describe_error(objects, false));
  // An unrelated event comes before an unrelated object.
  const auto both = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\",\"o2\""),
                             event("e2", "b", "2024-01-01T11:00:00", "\"o3\"",
                                   "{\"ocel:activity\":null}"),
                             event("e3", "c", "2024-01-01T12:00:00", "\"o4\"")},
                            {object("o1", "t"), object("o2", "u"), object("o3", "u"),
                             object("o4", "u", "{\"ocel:type\":\"\"}")});
  expect(describe_error(both, true) == "KeyError: 'e3'", "events first: " + describe_error(both, true));
  // One instant spelled with "Z" and "+00:00" is one time: the activity
  // orders the two events.
  const auto spelled = classic({event("e1", "b", "2024-01-01T11:00:00+00:00", "\"o1\""),
                                event("e2", "a", "2024-01-01T11:00:00Z", "\"o2\""),
                                event("e3", "c", "2024-01-01T10:00:00Z", "\"o1\"")},
                               {object("o1", "t"), object("o2", "t")});
  const auto description = describe_ocel_tuples(spelled);
  std::string order;
  for (const auto& item : description.events) order += item.activity;
  expect(order == "cab", "one instant spelled two ways: " + order);
}

std::string features_error(const Ocel& log, OcelFeatureVariant variant,
                           const std::function<void(OcelFeatureOptions&)>& set) {
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  set(options);
  return error_of([&] { (void)extract_ocel_features(log, variant, options); });
}

void features_and_enrichment() {
  const auto two = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                            event("e4", "b", "2024-01-01T13:00:00", "\"o2\""),
                            event("e2", "b", "2024-01-01T11:00:00", "\"o2\""),
                            event("e3", "c", "2024-01-01T12:00:00", "\"o1\"")},
                           {object("o1", "t"), object("o2", "t", "{\"ocel:type\":\"\"}")});
  const auto none = [](OcelFeatureOptions&) {};
  expect(features_error(two, OcelFeatureVariant::kEvents, none) == "KeyError: 'e2'",
         "event relation counts: " + features_error(two, OcelFeatureVariant::kEvents, none));
  const auto only = [](const char* which) {
    return [which](OcelFeatureOptions& options) {
      options.include_all_object_features = false;
      options.include_obj_con_in_graph_features = false;
      const std::string name = which;
      if (name == "descendants") options.include_general_descendants_features = true;
      if (name == "inheritance") options.include_general_inheritance_features = true;
      if (name == "cobirth") options.include_cobirth_features = true;
      if (name == "codeath") options.include_codeath_features = true;
      if (name == "connected") options.include_obj_con_in_graph_features = true;
    };
  };
  const std::vector<std::pair<const char*, const char*>> graphs = {
      {"descendants", "KeyError: 'e2'"}, {"inheritance", "KeyError: 'e4'"},
      {"cobirth", "KeyError: 'e2'"}, {"codeath", "KeyError: 'e4'"},
      {"connected", "KeyError: 'e2'"}};
  for (const auto& [name, want] : graphs) {
    const auto got = features_error(two, OcelFeatureVariant::kObjects, only(name));
    expect(got == want, std::string("object features, ") + name + ": " + got);
  }
  expect(features_error(two, OcelFeatureVariant::kObjects, only("")).empty(),
         "object features without the graph families: " +
             features_error(two, OcelFeatureVariant::kObjects, only("")));
  OcelEnrichmentOptions all_graphs;
  expect(!error_of([&] { (void)enrich_ocel(two, all_graphs); }).empty(),
         "enrichment with the object graphs fails on an unrelated event");
  OcelEnrichmentOptions lifecycle_only;
  lifecycle_only.include_interaction = lifecycle_only.include_descendants =
      lifecycle_only.include_inheritance = lifecycle_only.include_cobirth =
          lifecycle_only.include_codeath = false;
  expect(error_of([&] { (void)enrich_ocel(two, lifecycle_only); }).empty(),
         "lifecycle enrichment alone keeps working");

  // An object without events: the endpoint types raise on the first one in
  // objects-frame order; the relation counts do not.
  const auto objects = classic({event("e1", "a", "2024-01-01T10:00:00", "\"o1\""),
                                event("e2", "b", "2024-01-01T11:00:00", "\"o9\",\"o2\"",
                                      "{\"ocel:activity\":\"\"}")},
                               {object("o1", "t"), object("o9", "u"), object("o2", "u")});
  expect(features_error(objects, OcelFeatureVariant::kEvents, none) == "KeyError: 'o9'",
         "event endpoint types: " + features_error(objects, OcelFeatureVariant::kEvents, none));
  const auto counts_only = [](OcelFeatureOptions& options) { options.include_endpoint_features = false; };
  expect(features_error(objects, OcelFeatureVariant::kEvents, counts_only).empty(),
         "relation counts alone: " + features_error(objects, OcelFeatureVariant::kEvents, counts_only));
  expect(features_error(objects, OcelFeatureVariant::kObjects, none).empty(),
         "object features on an object without events: " +
             features_error(objects, OcelFeatureVariant::kObjects, none));
}

void flattening() {
  const auto log = parse_ocel_json(
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{},"
      "\"ocel:events\":{\"e1\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":\"2024-01-01T10:00:00\","
      "\"ocel:omap\":[\"o1\"],\"ocel:vmap\":{\"x\":Infinity,\"y\":NaN}},"
      "\"e2\":{\"ocel:activity\":\"b\",\"ocel:timestamp\":\"2024-01-01T11:00:00\","
      "\"ocel:omap\":[\"o1\"],\"ocel:vmap\":{\"x\":1.5,\"y\":-Infinity}}},"
      "\"ocel:objects\":{\"o1\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{}}}}");
  const auto flat = flatten_ocel_to_event_log(log, "t");
  expect(flat.traces.size() == 1 && flat.traces[0].events.size() == 2, "one trace of two events");
  if (flat.traces.size() != 1 || flat.traces[0].events.size() != 2) return;
  const auto find = [](const Event& e, const std::string& key) -> const EventAttribute* {
    for (const auto& attribute : e.attributes)
      if (attribute.key == key) return &attribute;
    return nullptr;
  };
  const auto& first = flat.traces[0].events[0];
  const auto& second = flat.traces[0].events[1];
  const auto* x = find(first, "x");
  expect(x && x->kind == AttributeKind::kFloat && x->value == "inf", "an infinity stays a float");
  expect(find(first, "y") == nullptr, "a NaN cell is left out");
  const auto* y = find(second, "y");
  expect(y && y->kind == AttributeKind::kFloat && y->value == "-inf", "a negative infinity");
}

void csv_objects() {
  const std::string table =
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:u\n"
      "e0,2024-03-02 03:42:20,d,,\"['u2','u1']\"\n"
      "e1,2024-03-01 03:42:20,a,\"['o2']\",\"['u3']\"\n"
      "e2,2024-03-02 05:42:20,a,\"['o1']\",\"['u1']\"\n";
  const auto log = parse_ocel_csv(table);
  std::string ids, types;
  for (const auto& o : log.objects) {
    ids += o.id + " ";
    types += o.type;
  }
  expect(types == "ttuuu", "objects type by type in type-column order: " + types);
  expect(ids == "o2 o1 u2 u1 u3 ", "first appearance within a type: " + ids);
  const std::string swapped =
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:u,ocel:type:t\n"
      "e0,2024-03-02 03:42:20,d,\"['u2','u1']\",\n"
      "e1,2024-03-01 03:42:20,a,\"['u3']\",\"['o2']\"\n";
  types.clear();
  for (const auto& o : parse_ocel_csv(swapped).objects) types += o.type;
  expect(types == "uuut", "the column order decides: " + types);

  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  const std::string first_u =
      "ocel:eid,ocel:timestamp,ocel:activity,x,ocel:type:t,ocel:type:u\n"
      "e0,2024-03-02 03:42:20,d,2.5,,\"['u1']\"\n"
      "e1,2024-03-03 03:42:20,a,s,\"['o1']\",\n"
      "e3,2024-03-05 03:42:20,b,s,\"['o1']\",\"['u1']\"\n";
  const auto table_features =
      extract_ocel_features(parse_ocel_csv(first_u), OcelFeatureVariant::kEvents, options);
  const auto& names = table_features.feature_names;
  const auto t = std::find(names.begin(), names.end(), "@@event_num_rel_objs_type_t");
  const auto u = std::find(names.begin(), names.end(), "@@event_num_rel_objs_type_u");
  expect(t != names.end() && u != names.end() && t < u,
         "per-type relation counts follow the type columns");
}

}  // namespace

int main() {
  descriptions();
  features_and_enrichment();
  flattening();
  csv_objects();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EF1 tests passed\n";
  return 0;
}
