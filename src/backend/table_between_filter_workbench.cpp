#include "minera/table_between_filter_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/pandas_between_filter.hpp"
#include "minera/polars_between_filter.hpp"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace minera {
namespace {
std::string q(std::string_view text) { return serialize_embedding_workbench_string_json(text); }
std::string counts(const EventTable& table) {
  return "{\"rows\":" + std::to_string(table.rows.size()) +
         ",\"columns\":" + std::to_string(table.columns.size()) + '}';
}
EventAttribute endpoint(std::string_view raw, std::string_view name) {
  const auto first = raw.find_first_not_of(" \t\r\n");
  if (first != std::string_view::npos && raw[first] == '"') {
    const auto values = parse_literal_string_vector_json("[" + std::string(raw) + "]");
    if (values.size() == 1) return EventAttribute{"", values.front()};
  } else if (first != std::string_view::npos && raw[first] == '[') {
    const auto values = parse_literal_string_vector_json(raw);
    EventAttribute result{"", ""};
    result.container = AttributeContainerKind::kList;
    for (std::size_t i = 0; i < values.size(); ++i)
      result.children.push_back({std::to_string(i), values[i]});
    return result;
  }
  throw std::invalid_argument(std::string(name) + " must be a JSON string or array of strings");
}
}  // namespace

DiscoveryWorkbenchResponse execute_table_between_filter(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query, std::string_view transport_error) {
  std::map<std::string, const DiscoveryWorkbenchPart*> fields;
  std::optional<EventTable> source, result;
  std::optional<std::string> source_before;
  std::optional<bool> unchanged;
  std::string engine, phase = "control", case_key = "case:concept:name", activity_key = "concept:name";
  std::string error_type, error, filtered;
  bool apply_returned = false;
  int status = 200;
  const auto field = [&](const std::string& name, std::string fallback = {}) {
    const auto at = fields.find(name);
    return at == fields.end() ? fallback : at->second->bytes;
  };
  try {
    if (!transport_error.empty()) throw std::invalid_argument(std::string(transport_error));
    static const std::set<std::string> allowed = {"engine", "source_file", "act1_json", "act2_json",
                                                "case_id_key", "activity_key", "subcase_concat_str"};
    for (const auto* collection : {&query, &parts}) for (const auto& part : *collection) {
      if (!allowed.contains(part.name)) throw std::invalid_argument("unknown table between control: " + part.name);
      if (!fields.emplace(part.name, &part).second) throw std::invalid_argument("duplicate table between control: " + part.name);
    }
    engine = field("engine");
    if (engine != "pandas" && engine != "polars") {
      engine.clear(); throw std::invalid_argument("choose pandas or polars table between filtering");
    }
    if (!fields.contains("act1_json") || !fields.contains("act2_json"))
      throw std::invalid_argument("table between filtering requires act1_json and act2_json");
    const auto found = fields.find("source_file");
    if (found == fields.end() || !found->second->file)
      throw std::invalid_argument("source_file requires an uploaded native EventTable JSON File");
    for (const auto& part : query) if (part.name == "source_file")
      throw std::invalid_argument("source_file must be a multipart File");
    const auto act1 = endpoint(field("act1_json"), "act1_json");
    const auto act2 = endpoint(field("act2_json"), "act2_json");
    case_key = field("case_id_key", case_key); activity_key = field("activity_key", activity_key);
    PandasBetweenOptions pandas; PolarsBetweenOptions polars;
    pandas.case_id_key = polars.case_id_key = EventAttribute{"", case_key};
    pandas.activity_key = polars.activity_key = EventAttribute{"", activity_key};
    if (fields.contains("subcase_concat_str"))
      pandas.subcase_concat_str = polars.subcase_concat_str = EventAttribute{"", field("subcase_concat_str")};
    phase = "source_import";
    source = parse_embedding_workbench_table_json(found->second->bytes);
    source_before = serialize_embedding_workbench_table_json(*source);
    phase = "apply";
    if (engine == "pandas") {
      result = filter_pandas_between(*source, act1, act2, pandas);
      apply_returned = true;
    } else {
      const auto lazy = LazyEventTable::from_table(*source);
      const auto filtered_lazy = filter_polars_between(lazy, act1, act2, polars);
      apply_returned = true;
      phase = "collect";
      result = filtered_lazy.materialize();
    }
    unchanged = *source_before == serialize_embedding_workbench_table_json(*source);
    phase = "result_export";
    filtered = serialize_embedding_workbench_table_json(*result);
    phase = "complete";
  } catch (const PandasBetweenError& e) {
    status = 400; error_type = e.type; error = e.message;
  } catch (const PolarsBetweenError& e) {
    status = 400; error_type = e.type; error = e.message; phase = e.phase;
  } catch (const std::exception& e) {
    status = 400; error_type = "native_error"; error = e.what();
  }
  if (source && source_before) try { unchanged = *source_before == serialize_embedding_workbench_table_json(*source); }
    catch (const std::exception&) { unchanged.reset(); }
  const auto text = [&](const std::string& value, bool present) { return present ? q(value) : std::string("null"); };
  const std::string json = "{\"family\":\"TABLE_BETWEEN_FILTER\",\"engine\":" + text(engine, !engine.empty()) +
    ",\"http_status\":" + std::to_string(status) + ",\"phase\":" + q(phase) +
    ",\"apply_returned\":" + (apply_returned ? "true" : "false") +
    ",\"source\":" + (source ? counts(*source) : "null") + ",\"result\":" + (result ? counts(*result) : "null") +
    ",\"source_unchanged\":" + (unchanged ? (*unchanged ? "true" : "false") : "null") +
    ",\"case_id_key\":" + q(case_key) + ",\"activity_key\":" + q(activity_key) +
    ",\"source_table_json\":" + (source_before ? q(*source_before) : "null") +
    ",\"filtered_table_json\":" + text(filtered, !filtered.empty()) +
    ",\"error_type\":" + text(error_type, !error_type.empty()) + ",\"error\":" + text(error, !error.empty()) + '}';
  return {status, json};
}
}  // namespace minera
