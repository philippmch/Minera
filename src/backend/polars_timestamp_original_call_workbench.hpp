#pragma once
#include "minera/table_timestamp_filter_workbench.hpp"
#include "polars_timestamp_original_call_parser.hpp"

namespace minera {
inline DiscoveryWorkbenchResponse execute_polars_timestamp_original_call(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  namespace original=polars_timestamp_original_call_detail;
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};const auto count=[](const EventTable& t){return "{\"rows\":"+std::to_string(t.rows.size())+",\"columns\":"+std::to_string(t.columns.size())+'}';};
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;std::optional<original::Call> call;std::optional<EventTable> source,result;
  std::optional<std::string> call_text,before,after,controls_before,controls_after,projection;std::optional<bool> source_unchanged,controls_unchanged;
  std::string engine,operation,phase="control",error_type,error,filtered;int status=200;bool returned=false;
  const auto field=[&](const std::string& name){const auto found=fields.find(name);return found==fields.end()?std::string{}:found->second->bytes;};
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    // Capture the unique raw call before engine/descriptor validation, preserving malformed imports too.
    const DiscoveryWorkbenchPart* selected=nullptr;for(const auto* items:{&query,&parts})for(const auto& p:*items)if(p.name=="call_json"){if(selected)throw std::invalid_argument("duplicate table timestamp control: call_json");selected=&p;}
    if(selected)call_text=selected->bytes;
    static const std::set<std::string> allowed={"engine","operation","source_file","call_json"};
    for(const auto* items:{&query,&parts})for(const auto& p:*items){
      if(!allowed.contains(p.name))throw std::invalid_argument("inactive or unknown original table timestamp control: "+p.name);
      if(p.file&&p.name!="source_file"&&p.filename!=p.name+".txt")throw std::invalid_argument("ordinary original timestamp controls require text or canonical byte File: "+p.name);
      if(!fields.emplace(p.name,&p).second)throw std::invalid_argument("duplicate table timestamp control: "+p.name);
    }
    engine=field("engine");if(engine!="polars"){engine.clear();throw std::invalid_argument("original Polars timestamp calls require polars");}
    operation=field("operation");if(!original::supported(operation)){operation.clear();throw std::invalid_argument("choose a supported original Polars timestamp operation");}
    if(!call_text)throw std::invalid_argument("original table timestamp calls require call_json");
    call=original::parse(*call_text,operation);
    for(const auto& p:query)if(p.name=="source_file")throw std::invalid_argument("source_file must be a multipart File");
    const auto uploaded=fields.find("source_file");if(uploaded==fields.end()||!uploaded->second->file)throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    phase="source_import";source=parse_embedding_workbench_table_json(uploaded->second->bytes);before=serialize_embedding_workbench_table_json(*source);
    controls_before=original::controls_json(*call,operation);projection=original::projection_json(*call,operation);phase="apply";
    const auto& o=call->operands;
    const auto invoke=[&](const auto& frame){
      using Frame=std::decay_t<decltype(frame)>;
      if(operation=="apply")return Frame(apply_polars_timestamp_filter(frame,call->raw_parameters));
      if(operation=="apply_auto_filter")return Frame(auto_filter_polars_timestamp(frame,call->raw_parameters));
      const auto& lower=o.at("dt1");const auto& upper=o.at("dt2");
      if(call->parameters){
        const auto& p=*call->parameters;
        if(operation=="apply_events")return Frame(filter_polars_timestamp_events_parameters(frame,lower,upper,p));
        if(operation=="filter_traces_contained")return Frame(filter_polars_timestamp_contained_parameters(frame,lower,upper,p));
        if(operation=="filter_traces_intersecting")return Frame(filter_polars_timestamp_intersecting_parameters(frame,lower,upper,p));
        return Frame(filter_polars_timestamp_attribute_parameters(frame,o.at("attribute"),o.at("attribute_value"),lower,upper,p));
      }
      const auto& p=call->raw_parameters;
      if(operation=="apply_events")return Frame(filter_polars_timestamp_events_parameter_value(frame,lower,upper,p));
      if(operation=="filter_traces_contained")return Frame(filter_polars_timestamp_contained_parameter_value(frame,lower,upper,p));
      if(operation=="filter_traces_intersecting")return Frame(filter_polars_timestamp_intersecting_parameter_value(frame,lower,upper,p));
      return Frame(filter_polars_timestamp_attribute_parameter_value(frame,o.at("attribute"),o.at("attribute_value"),lower,upper,p));
    };
    if(call->eager){result=invoke(*source);returned=true;}
    else{const auto selected_frame=invoke(LazyEventTable::from_table(*source));returned=true;phase="collect";result=selected_frame.materialize();}
    phase="result_export";filtered=serialize_embedding_workbench_table_json(*result);phase="complete";
  }catch(const PolarsTimestampFilterError& e){status=400;error_type=e.type;error=e.message;phase=e.phase;}
  catch(const std::exception& e){status=400;error_type="native_error";error=e.what();}
  if(source&&before)try{after=serialize_embedding_workbench_table_json(*source);source_unchanged=*before==*after;}catch(const std::exception&){}
  if(call&&controls_before)try{controls_after=original::controls_json(*call,operation);controls_unchanged=*controls_before==*controls_after;}catch(const std::exception&){}
  const auto text=[&](const std::string& x,bool present){return present?q(x):std::string("null");};const auto optional=[&](const std::optional<std::string>& x){return x?q(*x):std::string("null");};const auto boolean=[](std::optional<bool> x){return x?(*x?"true":"false"):"null";};
  return {status,"{\"family\":\"TABLE_TIMESTAMP_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(returned?"true":"false")+",\"source\":"+(source?count(*source):"null")+",\"result\":"+(result?count(*result):"null")+",\"source_unchanged\":"+boolean(source_unchanged)+",\"case_id_key\":null,\"attribute_key\":null,\"timestamp_key\":null,\"source_table_json\":"+optional(before)+",\"filtered_table_json\":"+text(filtered,!filtered.empty())+",\"error_type\":"+text(error_type,status==400)+",\"error\":"+text(error,status==400)+",\"call_mode\":"+(call_text?q("original"):"null")+",\"call_json\":"+optional(call_text)+",\"controls_before_json\":"+optional(controls_before)+",\"controls_after_json\":"+optional(controls_after)+",\"controls_unchanged\":"+boolean(controls_unchanged)+",\"source_after_table_json\":"+optional(after)+",\"call_projection_json\":"+optional(projection)+'}'};
}
} // namespace minera
