#include "minera/saw_nets.hpp"

#include <cassert>
#include <string>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.objects = {{"o1", "order", {}}, {"i1", "item", {}}};
  log.events = {{"e1", "Create", "2026-01-01T00:00:00Z", {}},
                {"e2", "Ship", "2026-01-01T00:00:04Z", {}},
                {"e3", "Create", "2026-01-01T00:00:00Z", {}},
                {"e4", "Ship", "2026-01-01T00:00:04Z", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""},
                   {"e3", "i1", ""}, {"e4", "i1", ""}};
  return log;
}

}  // namespace

int main() {
  const auto result = minera::discover_saw_nets(sample());
  assert(result.ot_saw_nets.size() == 2);
  assert(result.ot_saw_nets.at("order").net.activities().size() == 2);
  assert(!result.ot_saw_nets.at("order").transition_weights.empty());
  assert(!result.multi_saw_net.net.activities().empty());
  const auto json = minera::saw_nets_json(result);
  assert(json.find("\"family\":\"SAW_NETS\"") != std::string::npos);
  assert(json.find("weight_distribution") != std::string::npos);
}
