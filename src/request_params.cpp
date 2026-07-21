#include "minera/request_params.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace minera {

namespace {
std::size_t parse_nonnegative_size(std::string_view value, std::string_view name) {
  if (value.empty()) throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  std::size_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer, got '" + std::string(value) + "'");
  return parsed;
}

std::size_t parse_positive_size(std::string_view value, std::string_view name) {
  if (value.empty())
    throw std::invalid_argument(std::string(name) +
                                ": expected a positive integer");
  std::size_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                      parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
      parsed == 0) {
    throw std::invalid_argument(std::string(name) +
                                ": expected a positive integer, got '" +
                                std::string(value) + "'");
  }
  return parsed;
}

double parse_finite_double(std::string_view value, std::string_view name) {
  std::string owned(value);
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size() || errno == ERANGE ||
      !std::isfinite(parsed)) {
    throw std::invalid_argument(std::string(name) +
                                ": expected a finite number, got '" +
                                owned + "'");
  }
  return parsed;
}

bool parse_boolean(std::string_view value, std::string_view name) {
  if (value == "true" || value == "1") return true;
  if (value == "false" || value == "0") return false;
  throw std::invalid_argument(std::string(name) +
                              ": expected true, false, 1, or 0, got '" +
                              std::string(value) + "'");
}

std::map<std::string, double> parse_cost_map(std::string_view value,
                                             std::string_view name) {
  if (value.empty()) throw std::invalid_argument(std::string(name) + ": expected a value");
  std::map<std::string, double> result;
  std::vector<std::string> items;
  std::size_t begin = 0;
  bool escaped = false;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i < value.size() && escaped) {
      escaped = false;
      continue;
    }
    if (i < value.size() && value[i] == '\\') {
      escaped = true;
      continue;
    }
    if (i == value.size() || value[i] == ',') {
      auto item = std::string(value.substr(begin, i - begin));
      const auto first = item.find_first_not_of(' '), last = item.find_last_not_of(' ');
      if (first != std::string::npos) items.push_back(item.substr(first, last - first + 1));
      begin = i + 1;
    }
  }
  if (escaped)
    throw std::invalid_argument(std::string(name) + ": dangling escape");
  for (const auto& item : items) {
    std::size_t sep = std::string::npos;
    escaped = false;
    for (std::size_t i = 0; i < item.size(); ++i) {
      if (escaped) {
        escaped = false;
      } else if (item[i] == '\\') {
        escaped = true;
      } else if (item[i] == '=' || item[i] == ':') {
        sep = i;
        break;
      }
    }
    if (sep == std::string::npos || sep == 0 || sep + 1 >= item.size())
      throw std::invalid_argument(std::string(name) + ": expected activity=value pairs");
    std::string activity;
    activity.reserve(sep);
    for (std::size_t i = 0; i < sep; ++i) {
      if (item[i] != '\\') {
        activity.push_back(item[i]);
        continue;
      }
      if (++i >= sep || (item[i] != '\\' && item[i] != ',' &&
                         item[i] != '=' && item[i] != ':' &&
                         item[i] != 's' && item[i] != 't' &&
                         item[i] != 'n' && item[i] != 'r'))
        throw std::invalid_argument(std::string(name) + ": invalid escape");
      switch (item[i]) {
        case 's': activity.push_back(' '); break;
        case 't': activity.push_back('\t'); break;
        case 'n': activity.push_back('\n'); break;
        case 'r': activity.push_back('\r'); break;
        default: activity.push_back(item[i]); break;
      }
    }
    if (activity == ">>" || activity.empty())
      throw std::invalid_argument(std::string(name) + ": activity must not be a gap");
    const double cost = parse_finite_double(std::string_view(item).substr(sep + 1), name);
    if (cost < 0.0) throw std::invalid_argument(std::string(name) + ": costs must be non-negative");
    result[activity] = cost;
  }
  if (result.empty()) throw std::invalid_argument(std::string(name) + ": expected a non-empty map");
  return result;
}

std::uint64_t parse_positive_u64(std::string_view value, std::string_view name) {
  if (value.empty()) throw std::invalid_argument(std::string(name) + ": expected a positive integer");
  std::uint64_t parsed = 0;
  const auto r = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (r.ec != std::errc{} || r.ptr != value.data() + value.size() || parsed == 0)
    throw std::invalid_argument(std::string(name) + ": expected a positive integer");
  return parsed;
}
}

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

std::vector<BusinessHourSlot> parse_business_hour_slots(
    std::string_view value, std::string_view name) {
  std::vector<BusinessHourSlot> slots;
  for (const auto& part : split_csv(value)) {
    auto separator = part.find(':');
    if (separator == std::string::npos) separator = part.find('-');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == part.size()) {
      throw std::invalid_argument(std::string(name) +
                                  ": expected start-end second pairs");
    }
    const double start =
        parse_finite_double(std::string_view(part).substr(0, separator), name);
    const double end = parse_finite_double(
        std::string_view(part).substr(separator + 1), name);
    if (start < 0.0 || start >= end || end > 604800.0) {
      throw std::invalid_argument(
          std::string(name) +
          ": slots must satisfy 0 <= start < end <= 604800");
    }
    slots.push_back({start, end});
  }
  return slots;
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

DfgAlignmentOptions parse_dfg_alignment_params(const FieldLookup& field) {
  return parse_dfg_alignment_params(field, FieldPresence{});
}

DfgAlignmentOptions parse_dfg_alignment_params(const FieldLookup& field,
                                               const FieldPresence& present) {
  DfgAlignmentOptions o;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (!supplied(n) || v.empty()) throw std::invalid_argument(std::string(n)+": expected a value"); return v; };
  if (supplied("dfg_variant") || supplied("variant")) {
    const auto n = supplied("dfg_variant") ? "dfg_variant" : "variant";
    const auto v = require(n);
    if (v != "classic" && v != "CLASSIC" && v != "DFG_CLASSIC" && v != "Variants.CLASSIC")
      throw std::invalid_argument(std::string(n)+": expected classic");
  }
  auto parse_bound = [&](std::string_view n, std::size_t& dst) {
    if (!supplied(n)) return;
    dst = parse_nonnegative_size(require(n), n);
  };
  parse_bound("dfg_max_states", o.max_visited_states);
  if (supplied("max_visited_states")) parse_bound("max_visited_states", o.max_visited_states);
  if (supplied("dfg_max_time_ms")) o.max_time_ms = parse_nonnegative_size(require("dfg_max_time_ms"), "dfg_max_time_ms");
  if (supplied("max_time_ms")) o.max_time_ms = parse_nonnegative_size(require("max_time_ms"), "max_time_ms");
  const auto map_field = [&](std::string_view canonical, std::initializer_list<std::string_view> aliases,
                             std::map<std::string,double>& dst) {
    std::string_view found = canonical;
    if (!supplied(found)) for (auto a : aliases) if (supplied(a)) { found = a; break; }
    if (supplied(found)) dst = parse_cost_map(require(found), found);
  };
  map_field("dfg_sync_cost", {"sync_cost", "sync_costs", "dfg_sync_costs"}, o.sync_cost);
  map_field("dfg_model_move_cost", {"model_move_cost", "model_costs", "dfg_model_cost", "dfg_model_move_costs"}, o.model_move_cost);
  map_field("dfg_log_move_cost", {"log_move_cost", "log_costs", "dfg_log_cost", "dfg_log_move_costs"}, o.log_move_cost);
  map_field("dfg_internal_log_move_cost", {"internal_log_move_cost", "internal_costs", "dfg_internal_cost", "dfg_internal_log_move_costs"}, o.internal_log_move_cost);
  return o;
}

EditDistanceOptions parse_edit_distance_params(const FieldLookup& field) {
  return parse_edit_distance_params(field, FieldPresence{});
}

EditDistanceOptions parse_edit_distance_params(const FieldLookup& field,
                                               const FieldPresence& present) {
  EditDistanceOptions o;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (!supplied(n) || v.empty()) throw std::invalid_argument(std::string(n)+": expected a value"); return v; };
  const std::string_view variant_name = supplied("edit_variant") ? "edit_variant" : (supplied("variant") ? "variant" : "");
  if (!variant_name.empty()) {
    const auto v = require(variant_name);
    if (v != "EDIT_DISTANCE" && v != "edit_distance" && v != "Variants.EDIT_DISTANCE")
      throw std::invalid_argument(std::string(variant_name)+": expected EDIT_DISTANCE");
  }
  const std::string_view anti_name = supplied("anti") ? "anti" : (supplied("perform_anti_alignment") ? "perform_anti_alignment" : "");
  if (!anti_name.empty()) o.perform_anti_alignment = parse_boolean(require(anti_name), anti_name);
  const std::string_view cost_name = supplied("deviation_cost") ? "deviation_cost" : (supplied("edit_deviation_cost") ? "edit_deviation_cost" : "");
  if (!cost_name.empty()) o.deviation_cost = parse_positive_u64(require(cost_name), cost_name);
  return o;
}

AlignmentOptions parse_alignment_params(const FieldLookup& field) {
  return parse_alignment_params(field, FieldPresence{});
}

AlignmentOptions parse_alignment_params(const FieldLookup& field,
                                        const FieldPresence& present) {
  AlignmentOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) -> std::string {
    const auto value = field(name);
    if (!supplied(name) || value.empty()) {
      throw std::invalid_argument(std::string(name) + ": expected a value");
    }
    return value;
  };
  if (supplied("alignment_variant")) {
    const auto value = field("alignment_variant");
    if (value == "state_equation_a_star" || value == "VERSION_STATE_EQUATION_A_STAR") options.variant = AlignmentVariant::kStateEquationAStar;
    else if (value == "dijkstra_no_heuristics" || value == "VERSION_DIJKSTRA_NO_HEURISTICS") options.variant = AlignmentVariant::kDijkstraNoHeuristics;
    else if (value == "dijkstra_less_memory" || value == "VERSION_DIJKSTRA_LESS_MEMORY") options.variant = AlignmentVariant::kDijkstraLessMemory;
    else if (value == "discounted_a_star" || value == "VERSION_DISCOUNTED_A_STAR") options.variant = AlignmentVariant::kDiscountedAStar;
    else if (value == "generator_dijkstra_no_heuristics") options.variant = AlignmentVariant::kGeneratorDijkstraNoHeuristics;
    else if (value == "generator_dijkstra_less_memory") options.variant = AlignmentVariant::kGeneratorDijkstraLessMemory;
    else throw std::invalid_argument("alignment_variant: unknown variant '" + value + "'");
  }
  if (supplied("max_states")) options.max_states = parse_positive_size(require("max_states"), "max_states");
  if (supplied("max_alignments")) options.max_alignments = parse_positive_size(require("max_alignments"), "max_alignments");
  if (supplied("max_align_time")) {
    options.max_total_time_seconds = parse_finite_double(require("max_align_time"), "max_align_time");
    if (options.max_total_time_seconds < 0) throw std::invalid_argument("max_align_time: expected a non-negative number");
  }
  if (supplied("max_align_time_trace")) {
    options.max_time_seconds = parse_finite_double(require("max_align_time_trace"), "max_align_time_trace");
    if (options.max_time_seconds < 0) throw std::invalid_argument("max_align_time_trace: expected a non-negative number");
  }
  if (supplied("exponent")) {
    options.exponent = parse_finite_double(require("exponent"), "exponent");
    if (options.exponent <= 1.0) throw std::invalid_argument("exponent: expected a value greater than 1");
  }
  if (supplied("exponent") && supplied("theta"))
    throw std::invalid_argument("exponent and theta cannot both be supplied");
  if (supplied("theta")) {
    options.exponent = parse_finite_double(require("theta"), "theta");
    if (options.exponent <= 1.0) throw std::invalid_argument("theta: expected a value greater than 1");
  }
  if (supplied("synchronous_dijkstra")) options.synchronous_dijkstra = parse_boolean(require("synchronous_dijkstra"), "synchronous_dijkstra");
  if (supplied("cost_profile")) {
    const auto value = field("cost_profile");
    if (value == "legacy") options.cost_profile = AlignmentCostProfile::kLegacyUnit;
    else if (value == "pm4py") options.cost_profile = AlignmentCostProfile::kPm4PyStandard;
    else throw std::invalid_argument("cost_profile: expected legacy or pm4py");
  }
  const auto parse_cost = [&](std::string_view name, std::optional<double>& destination) {
    if (!supplied(name)) return;
    const double parsed = parse_finite_double(require(name), name);
    if (parsed < 0) throw std::invalid_argument(std::string(name) + ": expected a non-negative number");
    destination = parsed;
  };
  parse_cost("trace_cost", options.default_trace_cost);
  parse_cost("model_cost", options.default_model_cost);
  parse_cost("silent_model_cost", options.default_silent_cost);
  parse_cost("sync_cost", options.default_sync_cost);
  if (options.variant != AlignmentVariant::kDiscountedAStar) {
    for (const auto name : {std::string_view{"exponent"}, std::string_view{"theta"}})
      if (supplied(name)) throw std::invalid_argument(std::string(name) + ": requires alignment_variant=discounted_a_star");
  }
  if (supplied("max_alignments") &&
      options.variant != AlignmentVariant::kGeneratorDijkstraNoHeuristics &&
      options.variant != AlignmentVariant::kGeneratorDijkstraLessMemory)
    throw std::invalid_argument("max_alignments: requires a generator alignment variant");
  if (supplied("synchronous_dijkstra") &&
      options.variant != AlignmentVariant::kDiscountedAStar)
    throw std::invalid_argument("synchronous_dijkstra: requires alignment_variant=discounted_a_star");
  return options;
}

ProcessTreeAlignmentOptions parse_process_tree_alignment_params(
    const FieldLookup& field) {
  return parse_process_tree_alignment_params(field, FieldPresence{});
}

ProcessTreeAlignmentOptions parse_process_tree_alignment_params(
    const FieldLookup& field, const FieldPresence& present) {
  ProcessTreeAlignmentOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) -> std::string {
    const auto value = field(name);
    if (!supplied(name) || value.empty()) {
      throw std::invalid_argument(std::string(name) + ": expected a value");
    }
    return value;
  };
  const auto normalize = [](std::string value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
      if (c == '-' || c == ' ' || c == '.') out.push_back('_');
      else out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
  };
  if (supplied("pt_alignment_variant")) {
    const auto raw = require("pt_alignment_variant");
    auto value = normalize(raw);
    if (value.rfind("variants_", 0) == 0) value.erase(0, 9);
    if (value == "search_graph_pt" || value == "search_graph" ||
        value == "searchgraphpt" || value == "search_graph_pt_variant") {
      options.variant = ProcessTreeAlignmentVariant::kSearchGraphPt;
    } else if (value == "dynamic_programming" || value == "dynamic_programming_pt" ||
               value == "dynamicprogramming") {
      options.variant = ProcessTreeAlignmentVariant::kDynamicProgramming;
    } else if (value == "milp") {
      options.variant = ProcessTreeAlignmentVariant::kMilp;
    } else if (value == "approximated_original" || value == "approximatedoriginal" ||
               value == "approx_original") {
      options.variant = ProcessTreeAlignmentVariant::kApproximatedOriginal;
    } else {
      throw std::invalid_argument("pt_alignment_variant: unknown variant '" + raw + "'");
    }
  }
  const auto parse_bound = [&](std::string_view name, std::size_t& target) {
    if (supplied(name)) target = parse_nonnegative_size(require(name), name);
  };
  parse_bound("pt_alignment_max_states", options.max_states);
  parse_bound("pt_alignment_max_trace_length", options.max_trace_length);
  parse_bound("pt_alignment_max_process_tree_height", options.max_process_tree_height);
  if (supplied("pt_alignment_batch_size")) {
    options.batch_size = parse_positive_size(require("pt_alignment_batch_size"),
                                              "pt_alignment_batch_size");
  }
  const auto parse_ms = [&](std::string_view name, double& target) {
    if (!supplied(name)) return;
    const auto parsed = parse_finite_double(require(name), name);
    if (parsed < 0.0) {
      throw std::invalid_argument(std::string(name) + ": expected a non-negative finite number");
    }
    target = parsed / 1000.0;
  };
  parse_ms("pt_alignment_max_time_ms", options.max_time_seconds);
  parse_ms("pt_alignment_max_total_time_ms", options.max_total_time_seconds);
  parse_ms("pt_alignment_max_trace_time_ms", options.max_trace_time_seconds);
  parse_ms("pt_alignment_max_per_trace_time_ms", options.max_per_trace_time_seconds);
  if (supplied("pt_alignment_use_global_cache")) {
    options.use_global_cache = parse_boolean(require("pt_alignment_use_global_cache"),
                                              "pt_alignment_use_global_cache");
  }
  if (supplied("pt_alignment_clear_cache")) {
    options.clear_cache = parse_boolean(require("pt_alignment_clear_cache"),
                                         "pt_alignment_clear_cache");
  }
  return options;
}

MineParams parse_mine_params(const FieldLookup& field) {
  return parse_mine_params(field, FieldPresence{});
}

MineParams parse_mine_params(const FieldLookup& field,
                             const FieldPresence& present) {
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
  if (const auto value = field("lifecycle_column"); !value.empty()) {
    params.csv.lifecycle_column = value;
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
  if (const auto value = field("keep_lifecycles"); !value.empty()) {
    params.filter.keep_lifecycles = split_csv(value);
  }
  if (const auto value = field("remove_lifecycles"); !value.empty()) {
    params.filter.remove_lifecycles = split_csv(value);
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
  if (const auto value = field("keep_directly_follows"); !value.empty()) {
    params.filter.keep_directly_follows = parse_activity_pairs(value);
  }
  if (const auto value = field("remove_directly_follows"); !value.empty()) {
    params.filter.remove_directly_follows = parse_activity_pairs(value);
  }
  if (const auto value = field("keep_eventually_follows"); !value.empty()) {
    params.filter.keep_eventually_follows = parse_activity_pairs(value);
  }
  if (const auto value = field("remove_eventually_follows"); !value.empty()) {
    params.filter.remove_eventually_follows = parse_activity_pairs(value);
  }
  if (const auto value = field("prefix_activity"); !value.empty()) {
    params.filter.prefix_activity = value;
  }
  if (const auto value = field("suffix_activity"); !value.empty()) {
    params.filter.suffix_activity = value;
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
  if (const auto value = field("variant_coverage"); !value.empty()) {
    params.filter.variant_coverage = std::atof(value.c_str());
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
  if (const auto value = field("heuristic_variant"); !value.empty()) {
    if (value == "classic")
      params.heuristic.variant = HeuristicMinerVariant::kClassic;
    else if (value == "plusplus")
      params.heuristic.variant = HeuristicMinerVariant::kPlusPlus;
    else
      throw std::invalid_argument(
          "heuristic_variant: expected classic or plusplus, got '" + value +
          "'");
  }
  if (const auto value = field("dependency_threshold"); !value.empty()) {
    params.heuristic.dependency_threshold =
        parse_finite_double(value, "dependency_threshold");
  }
  if (const auto value = field("and_threshold"); !value.empty()) {
    params.heuristic.and_threshold =
        parse_finite_double(value, "and_threshold");
  }
  if (const auto value = field("min_edge_frequency"); !value.empty()) {
    params.heuristic.min_frequency =
        parse_nonnegative_size(value, "min_edge_frequency");
  }
  if (const auto value = field("heuristic_min_activity_count"); !value.empty()) {
    params.heuristic.min_activity_count =
        parse_nonnegative_size(value, "heuristic_min_activity_count");
  }
  if (const auto value = field("heuristic_decoration"); !value.empty()) {
    if (value == "frequency")
      params.heuristic.decoration = HeuristicNetDecoration::kFrequency;
    else if (value == "performance")
      params.heuristic.decoration = HeuristicNetDecoration::kPerformance;
    else
      throw std::invalid_argument(
          "heuristic_decoration: expected frequency or performance, got '" +
          value + "'");
  }
  if (const auto value = field("heuristic_start_timestamp_attribute");
      !value.empty()) {
    params.heuristic.start_timestamp_attribute = value;
  }
  if (const auto value = field("heuristic_lifecycle_instance_attribute");
      !value.empty()) {
    params.heuristic.lifecycle_instance_attribute = value;
  }
  if (const auto value =
          field("heuristic_dfg_pre_cleaning_noise_threshold");
      !value.empty()) {
    params.heuristic.dfg_pre_cleaning_noise_threshold = parse_finite_double(
        value, "heuristic_dfg_pre_cleaning_noise_threshold");
  }
  if (const auto value = field("heuristic_loop_length_two_threshold");
      !value.empty()) {
    params.heuristic.loop_length_two_threshold =
        parse_finite_double(value, "heuristic_loop_length_two_threshold");
  }
  if (const auto value = field("heuristic_keep_once_per_case");
      !value.empty()) {
    params.heuristic.keep_once_per_case =
        parse_boolean(value, "heuristic_keep_once_per_case");
  }
  if (const auto value = field("heuristic_most_common_paths");
      !value.empty()) {
    for (const auto& pair : parse_activity_pairs(value)) {
      params.heuristic.most_common_paths.emplace(pair.predecessor,
                                                 pair.successor);
    }
  }
  if (const auto value = field("heuristic_performance_aggregation");
      !value.empty()) {
    if (value == "mean")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kMean;
    else if (value == "median")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kMedian;
    else if (value == "min")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kMinimum;
    else if (value == "max")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kMaximum;
    else if (value == "stdev")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kStandardDeviation;
    else if (value == "sum")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kSum;
    else if (value == "raw_values")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kRawValues;
    else if (value == "all")
      params.heuristic.performance_aggregation =
          HeuristicPerformanceAggregation::kAll;
    else
      throw std::invalid_argument(
          "heuristic_performance_aggregation: expected mean, median, min, "
          "max, stdev, sum, raw_values, or all, got '" +
          value + "'");
  }

  if (const auto value = field("powl_variant"); !value.empty()) {
    if (value == "tree")
      params.powl.variant = PowlDiscoveryVariant::kTree;
    else if (value == "brute_force")
      params.powl.variant = PowlDiscoveryVariant::kBruteForce;
    else if (value == "maximal")
      params.powl.variant = PowlDiscoveryVariant::kMaximal;
    else if (value == "dynamic_clustering")
      params.powl.variant = PowlDiscoveryVariant::kDynamicClustering;
    else if (value == "dynamic_clustering_plain")
      params.powl.variant = PowlDiscoveryVariant::kDynamicClusteringPlain;
    else
      throw std::invalid_argument(
          "powl_variant: expected tree, brute_force, maximal, "
          "dynamic_clustering, or dynamic_clustering_plain, got '" +
          value + "'");
  }
  if (const auto value = field("powl_filtering_type"); !value.empty()) {
    if (value == "decreasing_factor")
      params.powl.filtering_type = PowlFilteringType::kDecreasingFactor;
    else if (value == "dynamic")
      params.powl.filtering_type = PowlFilteringType::kDynamic;
    else
      throw std::invalid_argument(
          "powl_filtering_type: expected decreasing_factor or dynamic, got '" +
          value + "'");
  }
  if (const auto value = field("powl_filtering_threshold"); !value.empty()) {
    params.powl.filtering_threshold =
        parse_finite_double(value, "powl_filtering_threshold");
    if (params.powl.filtering_threshold < 0.0 ||
        params.powl.filtering_threshold >= 1.0) {
      throw std::invalid_argument(
          "powl_filtering_threshold: expected 0 <= value < 1");
    }
  }
  if (const auto value = field("powl_order_frequency_ratio"); !value.empty()) {
    params.powl.order_frequency_ratio =
        parse_finite_double(value, "powl_order_frequency_ratio");
    if (params.powl.order_frequency_ratio <= 0.5 ||
        params.powl.order_frequency_ratio > 1.0) {
      throw std::invalid_argument(
          "powl_order_frequency_ratio: expected 0.5 < value <= 1");
    }
  }
  if (const auto value = field("powl_brute_force_activity_limit");
      !value.empty()) {
    params.powl.brute_force_activity_limit =
        parse_nonnegative_size(value, "powl_brute_force_activity_limit");
  }

  if (const auto value = field("transition_view"); !value.empty()) {
    if (value == "sequence") params.transition_system.view = TransitionView::kSequence;
    else if (value == "set") params.transition_system.view = TransitionView::kSet;
    else if (value == "multiset") params.transition_system.view = TransitionView::kMultiset;
    else throw std::invalid_argument("transition_view: expected sequence, set, or multiset, got '" + value + "'");
  }
  if (const auto value = field("transition_direction"); !value.empty()) {
    if (value == "forward") params.transition_system.direction = TransitionDirection::kForward;
    else if (value == "backward") params.transition_system.direction = TransitionDirection::kBackward;
    else throw std::invalid_argument("transition_direction: expected forward or backward, got '" + value + "'");
  }
  if (const auto value = field("transition_window"); !value.empty()) {
    params.transition_system.window = parse_nonnegative_size(value, "transition_window");
  }
  if (const auto value = field("transition_include_data"); !value.empty()) {
    params.transition_system.include_data =
        parse_boolean(value, "transition_include_data");
  }

  if (const auto value = field("token_replay_variant"); !value.empty()) {
    if (value == "forward")
      params.token_replay.variant = TokenReplayVariant::kForward;
    else if (value == "backwards")
      params.token_replay.variant = TokenReplayVariant::kBackwards;
    else
      throw std::invalid_argument(
          "token_replay_variant: expected forward or backwards, got '" +
          value + "'");
  }
  if (const auto value = field("token_replay_stop_immediately_unfit");
      !value.empty()) {
    params.token_replay.stop_immediately_unfit =
        parse_boolean(value, "token_replay_stop_immediately_unfit");
  }

  if (const auto value = field("log_skeleton_noise_threshold");
      !value.empty()) {
    params.log_skeleton.noise_threshold =
        parse_finite_double(value, "log_skeleton_noise_threshold");
    if (params.log_skeleton.noise_threshold < 0.0 ||
        params.log_skeleton.noise_threshold > 1.0) {
      throw std::invalid_argument(
          "log_skeleton_noise_threshold: expected 0 <= value <= 1");
    }
  }
  if (const auto value = field("log_skeleton_constraints"); !value.empty()) {
    const auto parts = split_csv(value);
    static constexpr std::string_view allowed[] = {
        "equivalence", "always_after", "always_before", "never_together",
        "directly_follows", "activ_freq"};
    std::vector<std::string> constraints;
    for (const auto& part : parts) {
      if (std::find(std::begin(allowed), std::end(allowed), part) ==
          std::end(allowed)) {
        throw std::invalid_argument(
            "log_skeleton_constraints: unknown constraint '" + part + "'");
      }
      constraints.push_back(part);
    }
    std::sort(constraints.begin(), constraints.end());
    constraints.erase(std::unique(constraints.begin(), constraints.end()),
                      constraints.end());
    params.log_skeleton.considered_constraints = std::move(constraints);
  }

  if (const auto value = field("declare_considered_activities");
      present ? present("declare_considered_activities") : !value.empty()) {
    auto activities = split_csv(value);
    params.declare_options.considered_activities = std::move(activities);
  }
  if (const auto value = field("declare_allowed_templates");
      present ? present("declare_allowed_templates") : !value.empty()) {
    static constexpr std::string_view allowed[] = {
        kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
        kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
        kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
        kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
        kDeclareChainSuccession, kDeclareAbsence, kDeclareCoexistence,
        kDeclareNonCoexistence, kDeclareNonSuccession,
        kDeclareNonChainSuccession};
    auto templates = split_csv(value);
    for (const auto& name : templates) {
      if (std::find(std::begin(allowed), std::end(allowed), name) ==
          std::end(allowed)) {
        throw std::invalid_argument(
            "declare_allowed_templates: unknown template '" + name + "'");
      }
    }
    std::sort(templates.begin(), templates.end());
    templates.erase(std::unique(templates.begin(), templates.end()),
                    templates.end());
    params.declare_options.allowed_templates = std::move(templates);
  }
  const auto parse_declare_ratio = [&](std::string_view field_name,
                                       double& destination,
                                       bool& has_value) {
    const auto value = field(field_name);
    if (value.empty()) return;
    destination = parse_finite_double(value, field_name);
    if (destination < 0.0 || destination > 1.0) {
      throw std::invalid_argument(std::string(field_name) +
                                  ": expected 0 <= value <= 1");
    }
    has_value = true;
  };
  parse_declare_ratio("declare_min_support_ratio",
                      params.declare_options.min_support_ratio,
                      params.declare_options.has_min_support_ratio);
  parse_declare_ratio("declare_min_confidence_ratio",
                      params.declare_options.min_confidence_ratio,
                      params.declare_options.has_min_confidence_ratio);
  if (const auto value = field("declare_auto_selection_multiplier");
      !value.empty()) {
    params.declare_options.auto_selection_multiplier =
        parse_finite_double(value, "declare_auto_selection_multiplier");
    if (params.declare_options.auto_selection_multiplier < 0.0 ||
        params.declare_options.auto_selection_multiplier > 1.0) {
      throw std::invalid_argument(
          "declare_auto_selection_multiplier: expected 0 <= value <= 1");
    }
  }

  const auto parse_footprints_set =
      [&](std::string_view field_name,
          const std::set<std::string>& allowed,
          std::set<std::string>& destination) {
        const auto value = field(field_name);
        const bool supplied = present ? present(field_name) : !value.empty();
        if (!supplied) return;
        const auto parts = split_csv(value);
        if (parts.empty()) {
          throw std::invalid_argument(std::string(field_name) +
                                      ": expected a non-empty list");
        }
        std::set<std::string> parsed;
        for (const auto& part : parts) {
          if (!allowed.contains(part)) {
            throw std::invalid_argument(std::string(field_name) +
                                        ": unknown variant '" + part + "'");
          }
          parsed.insert(part);
        }
        destination = std::move(parsed);
      };
  parse_footprints_set(
      "footprints_discovery_variants",
      {"entire_event_log", "trace_by_trace", "dfg", "process_tree", "powl",
       "petri_reach_graph"},
      params.footprints.discovery_variants);
  if (const auto value = field("footprints_model");
      present ? present("footprints_model") : !value.empty()) {
    static const std::set<std::string> allowed = {
        "process_tree", "powl", "inductive", "alpha", "heuristic", "dfg"};
    if (!allowed.contains(value)) {
      throw std::invalid_argument("footprints_model: unknown model '" + value +
                                  "'");
    }
    params.footprints.model = value;
  }
  parse_footprints_set("footprints_conformance_variants",
                       {"log_model", "log_extensive", "trace_extensive"},
                       params.footprints.conformance_variants);
  if (const auto value = field("footprints_strict");
      present ? present("footprints_strict") : !value.empty()) {
    params.footprints.strict = parse_boolean(value, "footprints_strict");
  }
  if (const auto value = field("footprints_enable_act_always_executed");
      present ? present("footprints_enable_act_always_executed")
              : !value.empty()) {
    params.footprints.enable_act_always_executed = parse_boolean(
        value, "footprints_enable_act_always_executed");
  }
  if (params.footprints.model == "dfg" &&
      params.footprints.discovery_variants.contains("petri_reach_graph")) {
    throw std::invalid_argument(
        "footprints_model=dfg cannot be combined with "
        "petri_reach_graph discovery");
  }

  const auto parse_dfg_variants =
      [&](std::string_view field_name,
          const std::vector<std::string>& catalog,
          std::vector<std::string>& destination) {
        const auto value = field(field_name);
        const bool supplied = present ? present(field_name) : !value.empty();
        if (!supplied) return;
        const auto requested = split_csv(value);
        if (requested.empty()) {
          throw std::invalid_argument(std::string(field_name) +
                                      ": expected a non-empty list");
        }
        for (const auto& name : requested) {
          if (std::find(catalog.begin(), catalog.end(), name) ==
              catalog.end()) {
            throw std::invalid_argument(std::string(field_name) +
                                        ": unknown variant '" + name + "'");
          }
        }
        destination.clear();
        for (const auto& name : catalog) {
          if (std::find(requested.begin(), requested.end(), name) !=
              requested.end()) {
            destination.push_back(name);
          }
        }
      };
  parse_dfg_variants(
      "dfg_statistical_variants",
      {"native", "frequency", "frequency_greedy", "frequency_triples",
       "performance", "performance_greedy", "case_attributes", "clean",
       "clean_polars", "clean_time"},
      params.dfg_statistical_variants);
  parse_dfg_variants("dfg_causal_variants", {"alpha", "heuristic"},
                     params.dfg_causal_variants);
  if (const auto value = field("dfg_window");
      present ? present("dfg_window") : !value.empty()) {
    params.dfg_statistics.window = parse_nonnegative_size(value, "dfg_window");
  }
  if (const auto value = field("dfg_keep_once_per_case");
      present ? present("dfg_keep_once_per_case") : !value.empty()) {
    params.dfg_statistics.keep_once_per_case =
        parse_boolean(value, "dfg_keep_once_per_case");
  }
  if (const auto value = field("dfg_performance_aggregation");
      present ? present("dfg_performance_aggregation") : !value.empty()) {
    if (value == "mean")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kMean;
    else if (value == "median")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kMedian;
    else if (value == "min")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kMinimum;
    else if (value == "max")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kMaximum;
    else if (value == "stdev")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kStandardDeviation;
    else if (value == "sum")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kSum;
    else if (value == "raw_values")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kRawValues;
    else if (value == "all")
      params.dfg_statistics.performance_aggregation =
          DfgPerformanceAggregation::kAll;
    else
      throw std::invalid_argument(
          "dfg_performance_aggregation: expected mean, median, min, max, "
          "stdev, sum, raw_values, or all, got '" +
          value + "'");
  }
  if (const auto value = field("dfg_start_timestamp_attribute");
      present ? present("dfg_start_timestamp_attribute") : !value.empty()) {
    params.dfg_statistics.start_timestamp_attribute = value;
  }
  if (const auto value = field("dfg_business_hours");
      present ? present("dfg_business_hours") : !value.empty()) {
    params.dfg_statistics.use_business_hours =
        parse_boolean(value, "dfg_business_hours");
  }
  if (const auto value = field("dfg_business_hour_slots");
      present ? present("dfg_business_hour_slots") : !value.empty()) {
    params.dfg_statistics.business_hour_slots =
        parse_business_hour_slots(value, "dfg_business_hour_slots");
  }
  if (const auto value = field("dfg_case_attributes");
      present ? present("dfg_case_attributes") : !value.empty()) {
    auto attributes = split_csv(value);
    std::sort(attributes.begin(), attributes.end());
    attributes.erase(std::unique(attributes.begin(), attributes.end()),
                     attributes.end());
    params.dfg_statistics.case_attributes = std::move(attributes);
  }
  if (const auto value = field("dfg_return_node_attributes");
      present ? present("dfg_return_node_attributes") : !value.empty()) {
    params.dfg_statistics.return_node_attributes =
        parse_boolean(value, "dfg_return_node_attributes");
  }

  // Correlation miner variants are deliberately opt-in. Keep a canonical
  // catalog order and deduplicate aliases/repeated selections so callers can
  // execute each requested PM4Py variant exactly once.
  const auto parse_correlation_variants = [&]() {
    constexpr std::string_view field_name = "correlation_variants";
    const auto value = field(field_name);
    const bool supplied = present ? present(field_name) : !value.empty();
    if (!supplied) return;
    static const std::vector<std::string> catalog = {
        "classic", "classic_split", "trace_based"};
    const auto requested = split_csv(value);
    if (requested.empty()) {
      throw std::invalid_argument("correlation_variants: expected a non-empty list");
    }
    for (const auto& name : requested) {
      if (std::find(catalog.begin(), catalog.end(), name) == catalog.end()) {
        throw std::invalid_argument("correlation_variants: unknown variant '" +
                                    name + "'");
      }
    }
    params.correlation_variants.clear();
    for (const auto& name : catalog) {
      if (std::find(requested.begin(), requested.end(), name) !=
          requested.end()) {
        params.correlation_variants.push_back(name);
      }
    }
  };
  parse_correlation_variants();
  if (const auto value = field("correlation_start_timestamp_attribute");
      present ? present("correlation_start_timestamp_attribute")
              : !value.empty()) {
    // With FieldPresence an explicit empty value is meaningful: it asks the
    // miner to use its own fallback attribute rather than the request parser's
    // default. The legacy lookup overload still treats empty as omitted.
    params.correlation.start_timestamp_attribute = value;
  }
  if (const auto value = field("correlation_exact_time_matching");
      present ? present("correlation_exact_time_matching") : !value.empty()) {
    params.correlation.exact_time_matching =
        parse_boolean(value, "correlation_exact_time_matching");
  }
  if (const auto value = field("correlation_sample_size");
      present ? present("correlation_sample_size") : !value.empty()) {
    params.correlation.sample_size =
        parse_positive_size(value, "correlation_sample_size");
  }

  // Performance spectrum and batch discovery are opt-in.  Unlike the legacy
  // list fields, an explicitly supplied empty spectrum pattern is invalid so
  // clients cannot accidentally request an unusable analysis.
  {
    const auto value = field("spectrum_activities");
    const bool supplied = present ? present("spectrum_activities") : !value.empty();
    if (supplied) {
      params.spectrum_activities = split_csv(value);
      if (params.spectrum_activities.size() < 2) {
        throw std::invalid_argument(
            "spectrum_activities: expected at least two activities");
      }
    }
  }
  const auto spectrum_supplied = [&]() {
    return present ? present("spectrum_activities")
                   : !field("spectrum_activities").empty();
  };
  const auto spectrum_option_supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  for (const auto name : {std::string_view{"spectrum_variant"},
                          std::string_view{"spectrum_sample_size"},
                          std::string_view{"spectrum_sort_log"}}) {
    if (spectrum_option_supplied(name) && !spectrum_supplied()) {
      throw std::invalid_argument(std::string(name) +
                                  ": requires spectrum_activities");
    }
  }
  if (const auto value = field("spectrum_variant");
      present ? present("spectrum_variant") : !value.empty()) {
    if (value == "connected") {
      params.spectrum.variant = PerformanceSpectrumVariant::kConnected;
    } else if (value == "disconnected") {
      params.spectrum.variant = PerformanceSpectrumVariant::kDisconnected;
    } else {
      throw std::invalid_argument(
          "spectrum_variant: expected connected or disconnected, got '" +
          value + "'");
    }
  }
  if (const auto value = field("spectrum_sample_size");
      present ? present("spectrum_sample_size") : !value.empty()) {
    params.spectrum.sample_size =
        parse_positive_size(value, "spectrum_sample_size");
  }
  if (const auto value = field("spectrum_sort_log");
      present ? present("spectrum_sort_log") : !value.empty()) {
    params.spectrum.sort_log = parse_boolean(value, "spectrum_sort_log");
  }
  if (const auto value = field("batches");
      present ? present("batches") : !value.empty()) {
    params.batches = parse_boolean(value, "batches");
  }
  if (const auto value = field("batch_merge_distance_seconds");
      present ? present("batch_merge_distance_seconds") : !value.empty()) {
    params.batch_options.merge_distance_seconds =
        parse_finite_double(value, "batch_merge_distance_seconds");
    if (params.batch_options.merge_distance_seconds < 0.0) {
      throw std::invalid_argument(
          "batch_merge_distance_seconds: expected a non-negative number");
    }
  }
  if (const auto value = field("batch_min_size");
      present ? present("batch_min_size") : !value.empty()) {
    params.batch_options.min_batch_size =
        parse_nonnegative_size(value, "batch_min_size");
    if (params.batch_options.min_batch_size < 2) {
      throw std::invalid_argument("batch_min_size: expected an integer >= 2");
    }
  }
  if (const auto value = field("batch_start_timestamp_attribute");
      present ? present("batch_start_timestamp_attribute") : !value.empty()) {
    // An explicit empty value intentionally selects the completion-time
    // fallback in the batch miner.
    params.batch_options.start_timestamp_attribute = value;
  }
  const auto batches_supplied = present ? present("batches") : !field("batches").empty();
  for (const auto name : {std::string_view{"batch_merge_distance_seconds"},
                          std::string_view{"batch_min_size"},
                          std::string_view{"batch_start_timestamp_attribute"}}) {
    const bool supplied = present ? present(name) : !field(name).empty();
    if (supplied && (!batches_supplied || !params.batches)) {
      throw std::invalid_argument(std::string(name) + ": requires batches=true");
    }
  }

  return params;
}

}  // namespace minera
