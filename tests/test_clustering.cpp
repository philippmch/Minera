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
  for (auto v : {TraceAttributeClusteringVariant::kDmmLeven, TraceAttributeClusteringVariant::kAvgLeven,
                 TraceAttributeClusteringVariant::kDmmVec, TraceAttributeClusteringVariant::kAvgVec,
                 TraceAttributeClusteringVariant::kDfg}) {
    o.variant=v; auto r=discover_trace_attribute_clustering(l,o); assert(r.diagnostics.computed); assert(r.diagnostics.group_count==2); assert(!r.tree.children.empty()); assert(r.tree.name == "root"); assert(r.leaf_names.size() == 2);
    const auto j=trace_attribute_clustering_json(r); assert(j.find("TRACE_ATTRIBUTE_CLUSTERING")!=std::string::npos); assert(j.find("leaf_names")!=std::string::npos);
  }
  EventLog empty; auto e=discover_trace_attribute_clustering(empty,o); assert(e.diagnostics.computed && e.diagnostics.group_count==0);
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
