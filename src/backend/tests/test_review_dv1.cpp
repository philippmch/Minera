// Review DV1: degenerate reference frames and the OCEL CSV timestamp formats.
// pm4py 2.7.22's read_ocel_csv builds the relations frame of a file whose rows
// name no object with only ocel:qualifier (float64), and both reference
// playouts that produce no event build events and objects frames without any
// column next to that relations frame; nearly every later function then
// raises KeyError on the first column it reads (a filter whose selection is
// empty returns an empty log first). read_ocel_csv also parses the timestamp
// column with pd.to_datetime, which infers one format from the first text,
// holds every later text to it and refuses mixed offsets. Observed black-box
// (review-harness round-dv, oc/deg.py, oc/v2ref.py, ts/model.py, ts/bank.py).
//
// With a bank path (docs/ocel-csv-timestamp-dv-bank.tsv.gz) the test also
// replays 4,000 fuzzed timestamp columns: texts joined by "|", a tab, then
// "ok" or the ValueError's text up to ". You might want to try:".
//
// Link analysis under PROPAGATE (round DS declared three calls): pm4py grows
// the sources' sets pass after pass by walking the linked pairs in the order
// of the self-merge with its sides swapped (in column with out column), not
// each source's targets together. Four CSV uploads of the DS larger-domain
// sweep (review-harness round-ds, pr/sw1.py, BIG=1 NMAX=30), every flag
// combination, pairs in pm4py's exact order (round-dv, pr/gen_bind.py).
#include "minera/csv_event_log_importer.hpp"
#include "minera/link_analysis.hpp"
#include "minera/oc_causal_net_reference_playout.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocpn_reference_playout.hpp"
#include "minera/saw_nets.hpp"

#include "../gzip_codec.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
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
  return {};
}

const std::string kNoObjects =
    "ocel:eid,ocel:timestamp,ocel:activity\n"
    "e1,2024-01-01 00:00:00,a\n"
    "e2,2024-01-02 00:00:00,b\n";
const std::string kHeaderOnly = "ocel:eid,ocel:timestamp,ocel:activity\n";
const std::string kObjects = "ocel:oid,ocel:type\no1,t\no2,t\n";

std::string key_error(const std::string& column) { return "KeyError: '" + column + "'"; }

struct Logs {
  Ocel no_objects;   // events, no object (csv_noobj)
  Ocel with_objects; // events and an object table (csv_noobj+ob)
  Ocel no_events;    // header only and an object table (csv_hdr+ob)
  Ocel playout;      // an OCPN playout without any event
};

Logs make_logs() {
  Logs logs;
  logs.no_objects = parse_ocel_csv(kNoObjects);
  logs.with_objects = parse_ocel_csv(kNoObjects, kObjects);
  logs.no_events = parse_ocel_csv(kHeaderOnly, kObjects);
  logs.playout = reference_ocpn_extensive_playout(discover_ocpn(Ocel{}));
  return logs;
}

void frames(const Logs& logs) {
  for (const auto* log : {&logs.no_objects, &logs.with_objects, &logs.no_events}) {
    expect(log->table_columns.relations &&
               *log->table_columns.relations == std::vector<std::string>{"ocel:qualifier"},
           "a CSV import naming no object has a qualifier-only relations frame");
    expect(!log->table_columns.events && !log->table_columns.objects,
           "its events and objects frames keep their columns");
    expect(ocel_relations_frame_degenerate(*log) && !ocel_events_frame_columnless(*log),
           "degenerate relations frame only");
  }
  expect(logs.playout.events.empty() && ocel_events_frame_columnless(logs.playout) &&
             logs.playout.table_columns.objects && logs.playout.table_columns.objects->empty() &&
             ocel_relations_frame_degenerate(logs.playout),
         "an empty playout has frames without columns");
  const auto causal = reference_oc_causal_net_playout(OcCausalNet{}, {});
  expect(causal.events.empty() && ocel_events_frame_columnless(causal) &&
             ocel_relations_frame_degenerate(causal),
         "an empty causal-net playout has frames without columns");

  // A file naming any object, None included, keeps the typed frame.
  const auto typed = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e1,2024-01-01 00:00:00,a,\n"
      ",2024-01-02 00:00:00,b,['o1']\n");
  expect(typed.relations.empty() && !typed.table_columns.relations,
         "an object named on a dropped row keeps the typed relations frame");
  const auto empty_lists = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e1,2024-01-01 00:00:00,a,[]\n");
  expect(ocel_relations_frame_degenerate(empty_lists), "empty lists name no object");
  const auto with_relations = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e1,2024-01-01 00:00:00,a,['o1']\n");
  expect(!with_relations.table_columns.relations, "a related CSV import keeps its frames");
  // A playout with events is untouched.
  Ocel related;
  related.events.push_back({"e1", "a", "2024-01-01T00:00:00Z", {}});
  related.objects.push_back({"o1", "t", {}});
  related.relations.push_back({"e1", "o1", ""});
  expect(!reference_ocpn_extensive_playout(discover_ocpn(related)).table_columns.events,
         "a playout with events keeps its frames");

  // The transport carries the declarations.
  for (const auto* log : {&logs.no_objects, &logs.playout}) {
    const auto again = parse_ocel_json(serialize_ocel_json(*log, OcelJsonFormat::kClassic));
    expect(again.table_columns.relations == log->table_columns.relations &&
               again.table_columns.events == log->table_columns.events &&
               again.table_columns.objects == log->table_columns.objects,
           "the transport keeps the degenerate frames");
  }
  // A filter result keeps them.
  const auto filtered = filter_ocel_objects(logs.no_objects, {"zz"});
  expect(filtered.events.empty() && ocel_relations_frame_degenerate(filtered),
         "an empty filter result keeps the qualifier-only frame");
}

struct Call {
  std::string name;
  std::function<void(const Ocel&)> call;
  // Expected per log (no_objects, with_objects, no_events, playout): empty for
  // a result, otherwise the exception text.
  std::vector<std::string> expected;
};

void calls(const Logs& logs) {
  const auto eid = key_error("ocel:eid"), oid = key_error("ocel:oid"), type = key_error("ocel:type"),
             activity = key_error("ocel:activity"), timestamp = key_error("ocel:timestamp");
  const std::string objects_index =
      "KeyError: \"None of [Index(['ocel:oid', 'ocel:type'], dtype='str')] are in the [columns]\"";
  const std::string naive_us = "TypeError: Invalid comparison between dtype=datetime64[us] and datetime";
  const std::string naive_s = "TypeError: Invalid comparison between dtype=datetime64[s] and datetime";
  const auto sqlite_path = std::filesystem::temp_directory_path() / "minera_review_dv1.sqlite";
  const std::vector<Call> table = {
      {"object type activities", [](const Ocel& l) { (void)object_type_activities(l); },
       {activity, activity, activity, activity}},
      {"objects per type count", [](const Ocel& l) { (void)objects_ot_count(l); }, {eid, eid, eid, eid}},
      {"activity associations", [](const Ocel& l) { (void)activity_associations(l); },
       {activity, activity, activity, activity}},
      {"activity associations start",
       [](const Ocel& l) { (void)activity_associations(l, OcelAssociationPrefilter::kStart); },
       {oid, oid, oid, oid}},
      {"type activity associations", [](const Ocel& l) { (void)object_type_activity_associations(l); },
       {type, type, type, type}},
      {"edge associations", [](const Ocel& l) { (void)edge_associations(l); }, {eid, eid, eid, eid}},
      {"description", [](const Ocel& l) { (void)describe_ocel_tuples(l); },
       {timestamp, timestamp, timestamp, objects_index}},
      {"ocdfg", [](const Ocel& l) { (void)discover_ocdfg_classic(l); }, {activity, activity, activity, activity}},
      {"otg", [](const Ocel& l) { (void)discover_otg_classic(l); }, {eid, eid, eid, type}},
      {"etot", [](const Ocel& l) { (void)discover_etot_classic(l); }, {"", "", "", ""}},
      {"ocpn", [](const Ocel& l) { (void)discover_ocpn(l); }, {activity, activity, activity, activity}},
      {"saw nets", [](const Ocel& l) { (void)discover_saw_nets(l); }, {"", type, type, type}},
      {"event features", [](const Ocel& l) { (void)extract_ocel_features(l, OcelFeatureVariant::kEvents); },
       {eid, eid, eid, eid}},
      {"object features", [](const Ocel& l) { (void)extract_ocel_features(l, OcelFeatureVariant::kObjects); },
       {oid, oid, oid, oid}},
      {"object types", [](const Ocel& l) { (void)filter_ocel_object_types(l, {"t"}); }, {"", oid, oid, type}},
      {"object types none", [](const Ocel& l) { (void)filter_ocel_object_types(l, {"zz"}); }, {"", "", "", type}},
      {"object types level 2", [](const Ocel& l) { (void)filter_ocel_object_types(l, {"t"}, true, 2); },
       {eid, eid, eid, type}},
      {"objects", [](const Ocel& l) { (void)filter_ocel_objects(l, {"o1"}); }, {"", oid, oid, oid}},
      {"objects negative", [](const Ocel& l) { (void)filter_ocel_objects(l, {"o1"}, false); },
       {"", oid, oid, oid}},
      {"objects level 2", [](const Ocel& l) { (void)filter_ocel_objects(l, {"o1"}, true, 2); },
       {eid, eid, eid, eid}},
      {"object attribute", [](const Ocel& l) { (void)filter_ocel_object_attribute(l, "ocel:type", {OcelValue("t")}); },
       {"", oid, oid, type}},
      {"cc object", [](const Ocel& l) { (void)filter_ocel_cc_object(l, "o1"); }, {eid, eid, eid, eid}},
      {"cc length", [](const Ocel& l) { (void)filter_ocel_cc_length(l, 1, 10); }, {eid, eid, eid, eid}},
      {"cc object type", [](const Ocel& l) { (void)filter_ocel_cc_object_type(l, "t"); }, {eid, eid, eid, type}},
      {"cc activity", [](const Ocel& l) { (void)filter_ocel_cc_activity(l, "a"); },
       {eid, eid, eid, activity}},
      {"events", [](const Ocel& l) { (void)filter_ocel_events(l, {"e1"}); }, {eid, eid, "", eid}},
      {"events none", [](const Ocel& l) { (void)filter_ocel_events(l, {"zz"}); }, {"", "", "", eid}},
      {"event attribute",
       [](const Ocel& l) { (void)filter_ocel_event_attribute(l, "ocel:activity", {OcelValue("a")}); },
       {eid, eid, "", activity}},
      {"start events", [](const Ocel& l) { (void)filter_ocel_start_events_per_object_type(l, "t"); },
       {type, type, type, type}},
      {"end events", [](const Ocel& l) { (void)filter_ocel_end_events_per_object_type(l, "t"); },
       {type, type, type, type}},
      {"activities of a type", [](const Ocel& l) { (void)filter_ocel_activities_connected_object_type(l, "t"); },
       {type, type, type, type}},
      {"timestamp",
       [](const Ocel& l) { (void)filter_ocel_events_timestamp(l, "2000-01-01 00:00:00", "2100-01-01 00:00:00"); },
       {naive_us, naive_us, naive_s, timestamp}},
      {"object count", [](const Ocel& l) { (void)filter_ocel_events_object_type_counts(l, {{"t", 1}}); },
       {eid, eid, eid, eid}},
      {"activity type matching", [](const Ocel& l) { (void)filter_ocel_activity_type_matching(l, {{"t", {"a"}}}); },
       {activity, activity, activity, activity}},
      {"split components", [](const Ocel& l) { (void)split_ocel(l); }, {eid, eid, eid, eid}},
      {"split ancestors",
       [](const Ocel& l) {
         OcelSplitOptions options;
         options.variant = OcelSplitVariant::kAncestorsDescendants;
         options.object_type = "t";
         (void)split_ocel(l, options);
       },
       {eid, eid, eid, eid}},
      {"lifecycle enrichment",
       [](const Ocel& l) {
         OcelEnrichmentOptions options;
         options.include_interaction = options.include_descendants = options.include_inheritance =
             options.include_cobirth = options.include_codeath = false;
         options.include_lifecycle_qualifiers = true;
         (void)enrich_ocel(l, options);
       },
       {eid, eid, eid, eid}},
      {"graph enrichment", [](const Ocel& l) { (void)enrich_ocel(l, OcelEnrichmentOptions{}); },
       {eid, eid, eid, eid}},
      {"interaction graph",
       [](const Ocel& l) { (void)discover_ocel_object_graph(l, OcelEnrichmentGraph::kInteraction); },
       {eid, eid, eid, eid}},
      {"codeath graph", [](const Ocel& l) { (void)discover_ocel_object_graph(l, OcelEnrichmentGraph::kCodeath); },
       {eid, eid, eid, eid}},
      {"flatten", [](const Ocel& l) { (void)flatten_ocel_to_event_log(l, "t"); }, {type, type, type, type}},
      {"sqlite",
       [&](const Ocel& l) {
         std::filesystem::remove(sqlite_path);
         export_ocel_sqlite(l, sqlite_path);
       },
       {"KeyError: \"['ocel:eid', 'ocel:oid'] not in index\"", "KeyError: \"['ocel:eid', 'ocel:oid'] not in index\"",
        "KeyError: \"['ocel:eid', 'ocel:oid'] not in index\"",
        "KeyError: \"None of [Index(['ocel:eid', 'ocel:activity'], dtype='str')] are in the [columns]\""}},
  };
  const std::vector<std::pair<const char*, const Ocel*>> shapes = {
      {"no objects", &logs.no_objects}, {"objects", &logs.with_objects},
      {"no events", &logs.no_events}, {"playout", &logs.playout}};
  for (const auto& row : table)
    for (std::size_t i = 0; i < shapes.size(); ++i) {
      const auto got = error_of([&] { row.call(*shapes[i].second); });
      expect(got == row.expected[i], row.name + " on " + shapes[i].first + ": got \"" + got + "\"");
    }
  std::filesystem::remove(sqlite_path);

  // An aware relation-free import filters by time: the kept events then
  // select the relations by event id, no kept event gives an empty log.
  const auto aware = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity\n"
      "e1,2024-01-01 00:00:00+01:00,a\n"
      "e2,2024-01-02 00:00:00+01:00,b\n");
  expect(error_of([&] { (void)filter_ocel_events_timestamp(aware, "2000-01-01 00:00:00", "2100-01-01 00:00:00"); }) ==
             eid,
         "an aware import's kept events read the relations");
  expect(filter_ocel_events_timestamp(aware, "2000-01-01 00:00:00", "2000-01-02 00:00:00").events.empty(),
         "no kept event gives an empty log");
}

// Texts as file rows: an empty text becomes a dropped row (no id), so the
// column still holds it as a missing value.
std::string csv_of(const std::vector<std::string>& texts) {
  std::string csv = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n";
  for (std::size_t i = 0; i < texts.size(); ++i)
    csv += texts[i].empty() ? std::string(",,a,\n") : "e" + std::to_string(i) + "," + texts[i] + ",a,['o1']\n";
  return csv;
}

std::string outcome(const std::vector<std::string>& texts) {
  try {
    (void)parse_ocel_csv(csv_of(texts));
  } catch (const std::exception& error) {
    std::string message = error.what();
    const auto cut = message.find(". You might want to try:");
    return cut == std::string::npos ? message : message.substr(0, cut);
  }
  return "ok";
}

void timestamp_formats() {
  const auto value_error = [](const std::string& text) { return "ValueError: " + text; };
  struct Case {
    std::vector<std::string> texts;
    std::string expected;
  };
  const std::vector<Case> cases = {
      {{"2024-01-01 00:00:00", "2024-01-02 00:00:00+02:00"},
       value_error("unconverted data remains when parsing with format \"%Y-%m-%d %H:%M:%S\": \"+02:00\"")},
      {{"2024-01-01 00:00:00.123456789", "2024-01-02 00:00:00"},
       value_error("time data \"2024-01-02 00:00:00\" doesn't match format \"%Y-%m-%d %H:%M:%S.%f\"")},
      {{"2024-01-01", "2024-01-02 10:00:00"},
       value_error("unconverted data remains when parsing with format \"%Y-%m-%d\": \" 10:00:00\"")},
      {{"2024-01-01 10:00:00", "2024-01-02"},
       value_error("time data \"2024-01-02\" doesn't match format \"%Y-%m-%d %H:%M:%S\"")},
      {{"2024-01-01T00:00:00", "2024-01-02 00:00:00"},
       value_error("time data \"2024-01-02 00:00:00\" doesn't match format \"%Y-%m-%dT%H:%M:%S\"")},
      {{"2024-01-01 00:00:00+01:00", "2024-01-02 00:00:00+02:00"},
       value_error("Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to convert "
                   "to a common timezone.")},
      {{"2024-01-01 00:00", "2024-01-02 00:00:00"},
       value_error("unconverted data remains when parsing with format \"%Y-%m-%d %H:%M\": \":00\"")},
      // The first text binds the format even after missing cells.
      {{"", "2024-01-02 00:00:00", "2024-01-03"},
       value_error("time data \"2024-01-03\" doesn't match format \"%Y-%m-%d %H:%M:%S\"")},
      // Up to nine fraction digits, more skipped at the end or before an
      // expected offset, kept as the rest otherwise.
      {{"2024-01-01 00:00:00.5", "2024-01-02 00:00:00.1234567891"}, "ok"},
      {{"2024-01-01 00:00:00.5", "2024-01-02 00:00:00.1234567891+01:00"},
       value_error("unconverted data remains when parsing with format \"%Y-%m-%d %H:%M:%S.%f\": \"1+01:00\"")},
      {{"2024-01-01 00:00:00.5Z", "2024-01-02 00:00:00.1234567891+00:00"}, "ok"},
      {{"2024-01-01 00:00:00Z", "2024-01-02 00:00:00+00:00", "2024-01-03 00:00:00-00:00"}, "ok"},
      {{"2024-01-01 00:00:00+0100", "2024-01-02 00:00:00+01:00"}, "ok"},
      {{"2024-01-01T00:00:00Z", "2024-01-02T00:00:00Z"}, "ok"},
  };
  for (const auto& item : cases) {
    const auto got = outcome(item.texts);
    expect(got == item.expected, "timestamps " + item.texts.front() + ": got \"" + got + "\"");
  }
  // The suggestions follow a format error.
  expect(error_of([] { (void)parse_ocel_csv(csv_of({"2024-01-01", "2024-01-02 00:00"})); })
                 .find("passing `format='mixed'`, and the format will be inferred for each element individually. "
                       "You might want to use `dayfirst` alongside this.") != std::string::npos,
         "the suggestions");
  // Another shape keeps the native reading (declared).
  expect(outcome({"2024-01-01t00:00:00", "2024-01-02 00:00:00"}) == "ok", "a lowercase separator reads natively");
}

int replay_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "cannot read " << path << '\n';
    return 1;
  }
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream lines(detail::gzip_decompress(bytes, std::size_t{1} << 28));
  std::string line;
  std::size_t count = 0, mismatches = 0;
  while (std::getline(lines, line)) {
    if (line.empty()) continue;
    const auto tab = line.find('\t');
    std::vector<std::string> texts;
    std::size_t start = 0;
    const std::string joined = line.substr(0, tab);
    while (true) {
      const auto bar = joined.find('|', start);
      texts.push_back(joined.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
      if (bar == std::string::npos) break;
      start = bar + 1;
    }
    ++count;
    const auto got = outcome(texts);
    if (got != line.substr(tab + 1)) {
      if (++mismatches <= 10) std::cerr << "bank: " << joined << " -> " << got << '\n';
    }
  }
  expect(count == 4000, "the bank holds 4,000 columns");
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}

struct LinkCall {
  const char* flags;  // look_forward, keep_first_occurrence, propagate
  bool error;
  const char* expected;  // "out-in" pairs in order
};

struct LinkCase {
  const char* csv;
  std::vector<LinkCall> calls;
};

const std::vector<LinkCase> kLinkCases = {
    // seed 7019 (BIG domains): doc flt, ref flt
    {
      "case,act,ts,doc,ref\n"
      "c3,a,2020-01-01 00:30:34,,3\n"
      "c3,b,2020-01-01 00:26:05,0.5,4\n"
      "c3,a,2020-01-01 00:22:01,3,3\n"
      "c2,b,2020-01-01 00:09:18,2,4\n"
      "c1,a,2020-01-01 00:43:30,1.5,3\n"
      "c0,b,2020-01-01 00:25:14,,3\n"
      "c1,b,2020-01-01 00:13:48,1.5,0.5\n"
      "c1,a,2020-01-01 00:06:12,3,3\n"
      "c1,b,2020-01-01 00:14:18,0.5,1.5\n"
      "c0,b,2020-01-01 00:40:53,,1.5\n"
      "c3,a,2020-01-01 00:40:41,1.5,3\n"
      "c2,b,2020-01-01 00:44:31,4,4\n"
      "c3,a,2020-01-01 00:19:46,,4\n"
      "c1,a,2020-01-01 00:42:59,3,2\n"
      "c0,a,2020-01-01 00:10:57,2,2\n"
      "c2,b,2020-01-01 00:43:56,0.5,\n"
      "c1,a,2020-01-01 00:42:50,2,\n"
      "c1,b,2020-01-01 00:45:57,,3\n",
      {
       {"110", false, "0-6 1-2 2-13 3-4 5-12 6-7 7-12 9-12 10-11 11-12 12-13 13-14"},
       {"111", false, "0-6 0-7 0-12 0-13 0-14 1-2 1-13 1-14 2-13 2-14 3-4 5-12 5-13 5-14 6-7 6-12 6-13 6-14 7-12 7-13 7-14 9-12 9-13 9-14 10-11 10-12 10-13 10-14 11-12 11-13 11-14 12-13 12-14 13-14"},
       {"100", false, "0-6 0-7 0-9 0-10 0-14 0-17 1-2 1-13 2-13 3-11 3-4 5-12 5-15 6-7 6-9 6-10 6-14 6-17 7-12 7-15 9-12 9-15 10-11 11-12 11-15 12-13 13-17 13-14"},
       {"101", false, "0-6 0-7 0-9 0-10 0-11 0-12 0-13 0-14 0-15 0-17 1-17 1-2 1-13 1-14 2-17 2-13 2-14 3-4 3-11 3-12 3-13 3-14 3-15 3-17 5-17 5-12 5-13 5-14 5-15 6-7 6-9 6-10 6-11 6-12 6-13 6-14 6-15 6-17 7-17 7-12 7-13 7-14 7-15 9-17 9-12 9-13 9-14 9-15 10-11 10-12 10-13 10-14 10-15 10-17 11-17 11-12 11-13 11-14 11-15 12-17 12-13 12-14 13-17 13-14"},
       {"010", false, "0-0 1-2 2-2 3-4 4-3 5-12 6-0 7-12 8-3 9-12 10-4 11-12 12-2 13-0 14-4 15-3 16-1 17-12"},
       {"011", false, "0-0 1-2 2-2 3-3 3-4 4-3 4-4 5-2 5-12 6-0 7-2 7-12 8-3 8-4 9-2 9-12 10-3 10-4 11-2 11-12 12-2 13-0 14-3 14-4 15-3 15-4 16-1 16-2 17-2 17-12"},
       {"000", false, "0-0 0-6 0-7 0-9 0-10 0-14 0-17 1-2 1-13 2-2 2-13 3-11 3-4 4-3 5-12 5-15 6-0 6-6 6-7 6-9 6-10 6-14 6-17 7-12 7-15 8-3 9-12 9-15 10-11 10-4 11-12 11-15 12-2 12-13 13-0 13-6 13-7 13-9 13-10 13-14 13-17 14-11 14-4 15-3 16-8 16-1 16-5 16-16 17-12 17-15"},
       {"001", false, "0-0 0-2 0-3 0-4 0-6 0-7 0-9 0-10 0-11 0-12 0-13 0-14 0-15 0-17 1-0 1-2 1-3 1-4 1-6 1-7 1-9 1-10 1-11 1-12 1-13 1-14 1-15 1-17 2-0 2-2 2-3 2-4 2-6 2-7 2-9 2-10 2-11 2-12 2-13 2-14 2-15 2-17 3-0 3-2 3-3 3-4 3-6 3-7 3-9 3-10 3-11 3-12 3-13 3-14 3-15 3-17 4-0 4-2 4-3 4-4 4-6 4-7 4-9 4-10 4-11 4-12 4-13 4-14 4-15 4-17 5-0 5-2 5-3 5-4 5-6 5-7 5-9 5-10 5-11 5-12 5-13 5-14 5-15 5-17 6-0 6-2 6-3 6-4 6-6 6-7 6-9 6-10 6-11 6-12 6-13 6-14 6-15 6-17 7-0 7-2 7-3 7-4 7-6 7-7 7-9 7-10 7-11 7-12 7-13 7-14 7-15 7-17 8-0 8-2 8-3 8-4 8-6 8-7 8-9 8-10 8-11 8-12 8-13 8-14 8-15 8-17 9-0 9-2 9-3 9-4 9-6 9-7 9-9 9-10 9-11 9-12 9-13 9-14 9-15 9-17 10-0 10-2 10-3 10-4 10-6 10-7 10-9 10-10 10-11 10-12 10-13 10-14 10-15 10-17 11-0 11-2 11-3 11-4 11-6 11-7 11-9 11-10 11-11 11-12 11-13 11-14 11-15 11-17 12-0 12-2 12-3 12-4 12-6 12-7 12-9 12-10 12-11 12-12 12-13 12-14 12-15 12-17 13-0 13-2 13-3 13-4 13-6 13-7 13-9 13-10 13-11 13-12 13-13 13-14 13-15 13-17 14-0 14-2 14-3 14-4 14-6 14-7 14-9 14-10 14-11 14-12 14-13 14-14 14-15 14-17 15-0 15-2 15-3 15-4 15-6 15-7 15-9 15-10 15-11 15-12 15-13 15-14 15-15 15-17 16-0 16-1 16-2 16-3 16-4 16-5 16-6 16-7 16-8 16-9 16-10 16-11 16-12 16-13 16-14 16-15 16-16 16-17 17-0 17-2 17-3 17-4 17-6 17-7 17-9 17-10 17-11 17-12 17-13 17-14 17-15 17-17"}}},
    // seed 7047 (BIG domains): doc txt, ref txt
    {
      "case,act,ts,doc,ref\n"
      "c1,a,2020-01-01 00:10:53,y,w\n"
      "c0,a,2020-01-01 00:08:13,z,z\n"
      "c0,a,2020-01-01 00:07:00,v,x\n"
      "c1,a,2020-01-01 00:45:13,y,z\n"
      "c0,b,2020-01-01 00:15:52,x,x\n"
      "c0,b,2020-01-01 00:20:10,x,v\n"
      "c1,a,2020-01-01 00:05:40,u,w\n"
      "c1,b,2020-01-01 00:22:05,z,v\n"
      "c0,a,2020-01-01 00:45:38,u,z\n"
      "c1,a,2020-01-01 00:32:28,y,z\n"
      "c0,b,2020-01-01 00:03:49,y,x\n"
      "c1,a,2020-01-01 00:11:56,w,z\n"
      "c1,b,2020-01-01 00:26:30,y,y\n"
      "c1,a,2020-01-01 00:05:44,w,x\n"
      "c1,a,2020-01-01 00:31:37,z,v\n"
      "c1,b,2020-01-01 00:05:10,u,z\n"
      "c1,b,2020-01-01 00:05:42,w,y\n"
      "c0,b,2020-01-01 00:24:40,z,x\n",
      {
       {"110", false, "0-3 3-7 4-7 5-10 6-8 7-13 9-12 10-12 11-15 12-15 14-15"},
       {"111", false, "0-3 0-13 0-7 3-13 3-7 4-13 4-7 5-10 5-12 5-15 6-8 7-13 9-12 9-15 10-12 10-15 11-15 12-15 14-15"},
       {"100", false, "0-3 0-13 3-7 4-7 5-10 5-11 5-14 6-8 6-16 6-17 6-15 7-13 9-12 10-12 11-16 11-17 11-15 12-16 12-17 12-15 14-16 14-17 14-15"},
       {"101", false, "0-3 0-13 0-7 3-13 3-7 4-13 4-7 5-16 5-17 5-10 5-11 5-12 5-14 5-15 6-8 6-16 6-17 6-15 7-13 9-16 9-17 9-12 9-15 10-16 10-17 10-12 10-15 11-16 11-17 11-15 12-16 12-17 12-15 14-16 14-17 14-15"},
       {"010", false, "0-3 3-2 4-2 5-10 6-1 7-3 8-2 9-0 10-0 11-1 12-1 13-3 14-1 15-3 16-3"},
       {"011", false, "0-2 0-3 3-2 4-2 5-0 5-2 5-3 5-10 6-1 7-2 7-3 8-2 9-0 9-2 9-3 10-0 10-2 10-3 11-1 12-1 13-2 13-3 14-1 15-2 15-3 16-2 16-3"},
       {"000", false, "0-3 0-13 3-2 3-7 4-2 4-7 5-10 5-11 5-14 6-1 6-6 6-8 6-15 6-16 6-17 7-3 7-13 8-2 8-7 9-0 9-4 9-5 9-9 9-12 10-0 10-4 10-5 10-9 10-12 11-1 11-6 11-8 11-15 11-16 11-17 12-1 12-6 12-8 12-15 12-16 12-17 13-3 13-13 14-1 14-6 14-8 14-15 14-16 14-17 15-3 15-13 16-3 16-13"},
       {"001", false, "0-2 0-3 0-13 0-7 3-2 3-3 3-7 3-13 4-2 4-3 4-7 4-13 5-0 5-1 5-2 5-3 5-4 5-5 5-6 5-7 5-8 5-9 5-10 5-11 5-12 5-13 5-14 5-15 5-16 5-17 6-1 6-2 6-3 6-6 6-7 6-8 6-13 6-15 6-16 6-17 7-2 7-3 7-13 7-7 8-2 8-3 8-7 8-13 9-0 9-1 9-2 9-3 9-4 9-5 9-6 9-7 9-8 9-9 9-10 9-11 9-12 9-13 9-14 9-15 9-16 9-17 10-0 10-1 10-2 10-3 10-4 10-5 10-6 10-7 10-8 10-9 10-10 10-11 10-12 10-13 10-14 10-15 10-16 10-17 11-1 11-2 11-3 11-6 11-7 11-8 11-13 11-15 11-16 11-17 12-1 12-2 12-3 12-6 12-7 12-8 12-13 12-15 12-16 12-17 13-2 13-3 13-13 13-7 14-1 14-2 14-3 14-6 14-7 14-8 14-13 14-15 14-16 14-17 15-2 15-3 15-13 15-7 16-2 16-3 16-13 16-7"}}},
    // seed 7110 (BIG domains): doc flt, ref int
    {
      "case,act,ts,doc,ref\n"
      "c3,a,2020-01-01 00:19:04,,4\n"
      "c3,a,2020-01-01 00:18:08,0.5,2\n"
      "c2,b,2020-01-01 00:46:09,0.5,4\n"
      "c1,b,2020-01-01 00:32:10,0.5,3\n"
      "c0,a,2020-01-01 00:22:17,,2\n"
      "c0,b,2020-01-01 00:22:57,1.5,3\n"
      "c0,b,2020-01-01 00:22:25,1.5,4\n"
      "c2,b,2020-01-01 00:18:51,2,1\n"
      "c3,b,2020-01-01 00:30:00,2,3\n"
      "c2,b,2020-01-01 00:45:50,0.5,4\n"
      "c3,b,2020-01-01 00:35:28,,3\n"
      "c3,b,2020-01-01 00:13:01,1.5,3\n"
      "c2,a,2020-01-01 00:28:53,1.5,4\n"
      "c0,a,2020-01-01 00:47:00,0.5,1\n"
      "c3,b,2020-01-01 00:22:33,2,4\n"
      "c1,a,2020-01-01 00:19:01,,3\n"
      "c0,a,2020-01-01 00:41:17,1.5,1\n"
      "c0,b,2020-01-01 00:46:10,2,2\n"
      "c3,a,2020-01-01 00:14:44,2,2\n"
      "c0,b,2020-01-01 00:32:37,1.5,4\n",
      {
       {"110", false, "1-2 3-6 8-18 11-18"},
       {"111", false, "1-2 3-6 8-18 11-18"},
       {"100", false, "1-2 1-18 1-6 3-18 3-6 8-18 11-18"},
       {"101", false, "1-2 1-18 1-6 3-18 3-6 8-18 11-18"},
       {"010", false, "1-1 3-2 8-18 11-1 18-1"},
       {"011", false, "1-1 3-2 8-1 8-18 11-1 18-1"},
       {"000", false, "1-2 3-2 1-18 3-1 1-6 8-1 8-18 8-2 3-18 8-6 18-1 1-1 18-2 18-18 18-6 11-1 11-2 11-18 3-6 11-6"},
       {"001", false, "1-2 3-2 1-18 3-1 1-6 8-1 8-18 8-2 3-18 8-6 18-1 1-1 18-2 18-18 18-6 11-1 11-2 11-18 3-6 11-6"}}},
    // seed 7134 (BIG domains): doc txt, ref mix
    {
      "case,act,ts,doc,ref\n"
      "c0,a,2020-01-01 00:17:17,x,x\n"
      "c1,a,2020-01-01 00:46:55,x,y\n"
      "c3,b,2020-01-01 00:46:04,y,x\n"
      "c1,b,2020-01-01 00:26:03,z,2\n"
      "c4,a,2020-01-01 00:19:44,y,2\n"
      "c0,b,2020-01-01 00:09:32,w,2\n"
      "c2,a,2020-01-01 00:29:43,y,x\n"
      "c2,a,2020-01-01 00:26:59,y,x\n"
      "c2,b,2020-01-01 00:20:08,w,1\n"
      "c1,a,2020-01-01 00:07:07,z,2\n"
      "c4,b,2020-01-01 00:38:09,z,2\n"
      "c0,b,2020-01-01 00:09:10,x,x\n"
      "c1,b,2020-01-01 00:28:26,w,x\n"
      "c2,b,2020-01-01 00:15:20,x,2\n"
      "c1,a,2020-01-01 00:32:48,w,1\n"
      "c4,b,2020-01-01 00:16:39,y,1\n"
      "c1,b,2020-01-01 00:45:34,y,2\n",
      {
       {"110", false, "1-5 3-5 4-16 5-9 6-16 9-16 11-16 14-16 15-16"},
       {"111", false, "1-16 1-9 1-5 3-16 3-9 3-5 4-16 5-16 5-9 6-16 9-16 11-16 14-16 15-16"},
       {"100", false, "1-5 1-9 1-10 1-11 1-15 3-5 3-9 3-10 3-11 3-15 4-16 5-9 5-10 5-11 5-15 6-16 9-16 11-16 14-16 15-16"},
       {"101", false, "1-16 1-5 1-9 1-10 1-11 1-15 3-16 3-5 3-9 3-10 3-11 3-15 4-16 5-16 5-9 5-10 5-11 5-15 6-16 9-16 11-16 14-16 15-16"},
       {"010", false, "1-1 3-1 4-16 5-1 6-16 9-16 11-16 14-16 15-16 16-1"},
       {"011", false, "1-1 3-1 4-16 4-1 5-1 6-16 6-1 9-16 9-1 11-16 11-1 14-16 14-1 15-16 15-1 16-1"},
       {"000", false, "1-1 1-5 1-9 1-10 1-11 1-15 3-1 3-5 3-9 3-10 3-11 3-15 4-16 5-1 5-5 5-9 5-10 5-11 5-15 6-16 9-16 11-16 14-16 15-16 16-1 16-5 16-9 16-10 16-11 16-15"},
       {"001", false, "1-16 1-1 1-5 1-9 1-10 1-11 1-15 3-16 3-1 3-5 3-9 3-10 3-11 3-15 4-1 4-5 4-9 4-10 4-11 4-15 4-16 5-16 5-1 5-5 5-9 5-10 5-11 5-15 6-1 6-5 6-9 6-10 6-11 6-15 6-16 9-1 9-5 9-9 9-10 9-11 9-15 9-16 11-1 11-5 11-9 11-10 11-11 11-15 11-16 14-1 14-5 14-9 14-10 14-11 14-15 14-16 15-1 15-5 15-9 15-10 15-11 15-15 15-16 16-16 16-1 16-5 16-9 16-10 16-11 16-15"}}}
};

void propagate_order() {
  std::size_t count = 0;
  for (const auto& item : kLinkCases) {
    CsvImportOptions options;
    options.case_id_column = "case";
    options.activity_column = "act";
    options.timestamp_column = "ts";
    std::istringstream input(item.csv);
    const auto log = import_csv_event_data(input, options).log;
    for (const auto& call : item.calls) {
      ++count;
      LinkAnalysisOptions link;
      link.out_column = "doc";
      link.in_column = "ref";
      link.look_forward = call.flags[0] == '1';
      link.keep_first_occurrence = call.flags[1] == '1';
      link.propagate = call.flags[2] == '1';
      std::string seen;
      try {
        for (const auto& pair : discover_link_analysis(log, link).links)
          seen += (seen.empty() ? "" : " ") + std::to_string(pair.source.index) + "-" + std::to_string(pair.target.index);
      } catch (const std::exception& error) {
        seen = std::string("raised ") + error.what();
      }
      expect(!call.error && seen == call.expected,
             std::string("link analysis ") + call.flags + ":\n  seen     " + seen + "\n  expected " + call.expected);
    }
  }
  expect(count == 32, "32 link analysis calls");
}

}  // namespace

int main(int argc, char** argv) {
  const auto logs = make_logs();
  frames(logs);
  calls(logs);
  timestamp_formats();
  propagate_order();
  if (argc > 1 && replay_bank(argv[1]) != 0) return 1;
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review DV1 passed\n";
  return 0;
}
