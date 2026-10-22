#include "minera/log_attributes_filter_workbench.hpp"
#include "minera/attributes_filter_controls.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/log_attributes_filter.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "log_attributes_original_call_parser.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <sstream>
namespace minera {
namespace {
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventLog& log){std::size_t events=0;for(const auto& trace:log.traces)events+=trace.events.size();return "{\"traces\":"+std::to_string(log.traces.size())+",\"events\":"+std::to_string(events)+'}';}
std::string parameters_json(const LogAttributesFilterParameters& parameters){
  EventTable table;table.columns={"key","value","enum_key"};table.dtypes={"object","object","object"};
  for(const auto& item:parameters)table.rows.push_back({EventAttribute{"",item.key},item.value,EventAttribute{"",item.enum_key?"true":"false",AttributeKind::kBoolean}});
  return serialize_embedding_workbench_table_json(table);
}
std::size_t trace_offset(const EventAttribute& value,std::size_t size){
  if(value.is_null || value.container!=AttributeContainerKind::kScalar || value.kind!=AttributeKind::kInt)
    throw std::invalid_argument("variant trace offsets require exact nonnegative integers");
  std::size_t offset=0;const auto parsed=std::from_chars(value.value.data(),value.value.data()+value.value.size(),offset);
  if(parsed.ec!=std::errc{} || parsed.ptr!=value.value.data()+value.value.size() || offset>=size)
    throw std::invalid_argument("variant trace offset is outside the auxiliary log");
  return offset;
}
}
DiscoveryWorkbenchResponse execute_log_attributes_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventLog> source,result,variants_source;
  std::optional<std::string> before,after,variants_before,variants_after,parameters_before,parameters_after;
  std::optional<std::string> call_text,cache_before,cache_after,original_counts_echo,original_variants_echo,original_vc_echo;
  std::optional<bool> unchanged,variants_unchanged,parameters_unchanged;
  std::optional<log_attributes_original_call_detail::Call> original_call;
  std::optional<LogAttributesVariantInput> original_cache;
  EventAttribute original_selector;
  LogAttributesFilterParameters parameters;
  std::string operation,format,phase="control",open_key="concept:name",attribute_key="concept:name",raw_result,xes,xes_error,error_type,error,call_mode;
  int status=200;
  const auto field=[&](const std::string& name,std::string fallback={}){const auto found=fields.find(name);return found==fields.end()?fallback:found->second->bytes;};
  const auto boolean=[&](const std::string& name,std::string fallback){const auto value=field(name,std::move(fallback));if(value!="true" && value!="false")throw std::invalid_argument(name+" must be true or false");return EventAttribute{"",value,AttributeKind::kBoolean};};
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"operation","source_file","source_format","case_id_column","activity_column","timestamp_column","delimiter","trim_fields","activity_key","attribute_key","positive","values_json","typed_values_json","lower","upper","criterion","case_id_key","case_id_glue","keep_once_per_case","stream_filter_key1","stream_filter_value1_json","stream_filter_key2","stream_filter_value2_json","counts_json","variants_file","variant_traces_json","variant_counts_json","call_json"};
    for(const auto* collection:{&query,&parts})for(const auto& part:*collection){
      if(!allowed.contains(part.name))throw std::invalid_argument("unknown log attributes control: "+part.name);
      if(part.file && part.name!="source_file" && part.name!="variants_file" && part.filename!=part.name+".txt")throw std::invalid_argument("ordinary log attributes controls must be text or canonical byte fields: "+part.name);
      if(!fields.emplace(part.name,&part).second)throw std::invalid_argument("duplicate log attributes control: "+part.name);
    }
    call_mode=fields.contains("call_json")?"original":"friendly";
    if(call_mode=="original")call_text=field("call_json");
    operation=field("operation");
    static const std::set<std::string> modes={"traces","events","numeric_traces","numeric_events","trace_values","top_activities","relative","threshold"};
    if(!modes.contains(operation)){operation.clear();throw std::invalid_argument("choose a supported log attributes operation");}
    if(call_mode=="original") {
      namespace original=log_attributes_original_call_detail;
      for(const auto& [name,part]:fields){(void)part;if(name!="operation"&&name!="source_file"&&name!="variants_file"&&name!="call_json")throw std::invalid_argument("friendly controls are inactive for original LOG attributes calls: "+name);}
      original_call=original::parse(*call_text,operation);auto& call=*original_call;
      if(fields.contains("variants_file")!=call.needs_auxiliary)throw std::invalid_argument("variants_file is required exactly when original cache uses auxiliary trace handles");
      const auto uploaded=fields.find("source_file");
      if(uploaded==fields.end()||!uploaded->second->file)throw std::invalid_argument("source_file requires an uploaded native log File");
      if(call.needs_auxiliary&&!fields.at("variants_file")->file)throw std::invalid_argument("variants_file requires an uploaded native log File");
      for(const auto& part:query)if(part.name=="source_file"||part.name=="variants_file")throw std::invalid_argument(part.name+" must be a multipart File");
      if(call.parameters)parameters_before=parameters_json(*call.parameters);
      else if(call.raw_parameters)parameters_before=original::attribute_json(*call.raw_parameters);
      if(operation=="threshold"){original_counts_echo=call.counts_echo;original_variants_echo=call.variants_echo;original_vc_echo=call.vc_echo;}
      format="native_log";phase="source_import";source=parse_embedding_workbench_raw_log_json(uploaded->second->bytes);
      bool found=false;
      for(const auto& trace:source->traces){for(const auto& event:trace.events){
        if(!event.activity_source_key.empty()){open_key=event.activity_source_key;found=true;break;}
        if(event.activity_attribute){open_key=event.activity_attribute->key;found=true;break;}
      }if(found)break;}
      before=serialize_embedding_workbench_raw_log_json(*source);
      if(call.needs_auxiliary){phase="auxiliary_import";variants_source=parse_embedding_workbench_raw_log_json(fields.at("variants_file")->bytes);variants_before=serialize_embedding_workbench_raw_log_json(*variants_source);}
      if(operation=="threshold") {
        original_cache=original::materialize(call,*source,variants_source);
        original_selector=call.args.size()==3?call.args[2]:EventAttribute{"","concept:name"};
        cache_before=original::cache_json(call.counts,*original_cache,call.args[0],original_selector);
      }
      phase="apply";
      auto options=call.parameters?resolve_log_attributes_filter_parameters(*call.parameters):LogAttributesFilterOptions{};
      if(call.raw_parameters)options.raw_parameters=*call.raw_parameters;
      if(operation=="traces")result=filter_log_attributes_traces(*source,call.args[0],options);
      else if(operation=="events")result=filter_log_attributes_events(*source,call.args[0],options);
      else if(operation=="trace_values")result=filter_log_attributes_trace_values(*source,call.args[0],options);
      else if(operation=="numeric_traces")result=filter_log_attributes_numeric(*source,call.args[0],call.args[1],options);
      else if(operation=="numeric_events")result=filter_log_attributes_numeric_events(*source,call.args[0],call.args[1],options);
      else if(operation=="top_activities") {
        const auto maximum=call.args.empty()?EventAttribute{"","25",AttributeKind::kInt}:call.args[0];
        if(call.parameters)result=filter_log_attributes_max_activities(*source,maximum,*call.parameters);
        else result=filter_log_attributes_max_activities(*source,maximum,options);
      }else if(operation=="relative")result=filter_log_attributes_relative_occurrence(*source,call.args[0],options);
      else result=filter_log_attributes_threshold(*source,call.counts,*original_cache,call.args[0],call.args[1],original_selector);
    }else {
    const bool membership=operation=="traces" || operation=="events" || operation=="trace_values",numeric=operation.starts_with("numeric_"),relative=operation=="relative",maximum=operation=="top_activities",threshold=operation=="threshold";
    const bool converter=operation=="events" || numeric || relative || maximum;
    const auto value_fields=static_cast<unsigned>(fields.contains("values_json"))+static_cast<unsigned>(fields.contains("typed_values_json"));
    if(value_fields!=(membership?1U:0U))throw std::invalid_argument("supply exactly one active values_json or typed_values_json control");
    if(fields.contains("lower")!=numeric || fields.contains("upper")!=numeric)throw std::invalid_argument("lower and upper require numeric attributes filtering");
    if(fields.contains("criterion")!=(relative || maximum || threshold))throw std::invalid_argument("criterion requires relative, maximum or threshold filtering");
    if(fields.contains("positive") && threshold)throw std::invalid_argument("positive is inactive for threshold filtering");
    if(fields.contains("case_id_key") && !converter)throw std::invalid_argument("case_id_key requires converter-based attributes filtering");
    if(fields.contains("case_id_glue") && operation!="numeric_traces")throw std::invalid_argument("case_id_glue requires numeric case filtering");
    if(fields.contains("keep_once_per_case") && !relative)throw std::invalid_argument("keep_once_per_case requires relative filtering");
    if(fields.contains("attribute_key") && maximum)throw std::invalid_argument("maximum activities uses activity_key");
    for(const auto suffix:{"1","2"}){
      const auto key=std::string("stream_filter_key")+suffix,value=std::string("stream_filter_value")+suffix+"_json";
      if(fields.contains(key)!=fields.contains(value) || (fields.contains(key) && operation!="numeric_traces"))throw std::invalid_argument("stream controls require complete key/value pairs for numeric case filtering");
    }
    for(const auto& name:{"counts_json","variants_file","variant_traces_json","variant_counts_json"})
      if(fields.contains(name)!=threshold)throw std::invalid_argument("threshold requires counts_json, variants_file, variant_traces_json and variant_counts_json");
    const auto file=fields.find("source_file");if(file==fields.end() || !file->second->file)throw std::invalid_argument("source_file requires an uploaded log File");
    if(threshold && !fields.at("variants_file")->file)throw std::invalid_argument("variants_file requires a separate uploaded native log File");
    for(const auto& part:query)if(part.name=="source_file" || part.name=="variants_file")throw std::invalid_argument(part.name+" must be a multipart File");
    const auto& uploaded=*file->second;
    format=field("source_format","auto");
    if(format=="auto"){
      auto name=uploaded.filename;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      const auto first=uploaded.bytes.find_first_not_of(" \r\n\t");
      if(name.ends_with(".csv"))format="csv";
      else if(name.ends_with(".json"))format="native_log";
      else if(name.ends_with(".xes") || name.ends_with(".xes.gz"))format="xes";
      else format=first!=std::string::npos && uploaded.bytes[first]=='{'?"native_log":first!=std::string::npos && uploaded.bytes[first]=='<'?"xes":"csv";
    }
    if(format!="csv" && format!="xes" && format!="native_log"){format.clear();throw std::invalid_argument("source_format must be auto, csv, xes or native_log");}
    if(format!="csv")for(const auto& name:{"case_id_column","activity_column","timestamp_column","delimiter","trim_fields"})if(fields.contains(name))throw std::invalid_argument("CSV controls require a CSV source");
    open_key=field("activity_key",format=="csv"?field("activity_column","activity"):"concept:name");
    attribute_key=maximum?open_key:field("attribute_key",operation=="trace_values"?"concept:name":format=="csv"?field("activity_column","activity"):"concept:name");
    // The mutable ordinary parameter dictionary is preserved separately from
    // source data, including maximum-activity helper writes before failures.
    parameters.push_back({maximum?"pm4py:param:activity_key":"pm4py:param:attribute_key",EventAttribute{"",attribute_key}});
    if(!threshold)parameters.push_back({"positive",boolean("positive","true")});
    if(fields.contains("case_id_key"))parameters.push_back({"pm4py:param:case_id_key",EventAttribute{"",field("case_id_key")}});
    if(fields.contains("case_id_glue"))parameters.push_back({"case_id_glue",EventAttribute{"",field("case_id_glue")}});
    if(fields.contains("keep_once_per_case"))parameters.push_back({"keep_once_per_case",boolean("keep_once_per_case","true")});
    for(const auto suffix:{"1","2"}){
      const auto key=std::string("stream_filter_key")+suffix,value=std::string("stream_filter_value")+suffix+"_json";
      if(fields.contains(key)){parameters.push_back({key,EventAttribute{"",field(key)}});parameters.push_back({std::string("stream_filter_value")+suffix,parse_attributes_filter_values_json(field(value),true)});}
    }
    const auto values=membership?parse_attributes_filter_values_json(field(fields.contains("typed_values_json")?"typed_values_json":"values_json"),fields.contains("typed_values_json")):EventAttribute{};
    const auto lower=numeric?parse_attributes_filter_bound_json(field("lower")):EventAttribute{},upper=numeric?parse_attributes_filter_bound_json(field("upper")):EventAttribute{};
    const auto criterion=(relative || maximum || threshold)?parse_attributes_filter_bound_json(field("criterion")):EventAttribute{};
    parameters_before=parameters_json(parameters);
    phase="source_import";
    if(format=="native_log")source=parse_embedding_workbench_raw_log_json(uploaded.bytes);
    else{
      std::istringstream input(uploaded.bytes);
      if(format=="xes")source=import_xes_event_log(input);
      else{
        CsvImportOptions options;options.case_id_column=field("case_id_column","case_id");options.activity_column=field("activity_column","activity");options.timestamp_column=field("timestamp_column","timestamp");
        options.trim_fields=boolean("trim_fields","false").value=="true";options.allow_case_id_aliases=true;
        const auto delimiter=field("delimiter",",");if(delimiter.size()!=1)throw std::invalid_argument("CSV delimiter must contain exactly one byte");options.delimiter=delimiter.front();source=import_csv_event_log(input,options);
      }
    }
    if(format!="csv"){
      bool found=false;
      for(const auto& trace:source->traces){for(const auto& event:trace.events){
        if(!event.activity_source_key.empty()){open_key=event.activity_source_key;found=true;break;}
        if(event.activity_attribute){open_key=event.activity_attribute->key;found=true;break;}
      }if(found)break;}
    }
    if(converter && format=="csv")for(auto& trace:source->traces)
      if(trace.case_id_source_key!="concept:name" && std::none_of(trace.attributes.begin(),trace.attributes.end(),[](const auto& value){return value.key=="concept:name";})){
        auto canonical=trace.case_id_attribute.value_or(EventAttribute{"",trace.case_id});canonical.key="concept:name";trace.attributes.push_back(std::move(canonical));
      }
    before=serialize_embedding_workbench_raw_log_json(*source);
    LogAttributesCounts supplied_counts;LogVariantsDictionary variants;EventAttribute vc;
    if(threshold){
      phase="auxiliary_import";variants_source=parse_embedding_workbench_raw_log_json(fields.at("variants_file")->bytes);variants_before=serialize_embedding_workbench_raw_log_json(*variants_source);
      supplied_counts=parse_attributes_filter_pairs_json(field("counts_json"));
      const auto entries=parse_attributes_filter_pairs_json(field("variant_traces_json"));
      for(const auto& [key,indices]:entries){
        if(indices.is_null || indices.container!=AttributeContainerKind::kList)throw std::invalid_argument("variant traces require typed lists of auxiliary trace offsets");
        LogVariantsDictionaryEntry entry;entry.variant=key;
        for(const auto& index:indices.children)entry.traces.push_back(variants_source->traces[trace_offset(index,variants_source->traces.size())]);
        variants.push_back(std::move(entry));
      }
      vc=parse_attributes_filter_values_json(field("variant_counts_json"),true);
    }
    phase="apply";const auto options=resolve_log_attributes_filter_parameters(parameters);
    if(operation=="traces")result=filter_log_attributes_traces(*source,values,options);
    else if(operation=="events")result=filter_log_attributes_events(*source,values,options);
    else if(operation=="trace_values")result=filter_log_attributes_trace_values(*source,values,options);
    else if(operation=="numeric_traces")result=filter_log_attributes_numeric(*source,lower,upper,options);
    else if(operation=="numeric_events")result=filter_log_attributes_numeric_events(*source,lower,upper,options);
    else if(maximum)result=filter_log_attributes_max_activities(*source,criterion,parameters);
    else if(relative)result=filter_log_attributes_relative_occurrence(*source,criterion,options);
    else result=filter_log_attributes_threshold(*source,supplied_counts,variants,vc,criterion,EventAttribute{"",attribute_key});
    }
    phase="result_export";raw_result=serialize_embedding_workbench_raw_log_json(*result);
    try{XesExportOptions export_options;export_options.preserve_nulls=true;xes=serialize_xes(*result,export_options);}catch(const std::exception& e){xes_error=e.what();}
    phase="complete";
  }catch(const LogAttributesFilterError& e){status=400;error_type=e.type;error=e.message;
  }catch(const std::exception& e){status=400;error_type="native_error";error=e.what();}
  if(source && before)try{after=serialize_embedding_workbench_raw_log_json(*source);unchanged=*before==*after;}catch(const std::exception&){}
  if(variants_source && variants_before)try{variants_after=serialize_embedding_workbench_raw_log_json(*variants_source);variants_unchanged=*variants_before==*variants_after;}catch(const std::exception&){}
  if(parameters_before)try{
    if(original_call){if(original_call->parameters)parameters_after=parameters_json(*original_call->parameters);else if(original_call->raw_parameters)parameters_after=log_attributes_original_call_detail::attribute_json(*original_call->raw_parameters);}
    else parameters_after=parameters_json(parameters);
    if(parameters_after)parameters_unchanged=*parameters_before==*parameters_after;
  }catch(const std::exception&){}
  if(cache_before&&original_call&&original_cache)try{cache_after=log_attributes_original_call_detail::cache_json(original_call->counts,*original_cache,original_call->args[0],original_selector);}catch(const std::exception&){}
  const auto text=[&](const std::string& value,bool present){return present?q(value):std::string("null");};
  const auto optional_text=[&](const std::optional<std::string>& value){return value?q(*value):std::string("null");};
  const auto optional_bool=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};
  return {status,"{\"family\":\"LOG_ATTRIBUTES_FILTER\",\"operation\":"+text(operation,!operation.empty())+",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+
    ",\"source_format\":"+text(format,!format.empty())+",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+
    ",\"source_unchanged\":"+optional_bool(unchanged)+",\"activity_key\":"+text(open_key,source.has_value())+",\"filter_attribute_key\":"+text(attribute_key,source.has_value()&&call_mode!="original")+
    ",\"source_log_json\":"+optional_text(before)+",\"source_after_log_json\":"+optional_text(after)+",\"parameters_before_json\":"+optional_text(parameters_before)+",\"parameters_after_json\":"+optional_text(parameters_after)+
    ",\"parameters_unchanged\":"+optional_bool(parameters_unchanged)+",\"variants_source_log_json\":"+optional_text(variants_before)+",\"variants_source_after_log_json\":"+optional_text(variants_after)+",\"variants_source_unchanged\":"+optional_bool(variants_unchanged)+
    ",\"threshold_counts_json\":"+(call_mode=="original"?optional_text(original_counts_echo):text(field("counts_json"),fields.contains("counts_json")))+",\"threshold_variants_map_json\":"+(call_mode=="original"?optional_text(original_variants_echo):text(field("variant_traces_json"),fields.contains("variant_traces_json")))+",\"variant_counts_json\":"+(call_mode=="original"?optional_text(original_vc_echo):text(field("variant_counts_json"),fields.contains("variant_counts_json")))+
    ",\"call_mode\":"+text(call_mode,!call_mode.empty())+",\"call_json\":"+optional_text(call_text)+",\"threshold_cache_before_json\":"+optional_text(cache_before)+",\"threshold_cache_after_json\":"+optional_text(cache_after)+
    ",\"filtered_log_json\":"+text(raw_result,!raw_result.empty())+",\"filtered_xes\":"+text(xes,result.has_value() && xes_error.empty() && error.empty())+",\"xes_error\":"+text(xes_error,!xes_error.empty())+",\"error_type\":"+text(error_type,!error_type.empty())+",\"error\":"+text(error,!error.empty())+'}'};
}
}
