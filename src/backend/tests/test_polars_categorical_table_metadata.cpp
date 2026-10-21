#include "table_between_filter_test_support.hpp"
#include <iostream>

namespace {
using namespace table_between_test;
EventTableCategoricalDtype dictionary(std::vector<std::string> values, bool ordered = false) {
  EventTableCategoricalDtype result; result.categories_dtype = "String"; result.ordered = ordered;
  for (auto& value : values) result.categories.push_back({"", std::move(value)});
  return result;
}
EventTable categorical_source() {
  auto source = between_source("polars");
  source.dtypes[0] = "Categorical(Categories(name=\"case\", namespace=\"native-test\", physical=pl.UInt32))";
  source.dtypes[1] = "Enum(categories=['unused', 'B', 'A', 'X'])";
  source.categorical_dtypes.resize(source.columns.size());
  source.categorical_dtypes[0] = dictionary({std::string(" unused\n\xce\xa9\0 ", 12), "c2", "c1"});
  source.categorical_dtypes[1] = dictionary({"unused", "B", "A", "X"}, true);
  return source;
}
void complete_dictionary_transport() {
  const auto source = categorical_source(); source.validate();
  const auto before = serialize_embedding_workbench_table_json(source);
  const auto reopened = parse_embedding_workbench_table_json(before); reopened.validate();
  require(serialize_embedding_workbench_table_json(reopened) == before, "categorical JSON round trip lost stored fields");
  const auto lazy = LazyEventTable::from_table(reopened);
  require(lazy.categorical_dtypes() == source.categorical_dtypes, "lazy dictionary declarations changed");
  require(lazy.materialize().categorical_dtypes == source.categorical_dtypes, "materialized dictionaries changed");
  const auto expected = direct_between("polars", source);
  require(expected.dtypes[0] == "String" && !expected.categorical_dtypes[0], "rewritten case retained categorical descriptor");
  require(expected.categorical_dtypes[1] == source.categorical_dtypes[1], "Enum activity lost unused categories or order");
  require(expected.rows.size() == 5, "categorical interval rows changed");
  const auto response = execute_table_between_filter(between_request("polars", reopened));
  success(response, source, expected, "polars");
  require(serialize_embedding_workbench_table_json(source) == before, "dictionary extraction mutated source");
}
void invalid_dictionary_carriers() {
  const auto rejected = [](EventTable source) {
    bool invalid = false; try { source.validate(); } catch (const std::invalid_argument&) { invalid = true; }
    require(invalid, "invalid Polars dictionary accepted");
  };
  auto source = categorical_source(); source.dtypes[0] = "String"; rejected(source);
  source = categorical_source(); source.dtypes[0] = "Categorical("; rejected(source);
  source = categorical_source(); source.categorical_dtypes[0]->categories_dtype = "int64"; rejected(source);
  source = categorical_source(); source.categorical_dtypes[0]->categories[0] = integer("", 3); rejected(source);
  source = categorical_source(); source.categorical_dtypes[0]->categories[0].is_null = true; rejected(source);
  source = categorical_source(); source.categorical_dtypes[0]->categories.push_back({"different key", "c1"}); rejected(source);
  source = categorical_source(); source.categorical_dtypes[0]->categories[0].scalar_wrapper = true; rejected(source);
}
}  // namespace
int main() { try { complete_dictionary_transport(); invalid_dictionary_carriers();
  std::cout << "Polars category/Enum dictionaries: validation, lazy storage and complete JSON/workbench transport passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
