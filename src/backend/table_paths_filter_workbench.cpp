#include "minera/table_paths_filter_workbench.hpp"
#include "minera/paths_filter_controls.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/pandas_paths_filter.hpp"
#include "minera/polars_paths_filter.hpp"
#include <map>
#include <optional>
#include <set>
namespace minera {
namespace {
std::string q(std::string_view value) {return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventTable& table) {return "{\"rows\":" + std::to_string(table.rows.size()) + ",\"columns\":" + std::to_string(table.columns.size()) + '}';}
}
DiscoveryWorkbenchResponse execute_table_paths_filter(const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source, result; std::optional<std::string> before; std::optional<bool> unchanged;
  std::string engine, operation, phase = "control", case_key = "case:concept:name", activity_key = "concept:name", target_key = activity_key, time_key = "time:timestamp", error_type, error, filtered;
  bool apply_returned = false; int status = 200;
  const auto field = [&](const std::string& name, std::string fallback = {}) {const auto found = fields.find(name); return found == fields.end() ? fallback : found->second->bytes;};
  try {
    if (!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed = {"engine", "operation", "source_file", "paths_json", "path_json", "positive", "case_id_key", "activity_key", "target_attribute_key", "timestamp_key", "minimum", "maximum"};
    for (const auto* collection : {&query, &parts}) for (const auto& part : *collection) {
      if (!allowed.contains(part.name)) throw std::invalid_argument("unknown table paths control: " + part.name);
      if (!fields.emplace(part.name, &part).second) throw std::invalid_argument("duplicate table paths control: " + part.name);
    }
    engine = field("engine"); if (engine != "pandas" && engine != "polars") {engine.clear(); throw std::invalid_argument("choose pandas or polars paths filtering");}
    operation = field("operation"); if (operation != "paths" && operation != "performance") {operation.clear(); throw std::invalid_argument("choose paths or performance filtering");}
    const bool performance = operation == "performance";
    if (!fields.contains(performance ? "path_json" : "paths_json")) throw std::invalid_argument(performance ? "performance filtering requires path_json" : "paths filtering requires paths_json");
    if (fields.contains(performance ? "paths_json" : "path_json")) throw std::invalid_argument("choose only the active path control");
    if (!performance && (fields.contains("minimum") || fields.contains("maximum"))) throw std::invalid_argument("minimum/maximum require path performance filtering");
    if (performance && fields.contains("target_attribute_key")) throw std::invalid_argument("target_attribute_key requires ordinary paths filtering");
    const auto file = fields.find("source_file"); if (file == fields.end() || !file->second->file) throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for (const auto& part : query) if (part.name == "source_file") throw std::invalid_argument("source_file must be a multipart File");
    const auto paths = performance ? parse_paths_filter_pair_json(field("path_json")) : parse_paths_filter_pairs_json(field("paths_json"));
    const auto positive = field("positive", "true"); if (positive != "true" && positive != "false") throw std::invalid_argument("positive must be true or false");
    case_key = field("case_id_key", case_key); activity_key = field("activity_key", activity_key); target_key = field("target_attribute_key", activity_key); time_key = field("timestamp_key", time_key);
    PandasPathsFilterOptions pandas; PolarsPathsFilterOptions polars;
    pandas.case_id_key = polars.case_id_key = EventAttribute{"", case_key}; pandas.attribute_key = polars.attribute_key = EventAttribute{"", activity_key};
    pandas.timestamp_key = polars.timestamp_key = EventAttribute{"", time_key}; pandas.positive = polars.positive = parse_case_filter_bound(positive);
    if (fields.contains("target_attribute_key")) pandas.target_attribute_key = polars.target_attribute_key = EventAttribute{"", target_key};
    if (fields.contains("minimum")) pandas.min_performance = polars.min_performance = parse_case_filter_bound(field("minimum"));
    if (fields.contains("maximum")) pandas.max_performance = polars.max_performance = parse_case_filter_bound(field("maximum"));
    phase = "source_import"; source = parse_embedding_workbench_table_json(file->second->bytes); before = serialize_embedding_workbench_table_json(*source); phase = "apply";
    if (engine == "pandas") {result = performance ? filter_pandas_path_performance(*source, paths, pandas) : filter_pandas_paths(*source, paths, pandas); apply_returned = true;}
    else {const auto lazy = LazyEventTable::from_table(*source); const auto selected = performance ? filter_polars_path_performance(lazy, paths, polars) : filter_polars_paths(lazy, paths, polars); apply_returned = true; phase = "collect"; result = selected.materialize();}
    phase = "result_export"; filtered = serialize_embedding_workbench_table_json(*result); phase = "complete";
  } catch (const PandasPathsFilterError& e) {status = 400; error_type = e.type; error = e.message;
  } catch (const PolarsPathsFilterError& e) {status = 400; error_type = e.type; error = e.message; phase = e.phase;
  } catch (const std::exception& e) {status = 400; error_type = "native_error"; error = e.what();}
  if (source && before) try {unchanged = *before == serialize_embedding_workbench_table_json(*source);} catch (const std::exception&) {unchanged.reset();}
  const auto text = [&](const std::string& value, bool present) {return present ? q(value) : std::string("null");};
  return {status, "{\"family\":\"TABLE_PATHS_FILTER\",\"engine\":" + text(engine, !engine.empty()) + ",\"operation\":" + text(operation, !operation.empty()) +
    ",\"http_status\":" + std::to_string(status) + ",\"phase\":" + q(phase) + ",\"apply_returned\":" + (apply_returned ? "true" : "false") +
    ",\"source\":" + (source ? counts(*source) : "null") + ",\"result\":" + (result ? counts(*result) : "null") + ",\"source_unchanged\":" + (unchanged ? (*unchanged ? "true" : "false") : "null") +
    ",\"case_id_key\":" + q(case_key) + ",\"activity_key\":" + q(activity_key) + ",\"target_attribute_key\":" + q(target_key) + ",\"timestamp_key\":" + q(time_key) +
    ",\"source_table_json\":" + (before ? q(*before) : "null") + ",\"filtered_table_json\":" + text(filtered, !filtered.empty()) + ",\"error_type\":" + text(error_type, !error_type.empty()) + ",\"error\":" + text(error, !error.empty()) + '}'};
}
}
