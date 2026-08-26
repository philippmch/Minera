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

  // Pandas-style variants regroup a stable event stream by the selected
  // trace-level case attribute, while log variants retain Trace boundaries.
  minera::EventLog regrouped;
  regrouped.traces.push_back({
      "original-1",
      { {"original-1", "A", "", "r1", 0, {}, ""},
        {"original-1", "B", "", "r2", 1, {}, ""} },
      {{"case:batch", "shared"}}});
  regrouped.traces.push_back({
      "original-2",
      { {"original-2", "C", "", "r3", 2, {}, ""},
        {"original-2", "D", "", "r1", 3, {}, ""} },
      {{"case:batch", "shared"}}});
  custom_options.variant = minera::OrganizationalSnaVariant::kHandoverPandas;
  custom_options.beta = 0.0;
  custom_options.resource_attribute = "org:resource";
  custom_options.case_id_attribute = "case:batch";
  const auto regrouped_pandas =
      minera::discover_organizational_sna(regrouped, custom_options);
  assert(regrouped_pandas.edges.size() == 3);
  assert(regrouped_pandas.edges[0].source == 0 &&
         regrouped_pandas.edges[0].target == 1);
  assert(std::abs(regrouped_pandas.edges[0].weight - 1.0 / 3.0) < 1e-12);
  custom_options.variant = minera::OrganizationalSnaVariant::kHandover;
  custom_options.case_id_attribute = "case:concept:name";
  const auto bounded_log =
      minera::discover_organizational_sna(regrouped, custom_options);
  assert(bounded_log.edges.size() == 2);

  // Event-level selectors can split one source trace into independent cases.
  minera::EventLog event_cases;
  event_cases.traces.push_back({
      "original",
      { {"original", "A", "", "r1", 0,
          {{"case:segment", "left"}}, ""},
        {"original", "B", "", "r2", 1,
          {{"case:segment", "left"}}, ""},
        {"original", "C", "", "r3", 2,
          {{"case:segment", "right"}}, ""},
        {"original", "D", "", "r1", 3,
          {{"case:segment", "right"}}, ""} }});
  custom_options.variant =
      minera::OrganizationalSnaVariant::kWorkingTogetherPandas;
  custom_options.case_id_attribute = "case:segment";
  const auto event_grouped =
      minera::discover_organizational_sna(event_cases, custom_options);
  assert(event_grouped.edges.size() == 4);

  custom_options.variant = minera::OrganizationalSnaVariant::kHandoverPandas;
  custom_options.case_id_attribute = "case:missing";
  bool missing_case_selector = false;
  try {
    (void)minera::discover_organizational_sna(regrouped, custom_options);
  } catch (const std::invalid_argument&) {
    missing_case_selector = true;
  }
  assert(missing_case_selector);

  custom_options.variant = minera::OrganizationalSnaVariant::kJointActivitiesPandas;
  custom_options.case_id_attribute = "case:batch";
  bool unsupported_case_selector = false;
  try {
    (void)minera::discover_organizational_sna(regrouped, custom_options);
  } catch (const std::invalid_argument&) {
    unsupported_case_selector = true;
  }
  assert(unsupported_case_selector);
  return 0;
}
