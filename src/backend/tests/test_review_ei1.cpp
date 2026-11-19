// Review EI1: pm4py 2.7.22 on NaT event times and a time column of None
// through the LLM abstractions, OC-DFG conformance and the object and event
// feature families, and the OCEL 2.0 JSON and XML readers on unreadable
// event times. Observed black-box (review-harness round-ei: rt/esw.py
// sweeps of csv/s31, csv/m22, x33 and sq37, o2/sfz.py and o2/sfz2.py reader
// fuzz, p3/ hand probes):
//
// - pm4py.llm.abstract_ocel is the OCEL description with timestamps: the
//   times print in the frame's shared offset of an OCEL CSV import (native
//   printed UTC), NaT times print "NaT" and stay out of the lifecycles, an
//   event without relations raises KeyError, an object whose events all
//   have NaT times raises the NaT error, with or without timestamps;
// - abstract_ocel_ocdfg discovers the OC-DFG with its edge performance
//   before it looks at include_header, so a time column of None raises the
//   TypeError of the performance also without the header;
// - conformance_ocdfg discovers the log's OC-DFG with that performance too;
// - the feature families raise in pm4py's order: the work-in-progress and
//   neighbor (obj_con_in_graph) families take the default object features,
//   whose lifecycle durations raise on an object with NaT times only before
//   the graphs look up an event without relations;
// - read_ocel2_json walks eventTypes, objectTypes, events (type, time,
//   attributes, relationships with a required qualifier, id) and objects
//   before it reads the times, so a missing key raises KeyError first and a
//   time that is no str raises AttributeError on .endswith in event order
//   with fromisoformat's ValueError;
// - read_ocel2_xml on an event without a time attribute raises dateutil's
//   TypeError on None.
//
// See docs/reference-2.7.22-parity.md, Review EI1.
#include "minera/llm_abstractions.hpp"
#include "minera/llm_context.hpp"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_xml.hpp"

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
  } catch (const OcdfgPerformanceError& error) {
    return error.type + ": " + error.what();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

const std::string kHeader = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n";
// o1 has e1, e2 (NaT) and e4; o2 has e3 (NaT) and e4.
const std::string kNatCsv = kHeader +
                            "e1,2024-01-01 10:00:00+02:00,a,['o1']\n"
                            "e2,,b,['o1']\n"
                            "e3,NaT,c,['o2']\n"
                            "e4,2024-01-01 09:00:00+02:00,d,\"['o1', 'o2']\"\n";
// e5 has no relation.
const std::string kUnrelatedCsv = kNatCsv + "e5,2024-01-01 11:00:00+02:00,x,[]\n";
// o2 has NaT times only, e3 no relation.
const std::string kNatObjectCsv = kHeader +
                                  "e1,2024-01-01 10:00:00+02:00,a,['o1']\n"
                                  "e2,,b,['o2']\n"
                                  "e3,2024-01-01 11:00:00+02:00,x,[]\n";

const std::string kNat = "ValueError: NaTType does not support timestamp";
const std::string kNoneMinus = "TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'";

std::string llm_ocel(const Ocel& log, bool timestamps) {
  LlmContextOptions options;
  options.variant = "ocel";
  options.include_timestamps = timestamps;
  options.max_len = 1 << 20;
  return build_llm_context(log, options).text;
}

void llm_ocel_context() {
  const auto log = parse_ocel_csv(kNatCsv);
  expect(llm_ocel(log, true) ==
             "\n\nevents:\n\n"
             "d ( related objects: o1, o2 )  timestamp: 2024-01-01 09:00:00+02:00\n"
             "a ( related objects: o1 )  timestamp: 2024-01-01 10:00:00+02:00\n"
             "b ( related objects: o1 )  timestamp: NaT\n"
             "c ( related objects: o2 )  timestamp: NaT\n"
             "\nobjects:\n\n"
             "o1 object type: order ( lifecycle start: 2024-01-01 09:00:00+02:00 ; lifecycle end: "
             "2024-01-01 10:00:00+02:00 ; lifecycle duration: 3600.0 )\n"
             "o2 object type: order ( lifecycle start: 2024-01-01 09:00:00+02:00 ; lifecycle end: "
             "2024-01-01 09:00:00+02:00 ; lifecycle duration: 0.0 )",
         "abstract_ocel in the shared offset:\n" + llm_ocel(log, true));
  expect(llm_ocel(log, false) ==
             "\n\nevents:\n\n"
             "d ( related objects: o1, o2 ) \n"
             "a ( related objects: o1 ) \n"
             "b ( related objects: o1 ) \n"
             "c ( related objects: o2 ) \n"
             "\nobjects:\n\n"
             "o1 object type: order\n"
             "o2 object type: order",
         "abstract_ocel without timestamps:\n" + llm_ocel(log, false));
  const auto unrelated = parse_ocel_csv(kUnrelatedCsv);
  for (const bool timestamps : {true, false})
    expect(error_of([&] { (void)llm_ocel(unrelated, timestamps); }) == "KeyError: 'e5'",
           "abstract_ocel on an event without relations");
  const auto nat_object = parse_ocel_csv(kNatObjectCsv);
  for (const bool timestamps : {true, false})
    expect(error_of([&] { (void)llm_ocel(nat_object, timestamps); }) == kNat,
           "abstract_ocel on an object with NaT times only");
}

void llm_ocel_features() {
  expect(error_of([&] { (void)llm_ocel_features_description(parse_ocel_csv(kUnrelatedCsv), "order"); }) ==
             "KeyError: 'e5'",
         "abstract_ocel_features on an event without relations");
  expect(error_of([&] { (void)llm_ocel_features_description(parse_ocel_csv(kNatObjectCsv), "order"); }) == kNat,
         "abstract_ocel_features: the NaT lifecycle raises before the KeyError");
  expect(error_of([&] { (void)llm_ocel_features_description(parse_ocel_csv(kNatCsv), "order"); }).empty(),
         "abstract_ocel_features with NaT times besides valid ones");
}

// An OCEL 1.0 XML file without any timestamp element: a time column of None.
const std::string kNoneXml =
    "<?xml version='1.0' encoding='UTF-8'?>\n<log>\n"
    "<global scope=\"event\"><string key=\"ocel:activity\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"object\"><string key=\"ocel:type\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"><string "
    "key=\"object-type\" value=\"t\"/></list><string key=\"version\" value=\"1.0\"/><string "
    "key=\"ordering\" value=\"timestamp\"/></global>\n<events>\n"
    "<event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/><list "
    "key=\"omap\"><string key=\"object-id\" value=\"o1\"/><string key=\"object-id\" value=\"o2\"/>"
    "</list><list key=\"vmap\"/></event>\n"
    "<event><string key=\"id\" value=\"e2\"/><string key=\"activity\" value=\"b\"/><list "
    "key=\"omap\"><string key=\"object-id\" value=\"o1\"/></list><list key=\"vmap\"/></event>\n"
    "</events>\n<objects>\n"
    "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object>\n"
    "<object><string key=\"id\" value=\"o2\"/><string key=\"type\" value=\"u\"/><list key=\"ovmap\"/></object>\n"
    "</objects>\n</log>\n";

void none_time_ocdfg() {
  const auto log = parse_ocel_xml(kNoneXml);
  expect(log.none_event_times, "a file without timestamps holds None times");
  for (const bool header : {true, false}) {
    LlmAbstractionOptions options;
    options.include_header = header;
    expect(error_of([&] { (void)llm_ocel_ocdfg_description(log, options); }) == kNoneMinus,
           std::string("abstract_ocel_ocdfg on None times, header ") + (header ? "on" : "off"));
  }
  const auto good = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e1,2024-01-01 10:00:00,a,\"['o1']\"\ne2,2024-01-01 11:00:00,b,\"['o1']\"\n");
  const auto normative = discover_ocdfg_classic(good);
  expect(error_of([&] { (void)compare_ocdfg(log, normative); }) == kNoneMinus,
         "conformance_ocdfg of a None-times log");
  expect(error_of([&] { (void)compare_ocdfg(good, normative); }).empty(), "conformance_ocdfg of valid times");
}

OcelFeatureOptions compatibility() {
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  return options;
}

void feature_family_order() {
  const auto nat_object = parse_ocel_csv(kNatObjectCsv);
  const auto unrelated = parse_ocel_csv(kUnrelatedCsv);
  // Events: the timestamp family raises before the relation counts look e3 up.
  expect(error_of([&] { (void)extract_ocel_features(nat_object, OcelFeatureVariant::kEvents, compatibility()); }) ==
             kNat,
         "events: NaT first");
  auto counts = compatibility();
  counts.include_timestamp_features = false;
  counts.include_endpoint_features = false;
  counts.include_attribute_counts = false;
  counts.include_interaction_features = false;
  expect(error_of([&] { (void)extract_ocel_features(nat_object, OcelFeatureVariant::kEvents, counts); }) ==
             "KeyError: 'e3'",
         "events without timestamps: KeyError");
  expect(error_of([&] { (void)extract_ocel_features(unrelated, OcelFeatureVariant::kEvents, counts); }) ==
             "KeyError: 'e5'",
         "events without timestamps on e5: KeyError");
  // Objects: the neighbor family takes the default object features.
  auto neighbors = compatibility();
  neighbors.include_lifecycle_duration_features = false;
  neighbors.include_obj_con_in_graph_features = true;
  expect(error_of([&] { (void)extract_ocel_features(nat_object, OcelFeatureVariant::kObjects, neighbors); }) ==
             kNat,
         "obj_con_in_graph on an object with NaT times only");
  expect(error_of([&] { (void)extract_ocel_features(unrelated, OcelFeatureVariant::kObjects, neighbors); }) ==
             "KeyError: 'e5'",
         "obj_con_in_graph on an event without relations");
  // The work-in-progress family takes the lifecycle durations.
  auto wip = compatibility();
  wip.include_lifecycle_duration_features = false;
  wip.include_lifecycle_wip_features = true;
  expect(error_of([&] { (void)extract_ocel_features(nat_object, OcelFeatureVariant::kObjects, wip); }) == kNat,
         "WIP on an object with NaT times only");
  wip.include_interaction_features = false;
  expect(error_of([&] { (void)extract_ocel_features(parse_ocel_csv(kNatCsv), OcelFeatureVariant::kObjects, wip); })
             .empty(),
         "WIP with NaT times besides valid ones");
}

std::string ocel2_json(const std::string& events, const std::string& objects,
                       const std::string& types =
                           "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],"
                           "\"objectTypes\":[{\"name\":\"t\",\"attributes\":[]}],") {
  return "{" + types + "\"events\":[" + events + "],\"objects\":[" + objects + "]}";
}

std::string event(const std::string& id, const std::string& time = "\"2024-01-01T10:00:00Z\"",
                  const std::string& relation = "{\"objectId\":\"o1\",\"qualifier\":\"q\"}") {
  return "{\"id\":\"" + id + "\",\"type\":\"a\",\"time\":" + time + ",\"relationships\":[" + relation + "]}";
}

const std::string kObject = "{\"id\":\"o1\",\"type\":\"t\"}";

void ocel2_json_walk() {
  const auto read = [](const std::string& text) { return error_of([&] { (void)parse_ocel_json(text); }); };
  const std::string no_qualifier = "{\"objectId\":\"o1\"}";
  expect(read(ocel2_json(event("e1", "\"2024-01-01T10:00:00Z\"", no_qualifier), kObject)) == "KeyError: 'qualifier'",
         "a relationship without a qualifier");
  expect(read(ocel2_json(event("e1", "null") + "," + event("e2", "\"2024-01-01T10:00:00Z\"", no_qualifier),
                         kObject)) == "KeyError: 'qualifier'",
         "the walk raises before the times are read");
  expect(read(ocel2_json(event("e1", "5"), "{\"type\":\"t\"}")) == "KeyError: 'id'",
         "an object without id before an int time");
  expect(read(ocel2_json(event("e1") + "," + event("e2", "5"), kObject)) ==
             "'int' object has no attribute 'endswith'",
         "an int time");
  expect(read(ocel2_json(event("e1", "\"garbage\"") + "," + event("e2", "null"), kObject)) ==
             "Invalid isoformat string: 'garbage'",
         "an unreadable text before a null time");
  expect(read(ocel2_json(event("e1", "null") + "," + event("e2", "\"garbage\""), kObject)) ==
             "'NoneType' object has no attribute 'endswith'",
         "a null time before an unreadable text");
  expect(read(ocel2_json(event("e1"), "{\"id\":\"o1\",\"type\":\"t\",\"relationships\":[{\"objectId\":\"o1\"}]}")) ==
             "KeyError: 'qualifier'",
         "an object relationship without a qualifier");
  expect(read(ocel2_json("", "",
                         "\"eventTypes\":[{\"attributes\":[{\"name\":\"x\",\"type\":\"string\"}]}],"
                         "\"objectTypes\":[],")) == "KeyError: 'name'",
         "a type without a name");
  expect(read(ocel2_json("{\"id\":\"e1\",\"time\":\"2024-01-01T10:00:00Z\"}", kObject)) == "KeyError: 'type'",
         "an event without a type");
  expect(read(ocel2_json(event("e1"), kObject)).empty(), "a valid file");
}

void ocel2_xml_without_time() {
  const std::string xml =
      "<?xml version='1.0' encoding='UTF-8'?>\n"
      "<log><object-types><object-type name=\"t\"><attributes/></object-type></object-types>\n"
      "<event-types><event-type name=\"a\"><attributes/></event-type></event-types>\n"
      "<objects><object id=\"o1\" type=\"t\"><attributes/></object></objects>\n"
      "<events><event id=\"e1\" type=\"a\" time=\"2024-01-01T10:00:00Z\"><objects><relationship "
      "object-id=\"o1\" qualifier=\"q\"/></objects></event>\n"
      "<event id=\"e2\" type=\"a\"><objects><relationship object-id=\"o1\" qualifier=\"q\"/></objects></event>"
      "</events></log>";
  expect(error_of([&] { (void)parse_ocel_xml(xml); }) == "Parser must be a string or character stream, not NoneType",
         "OCEL 2.0 XML event without a time: " + error_of([&] { (void)parse_ocel_xml(xml); }));
}

}  // namespace

int main() {
  llm_ocel_context();
  llm_ocel_features();
  none_time_ocdfg();
  feature_family_order();
  ocel2_json_walk();
  ocel2_xml_without_time();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EI1 tests passed\n";
  return 0;
}
