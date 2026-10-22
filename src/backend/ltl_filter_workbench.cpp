#include "minera/ltl_filter_workbench.hpp"
#include "minera/attributes_filter_controls.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/log_ltl_filter.hpp"
#include "minera/pandas_ltl_filter.hpp"
#include "minera/polars_ltl_filter.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include "pandas_ltl_original_call_parser.hpp"
#include "log_ltl_original_call_parser.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <sstream>
namespace minera {
namespace {
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventTable& table){return "{\"rows\":"+std::to_string(table.rows.size())+",\"columns\":"+std::to_string(table.columns.size())+'}';}
std::string counts(const EventLog& log){std::size_t events=0;for(const auto& trace:log.traces)events+=trace.events.size();return "{\"traces\":"+std::to_string(log.traces.size())+",\"events\":"+std::to_string(events)+'}';}
std::string helper_json(const EventAttribute& value){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{value}};return serialize_embedding_workbench_table_json(table);}
}
DiscoveryWorkbenchResponse execute_ltl_filter_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error) {
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventLog> log,result_log;std::optional<EventTable> table,result_table;
  std::optional<std::string> before,after;std::optional<bool> unchanged;
  std::optional<std::string> original_call_text;
  std::optional<PolarsLtlFilterCall> original_call;
  std::optional<pandas_ltl_original_call_detail::Call> original_pandas_call;
  std::optional<log_ltl_original_call_detail::Call> original_log_call;
  std::string engine,operation,kind,format,input_kind,call_mode,phase="control",open_key="concept:name",case_key="case:concept:name",attribute_key="concept:name",resource_key="org:resource",timestamp_key="time:timestamp";
  std::string result_json,xes,xes_error,helper_result,error_type,error;int status=200;bool apply_returned=false;
  const auto field=[&](const std::string& name,std::string fallback={}){const auto found=fields.find(name);return found==fields.end()?fallback:found->second->bytes;};
  const auto boolean=[&](const std::string& name,std::string fallback){const auto value=field(name,std::move(fallback));if(value!="true" && value!="false")throw std::invalid_argument(name+" must be true or false");return EventAttribute{"",value,AttributeKind::kBoolean};};
  try {
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"engine","operation","input_kind","call_json","source_file","source_format","case_id_column","activity_column","timestamp_column","resource_column","delimiter","trim_fields","activity_key","case_id_key","attribute_key","resource_key","timestamp_key","positive","enable_timestamp","timestamp_diff_boundaries_json","values_json","typed_values_json","a_json","b_json","c_json","target_json","helper_a_json","helper_b_json"};
    for(const auto* collection:{&query,&parts})for(const auto& part:*collection){
      if(!allowed.contains(part.name))throw std::invalid_argument("unknown relationship control: "+part.name);
      if(part.file && part.name!="source_file" && part.filename!=part.name+".txt")throw std::invalid_argument("ordinary relationship controls must be text or canonical byte fields: "+part.name);
      if(!fields.emplace(part.name,&part).second)throw std::invalid_argument("duplicate relationship control: "+part.name);
    }
    call_mode=fields.contains("call_json")?"original":"friendly";
    if(call_mode=="original")original_call_text=field("call_json");
    engine=field("engine");if(engine!="log" && engine!="pandas" && engine!="polars"){engine.clear();throw std::invalid_argument("choose log, pandas or polars relationship filtering");}
    if(engine=="polars"){
      const auto selected_kind=field("input_kind","LazyFrame");
      if(selected_kind!="LazyFrame" && selected_kind!="DataFrame")throw std::invalid_argument("input_kind must be LazyFrame or DataFrame");
      input_kind=selected_kind;
    }else if(fields.contains("input_kind"))throw std::invalid_argument("input_kind requires the polars engine");
    operation=field("operation");static const std::set<std::string> modes={"eventually","next","four_eyes","different_persons","timestamp_ge","timestamp_le"};
    if(!modes.contains(operation)){operation.clear();throw std::invalid_argument("choose a supported relationship operation");}
    const bool helper=operation.starts_with("timestamp_"),collection=operation=="eventually" || (engine=="polars" && (operation=="next" || operation=="four_eyes")),social=operation=="four_eyes" || operation=="different_persons";
    if(call_mode=="original"){
      if(helper && engine!="log")throw std::invalid_argument(engine=="polars"?"original Polars calls require a relationship operation":"original pandas calls require a relationship operation");
      for(const auto& [name,value]:fields){(void)value;if(name!="engine" && name!="operation" && name!="input_kind" && name!="source_file" && name!="call_json")throw std::invalid_argument("friendly control conflicts with original call_json: "+name);}
      if(engine=="log"){
        if(helper && fields.contains("source_file"))throw std::invalid_argument("original timestamp helpers forbid source_file by presence");
        original_log_call=log_ltl_original_call_detail::parse(*original_call_text,helper?2U:operation=="next"?3U:operation=="four_eyes"?2U:1U,helper);
      }else if(engine=="pandas")original_pandas_call=pandas_ltl_original_call_detail::parse(*original_call_text,operation=="next"?3U:operation=="four_eyes"?2U:1U);
      else {
        original_call=polars_ltl_original_call_detail::parse(*original_call_text);
        original_call->operation=operation=="eventually"?PolarsLtlFilterOperation::kEventuallyFollows:operation=="next"?PolarsLtlFilterOperation::kNext:operation=="four_eyes"?PolarsLtlFilterOperation::kFourEyes:PolarsLtlFilterOperation::kDifferentPersons;
      }
    }
    if(helper && engine!="log")throw std::invalid_argument("timestamp list helpers require the event-log engine");
    if(helper){
      EventAttribute a,b;
      if(original_log_call){a=original_log_call->args[0];b=original_log_call->args[1];}
      else{
      for(const auto& [name,value]:fields){(void)value;if(name!="engine" && name!="operation" && name!="helper_a_json" && name!="helper_b_json")throw std::invalid_argument("inactive timestamp-helper control: "+name);}
      if(!fields.contains("helper_a_json") || !fields.contains("helper_b_json"))throw std::invalid_argument("both typed timestamp operands are required");
      a=parse_attributes_filter_values_json(field("helper_a_json"),true);b=parse_attributes_filter_values_json(field("helper_b_json"),true);
      }
      phase="apply";
      helper_result=helper_json(operation=="timestamp_ge"?log_ltl_timestamp_list_is_ge(a,b):log_ltl_timestamp_list_is_le(a,b));
      apply_returned=true;kind="helper";phase="complete";
    }else {
      EventAttribute enabled{"","false",AttributeKind::kBoolean};
      if(call_mode=="friendly"){
      if(fields.contains("helper_a_json") || fields.contains("helper_b_json"))throw std::invalid_argument("timestamp helper operands require a helper operation");
      const auto value_fields=static_cast<unsigned>(fields.contains("values_json"))+static_cast<unsigned>(fields.contains("typed_values_json"));
      if(value_fields!=(collection?1U:0U))throw std::invalid_argument("supply exactly one active collection representation");
      if(fields.contains("target_json")!=(operation=="different_persons"))throw std::invalid_argument("target_json requires the different-performers rule");
      const bool named=!collection && operation!="different_persons";
      if(fields.contains("a_json")!=named || fields.contains("b_json")!=named || fields.contains("c_json")!=(named && operation=="next"))throw std::invalid_argument("supply exactly the active relationship operands");
      if(fields.contains("case_id_key") && engine=="log")throw std::invalid_argument("case_id_key requires a table engine");
      if(fields.contains("resource_key") && !social)throw std::invalid_argument("resource_key requires a performer rule");
      if(fields.contains("enable_timestamp") && (operation!="eventually" || engine=="polars"))throw std::invalid_argument("timestamp difference constraints require log or pandas eventual filtering");
      enabled=boolean("enable_timestamp","false");
      const bool timestamp_order=engine=="polars" && (operation=="eventually" || operation=="next"),difference_bounds=engine!="polars" && operation=="eventually";
      if(fields.contains("timestamp_key") && !(timestamp_order || difference_bounds && (enabled.value=="true" || engine=="pandas" && !fields.contains("enable_timestamp") && fields.contains("timestamp_diff_boundaries_json"))))throw std::invalid_argument("timestamp_key requires Polars sequence ordering or enabled timestamp differences");
      if(fields.contains("timestamp_diff_boundaries_json") && !difference_bounds)throw std::invalid_argument("timestamp difference boundaries require log or pandas eventual filtering");
      if(engine!="log")for(const auto& name:{"source_format","activity_key","case_id_column","activity_column","timestamp_column","resource_column","delimiter","trim_fields"})if(fields.contains(name))throw std::invalid_argument("log source control requires the event-log engine: "+std::string(name));
      }
      const auto uploaded=fields.find("source_file");if(uploaded==fields.end() || !uploaded->second->file)throw std::invalid_argument("source_file requires an actual multipart File");
      for(const auto& part:query)if(part.name=="source_file")throw std::invalid_argument("source_file must be a multipart File");
      EventAttribute values,target,a,b,c,positive;std::optional<EventAttribute> boundaries;
      if(call_mode=="friendly"){
        const bool named=!collection && operation!="different_persons";
        if(collection)values=parse_attributes_filter_values_json(field(fields.contains("typed_values_json")?"typed_values_json":"values_json"),fields.contains("typed_values_json"));
        if(operation=="different_persons")target=parse_attributes_filter_values_json(field("target_json"),true);
        if(named){a=parse_attributes_filter_values_json(field("a_json"),true);b=parse_attributes_filter_values_json(field("b_json"),true);if(operation=="next")c=parse_attributes_filter_values_json(field("c_json"),true);}
        positive=boolean("positive","true");
        if(fields.contains("timestamp_diff_boundaries_json"))boundaries=parse_attributes_filter_values_json(field("timestamp_diff_boundaries_json"),true);
      }
      if(engine=="log"){
        format=original_log_call?"native_log":field("source_format","auto");if(format=="auto"){
          auto name=uploaded->second->filename;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
          const auto first=uploaded->second->bytes.find_first_not_of(" \r\n\t");
          if(name.ends_with(".csv"))format="csv";else if(name.ends_with(".json"))format="native_log";else if(name.ends_with(".xes") || name.ends_with(".xes.gz"))format="xes";
          else format=first!=std::string::npos && uploaded->second->bytes[first]=='{'?"native_log":first!=std::string::npos && uploaded->second->bytes[first]=='<'?"xes":"csv";
        }
        if(format!="csv" && format!="xes" && format!="native_log"){format.clear();throw std::invalid_argument("source_format must be auto, csv, xes or native_log");}
        if(format!="csv")for(const auto& name:{"case_id_column","activity_column","timestamp_column","resource_column","delimiter","trim_fields"})if(fields.contains(name))throw std::invalid_argument("CSV controls require a CSV source");
        open_key=field("activity_key",format=="csv"?field("activity_column","activity"):"concept:name");
        attribute_key=field("attribute_key",format=="csv"?field("activity_column","activity"):"concept:name");
        resource_key=field("resource_key",format=="csv"?field("resource_column","resource"):"org:resource");timestamp_key=field("timestamp_key",format=="csv"?field("timestamp_column","timestamp"):"time:timestamp");
        phase="source_import";
        if(format=="native_log")log=parse_embedding_workbench_raw_log_json(uploaded->second->bytes);
        else {std::istringstream input(uploaded->second->bytes);if(format=="xes")log=import_xes_event_log(input);else {
          CsvImportOptions options;options.case_id_column=field("case_id_column","case_id");options.activity_column=field("activity_column","activity");options.timestamp_column=field("timestamp_column","timestamp");options.resource_column=field("resource_column","resource");
          options.trim_fields=boolean("trim_fields","false").value=="true";options.allow_case_id_aliases=true;const auto delimiter=field("delimiter",",");if(delimiter.size()!=1)throw std::invalid_argument("CSV delimiter must contain exactly one byte");options.delimiter=delimiter.front();log=import_csv_event_log(input,options);
        }}
        if(format!="csv" && !fields.contains("activity_key")){bool found=false;for(const auto& trace:log->traces){for(const auto& event:trace.events){if(!event.activity_source_key.empty()){open_key=event.activity_source_key;found=true;break;}if(event.activity_attribute){open_key=event.activity_attribute->key;found=true;break;}}if(found)break;}}
        before=serialize_embedding_workbench_raw_log_json(*log);phase="apply";
        LogLtlFilterOptions options;
        if(original_log_call){
          options=original_log_call->options;
          if(operation=="eventually")values=original_log_call->args[0];
          else if(operation=="different_persons")target=original_log_call->args[0];
          else {a=original_log_call->args[0];b=original_log_call->args[1];if(operation=="next")c=original_log_call->args[2];}
        }else {options.attribute_key={"",attribute_key};options.resource_key={"",resource_key};options.timestamp_key={"",timestamp_key};options.positive=positive;options.enable_timestamp=enabled;options.timestamp_diff_boundaries=boundaries;}
        if(operation=="eventually")result_log=filter_log_ltl_eventually_follows(*log,values,options);
        else if(operation=="next")result_log=filter_log_ltl_next(*log,a,b,c,options);
        else if(operation=="four_eyes")result_log=filter_log_ltl_four_eyes(*log,a,b,options);
        else result_log=filter_log_ltl_different_persons(*log,target,options);
        apply_returned=true;kind="log";phase="result_export";result_json=serialize_embedding_workbench_raw_log_json(*result_log);
        try{XesExportOptions opts;opts.preserve_nulls=true;xes=serialize_xes(*result_log,opts);}catch(const std::exception& e){xes_error=e.what();}
      }else {
        format="native_table";case_key=field("case_id_key",case_key);attribute_key=field("attribute_key",attribute_key);resource_key=field("resource_key",resource_key);timestamp_key=field("timestamp_key",timestamp_key);
        phase="source_import";table=parse_embedding_workbench_table_json(uploaded->second->bytes);before=serialize_embedding_workbench_table_json(*table);phase="apply";
        if(engine=="pandas"){
          if(original_pandas_call){
            const auto& call=*original_pandas_call;
            const auto invoke=[&](const auto& parameters){
              if(operation=="eventually")return filter_pandas_ltl_eventually_follows(*table,call.args[0],parameters);
              if(operation=="next")return filter_pandas_ltl_next(*table,call.args[0],call.args[1],call.args[2],parameters);
              if(operation=="four_eyes")return filter_pandas_ltl_four_eyes(*table,call.args[0],call.args[1],parameters);
              return filter_pandas_ltl_different_persons(*table,call.args[0],parameters);
            };
            if(call.parameter_value)result_table=invoke(*call.parameter_value);
            else if(call.parameter_entries)result_table=invoke(*call.parameter_entries);
            else result_table=invoke(PandasLtlFilterOptions{});
          }else{
          PandasLtlFilterOptions options;options.case_id_key={"",case_key};options.attribute_key={"",attribute_key};options.resource_key={"",resource_key};options.timestamp_key={"",timestamp_key};options.positive=positive;if(fields.contains("enable_timestamp"))options.enable_timestamp=enabled;options.timestamp_diff_boundaries=boundaries;
          if(operation=="eventually")result_table=filter_pandas_ltl_eventually_follows(*table,values,options);
          else if(operation=="next")result_table=filter_pandas_ltl_next(*table,a,b,c,options);
          else if(operation=="four_eyes")result_table=filter_pandas_ltl_four_eyes(*table,a,b,options);
          else result_table=filter_pandas_ltl_different_persons(*table,target,options);
          }
          apply_returned=true;
        }else {
          PolarsLtlFilterOptions options;options.case_id_key={"",case_key};options.attribute_key={"",attribute_key};options.resource_key={"",resource_key};options.timestamp_key={"",timestamp_key};options.positive=positive;options.enable_timestamp=enabled;options.timestamp_diff_boundaries=boundaries;
          if(input_kind=="DataFrame"){
            if(original_call)result_table=invoke_polars_ltl_filter(*table,*original_call);
            else result_table=operation=="eventually"?filter_polars_ltl_eventually_follows(*table,values,options):operation=="next"?filter_polars_ltl_next(*table,values,options):operation=="four_eyes"?filter_polars_ltl_four_eyes(*table,values,options):filter_polars_ltl_different_persons(*table,target,options);
            apply_returned=true;
          }else{
            const auto lazy=LazyEventTable::from_table(*table);
            const auto selected=original_call?invoke_polars_ltl_filter(lazy,*original_call):operation=="eventually"?filter_polars_ltl_eventually_follows(lazy,values,options):operation=="next"?filter_polars_ltl_next(lazy,values,options):operation=="four_eyes"?filter_polars_ltl_four_eyes(lazy,values,options):filter_polars_ltl_different_persons(lazy,target,options);
            apply_returned=true;phase="collect";result_table=selected.materialize();
          }
        }
        kind="table";phase="result_export";result_json=serialize_embedding_workbench_table_json(*result_table);
      }
      phase="complete";
    }
  }catch(const LogLtlFilterError& e){status=400;error_type=e.type;error=e.message;
  }catch(const PandasLtlFilterError& e){status=400;error_type=e.type;error=e.message;
  }catch(const PolarsLtlFilterError& e){status=400;error_type=e.type;error=e.message;phase=e.phase;
  }catch(const std::exception& e){status=400;error_type="native_error";error=e.what();}
  if(before)try{if(log)after=serialize_embedding_workbench_raw_log_json(*log);else if(table)after=serialize_embedding_workbench_table_json(*table);if(after)unchanged=*before==*after;}catch(const std::exception&){}
  const auto text=[&](const std::string& value,bool present){return present?q(value):std::string("null");};
  const auto optional_text=[&](const std::optional<std::string>& value){return value?q(*value):std::string("null");};
  const bool imported=log.has_value() || table.has_value();
  const bool friendly_roles=imported && call_mode!="original";
  return {status,"{\"family\":\"REFERENCE_LTL_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+",\"result_kind\":"+text(kind,!kind.empty())+
    ",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(apply_returned?"true":"false")+",\"source_format\":"+text(format,!format.empty())+",\"input_kind\":"+text(input_kind,!input_kind.empty())+
    ",\"call_mode\":"+text(call_mode,!call_mode.empty())+",\"call_json\":"+optional_text(original_call_text)+
    ",\"source\":"+(log?counts(*log):table?counts(*table):"null")+",\"result\":"+(result_log?counts(*result_log):result_table?counts(*result_table):"null")+",\"source_unchanged\":"+(unchanged?(*unchanged?"true":"false"):"null")+
    ",\"activity_key\":"+text(open_key,log.has_value())+",\"case_id_key\":"+text(case_key,table.has_value() && friendly_roles)+",\"attribute_key\":"+text(attribute_key,friendly_roles)+",\"resource_key\":"+text(resource_key,friendly_roles)+",\"timestamp_key\":"+text(timestamp_key,friendly_roles)+
    ",\"source_json\":"+optional_text(before)+",\"source_after_json\":"+optional_text(after)+",\"result_json\":"+text(result_json,!result_json.empty())+",\"filtered_xes\":"+text(xes,result_log.has_value() && xes_error.empty() && error.empty())+
    ",\"xes_error\":"+text(xes_error,!xes_error.empty())+",\"helper_result_json\":"+text(helper_result,!helper_result.empty())+",\"error_type\":"+text(error_type,!error_type.empty())+",\"error\":"+text(error,!error.empty())+'}'};
}
}
