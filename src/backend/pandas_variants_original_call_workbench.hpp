#pragma once
#include "minera/table_variants_filter_workbench.hpp"
#include "pandas_variants_original_call_parser.hpp"

namespace minera {
inline DiscoveryWorkbenchResponse execute_pandas_variants_original_call(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  namespace original=pandas_variants_original_call_detail;
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};const auto count=[](const EventTable& t){return "{\"rows\":"+std::to_string(t.rows.size())+",\"columns\":"+std::to_string(t.columns.size())+'}';};
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;std::optional<original::Call> call;std::optional<EventTable> source,result;
  std::optional<std::string> call_text,before,after,controls_before,controls_after,variants_before;std::optional<bool> source_unchanged,controls_unchanged,variants_unchanged;
  std::string engine,operation,phase="control",error_type,error,filtered;int status=200;bool returned=false;
  const auto field=[&](const std::string& name){const auto found=fields.find(name);return found==fields.end()?std::string{}:found->second->bytes;};
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    // Capture the unique raw call before engine/descriptor validation, preserving malformed imports too.
    const DiscoveryWorkbenchPart* selected=nullptr;for(const auto* items:{&query,&parts})for(const auto& p:*items)if(p.name=="call_json"){if(selected)throw std::invalid_argument("duplicate table variants control: call_json");selected=&p;}
    if(selected)call_text=selected->bytes;
    static const std::set<std::string> allowed={"engine","operation","source_file","call_json","variants_file"};
    for(const auto* items:{&query,&parts})for(const auto& p:*items){
      if(!allowed.contains(p.name))throw std::invalid_argument("inactive or unknown original table variants control: "+p.name);
      if(p.file&&p.name!="source_file"&&p.name!="variants_file"&&p.filename!=p.name+".txt")throw std::invalid_argument("ordinary original variants controls require text or canonical byte File: "+p.name);
      if(!fields.emplace(p.name,&p).second)throw std::invalid_argument("duplicate table variants control: "+p.name);
    }
    engine=field("engine");if(engine!="pandas"){engine.clear();throw std::invalid_argument("original table variants calls require pandas");}
    operation=field("operation");if(operation!="membership"&&operation!="top_k"&&operation!="min_coverage"&&operation!="max_coverage"){operation.clear();throw std::invalid_argument("choose a supported original pandas variants operation");}
    if(!call_text)throw std::invalid_argument("original table variants calls require call_json");
    call=original::parse(*call_text,operation);
    if(fields.contains("variants_file")!=call->frame_entry.has_value())throw std::invalid_argument("variants_file is required only for the active direct DataFrame reference");
    for(const auto& p:query)if(p.name=="source_file"||p.name=="variants_file")throw std::invalid_argument("source and variants tables must be multipart Files");
    const auto uploaded=fields.find("source_file");if(uploaded==fields.end()||!uploaded->second->file)throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    if(call->frame_entry&&!fields.at("variants_file")->file)throw std::invalid_argument("variants_file requires an uploaded native EventTable JSON File");
    phase="source_import";source=parse_embedding_workbench_table_json(uploaded->second->bytes);before=serialize_embedding_workbench_table_json(*source);
    if(call->frame_entry){phase="variants_import";call->parameters->at(*call->frame_entry).table=parse_embedding_workbench_table_json(fields.at("variants_file")->bytes);variants_before=serialize_embedding_workbench_table_json(*original::variants_table(*call));}
    controls_before=original::controls_json(*call);phase="apply";
    const auto invoke=[&](const auto& parameter)->EventTable{
      if(operation=="membership"){if(call->admission)return filter_pandas_variants(*source,*call->admission,parameter);return filter_pandas_variants(*source,call->criterion,parameter);}
      if(operation=="top_k")return filter_pandas_variants_top_k(*source,call->criterion,parameter);
      if(operation=="min_coverage")return filter_pandas_variants_min_coverage(*source,call->criterion,parameter);
      return filter_pandas_variants_max_coverage(*source,call->criterion,parameter);
    };
    result=call->parameters?invoke(*call->parameters):invoke(call->raw_parameters);returned=true;phase="result_export";filtered=serialize_embedding_workbench_table_json(*result);phase="complete";
  }catch(const PandasVariantsFilterError& e){status=400;error_type=e.type;error=e.message;}
  catch(const std::exception& e){status=400;error_type="native_error";error=e.what();}
  if(source&&before)try{after=serialize_embedding_workbench_table_json(*source);source_unchanged=*before==*after;}catch(const std::exception&){}
  if(call&&controls_before)try{controls_after=original::controls_json(*call);controls_unchanged=*controls_before==*controls_after;}catch(const std::exception&){}
  if(call&&variants_before)try{const auto* table=original::variants_table(*call);if(table)variants_unchanged=*variants_before==serialize_embedding_workbench_table_json(*table);}catch(const std::exception&){}
  const auto text=[&](const std::string& x,bool present){return present?q(x):std::string("null");};const auto optional=[&](const std::optional<std::string>& x){return x?q(*x):std::string("null");};const auto boolean=[](std::optional<bool> x){return x?(*x?"true":"false"):"null";};
  return {status,"{\"family\":\"TABLE_VARIANTS_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(returned?"true":"false")+",\"source\":"+(source?count(*source):"null")+",\"result\":"+(result?count(*result):"null")+",\"source_unchanged\":"+boolean(source_unchanged)+",\"case_id_key\":null,\"attribute_key\":null,\"timestamp_key\":null,\"source_table_json\":"+optional(before)+",\"filtered_table_json\":"+text(filtered,!filtered.empty())+",\"variants_table_json\":"+optional(variants_before)+",\"variants_source_unchanged\":"+boolean(variants_unchanged)+",\"error_type\":"+text(error_type,status==400)+",\"error\":"+text(error,status==400)+",\"call_mode\":"+(call_text?q("original"):"null")+",\"call_json\":"+optional(call_text)+",\"controls_before_json\":"+optional(controls_before)+",\"controls_after_json\":"+optional(controls_after)+",\"controls_unchanged\":"+boolean(controls_unchanged)+",\"source_after_table_json\":"+optional(after)+'}'};
}
} // namespace minera
