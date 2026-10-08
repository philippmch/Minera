#include "minera/clustering.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>

using namespace minera;

EventLog sample() {
  EventLog l;
  Trace a{"a", {Event{"a","A","","",0,{},""}, Event{"a","B","","",1,{},""}}, {{"segment","z"}}};
  Trace b{"b", {Event{"b","A","","",0,{},""}, Event{"b","C","","",1,{},""}}, {{"segment","y"}}};
  Trace c{"c", {Event{"c","A","","",0,{},""}, Event{"c","B","","",1,{},""}}, {{"segment","z"}}};
  l.traces = {a,b,c}; return l;
}

int main() {
  auto l = sample(); TraceAttributeClusteringOptions o; o.trace_attribute="segment";
  const auto memberships = trace_attribute_clustering_groups(l, "segment");
  assert(memberships.size() == 2 && memberships[0].name == "z" && memberships[1].name == "y");
  assert(memberships[0].trace_indices == std::vector<std::size_t>({0, 2}));
  assert(memberships[1].trace_indices == std::vector<std::size_t>({1}));
  for (auto v : {TraceAttributeClusteringVariant::kDmmLeven, TraceAttributeClusteringVariant::kAvgLeven,
                 TraceAttributeClusteringVariant::kDmmVec, TraceAttributeClusteringVariant::kAvgVec,
                 TraceAttributeClusteringVariant::kDfg}) {
    o.variant=v; auto r=discover_trace_attribute_clustering(l,o); assert(r.diagnostics.computed); assert(r.diagnostics.group_count==2); assert(!r.tree.children.empty()); assert(r.tree.name == "root"); assert(r.leaf_names.size() == 2);
    const auto j=trace_attribute_clustering_json(r); assert(j.find("TRACE_ATTRIBUTE_CLUSTERING")!=std::string::npos); assert(j.find("leaf_names")!=std::string::npos);
  }
  EventLog empty; auto e=discover_trace_attribute_clustering(empty,o); assert(e.diagnostics.computed && e.diagnostics.group_count==0);
  assert(trace_attribute_clustering_json(e).find("empty event log") != std::string::npos);
  auto no_groups = o; no_groups.trace_attribute = "missing";
  const auto unmatched = discover_trace_attribute_clustering(l, no_groups);
  assert(unmatched.diagnostics.trace_count == 3 && unmatched.tree.size == 0);
  assert(trace_attribute_clustering_json(unmatched).find("no matching trace attribute groups") != std::string::npos);
  auto blank_label = l;
  blank_label.traces[0].attributes[0].value.clear();
  const auto blank_result = discover_trace_attribute_clustering(blank_label, o);
  assert(trace_attribute_clustering_json(blank_result).find("\"name\":\"\"") != std::string::npos);
  auto repeated_ids = l;
  repeated_ids.traces[2].case_id = repeated_ids.traces[0].case_id;
  const auto repeated_groups = trace_attribute_clustering_groups(repeated_ids, "case:concept:name");
  assert(repeated_groups.size() == 2 && repeated_groups[0].trace_indices == std::vector<std::size_t>({0, 2}));
  bool threw=false; try { auto bad=o; bad.alpha=2; (void)discover_trace_attribute_clustering(l,bad); } catch(const std::invalid_argument&) {threw=true;} assert(threw);
  threw=false; try { auto bad=o; bad.percent=0; (void)discover_trace_attribute_clustering(l,bad); } catch(const std::invalid_argument&) {threw=true;} assert(threw);
  threw=false; try { auto bad=o; bad.max_output_nodes=2; (void)discover_trace_attribute_clustering(l,bad); } catch(const std::invalid_argument&) {threw=true;} assert(threw);
  Trace case_prefixed{"d", {Event{"d","A","","",0,{},""}}, {{"case:segment","x"}}};
  EventLog prefixed; prefixed.traces.push_back(case_prefixed);
  auto prefixed_options=o; prefixed_options.trace_attribute="segment";
  auto prefixed_result=discover_trace_attribute_clustering(prefixed,prefixed_options);
  assert(prefixed_result.diagnostics.group_count==1 && prefixed_result.leaf_names[0]=="x");
  auto case_options=o; case_options.trace_attribute="case:concept:name";
  auto case_result=discover_trace_attribute_clustering(l,case_options);
  assert(case_result.diagnostics.group_count==3);
  // Trace-attribute values and equal-frequency variants retain first-seen
  // order. This is observable in leaf numbering and nearest-match ties.
  EventLog insertion_order;
  insertion_order.traces.push_back(Trace{"z-case", {Event{"z-case", "Z", "", "", 0, {}, ""}}, {{"segment", "z"}}});
  insertion_order.traces.push_back(Trace{"a-case", {Event{"a-case", "A", "", "", 0, {}, ""}}, {{"segment", "a"}}});
  auto insertion_options = case_options;
  insertion_options.trace_attribute = "segment";
  const auto insertion_result =
      discover_trace_attribute_clustering(insertion_order, insertion_options);
  assert((insertion_result.leaf_names == std::vector<std::string>{"z", "a"}));
  assert(trace_attribute_clustering_json(insertion_result) ==
         trace_attribute_clustering_json(
             discover_trace_attribute_clustering(insertion_order,
                                                 insertion_options)));
  EventLog dmm_log;
  dmm_log.traces.push_back(Trace{"left", {Event{"left", "A", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"left-2", {Event{"left-2", "A", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"left-3", {Event{"left-3", "B", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"right", {Event{"right", "A", "", "", 0, {}, ""}}, {{"segment", "right"}}});
  auto dmm_options=o; dmm_options.trace_attribute="segment"; dmm_options.variant=TraceAttributeClusteringVariant::kDmmLeven;
  const auto dmm_result=discover_trace_attribute_clustering(dmm_log,dmm_options);
  assert(dmm_result.tree.distance > 0.19 && dmm_result.tree.distance < 0.21);

  // Vector DMM matches activity and succession components independently;
  // matching their already-combined pair scores produces a different value.
  EventLog component_log;
  component_log.traces.push_back(Trace{
      "component-a-1",
      {Event{"component-a-1", "A", "", "", 0, {}, ""},
       Event{"component-a-1", "A", "", "", 1, {}, ""}},
      {{"segment", "component-a"}}});
  component_log.traces.push_back(Trace{
      "component-a-2",
      {Event{"component-a-2", "A", "", "", 0, {}, ""},
       Event{"component-a-2", "B", "", "", 1, {}, ""}},
      {{"segment", "component-a"}}});
  component_log.traces.push_back(Trace{
      "component-b-1",
      {Event{"component-b-1", "B", "", "", 0, {}, ""},
       Event{"component-b-1", "A", "", "", 1, {}, ""}},
      {{"segment", "component-b"}}});
  component_log.traces.push_back(Trace{
      "component-b-2",
      {Event{"component-b-2", "A", "", "", 0, {}, ""},
       Event{"component-b-2", "B", "", "", 1, {}, ""},
       Event{"component-b-2", "C", "", "", 2, {}, ""},
       Event{"component-b-2", "C", "", "", 3, {}, ""}},
      {{"segment", "component-b"}}});
  auto component_options = dmm_options;
  component_options.variant = TraceAttributeClusteringVariant::kDmmVec;
  component_options.alpha = 0.5;
  const auto component_result =
      discover_trace_attribute_clustering(component_log, component_options);
  assert(std::abs(component_result.tree.distance - 0.4451053) < 1e-6);

  // Empty succession vectors are maximally distant, including for two
  // one-event variants.
  EventLog singleton_log;
  singleton_log.traces.push_back(Trace{
      "singleton-a", {Event{"singleton-a", "A", "", "", 0, {}, ""}},
      {{"segment", "singleton-a"}}});
  singleton_log.traces.push_back(Trace{
      "singleton-b", {Event{"singleton-b", "B", "", "", 0, {}, ""}},
      {{"segment", "singleton-b"}}});
  const auto singleton_result =
      discover_trace_attribute_clustering(singleton_log, component_options);
  assert(std::abs(singleton_result.tree.distance - 1.0) < 1e-12);

  // Traces without the selected case attribute are not an artificial empty
  // group.
  EventLog missing_attribute_log;
  missing_attribute_log.traces.push_back(Trace{
      "present-x", {Event{"present-x", "A", "", "", 0, {}, ""}},
      {{"segment", "x"}}});
  missing_attribute_log.traces.push_back(Trace{
      "present-y", {Event{"present-y", "B", "", "", 0, {}, ""}},
      {{"segment", "y"}}});
  missing_attribute_log.traces.push_back(Trace{
      "missing", {Event{"missing", "C", "", "", 0, {}, ""}}, {}});
  const auto missing_result = discover_trace_attribute_clustering(
      missing_attribute_log, component_options);
  assert(missing_result.diagnostics.group_count == 2);
  assert((missing_result.leaf_names ==
          std::vector<std::string>{"x", "y"}));

  // DFG distances use complete group sublogs.  The percentage parameter is
  // accepted consistently across variants but must not alter this distance.
  EventLog dfg_percent_log;
  dfg_percent_log.traces.push_back(Trace{
      "dfg-x-ab", {Event{"dfg-x-ab", "A", "", "", 0, {}, ""},
                    Event{"dfg-x-ab", "B", "", "", 1, {}, ""}},
      {{"segment", "dfg-x"}}});
  dfg_percent_log.traces.push_back(Trace{
      "dfg-x-cd", {Event{"dfg-x-cd", "C", "", "", 0, {}, ""},
                    Event{"dfg-x-cd", "D", "", "", 1, {}, ""}},
      {{"segment", "dfg-x"}}});
  dfg_percent_log.traces.push_back(Trace{
      "dfg-y-ab", {Event{"dfg-y-ab", "A", "", "", 0, {}, ""},
                    Event{"dfg-y-ab", "B", "", "", 1, {}, ""}},
      {{"segment", "dfg-y"}}});
  dfg_percent_log.traces.push_back(Trace{
      "dfg-y-ef", {Event{"dfg-y-ef", "E", "", "", 0, {}, ""},
                    Event{"dfg-y-ef", "F", "", "", 1, {}, ""}},
      {{"segment", "dfg-y"}}});
  auto dfg_percent_options = o;
  dfg_percent_options.trace_attribute = "segment";
  dfg_percent_options.variant = TraceAttributeClusteringVariant::kDfg;
  dfg_percent_options.alpha = 0.5;
  dfg_percent_options.percent = 1.0;
  const auto dfg_full = discover_trace_attribute_clustering(
      dfg_percent_log, dfg_percent_options);
  dfg_percent_options.percent = 0.5;
  const auto dfg_partial = discover_trace_attribute_clustering(
      dfg_percent_log, dfg_percent_options);
  assert(dfg_full.tree.distance > 0.0);
  assert(std::abs(dfg_full.tree.distance - dfg_partial.tree.distance) < 1e-12);

  // Both distance profiles accept a selected event attribute.  A custom
  // selector must change the sequence values rather than silently falling
  // back to Event::activity.
  EventLog selected_activity_log;
  selected_activity_log.traces.push_back(Trace{
      "selected-left",
      {Event{"selected-left", "same", "", "", 0,
             {{"phase", "P1", AttributeKind::kString}}, ""}},
      {{"segment", "left"}}});
  selected_activity_log.traces.push_back(Trace{
      "selected-right",
      {Event{"selected-right", "same", "", "", 0,
             {{"phase", "P2", AttributeKind::kString}}, ""}},
      {{"segment", "right"}}});
  auto selected_options = o;
  selected_options.trace_attribute = "segment";
  selected_options.activity_attribute = "phase";
  selected_options.variant = TraceAttributeClusteringVariant::kAvgLeven;
  const auto selected_result = discover_trace_attribute_clustering(
      selected_activity_log, selected_options);
  assert(std::abs(selected_result.tree.distance - 1.0) < 1e-12);
  auto missing_selected = selected_options;
  missing_selected.activity_attribute = "missing_phase";
  bool missing_selected_threw = false;
  try {
    (void)discover_trace_attribute_clustering(selected_activity_log,
                                              missing_selected);
  } catch (const std::invalid_argument&) {
    missing_selected_threw = true;
  }
  assert(missing_selected_threw);

  // Linkage child order follows the merge order, not lexical leaf names.
  assert(insertion_result.tree.children.size() == 2);
  assert(insertion_result.tree.children[0].name == "z");
  assert(insertion_result.tree.children[1].name == "a");

  EventLog nested_order;
  nested_order.traces.push_back(Trace{
      "nested-z", {Event{"nested-z", "A", "", "", 0, {}, ""}},
      {{"segment", "z"}}});
  nested_order.traces.push_back(Trace{
      "nested-a", {Event{"nested-a", "A", "", "", 0, {}, ""}},
      {{"segment", "a"}}});
  nested_order.traces.push_back(Trace{
      "nested-m", {Event{"nested-m", "B", "", "", 0, {}, ""}},
      {{"segment", "m"}}});
  auto nested_options = insertion_options;
  nested_options.variant = TraceAttributeClusteringVariant::kDmmLeven;
  const auto nested_result =
      discover_trace_attribute_clustering(nested_order, nested_options);
  assert(nested_result.tree.name == "root");
  assert(nested_result.tree.children.size() == 2);
  assert(nested_result.tree.children[0].name == "m");
  assert(nested_result.tree.children[1].name == "a-z");
  assert(nested_result.tree.children[1].children.size() == 2);
  assert(nested_result.tree.children[1].children[0].name == "z");
  assert(nested_result.tree.children[1].children[1].name == "a");

  ProfileClusteringOptions profile_options;
  profile_options.cluster_count = 2;
  profile_options.variant = ProfileClusteringVariant::SKLEARN_PROFILES;
  const auto profile_result = discover_profile_clustering(l, profile_options);
  const auto profile_repeat = discover_profile_clustering(l, profile_options);
  assert(profile_result.diagnostics.computed);
  assert(profile_result.variant == ProfileClusteringVariant::kSklearnProfiles);
  assert(profile_result.labels.size() == l.traces.size());
  assert(profile_result.clusters.size() == 2);
  assert(!profile_result.feature_names.empty());
  assert(std::find(profile_result.feature_names.begin(), profile_result.feature_names.end(),
                   "event:concept:name@A") != profile_result.feature_names.end());
  assert(std::find(profile_result.feature_names.begin(), profile_result.feature_names.end(),
                   "succession:concept:name@A#B") != profile_result.feature_names.end());
  assert(profile_result.labels == profile_repeat.labels);
  for (const auto& cluster : profile_result.clusters)
    assert(std::is_sorted(cluster.begin(), cluster.end()));
  assert(profile_clustering_json(profile_result) == profile_clustering_json(profile_repeat));
  assert(profile_clustering_json(profile_result).find(
             "\"variant\":\"sklearn_profiles\"") != std::string::npos);
  auto malformed_profile = profile_result;
  malformed_profile.clusters[0].push_back(l.traces.size());
  bool malformed_profile_rejected = false;
  try { (void)profile_clustering_json(malformed_profile); }
  catch (const std::invalid_argument&) { malformed_profile_rejected = true; }
  assert(malformed_profile_rejected);
  auto malformed_tree = nested_result;
  malformed_tree.diagnostics.node_count += 1;
  bool malformed_tree_rejected = false;
  try { (void)trace_attribute_clustering_json(malformed_tree); }
  catch (const std::invalid_argument&) { malformed_tree_rejected = true; }
  assert(malformed_tree_rejected);

  EventLog selected_profile_log;
  selected_profile_log.traces.push_back(Trace{
      "profile-left",
      {Event{"profile-left", "same", "", "", 0,
             {{"phase", "P1", AttributeKind::kString}}, ""},
       Event{"profile-left", "same", "", "", 1,
             {{"phase", "P2", AttributeKind::kString}}, ""}},
      {}});
  selected_profile_log.traces.push_back(Trace{
      "profile-right",
      {Event{"profile-right", "same", "", "", 0,
             {{"phase", "P2", AttributeKind::kString}}, ""},
       Event{"profile-right", "same", "", "", 1,
             {{"phase", "P1", AttributeKind::kString}}, ""}},
      {}});
  auto selected_profile_options = profile_options;
  selected_profile_options.activity_attribute = "phase";
  const auto selected_profile = discover_profile_clustering(
      selected_profile_log, selected_profile_options);
  assert(std::find(selected_profile.feature_names.begin(),
                   selected_profile.feature_names.end(),
                   "event:phase@P1") != selected_profile.feature_names.end());
  assert(std::find(selected_profile.feature_names.begin(),
                   selected_profile.feature_names.end(),
                   "succession:phase@P1#P2") !=
         selected_profile.feature_names.end());
  assert(std::count(selected_profile.feature_names.begin(),
                    selected_profile.feature_names.end(),
                    "event:phase@P1") == 1);
  auto missing_profile = selected_profile_options;
  missing_profile.activity_attribute = "missing_phase";
  bool missing_profile_threw = false;
  try {
    (void)discover_profile_clustering(selected_profile_log, missing_profile);
  } catch (const std::invalid_argument&) {
    missing_profile_threw = true;
  }
  assert(missing_profile_threw);

  ProfileClusteringOptions numeric_options;
  numeric_options.cluster_count = 2;
  numeric_options.event_numeric_attributes = {"cost"};
  for (auto& trace : l.traces) {
    for (auto& event : trace.events) {
      event.attributes.push_back({"cost", "1.5", AttributeKind::kFloat});
    }
  }
  const auto numeric_profile = discover_profile_clustering(l, numeric_options);
  assert(std::find(numeric_profile.feature_names.begin(), numeric_profile.feature_names.end(),
                   "event:cost") != numeric_profile.feature_names.end());

  EventLog seeded_profile_log;
  const std::vector<double> seeded_scores = {0.0, 0.5, 1.0, 9.0, 9.5, 10.0};
  for (std::size_t i = 0; i < seeded_scores.size(); ++i) {
    seeded_profile_log.traces.push_back(Trace{
        "seeded-" + std::to_string(i),
        {Event{"seeded-" + std::to_string(i), "row", "", "", 0,
               {{"score", std::to_string(seeded_scores[i]), AttributeKind::kFloat}},
               ""}},
        {}});
  }
  ProfileClusteringOptions seeded_options;
  seeded_options.cluster_count = 2;
  seeded_options.include_activity_representation = false;
  seeded_options.include_succession_representation = false;
  seeded_options.event_numeric_attributes = {"score"};
  seeded_options.seed = 17;
  const auto seeded_result = discover_profile_clustering(seeded_profile_log,
                                                          seeded_options);
  const auto seeded_repeat = discover_profile_clustering(seeded_profile_log,
                                                          seeded_options);
  assert(seeded_result.labels == seeded_repeat.labels);
  assert(seeded_result.centroids == seeded_repeat.centroids);
  bool seed_changes_initialization = false;
  for (std::uint64_t seed = 18; seed < 50 && !seed_changes_initialization; ++seed) {
    seeded_options.seed = seed;
    const auto other = discover_profile_clustering(seeded_profile_log,
                                                    seeded_options);
    seed_changes_initialization = other.labels != seeded_result.labels ||
                                  other.centroids != seeded_result.centroids;
  }
  assert(seed_changes_initialization);

  bool profile_threw = false;
  try {
    auto bad = profile_options;
    bad.cluster_count = l.traces.size() + 1;
    (void)discover_profile_clustering(l, bad);
  } catch (const std::invalid_argument&) {
    profile_threw = true;
  }
  assert(profile_threw);
  const auto empty_profile = discover_profile_clustering(EventLog{});
  assert(empty_profile.diagnostics.computed && empty_profile.labels.empty());
  assert(empty_profile.variant == ProfileClusteringVariant::kSklearnProfiles);

  bool invalid_variant = false;
  try {
    auto bad = profile_options;
    bad.variant = static_cast<ProfileClusteringVariant>(99);
    (void)discover_profile_clustering(l, bad);
  } catch (const std::invalid_argument&) {
    invalid_variant = true;
  }
  assert(invalid_variant);
}
