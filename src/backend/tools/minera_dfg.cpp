#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/dfg_conversion.hpp"
#include "minera/dfg_filtering.hpp"
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

std::vector<minera::DfgFilteringEdge> parse_paths(std::string_view text) {
  std::vector<minera::DfgFilteringEdge> result;
  for (const auto& pair : minera::parse_activity_pairs(text))
    result.emplace_back(pair.predecessor, pair.successor);
  return result;
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
                   "[filter options] INPUT.dfg [OUTPUT.dfg]\n"
                   "       "
                << argv[0]
                << " filter options: --activities-percentage P "
                   "--paths-percentage P --keep-connected T "
                   "[--keep-all-activities] --to-activity A "
                   "--from-activity A --contain-activity A\n"
                   "       "
                << argv[0]
                << " --to-petri <variant> INPUT.dfg [OUTPUT.json]\n";
      return 2;
    }

    bool clean_noise = false;
    double noise_threshold = 0.0;
    std::vector<minera::DfgFilteringEdge> most_common_paths;
    std::optional<double> activities_percentage;
    std::optional<double> paths_percentage;
    std::optional<double> keep_connected;
    bool keep_all_activities = false;
    std::optional<std::string> to_activity;
    std::optional<std::string> from_activity;
    std::optional<std::string> contain_activity;
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
      } else if (argument == "--activities-percentage" ||
                 argument == "--activities_percentage") {
        if (++i >= argc)
          throw std::invalid_argument(
              "--activities-percentage requires a percentage");
        activities_percentage = parse_number(argv[i], "--activities-percentage");
      } else if (argument == "--paths-percentage" ||
                 argument == "--paths_percentage") {
        if (++i >= argc)
          throw std::invalid_argument("--paths-percentage requires a percentage");
        paths_percentage = parse_number(argv[i], "--paths-percentage");
      } else if (argument == "--keep-connected" ||
                 argument == "--keep_connected") {
        if (++i >= argc)
          throw std::invalid_argument(
              "--keep-connected requires a dependency threshold");
        keep_connected = parse_number(argv[i], "--keep-connected");
      } else if (argument == "--keep-all-activities" ||
                 argument == "--keep_all_activities") {
        keep_all_activities = true;
      } else if (argument == "--to-activity" || argument == "--to_activity") {
        if (++i >= argc)
          throw std::invalid_argument("--to-activity requires an activity");
        to_activity = std::string(argv[i]);
        if (to_activity->empty())
          throw std::invalid_argument("--to-activity requires a non-empty activity");
      } else if (argument == "--from-activity" ||
                 argument == "--from_activity") {
        if (++i >= argc)
          throw std::invalid_argument("--from-activity requires an activity");
        from_activity = std::string(argv[i]);
        if (from_activity->empty())
          throw std::invalid_argument(
              "--from-activity requires a non-empty activity");
      } else if (argument == "--contain-activity" ||
                 argument == "--contain_activity") {
        if (++i >= argc)
          throw std::invalid_argument(
              "--contain-activity requires an activity");
        contain_activity = std::string(argv[i]);
        if (contain_activity->empty())
          throw std::invalid_argument(
              "--contain-activity requires a non-empty activity");
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

    if (keep_all_activities && !paths_percentage && !keep_connected) {
      throw std::invalid_argument(
          "--keep-all-activities requires --paths-percentage or "
          "--keep-connected");
    }

    // The reference port (algo.filtering.dfg.dfg_filtering). A .dfg file has
    // no activity counts; dfg_filtering_data_from_dfg derives them as the
    // reference's public DFG filter wrappers do.
    minera::DfgFilteringRequest filters;
    if (clean_noise) {
      filters.noise_threshold = noise_threshold;
      if (!most_common_paths.empty()) filters.most_common_paths = most_common_paths;
    }
    filters.activities_percentage = activities_percentage;
    filters.paths_percentage = paths_percentage;
    filters.keep_connected = keep_connected;
    filters.keep_all_activities = keep_all_activities;
    filters.to_activity = to_activity;
    filters.from_activity = from_activity;
    filters.contain_activity = contain_activity;
    minera::DfgCleanAnalysis dfg;
    try {
      dfg = minera::to_dfg_clean_analysis(minera::apply_dfg_filtering_request(
          minera::dfg_filtering_data_from_dfg(
              minera::import_dfg(positional.front())),
          filters));
    } catch (const minera::FilteringCommonError& error) {
      throw std::invalid_argument(error.type + ": " + error.message);
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
