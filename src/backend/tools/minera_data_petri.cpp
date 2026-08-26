#include <fstream>
#include <iostream>
#include <exception>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/data_petri_net.hpp"
#include "minera/pnml.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " MODEL.pnml INPUT.csv [OUTPUT.json] [options]\n"
               "  --max-depth N             Decision tree depth (0..64)\n"
               "  --min-split N             Minimum observations before a split\n"
               "  --min-leaf N              Minimum observations per child\n"
               "  --max-categories N        Equality candidates per key\n"
               "  --max-thresholds N        Numeric threshold candidates\n"
               "  --min-gain F              Minimum Gini gain\n"
               "  --no-trace-attributes     Exclude case attributes\n"
               "  --attributes A,B,...      Restrict context keys\n";
  return 2;
}

std::size_t parse_size(std::string_view value, std::string_view name,
                       bool allow_zero) {
  if (value.empty() || value.front() == '-')
    throw std::invalid_argument(std::string(name) + " expects a non-negative integer");
  char* end = nullptr;
  const std::string copy(value);
  const auto parsed = std::strtoull(copy.c_str(), &end, 10);
  if (end != copy.c_str() + copy.size() ||
      parsed > std::numeric_limits<std::size_t>::max() ||
      (!allow_zero && parsed == 0))
    throw std::invalid_argument(std::string(name) + " expects a valid integer");
  return static_cast<std::size_t>(parsed);
}

double parse_number(std::string_view value, std::string_view name) {
  char* end = nullptr;
  const std::string copy(value);
  const double parsed = std::strtod(copy.c_str(), &end);
  if (end != copy.c_str() + copy.size() || !std::isfinite(parsed))
    throw std::invalid_argument(std::string(name) + " expects a finite number");
  return parsed;
}

std::vector<std::string> split_names(std::string_view value) {
  std::vector<std::string> names;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const auto end = value.find(',', begin);
    const auto part = value.substr(begin, end == std::string_view::npos
                                            ? value.size() - begin
                                            : end - begin);
    if (part.empty())
      throw std::invalid_argument("--attributes expects non-empty names");
    names.emplace_back(part);
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  return names;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3) return usage(argv[0]);
    const auto net = minera::import_pnml(argv[1]);
    const auto log = minera::import_csv_event_log(argv[2]);
    std::string output_path;
    minera::DataPetriNetOptions options;
    for (int i = 3; i < argc; ++i) {
      const std::string_view argument(argv[i]);
      const auto next = [&]() -> std::string_view {
        if (++i >= argc || std::string_view(argv[i]).empty())
          throw std::invalid_argument(std::string(argument) + " expects a value");
        return argv[i];
      };
      if (argument == "--no-trace-attributes") {
        options.decision_options.include_trace_attributes = false;
      } else if (argument == "--attributes") {
        options.decision_options.attributes = split_names(next());
      } else if (argument == "--max-depth" || argument == "--min-split" ||
                 argument == "--min-leaf" || argument == "--max-categories" ||
                 argument == "--max-thresholds") {
        const auto value = next();
        const bool allow_zero = argument == "--max-depth";
        const auto parsed = parse_size(value, argument, allow_zero);
        if (argument == "--max-depth") {
          if (parsed > 64) throw std::invalid_argument("--max-depth expects 0 through 64");
          options.decision_options.max_depth = parsed;
        } else if (argument == "--min-split") {
          options.decision_options.min_split = parsed;
        } else if (argument == "--min-leaf") {
          options.decision_options.min_leaf = parsed;
        } else if (argument == "--max-categories") {
          options.decision_options.max_categories = parsed;
        } else {
          options.decision_options.max_thresholds = parsed;
        }
      } else if (argument == "--min-gain") {
        options.decision_options.min_gain = parse_number(next(), argument);
        if (options.decision_options.min_gain < 0.0)
          throw std::invalid_argument("--min-gain expects a non-negative number");
      } else if (output_path.empty() && !argument.empty() &&
                 argument.front() != '-') {
        output_path = std::string(argument);
      } else {
        return usage(argv[0]);
      }
    }
    const auto result = minera::mine_data_petri_net(log, net, options);
    const auto payload = minera::serialize_data_petri_net_json(result) + '\n';
    if (output_path.empty()) {
      std::cout << payload;
    } else {
      std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
      if (!output) throw std::runtime_error("cannot open output file");
      output << payload;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
