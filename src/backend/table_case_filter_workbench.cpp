#include "minera/table_case_filter_workbench.hpp"
#include "minera/case_filter_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/polars_case_filter.hpp"
#include <map>
#include <optional>
#include <set>

namespace minera {
namespace {
std::string q(std::string_view text) { return serialize_embedding_workbench_string_json(text); }
std::string counts(const EventTable& table) {
  return "{\"rows\":" + std::to_string(table.rows.size()) + ",\"columns\":" + std::to_string(table.columns.size()) + '}';
}
}
DiscoveryWorkbenchResponse execute_table_case_filter(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source, result;
  std::optional<std::string> before;
  std::optional<bool> unchanged;
  std::string engine, operation, phase = "control", case_key = "case:concept:name", time_key = "time:timestamp";
  std::string error_type, error, filtered;
  bool apply_returned = false;
  int status = 200;
  const auto field = [&](const std::string& name, std::string fallback = {}) {
    const auto at = fields.find(name); return at == fields.end() ? fallback : at->second->bytes;
  };
  try {
    if (!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed = {"engine", "operation", "source_file", "minimum", "maximum", "max_cases", "case_id_key", "timestamp_key", "business_hours"};
    for (const auto* collection : {&query, &parts}) for (const auto& part : *collection) {
      if (!allowed.contains(part.name)) throw std::invalid_argument("unknown table case control: " + part.name);
      if (!fields.emplace(part.name, &part).second) throw std::invalid_argument("duplicate table case control: " + part.name);
    }
    engine = field("engine");
    if (engine != "pandas" && engine != "polars") { engine.clear(); throw std::invalid_argument("choose pandas or polars case filtering"); }
    operation = field("operation");
    if (operation != "size" && operation != "count" && operation != "performance") { operation.clear(); throw std::invalid_argument("choose case size, count or performance filtering"); }
    if (operation == "count" && (fields.contains("minimum") || fields.contains("maximum"))) throw std::invalid_argument("minimum/maximum require size or performance filtering");
    if (operation != "count" && fields.contains("max_cases")) throw std::invalid_argument("max_cases requires count filtering");
    if (operation != "performance" && (fields.contains("timestamp_key") || fields.contains("business_hours"))) throw std::invalid_argument("timestamp_key/business_hours require performance filtering");
    const auto file = fields.find("source_file");
    if (file == fields.end() || !file->second->file) throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for (const auto& part : query) if (part.name == "source_file") throw std::invalid_argument("source_file must be a multipart File");
    const auto minimum = parse_case_filter_bound(field("minimum", operation == "size" ? "2" : "0"));
    const auto maximum = parse_case_filter_bound(field("maximum", operation == "size" ? "null" : "10000000000"));
    const auto limit = parse_case_filter_bound(field("max_cases", "1000"));
    const auto hours = field("business_hours", "false");
    if (hours != "true" && hours != "false") throw std::invalid_argument("business_hours must be true or false");
    case_key = field("case_id_key", case_key); time_key = field("timestamp_key", time_key);
    PandasCaseFilterOptions pandas; PolarsCaseFilterOptions polars;
    pandas.case_id_key = polars.case_id_key = EventAttribute{"", case_key};
    pandas.timestamp_key = polars.timestamp_key = EventAttribute{"", time_key};
    pandas.business_hours = polars.business_hours = parse_case_filter_bound(hours);
    phase = "source_import";
    source = parse_embedding_workbench_table_json(file->second->bytes);
    before = serialize_embedding_workbench_table_json(*source);
    phase = "apply";
    if (engine == "pandas") {
      if (operation == "size") result = filter_pandas_case_size(*source, minimum, maximum, pandas);
      else if (operation == "count") result = filter_pandas_ncases(*source, limit, pandas);
      else result = filter_pandas_case_performance(*source, minimum, maximum, pandas);
      apply_returned = true;
    } else {
      const auto lazy = LazyEventTable::from_table(*source);
      const auto selected = operation == "size" ? filter_polars_case_size(lazy, minimum, maximum, polars) :
          operation == "count" ? filter_polars_ncases(lazy, limit, polars) : filter_polars_case_performance(lazy, minimum, maximum, polars);
      apply_returned = true; phase = "collect"; result = selected.materialize();
    }
    phase = "result_export";
    filtered = serialize_embedding_workbench_table_json(*result); phase = "complete";
  } catch (const PandasCaseFilterError& e) { status = 400; error_type = e.type; error = e.message;
  } catch (const PolarsCaseFilterError& e) { status = 400; error_type = e.type; error = e.message; phase = e.phase;
  } catch (const std::exception& e) { status = 400; error_type = "native_error"; error = e.what(); }
  if (source && before) try { unchanged = *before == serialize_embedding_workbench_table_json(*source); } catch (const std::exception&) { unchanged.reset(); }
  const auto text = [&](const std::string& value, bool present) { return present ? q(value) : std::string("null"); };
  return {status, "{\"family\":\"TABLE_CASE_FILTER\",\"engine\":" + text(engine, !engine.empty()) +
    ",\"operation\":" + text(operation, !operation.empty()) + ",\"http_status\":" + std::to_string(status) + ",\"phase\":" + q(phase) +
    ",\"apply_returned\":" + (apply_returned ? "true" : "false") +
    ",\"source\":" + (source ? counts(*source) : "null") + ",\"result\":" + (result ? counts(*result) : "null") +
    ",\"source_unchanged\":" + (unchanged ? (*unchanged ? "true" : "false") : "null") +
    ",\"case_id_key\":" + q(case_key) + ",\"timestamp_key\":" + q(time_key) +
    ",\"source_table_json\":" + (before ? q(*before) : "null") + ",\"filtered_table_json\":" + text(filtered, !filtered.empty()) +
    ",\"error_type\":" + text(error_type, !error_type.empty()) + ",\"error\":" + text(error, !error.empty()) + '}'};
}
}
