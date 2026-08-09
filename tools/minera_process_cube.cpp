#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/process_cube.hpp"

namespace {

std::size_t parse_size(std::string_view text, std::string_view option) {
  if (text.empty() || text.front() == '-')
    throw std::invalid_argument(std::string(option) + " expects a non-negative integer");
  char* end = nullptr;
  const std::string value(text);
  const auto parsed = std::strtoull(value.c_str(), &end, 10);
  if (end != value.c_str() + value.size() ||
      parsed > std::numeric_limits<std::size_t>::max() || parsed == 0 || parsed > 10000)
    throw std::invalid_argument(std::string(option) + " expects an integer from 1 through 10000");
  return static_cast<std::size_t>(parsed);
}

std::vector<double> parse_bins(std::string_view text, std::string_view option) {
  std::vector<double> result;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const auto end = text.find(',', begin);
    const auto token = text.substr(begin, end == std::string_view::npos
                                             ? std::string_view::npos
                                             : end - begin);
    if (token.empty())
      throw std::invalid_argument(std::string(option) + " contains an empty edge");
    const std::string value(token);
    char* parsed_end = nullptr;
    const double parsed = std::strtod(value.c_str(), &parsed_end);
    if (parsed_end != value.c_str() + value.size() || !std::isfinite(parsed))
      throw std::invalid_argument(std::string(option) + " expects finite numeric edges");
    result.push_back(parsed);
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  if (result.size() < 2)
    throw std::invalid_argument(std::string(option) + " expects at least two edges");
  return result;
}

std::vector<std::string> parse_names(std::string_view text, std::string_view option) {
  std::vector<std::string> result;
  std::string current;
  bool escaped = false;
  for (const char value : text) {
    if (escaped) {
      if (value != '\\' && value != ',')
        throw std::invalid_argument(std::string(option) + " supports only \\, and \\\\ escapes");
      current.push_back(value);
      escaped = false;
    } else if (value == '\\') {
      escaped = true;
    } else if (value == ',') {
      if (current.empty()) throw std::invalid_argument(std::string(option) + " contains an empty name");
      result.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(value);
    }
  }
  if (escaped) throw std::invalid_argument(std::string(option) + " has a dangling escape");
  if (current.empty()) throw std::invalid_argument(std::string(option) + " contains an empty name");
  result.push_back(std::move(current));
  return result;
}

minera::ProcessCubeAggregation parse_aggregation(std::string_view value) {
  if (value == "mean") return minera::ProcessCubeAggregation::kMean;
  if (value == "sum") return minera::ProcessCubeAggregation::kSum;
  if (value == "min") return minera::ProcessCubeAggregation::kMin;
  if (value == "max") return minera::ProcessCubeAggregation::kMax;
  if (value == "count") return minera::ProcessCubeAggregation::kCount;
  if (value == "median") return minera::ProcessCubeAggregation::kMedian;
  if (value == "first") return minera::ProcessCubeAggregation::kFirst;
  if (value == "last") return minera::ProcessCubeAggregation::kLast;
  throw std::invalid_argument(
      "--aggregation expects mean, sum, min, max, count, median, first, or last");
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " TABLE.json (--x-col NAME | --x-cols A,B,...)"
               " (--y-col NAME | --y-cols A,B,...) --agg-col NAME"
               " [--aggregation mean|sum|min|max|count|median|first|last]"
               " [--max-divisions-x N] [--max-divisions-y N]"
               " [--x-bins A,B,...] [--y-bins A,B,...]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) return usage(argv[0]);
    const std::string input_path = argv[1];
    minera::ProcessCubeOptions options;
    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if (i + 1 >= argc) return usage(argv[0]);
      const std::string value = argv[++i];
      if (option == "--x-col") options.x_col = value;
      else if (option == "--x-cols") options.x_columns = parse_names(value, option);
      else if (option == "--y-col") options.y_col = value;
      else if (option == "--y-cols") options.y_columns = parse_names(value, option);
      else if (option == "--agg-col") options.aggregation_column = value;
      else if (option == "--aggregation") options.aggregation = parse_aggregation(value);
      else if (option == "--max-divisions-x") options.max_divisions_x = parse_size(value, option);
      else if (option == "--max-divisions-y") options.max_divisions_y = parse_size(value, option);
      else if (option == "--x-bins") options.x_bins = parse_bins(value, option);
      else if (option == "--y-bins") options.y_bins = parse_bins(value, option);
      else return usage(argv[0]);
    }
    std::ifstream input(input_path, std::ios::binary);
    if (!input) throw std::invalid_argument("could not open process cube table: " + input_path);
    std::ostringstream content;
    content << input.rdbuf();
    const auto table = minera::parse_process_cube_table_json(content.str());
    std::cout << minera::serialize_process_cube_json(
        minera::process_cube(table, options)) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
