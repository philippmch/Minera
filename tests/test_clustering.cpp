#include "minera/clustering.hpp"
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
  EventLog dmm_log;
  dmm_log.traces.push_back(Trace{"left", {Event{"left", "A", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"left-2", {Event{"left-2", "A", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"left-3", {Event{"left-3", "B", "", "", 0, {}, ""}}, {{"segment", "left"}}});
  dmm_log.traces.push_back(Trace{"right", {Event{"right", "A", "", "", 0, {}, ""}}, {{"segment", "right"}}});
  auto dmm_options=o; dmm_options.trace_attribute="segment"; dmm_options.variant=TraceAttributeClusteringVariant::kDmmLeven;
  const auto dmm_result=discover_trace_attribute_clustering(dmm_log,dmm_options);
  assert(dmm_result.tree.distance > 0.19 && dmm_result.tree.distance < 0.21);
}
