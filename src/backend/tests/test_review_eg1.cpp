// Review EG1: pm4py 2.7.22 on OCEL logs holding NaT event times (an OCEL
// 2.0 SQLite time '' or NULL, which native keeps as an empty text and sends
// on in transports marked "minera:unreadableEventTimes"), and the OCEL 1.0
// XML vmap ocel:* entries. Observed black-box (review-harness round-eg,
// nat/ probes and rt/rsw.py route sweeps with READER=sqlite: 1 of 7,106
// route calls on 187 SQLite files with NaT times in seven shapes differs,
// a declared writer coercion; xv/ probes and the round-ED XML fuzz files):
//
// - the descriptions sort a NaT time after every time and print it "NaT";
//   an object's lifecycle leaves NaT times out, and an object whose events
//   all have one raises "NaTType does not support timestamp" before any
//   lookup (with timestamps or without); so does the LLM OCEL context;
// - the compatibility event features raise on any NaT time (timestamp
//   columns come first), the object features on an object whose events all
//   have one (lifecycle columns come first);
// - the OC-DFG measures a pair with a NaT end as 0;
// - the ancestors/descendants split gives no direction to a pair of
//   interacting objects when either starts at NaT;
// - write_ocel_xml leaves a NaT time out, write_ocel2_xml writes "NaT",
//   write_ocel_json leaves it out of the event entry, write_ocel2_json raises
//   KeyError 'ocel:timestamp', write_ocel2_sqlite raises KeyError
//   'ocel_time' when every event of a type has one (else NULL);
// - read_ocel_xml takes a vmap ocel:eid/ocel:activity/ocel:timestamp entry
//   into the events frame while the relations frame keeps the event's own
//   value: an entry equal to the own value changes nothing, an empty or
//   missing id or activity, or an id no relation names, drops the row (its
//   relations go, the objects they name stay); other entries whose row
//   remains are refused (declared).
//
// See docs/reference-2.7.22-parity.md, Review EG1.
#include "minera/llm_context.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <algorithm>
#include <filesystem>
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

const std::string kNat = "ValueError: NaTType does not support timestamp";

std::string event(const std::string& id, const std::string& activity, const std::string& time,
                  const std::string& omap) {
  return "\"" + id + "\":{\"ocel:activity\":\"" + activity + "\",\"ocel:timestamp\":\"" + time +
         "\",\"ocel:omap\":[" + omap + "],\"ocel:vmap\":{}}";
}

std::string object(const std::string& id, const std::string& type) {
  return "\"" + id + "\":{\"ocel:type\":\"" + type + "\",\"ocel:ovmap\":{}}";
}

// A transport as a route sends on an OCEL 2.0 SQLite import: events in the
// reader's order (times sorted, NaT last), a NaT time as an empty text.
Ocel transport(const std::vector<std::string>& events, const std::vector<std::string>& objects) {
  std::string json =
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{},"
      "\"ocel:events\":{";
  for (std::size_t i = 0; i < events.size(); ++i) json += (i ? "," : "") + events[i];
  json += "},\"ocel:objects\":{";
  for (std::size_t i = 0; i < objects.size(); ++i) json += (i ? "," : "") + objects[i];
  return parse_ocel_json(json + "},\"minera:unreadableEventTimes\":true}");
}

// e5 a 09:00 (o1), e7 c 09:30 (o3), e1 a 10:00 (o1 o2), e3 c 11:00 (o2),
// then NaT: e6 a (o3 o1), e2 b (o1), e4 b (o2 o3).
Ocel mixed() {
  return transport({event("e5", "a", "2024-01-01T09:00:00+00:00", "\"o1\""),
                    event("e7", "c", "2024-01-01T09:30:00+00:00", "\"o3\""),
                    event("e1", "a", "2024-01-01T10:00:00+00:00", "\"o1\",\"o2\""),
                    event("e3", "c", "2024-01-01T11:00:00+00:00", "\"o2\""),
                    event("e2", "b", "", "\"o1\""), event("e6", "a", "", "\"o3\",\"o1\""),
                    event("e4", "b", "", "\"o2\",\"o3\"")},
                   {object("o1", "t"), object("o2", "t"), object("o3", "u")});
}

// o3's only event e4 has a NaT time.
Ocel nat_only_object() {
  return transport({event("e5", "a", "2024-01-01T09:00:00+00:00", "\"o1\""),
                    event("e1", "a", "2024-01-01T10:00:00+00:00", "\"o1\",\"o2\""),
                    event("e3", "c", "2024-01-01T11:00:00+00:00", "\"o2\""),
                    event("e2", "b", "", "\"o1\""), event("e4", "b", "", "\"o3\"")},
                   {object("o1", "t"), object("o2", "t"), object("o3", "u")});
}

void descriptions() {
  const auto log = mixed();
  const auto text = serialize_ocel_description_json(log, OcelDescriptionVariant::kVariant1);
  const std::string events =
      "a ( related objects: o1 )  timestamp: 2024-01-01 09:00:00+00:00\\n"
      "c ( related objects: o3 )  timestamp: 2024-01-01 09:30:00+00:00\\n"
      "a ( related objects: o1, o2 )  timestamp: 2024-01-01 10:00:00+00:00\\n"
      "c ( related objects: o2 )  timestamp: 2024-01-01 11:00:00+00:00\\n"
      "a ( related objects: o1, o3 )  timestamp: NaT\\n"
      "b ( related objects: o1 )  timestamp: NaT\\n"
      "b ( related objects: o2, o3 )  timestamp: NaT";
  expect(text.find(events) != std::string::npos, "NaT events sort last by activity and id: " + text);
  const std::string objects =
      "o1 object type: t ( lifecycle start: 2024-01-01 09:00:00+00:00 ; lifecycle end: "
      "2024-01-01 10:00:00+00:00 ; lifecycle duration: 3600.0 )\\n"
      "o2 object type: t ( lifecycle start: 2024-01-01 10:00:00+00:00 ; lifecycle end: "
      "2024-01-01 11:00:00+00:00 ; lifecycle duration: 3600.0 )\\n"
      "o3 object type: u ( lifecycle start: 2024-01-01 09:30:00+00:00 ; lifecycle end: "
      "2024-01-01 09:30:00+00:00 ; lifecycle duration: 0.0 )";
  expect(text.find(objects) != std::string::npos, "lifecycles leave NaT times out: " + text);
  const auto tuples = describe_ocel_tuples(log);
  expect(tuples.events.size() == 7 && tuples.events[6].timestamp == "NaT",
         "variant2 prints NaT");

  const auto only = nat_only_object();
  for (const bool timestamps : {true, false}) {
    OcelDescriptionOptions options;
    options.include_timestamps = timestamps;
    const auto got = error_of([&] { (void)describe_ocel_tuples(only, options); });
    expect(got == kNat, "an object with only NaT events raises: " + got);
  }
}

void llm_context() {
  LlmContextOptions options;
  options.variant = "ocel";
  const auto text = build_llm_context(mixed(), options).text;
  expect(text.find("a ( related objects: o1, o3 )  timestamp: NaT\nb ( related objects: o1 )  timestamp: NaT") !=
             std::string::npos,
         "the LLM OCEL context sorts NaT last: " + text);
  expect(text.find("o3 object type: u ( lifecycle start: 2024-01-01 09:30:00+00:00") != std::string::npos,
         "the LLM OCEL context leaves NaT out of lifecycles: " + text);
  const auto got = error_of([&] { (void)build_llm_context(nat_only_object(), options); });
  expect(got == kNat, "the LLM OCEL context raises on an object with only NaT events: " + got);
}

std::string features_error(const Ocel& log, OcelFeatureVariant variant) {
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  if (variant == OcelFeatureVariant::kObjects) options.include_all_object_features = true;
  return error_of([&] { (void)extract_ocel_features(log, variant, options); });
}

void features() {
  expect(features_error(mixed(), OcelFeatureVariant::kEvents) == kNat,
         "event features raise on a NaT time: " + features_error(mixed(), OcelFeatureVariant::kEvents));
  expect(features_error(mixed(), OcelFeatureVariant::kObjects).empty(),
         "object features keep objects with some times: " +
             features_error(mixed(), OcelFeatureVariant::kObjects));
  expect(features_error(nat_only_object(), OcelFeatureVariant::kObjects) == kNat,
         "object features raise on an object with only NaT events: " +
             features_error(nat_only_object(), OcelFeatureVariant::kObjects));
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  options.include_all_object_features = true;
  const auto table = extract_ocel_features(mixed(), OcelFeatureVariant::kObjects, options);
  const auto column = std::find(table.feature_names.begin(), table.feature_names.end(),
                                "@@object_lifecycle_duration") - table.feature_names.begin();
  expect(static_cast<std::size_t>(column) < table.feature_names.size() && table.values[0][column] == 3600.0,
         "o1 lasts 09:00 to 10:00");
}

void ocdfg() {
  const auto graph = discover_ocdfg_classic(mixed(), true);
  const auto& t = graph.edges.at("t");
  // o1: e5 a, e1 a, e2 b (NaT), e6 a (NaT); o2: e1 a, e3 c, e4 b (NaT).
  const auto perf = [&](const char* from, const char* to) {
    return t.at({from, to}).performance_event_couples;
  };
  const auto shown = [](const std::vector<double>& values) {
    std::string out;
    for (const auto value : values) out += std::to_string(value) + ",";
    return out;
  };
  expect(perf("a", "a") == std::vector<double>({3600.0}), "a->a: e5..e1 measures 3600: " + shown(perf("a", "a")));
  expect(perf("a", "b") == std::vector<double>({0.0}), "a->b with a NaT end measures 0: " + shown(perf("a", "b")));
  expect(perf("b", "a") == std::vector<double>({0.0}), "b->a between NaT times measures 0: " + shown(perf("b", "a")));
  expect(perf("c", "b") == std::vector<double>({0.0}), "c->b with a NaT end measures 0");
}

void split() {
  // o1 (t) starts known; o2 (u) has only NaT events and interacts with o1
  // and o3 (u, known): no direction either way, so each projection holds
  // its centre alone.
  const auto log = transport({event("e1", "a", "2024-01-01T10:00:00+00:00", "\"o1\""),
                              event("e3", "c", "2024-01-01T12:00:00+00:00", "\"o3\""),
                              event("e2", "b", "", "\"o1\",\"o2\""), event("e4", "c", "", "\"o2\",\"o3\"")},
                             {object("o1", "t"), object("o2", "u"), object("o3", "u")});
  const auto events_of = [](const Ocel& part) {
    std::vector<std::string> ids;
    for (const auto& item : part.events) ids.push_back(item.id);
    std::sort(ids.begin(), ids.end());
    std::string joined;
    for (const auto& id : ids) joined += id + ",";
    return joined;
  };
  OcelSplitOptions options;
  options.variant = OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "t";
  auto result = split_ocel(log, options);
  expect(result.logs.size() == 1 && events_of(result.logs[0]) == "e1,e2,", "centre o1 alone");
  options.object_type = "u";
  result = split_ocel(log, options);
  expect(result.logs.size() == 2 && events_of(result.logs[0]) == "e2,e4," &&
             events_of(result.logs[1]) == "e3,e4,",
         "centres o2 and o3 alone");
}

void writers() {
  const auto log = mixed();
  OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto xml = serialize_ocel_xml(log, classic);
  const auto e2 = xml.find("<string key=\"id\" value=\"e2\"/>");
  const std::string untimed =
      "<string key=\"id\" value=\"e2\"/>\n      <string key=\"activity\" value=\"b\"/>\n      <list";
  expect(e2 != std::string::npos && xml.compare(e2, untimed.size(), untimed) == 0,
         "write_ocel_xml leaves a NaT time out: " + (e2 == std::string::npos ? xml : xml.substr(e2, 160)));
  OcelXmlExportOptions standard;
  standard.format = OcelXmlFormat::kStandard;
  const auto xml2 = serialize_ocel_xml(log, standard);
  expect(xml2.find("<event id=\"e2\" type=\"b\" time=\"NaT\">") != std::string::npos,
         "write_ocel2_xml writes NaT");
  const auto download = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(download.find("\"e2\":{\"ocel:activity\":\"b\",\"ocel:omap\"") != std::string::npos,
         "write_ocel_json leaves a NaT time out of the event entry");
  const auto kept = serialize_ocel_json(log);
  expect(kept.find("\"e2\":{\"ocel:activity\":\"b\",\"ocel:timestamp\":\"\"") != std::string::npos &&
             parse_ocel_json(kept).events.size() == 7,
         "the transport keeps the empty time");
  const auto standard_json = error_of([&] { (void)serialize_ocel_json(log, OcelJsonFormat::kStandard, false); });
  expect(standard_json == "KeyError: 'ocel:timestamp'", "write_ocel2_json raises: " + standard_json);
  expect(error_of([&] { (void)serialize_ocel_json(log, OcelJsonFormat::kStandard); }).empty(),
         "a route's open source view keeps working");
#if defined(MINERA_HAS_SQLITE)
  const auto path = std::filesystem::temp_directory_path() / "minera_review_eg1.sqlite";
  // Activity b has only NaT times: its type table loses the time column.
  const auto sqlite = error_of([&] { export_ocel_sqlite(log, path); });
  expect(sqlite == "KeyError: 'ocel_time'", "write_ocel2_sqlite raises: " + sqlite);
  // e2 b keeps a known time beside the NaT one: written, read back as NaT.
  const auto some = transport({event("e1", "a", "2024-01-01T10:00:00+00:00", "\"o1\""),
                               event("e3", "b", "2024-01-01T12:00:00+00:00", "\"o1\""),
                               event("e2", "b", "", "\"o1\",\"o2\"")},
                              {object("o1", "t"), object("o2", "t")});
  const auto written = error_of([&] { export_ocel_sqlite(some, path); });
  expect(written.empty(), "a type with a known time is written: " + written);
  if (written.empty()) {
    const auto back = import_ocel_sqlite(path);
    expect(back.events.size() == 3 && back.events[2].id == "e2" && back.events[2].timestamp.empty(),
           "the NaT time reads back as NaT, last");
  }
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
#endif
}

std::string xml_doc(const std::string& events, const std::string& objects) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log><global scope=\"event\"><string key=\"activity\" "
         "value=\"__INVALID__\"/></global><global scope=\"object\"><string key=\"type\" "
         "value=\"__INVALID__\"/></global><global scope=\"log\"><list key=\"attribute-names\"/><list "
         "key=\"object-types\"><string key=\"object-type\" value=\"t\"/></list><string key=\"version\" "
         "value=\"1.0\"/><string key=\"ordering\" value=\"timestamp\"/></global><events>" +
         events + "</events><objects>" + objects + "</objects></log>";
}

std::string xml_event(const std::string& id, const std::string& time, const std::string& object_id,
                      const std::string& vmap) {
  return "<event><string key=\"id\" value=\"" + id +
         "\"/><string key=\"activity\" value=\"a\"/><date key=\"timestamp\" value=\"" + time +
         "\"/><list key=\"omap\">" +
         (object_id.empty() ? "" : "<string key=\"object-id\" value=\"" + object_id + "\"/>") +
         "</list><list key=\"vmap\">" + vmap + "</list></event>";
}

std::string xml_object(const std::string& id) {
  return "<object><string key=\"id\" value=\"" + id + "\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object>";
}

std::string summary(const Ocel& log) {
  std::string out;
  for (const auto& item : log.events) out += item.id + ":" + item.activity + ",";
  out += "|";
  for (const auto& item : log.objects) out += item.id + ",";
  out += "|";
  for (const auto& item : log.relations) out += item.event_id + "-" + item.object_id + ",";
  out += "|";
  for (const auto& item : log.events) out += std::to_string(item.attributes.size());
  return out;
}

std::string read_xml(const std::string& vmap, const std::string& second_object = "o1",
                     const std::string& first_object = "o1") {
  try {
    return summary(parse_ocel_xml(xml_doc(
        xml_event("e1", "2024-01-01T10:00:00", first_object, vmap) +
            xml_event("e2", "2024-01-02T10:00:00", second_object, ""),
        xml_object("o1") + (second_object == "o2" ? xml_object("o2") : ""))));
  } catch (const std::exception& error) {
    return error.what();
  }
}

void xml_overrides() {
  const auto entry = [](const std::string& key, const std::string& value, const char* tag = "string") {
    return "<" + std::string(tag) + " key=\"" + key + "\" value=\"" + value + "\"/>";
  };
  const std::string both = "e1:a,e2:a,|o1,|e1-o1,e2-o1,|00";
  expect(read_xml(entry("ocel:activity", "a")) == both, "an equal activity: " + read_xml(entry("ocel:activity", "a")));
  expect(read_xml(entry("ocel:eid", "e1")) == both, "an equal id: " + read_xml(entry("ocel:eid", "e1")));
  expect(read_xml(entry("ocel:timestamp", "2024-01-01T10:00:00", "date")) == both,
         "an equal date: " + read_xml(entry("ocel:timestamp", "2024-01-01T10:00:00", "date")));
  const std::string second = "e2:a,|o1,|e2-o1,|0";
  expect(read_xml(entry("ocel:eid", "zz")) == second, "an id no relation names: " + read_xml(entry("ocel:eid", "zz")));
  expect(read_xml(entry("ocel:eid", "")) == second, "an empty id: " + read_xml(entry("ocel:eid", "")));
  expect(read_xml(entry("ocel:activity", "")) == second, "an empty activity: " + read_xml(entry("ocel:activity", "")));
  // o1's relation goes with e1, o1 stays (named by it when the objects frame
  // was filtered).
  expect(read_xml(entry("ocel:eid", "zz"), "o2") == "e2:a,|o1,o2,|e2-o2,|0",
         "the objects a dropped row's relations name stay: " + read_xml(entry("ocel:eid", "zz"), "o2"));
  // the only event dropped: nothing stays.
  const auto alone = summary(parse_ocel_xml(xml_doc(
      xml_event("e1", "2024-01-01T10:00:00", "o1", entry("ocel:eid", "zz")), xml_object("o1"))));
  expect(alone == "|||", "no relation left: " + alone);
  const std::string held =
      ": pm4py puts it in the events frame and keeps the event's own value in the relations frame, "
      "which a native OCEL cannot hold";
  expect(read_xml(entry("ocel:activity", "z")) == "OCEL XML event vmap entry ocel:activity" + held,
         "declared: another activity: " + read_xml(entry("ocel:activity", "z")));
  expect(read_xml(entry("ocel:timestamp", "2024-01-05T10:00:00", "date")) ==
             "OCEL XML event vmap entry ocel:timestamp" + held,
         "declared: another date");
  expect(read_xml(entry("ocel:eid", "e2")).find("next to the rows it names") != std::string::npos,
         "declared: an id naming another event: " + read_xml(entry("ocel:eid", "e2")));
  // a text time turns the frame's time column into Python objects, even on
  // a dropped row (declared); an activity on a dropped row changes nothing.
  expect(read_xml(entry("ocel:timestamp", "zz"), "o1", "") == "OCEL XML event vmap entry ocel:timestamp" + held,
         "declared: a text time");
  expect(read_xml(entry("ocel:activity", "z"), "o1", "") == second,
         "an activity on a row without relations: " + read_xml(entry("ocel:activity", "z"), "o1", ""));
}

}  // namespace

int main() {
  descriptions();
  llm_context();
  features();
  ocdfg();
  split();
  writers();
  xml_overrides();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EG1 tests passed\n";
  return 0;
}
