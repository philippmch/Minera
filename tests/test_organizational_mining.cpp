#include "minera/organizational_mining.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace {
minera::EventLog sample() {
  minera::EventLog log;
  log.traces.push_back({"c1", {{"c1", "A", "", "alice", 0, {}, ""},
                               {"c1", "B", "", "bob", 1, {}, ""},
                               {"c1", "C", "", "alice", 2, {}, ""}}});
  log.traces.push_back({"c2", {{"c2", "A", "", "alice", 0, {}, ""},
                               {"c2", "B", "", "carol", 1, {}, ""}}});
  log.traces.push_back({"c3", {{"c3", "D", "", "", 0, {}, ""}}});
  return log;
}
}

int main() {
  minera::OrganizationalSnaOptions options;
  options.beta = 0.5;
  const auto handover = minera::discover_organizational_sna(sample(), options);
  assert(handover.resources.size() == 3);
  assert(handover.diagnostics.computed);
  assert(handover.edges.size() == 4);
  assert(handover.resources[0] == "alice");
  assert(std::isfinite(handover.edges.front().weight));
  assert(handover.directed);
  for (const auto variant : {minera::OrganizationalSnaVariant::kHandoverPandas,
                             minera::OrganizationalSnaVariant::kWorkingTogetherPandas,
                             minera::OrganizationalSnaVariant::kSubcontractingPandas,
                             minera::OrganizationalSnaVariant::kJointActivitiesPandas}) {
    auto adapter_options = options;
    adapter_options.variant = variant;
    const auto adapter_result =
        minera::discover_organizational_sna(sample(), adapter_options);
    assert(adapter_result.variant == variant);
    assert(adapter_result.resources == handover.resources);
  }

  options.beta = 0.0;
  const auto adjacent = minera::discover_organizational_sna(sample(), options);
  assert(adjacent.edges.size() == 3);
  for (const auto& edge : adjacent.edges) assert(std::abs(edge.weight - 1.0 / 3.0) < 1e-12);

  options.variant = minera::OrganizationalSnaVariant::kWorkingTogether;
  const auto together = minera::discover_organizational_sna(sample(), options);
  assert(!together.edges.empty());
  assert(!together.directed);
  assert(together.edges.size() == 4);
  assert(together.edges[0].source < together.edges[0].target);
  assert(together.edges.front().weight <= 1.0);

  options.variant = minera::OrganizationalSnaVariant::kSubcontracting;
  options.n = 2;
  const auto subcontracting = minera::discover_organizational_sna(sample(), options);
  assert(subcontracting.edges.size() == 1);
  assert(subcontracting.edges.front().source == 0);

  options.variant = minera::OrganizationalSnaVariant::kJointActivities;
  const auto joint = minera::discover_organizational_sna(sample(), options);
  assert(!joint.directed);
  assert(joint.edges.size() == 6);
  for (const auto& edge : joint.edges) assert(std::isfinite(edge.weight));

  bool threw = false;
  options.beta = 2.0;
  try { (void)minera::discover_organizational_sna(sample(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::discover_organizational_sna({}, {}); }
  catch (...) { threw = true; }
  assert(!threw);

  options.variant = minera::OrganizationalSnaVariant::kWorkingTogether;
  options.beta = 0.0;
  options.max_edges = 1;
  const auto truncated = minera::discover_organizational_sna(sample(), options);
  assert(truncated.diagnostics.output_truncated && truncated.edges.size() == 1);

  minera::EventLog custom;
  custom.traces.push_back({"custom", {
      {"custom", "ignored", "", "ignored", 0,
       {{"team:activity", "A"}, {"team:resource", "r1"}}, ""},
      {"custom", "ignored", "", "ignored", 1,
       {{"team:activity", "B"}, {"team:resource", "r2"}}, ""},
  }});
  minera::OrganizationalSnaOptions custom_options;
  custom_options.activity_attribute = "team:activity";
  custom_options.resource_attribute = "team:resource";
  const auto custom_sna =
      minera::discover_organizational_sna(custom, custom_options);
  assert((custom_sna.resources == std::vector<std::string>{"r1", "r2"}));
  assert(custom_sna.edges.size() == 1);
  assert(custom_sna.edges.front().source == 0 &&
         custom_sna.edges.front().target == 1);
  return 0;
}
