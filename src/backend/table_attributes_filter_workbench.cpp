#include "minera/table_attributes_filter_workbench.hpp"
#include "minera/attributes_filter_controls.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/pandas_attributes_filter.hpp"
#include "minera/polars_attributes_filter.hpp"
#include <map>
#include <optional>
#include <set>
namespace minera {
namespace {
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventTable& table){return "{\"rows\":"+std::to_string(table.rows.size())+",\"columns\":"+std::to_string(table.columns.size())+'}';}
EventTable count_table(const PolarsAttributeCounts& entries){EventTable table;table.columns={"value","count"};table.dtypes={"object","object"};for(const auto& entry:entries)table.rows.push_back({entry.value,entry.count});return table;}
}
DiscoveryWorkbenchResponse execute_table_attributes_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error){
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source,result;std::optional<std::string> before,after;std::optional<bool> unchanged;
  std::string engine,operation,kind,phase="control",case_key="case:concept:name",attribute_key="concept:name",error_type,error,filtered,counted;
  int status=200;bool apply_returned=false;
  const auto field=[&](const std::string& name,std::string fallback={}){const auto found=fields.find(name);return found==fields.end()?fallback:found->second->bytes;};
  const auto boolean=[&](const std::string& name,std::string fallback){const auto value=field(name,std::move(fallback));if(value!="true" && value!="false")throw std::invalid_argument(name+" must be true or false");return parse_case_filter_bound(value);};
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"engine","operation","source_file","values_json","typed_values_json","lower","upper","criterion","positive","keep_nan_values","keep_once_per_case","case_id_key","attribute_key","activity_key","stream_filter_key1","stream_filter_value1_json","stream_filter_key2","stream_filter_value2_json","counts_json","counts_value_json","most_common_json"};
    for(const auto* collection:{&query,&parts})for(const auto& part:*collection){
      if(!allowed.contains(part.name))throw std::invalid_argument("unknown table attributes control: "+part.name);
      if(part.file && part.name!="source_file" && part.filename!=part.name+".txt")throw std::invalid_argument("ordinary table attributes controls must be text or canonical byte fields: "+part.name);
      if(!fields.emplace(part.name,&part).second)throw std::invalid_argument("duplicate table attributes control: "+part.name);
    }
    engine=field("engine");if(engine!="pandas" && engine!="polars"){engine.clear();throw std::invalid_argument("choose pandas or polars attributes filtering");}
    operation=field("operation");
    static const std::set<std::string> modes={"traces","events","numeric_traces","numeric_events","threshold","top_activities","on_values","relative","counts"};
    if(!modes.contains(operation)){operation.clear();throw std::invalid_argument("choose a supported attributes operation");}
    const bool membership=operation=="traces" || operation=="events" || operation=="on_values",numeric=operation.starts_with("numeric_"),relative=operation=="relative",counting=operation=="counts",threshold=operation=="threshold",frequency=threshold || operation=="top_activities";
    if(engine=="pandas" && counting)throw std::invalid_argument("the public attributes counting helper requires Polars");
    const auto value_fields=static_cast<unsigned>(fields.contains("values_json"))+static_cast<unsigned>(fields.contains("typed_values_json"));
    if(value_fields!=(membership?1U:0U))throw std::invalid_argument("supply exactly one active values_json or typed_values_json control");
    if(fields.contains("lower")!=numeric || fields.contains("upper")!=numeric)throw std::invalid_argument("lower and upper require numeric attributes filtering");
    if(fields.contains("criterion")!=(frequency || relative))throw std::invalid_argument("criterion requires an occurrence or activity-limit operation");
    if(fields.contains("positive") && !(membership || numeric || relative))throw std::invalid_argument("positive requires value, numeric or relative filtering");
    if(fields.contains("keep_nan_values") && (engine!="pandas" || frequency))throw std::invalid_argument("keep_nan_values requires pandas value, numeric or relative filtering");
    if(fields.contains("keep_once_per_case") && !(relative || counting))throw std::invalid_argument("keep_once_per_case requires relative filtering or attribute counts");
    const auto once=field("keep_once_per_case","default");
    if(fields.contains("keep_once_per_case") && once!="true" && once!="false")throw std::invalid_argument("keep_once_per_case must be true or false");
    const bool case_active=operation=="traces" || operation=="numeric_traces" || operation=="on_values" || (relative && once!="false") || (counting && once=="true");
    if(fields.contains("case_id_key") && !case_active)throw std::invalid_argument("case_id_key is inactive for this attributes operation");
    if(fields.contains("attribute_key") && frequency)throw std::invalid_argument("frequency helpers use activity_key");
    if(fields.contains("activity_key") && !frequency)throw std::invalid_argument("activity_key requires a direct frequency helper");
    for(const auto suffix:{"1","2"}){
      const auto key=std::string("stream_filter_key")+suffix,value=std::string("stream_filter_value")+suffix+"_json";
      if(fields.contains(key)!=fields.contains(value) || (fields.contains(key) && operation!="numeric_traces"))throw std::invalid_argument("stream controls require complete key/value pairs for numeric case filtering");
    }
    if((fields.contains("counts_json") || fields.contains("counts_value_json") || fields.contains("most_common_json")) && !threshold)throw std::invalid_argument("supplied counts and most-common controls require threshold filtering");
    if(fields.contains("counts_json") && fields.contains("counts_value_json"))throw std::invalid_argument("choose one supplied counter representation");
    const auto file=fields.find("source_file");if(file==fields.end() || !file->second->file)throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for(const auto& part:query)if(part.name=="source_file")throw std::invalid_argument("source_file must be a multipart File");
    PandasAttributesFilterOptions pandas;PolarsAttributesFilterOptions polars;
    case_key=field("case_id_key",case_key);attribute_key=field(frequency?"activity_key":"attribute_key",attribute_key);
    pandas.case_id_key=polars.case_id_key=EventAttribute{"",case_key};
    if(frequency)pandas.activity_key=polars.activity_key=EventAttribute{"",attribute_key};
    else pandas.attribute_key=polars.attribute_key=EventAttribute{"",attribute_key};
    pandas.positive=polars.positive=boolean("positive","true");pandas.keep_nan_values=boolean("keep_nan_values","false");
    if(fields.contains("keep_once_per_case")){const auto value=boolean("keep_once_per_case","true");pandas.keep_once_per_case=value;polars.keep_once_per_case=value;}
    for(const auto suffix:{"1","2"}){
      const auto key=std::string("stream_filter_key")+suffix,value=std::string("stream_filter_value")+suffix+"_json";
      if(fields.contains(key)){
        const EventAttribute selected{"",field(key)};const auto target=parse_attributes_filter_values_json(field(value),true);
        if(suffix==std::string("1")){pandas.stream_filter_key1=polars.stream_filter_key1=selected;pandas.stream_filter_value1=polars.stream_filter_value1=target;}
        else{pandas.stream_filter_key2=polars.stream_filter_key2=selected;pandas.stream_filter_value2=polars.stream_filter_value2=target;}
      }
    }
    const auto values=membership?parse_attributes_filter_values_json(field(fields.contains("typed_values_json")?"typed_values_json":"values_json"),fields.contains("typed_values_json")):EventAttribute{};
    const auto lower=numeric?parse_attributes_filter_bound_json(field("lower")):EventAttribute{},upper=numeric?parse_attributes_filter_bound_json(field("upper")):EventAttribute{};
    const auto criterion=(frequency || relative)?parse_attributes_filter_bound_json(field("criterion")):EventAttribute{};
    std::optional<PolarsAttributeCountCache> polars_cache;std::optional<EventAttribute> most_common;
    if(fields.contains("counts_json")){
      const auto entries=parse_attributes_filter_pairs_json(field("counts_json"));pandas.act_count0=PandasAttributesCountMap{entries};PolarsAttributeCounts counts;
      for(const auto& [key,value]:entries)counts.push_back({key,value});polars_cache=std::move(counts);
    }else if(fields.contains("counts_value_json")){
      const auto value=parse_attributes_filter_values_json(field("counts_value_json"),true);
      if(!value.is_null && value.container==AttributeContainerKind::kContainer){
        PandasAttributesCountMap mapping;PolarsAttributeCounts entries;
        for(const auto& child:value.children){EventAttribute key{"",child.key};mapping.entries.emplace_back(key,child);entries.push_back({std::move(key),child});}
        pandas.act_count0=std::move(mapping);polars_cache=std::move(entries);
      }else{pandas.act_count0=value;polars_cache=value;}
    }
    if(fields.contains("most_common_json")){most_common=parse_attributes_filter_values_json(field("most_common_json"),true);pandas.most_common_variant=most_common;}
    phase="source_import";source=parse_embedding_workbench_table_json(file->second->bytes);before=serialize_embedding_workbench_table_json(*source);phase="apply";
    if(engine=="pandas"){
      if(operation=="traces")result=filter_pandas_attributes(*source,values,pandas);
      else if(operation=="events")result=filter_pandas_attributes_events(*source,values,pandas);
      else if(operation=="numeric_traces")result=filter_pandas_attributes_numeric(*source,lower,upper,pandas);
      else if(operation=="numeric_events")result=filter_pandas_attributes_numeric_events(*source,lower,upper,pandas);
      else if(operation=="threshold")result=filter_pandas_attributes_threshold(*source,criterion,pandas);
      else if(operation=="top_activities")result=filter_pandas_attributes_top_activities(*source,criterion,pandas);
      else if(operation=="on_values")result=filter_pandas_attributes_on_values(*source,values,pandas);
      else result=filter_pandas_attributes_relative_occurrence(*source,criterion,pandas);
      apply_returned=true;kind="table";
    }else{
      const auto lazy=LazyEventTable::from_table(*source);
      if(counting){const auto entries=get_polars_attribute_values(lazy,polars.attribute_key,polars);apply_returned=true;result=count_table(entries);kind="counts";}
      else{
        const auto selected=operation=="traces"?filter_polars_attributes(lazy,values,polars):operation=="events"?filter_polars_attributes_events(lazy,values,polars):operation=="numeric_traces"?filter_polars_attributes_numeric(lazy,lower,upper,polars):operation=="numeric_events"?filter_polars_attributes_numeric_events(lazy,lower,upper,polars):operation=="threshold"?filter_polars_attributes_threshold(lazy,criterion,polars,polars_cache,most_common):operation=="top_activities"?filter_polars_attributes_top_activities(lazy,criterion,polars):operation=="on_values"?filter_polars_attributes_on_values(lazy,values,polars):filter_polars_attributes_relative(lazy,criterion,polars);
        apply_returned=true;phase="collect";result=selected.materialize();kind="table";
      }
    }
    phase="result_export";if(kind=="counts")counted=serialize_embedding_workbench_table_json(*result);else filtered=serialize_embedding_workbench_table_json(*result);phase="complete";
  }catch(const PandasAttributesFilterError& e){status=400;error_type=e.type;error=e.message;
  }catch(const PolarsAttributesFilterError& e){status=400;error_type=e.type;error=e.message;phase=e.phase;
  }catch(const std::exception& e){status=400;error_type="native_error";error=e.what();}
  if(source && before)try{after=serialize_embedding_workbench_table_json(*source);unchanged=*before==*after;}catch(const std::exception&){}
  const auto text=[&](const std::string& value,bool present){return present?q(value):std::string("null");};
  return {status,"{\"family\":\"TABLE_ATTRIBUTES_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+
    ",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(apply_returned?"true":"false")+
    ",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+",\"source_unchanged\":"+(unchanged?(*unchanged?"true":"false"):"null")+
    ",\"case_id_key\":"+q(case_key)+",\"attribute_key\":"+q(attribute_key)+",\"source_table_json\":"+(before?q(*before):"null")+
    ",\"source_after_table_json\":"+(after?q(*after):"null")+",\"filtered_table_json\":"+text(filtered,!filtered.empty())+",\"counts_table_json\":"+text(counted,!counted.empty())+
    ",\"result_kind\":"+text(kind,!kind.empty())+",\"error_type\":"+text(error_type,!error_type.empty())+",\"error\":"+text(error,!error.empty())+'}'};
}
}
