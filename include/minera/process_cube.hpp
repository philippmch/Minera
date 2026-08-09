#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <string_view>

namespace minera {

struct ProcessCubeTable {
  std::vector<std::string> case_ids;
  std::map<std::string, std::vector<double>> columns;
  // Compatibility spelling for callers that explicitly say numeric_columns.
  std::map<std::string, std::vector<double>> numeric_columns;
  // Categorical dimensions are represented as one string value per case.
  // They may be supplied either in `columns` (when decoding JSON) or in this
  // explicitly named map for callers that want an unambiguous schema.
  std::map<std::string, std::vector<std::string>> categorical_columns;
  // Compatibility spelling for callers that use the shorter string name.
  std::map<std::string, std::vector<std::string>> string_columns;
};

enum class ProcessCubeAggregation {
  kMean, kSum, kMin, kMax, kCount, kMedian, kFirst, kLast,
  MEAN = kMean, SUM = kSum, MIN = kMin, MAX = kMax, COUNT = kCount,
  MEDIAN = kMedian, FIRST = kFirst, LAST = kLast,
};
using ProcessCubeAggregationFunction = ProcessCubeAggregation;

struct ProcessCubeOptions {
  // x_dimension/y_dimension name either a numeric column or a prefix group.
  std::string x_dimension;
  std::string y_dimension;
  // Short column spellings are accepted as equivalent aliases.
  std::string x_col;
  std::string y_col;
  std::string aggregation_column;
  std::string value_column;
  // A one-element vector is equivalent to x_col/y_col. Multiple names form a
  // deterministic composite categorical dimension.
  std::vector<std::string> x_columns;
  std::vector<std::string> y_columns;
  ProcessCubeAggregation aggregation = ProcessCubeAggregation::kMean;
  std::size_t max_divisions_x = 4;
  std::size_t max_divisions_y = 4;
  std::vector<double> x_bins;
  std::vector<double> y_bins;
};

struct ProcessCubeBin {
  std::string label;
  double lower = 0.0;
  double upper = 0.0;
  bool numeric = true;
};

struct ProcessCubeResult {
  std::string x_dimension;
  std::string y_dimension;
  std::string aggregation;
  std::vector<ProcessCubeBin> x_bins;
  std::vector<ProcessCubeBin> y_bins;
  std::vector<double> x_edges;
  std::vector<double> y_edges;
  std::vector<std::vector<std::optional<double>>> cells;
  std::vector<std::vector<std::set<std::string>>> case_ids;
  std::vector<std::vector<std::optional<double>>> pivot;
  std::vector<std::vector<std::set<std::string>>> cell_case_ids;
};

[[nodiscard]] ProcessCubeResult process_cube(
    const ProcessCubeTable& table, const ProcessCubeOptions& options = {});

[[nodiscard]] ProcessCubeTable parse_process_cube_table_json(
    std::string_view json);
[[nodiscard]] std::string serialize_process_cube_json(
    const ProcessCubeResult& result);
[[nodiscard]] inline ProcessCubeResult compute_process_cube(
    const ProcessCubeTable& table, const ProcessCubeOptions& options = {}) {
  return process_cube(table, options);
}
[[nodiscard]] inline ProcessCubeResult discover_process_cube(
    const ProcessCubeTable& table, const ProcessCubeOptions& options = {}) {
  return process_cube(table, options);
}

}  // namespace minera
