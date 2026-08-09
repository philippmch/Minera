#include "minera/process_cube.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace minera;

int main() {
  ProcessCubeTable t{{"a", "b", "c"}, {{"x", {0, 1, 2}}, {"y", {0, 1, 2}}, {"v", {2, 4, 8}}}};
  ProcessCubeOptions o; o.x_dimension = "x"; o.y_dimension = "y"; o.aggregation_column = "v";
  o.max_divisions_x = o.max_divisions_y = 2;
  auto r = process_cube(t, o);
  assert(r.x_bins.size() == 2 && r.y_bins.size() == 2);
  assert(r.cells[0][0].has_value() && std::abs(*r.cells[0][0] - 2.0) < 1e-9);
  assert(r.case_ids[1][1].count("c") == 1);

  o.aggregation = ProcessCubeAggregation::kCount;
  r = process_cube(t, o);
  assert(r.cells[0][0].has_value() && *r.cells[0][0] == 1.0);

  ProcessCubeTable p{{"a", "b"}, {{"x_A", {1, 0}}, {"x_B", {1, 1}}, {"y_Q", {1, 1}}, {"v", {3, 5}}}};
  o = {}; o.x_dimension = "x"; o.y_dimension = "y"; o.aggregation_column = "v";
  r = process_cube(p, o);
  assert(r.x_bins.size() == 2 && r.case_ids[0][0].count("a") == 1 && r.case_ids[1][0].count("b") == 1);

  bool threw = false;
  try { ProcessCubeOptions bad = o; bad.max_divisions_x = 0; (void)process_cube(p, bad); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  // Explicit edges are retained even when they exceed the automatic division
  // limit, and values outside the configured range are omitted from cells.
  o = {};
  o.x_dimension = "x";
  o.y_dimension = "y";
  o.aggregation_column = "v";
  o.max_divisions_x = o.max_divisions_y = 1;
  o.x_bins = {-1.0, 0.5, 1.5, 3.0};
  o.y_bins = {-1.0, 3.0};
  r = process_cube(t, o);
  assert(r.x_bins.size() == 3 && r.x_edges.size() == 4);
  assert(r.cells[0][0].has_value() && *r.cells[0][0] == 2.0);
  assert(r.cells[1][0].has_value() && *r.cells[1][0] == 4.0);
  assert(r.cells[2][0].has_value() && *r.cells[2][0] == 8.0);

  const auto round_trip = parse_process_cube_table_json(
      R"({"case_ids":["a","b"],"columns":{"x":[1,2],"y":[3,4],"v":[5,6]}})");
  assert(round_trip.case_ids.size() == 2 && round_trip.columns.at("v")[1] == 6.0);
  const auto unicode = parse_process_cube_table_json(
      R"({"case_ids":["caf\u00e9"],"columns":{"x":[1],"y":[1],"v":[1]}})");
  assert(unicode.case_ids.front() == "caf\xC3\xA9");

  // Existing categorical columns form deterministic bins, while a tuple of
  // categorical columns forms one composite bin per observed combination.
  ProcessCubeTable categorical;
  categorical.case_ids = {"c1", "c2", "c3"};
  categorical.categorical_columns = {
      {"region", {"US", "DE", "US"}},
      {"segment", {"retail", "retail", "online"}},
  };
  categorical.columns = {{"value", {1.0, 3.0, 5.0}}, {"score", {0.0, 1.0, 2.0}}};
  ProcessCubeOptions category_options;
  category_options.x_col = "region";
  category_options.y_col = "segment";
  category_options.aggregation_column = "value";
  auto category_result = process_cube(categorical, category_options);
  assert(category_result.x_bins.size() == 2 && !category_result.x_bins[0].numeric);
  assert(category_result.x_bins[0].label == "DE" && category_result.x_bins[1].label == "US");
  assert(category_result.cells[0][1].has_value() && *category_result.cells[0][1] == 3.0);
  assert(category_result.cells[1][0].has_value() && *category_result.cells[1][0] == 5.0);

  category_options = {};
  category_options.x_columns = {"region", "segment"};
  category_options.y_col = "score";
  category_options.aggregation_column = "value";
  category_result = process_cube(categorical, category_options);
  assert(category_result.x_bins.size() == 3);
  assert(category_result.x_bins[0].label == "(DE,retail)");
  assert(category_result.case_ids[0][2].count("c2") == 1);

  const auto categorical_json = parse_process_cube_table_json(
      R"({"case_ids":["a","b"],"columns":{"region":["US","DE"],"value":[2,4]}})");
  assert(categorical_json.categorical_columns.at("region")[1] == "DE");

  threw = false;
  try {
    auto invalid = categorical;
    invalid.columns.emplace("region", std::vector<double>{1, 2, 3});
    (void)process_cube(invalid, category_options);
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  const auto encoded = serialize_process_cube_json(r);
  assert(encoded.find("\"family\":\"PROCESS_CUBE\"") != std::string::npos);

  threw = false;
  try {
    (void)parse_process_cube_table_json(
        R"({"case_ids":["a"],"columns":{"x":[1,2]}})");
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
