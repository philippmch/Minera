#include "minera/table_timestamp_filter_workbench.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/pandas_timestamp_filter.hpp"
#include "minera/polars_timestamp_filter.hpp"
#include <map>
#include <optional>
#include <set>

namespace minera {
namespace {
std::string q(std::string_view value) {return serialize_embedding_workbench_string_json(value);}
std::string counts(const EventTable& table) {
  return "{\"rows\":" + std::to_string(table.rows.size()) + ",\"columns\":" + std::to_string(table.columns.size()) + '}';
}
}
DiscoveryWorkbenchResponse execute_table_timestamp_filter(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source, result;
  std::optional<std::string> before;
  std::optional<bool> unchanged;
  std::string engine, operation, phase = "control", case_key = "case:concept:name",
      attribute_key = "concept:name", time_key = "time:timestamp", error_type, error, filtered;
  bool apply_returned = false;
  int status = 200;
  const auto field = [&](const std::string& name, std::string fallback = {}) {
    const auto found = fields.find(name); return found == fields.end() ? fallback : found->second->bytes;
  };
  const auto boolean = [&](const std::string& name, bool fallback) {
    const auto value = field(name, fallback ? "true" : "false");
    if (value != "true" && value != "false") throw std::invalid_argument(name + " must be true or false");
    return value == "true";
  };
  try {
    if (!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed = {"engine", "operation", "source_file", "lower", "upper",
        "positive", "keep_nan_values", "case_id_key", "attribute_key", "attribute_value_json", "timestamp_key"};
    for (const auto* collection : {&query, &parts}) for (const auto& part : *collection) {
      if (!allowed.contains(part.name)) throw std::invalid_argument("unknown table timestamp control: " + part.name);
      if (!fields.emplace(part.name, &part).second) throw std::invalid_argument("duplicate table timestamp control: " + part.name);
    }
    engine = field("engine");
    if (engine != "pandas" && engine != "polars") {engine.clear(); throw std::invalid_argument("choose pandas or polars timestamp filtering");}
    operation = field("operation");
    static const std::set<std::string> modes = {"events", "contained", "intersecting", "starting", "completing", "attribute"};
    if (!modes.contains(operation)) {operation.clear(); throw std::invalid_argument("choose a supported timestamp filter");}
    const bool boundary = operation == "starting" || operation == "completing", attribute = operation == "attribute";
    if (engine == "polars" && boundary) throw std::invalid_argument("starting/completing timestamp filters require pandas");
    if (fields.contains("positive") && !boundary) throw std::invalid_argument("positive requires starting/completing timestamp filtering");
    if (fields.contains("keep_nan_values") && engine != "pandas") throw std::invalid_argument("keep_nan_values requires pandas");
    if (!attribute && (fields.contains("attribute_key") || fields.contains("attribute_value_json")))
      throw std::invalid_argument("attribute controls require attribute-in-timeframe filtering");
    if (attribute && !fields.contains("attribute_value_json")) throw std::invalid_argument("attribute filtering requires attribute_value_json");
    if (operation == "events" && fields.contains("case_id_key")) throw std::invalid_argument("case_id_key requires case timestamp filtering");
    if (!fields.contains("lower") || !fields.contains("upper")) throw std::invalid_argument("timestamp filtering requires lower and upper endpoints");
    const auto file = fields.find("source_file");
    if (file == fields.end() || !file->second->file) throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for (const auto& part : query) if (part.name == "source_file") throw std::invalid_argument("source_file must be a multipart File");
    case_key = field("case_id_key", case_key); time_key = field("timestamp_key", time_key); attribute_key = field("attribute_key", attribute_key);
    const EventAttribute lower{"", field("lower")}, upper{"", field("upper")}, attr{"", attribute_key};
    const auto target = attribute ? parse_log_attribute_repetition_target_json(field("attribute_value_json")) : EventAttribute{};
    PandasTimestampFilterOptions pandas; PolarsTimestampFilterOptions polars;
    pandas.case_id_key = polars.case_id_key = EventAttribute{"", case_key};
    pandas.timestamp_key = polars.timestamp_key = EventAttribute{"", time_key};
    pandas.positive = parse_case_filter_bound(boolean("positive", true) ? "true" : "false");
    pandas.keep_nan_values = parse_case_filter_bound(boolean("keep_nan_values", false) ? "true" : "false");
    phase = "source_import"; source = parse_embedding_workbench_table_json(file->second->bytes);
    before = serialize_embedding_workbench_table_json(*source); phase = "apply";
    if (engine == "pandas") {
      if (operation == "events") result = filter_pandas_timestamp_events(*source, lower, upper, pandas);
      else if (operation == "contained") result = filter_pandas_timestamp_contained(*source, lower, upper, pandas);
      else if (operation == "intersecting") result = filter_pandas_timestamp_intersecting(*source, lower, upper, pandas);
      else if (operation == "starting") result = filter_pandas_timestamp_starting(*source, lower, upper, pandas);
      else if (operation == "completing") result = filter_pandas_timestamp_completing(*source, lower, upper, pandas);
      else result = filter_pandas_timestamp_attribute(*source, attr, target, lower, upper, pandas);
      apply_returned = true;
    } else {
      const auto lazy = LazyEventTable::from_table(*source);
      const auto selected = operation == "events" ? filter_polars_timestamp_events(lazy, lower, upper, polars) :
          operation == "contained" ? filter_polars_timestamp_contained(lazy, lower, upper, polars) :
          operation == "intersecting" ? filter_polars_timestamp_intersecting(lazy, lower, upper, polars) :
          filter_polars_timestamp_attribute(lazy, attr, target, lower, upper, polars);
      apply_returned = true; phase = "collect"; result = selected.materialize();
    }
    phase = "result_export"; filtered = serialize_embedding_workbench_table_json(*result); phase = "complete";
  } catch (const PandasTimestampFilterError& e) {status = 400; error_type = e.type; error = e.message;
  } catch (const PolarsTimestampFilterError& e) {status = 400; error_type = e.type; error = e.message; phase = e.phase;
  } catch (const std::exception& e) {status = 400; error_type = "native_error"; error = e.what();}
  if (source && before) try {unchanged = *before == serialize_embedding_workbench_table_json(*source);} catch (const std::exception&) {unchanged.reset();}
  const auto text = [&](const std::string& value, bool present) {return present ? q(value) : std::string("null");};
  return {status, "{\"family\":\"TABLE_TIMESTAMP_FILTER\",\"engine\":" + text(engine, !engine.empty()) + ",\"operation\":" + text(operation, !operation.empty()) +
    ",\"http_status\":" + std::to_string(status) + ",\"phase\":" + q(phase) + ",\"apply_returned\":" + (apply_returned ? "true" : "false") +
    ",\"source\":" + (source ? counts(*source) : "null") + ",\"result\":" + (result ? counts(*result) : "null") + ",\"source_unchanged\":" + (unchanged ? (*unchanged ? "true" : "false") : "null") +
    ",\"case_id_key\":" + q(case_key) + ",\"attribute_key\":" + q(attribute_key) + ",\"timestamp_key\":" + q(time_key) +
    ",\"source_table_json\":" + (before ? q(*before) : "null") + ",\"filtered_table_json\":" + text(filtered, !filtered.empty()) + ",\"error_type\":" + text(error_type, !error_type.empty()) + ",\"error\":" + text(error, !error.empty()) + '}'};
}
}
