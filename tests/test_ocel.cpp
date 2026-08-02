#include "minera/ocel.hpp"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
  const std::string input =
      R"({"ocel:events":{"e2":{"ocel:activity":"Ship","ocel:timestamp":"2024-01-02T00:00:00Z","ocel:omap":["o1","o2"],"ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":"source"},{"ocel:oid":"o2","ocel:qualifier":"target"}],"ocel:vmap":{"count":2}},"e1":{"ocel:activity":"Create","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"]}},"ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{"amount":12.5}},"o2":{"ocel:type":"item","ocel:ovmap":{}}}})";
  auto log = minera::parse_ocel_json(input);
  assert(log.events.size() == 2 && log.objects.size() == 2 &&
         log.relations.size() == 3);
  assert(log.relations[1].qualifier == "source");
  assert(std::get<double>(log.objects[0].attributes[0].value) == 12.5);

  log.o2o.push_back({"o1", "o2", "contains"});
  log.object_changes.push_back(
      {"o1", "order", "2024-01-03T00:00:00Z", "status",
       std::string("shipped")});
  log.globals["ocel:global-log.version"] = std::int64_t{2};
  log.globals["ocel:global-event.owner"] = std::string("a\"b\\c");

  const auto etot = minera::discover_etot_classic(log);
  assert(etot.edges.size() == 3);
  assert(etot.edges[0].activity == "Create" &&
         etot.edges[0].object_type == "order" &&
         etot.edges[0].frequency == 1);

  const auto otg = minera::discover_otg_classic(log);
  assert(otg.object_types.size() == 2 && otg.edges.size() == 4);
  bool saw_interaction = false;
  bool saw_descendant = false;
  bool saw_inheritance = false;
  for (const auto& edge : otg.edges) {
    if (edge.relation == "object_interaction") saw_interaction = true;
    if (edge.relation == "object_descendants") saw_descendant = true;
    if (edge.relation == "object_inheritance") saw_inheritance = true;
  }
  assert(saw_interaction && saw_descendant && saw_inheritance);

  const auto round =
      minera::parse_ocel_json(minera::serialize_ocel_json(log));
  assert(round.events.size() == log.events.size() &&
         round.relations.size() == log.relations.size() &&
         round.o2o.size() == log.o2o.size() &&
         round.object_changes.size() == log.object_changes.size() &&
         round.globals == log.globals);
  assert(round.o2o.back().qualifier == "contains");
  assert(std::get<std::string>(round.object_changes.front().value) ==
         "shipped");

  const auto standard =
      minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  const auto standard_round = minera::parse_ocel_json(standard);
  assert(standard_round.objects.size() == 2 &&
         standard_round.events.size() == 2 &&
         standard_round.relations.size() == log.relations.size() &&
         standard_round.o2o.size() == log.o2o.size() &&
         standard_round.object_changes.size() == log.object_changes.size() &&
         standard_round.globals == log.globals);

  minera::Ocel escaped;
  escaped.objects.push_back(
      {"o\"1", "type\\x", {{"k\"\\", std::string("v\n")}}});
  escaped.events.push_back(
      {"e\"1", "act\\x", "t", {{"a", std::string("b\\c")}}});
  escaped.relations.push_back({"e\"1", "o\"1", "q"});
  const auto escaped_round =
      minera::parse_ocel_json(minera::serialize_ocel_json(escaped));
  assert(escaped_round.objects[0].id == "o\"1" &&
         escaped_round.objects[0].type == "type\\x" &&
         escaped_round.events[0].activity == "act\\x" &&
         escaped_round.relations[0].qualifier == "q");

  bool malformed = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{},"ocel:events":{"e":{}}})");
  } catch (...) {
    malformed = true;
  }
  assert(malformed);

  bool duplicate = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{"o":{"ocel:type":"x"}},"ocel:events":{"e":{"ocel:activity":"a","ocel:timestamp":"t","ocel:omap":["o"]},"e":{"ocel:activity":"a","ocel:timestamp":"t","ocel:omap":["o"]}}})");
  } catch (...) {
    duplicate = true;
  }
  assert(duplicate);

  bool bad_o2o = false;
  try {
    (void)minera::parse_ocel_json(
        R"({"ocel:objects":{"o":{"ocel:type":"x","ocel:o2o":[{"ocel:oid":"missing"}]}},"ocel:events":{}})");
  } catch (...) {
    bad_o2o = true;
  }
  assert(bad_o2o);
  assert(minera::etot_json(etot).find("\"frequency\":1") !=
         std::string::npos);
  assert(minera::otg_json(otg).find("\"object_interaction\"") !=
         std::string::npos);
  return 0;
}
