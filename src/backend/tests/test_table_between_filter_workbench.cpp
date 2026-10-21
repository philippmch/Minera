#include "table_between_filter_test_support.hpp"
#include <iostream>

namespace {
using namespace table_between_test;
void complete_between_workflows() {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = between_source(engine), expected = direct_between(engine, source);
    require(expected.rows.size() == 5, "complete example intervals were lost");
    success(execute_table_between_filter(between_request(engine, source)), source, expected, engine);
    auto literal = source;
    const std::string key(" activity\n\0", 11);
    literal.columns[0] = ""; literal.columns[1] = key;
    for (auto& row : literal.rows) { row[0]->key = ""; row[1]->key = key; }
    const auto literal_expected = direct_between(engine, literal, "", key, "");
    success(execute_table_between_filter(between_request(engine, literal,
      {field("case_id_key", ""), field("activity_key", key), field("subcase_concat_str", "")})), literal, literal_expected, engine);
    auto parts = between_request(engine, source); parts[1].bytes = "[]";
    // The direct native helper establishes engine-specific schema changes even
    // for an empty selection; the transport must preserve that complete result.
    EventAttribute no_start{"", ""}; no_start.container = AttributeContainerKind::kList;
    const auto no_result = engine == "pandas" ? filter_pandas_between(source, no_start, EventAttribute{"", "B"})
                                               : filter_polars_between(source, no_start, EventAttribute{"", "B"});
    require(no_result.rows.empty(), "empty start collection produced intervals");
    success(execute_table_between_filter(parts), source, no_result, engine);
  }
}
void controls_and_engine_errors() {
  for (const std::string engine : {"pandas", "polars"}) {
    const auto source = between_source(engine);
    auto parts = between_request(engine, source); parts[1].bytes = "[\"A\",1]";
    auto failed = execute_table_between_filter(parts); require(failed.status == 400, "nonstring endpoint accepted"); contains(failed, "\"phase\":\"control\"");
    failed = execute_table_between_filter(between_request(engine, source), {field("engine", engine)});
    require(failed.status == 400, "duplicate controls accepted");
    failed = execute_table_between_filter(between_request(engine, source, {field("activity_key", "missing\n")}));
    require(failed.status == 400, "missing activity accepted"); contains(failed, "\"source_unchanged\":true");
    require(text(failed.json, "source_table_json") == serialize_embedding_workbench_table_json(source), "engine error lost complete source");
    parts = between_request(engine, source); parts[3].bytes = "{invalid";
    failed = execute_table_between_filter(parts); require(failed.status == 400, "invalid table accepted"); contains(failed, "\"phase\":\"source_import\"");
    require(!text(failed.json, "source_table_json"), "invalid source artifact exposed");
  }
  auto source = between_source("polars"); source.columns.pop_back(); source.dtypes.pop_back(); for (auto& row : source.rows) row.pop_back();
  const auto failed = execute_table_between_filter(between_request("polars", source));
  require(failed.status == 400, "missing canonical timestamp accepted"); contains(failed, "\"phase\":\"collect\"");
  contains(failed, "time:timestamp"); contains(failed, "\"apply_returned\":true");
}
}  // namespace
int main() { try { complete_between_workflows(); controls_and_engine_errors(); std::cout << "complete typed between table workflows passed\n"; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
