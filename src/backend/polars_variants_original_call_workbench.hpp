#pragma once
#include "minera/table_variants_filter_workbench.hpp"
#include "polars_variants_original_call_parser.hpp"

namespace minera {
inline DiscoveryWorkbenchResponse execute_polars_variants_original_call(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  namespace original=polars_variants_original_call_detail;
  const auto q=[](std::string_view value){return serialize_embedding_workbench_string_json(value);};
  const auto counts=[](const EventTable& table){return "{\"rows\":"+std::to_string(table.rows.size())+",\"columns\":"+std::to_string(table.columns.size())+'}';};
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;std::optional<original::Call> call,caller_before;std::optional<EventTable> source,result;
  std::optional<std::string> call_text,before,after,controls_before,controls_after,returned_metadata;std::optional<bool> source_unchanged,controls_unchanged;
  std::string engine,operation,phase="control",error_type,error,filtered;int status=200;bool returned=false;
  const auto field=[&](const std::string& name){const auto found=fields.find(name);return found==fields.end()?std::string{}:found->second->bytes;};
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    const DiscoveryWorkbenchPart* selected=nullptr;for(const auto* collection:{&query,&parts})for(const auto& part:*collection)if(part.name=="call_json"){if(selected)throw std::invalid_argument("duplicate table variants control: call_json");selected=&part;}
    if(selected)call_text=selected->bytes;
    static const std::set<std::string> allowed={"engine","operation","source_file","call_json"};
    for(const auto* collection:{&query,&parts})for(const auto& part:*collection){
      if(!allowed.contains(part.name))throw std::invalid_argument("inactive or unknown original table variants control: "+part.name);
      if(part.file&&part.name!="source_file"&&part.filename!=part.name+".txt")throw std::invalid_argument("ordinary original variants controls require text or canonical byte File: "+part.name);
      if(!fields.emplace(part.name,&part).second)throw std::invalid_argument("duplicate table variants control: "+part.name);
    }
    engine=field("engine");if(engine!="polars"){engine.clear();throw std::invalid_argument("original Polars variants calls require polars");}
    operation=field("operation");if(operation!="membership"&&operation!="top_k"&&operation!="from_list"&&operation!="auto"){operation.clear();throw std::invalid_argument("choose a supported original Polars variants operation");}
    if(!call_text)throw std::invalid_argument("original Polars variants calls require call_json");call=original::parse(*call_text,operation);
    for(const auto& part:query)if(part.name=="source_file")throw std::invalid_argument("source table must be a multipart File");
    const auto uploaded=fields.find("source_file");if(uploaded==fields.end()||!uploaded->second->file)throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    phase="source_import";source=parse_embedding_workbench_table_json(uploaded->second->bytes);before=serialize_embedding_workbench_table_json(*source);
    caller_before=*call;controls_before=original::controls_json(*call);phase="apply";
    if(call->eager){
      result=original::invoke(*source,*call);returned=true;phase="result_export";returned_metadata=original::returned_metadata(*result);
    }else{
      const auto input=LazyEventTable::from_table(*source);const auto selected_table=original::invoke(input,*call);returned=true;phase="result_export";returned_metadata=original::returned_metadata(selected_table);
      phase="collect";result=selected_table.materialize();
    }
    phase="result_export";filtered=serialize_embedding_workbench_table_json(*result);phase="complete";
  }catch(const PolarsVariantsFilterError& failure){status=400;error_type=failure.type;error=failure.message;phase=failure.phase;}
  catch(const std::exception& failure){status=400;error_type="native_error";error=failure.what();}
  if(source&&before)try{after=serialize_embedding_workbench_table_json(*source);source_unchanged=*before==*after;}catch(const std::exception&){}
  if(call&&caller_before&&controls_before)try{controls_after=original::controls_json(*call);controls_unchanged=original::same_call(*caller_before,*call)&&*controls_before==*controls_after;}catch(const std::exception&){}
  const auto text=[&](const std::string& value,bool present){return present?q(value):std::string("null");};
  const auto optional=[&](const std::optional<std::string>& value){return value?q(*value):std::string("null");};
  const auto boolean=[](std::optional<bool> value){return value?(*value?"true":"false"):"null";};
  return {status,"{\"family\":\"TABLE_VARIANTS_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(returned?"true":"false")+",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+",\"source_unchanged\":"+boolean(source_unchanged)+",\"case_id_key\":null,\"attribute_key\":null,\"timestamp_key\":null,\"source_table_json\":"+optional(before)+",\"filtered_table_json\":"+text(filtered,!filtered.empty())+",\"variants_table_json\":null,\"variants_source_unchanged\":null,\"error_type\":"+text(error_type,status==400)+",\"error\":"+text(error,status==400)+",\"call_mode\":"+(call_text?q("original"):"null")+",\"call_json\":"+optional(call_text)+",\"controls_before_json\":"+optional(controls_before)+",\"controls_after_json\":"+optional(controls_after)+",\"controls_unchanged\":"+boolean(controls_unchanged)+",\"source_after_table_json\":"+optional(after)+",\"returned_table_metadata_json\":"+optional(returned_metadata)+'}'};
}
} // namespace minera
