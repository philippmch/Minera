#include "minera/log_variants_filter_workbench.hpp"
#include "minera/xes_export.hpp"
#include "log_variants_original_call_parser.hpp"
#include <map>
#include <optional>
#include <set>

namespace minera {
namespace {
std::string variants_counts(const EventLog& log){std::size_t events=0;for(const auto& trace:log.traces)events+=trace.events.size();return "{\"traces\":"+std::to_string(log.traces.size())+",\"events\":"+std::to_string(events)+'}';}
}
DiscoveryWorkbenchResponse execute_log_variants_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  namespace original=log_variants_original_call_detail;
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventLog> source,result;
  std::optional<original::Call> call;
  std::optional<std::string> raw_call,before,after,controls_before,controls_after,raw_result,xes,xes_error,value_json;
  std::optional<bool> unchanged,controls_unchanged;
  std::string operation,mode,format,phase="control",error_type,error;
  int status=200;
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    if(!query.empty())throw std::invalid_argument("log variants controls require multipart fields; query fields are inactive");
    static const std::set<std::string> allowed={"operation","source_file","call_json"};
    for(const auto& part:parts){
      if(!allowed.contains(part.name))throw std::invalid_argument("unknown log variants control: "+part.name);
      if(part.file&&part.name!="source_file"&&part.filename!=part.name+".txt")throw std::invalid_argument("log variants controls require text or canonical byte Files: "+part.name);
      if(!fields.emplace(part.name,&part).second)throw std::invalid_argument("duplicate log variants control: "+part.name);
    }
    if(fields.contains("call_json")){mode="original";raw_call=fields.at("call_json")->bytes;}
    if(!raw_call)throw std::invalid_argument("log variants filtering requires call_json");
    if(fields.contains("operation"))operation=fields.at("operation")->bytes;
    original::signature(operation);
    call=original::parse(*raw_call,operation);
    const auto file=fields.find("source_file");
    if(file==fields.end()||!file->second->file)throw std::invalid_argument("source_file requires an uploaded native log File");
    format="native_log";phase="source_import";
    source=parse_embedding_workbench_raw_log_json(file->second->bytes);
    before=serialize_embedding_workbench_raw_log_json(*source);
    phase="control";original::materialize(*call,*source);
    controls_before=original::controls_json(*call);
    phase="filter";
    auto& c=*call;
    if(operation=="apply"){
      const auto& admission=c.binding(0);
      if(admission.kind==original::Operand::kAdmission)result=filter_log_variants(*source,admission.native_admission,c.options);
      else result=filter_log_variants(*source,admission.value,c.options);
    }else if(operation=="filter_log_variants_percentage")result=filter_log_variants_cumulative(*source,*c.bound,c.options);
    else if(operation=="filter_variants_by_coverage_percentage")result=filter_log_variants_min_coverage(*source,*c.bound,c.options);
    else if(operation=="filter_variants_by_maximum_coverage_percentage")result=filter_log_variants_max_coverage(*source,*c.bound,c.options);
    else if(operation=="filter_variants_top_k")result=filter_log_variants_top_k(*source,*c.bound,c.options);
    else{
      const auto& variants=c.binding(0);
      if(operation=="filter_variants_variants_percentage"){
        if(variants.kind==original::Operand::kVariants)result=filter_log_variants_dictionary_percentage(*source,variants.variants,*c.bound);
        else result=filter_log_variants_dictionary_percentage(*source,variants.value,*c.bound);
      }else{
        const auto value=variants.kind==original::Operand::kVariants?find_log_variants_auto_threshold(*source,variants.variants,*c.bound):find_log_variants_auto_threshold(*source,variants.value,*c.bound);
        value_json=original::cell_json(original::real(value),"float64");
      }
    }
    phase="result_export";
    if(result){
      raw_result=serialize_embedding_workbench_raw_log_json(*result);
      try{XesExportOptions options;options.preserve_nulls=true;xes=serialize_xes(*result,options);}catch(const std::exception& failure){xes_error=failure.what();}
    }
    phase="complete";
  }catch(const LogVariantsFilterError& failure){status=400;error_type=failure.type;error=failure.message;}
  catch(const std::exception& failure){status=400;error_type="native_error";error=failure.what();}
  if(source&&before)try{after=serialize_embedding_workbench_raw_log_json(*source);unchanged=*before==*after;}catch(const std::exception&){}
  if(call&&controls_before)try{controls_after=original::controls_json(*call);controls_unchanged=*controls_before==*controls_after;}catch(const std::exception&){}
  const auto q=[](std::string_view v){return serialize_embedding_workbench_string_json(v);};
  const auto text=[&](const std::string& v){return v.empty()?std::string("null"):q(v);};
  const auto optional=[&](const std::optional<std::string>& v){return v?q(*v):std::string("null");};
  const auto boolean=[](const std::optional<bool>& v){return v?(*v?"true":"false"):"null";};
  return {status,"{\"family\":\"REFERENCE_LOG_VARIANTS\",\"operation\":"+text(operation)+",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+
    ",\"source_format\":"+text(format)+",\"source\":"+(source?variants_counts(*source):"null")+",\"result\":"+(result?variants_counts(*result):"null")+
    ",\"source_unchanged\":"+boolean(unchanged)+",\"call_mode\":"+text(mode)+",\"call_json\":"+optional(raw_call)+
    ",\"controls_before_json\":"+optional(controls_before)+",\"controls_after_json\":"+optional(controls_after)+",\"controls_unchanged\":"+boolean(controls_unchanged)+
    ",\"source_log_json\":"+optional(before)+",\"source_after_log_json\":"+optional(after)+",\"filtered_log_json\":"+optional(raw_result)+
    ",\"filtered_xes\":"+optional(xes)+",\"xes_error\":"+optional(xes_error)+",\"value_json\":"+optional(value_json)+",\"error_type\":"+original::failure_text_json(error_type,status==400)+",\"error\":"+original::failure_text_json(error,status==400)+'}'};
}
}
