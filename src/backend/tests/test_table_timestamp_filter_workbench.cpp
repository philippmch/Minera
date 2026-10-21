#include "table_timestamp_filter_test_support.hpp"
#include <iostream>
namespace {
using namespace table_timestamp_test;
void invalid_controls() {
  const auto source = timestamp_source("pandas");
  for (const auto& controls : std::vector<std::vector<DiscoveryWorkbenchPart>>{{field("engine", "pandas")}, {field("unknown", "x")},
      {field("positive", "true")}, {field("case_id_key", "ignored")}, {field("attribute_key", "ignored")}, {field("keep_nan_values", "yes")}})
    require(execute_table_timestamp_filter(timestamp_request("pandas", source, "events", controls)).status == 400, "invalid timestamp control accepted");
  require(execute_table_timestamp_filter(timestamp_request("polars", timestamp_source("polars"), "starting")).status == 400, "invented Polars start filter accepted");
  require(execute_table_timestamp_filter(timestamp_request("polars", timestamp_source("polars"), "events", {field("keep_nan_values", "false")})).status == 400, "inactive Polars keep-missing accepted");
  require(execute_table_timestamp_filter(timestamp_request("pandas", source, "attribute")).status == 400, "missing attribute target accepted");
  auto request = timestamp_request("pandas", source); request.back().file = false;
  require(execute_table_timestamp_filter(request).status == 400, "ordinary source field accepted");
  request = timestamp_request("pandas", source); request.erase(request.begin() + 2);
  require(execute_table_timestamp_filter(request).status == 400, "missing lower endpoint accepted");
  require(execute_table_timestamp_filter(timestamp_request("pandas", source), {field("engine", "pandas")}).status == 400, "query/body duplicate accepted");
}
}
int main() {try {timestamp_workflows([](const auto& parts) {return execute_table_timestamp_filter(parts);}); invalid_controls(); std::cout << "complete table timestamp workbench passed\n";}
catch (const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}}
