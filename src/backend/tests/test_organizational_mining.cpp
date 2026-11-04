#include "minera/organizational_mining.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
  auto malformed_result = handover;
  ++malformed_result.diagnostics.edge_count;
  try { (void)minera::organizational_sna_json(malformed_result); assert(false); }
  catch (const std::invalid_argument&) {}

  // Expectations below were captured from pm4py 2.7.22
  // (algo.organizational_mining.sna.algorithm.apply, LOG on the converted
  // EventLog and PANDAS on the formatted DataFrame).
  const auto weight_of = [](const minera::OrganizationalSnaResult& r,
                            const std::string& a, const std::string& b) {
    for (const auto& edge : r.edges)
      if (r.resources[edge.source] == a && r.resources[edge.target] == b)
        return edge.weight;
    return -100.0;
  };
  const auto make = [](std::vector<std::vector<std::pair<std::string, std::string>>> cases) {
    minera::EventLog out;
    std::size_t t = 0;  // input_order (a double here is a narrowing error on Clang and MSVC)
    for (std::size_t c = 0; c < cases.size(); ++c) {
      const auto id = std::to_string(c + 1);
      minera::Trace trace{id, {}, {}};
      for (const auto& [activity, resource] : cases[c])
        trace.events.push_back({id, activity, "", resource, t++, {}, ""});
      out.traces.push_back(std::move(trace));
    }
    return out;
  };
  // Case 1 Ann,Bob,Ann,Bob,Ann; case 2 Ann.
  const auto alternating = make({{{"A","Ann"},{"B","Bob"},{"C","Ann"},{"D","Bob"},{"E","Ann"}}, {{"A","Ann"}}});
  for (const auto variant : {minera::OrganizationalSnaVariant::kSubcontracting,
                             minera::OrganizationalSnaVariant::kSubcontractingPandas}) {
    minera::OrganizationalSnaOptions sub; sub.variant = variant; sub.n = 2;
    const auto r = minera::discover_organizational_sna(alternating, sub);
    // Only the first n-window closed by each source counts: {Ann->Bob 0.5, Bob->Ann 0.5}.
    assert(r.edges.size() == 2);
    assert(std::abs(weight_of(r, "Ann", "Bob") - 0.5) < 1e-12);
    assert(std::abs(weight_of(r, "Bob", "Ann") - 0.5) < 1e-12);
  }
  {
    // Variant order decides which window is first: LOG uses first appearance
    // ({Ann->Bob 1/3}), PANDAS uses descending (count, variant) ({Ann->Carl 2/3}).
    const auto ordered = make({{{"A","Ann"},{"B","Bob"},{"C","Ann"}},
                               {{"A","Ann"},{"B","Carl"},{"C","Ann"}},
                               {{"A","Ann"},{"B","Carl"},{"C","Ann"}}});
    minera::OrganizationalSnaOptions sub; sub.variant = minera::OrganizationalSnaVariant::kSubcontracting;
    const auto log_result = minera::discover_organizational_sna(ordered, sub);
    assert(log_result.edges.size() == 1);
    assert(std::abs(weight_of(log_result, "Ann", "Bob") - 1.0 / 3.0) < 1e-12);
    sub.variant = minera::OrganizationalSnaVariant::kSubcontractingPandas;
    const auto pandas_result = minera::discover_organizational_sna(ordered, sub);
    assert(pandas_result.edges.size() == 1);
    assert(std::abs(weight_of(pandas_result, "Ann", "Carl") - 2.0 / 3.0) < 1e-12);
  }
  {
    // Working together: LOG divides by cases (0.5), PANDAS by event rows (1/6).
    minera::OrganizationalSnaOptions together_options;
    together_options.variant = minera::OrganizationalSnaVariant::kWorkingTogether;
    const auto log_result = minera::discover_organizational_sna(alternating, together_options);
    assert(std::abs(weight_of(log_result, "Ann", "Bob") - 0.5) < 1e-12);
    assert(std::abs(weight_of(log_result, "Bob", "Ann") - 0.5) < 1e-12);
    together_options.variant = minera::OrganizationalSnaVariant::kWorkingTogetherPandas;
    const auto pandas_result = minera::discover_organizational_sna(alternating, together_options);
    assert(std::abs(weight_of(pandas_result, "Ann", "Bob") - 1.0 / 6.0) < 1e-12);
    assert(std::abs(weight_of(pandas_result, "Bob", "Ann") - 1.0 / 6.0) < 1e-12);
  }
  for (const auto variant : {minera::OrganizationalSnaVariant::kJointActivities,
                             minera::OrganizationalSnaVariant::kJointActivitiesPandas}) {
    // Ann [2,1,0], Bob [1,1,1] (constant), Carl [1,1,2] over A,B,C: pearsonr
    // gives NaN for every Bob pair and -0.8660254037844386 for Ann/Carl.
    // With normalization the NaN makes the maximum NaN, so nothing is scaled.
    const auto joint_log = make({{{"A","Ann"},{"A","Ann"},{"B","Ann"},{"A","Bob"},{"B","Bob"},
                                  {"C","Bob"},{"B","Carl"},{"C","Carl"},{"C","Carl"},{"A","Carl"}}});
    for (const bool normalize : {false, true}) {
      minera::OrganizationalSnaOptions joint_options;
      joint_options.variant = variant;
      joint_options.metric_normalization = normalize;
      const auto r = minera::discover_organizational_sna(joint_log, joint_options);
      assert(r.edges.size() == 6);
      assert(std::isnan(weight_of(r, "Ann", "Bob")) && std::isnan(weight_of(r, "Bob", "Ann")));
      assert(std::isnan(weight_of(r, "Bob", "Carl")) && std::isnan(weight_of(r, "Carl", "Bob")));
      assert(std::abs(weight_of(r, "Ann", "Carl") + 0.8660254037844386) < 1e-12);
      assert(std::abs(weight_of(r, "Carl", "Ann") + 0.8660254037844386) < 1e-12);
      const auto json = minera::organizational_sna_json(r);
      assert(json.find("\"weight\":null") != std::string::npos);
    }
    // One activity: pearsonr raises "`x` and `y` must have length at least 2".
    minera::OrganizationalSnaOptions joint_options; joint_options.variant = variant;
    bool raised = false;
    try { (void)minera::discover_organizational_sna(make({{{"A","Ann"},{"A","Bob"}}}), joint_options); }
    catch (const std::invalid_argument&) { raised = true; }
    assert(raised);
  }
  return 0;
}
