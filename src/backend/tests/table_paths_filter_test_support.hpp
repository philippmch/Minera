#pragma once
#include <functional>
#include "table_between_filter_test_support.hpp"
#include "minera/table_paths_filter_workbench.hpp"
namespace table_paths_test {
using namespace table_between_test;
inline std::vector<DiscoveryWorkbenchPart> paths_request(const std::string& engine, const EventTable& source,
    std::string operation = "paths", std::vector<DiscoveryWorkbenchPart> controls = {}, std::string path_json = {}) {
  std::vector<DiscoveryWorkbenchPart> result = {field("engine", engine), field("operation", operation),
    field(operation == "paths" ? "paths_json" : "path_json", path_json.empty() ? (operation == "paths" ? "[[\"A\",\"B\"]]" : "[\"A\",\"B\"]") : path_json),
    {"source_file", "table.json", serialize_embedding_workbench_table_json(source), true}};
  result.insert(result.end(), controls.begin(), controls.end()); return result;
}
inline EventTable subset_rows(EventTable source, std::vector<std::size_t> indices) {
  const auto rows = source.rows; source.rows.clear();
  const auto index = source.index; if (source.index) source.index->labels.clear();
  for (const auto i : indices) {source.rows.push_back(rows[i]); if (source.index) source.index->labels.push_back(index->labels[i]);} return source;
}
inline void paths_workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run) {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = between_source(engine), expected = subset_rows(source, {3, 4});
    success(run(paths_request(engine, source)), source, expected, engine);
    success(run(paths_request(engine, source, "paths", {field("positive", "false")})), source, subset_rows(source, {0, 1, 2}), engine);
    success(run(paths_request(engine, source, "performance", {field("minimum", "1"), field("maximum", "1")})), source, expected, engine);
    success(run(paths_request(engine, source, "paths", {}, "[]")), source, subset_rows(source, {}), engine);
    success(run(paths_request(engine, source, "paths", {field("positive", "false")}, "[]")), source, source, engine);
    auto custom = source; const std::string case_key = "", activity_key(" activity\n\0Ω ", 15), time_key(" time\n\0Ω ", 11);
    custom.columns[0] = case_key; custom.columns[1] = activity_key; custom.columns[4] = time_key;
    for (auto& row : custom.rows) {row[0]->key = case_key; row[1]->key = activity_key; row[4]->key = time_key;}
    const std::vector<DiscoveryWorkbenchPart> roles = {field("case_id_key", case_key), field("activity_key", activity_key), field("timestamp_key", time_key)};
    success(run(paths_request(engine, custom, "paths", roles)), custom, subset_rows(custom, {3, 4}), engine);
    auto missing = run(paths_request(engine, source, "paths", {field("timestamp_key", "missing")}));
    require(missing.status == 400, "plain paths accepted missing timestamp"); contains(missing, "\"source_unchanged\":true");
    contains(missing, engine == "pandas" ? "\"phase\":\"apply\"" : "\"phase\":\"collect\"");
  }
}
}
