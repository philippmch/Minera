// Review ED1: pm4py 2.7.22's read_ocel_xml on OCEL 1.0 XML files whose events
// and objects carry their id as a keyed child (the shape pm4py writes).
// Observed black-box (review-harness round-ed, cl/pr1.py-pr14.py probes and
// cl/fz2.py, 0 differences over 4,500 fuzzed files besides declared shapes):
//
// - children count by their exact key attribute whatever their tag (an event
//   reads "id", "activity", "timestamp", every "omap" and every "vmap"; an
//   object "id", "type" and every "ovmap"; "type", "time", "event-id",
//   "object-id", "object-type" and name= are no keys), and every child of
//   <events> or <objects> is an event or an object; the last of a repeated
//   key wins;
// - id, activity and type take the value attribute as written (none drops
//   the event or object); a time, a vmap and an ovmap value read by tag: a
//   tag naming "float" in any case is float(value), else one naming "date"
//   is fromisoformat(value) stamped UTC, else the value text, "None" without
//   one (<int>, <boolean> and nested lists stay texts);
// - the time column sorts as pandas sorts it: dates (or floats) first by
//   value, then texts by code points, a missing time or NaN last, each
//   stably; dates next to floats raise;
// - vmap lists merge, a repeated key's last value winning at its first
//   position; an object keeps an ovmap key's first value, and a later entry
//   under the same key (ocel:oid and ocel:type are held from the start) is an
//   object change at its timestamp attribute, read after the object's values
//   (none raises AttributeError);
// - values convert as the file is walked, so the first unreadable one raises
//   float()'s or fromisoformat's message in document order.
//
// Declared (see docs/reference-2.7.22-parity.md, ED1): keyless values (a NaN
// column) are dropped; vmap entries keyed ocel:eid/ocel:activity/
// ocel:timestamp are refused, and so are object changes of ocel:oid/ocel:type
// whose change row survives the filtering (pm4py writes them into the change
// frame's own columns); processing instructions in a vmap are skipped; texts
// and floats kept as event times read as times downstream where native can.
//
// With a bank path (docs/ocel-xml-reference-ed-bank.tsv.gz) the test also
// replays 1,336 files read on 2026-11-18: the file, a tab, then the reading
// as reading() prints it.
#include "minera/ocel.hpp"
#include "minera/ocel_xml.hpp"
#include "../gzip_codec.hpp"
#include "../sqlite_real_text.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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

// parse_ocel_xml's reading of one file, as the harness driver prints it.
std::string reading(const std::string& xml) {
  std::ostringstream o;
  try {
    const auto log = parse_ocel_xml(xml);
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
    o << "],\"changes\":[";
    for (std::size_t k = 0; k < log.object_changes.size(); ++k) {
      const auto& c = log.object_changes[k];
      o << (k ? "," : "") << "[" << quoted(c.object_id) << "," << quoted(c.object_type) << "," << quoted(c.timestamp)
        << "," << quoted(c.field) << "," << value_text(c.value) << "]";
    }
    o << "]}";
  } catch (const std::exception& error) {
    o.str("");
    o << "{\"error\":" << quoted(error.what()) << "}";
  }
  return o.str();
}

const std::string kGlobals =
    R"(<global scope="event"><string key="activity" value="__INVALID__"/></global><global scope="object"><string key="type" value="__INVALID__"/></global><global scope="log"><list key="attribute-names"/><list key="object-types"><string key="object-type" value="t"/></list><string key="version" value="1.0"/><string key="ordering" value="timestamp"/></global>)";

void check(const std::string& events, const std::string& objects, const std::string& want) {
  const auto got = reading("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log>" + kGlobals + "<events>" + events +
                           "</events><objects>" + objects + "</objects></log>");
  expect(got == want, "reading of " + events + objects + "\n  got  " + got + "\n  want " + want);
}

void reference_shapes() {
  // a keyed child counts whatever its tag
  check(R"(<event><foo key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // a repeated key: the last activity wins
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/><string key="activity" value="b"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","b","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // the last id wins
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/><string key="id" value="e9"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e9","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e9","o1",""]],"changes":[]})");
  // the last object type wins
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/><string key="type" value="u"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","u",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // "type" is no activity key, so the event has none and is dropped
  check(R"(<event><string key="id" value="e1"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/><string key="type" value="b"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[],"objects":[],"relations":[],"changes":[]})");
  // "time" is no timestamp key: a missing time sorts last
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/><date key="time" value="2024-01-01T10:00:00"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e2","a","2024-01-01T10:00:00Z",[]],["e1","a","",[]]],"objects":[["o1","t",[]]],"relations":[["e2","o1",""],["e1","o1",""]],"changes":[]})");
  // every child of <events> is an event
  check(R"(<foo><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></foo><event><string key="id" value="e2"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-02T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]],["e2","a","2024-01-02T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""],["e2","o1",""]],"changes":[]})");
  // a <string> timestamp stays a text
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><string key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // texts sort as Python sorts them, stably
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><string key="timestamp" value="2024-01-02"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e1"/><string key="activity" value="a"/><string key="timestamp" value="2024-01-01T10:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><string key="timestamp" value="b"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e3"/><string key="activity" value="a"/><string key="timestamp" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e4"/><string key="activity" value="a"/><string key="timestamp" value="2024-01-01T10:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00",[]],["e4","a","2024-01-01T10:00",[]],["e0","a","2024-01-02",[]],["e3","a","a",[]],["e2","a","b",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""],["e4","o1",""],["e0","o1",""],["e3","o1",""],["e2","o1",""]],"changes":[]})");
  // dates first, then texts
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-03T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e1"/><string key="activity" value="a"/><string key="timestamp" value="b"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-02T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e3"/><string key="activity" value="a"/><string key="timestamp" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e4"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e4","a","2024-01-01T10:00:00Z",[]],["e2","a","2024-01-02T10:00:00Z",[]],["e0","a","2024-01-03T10:00:00Z",[]],["e3","a","a",[]],["e1","a","b",[]]],"objects":[["o1","t",[]]],"relations":[["e4","o1",""],["e2","o1",""],["e0","o1",""],["e3","o1",""],["e1","o1",""]],"changes":[]})");
  // floats first, then texts
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><float key="timestamp" value="5"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e1"/><string key="activity" value="a"/><string key="timestamp" value="b"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><float key="timestamp" value="3"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e3"/><string key="activity" value="a"/><string key="timestamp" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e2","a","3.0",[]],["e0","a","5.0",[]],["e3","a","a",[]],["e1","a","b",[]]],"objects":[["o1","t",[]]],"relations":[["e2","o1",""],["e0","o1",""],["e3","o1",""],["e1","o1",""]],"changes":[]})");
  // floats by value, NaN last
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><float key="timestamp" value="nan"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e1"/><string key="activity" value="a"/><float key="timestamp" value="3"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><float key="timestamp" value="inf"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e3"/><string key="activity" value="a"/><float key="timestamp" value="-inf"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e3","a","-inf",[]],["e1","a","3.0",[]],["e2","a","inf",[]],["e0","a","",[]]],"objects":[["o1","t",[]]],"relations":[["e3","o1",""],["e1","o1",""],["e2","o1",""],["e0","o1",""]],"changes":[]})");
  // dates next to floats raise pandas' error
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><float key="timestamp" value="5"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"error":"'values' is not ordered, please explicitly specify the categories order by passing in a categories argument."})");
  // an event without a time sorts last
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event><event><string key="id" value="e2"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e2","a","2024-01-01T10:00:00Z",[]],["e1","a","",[]]],"objects":[["o1","t",[]]],"relations":[["e2","o1",""],["e1","o1",""]],"changes":[]})");
  // <int> and <boolean> values stay texts, a <float> is a float
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><int key="m" value="5"/><boolean key="b" value="true"/><float key="f" value="1"/></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["m","",["s","5"]],["b","",["s","true"]],["f","",["f","1"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // float() reads spaces, 1e400 and underscores
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><int key="i" value="x"/><boolean key="b" value="maybe"/><float key="f" value=" 2.5 "/><float key="g" value="1e400"/><float key="h" value="1_0"/></list></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[["i","",["s","x"]],["b","",["s","maybe"]],["f","",["f","2.5"]],["g","",["f","inf"]],["h","",["f","10"]]]]],"relations":[["e1","o1",""]],"changes":[]})");
  // a value without a value attribute is "None"
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><string key="n"/></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["n","",["s","None"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // element text is no value
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><string key="n">txt</string></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["n","",["s","None"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // a nested list is a value too ("None")
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><list key="l"><string key="a" value="1"/></list></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["l","",["s","None"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // vmap lists merge, the last value of a key winning
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><string key="n" value="4"/></list><list key="vmap"><string key="k" value="7"/></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[["n","",["s","4"]],["k","",["s","7"]]]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // values are read in document order, so a bad first time raises
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/><date key="timestamp" value="x"/><string key="timestamp" value="ok"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"error":"Invalid isoformat string: 'x'"})");
  // float()'s message
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><float key="timestamp" value="x"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"error":"could not convert string to float: 'x'"})");
  // a date without a value
  check(R"(<event><string key="id" value="e0"/><string key="activity" value="a"/><date key="timestamp"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"error":"'NoneType' object has no attribute 'endswith'"})");
  // a repeated ovmap key is an object change at its timestamp attribute
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><string key="y" value="a"/><string key="y" value="b" timestamp="2024-01-05T00:00:00"/><float key="y" value="3" timestamp="2024-01-04T00:00:00"/></list></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[["y","",["s","a"]]]]],"relations":[["e1","o1",""]],"changes":[["o1","t","2024-01-05T00:00:00+00:00","y",["s","b"]],["o1","t","2024-01-04T00:00:00+00:00","y",["f","3"]]]})");
  // ocel:type is held from the start: an ovmap entry for it needs a timestamp
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><string key="ocel:type" value="u"/></list></object>)",
        R"({"error":"'NoneType' object has no attribute 'endswith'"})");
  // "object-id" is no id key: the relation names an unlisted object
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="type" value="t"/><list key="ovmap"/><string key="object-id" value="o1"/></object>)",
        R"({"error":"OCEL XML relation references unknown object"})");
  // declared: a vmap ocel:activity entry is refused
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><string key="ocel:activity" value="z"/></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"error":"OCEL XML event vmap entry ocel:activity: pm4py puts it in the events frame and keeps the event's own value in the relations frame, which a native OCEL cannot hold"})");
  // an ocel:type change of an object the filtering drops goes with it
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object><object><string key="id" value="o2"/><string key="type" value="t"/><list key="ovmap"><string key="ocel:type" value="u" timestamp="2024-01-02T00:00:00"/></list></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // an ocel:oid change's row names the object its value names: none here
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><string key="ocel:oid" value="o9" timestamp="2024-01-02T00:00:00"/></list></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
  // declared: an ocel:oid change whose row names a kept object is refused
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object><object><string key="id" value="o2"/><string key="type" value="t"/><list key="ovmap"><string key="ocel:oid" value="o1" timestamp="2024-01-02T00:00:00"/></list></object>)",
        R"({"error":"OCEL XML object change of ocel:oid: pm4py writes it into the change frame's own column, which a native OCEL cannot hold"})");
  // declared: an ocel:type change of a kept object is refused
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"/></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><string key="ocel:type" value="u" timestamp="2024-01-02T00:00:00"/></list></object>)",
        R"({"error":"OCEL XML object change of ocel:type: pm4py writes it into the change frame's own column, which a native OCEL cannot hold"})");
  // declared: a keyless value is dropped
  check(R"(<event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00"/><list key="omap"><string key="object-id" value="o1"/></list><list key="vmap"><string value="4"/></list></event>)",
        R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"/></object>)",
        R"({"events":[["e1","a","2024-01-01T10:00:00Z",[]]],"objects":[["o1","t",[]]],"relations":[["e1","o1",""]],"changes":[]})");
}

// ED1-7: SQLite REAL cells spelled as SQLite 3.45 spells them on every
// platform (Windows' winsqlite3 gave 17 digits through sqlite3_snprintf too).
void sqlite_real_texts() {
  const double inf = std::numeric_limits<double>::infinity();
  const std::pair<double, const char*> cases[] = {
      {210234042488.69626, "210234042488.696"}, {616388229777.79272, "616388229777.793"},
      {37743464580.170082, "37743464580.1701"}, {2.0, "2.0"}, {-2.5, "-2.5"}, {0.1, "0.1"},
      {1e20, "1.0e+20"}, {1e15, "1.0e+15"}, {999999999999999.0, "999999999999999.0"},
      {99999999999999.953, "100000000000000.0"}, {1.5e-5, "1.5e-05"}, {0.0001, "0.0001"},
      {0.00012345678901234567, "0.000123456789012346"}, {123456789012345678.0, "1.23456789012346e+17"},
      {1e100, "1.0e+100"}, {5e-324, "4.94065645841247e-324"}, {-0.0, "0.0"}, {inf, "Inf"}, {-inf, "-Inf"}};
  for (const auto& [value, text] : cases)
    expect(sqlite_legacy_real_text(value) == text,
           "SQLite REAL text " + std::to_string(value) + ": " + sqlite_legacy_real_text(value) + " vs " + text);
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
  expect(count == 1336, "the bank holds 1,336 files: " + std::to_string(count));
  expect(mismatches == 0, "bank mismatches: " + std::to_string(mismatches));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  reference_shapes();
  sqlite_real_texts();
  if (argc > 1 && replay_bank(argv[1]) != 0) return 1;
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review ED1 tests passed\n";
  return 0;
}
