#pragma once
#include "table_attribute_repetition_test_support.hpp"
#include "minera/table_between_filter_workbench.hpp"
#include "minera/pandas_between_filter.hpp"
#include "minera/polars_between_filter.hpp"

namespace table_between_test {
using namespace table_repetition_test;
inline EventTable between_source(const std::string& engine) {
  auto table = source_table(engine);
  table.columns.push_back("time:timestamp");
  table.dtypes[1] = engine == "polars" ? "String" : "object";
  table.dtypes.push_back(engine == "polars" ? "Datetime(time_unit='us', time_zone=None)" : "datetime64[ns]");
  table.categorical_dtypes.clear(); table.rows.clear();
  if (table.index) table.index->labels.clear();
  const std::vector<std::pair<std::string, std::string>> sequence = {{"c1","A"},{"c1","X"},{"c1","B"},{"c2","A"},{"c2","B"}};
  for (std::size_t i = 0; i < sequence.size(); ++i) {
    const auto time = "2026-06-01T00:00:0" + std::to_string(i);
    EventAttribute timestamp{"time:timestamp", time, AttributeKind::kDate}; timestamp.typed_value = NativeDate{time};
    table.rows.push_back({EventAttribute{table.columns[0], sequence[i].first}, EventAttribute{table.columns[1], sequence[i].second},
      integer("signed", 9007199254740993LL), unsigned_integer("unsigned", 18446744073709551615ULL), timestamp});
    if (table.index) table.index->labels.push_back({integer("", static_cast<std::int64_t>(10+i))});
  }
  return table;
}
inline std::vector<DiscoveryWorkbenchPart> between_request(const std::string& engine, const EventTable& source,
    std::vector<DiscoveryWorkbenchPart> controls = {}) {
  std::vector<DiscoveryWorkbenchPart> parts = {field("engine", engine), field("act1_json", "\"A\""), field("act2_json", "\"B\""),
    {"source_file", "table.json", serialize_embedding_workbench_table_json(source), true}};
  parts.insert(parts.end(), controls.begin(), controls.end()); return parts;
}
inline EventTable direct_between(const std::string& engine, const EventTable& source,
    const std::string& case_key = "case:concept:name", const std::string& activity_key = "concept:name", const std::string& separator = "##@@") {
  if (engine == "pandas") {
    PandasBetweenOptions options; options.case_id_key = {"", case_key}; options.activity_key = {"", activity_key}; options.subcase_concat_str = {"", separator};
    return filter_pandas_between(source, EventAttribute{"", "A"}, EventAttribute{"", "B"}, options);
  }
  PolarsBetweenOptions options; options.case_id_key = {"", case_key}; options.activity_key = {"", activity_key}; options.subcase_concat_str = {"", separator};
  return filter_polars_between(source, EventAttribute{"", "A"}, EventAttribute{"", "B"}, options);
}
}  // namespace table_between_test
