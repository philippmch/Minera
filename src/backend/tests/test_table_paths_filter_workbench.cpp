#include "table_paths_filter_test_support.hpp"
#include "minera/paths_filter_controls.hpp"
#include <iostream>
namespace {
using namespace table_paths_test;
void invalid_controls_and_pairs() {
  const auto source = between_source("pandas");
  for (const auto& raw : {"[[]]", "[[\"A\"]]", "[[\"A\",\"B\",\"C\"]]", "[[\"A\",1]]", "[[\"A\",\"B\"],]", "[[\"A\",\"B\"]] trailing"})
    require(execute_table_paths_filter(paths_request("pandas", source, "paths", {}, raw)).status == 400, "malformed path accepted");
  const auto pair = parse_paths_filter_pair_json("[\" A,B \",\"\\u0000\\nΩ\"]"); require(pair.container == AttributeContainerKind::kTuple && pair.children[0].value == " A,B " && pair.children[1].value == std::string("\0\nΩ", 4), "pair labels normalized");
  for (const auto& controls : std::vector<std::vector<DiscoveryWorkbenchPart>>{{field("engine", "pandas")}, {field("unknown", "x")}, {field("minimum", "0")}, {field("positive", "yes")}, {field("path_json", "[]")}})
    require(execute_table_paths_filter(paths_request("pandas", source, "paths", controls)).status == 400, "invalid table control accepted");
  require(execute_table_paths_filter(paths_request("pandas", source, "performance", {field("target_attribute_key", "ignored")})).status == 400, "inert performance target accepted as active control");
  auto request = paths_request("pandas", source); request[3].file = false;
  require(execute_table_paths_filter(request).status == 400, "ordinary source field accepted");
  require(execute_table_paths_filter(paths_request("pandas", source), {field("engine", "pandas")}).status == 400, "query/body duplicate accepted");
}
}
int main() {try {paths_workflows([](const auto& parts) {return execute_table_paths_filter(parts);}); invalid_controls_and_pairs(); std::cout << "complete table paths workbench passed\n";}
catch (const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}}
