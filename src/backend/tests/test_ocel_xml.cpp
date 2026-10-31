#include "minera/ocel_xml.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>
#include <variant>

int main() {
  minera::Ocel log;
  log.objects.push_back({"o2", "Item", {{"price", std::int64_t(7)}}});
  log.objects.push_back({"o1", "Order", {}});
  log.events.push_back({"e1", "create", "2024-01-01T00:00:00Z", {{"ok", true}}});
  log.events.push_back({"e2", "ship", "2024-01-02T00:00:00Z", {}});
  log.relations.push_back({"e1", "o1", "owner"});
  log.o2o.push_back({"o1", "o2", "contains"});
  log.e2e.push_back({"e1", "e2", "precedes"});
  log.object_changes.push_back({"o1", "Order", "2024-01-02T00:00:00Z", "status", std::string("paid")});
  log.globals["ocel:global-event.version"] = std::string("1");

  const auto classic = minera::serialize_ocel_xml(log);
  const auto parsed = minera::parse_ocel_xml(classic);
  assert(parsed.events.size() == 2 && parsed.objects.size() == 2);
  assert(parsed.relations.front().qualifier == "owner");
  assert(parsed.o2o.front().qualifier == "contains");
  assert(parsed.e2e.size() == 1 && parsed.e2e.front().qualifier == "precedes");
  assert(parsed.object_changes.size() == 1);
  assert(classic == minera::serialize_ocel_xml(log));

  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  assert(xml2.find("<event-type name=\"create\">") != std::string::npos);
  assert(xml2.find("time=\"1970-01-01T00:00:00Z\"") !=
         std::string::npos);
  const auto parsed2 = minera::parse_ocel_xml(xml2);
  // o1 has no base "status": like PM4Py 2.7.22 write_ocel2_xml no null base
  // entry is written, so the single change reads back as o1's base value.
  assert(parsed2.events.size() == 2 && parsed2.relations.size() == 1 &&
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
  assert(std::get<std::string>(keyed.globals.at("ocel:global-log.version")) == "1.0");

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
      <objects><relationship object-id="o1" qualifier="ships"/></objects>
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
  assert(standard_fixture.relations.size() == 1);
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
  <events/>
</log>
)xml");
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
  return 0;
}
