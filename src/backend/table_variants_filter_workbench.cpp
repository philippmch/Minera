#include "minera/table_variants_filter_workbench.hpp"
#include "minera/variants_filter_controls.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/pandas_variants_filter.hpp"
#include "minera/polars_variants_filter.hpp"
#include <map>
#include <optional>
#include <set>
namespace minera {
namespace {
std::string q(std::string_view value) {return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventTable& table) {return "{\"rows\":"+std::to_string(table.rows.size())+",\"columns\":"+std::to_string(table.columns.size())+'}';}
}
DiscoveryWorkbenchResponse execute_table_variants_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,std::string_view transport_error) {
  std::map<std::string,const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source,result; std::optional<std::string> before,variants_before;
  std::optional<bool> unchanged,variants_unchanged;
  std::string engine,operation,phase="control",case_key="case:concept:name",activity_key="concept:name",time_key="time:timestamp",error_type,error,filtered;
  PandasVariantsFilterOptions pandas; PolarsVariantsFilterOptions polars;
  int status=200; bool apply_returned=false;
  const auto field=[&](const std::string& name,std::string fallback={}) {const auto found=fields.find(name);return found==fields.end()?fallback:found->second->bytes;};
  try {
    if(!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"engine","operation","source_file","admitted_json","variants_json","criterion","positive","case_id_key","activity_key","timestamp_key","variants_file","sequence_kind"};
    for(const auto* collection:{&query,&parts}) for(const auto& part:*collection) {
      if(!allowed.contains(part.name)) throw std::invalid_argument("unknown table variants control: "+part.name);
      if(!fields.emplace(part.name,&part).second) throw std::invalid_argument("duplicate table variants control: "+part.name);
    }
    engine=field("engine");if(engine!="pandas" && engine!="polars") {engine.clear();throw std::invalid_argument("choose pandas or polars variants filtering");}
    operation=field("operation");
    static const std::set<std::string> modes={"membership","top_k","min_coverage","max_coverage","from_list","auto"};
    if(!modes.contains(operation)) {operation.clear();throw std::invalid_argument("choose a supported variants filter");}
    const bool membership=operation=="membership",from_list=operation=="from_list",automatic=operation=="auto";
    if(engine=="pandas" && (from_list || automatic)) throw std::invalid_argument("from_list/auto variants filtering requires Polars");
    if(engine=="polars" && (operation=="min_coverage" || operation=="max_coverage")) throw std::invalid_argument("coverage variants filtering requires pandas");
    if(fields.contains("admitted_json")!=membership || fields.contains("variants_json")!=from_list)
      throw std::invalid_argument("supply only the active admitted_json/variants_json control");
    if(fields.contains("criterion")!=(operation=="top_k" || operation=="min_coverage" || operation=="max_coverage"))
      throw std::invalid_argument("criterion requires top_k or coverage filtering");
    if(fields.contains("variants_file") && engine!="pandas") throw std::invalid_argument("variants_file requires pandas variants filtering");
    if(fields.contains("sequence_kind") && !membership) throw std::invalid_argument("sequence_kind requires membership filtering");
    const auto sequence_kind=field("sequence_kind","list");
    if(sequence_kind!="list" && sequence_kind!="tuple") throw std::invalid_argument("sequence_kind must be list or tuple");
    if(fields.contains("timestamp_key") && (engine!="polars" || operation!="top_k")) throw std::invalid_argument("timestamp_key requires Polars top_k statistics");
    const auto file=fields.find("source_file");
    if(file==fields.end() || !file->second->file) throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for(const auto& part:query) if(part.name=="source_file" || part.name=="variants_file") throw std::invalid_argument("source/mapping tables must be multipart Files");
    const auto positive=field("positive","true");if(positive!="true" && positive!="false") throw std::invalid_argument("positive must be true or false");
    case_key=field("case_id_key",case_key);activity_key=field("activity_key",activity_key);
    time_key=field("timestamp_key",time_key);polars.timestamp_key=EventAttribute{"",time_key};
    pandas.case_id_key=polars.case_id_key=EventAttribute{"",case_key};pandas.activity_key=polars.activity_key=EventAttribute{"",activity_key};
    pandas.positive=polars.positive=parse_case_filter_bound(positive);
    const auto admitted=membership?parse_variants_filter_admitted_json(field("admitted_json"),sequence_kind=="tuple"):from_list?parse_variants_filter_string_list_json(field("variants_json")):EventAttribute{};
    const auto criterion=fields.contains("criterion")?parse_case_filter_bound(field("criterion")):EventAttribute{};
    phase="source_import";source=parse_embedding_workbench_table_json(file->second->bytes);before=serialize_embedding_workbench_table_json(*source);
    if(fields.contains("variants_file")) {
      const auto& mapping=*fields.at("variants_file");if(!mapping.file) throw std::invalid_argument("variants_file requires an uploaded native EventTable JSON File");
      phase="variants_import";auto table=parse_embedding_workbench_table_json(mapping.bytes);variants_before=serialize_embedding_workbench_table_json(table);pandas.variants_df=std::move(table);
    }
    phase="apply";
    if(engine=="pandas") {
      if(membership) result=filter_pandas_variants(*source,admitted,pandas);
      else if(operation=="top_k") result=filter_pandas_variants_top_k(*source,criterion,pandas);
      else if(operation=="min_coverage") result=filter_pandas_variants_min_coverage(*source,criterion,pandas);
      else result=filter_pandas_variants_max_coverage(*source,criterion,pandas);
      apply_returned=true;
    } else {
      const auto lazy=LazyEventTable::from_table(*source);
      const auto selected=membership?filter_polars_variants(lazy,admitted,polars):from_list?filter_polars_variants_from_list(lazy,admitted,polars):automatic?auto_filter_polars_variants(lazy,polars):filter_polars_variants_top_k(lazy,criterion,polars);
      apply_returned=true;phase="collect";result=selected.materialize();
    }
    phase="result_export";filtered=serialize_embedding_workbench_table_json(*result);phase="complete";
  } catch(const PandasVariantsFilterError& e) {status=400;error_type=e.type;error=e.message;
  } catch(const PolarsVariantsFilterError& e) {status=400;error_type=e.type;error=e.message;phase=e.phase;
  } catch(const std::exception& e) {status=400;error_type="native_error";error=e.what();}
  if(source && before) try {unchanged=*before==serialize_embedding_workbench_table_json(*source);} catch(const std::exception&) {}
  if(variants_before && pandas.variants_df && std::holds_alternative<EventTable>(*pandas.variants_df))
    try {variants_unchanged=*variants_before==serialize_embedding_workbench_table_json(std::get<EventTable>(*pandas.variants_df));} catch(const std::exception&) {}
  const auto text=[&](const std::string& value,bool present) {return present?q(value):std::string("null");};
  const auto boolean=[](std::optional<bool> value) {return value?(*value?"true":"false"):"null";};
  return {status,"{\"family\":\"TABLE_VARIANTS_FILTER\",\"engine\":"+text(engine,!engine.empty())+",\"operation\":"+text(operation,!operation.empty())+
    ",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+",\"apply_returned\":"+(apply_returned?"true":"false")+
    ",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+",\"source_unchanged\":"+boolean(unchanged)+
    ",\"case_id_key\":"+q(case_key)+",\"attribute_key\":"+q(activity_key)+",\"timestamp_key\":"+q(time_key)+",\"source_table_json\":"+(before?q(*before):"null")+
    ",\"filtered_table_json\":"+text(filtered,!filtered.empty())+",\"variants_table_json\":"+(variants_before?q(*variants_before):"null")+
    ",\"variants_source_unchanged\":"+boolean(variants_unchanged)+",\"error_type\":"+text(error_type,!error_type.empty())+",\"error\":"+text(error,!error.empty())+'}'};
}
}
