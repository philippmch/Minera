#include "minera/ocel_xml.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_sqlite.hpp"

#include <chrono>
#include <filesystem>

#include <cassert>
#include <cstdint>
#include <regex>
#include <stdexcept>
#include <string>
#include <sstream>
#include <variant>

namespace {

// Captured with PM4Py 2.7.22: read_ocel2_json on kFixture, then
// write_ocel2_xml (kReferenceXml2) and, on the copy with whole-second
// timestamps, write_ocel (kReferenceXml1). Event attribute types follow the
// reference frame: an int or bool column is declared "string", a float
// column "float"; integers in a float column print as floats.
constexpr const char* kFixture = R"REF({"objectTypes":[{"name":"order","attributes":[{"name":"color","type":"string"}]},{"name":"item","attributes":[{"name":"weight","type":"float"}]}],
 "eventTypes":[{"name":"create","attributes":[]},{"name":"pick, item","attributes":[]}],
 "objects":[
  {"id":"o1","type":"order","attributes":[{"name":"color","time":"1970-01-01T00:00:00Z","value":"red"},{"name":"color","time":"2020-01-02T00:00:00Z","value":"blue"}],"relationships":[{"objectId":"i1","qualifier":"contains"}]},
  {"id":"i1","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":2.5}]},
  {"id":"i'2","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":3}]}],
 "events":[
  {"id":"e1","type":"create","time":"2020-01-01T00:00:00Z","attributes":[{"name":"n","value":4},{"name":"flag","value":false},{"name":"cost","value":0.1},{"name":"note","value":"a<b&\"c\"\nd"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},
  {"id":"e2","type":"pick, item","time":"2020-01-01T00:00:01.250000Z","attributes":[{"name":"n","value":5},{"name":"flag","value":true}],"relationships":[{"objectId":"i'2","qualifier":""}]},
  {"id":"e3","type":"create","time":"2020-01-01T00:00:02.000001Z","attributes":[{"name":"n","value":-6},{"name":"flag","value":true},{"name":"cost","value":1e20}],"relationships":[{"objectId":"i1","qualifier":""}]},
  {"id":"e4","type":"pick, item","time":"2020-01-01T00:00:03Z","attributes":[{"name":"n","value":7},{"name":"flag","value":false}],"relationships":[{"objectId":"o1","qualifier":"r"}]}]})REF";

constexpr const char* kReferenceXml2 = R"REF(<?xml version='1.0' encoding='UTF-8'?>
<log>
  <object-types>
    <object-type name="item">
      <attributes>
        <attribute name="weight" type="float"/>
      </attributes>
    </object-type>
    <object-type name="order">
      <attributes>
        <attribute name="color" type="string"/>
      </attributes>
    </object-type>
  </object-types>
  <event-types>
    <event-type name="create">
      <attributes>
        <attribute name="n" type="string"/>
        <attribute name="flag" type="string"/>
        <attribute name="cost" type="float"/>
        <attribute name="note" type="string"/>
      </attributes>
    </event-type>
    <event-type name="pick, item">
      <attributes>
        <attribute name="n" type="string"/>
        <attribute name="flag" type="string"/>
      </attributes>
    </event-type>
  </event-types>
  <objects>
    <object id="o1" type="order">
      <attributes>
        <attribute name="color" time="1970-01-01T00:00:00Z">red</attribute>
        <attribute name="color" time="2020-01-02T00:00:00+00:00">blue</attribute>
      </attributes>
      <objects>
        <relationship object-id="i1" qualifier="contains"/>
      </objects>
    </object>
    <object id="i1" type="item">
      <attributes>
        <attribute name="weight" time="1970-01-01T00:00:00Z">2.5</attribute>
      </attributes>
    </object>
    <object id="i'2" type="item">
      <attributes>
        <attribute name="weight" time="1970-01-01T00:00:00Z">3.0</attribute>
      </attributes>
    </object>
  </objects>
  <events>
    <event id="e1" type="create" time="2020-01-01T00:00:00+00:00">
      <attributes>
        <attribute name="n">4</attribute>
        <attribute name="flag">False</attribute>
        <attribute name="cost">0.1</attribute>
        <attribute name="note">a&lt;b&amp;"c"
d</attribute>
      </attributes>
      <objects>
        <relationship object-id="o1" qualifier="q"/>
      </objects>
    </event>
    <event id="e2" type="pick, item" time="2020-01-01T00:00:01.250000+00:00">
      <attributes>
        <attribute name="n">5</attribute>
        <attribute name="flag">True</attribute>
      </attributes>
      <objects>
        <relationship object-id="i'2" qualifier=""/>
      </objects>
    </event>
    <event id="e3" type="create" time="2020-01-01T00:00:02.000001+00:00">
      <attributes>
        <attribute name="n">-6</attribute>
        <attribute name="flag">True</attribute>
        <attribute name="cost">1e+20</attribute>
      </attributes>
      <objects>
        <relationship object-id="i1" qualifier=""/>
      </objects>
    </event>
    <event id="e4" type="pick, item" time="2020-01-01T00:00:03+00:00">
      <attributes>
        <attribute name="n">7</attribute>
        <attribute name="flag">False</attribute>
      </attributes>
      <objects>
        <relationship object-id="o1" qualifier="r"/>
      </objects>
    </event>
  </events>
</log>
)REF";

constexpr const char* kReferenceXml1 = R"REF(<?xml version='1.0' encoding='UTF-8'?>
<log>
  <global scope="event"/>
  <global scope="object"/>
  <global scope="log">
    <list key="attribute-names">
      <string key="attribute-name" value="color"/>
      <string key="attribute-name" value="cost"/>
      <string key="attribute-name" value="flag"/>
      <string key="attribute-name" value="n"/>
      <string key="attribute-name" value="note"/>
      <string key="attribute-name" value="weight"/>
    </list>
    <list key="object-types">
      <string key="object-type" value="order"/>
      <string key="object-type" value="item"/>
    </list>
    <string key="version" value="1.0"/>
    <string key="ordering" value="timestamp"/>
  </global>
  <events>
    <event>
      <string key="id" value="e1"/>
      <date key="timestamp" value="2020-01-01T00:00:00Z"/>
      <string key="activity" value="create"/>
      <list key="omap">
        <string key="object-id" value="o1"/>
      </list>
      <list key="vmap">
        <string key="n" value="4"/>
        <string key="flag" value="False"/>
        <float key="cost" value="0.1"/>
        <string key="note" value="a&lt;b&amp;&quot;c&quot;&#10;d"/>
      </list>
    </event>
    <event>
      <string key="id" value="e2"/>
      <date key="timestamp" value="2020-01-01T00:00:01Z"/>
      <string key="activity" value="pick, item"/>
      <list key="omap">
        <string key="object-id" value="i'2"/>
      </list>
      <list key="vmap">
        <string key="n" value="5"/>
        <string key="flag" value="True"/>
      </list>
    </event>
    <event>
      <string key="id" value="e3"/>
      <date key="timestamp" value="2020-01-01T00:00:02Z"/>
      <string key="activity" value="create"/>
      <list key="omap">
        <string key="object-id" value="i1"/>
      </list>
      <list key="vmap">
        <string key="n" value="-6"/>
        <string key="flag" value="True"/>
        <float key="cost" value="1e+20"/>
      </list>
    </event>
    <event>
      <string key="id" value="e4"/>
      <date key="timestamp" value="2020-01-01T00:00:03Z"/>
      <string key="activity" value="pick, item"/>
      <list key="omap">
        <string key="object-id" value="o1"/>
      </list>
      <list key="vmap">
        <string key="n" value="7"/>
        <string key="flag" value="False"/>
      </list>
    </event>
  </events>
  <objects>
    <object>
      <string key="id" value="o1"/>
      <string key="type" value="order"/>
      <list key="ovmap">
        <string key="color" value="red"/>
      </list>
    </object>
    <object>
      <string key="id" value="i1"/>
      <string key="type" value="item"/>
      <list key="ovmap">
        <float key="weight" value="2.5"/>
      </list>
    </object>
    <object>
      <string key="id" value="i'2"/>
      <string key="type" value="item"/>
      <list key="ovmap">
        <float key="weight" value="3.0"/>
      </list>
    </object>
  </objects>
</log>
)REF";

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    text.replace(at, from.size(), to);
  return text;
}

void reference_capture_tests() {
  const auto log = minera::parse_ocel_json(kFixture);

  // OCEL 2.0 XML: the reference document, except that Minera also names
  // each value's type (a declared divergence the reference reader ignores).
  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  const std::regex value_type(R"((<attribute [^>]*) type="[a-z]+">)");
  assert(std::regex_replace(xml2, value_type, "$1>") == kReferenceXml2);
  assert(xml2.find("<attribute name=\"n\" time=") == std::string::npos);
  // O2O links come back from the reference's <objects> form.
  const auto reread2 = minera::parse_ocel_xml(kReferenceXml2);
  assert(reread2.o2o.size() == 1 && reread2.o2o[0].source_id == "o1" &&
         reread2.o2o[0].target_id == "i1" && reread2.o2o[0].qualifier == "contains");
  assert(reread2.object_changes.size() == 1);
  assert(minera::parse_ocel_xml(xml2).o2o.size() == 1);

  // The reference reader reads an attribute element without text as "None"
  // (string, integer, boolean and time declarations alike) and raises for a
  // float; a native file names the value's type and keeps the empty string.
  const auto empty_text = [](const std::string& type, const std::string& value) {
    return "<log><event-types><event-type name=\"A\"><attributes><attribute name=\"a\" type=\"" + type +
           "\"/></attributes></event-type></event-types><objects><object id=\"o1\" type=\"t\"/></objects>"
           "<events><event id=\"e1\" type=\"A\" time=\"2020-01-01T00:00:00Z\"><attributes>" + value +
           "</attributes><objects><relationship object-id=\"o1\" qualifier=\"\"/></objects></event></events></log>";
  };
  for (const char* type : {"string", "integer", "boolean", "time"})
    assert(minera::parse_ocel_xml(empty_text(type, "<attribute name=\"a\"></attribute>"))
               .events[0].attributes[0].value == minera::OcelValue(std::string("None")));
  assert(minera::parse_ocel_xml(empty_text("string", "<attribute name=\"a\"> </attribute>"))
             .events[0].attributes[0].value == minera::OcelValue(std::string(" ")));
  bool float_raises = false;
  try {
    (void)minera::parse_ocel_xml(empty_text("float", "<attribute name=\"a\"/>"));
  } catch (const std::invalid_argument&) {
    float_raises = true;
  }
  assert(float_raises);
  minera::Ocel empty_string;
  empty_string.objects.push_back({"o1", "t", {}});
  empty_string.events.push_back({"e1", "A", "2020-01-01T00:00:00Z", {{"a", std::string()}}});
  empty_string.relations.push_back({"e1", "o1", ""});
  assert(minera::parse_ocel_xml(minera::serialize_ocel_xml(empty_string, standard))
             .events[0].attributes[0].value == minera::OcelValue(std::string()));

  // OCEL 1.0 XML: byte for byte what the reference writes. Its qualifiers,
  // O2O link and object change need the loss allowance.
  auto whole_seconds = replace_all(kFixture, "00:00:01.250000Z", "00:00:01Z");
  whole_seconds = replace_all(whole_seconds, "00:00:02.000001Z", "00:00:02Z");
  const auto seconds_log = minera::parse_ocel_json(whole_seconds);
  bool refused = false;
  try { (void)minera::serialize_ocel_xml(seconds_log); } catch (const std::invalid_argument&) { refused = true; }
  assert(refused);
  minera::OcelXmlExportOptions lossy;
  lossy.allow_lossy = true;
  assert(minera::serialize_ocel_xml(seconds_log, lossy) == kReferenceXml1);
  const auto reread1 = minera::parse_ocel_xml(kReferenceXml1);
  assert(reread1.events.size() == 4 && reread1.objects.size() == 3 && reread1.relations.size() == 4);
  assert(reread1.events[0].attributes.size() == 4 &&
         reread1.events[0].attributes[3].value == minera::OcelValue(std::string("a<b&\"c\"\nd")));
}

// The reference writers follow the events frame's column order: id,
// activity, timestamp after either XML reader (or a CSV or SQLite table in
// that order), id, timestamp, activity after JSON. read_ocel_xml reads only
// the vmap/ovmap lists of a reference-shaped event or object; other keyed
// children are dropped (both observed black-box).
void frame_order_and_unlisted_keys() {
  const std::string xml = R"xml(<log>
  <events>
    <event>
      <string key="id" value="e1"/>
      <string key="activity" value="Create"/>
      <date key="timestamp" value="2024-01-01T10:00:00Z"/>
      <string key="foo" value="w"/>
      <list key="omap"><string key="object-id" value="o1"/></list>
      <list key="vmap"><string key="amount" value="1"/></list>
      <date key="later" value="2024-01-05T00:00:00Z"/>
    </event>
  </events>
  <objects>
    <object>
      <string key="id" value="o1"/>
      <string key="type" value="order"/>
      <string key="bar" value="z"/>
      <list key="ovmap"><string key="status" value="open"/></list>
      <float key="baz" value="1.5"/>
    </object>
  </objects>
</log>)xml";
  const auto log = minera::parse_ocel_xml(xml);
  assert(log.event_activity_first);
  assert(log.events[0].attributes.size() == 1 && log.events[0].attributes[0].key == "amount");
  assert(log.objects[0].attributes.size() == 1 && log.objects[0].attributes[0].key == "status");
  assert(!log.event_type_attributes.at("Create").contains("later"));
  minera::OcelXmlExportOptions lossy;
  lossy.allow_lossy = true;
  const auto written = minera::serialize_ocel_xml(log, lossy);
  const auto activity = written.find("<string key=\"activity\" value=\"Create\"/>");
  const auto timestamp = written.find("<date key=\"timestamp\" value=\"2024-01-01T10:00:00Z\"/>");
  assert(activity != std::string::npos && timestamp != std::string::npos && activity < timestamp);
  assert(written.find("foo") == std::string::npos && written.find("bar") == std::string::npos);
  // Classic JSON carries the order between routes; OCEL 2.0 JSON does not.
  const auto classic = minera::serialize_ocel_json(log);
  assert(classic.find("\"minera:eventActivityFirst\":true") != std::string::npos);
  assert(minera::parse_ocel_json(classic).event_activity_first);
  // A download leaves the marker out and reads as the reference reads it.
  const auto file = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kClassic, false);
  assert(file.find("minera:eventActivityFirst") == std::string::npos);
  assert(!minera::parse_ocel_json(file).event_activity_first);
  const auto standard = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  assert(!minera::parse_ocel_json(standard).event_activity_first);
  const auto from_json = minera::serialize_ocel_xml(minera::parse_ocel_json(standard), lossy);
  assert(from_json.find("<string key=\"activity\"") > from_json.find("<date key=\"timestamp\""));
  // The former native dialect (id and type as XML attributes) keeps them.
  const auto former = minera::parse_ocel_xml(
      "<log><events><event id=\"e1\" activity=\"A\" timestamp=\"2024-01-01T00:00:00Z\">"
      "<string key=\"foo\" value=\"w\"/><omap><object object-id=\"o1\"/></omap></event></events>"
      "<objects><object id=\"o1\" type=\"order\"><string key=\"bar\" value=\"z\"/></object></objects></log>");
  assert(former.events[0].attributes.size() == 1 && former.events[0].attributes[0].key == "foo");
  assert(former.objects[0].attributes.size() == 1 && former.objects[0].attributes[0].key == "bar");
}

}  // namespace

// An OCEL read from OCEL 2.0 XML declares no global scope, so the classic
// JSON the routes pass along carries the OCEL 1.0 writers' "__INVALID__"
// scope defaults. Writing that JSON back as OCEL 2.0 XML or SQLite leaves
// those reference-generated globals out (as the OCEL 2.0 JSON writer does)
// and keeps a real global.
void standard_writers_drop_reference_generated_globals() {
  minera::Ocel log;
  log.objects.push_back({"o1", "order", {}});
  log.events.push_back({"e1", "create", "2020-01-01T00:00:00Z", {}});
  log.relations.push_back({"e1", "o1", ""});
  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  const auto imported = minera::parse_ocel_xml(minera::serialize_ocel_xml(log, standard));
  assert(imported.globals.empty() && !imported.global_event_scope);
  const auto passed = minera::parse_ocel_json(minera::serialize_ocel_json(imported));
  assert(passed.globals.at("ocel:global-event.ocel:activity") ==
         minera::OcelValue(std::string("__INVALID__")));
  const auto xml = minera::serialize_ocel_xml(passed, standard);
  assert(xml.find("<globals>") == std::string::npos);
  assert(xml.find("__INVALID__") == std::string::npos);
  const auto path = std::filesystem::temp_directory_path() /
                    ("minera_ocel_globals_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
  std::filesystem::remove(path);
  minera::export_ocel_sqlite(passed, path);
  assert(minera::import_ocel_sqlite(path).globals.empty());
  std::filesystem::remove(path);

  auto custom = passed;
  custom.globals["ocel:global-event.version"] = std::string("2");
  const auto kept = minera::serialize_ocel_xml(custom, standard);
  assert(kept.find("ocel:global-event.version") != std::string::npos);
  assert(kept.find("__INVALID__") == std::string::npos);
  minera::export_ocel_sqlite(custom, path);
  const auto reread = minera::import_ocel_sqlite(path).globals;
  assert(reread.size() == 1 && reread.count("ocel:global-event.version") == 1);
  std::filesystem::remove(path);
}

int main() {
  frame_order_and_unlisted_keys();
  standard_writers_drop_reference_generated_globals();
  minera::Ocel log;
  log.objects.push_back({"o2", "Item", {{"price", std::int64_t(7)}}});
  log.objects.push_back({"o1", "Order", {}});
  log.events.push_back({"e1", "create", "2024-01-01T00:00:00Z", {{"ok", true}}});
  log.events.push_back({"e2", "ship", "2024-01-02T00:00:00Z", {}});
  log.relations.push_back({"e1", "o1", "owner"});
  // Every event and object takes part in a relation: the readers drop the
  // others, as the reference readers do (checked separately below).
  log.relations.push_back({"e2", "o2", "item"});
  log.o2o.push_back({"o1", "o2", "contains"});
  log.e2e.push_back({"e1", "e2", "precedes"});
  log.object_changes.push_back({"o1", "Order", "2024-01-02T00:00:00Z", "status", std::string("paid")});
  log.globals["ocel:global-event.version"] = std::string("1");

  // OCEL 1.0 XML has no qualifiers, O2O/E2E links or object changes: the
  // writer refuses to drop them unless loss is allowed, then writes what the
  // reference writer writes.
  bool refused = false;
  try { (void)minera::serialize_ocel_xml(log); } catch (const std::invalid_argument&) { refused = true; }
  assert(refused);
  minera::OcelXmlExportOptions lossy;
  lossy.allow_lossy = true;
  const auto classic = minera::serialize_ocel_xml(log, lossy);
  const auto parsed = minera::parse_ocel_xml(classic);
  assert(parsed.events.size() == 2 && parsed.objects.size() == 2);
  assert(parsed.relations.front().qualifier.empty());
  assert(parsed.o2o.empty() && parsed.e2e.empty() && parsed.object_changes.empty());
  // The writer keeps the event global; read_ocel_xml reads no global scope
  // (observed black-box), so the read declares none and holds no globals.
  assert(classic.find("<global scope=\"event\">\n    <string key=\"version\" value=\"1\"/>") !=
         std::string::npos);
  assert(classic.find("<string key=\"ocel:type\" value=\"__INVALID__\"/>") != std::string::npos);
  assert(parsed.globals.empty() && !parsed.global_event_scope && !parsed.global_object_scope);
  // Booleans are written as text, as the reference writes them; the integer
  // price sits in a column with a missing cell, a float column in the
  // reference frame, so it is written as the float 7.0.
  assert(parsed.objects[0].attributes[0].value == minera::OcelValue(7.0));
  assert(parsed.events[0].attributes[0].value == minera::OcelValue(std::string("True")));
  assert(classic == minera::serialize_ocel_xml(log, lossy));
  reference_capture_tests();

  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  assert(xml2.find("<event-type name=\"create\">") != std::string::npos);
  assert(xml2.find("time=\"1970-01-01T00:00:00Z\"") !=
         std::string::npos);
  const auto parsed2 = minera::parse_ocel_xml(xml2);
  // o1 has no base "status": like PM4Py 2.7.22 write_ocel2_xml no null base
  // entry is written, so the single change reads back as o1's base value.
  assert(parsed2.events.size() == 2 && parsed2.relations.size() == 2 &&
         parsed2.object_changes.empty());
  for (const auto& object : parsed2.objects)
    if (object.id == "o1")
      assert(object.attributes.size() == 1 && object.attributes[0].key == "status" &&
             object.attributes[0].value == minera::OcelValue(std::string("paid")));

  std::istringstream input(classic);
  assert(minera::import_ocel_xml(input).objects.size() == 2);
  bool malformed = false;
  try { (void)minera::parse_ocel_xml("<log><events>"); } catch (...) { malformed = true; }
  assert(malformed);

  const std::string keyed_classic = R"xml(
<log>
  <global scope="log"><string key="version" value="1.0"/></global>
  <events>
    <event>
      <string key="id" value="e1"/>
      <date key="timestamp" value="2024-01-01T00:00:00Z"/>
      <string key="activity" value="create"/>
      <list key="omap"><string key="object-id" value="o1" qualifier="owner"/></list>
      <list key="vmap"><int key="amount" value="7"/></list>
    </event>
  </events>
  <objects>
    <object>
      <string key="id" value="o1"/>
      <string key="type" value="Order"/>
      <list key="ovmap"><string key="state" value="new"/></list>
    </object>
  </objects>
</log>
)xml";
  const auto keyed = minera::parse_ocel_xml(keyed_classic);
  assert(keyed.objects.size() == 1 && keyed.objects.front().id == "o1");
  assert(keyed.objects.front().attributes.size() == 1);
  assert(keyed.objects.front().attributes.front().key == "state");
  assert(keyed.events.size() == 1 && keyed.events.front().activity == "create");
  assert(keyed.relations.size() == 1 && keyed.relations.front().qualifier == "owner");
  assert(std::get<std::int64_t>(keyed.events.front().attributes.front().value) == 7);
  // read_ocel_xml ignores the <global> scopes.
  assert(keyed.globals.empty());

  const std::string standard_links = R"xml(
<log>
  <object-types><object-type name="Order"/></object-types>
  <event-types><event-type name="ship"/></event-types>
  <objects>
    <object id="o1" type="Order">
      <attributes><attribute name="state" type="string">new</attribute></attributes>
      <objects><relationship object-id="o2" qualifier="contains"/></objects>
    </object>
    <object id="o2" type="Item"><attributes/></object>
  </objects>
  <events>
    <event id="e1" type="ship" time="2024-01-01T00:00:00Z">
      <attributes><attribute name="count" type="int">2</attribute></attributes>
      <objects><relationship object-id="o1" qualifier="ships"/><relationship object-id="o2" qualifier="ships"/></objects>
    </event>
  </events>
  <e2e><relationship source-event-id="e1" target-event-id="e1" qualifier="self"/></e2e>
</log>
)xml";
  const auto standard_fixture = minera::parse_ocel_xml(standard_links);
  assert(standard_fixture.o2o.size() == 1);
  assert(standard_fixture.o2o.front().qualifier == "contains");
  assert(standard_fixture.e2e.size() == 1 &&
         standard_fixture.e2e.front().qualifier == "self");
  // o2 is kept because e1 relates it (the reader drops unrelated objects).
  assert(standard_fixture.relations.size() == 2);
  assert(std::get<std::int64_t>(standard_fixture.events.front().attributes.front().value) == 2);

  const std::string typed_standard = R"xml(
<log ocelVersion="2.0">
  <object-types>
    <object-type name="Order"><attributes>
      <attribute name="amount" type="int"/>
      <attribute name="status" type="string"/>
    </attributes></object-type>
    <object-type name="EmptyObject"><attributes>
      <attribute name="label" type="string"/>
    </attributes></object-type>
  </object-types>
  <event-types>
    <event-type name="Create"><attributes>
      <attribute name="approved" type="boolean"/>
      <attribute name="planned_at" type="datetime"/>
    </attributes></event-type>
    <event-type name="EmptyEvent"><attributes>
      <attribute name="reason" type="string"/>
    </attributes></event-type>
  </event-types>
  <objects>
    <object id="o1" type="Order"><attributes>
      <attribute name="amount" time="1970-01-01T00:00:00Z" value="7"/>
      <attribute name="status" time="1970-01-01T00:00:00Z" value="new"/>
      <attribute name="status" time="2024-01-02T00:00:00Z" value="paid"/>
    </attributes></object>
  </objects>
  <events><event id="e1" type="Create" time="2024-01-01T00:00:00Z">
    <attributes><attribute name="approved" value="true"/></attributes>
    <objects><relationship object-id="o1" qualifier="created"/></objects>
  </event></events>
</log>
)xml";
  const auto typed = minera::parse_ocel_xml(typed_standard);
  assert(std::get<std::int64_t>(typed.objects.front().attributes.front().value) ==
         7);
  assert(std::get<bool>(typed.events.front().attributes.front().value));
  assert(typed.object_changes.size() == 1 &&
         std::get<std::string>(typed.object_changes.front().value) == "paid");
  assert(typed.object_type_attributes.at("EmptyObject").at("label") ==
         "string");
  assert(typed.event_type_attributes.at("EmptyEvent").at("reason") ==
         "string");
  assert(typed.event_type_attributes.at("Create").at("planned_at") ==
         "datetime");

  const auto repeated_epoch = minera::parse_ocel_xml(R"xml(
<log>
  <object-types><object-type name="Order"><attributes>
    <attribute name="state" type="string"/>
  </attributes></object-type></object-types>
  <event-types/>
  <objects><object id="o1" type="Order"><attributes>
    <attribute name="state" time="1970-01-01T00:00:00Z" value="new"/>
    <attribute name="state" time="1970-01-01T00:00:00Z" value="reopened"/>
    <attribute name="state" time="0" value="queued"/>
  </attributes></object></objects>
  <events><event id="e1" type="Create" time="2024-01-01T00:00:00Z">
    <objects><relationship object-id="o1" qualifier=""/></objects>
  </event></events>
</log>
)xml");
  // e1 keeps o1: the reader drops objects without relations.
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
    (void)minera::parse_ocel_xml(R"xml(
<log><object-types><object-type name="Order"/></object-types>
<objects><object id="o1" type="Order"><attributes>
  <attribute name="state" value="new"/>
  <attribute name="state" value="queued"/>
</attributes></object></objects><events/></log>
)xml");
  } catch (...) {
    missing_change_time = true;
  }
  assert(missing_change_time);

  // Typed values with surrounding whitespace read like PM4Py's float()/int()
  // and date parser; string values keep their whitespace.
  const auto spaced = minera::parse_ocel_xml(R"xml(
<log><object-types><object-type name="Order"><attributes>
  <attribute name="w" type="float"/><attribute name="n" type="integer"/>
  <attribute name="ok" type="boolean"/><attribute name="due" type="date"/>
  <attribute name="s" type="string"/>
</attributes></object-type></object-types>
<event-types><event-type name="A"/></event-types>
<objects><object id="o1" type="Order"><attributes>
  <attribute name="w" time="0">
    4.5
  </attribute>
  <attribute name="n" time="0"> 3
</attribute>
  <attribute name="ok" time="0">	True </attribute>
  <attribute name="due" time="0">
    2024-01-02T00:00:00Z
  </attribute>
  <attribute name="s" time="0"> x </attribute>
</attributes></object></objects>
<events><event id="e1" type="A" time="2024-01-01T00:00:00Z"><objects>
  <relationship object-id="o1" qualifier=""/></objects></event></events></log>
)xml");
  assert(spaced.objects.size() == 1);
  for (const auto& attribute : spaced.objects[0].attributes) {
    if (attribute.key == "w") assert(std::get<double>(attribute.value) == 4.5);
    if (attribute.key == "n") assert(std::get<std::int64_t>(attribute.value) == 3);
    if (attribute.key == "ok") assert(std::get<bool>(attribute.value));
    if (attribute.key == "due")
      assert(std::get<std::string>(attribute.value) == "2024-01-02T00:00:00Z");
    if (attribute.key == "s") assert(std::get<std::string>(attribute.value) == " x ");
  }
  // Reference capture (pm4py 2.7.22 read_ocel2_xml / read_ocel_xml): an event
  // without objects (e2) and an object without events (o2) are dropped; both
  // readers return one event and one object.
  for (const char* orphan_xml : {R"xml(<?xml version='1.0' encoding='UTF-8'?>
<log>
  <object-types>
    <object-type name="order">
      <attributes/>
    </object-type>
  </object-types>
  <event-types>
    <event-type name="A">
      <attributes/>
    </event-type>
    <event-type name="B">
      <attributes/>
    </event-type>
  </event-types>
  <objects>
    <object id="o1" type="order">
      <attributes/>
    </object>
    <object id="o2" type="order">
      <attributes/>
    </object>
  </objects>
  <events>
    <event id="e1" type="A" time="2024-01-01T00:00:00">
      <attributes/>
      <objects>
        <relationship object-id="o1" qualifier=""/>
      </objects>
    </event>
    <event id="e2" type="B" time="2024-01-01T01:00:00">
      <attributes/>
    </event>
  </events>
</log>
)xml", R"xml(<?xml version='1.0' encoding='UTF-8'?>
<log>
  <global scope="event">
    <string key="ocel:activity" value="__INVALID__"/>
  </global>
  <global scope="object">
    <string key="ocel:type" value="__INVALID__"/>
  </global>
  <global scope="log">
    <list key="attribute-names"/>
    <list key="object-types">
      <string key="object-type" value="order"/>
    </list>
    <string key="version" value="1.0"/>
    <string key="ordering" value="timestamp"/>
  </global>
  <events>
    <event>
      <string key="id" value="e1"/>
      <string key="activity" value="A"/>
      <date key="timestamp" value="2024-01-01T00:00:00Z"/>
      <list key="omap">
        <string key="object-id" value="o1"/>
      </list>
      <list key="vmap"/>
    </event>
    <event>
      <string key="id" value="e2"/>
      <string key="activity" value="B"/>
      <date key="timestamp" value="2024-01-02T00:00:00Z"/>
      <list key="omap"/>
      <list key="vmap"/>
    </event>
  </events>
  <objects>
    <object>
      <string key="id" value="o1"/>
      <string key="type" value="order"/>
      <list key="ovmap"/>
    </object>
    <object>
      <string key="id" value="o2"/>
      <string key="type" value="order"/>
      <list key="ovmap"/>
    </object>
  </objects>
</log>
)xml"}) {
    const auto orphans = minera::parse_ocel_xml(orphan_xml);
    assert(orphans.events.size() == 1 && orphans.events[0].id == "e1");
    assert(orphans.objects.size() == 1 && orphans.objects[0].id == "o1");
    assert(orphans.relations.size() == 1);
  }
  return 0;
}
