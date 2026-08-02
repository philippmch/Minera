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
  return 0;
}
