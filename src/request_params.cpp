#include "minera/request_params.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace minera {

bool ends_with_ci(std::string_view value, std::string_view suffix) {
  if (value.size() < suffix.size()) return false;
  const auto tail = value.substr(value.size() - suffix.size());
  return std::equal(tail.begin(), tail.end(), suffix.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == b;
  });
}

bool looks_like_xes(std::string_view filename, std::string_view content) {
  if (ends_with_ci(filename, ".xes")) return true;
  std::size_t i = 0;
  if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
      static_cast<unsigned char>(content[1]) == 0xBB &&
      static_cast<unsigned char>(content[2]) == 0xBF) {
    i = 3;
  }
  while (i < content.size() &&
         std::isspace(static_cast<unsigned char>(content[i]))) {
    ++i;
  }
  const auto rest = content.substr(i);
  return rest.rfind("<?xml", 0) == 0 || rest.rfind("<log", 0) == 0;
}

std::vector<std::string> split_csv(std::string_view value) {
  std::vector<std::string> parts;
  const auto flush = [&](std::string_view token) {
    const auto begin = token.find_first_not_of(' ');
    if (begin == std::string_view::npos) return;  // all spaces: an empty part
    const auto end = token.find_last_not_of(' ');
    parts.emplace_back(token.substr(begin, end - begin + 1));
  };
  std::size_t start = 0;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i == value.size() || value[i] == ',') {
      flush(value.substr(start, i - start));
      start = i + 1;
    }
  }
  return parts;
}

std::string error_json(std::string_view message) {
  std::ostringstream out;
  out << "{\"error\": \"";
  for (const char c : message) {
    if (c == '"' || c == '\\') out << '\\';
    out << c;
  }
  out << "\"}";
  return out.str();
}

MineParams parse_mine_params(const FieldLookup& field) {
  MineParams params;

  if (const auto value = field("case_column"); !value.empty()) {
    params.csv.case_id_column = value;
  }
  if (const auto value = field("activity_column"); !value.empty()) {
    params.csv.activity_column = value;
  }
  if (const auto value = field("timestamp_column"); !value.empty()) {
    params.csv.timestamp_column = value;
  }
  if (const auto value = field("resource_column"); !value.empty()) {
    params.csv.resource_column = value;
  }
  if (const auto value = field("delimiter"); !value.empty()) {
    params.csv.delimiter = value.front();
  }

  // Optional log-preprocessing filters, applied before discovery.
  if (const auto value = field("time_from"); !value.empty()) {
    params.filter.time_from = value;
  }
  if (const auto value = field("time_to"); !value.empty()) {
    params.filter.time_to = value;
  }
  if (const auto value = field("time_mode"); !value.empty()) {
    const auto mode = parse_time_filter_mode(value);
    if (!mode) {
      throw std::invalid_argument(
          "time_mode: expected events, traces_contained, or "
          "traces_intersecting, got '" +
          value + "'");
    }
    params.filter.time_mode = *mode;
  }
  if (const auto value = field("keep_activities"); !value.empty()) {
    params.filter.keep_activities = split_csv(value);
  }
  if (const auto value = field("remove_activities"); !value.empty()) {
    params.filter.remove_activities = split_csv(value);
  }
  if (const auto value = field("keep_resources"); !value.empty()) {
    params.filter.keep_resources = split_csv(value);
  }
  if (const auto value = field("remove_resources"); !value.empty()) {
    params.filter.remove_resources = split_csv(value);
  }
  if (const auto value = field("attribute_key"); !value.empty()) {
    params.filter.attribute_key = value;
  }
  if (const auto value = field("keep_attribute_values"); !value.empty()) {
    params.filter.keep_attribute_values = split_csv(value);
  }
  if (const auto value = field("remove_attribute_values"); !value.empty()) {
    params.filter.remove_attribute_values = split_csv(value);
  }
  if (const auto value = field("case_attribute_key"); !value.empty()) {
    params.filter.case_attribute_key = value;
  }
  if (const auto value = field("keep_case_attribute_values"); !value.empty()) {
    params.filter.keep_case_attribute_values = split_csv(value);
  }
  if (const auto value = field("remove_case_attribute_values"); !value.empty()) {
    params.filter.remove_case_attribute_values = split_csv(value);
  }
  if (const auto value = field("attribute_mode"); !value.empty()) {
    const auto mode = parse_attribute_filter_mode(value);
    if (!mode) {
      throw std::invalid_argument(
          "attribute_mode: expected events or traces, got '" + value + "'");
    }
    params.filter.attribute_mode = *mode;
  }
  if (const auto value = field("rework_activity"); !value.empty()) {
    params.filter.rework_activity = value;
  }
  if (const auto value = field("rework_min_occurrences"); !value.empty()) {
    params.filter.rework_min_occurrences =
        static_cast<std::uint64_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("between_start"); !value.empty()) {
    params.filter.between_start = value;
  }
  if (const auto value = field("between_end"); !value.empty()) {
    params.filter.between_end = value;
  }
  if (const auto value = field("start_activities"); !value.empty()) {
    params.filter.start_activities = split_csv(value);
  }
  if (const auto value = field("end_activities"); !value.empty()) {
    params.filter.end_activities = split_csv(value);
  }
  if (const auto value = field("min_length"); !value.empty()) {
    params.filter.min_length = static_cast<std::size_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("max_length"); !value.empty()) {
    params.filter.max_length = static_cast<std::size_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("min_variant_frequency"); !value.empty()) {
    params.filter.min_variant_frequency =
        static_cast<std::uint64_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("top_variants"); !value.empty()) {
    params.filter.top_variants = static_cast<std::size_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("noise_threshold"); !value.empty()) {
    params.inductive.noise_threshold = std::atof(value.c_str());
  }
  if (const auto value = field("inductive_variant"); !value.empty()) {
    if (value != "log" && value != "dfg") {
      throw std::invalid_argument(
          "inductive_variant: expected log or dfg, got '" + value + "'");
    }
    params.inductive.variant = value == "dfg" ? InductiveMinerVariant::kDfg
                                              : InductiveMinerVariant::kLog;
  }
  if (const auto value = field("alpha_variant"); !value.empty()) {
    if (value != "classic" && value != "plus") {
      throw std::invalid_argument("alpha_variant: expected classic or plus, got '" +
                                  value + "'");
    }
    params.alpha.short_loops = value == "plus";
  }
  if (const auto value = field("dependency_threshold"); !value.empty()) {
    params.heuristic.dependency_threshold = std::atof(value.c_str());
  }
  if (const auto value = field("and_threshold"); !value.empty()) {
    params.heuristic.and_threshold = std::atof(value.c_str());
  }
  if (const auto value = field("min_edge_frequency"); !value.empty()) {
    params.heuristic.min_frequency =
        static_cast<std::uint64_t>(std::atoll(value.c_str()));
  }

  return params;
}

}  // namespace minera
