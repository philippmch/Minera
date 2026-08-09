#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/dfg_conversion.hpp"
#include "minera/dfg_filter.hpp"
#include "minera/dfg_io.hpp"
#include "minera/log_filter.hpp"

namespace {

double parse_number(std::string_view text, std::string_view option) {
  const std::string owned(text);
  char* end = nullptr;
  const double value = std::strtod(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size() || !std::isfinite(value)) {
    throw std::invalid_argument(std::string(option) +
                                " expects a finite number");
  }
  return value;
}

std::set<std::pair<std::string, std::string>> parse_paths(
    std::string_view text) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& pair : minera::parse_activity_pairs(text))
    result.emplace(pair.predecessor, pair.successor);
  return result;
}

minera::DfgFilterData to_filter_data(const minera::DfgCleanAnalysis& input) {
  minera::DfgFilterData result;
  result.edges = input.graph;
  result.start_activities = input.start_activities;
  result.end_activities = input.end_activities;
  for (const auto& [edge, count] : input.graph) {
    (void)count;
    result.activities.try_emplace(edge.first, 1);
    result.activities.try_emplace(edge.second, 1);
  }
  for (const auto& [activity, count] : input.start_activities) {
    (void)count;
    result.activities.try_emplace(activity, 1);
  }
  for (const auto& [activity, count] : input.end_activities) {
    (void)count;
    result.activities.try_emplace(activity, 1);
  }
  return result;
}

minera::DfgCleanAnalysis to_clean_analysis(const minera::DfgFilterData& input) {
  return {input.edges, input.start_activities, input.end_activities};
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc >= 2 && std::string_view(argv[1]) == "--to-petri") {
      if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0]
                  << " --to-petri <activity_defines_place|invisibles_no_duplicates> "
                     "INPUT.dfg [OUTPUT.json]\n";
        return 2;
      }
      const auto variant =
          minera::parse_dfg_petri_conversion_variant(argv[2]);
      const auto dfg = minera::import_dfg(argv[3]);
      const auto net = minera::convert_dfg_to_petri_net(
          dfg, minera::DfgPetriConversionOptions{variant});
      const auto json = minera::serialize_dfg_petri_net_json(net, variant);
      if (argc == 5) {
        std::ofstream output(argv[4], std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open output file");
        output << json << '\n';
      } else {
        std::cout << json << '\n';
      }
      return 0;
    }
    if (argc < 2) {
      std::cerr << "Usage: " << argv[0]
                << " INPUT.dfg [OUTPUT.dfg]\n"
                   "       "
                << argv[0]
                << " --clean-noise THRESHOLD [--most-common-paths A->B,...] "
                   "INPUT.dfg [OUTPUT.dfg]\n"
                   "       "
                << argv[0]
                << " --to-petri <variant> INPUT.dfg [OUTPUT.json]\n";
      return 2;
    }

    bool clean_noise = false;
    double noise_threshold = 0.0;
    std::set<std::pair<std::string, std::string>> most_common_paths;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
      const std::string_view argument(argv[i]);
      if (argument == "--clean-noise") {
        if (++i >= argc)
          throw std::invalid_argument("--clean-noise requires a threshold");
        clean_noise = true;
        noise_threshold = parse_number(argv[i], "--clean-noise");
      } else if (argument == "--most-common-paths") {
        if (++i >= argc)
          throw std::invalid_argument(
              "--most-common-paths requires a path list");
        most_common_paths = parse_paths(argv[i]);
      } else if (!argument.empty() && argument.front() == '-') {
        throw std::invalid_argument("unknown option: " + std::string(argument));
      } else {
        positional.emplace_back(argument);
      }
    }
    if (positional.empty() || positional.size() > 2)
      throw std::invalid_argument("expected INPUT.dfg and optional OUTPUT.dfg");
    if (!clean_noise && !most_common_paths.empty())
      throw std::invalid_argument(
          "--most-common-paths requires --clean-noise");

    auto dfg = minera::import_dfg(positional.front());
    if (clean_noise) {
      dfg = to_clean_analysis(minera::clean_dfg_based_on_noise_threshold(
          to_filter_data(dfg), noise_threshold, most_common_paths));
    }
    if (positional.size() == 2) {
      minera::export_dfg(positional[1], dfg);
    } else {
      std::cout << minera::to_dfg(dfg);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
