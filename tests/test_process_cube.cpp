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
  const auto encoded = serialize_process_cube_json(r);
  assert(encoded.find("\"family\":\"PROCESS_CUBE\"") != std::string::npos);

  threw = false;
  try {
    (void)parse_process_cube_table_json(
        R"({"case_ids":["a"],"columns":{"x":[1,2]}})");
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
