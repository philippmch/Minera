#include <functional>
#include "table_case_filter_test_support.hpp"
#include <iostream>
namespace {
using namespace table_case_test;
void validation() {
  const auto source = between_source("pandas");
  for (const auto& controls : std::vector<std::vector<DiscoveryWorkbenchPart>>{
      {field("engine", "pandas")}, {field("unknown", "x")}, {field("timestamp_key", "unused")}, {field("max_cases", "1")}, {field("business_hours", "false")}}) {
    const auto result = execute_table_case_filter(case_request("pandas", source, "size", controls));
    require(result.status == 400, "inapplicable or duplicate table controls accepted"); contains(result, "\"phase\":\"control\"");
  }
  auto request = case_request("pandas", source); request.back().file = false;
  require(execute_table_case_filter(request).status == 400, "ordinary source field accepted");
  request = case_request("pandas", source); const auto duplicate = execute_table_case_filter(request, {field("engine", "pandas")});
  require(duplicate.status == 400, "query/body duplicate accepted");
  require(execute_table_case_filter(case_request("other", source)).status == 400, "unknown engine accepted");
  require(execute_table_case_filter(case_request("pandas", source, "other")).status == 400, "unknown operation accepted");
  auto result = execute_table_case_filter(case_request("polars", between_source("polars"), "performance", {field("business_hours", "true")}));
  require(result.status == 400, "unsupported Polars business hours accepted"); contains(result, "\"phase\":\"apply\""); contains(result, "\"apply_returned\":false");
}
}
int main() {try {basic_case_workflows([](const auto& parts) {return execute_table_case_filter(parts);}); validation(); std::cout << "complete case table workbench passed\n";}
  catch (const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}}
