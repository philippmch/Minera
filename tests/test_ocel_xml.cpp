#include "minera/ocel_xml.hpp"

#include <cassert>
#include <sstream>

int main() {
  minera::Ocel log;
  log.objects.push_back({"o2", "Item", {{"price", std::int64_t(7)}}});
  log.objects.push_back({"o1", "Order", {}});
  log.events.push_back({"e1", "create", "2024-01-01T00:00:00Z", {{"ok", true}}});
  log.relations.push_back({"e1", "o1", "owner"});
  log.o2o.push_back({"o1", "o2", "contains"});
  log.object_changes.push_back({"o1", "Order", "2024-01-02T00:00:00Z", "status", std::string("paid")});
  log.globals["ocel:global-event.version"] = std::string("1");

  const auto classic = minera::serialize_ocel_xml(log);
  const auto parsed = minera::parse_ocel_xml(classic);
  assert(parsed.events.size() == 1 && parsed.objects.size() == 2);
  assert(parsed.relations.front().qualifier == "owner");
  assert(parsed.o2o.front().qualifier == "contains");
  assert(parsed.object_changes.size() == 1);
  assert(classic == minera::serialize_ocel_xml(log));

  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  const auto parsed2 = minera::parse_ocel_xml(xml2);
  assert(parsed2.events.size() == 1 && parsed2.relations.size() == 1);

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
</log>
)xml";
  const auto standard_fixture = minera::parse_ocel_xml(standard_links);
  assert(standard_fixture.o2o.size() == 1);
  assert(standard_fixture.o2o.front().qualifier == "contains");
  assert(standard_fixture.relations.size() == 1);
  assert(std::get<std::int64_t>(standard_fixture.events.front().attributes.front().value) == 2);
  return 0;
}
