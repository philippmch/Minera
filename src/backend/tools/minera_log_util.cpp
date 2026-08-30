#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/log_util.hpp"
#include "minera/log_util_json.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

std::size_t parse_size(std::string_view text, std::string_view option) {
  if (text.empty() || text.front() == '-')
    throw std::invalid_argument(std::string(option) +
                                " expects a non-negative integer");
  std::size_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    throw std::invalid_argument(std::string(option) +
                                " expects a non-negative integer");
  return value;
}

std::uint64_t parse_seed(std::string_view text) {
  std::uint64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    throw std::invalid_argument("--seed expects a non-negative integer");
  return value;
}

std::vector<std::string> split_escaped(std::string_view text,
                                       std::string_view option) {
  std::vector<std::string> values;
  std::string current;
  bool escaped = false;
  for (const char ch : text) {
    if (escaped) {
      if (ch != '\\' && ch != ',')
        throw std::invalid_argument(std::string(option) +
                                    " supports only comma and backslash escapes");
      current.push_back(ch);
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == ',') {
      if (current.empty())
        throw std::invalid_argument(std::string(option) + " contains an empty item");
      values.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  if (escaped || current.empty())
    throw std::invalid_argument(std::string(option) + " contains an invalid item");
  values.push_back(std::move(current));
  return values;
}

double parse_percentage(std::string_view text) {
  const std::string value(text);
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end != value.c_str() + value.size() || !std::isfinite(parsed) ||
      parsed < 0.0 || parsed > 1.0)
    throw std::invalid_argument("--train-percentage expects a finite number in [0,1]");
  return parsed;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " INPUT.csv|INPUT.xes[.gz] (--train-percentage P | --prefix-length N |"
               " --all-prefixes | --until-activity ACTIVITY | --to-activities A,B |"
               " --feature-selection) [--seed N] [--activity-key NAME]"
               " [--timestamp-key NAME] [--feature-min-different N]"
               " [--feature-max-different N] [--feature-consider-all BOOL]"
               " [--feature-mandatory A,B]\n";
  return 2;
}

minera::EventLog import_log(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::invalid_argument("could not open event log: " + path);
  std::ostringstream content;
  content << input.rdbuf();
  const auto text = content.str();
  std::istringstream stream(text);
  auto lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  if (lower.ends_with(".xes") || lower.ends_with(".xes.gz"))
    return minera::import_xes_event_log(stream);
  return minera::import_csv_event_log(stream);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3) return usage(argv[0]);
    const std::string input_path = argv[1];
    minera::LogUtilityVariant variant = minera::LogUtilityVariant::kSplitTrainTest;
    std::size_t selected = 0;
    std::size_t operation_count = 0;
    double train_percentage = 0.8;
    std::uint64_t seed = 0;
    std::string activity;
    std::string activity_key = "activity";
    std::string timestamp_key = "timestamp";
    std::vector<std::string> activities;
    minera::LogFeatureSelectionOptions feature_options;

    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--all-prefixes") {
        variant = minera::LogUtilityVariant::kAllPrefixes;
        ++operation_count;
      } else if (option == "--feature-selection") {
        variant = minera::LogUtilityVariant::kFeatureSelection;
        ++operation_count;
      } else {
        if (i + 1 >= argc) return usage(argv[0]);
        const std::string value = argv[++i];
        if (option == "--train-percentage") {
          variant = minera::LogUtilityVariant::kSplitTrainTest;
          train_percentage = parse_percentage(value);
          ++operation_count;
        } else if (option == "--prefix-length") {
          variant = minera::LogUtilityVariant::kFixedPrefixes;
          selected = parse_size(value, option);
          ++operation_count;
        } else if (option == "--until-activity") {
          variant = minera::LogUtilityVariant::kUntilActivity;
          activity = value;
          ++operation_count;
        } else if (option == "--to-activities") {
          variant = minera::LogUtilityVariant::kToActivities;
          activities = split_escaped(value, option);
          ++operation_count;
        } else if (option == "--seed") {
          seed = parse_seed(value);
        } else if (option == "--activity-key") {
          activity_key = value;
        } else if (option == "--timestamp-key") {
          timestamp_key = value;
        } else if (option == "--feature-min-different") {
          feature_options.min_different_values = parse_size(value, option);
        } else if (option == "--feature-max-different") {
          feature_options.max_different_values = parse_size(value, option);
        } else if (option == "--feature-consider-all") {
          if (value == "true" || value == "1") feature_options.consider_all_attributes = true;
          else if (value == "false" || value == "0") feature_options.consider_all_attributes = false;
          else throw std::invalid_argument(option + " expects true or false");
        } else if (option == "--feature-mandatory") {
          feature_options.mandatory_attributes = split_escaped(value, option);
        } else {
          return usage(argv[0]);
        }
      }
    }
    if (operation_count != 1)
      throw std::invalid_argument("choose exactly one log utility operation");
    const auto log = import_log(input_path);
    if (variant == minera::LogUtilityVariant::kSplitTrainTest) {
      std::cout << minera::log_train_test_json(
                       minera::split_log_train_test(log, train_percentage, seed))
                << '\n';
    } else if (variant == minera::LogUtilityVariant::kFixedPrefixes) {
      std::cout << minera::log_prefix_json(
                       minera::get_log_prefixes(log, selected), variant)
                << '\n';
    } else if (variant == minera::LogUtilityVariant::kAllPrefixes) {
      const auto result = minera::get_log_with_prefixes(log);
      std::cout << minera::log_prefix_json(result.log, variant,
                                           result.change_indexes)
                << '\n';
    } else if (variant == minera::LogUtilityVariant::kUntilActivity) {
      minera::LogUtilityOptions options;
      options.activity_key = activity_key;
      options.timestamp_key = timestamp_key;
      std::cout << minera::log_until_activity_json(
                       minera::get_log_traces_until_activity(log, activity,
                                                              options))
                << '\n';
    } else if (variant == minera::LogUtilityVariant::kToActivities) {
      minera::LogUtilityOptions options;
      options.activity_key = activity_key;
      options.timestamp_key = timestamp_key;
      std::cout << minera::log_activity_targets_json(
                       minera::get_log_traces_to_activities(log, activities,
                                                            options))
                << '\n';
    } else {
      std::cout << minera::log_feature_selection_json(
                       minera::select_log_features(log, feature_options))
                << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
