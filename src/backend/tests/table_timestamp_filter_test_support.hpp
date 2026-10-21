#pragma once
#include <functional>
#include "table_paths_filter_test_support.hpp"
#include "minera/table_timestamp_filter_workbench.hpp"
namespace table_timestamp_test {
using namespace table_paths_test;
inline EventTable timestamp_source(const std::string& engine) {
  auto source = between_source(engine);
  source.dtypes[4] = engine == "polars" ? "Datetime(time_unit='us', time_zone='UTC')" : "datetime64[ns, UTC]";
  for (auto& row : source.rows) {
    auto& cell = *row[4]; cell.value += 'Z';
    cell.typed_value = NativeDate{cell.value, NativeDateResolution::kMicrosecond, "UTC"};
  }
  return source;
}
inline std::vector<DiscoveryWorkbenchPart> timestamp_request(const std::string& engine, const EventTable& source,
    std::string operation = "events", std::vector<DiscoveryWorkbenchPart> controls = {},
    std::string lower = "2026-06-01 00:00:01", std::string upper = "2026-06-01 00:00:03") {
  std::vector<DiscoveryWorkbenchPart> result = {field("engine", engine), field("operation", operation),
    field("lower", lower), field("upper", upper), {"source_file", "table.json", serialize_embedding_workbench_table_json(source), true}};
  result.insert(result.end(), controls.begin(), controls.end()); return result;
}
inline void timestamp_workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run) {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = timestamp_source(engine);
    success(run(timestamp_request(engine, source)), source, subset_rows(source, {1, 2, 3}), engine);
    success(run(timestamp_request(engine, source, "contained", {}, "2026-06-01 00:00:00", "2026-06-01 00:00:02")), source, subset_rows(source, {0, 1, 2}), engine);
    success(run(timestamp_request(engine, source, "intersecting", {}, "2026-06-01 00:00:00", "2026-06-01 00:00:05")), source, source, engine);
    success(run(timestamp_request(engine, source, "attribute", {field("attribute_value_json", R"({"type":"string","value":"A"})")})), source, subset_rows(source, {3, 4}), engine);
    success(run(timestamp_request(engine, source, "events", {}, "2026-06-01 00:00:03", "2026-06-01 00:00:01")), source, subset_rows(source, {}), engine);
    auto custom = source; const std::string case_key = "", time_key(" time\n\0Ω ", 11), attr_key(" selected\n\0Ω ", 15);
    custom.columns[0] = case_key; custom.columns[1] = attr_key; custom.columns[4] = time_key;
    for (auto& row : custom.rows) {row[0]->key = case_key; row[1]->key = attr_key; row[4]->key = time_key;}
    success(run(timestamp_request(engine, custom, "attribute", {field("case_id_key", case_key), field("timestamp_key", time_key), field("attribute_key", attr_key), field("attribute_value_json", R"({"type":"string","value":"A"})")})), custom, subset_rows(custom, {3, 4}), engine);
    auto missing = run(timestamp_request(engine, source, "events", {field("timestamp_key", "missing\n\0")}));
    require(missing.status == 400, "missing timestamp accepted"); contains(missing, "\"source_unchanged\":true");
    require(run(timestamp_request(engine, source, "events", {}, "2026-06-01T00:00:01", "2026-06-01 00:00:03")).status == 400, "unsupported ISO separator accepted");
    if (engine == "pandas") {
      success(run(timestamp_request(engine, source, "starting")), source, subset_rows(source, {3, 4}), engine);
      success(run(timestamp_request(engine, source, "completing")), source, subset_rows(source, {0, 1, 2}), engine);
      success(run(timestamp_request(engine, source, "starting", {field("positive", "false")})), source, subset_rows(source, {0, 1, 2}), engine);
    }
  }
}
}
