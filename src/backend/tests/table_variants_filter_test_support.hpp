#pragma once
#include "table_paths_filter_test_support.hpp"
#include "minera/table_variants_filter_workbench.hpp"
#include "minera/variants_filter_controls.hpp"
namespace table_variants_test {
using namespace table_paths_test;
inline std::vector<DiscoveryWorkbenchPart> variants_request(const std::string& engine, const EventTable& source,
    std::string operation="membership", std::vector<DiscoveryWorkbenchPart> controls={}, bool tuples=true) {
  std::vector<DiscoveryWorkbenchPart> parts={field("engine",engine),field("operation",operation),
    {"source_file","table.json",serialize_embedding_workbench_table_json(source),true}};
  if(operation=="membership") {parts.push_back(field("admitted_json","[[\"A\",\"B\"]]"));if(tuples)parts.push_back(field("sequence_kind","tuple"));}
  else if(operation!="auto" && operation!="from_list") parts.push_back(field("criterion",operation=="top_k"?"1":"0.5"));
  if(operation=="from_list")parts.push_back(field("variants_json","[\"A,B\"]"));
  parts.insert(parts.end(),controls.begin(),controls.end());return parts;
}
inline void variants_workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run) {
  for(const std::string engine:{"pandas","polars"}) {
    const auto source=between_source(engine);
    success(run(variants_request(engine,source)),source,subset_rows(source,{3,4}),engine);
    success(run(variants_request(engine,source,"membership",{field("positive","false")})),source,subset_rows(source,{0,1,2}),engine);
    success(run(variants_request(engine,source,"membership",{},false)),source,subset_rows(source,engine=="pandas"?std::vector<std::size_t>{}:std::vector<std::size_t>{3,4}),engine);
    success(run(variants_request(engine,source,"top_k")),source,subset_rows(source,{0,1,2}),engine);
    success(run(variants_request(engine,source,"top_k",{field("positive","false")})),source,subset_rows(source,{3,4}),engine);
    auto custom=source;const std::string activity(" activity\n\0Ω ",15);custom.columns[0]="";custom.columns[1]=activity;
    for(auto& row:custom.rows){row[0]->key="";row[1]->key=activity;}
    success(run(variants_request(engine,custom,"membership",{field("case_id_key",""),field("activity_key",activity)})),custom,subset_rows(custom,{3,4}),engine);
    auto missing=run(variants_request(engine,source,"membership",{field("activity_key","missing")}));
    require(missing.status==400,"missing activity accepted");contains(missing,"\"source_unchanged\":true");
    if(engine=="pandas") {
      success(run(variants_request(engine,source,"min_coverage")),source,source,engine);
      success(run(variants_request(engine,source,"max_coverage")),source,source,engine);
      EventTable mapping;mapping.columns={"variant"};mapping.dtypes={"object"};
      EventAttribute tuple=parse_variants_filter_admitted_json("[[\"override\"]]",true).children.at(0);tuple.key="variant";
      mapping.rows={{tuple}};EventTableIndex index;index.kind="Index";index.dtype="object";index.names={"case:concept:name"};index.labels={{EventAttribute{"","c1"}}};mapping.index=index;
      auto request=variants_request(engine,source);for(auto& part:request)if(part.name=="admitted_json")part.bytes="[[\"override\"]]";
      request.push_back({"variants_file","mapping.json",serialize_embedding_workbench_table_json(mapping),true});
      const auto answer=run(request);success(answer,source,subset_rows(source,{0,1,2}),engine);contains(answer,"\"variants_source_unchanged\":true");
      require(text(answer.json,"variants_table_json")==serialize_embedding_workbench_table_json(mapping),"genuine supplied mapping lost");
      mapping.rows[0][0]=parse_variants_filter_admitted_json("[[\"A\",\"X\",\"B\"]]",true).children.at(0);mapping.rows[0][0]->key="variant";
      mapping.index->labels={{EventAttribute{"","c2"}}};
      for(const auto mode:{"top_k","min_coverage","max_coverage"}) {
        auto ranked=variants_request(engine,source,mode);ranked.push_back({"variants_file","mapping.json",serialize_embedding_workbench_table_json(mapping),true});
        const auto selected=run(ranked);success(selected,source,subset_rows(source,{3,4}),engine);contains(selected,"\"variants_source_unchanged\":true");
      }
    } else {
      for(const auto operation:{"from_list","auto"}){const auto answer=run(variants_request(engine,source,operation));require(answer.status==400,"unavailable helper invented");contains(answer,"DEFAULT_NAME_KEY");contains(answer,"\"apply_returned\":false");}
      const auto bad=run(variants_request(engine,source,"top_k",{field("timestamp_key","missing")}));require(bad.status==400,"missing ranking time accepted");contains(bad,"\"phase\":\"apply\"");
    }
  }
}
}
