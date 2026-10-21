#pragma once
#include "table_between_filter_test_support.hpp"
#include "minera/table_case_filter_workbench.hpp"
namespace table_case_test {
using namespace table_between_test;
inline std::vector<DiscoveryWorkbenchPart> case_request(const std::string& engine, const EventTable& source,
    std::string operation = "size", std::vector<DiscoveryWorkbenchPart> controls = {}) {
  std::vector<DiscoveryWorkbenchPart> result = {field("engine", engine), field("operation", operation),
    {"source_file", "table.json", serialize_embedding_workbench_table_json(source), true}};
  result.insert(result.end(), controls.begin(), controls.end()); return result;
}
inline EventTable first_case(EventTable source) {
  source.rows.resize(3); if (source.index) source.index->labels.resize(3); return source;
}
inline void basic_case_workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run) {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = between_source(engine), expected = first_case(source);
    for (const std::string operation : {"size", "count", "performance"}) {
      const auto controls = operation == "count" ? std::vector{field("max_cases", "1")} :
          operation == "size" ? std::vector{field("minimum", "3"), field("maximum", "3")} : std::vector{field("minimum", "2"), field("maximum", "2")};
      success(run(case_request(engine, source, operation, controls)), source, expected, engine);
    }
    auto custom = source; const std::string case_key(" case\n\0Ω ", 11), time_key(" time\n\0Ω ", 11);
    custom.columns[0] = case_key; custom.columns[4] = time_key;
    for (auto& row : custom.rows) {row[0]->key = case_key; row[4]->key = time_key;}
    success(run(case_request(engine, custom, "performance", {field("case_id_key", case_key), field("timestamp_key", time_key), field("minimum", "2"), field("maximum", "2")})), custom, first_case(custom), engine);
    auto missing = run(case_request(engine, source, "size", {field("case_id_key", "missing")}));
    require(missing.status == 400, "missing case accepted"); contains(missing, "\"source_unchanged\":true");
    require(text(missing.json, "filtered_table_json") == std::nullopt, "failure has result artifact");
  }
}
}
