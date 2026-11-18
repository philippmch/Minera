// Review EE1: pm4py 2.7.22's read_ocel_json on classic OCEL 1.0 JSON files.
// Observed black-box (review-harness round-ee, js/jp2.py-jp12.py probes and
// js/jfz.py, 0 differences over 456 probes and 4,000 fuzzed files besides
// declared refusals):
//
// - the file is read as json.load reads it (NaN and Infinity, any integer,
//   a repeated key's last value at its first place); "ocel:objects",
//   "ocel:events", each "ocel:type", "ocel:ovmap", "ocel:activity",
//   "ocel:timestamp", "ocel:omap" and "ocel:vmap", the three global scopes
//   and, unless no event is listed, the time's .endswith are looked up as
//   Python looks them up, so a missing one raises its KeyError and a wrong
//   type its TypeError or AttributeError, in the order pm4py walks the file;
//   an unreadable event time raises fromisoformat's error;
// - the frames are pandas record frames: a column's dtype comes from all its
//   rows (an integer next to a float or a missing cell reads as float, one
//   past int64 that fits uint64 too), activities and types read as str() of
//   their column's cell, an omap entry that is no str names no object and a
//   str omap is read character by character;
// - vmap ocel:eid/ocel:activity/ocel:timestamp and ovmap ocel:oid/ocel:type
//   entries take the events or objects frame's cell while the relations
//   frame keeps the event's or object's own value;
// - each frame drops its rows with a missing or empty id, activity or type,
//   the events and objects frames then keep the rows some remaining relation
//   names, and the relations those whose event and object remain; when none
//   does, no event and no object remains either;
// - object changes are a record frame when the first entry is a dict
//   (dict(entry) of each); a change of an unlisted object is dropped; the
//   change takes its object's type; a time pm4py cannot read (or one that is
//   no str) leaves every change time as written;
// - a vmap ocel:timestamp that is a number or boolean next to a parsed time
//   raises pandas' sort error.
//
// Declared (see docs/reference-2.7.22-parity.md, EE1): a kept row holding
// what a native OCEL cannot (a list, dict or integer past 64 bits, a frame
// cell the relations frame disagrees with, an id override naming another
// row, a time override, a change without a str id, time or field, a change
// time pm4py keeps as text next to an unreadable one) is refused; a non-dict
// change entry list of pairs is refused; a root that is no object raises
// Minera's own message. Classic JSON is also Minera's transport: a document
// it wrote for a log holding an event time read_ocel_json cannot read says
// so ("minera:unreadableEventTimes") and keeps it.
//
// With a bank path (docs/ocel-json-reference-ee-bank.tsv.gz) the test also
// replays 2,056 files read on 2026-11-18: the document on one line, a tab,
// then the reading as reading() prints it.
#include "minera/ocel.hpp"
#include "../gzip_codec.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string quoted(const std::string& s) {
  std::string out = "\"";
  for (const unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      char buffer[8];
      std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
      out += buffer;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out + "\"";
}

std::string value_text(const OcelValue& v) {
  if (std::holds_alternative<std::nullptr_t>(v)) return "[\"n\"]";
  if (const auto* b = std::get_if<bool>(&v)) return std::string("[\"b\",") + (*b ? "true" : "false") + "]";
  if (const auto* i = std::get_if<std::int64_t>(&v)) return "[\"i\"," + std::to_string(*i) + "]";
  if (const auto* d = std::get_if<double>(&v)) {
    if (std::isnan(*d)) return "[\"f\",\"nan\"]";
    if (std::isinf(*d)) return *d > 0 ? "[\"f\",\"inf\"]" : "[\"f\",\"-inf\"]";
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.17g", *d);
    return std::string("[\"f\",\"") + buffer + "\"]";
  }
  return "[\"s\"," + quoted(std::get<std::string>(v)) + "]";
}

std::string catalog(const OcelTypeCatalog& types, const std::string& type, const std::string& key) {
  const auto found = types.find(type);
  if (found == types.end()) return "";
  const auto entry = found->second.find(key);
  return entry == found->second.end() ? "" : entry->second;
}

std::string reading_of(const Ocel& log) {
  std::ostringstream o;
  o << "{\"events\":[";
  for (std::size_t k = 0; k < log.events.size(); ++k) {
    const auto& e = log.events[k];
    o << (k ? "," : "") << "[" << quoted(e.id) << "," << quoted(e.activity) << "," << quoted(e.timestamp) << ",[";
    for (std::size_t a = 0; a < e.attributes.size(); ++a)
      o << (a ? "," : "") << "[" << quoted(e.attributes[a].key) << ","
        << quoted(catalog(log.event_type_attributes, e.activity, e.attributes[a].key)) << ","
        << value_text(e.attributes[a].value) << "]";
    o << "]]";
  }
  o << "],\"objects\":[";
  for (std::size_t k = 0; k < log.objects.size(); ++k) {
    const auto& e = log.objects[k];
    o << (k ? "," : "") << "[" << quoted(e.id) << "," << quoted(e.type) << ",[";
    for (std::size_t a = 0; a < e.attributes.size(); ++a)
      o << (a ? "," : "") << "[" << quoted(e.attributes[a].key) << ","
        << quoted(catalog(log.object_type_attributes, e.type, e.attributes[a].key)) << ","
        << value_text(e.attributes[a].value) << "]";
    o << "]]";
  }
  o << "],\"relations\":[";
  for (std::size_t k = 0; k < log.relations.size(); ++k) {
    const auto& r = log.relations[k];
    o << (k ? "," : "") << "[" << quoted(r.event_id) << "," << quoted(r.object_id) << "," << quoted(r.qualifier) << "]";
  }
  o << "],\"o2o\":[";
  for (std::size_t k = 0; k < log.o2o.size(); ++k) {
    const auto& r = log.o2o[k];
    o << (k ? "," : "") << "[" << quoted(r.source_id) << "," << quoted(r.target_id) << "," << quoted(r.qualifier) << "]";
  }
  o << "],\"changes\":[";
  for (std::size_t k = 0; k < log.object_changes.size(); ++k) {
    const auto& c = log.object_changes[k];
    o << (k ? "," : "") << "[" << quoted(c.object_id) << "," << quoted(c.object_type) << "," << quoted(c.timestamp)
      << "," << quoted(c.field) << "," << value_text(c.value) << "]";
  }
  o << "]}";
  return o.str();
}

// parse_ocel_json's reading of one document, as the harness driver prints it.
std::string reading(const std::string& json) {
  try {
    return reading_of(parse_ocel_json(json));
  } catch (const std::exception& error) {
    return "{\"error\":" + quoted(error.what()) + "}";
  }
}

void check(const std::string& json, const std::string& want) {
  const auto got = reading(json);
  expect(got == want, "reading of " + json + "\n  got  " + got + "\n  want " + want);
}

void reference_shapes() {
  // an int activity reads as its str()
  check(R"({"ocel:events":{"e1":{"ocel:activity":1,"ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","1","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"o2o":[],"changes":[]})");
  // an int activity next to a float one reads as a float's str()
  check(R"({"ocel:events":{"e1":{"ocel:activity":1,"ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}},"e2":{"ocel:activity":2.5,"ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","1.0","2024-01-01T10:00:00Z",[]],["e2","2.5","2024-01-01T11:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""],["e2","o1",""]],"o2o":[],"changes":[]})");
  // a null activity drops its event
  check(R"({"ocel:events":{"e1":{"ocel:activity":null,"ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}},"e2":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e2","a","2024-01-01T11:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e2","o1",""]],"o2o":[],"changes":[]})");
  // a non-str omap entry names no object
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1",1],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}},"1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"o2o":[],"changes":[]})");
  // a str omap is read character by character
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":"ab","ocel:vmap":{}}},"ocel:objects":{"a":{"ocel:type":"t","ocel:ovmap":{}},"b":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["a","t",[]],["b","t",[]]],"relations":[["e1","a",""],["e1","b",""]],"o2o":[],"changes":[]})");
  // an event without ocel:vmap raises KeyError
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"]}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"KeyError: 'ocel:vmap'"})");
  // an object without ocel:ovmap raises KeyError
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t"}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"KeyError: 'ocel:ovmap'"})");
  // an ocel:activity vmap entry of a kept event is refused
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"ocel:activity":"z"}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"OCEL JSON event e1: pm4py reads its activity as 'z' in the events frame and 'a' in the relations frame (the columns' dtypes differ), which a native OCEL cannot hold"})");
  // a list value of a kept event is refused
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"x":[1,2]}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"OCEL JSON event e1: value 'x' is a list ([1, 2]), which pandas keeps as a Python object, which a native OCEL cannot hold"})");
  // an integer past 64 bits of a kept object is refused
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{"x":100000000000000000000}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"OCEL JSON object o1: value 'x' is an integer past 64 bits (100000000000000000000), which a native OCEL cannot hold"})");
  // NaN and Infinity read as json.load reads them
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"x":NaN,"y":Infinity}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["x","",["f","nan"]],["y","",["f","inf"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"o2o":[],"changes":[]})");
  // an unreadable time raises fromisoformat's error
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"x","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"Invalid isoformat string: 'x'"})");
  // a time that is no str raises
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":5,"ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"'int' object has no attribute 'endswith'"})");
  // an int column with a missing cell is float
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"x":2}},"e2":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["x","",["f","2"]]]],["e2","a","2024-01-01T11:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""],["e2","o1",""]],"o2o":[],"changes":[]})");
  // an integer past int64 next to a missing cell is float
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"x":9223372036854775808}},"e2":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o1"],"ocel:vmap":{"x":1}},"e3":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T12:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["x","",["f","9.2233720368547758e+18"]]]],["e2","a","2024-01-01T11:00:00Z",[["x","",["f","1"]]]],["e3","a","2024-01-01T12:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""],["e2","o1",""],["e3","o1",""]],"o2o":[],"changes":[]})");
  // an event whose only object the objects frame drops stays while another relation remains
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}},"e2":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o2"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{"ocel:type":null}},"o2":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]],["e2","a","2024-01-01T11:00:00Z",[]]],"objects":[["o2","t",[]]],"relations":[["e2","o2",""]],"o2o":[],"changes":[]})");
  // no event or object stays once no relation remains
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{"ocel:type":null}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[],"objects":[],"relations":[],"o2o":[],"changes":[]})");
  // a change of an int object id finds no type and is refused
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["1"],"ocel:vmap":{}}},"ocel:objects":{"1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:objectChanges":[{"ocel:oid":1,"ocel:timestamp":"2024-01-02","ocel:field":"x","x":5}],"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"OCEL JSON object change of 1: its id 1 is no str, so pm4py gives it no object type, which a native OCEL cannot hold"})");
  // a readable change time next to an unreadable one stays text and is refused
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:objectChanges":[{"ocel:oid":"o1","ocel:timestamp":"2024-01-02","ocel:field":"x","x":5},{"ocel:oid":"o1","ocel:timestamp":"abc","ocel:field":"x","x":6}],"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"OCEL JSON object change of o1: pm4py keeps its time '2024-01-02' as text next to a time it cannot read, which a native OCEL cannot hold"})");
  // an unreadable change time alone stays as written
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:objectChanges":[{"ocel:oid":"o1","ocel:timestamp":"abc","ocel:field":"x","x":5}],"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"o2o":[],"changes":[["o1","t","abc","x",["i",5]]]})");
  // a change list starting with a non-dict has no ocel:oid column
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:objectChanges":[5,{"ocel:oid":"o1"}],"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"KeyError: 'ocel:oid'"})");
  // a change value column takes its dtype from every change row
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:objectChanges":[{"ocel:oid":"o1","ocel:timestamp":"2024-01-02","ocel:field":"x","x":5},{"ocel:oid":"zz","ocel:timestamp":"2024-01-03","ocel:field":"x","x":1.5}],"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"o2o":[],"changes":[["o1","t","2024-01-02T00:00:00+00:00","x",["f","5"]]]})");
  // a boolean vmap time next to a parsed time raises
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{"ocel:timestamp":true}},"e2":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T11:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."})");
  // a file without ocel:global-log raises KeyError
  check(R"({"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00","ocel:omap":["o1"],"ocel:vmap":{}}},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}}})",
        R"({"error":"KeyError: 'ocel:global-log'"})");
  // a file without events raises KeyError
  check(R"({"ocel:events":{},"ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:global-log":{},"ocel:global-event":{},"ocel:global-object":{}})",
        R"({"error":"KeyError: 'ocel:timestamp'"})");
}

// Classic JSON is Minera's transport: a log holding an event time
// read_ocel_json cannot read (an empty time of a CSV import, say) travels
// with "minera:unreadableEventTimes" and keeps it in file order; a download
// leaves the marker and the time out and reads as pm4py reads the file.
void transport() {
  Ocel log;
  log.objects.push_back({"o1", "t", {{"n", std::int64_t{2}}}});
  log.objects.push_back({"o2", "t", {}});
  log.events.push_back({"late", "a", "2024-01-02T00:00:00Z", {{"x", std::int64_t{1}}}});
  log.events.push_back({"none", "a", "", {}});
  log.events.push_back({"early", "a", "2024-01-01T00:00:00Z", {{"x", 2.5}}});
  for (const auto* id : {"late", "none", "early"}) log.relations.push_back({id, "o1", ""});
  log.relations.push_back({"early", "o2", "q"});
  log.o2o.push_back({"o1", "o2", "r"});
  log.object_changes.push_back({"o1", "t", "2024-01-03T00:00:00+00:00", "n", std::int64_t{3}});
  const auto json = serialize_ocel_json(log, OcelJsonFormat::kClassic);
  expect(json.find("\"minera:unreadableEventTimes\":true") != std::string::npos, "the transport carries the marker");
  // The columns read as pandas' record frames (x next to 2.5 and n next to
  // o2's missing cell are float), the change keeps its int.
  const auto round = reading(json);
  expect(round ==
             R"({"events":[["late","a","2024-01-02T00:00:00Z",[["x","int",["f","1"]]]],["none","a","",[]],)"
             R"(["early","a","2024-01-01T00:00:00Z",[["x","int",["f","2.5"]]]]],"objects":[["o1","t",[["n","int",["f","2"]]]],)"
             R"(["o2","t",[]]],"relations":[["late","o1",""],["none","o1",""],["early","o1",""],["early","o2","q"]],)"
             R"("o2o":[["o1","o2","r"]],"changes":[["o1","t","2024-01-03T00:00:00+00:00","n",["i",3]]]})",
         "the transport reads back in file order: " + round);
  const auto download = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(download.find("minera:unreadableEventTimes") == std::string::npos, "a download leaves the marker out");
  // Round EG: pm4py's write_ocel_json leaves a NaT time out of the event
  // entry, so reading the download back raises on the missing key.
  expect(reading(download) == R"({"error":"KeyError: 'ocel:timestamp'"})",
         "a download reads as pm4py reads it: " + reading(download));
  bool refused = false;
  try {
    (void)parse_ocel_json(R"({"ocel:events":{},"ocel:objects":{},"minera:unreadableEventTimes":1})");
  } catch (const std::invalid_argument& error) {
    refused = std::string(error.what()) == "minera:unreadableEventTimes must be boolean";
  }
  expect(refused, "the marker must be boolean");
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
    const auto want = line.substr(tab + 1);
    const auto got = reading(line.substr(0, tab));
    ++count;
    if (got != want && ++mismatches <= 10)
      std::cerr << "bank: " << line.substr(0, tab) << "\n  got  " << got << "\n  want " << want << '\n';
  }
  expect(count == 2056, "the bank holds 2,056 files: " + std::to_string(count));
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  reference_shapes();
  transport();
  if (argc > 1 && replay_bank(argv[1]) != 0) return 1;
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EE1 tests passed\n";
  return 0;
}
