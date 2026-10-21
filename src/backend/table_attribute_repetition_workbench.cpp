#include "minera/table_attribute_repetition_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/pandas_attribute_repetition_filter.hpp"
#include "minera/polars_attribute_repetition_filter.hpp"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace minera {
namespace {
std::string q(std::string_view text) { return serialize_embedding_workbench_string_json(text); }
std::string counts(const EventTable& table) {
  return "{\"rows\":" + std::to_string(table.rows.size()) +
         ",\"columns\":" + std::to_string(table.columns.size()) + '}';
}
template<class Scalar> Scalar bound(std::string_view text) {
  return std::visit([](const auto& value) -> Scalar {
    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, NativeNA> ||
                  std::is_same_v<std::decay_t<decltype(value)>, EventAttribute>)
      throw std::invalid_argument("Table repetition bounds require supported scalar values");
    else return value;
  }, parse_log_attribute_repetition_bound_json(text));
}
}  // namespace

DiscoveryWorkbenchResponse execute_table_attribute_repetition(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source, result;
  std::optional<std::string> source_before;
  std::optional<bool> unchanged;
  std::string engine, phase="control", case_key="case:concept:name", attribute_key="concept:name";
  std::string error_type, error, filtered;
  bool apply_returned=false;
  int status=200;
  const auto field=[&](const std::string& name, std::string fallback={}) {
    const auto at=fields.find(name); return at==fields.end()?fallback:at->second->bytes;
  };
  try {
    if(!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed={"engine","source_file","target_json","case_id_key","attribute_key","min_rep","max_rep"};
    for(const auto* collection:{&query,&parts}) for(const auto& part:*collection) {
      if(!allowed.contains(part.name)) throw std::invalid_argument("unknown table repetition control: "+part.name);
      if(!fields.emplace(part.name,&part).second) throw std::invalid_argument("duplicate table repetition control: "+part.name);
    }
    engine=field("engine");
    if(engine!="pandas" && engine!="polars") { engine.clear();throw std::invalid_argument("choose pandas or polars table repetition"); }
    if(!fields.contains("target_json")) throw std::invalid_argument("table repetition requires target_json");
    const auto found=fields.find("source_file");
    if(found==fields.end() || !found->second->file) throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for(const auto& part:query) if(part.name=="source_file") throw std::invalid_argument("source_file must be a multipart File");
    case_key=field("case_id_key",case_key); attribute_key=field("attribute_key",attribute_key);
    // Controls are parsed separately from either engine's apply/collect phases.
    const auto target=parse_log_attribute_repetition_target_json(field("target_json"));
    PandasAttributeRepetitionOptions pandas; PolarsAttributeRepetitionOptions polars;
    pandas.case_id_key=case_key; pandas.attribute_key=attribute_key;
    polars.case_id_key=case_key; polars.attribute_key=attribute_key;
    if(fields.contains("min_rep")) {
      pandas.minimum=bound<PandasAttributeRepetitionScalar>(field("min_rep"));
      polars.minimum=bound<PolarsAttributeRepetitionScalar>(field("min_rep"));
    }
    if(fields.contains("max_rep")) {
      pandas.maximum=bound<PandasAttributeRepetitionScalar>(field("max_rep"));
      polars.maximum=bound<PolarsAttributeRepetitionScalar>(field("max_rep"));
    }
    phase="source_import";
    source=parse_embedding_workbench_table_json(found->second->bytes);
    source_before=serialize_embedding_workbench_table_json(*source);
    if(engine=="pandas") {
      phase="apply";
      result=filter_pandas_attribute_value_repetition(*source,target,pandas);
      apply_returned=true;
    } else {
      phase="lazy_source";
      const auto lazy=LazyEventTable::from_table(*source);
      phase="apply";
      const auto filtered_lazy=filter_polars_attribute_value_repetition(lazy,target,polars);
      apply_returned=true;
      phase="collect";
      result=filtered_lazy.materialize();
    }
    unchanged=*source_before==serialize_embedding_workbench_table_json(*source);
    phase="result_export";
    filtered=serialize_embedding_workbench_table_json(*result);
    phase="complete";
  } catch(const PandasAttributeRepetitionError& e) {
    status=400;error_type=e.type;error=e.message;
  } catch(const PolarsAttributeRepetitionError& e) {
    status=400;error_type=e.type;error=e.message;phase=e.phase;
  } catch(const std::exception& e) {
    status=400;error_type="native_error";error=e.what();
  }
  if(source && source_before) try { unchanged=*source_before==serialize_embedding_workbench_table_json(*source); }
    catch(const std::exception&) { unchanged.reset(); }
  const auto text=[&](const std::string& value,bool present) { return present?q(value):std::string("null"); };
  std::string json="{\"family\":\"TABLE_ATTRIBUTE_REPETITION\",\"engine\":"+text(engine,!engine.empty())+
    ",\"http_status\":"+std::to_string(status)+",\"phase\":"+q(phase)+
    ",\"apply_returned\":"+(apply_returned?"true":"false")+
    ",\"source\":"+(source?counts(*source):"null")+",\"result\":"+(result?counts(*result):"null")+
    ",\"source_unchanged\":"+(unchanged?(*unchanged?"true":"false"):"null")+
    ",\"case_id_key\":"+q(case_key)+",\"attribute_key\":"+q(attribute_key)+
    ",\"source_table_json\":"+(source_before?q(*source_before):"null")+
    ",\"filtered_table_json\":"+text(filtered,!filtered.empty())+
    ",\"error_type\":"+text(error_type,!error_type.empty())+",\"error\":"+text(error,!error.empty())+'}';
  return {status,std::move(json)};
}
}  // namespace minera
