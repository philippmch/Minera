#include "minera/request_params.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <limits>
#include <climits>
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

std::uint64_t parse_nonnegative_u64(std::string_view value,
                                    std::string_view name) {
  if (value.empty())
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  std::uint64_t parsed = 0;
  const auto r = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (r.ec != std::errc{} || r.ptr != value.data() + value.size())
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  return parsed;
}

std::vector<std::size_t> parse_marking(std::string_view value,
                                       std::string_view name) {
  if (value.empty())
    throw std::invalid_argument(std::string(name) + ": expected a non-empty marking");
  std::vector<std::size_t> result;
  std::size_t begin = 0;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i != value.size() && value[i] != ',') continue;
    auto token = value.substr(begin, i - begin);
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
      throw std::invalid_argument(std::string(name) + ": expected non-negative marking entries");
    const auto last = token.find_last_not_of(" \t\r\n");
    token = token.substr(first, last - first + 1);
    result.push_back(parse_nonnegative_size(token, name));
    begin = i + 1;
  }
  if (result.empty())
    throw std::invalid_argument(std::string(name) + ": expected a non-empty marking");
  return result;
}

std::vector<int> parse_int_marking(std::string_view value,
                                   std::string_view name) {
  if (value.empty())
    throw std::invalid_argument(std::string(name) +
                                ": expected a non-empty marking");
  std::vector<int> result;
  std::size_t begin = 0;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i != value.size() && value[i] != ',') continue;
    auto token = value.substr(begin, i - begin);
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
      throw std::invalid_argument(std::string(name) +
                                  ": expected non-negative marking entries");
    const auto last = token.find_last_not_of(" \t\r\n");
    token = token.substr(first, last - first + 1);
    const auto parsed = parse_nonnegative_size(token, name);
    if (parsed > static_cast<std::size_t>(INT_MAX))
      throw std::invalid_argument(std::string(name) +
                                  ": marking entry is out of range");
    result.push_back(static_cast<int>(parsed));
    begin = i + 1;
  }
  if (result.empty())
    throw std::invalid_argument(std::string(name) +
                                ": expected a non-empty marking");
  return result;
}

template <typename Map>
void parse_activity_cost_field(const FieldLookup& field,
                               const FieldPresence& present,
                               std::string_view name, Map& destination) {
  const bool supplied = present ? present(name) : !field(name).empty();
  if (!supplied) return;
  const auto value = field(name);
  const auto parsed = parse_cost_map(value, name);
  destination.clear();
  destination.insert(parsed.begin(), parsed.end());
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

// Target classes use a small escaped-list grammar so activity names may
// contain commas: `\\,` is a comma and `\\\\` is a backslash.
std::vector<std::string> split_target_activities(std::string_view value) {
  if (value.empty()) return {};
  std::vector<std::string> parts;
  std::string current;
  bool escaped = false;
  const auto flush = [&]() {
    const auto begin = current.find_first_not_of(" \t\r\n");
    if (begin != std::string::npos) {
      const auto end = current.find_last_not_of(" \t\r\n");
      parts.push_back(current.substr(begin, end - begin + 1));
    }
    current.clear();
  };
  for (const char ch : value) {
    if (escaped) {
      if (ch != ',' && ch != '\\')
        throw std::invalid_argument(
            "target_activities: invalid escape (use \\, or \\\\)");
      current.push_back(ch);
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == ',') {
      flush();
    } else {
      current.push_back(ch);
    }
  }
  if (escaped)
    throw std::invalid_argument("target_activities: dangling escape");
  flush();
  return parts;
}

std::map<std::string, double> parse_activity_costs(std::string_view value,
                                                   std::string_view name) {
  return parse_cost_map(value, name);
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
    else if (value == "standard") options.cost_profile = AlignmentCostProfile::kStandard;
    else throw std::invalid_argument("cost_profile: expected legacy or standard");
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

DecomposedAlignmentOptions parse_decomposed_alignment_params(const FieldLookup& field) {
  return parse_decomposed_alignment_params(field, FieldPresence{});
}

DecomposedAlignmentOptions parse_decomposed_alignment_params(const FieldLookup& field,
                                                             const FieldPresence& present) {
  DecomposedAlignmentOptions o;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (!supplied(n) || v.empty()) throw std::invalid_argument(std::string(n)+": expected a value"); return v; };
  if (supplied("variant")) { const auto v=require("variant"); if(v!="recompos_maximal"&&v!="RECOMPOS_MAXIMAL"&&v!="Variants.RECOMPOS_MAXIMAL") throw std::invalid_argument("variant: expected recompos_maximal"); }
  auto nonneg = [&](std::string_view n,double& d){ if(supplied(n)){ d=parse_finite_double(require(n),n); if(d<0) throw std::invalid_argument(std::string(n)+": expected a non-negative number"); }};
  nonneg("threshold_border_agreement",o.threshold_border_agreement); nonneg("max_time_trace",o.max_time_trace); nonneg("max_time_total",o.max_time_total);
  if(supplied("max_states")) o.max_states=parse_positive_size(require("max_states"),"max_states"); if(supplied("max_total_states")) o.max_total_states=parse_positive_size(require("max_total_states"),"max_total_states");
  if(supplied("use_cache")) o.use_cache=parse_boolean(require("use_cache"),"use_cache"); if(supplied("clear_cache")) o.clear_cache=parse_boolean(require("clear_cache"),"clear_cache");
  nonneg("log_move_cost",o.log_move_cost); nonneg("model_move_cost",o.model_move_cost); nonneg("tau_move_cost",o.tau_model_cost); nonneg("tau_model_cost",o.tau_model_cost); nonneg("sync_move_cost",o.sync_cost); nonneg("sync_cost",o.sync_cost);
  parse_activity_cost_field(field,present,"activity_log_costs",o.activity_log_costs); parse_activity_cost_field(field,present,"activity_model_costs",o.activity_model_costs); parse_activity_cost_field(field,present,"activity_sync_costs",o.activity_sync_costs);
  if(supplied("initial_marking")) o.initial_marking=parse_marking(require("initial_marking"),"initial_marking"); if(supplied("final_marking")) o.final_marking=parse_marking(require("final_marking"),"final_marking"); return o;
}

PetriAntiAlignmentOptions parse_petri_anti_alignment_params(const FieldLookup& field) { return parse_petri_anti_alignment_params(field, FieldPresence{}); }
PetriAntiAlignmentOptions parse_petri_anti_alignment_params(const FieldLookup& field,const FieldPresence& present) {
  PetriAntiAlignmentOptions o; const auto supplied=[&](std::string_view n){return present?present(n):!field(n).empty();}; const auto require=[&](std::string_view n){auto v=field(n);if(!supplied(n)||v.empty())throw std::invalid_argument(std::string(n)+": expected a value");return v;};
  if(supplied("variant")){auto v=require("variant");if(v!="version_discounted_a_star"&&v!="VERSION_DISCOUNTED_A_STAR"&&v!="discounted_a_star")throw std::invalid_argument("variant: expected version_discounted_a_star");}
  if(supplied("exponent")){o.exponent=parse_finite_double(require("exponent"),"exponent");if(o.exponent<=1)throw std::invalid_argument("exponent: expected > 1");} if(supplied("epsilon")){o.epsilon=parse_finite_double(require("epsilon"),"epsilon");if(o.epsilon<0)throw std::invalid_argument("epsilon: expected non-negative");}
  if(supplied("marking_limit"))o.marking_limit=parse_positive_size(require("marking_limit"),"marking_limit"); if(supplied("max_states"))o.max_states=parse_nonnegative_size(require("max_states"),"max_states"); if(supplied("initial_marking"))o.initial_marking=parse_marking(require("initial_marking"),"initial_marking"); if(supplied("final_marking"))o.final_marking=parse_marking(require("final_marking"),"final_marking"); return o;
}
PetriMultiAlignmentOptions parse_petri_multi_alignment_params(const FieldLookup& field) { return parse_petri_multi_alignment_params(field, FieldPresence{}); }
PetriMultiAlignmentOptions parse_petri_multi_alignment_params(const FieldLookup& field,const FieldPresence& present) {
  PetriMultiAlignmentOptions o; const auto supplied=[&](std::string_view n){return present?present(n):!field(n).empty();}; const auto require=[&](std::string_view n){auto v=field(n);if(!supplied(n)||v.empty())throw std::invalid_argument(std::string(n)+": expected a value");return v;};
  if(supplied("variant")){auto v=require("variant");if(v!="version_discounted_a_star"&&v!="VERSION_DISCOUNTED_A_STAR"&&v!="discounted_a_star")throw std::invalid_argument("variant: expected version_discounted_a_star");} if(supplied("exponent")){o.exponent=parse_finite_double(require("exponent"),"exponent");if(o.exponent<=1)throw std::invalid_argument("exponent: expected > 1");} if(supplied("marking_limit"))o.marking_limit=parse_positive_size(require("marking_limit"),"marking_limit"); if(supplied("max_states"))o.max_states=parse_nonnegative_size(require("max_states"),"max_states"); if(supplied("initial_marking"))o.initial_marking=parse_marking(require("initial_marking"),"initial_marking"); if(supplied("final_marking"))o.final_marking=parse_marking(require("final_marking"),"final_marking"); return o;
}
ProcessTreeReductionOptions parse_process_tree_reduction_params(const FieldLookup& field) { return parse_process_tree_reduction_params(field, FieldPresence{}); }
ProcessTreeReductionOptions parse_process_tree_reduction_params(const FieldLookup& field,const FieldPresence& present) {
  ProcessTreeReductionOptions o; const auto supplied=[&](std::string_view n){return present?present(n):!field(n).empty();}; if(supplied("variant")){auto v=field("variant");if(v!="tree_tr_based"&&v!="TREE_TR_BASED")throw std::invalid_argument("variant: expected tree_tr_based");} if(supplied("activity_key"))o.activity_key=field("activity_key"); return o;
}

EvaluationParams parse_evaluation_params(const FieldLookup& field) {
  return parse_evaluation_params(field, FieldPresence{});
}

EvaluationParams parse_evaluation_params(const FieldLookup& field,
                                         const FieldPresence& present) {
  EvaluationParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (!supplied(name) || value.empty()) {
      throw std::invalid_argument(std::string(name) + ": expected a value");
    }
    return value;
  };
  // Family selection is optional and defaults to replay fitness.  The
  // selected family does not gate parsing of the remaining fields below.
  if (supplied("evaluation_family")) {
    const auto value = require("evaluation_family");
    if (value == "replay_fitness") {
      params.family = EvaluationFamily::kReplayFitness;
    } else if (value == "precision") {
      params.family = EvaluationFamily::kPrecision;
    } else if (value == "generalization") {
      params.family = EvaluationFamily::kGeneralization;
    } else if (value == "simplicity") {
      params.family = EvaluationFamily::kSimplicity;
    } else {
      throw std::invalid_argument(
          "evaluation_family: expected replay_fitness, precision, "
          "generalization, or simplicity, got '" + value + "'");
    }
  }

  if (supplied("fitness_variant")) {
    const auto value = require("fitness_variant");
    if (value == "auto") {
      params.replay_fitness_variant = ReplayFitnessVariant::AUTO;
    } else if (value == "token_based") {
      params.replay_fitness_variant = ReplayFitnessVariant::TOKEN_BASED;
    } else if (value == "alignment_based") {
      params.replay_fitness_variant = ReplayFitnessVariant::ALIGNMENT_BASED;
    } else {
      throw std::invalid_argument(
          "fitness_variant: expected auto, token_based, or alignment_based, "
          "got '" + value + "'");
    }
  }

  if (supplied("precision_variant")) {
    const auto value = require("precision_variant");
    if (value == "auto") {
      params.precision_variant = PrecisionVariant::AUTO;
    } else if (value == "etconformance_token") {
      params.precision_variant = PrecisionVariant::ETCONFORMANCE_TOKEN;
    } else if (value == "align_etconformance") {
      params.precision_variant = PrecisionVariant::ALIGN_ETCONFORMANCE;
    } else if (value == "automaton_after_align") {
      params.precision_variant = PrecisionVariant::AUTOMATON_AFTER_ALIGN;
    } else {
      throw std::invalid_argument(
          "precision_variant: expected auto, etconformance_token, "
          "align_etconformance, or automaton_after_align, got '" + value +
          "'");
    }
  }
  if (supplied("precision_use_task_ids")) {
    params.precision_options.use_task_ids =
        parse_boolean(require("precision_use_task_ids"),
                      "precision_use_task_ids");
  }
  if (supplied("precision_max_states")) {
    params.precision_options.max_states =
        parse_positive_size(require("precision_max_states"),
                            "precision_max_states");
  }

  if (supplied("generalization_variant")) {
    const auto value = require("generalization_variant");
    if (value != "generalization_token") {
      throw std::invalid_argument(
          "generalization_variant: expected generalization_token, got '" +
          value + "'");
    }
    params.generalization_variant = GeneralizationVariant::GENERALIZATION_TOKEN;
  }

  if (supplied("simplicity_variant")) {
    const auto value = require("simplicity_variant");
    if (value == "arc_degree") {
      params.simplicity_variant = SimplicityVariant::ARC_DEGREE;
    } else if (value == "extended_cardoso") {
      params.simplicity_variant = SimplicityVariant::EXTENDED_CARDOSO;
    } else if (value == "extended_cyclomatic") {
      params.simplicity_variant = SimplicityVariant::EXTENDED_CYCLOMATIC;
    } else {
      throw std::invalid_argument(
          "simplicity_variant: expected arc_degree, extended_cardoso, or "
          "extended_cyclomatic, got '" + value + "'");
    }
  }
  if (supplied("simplicity_arc_degree_k")) {
    params.simplicity_options.arc_degree_k =
        parse_finite_double(require("simplicity_arc_degree_k"),
                            "simplicity_arc_degree_k");
    if (params.simplicity_options.arc_degree_k < 0.0) {
      throw std::invalid_argument(
          "simplicity_arc_degree_k: expected a non-negative number");
    }
  }
  if (supplied("simplicity_max_states")) {
    params.simplicity_options.max_states =
        parse_positive_size(require("simplicity_max_states"),
                            "simplicity_max_states");
  }
  if (supplied("simplicity_initial_marking")) {
    params.simplicity_options.initial_marking =
        parse_int_marking(require("simplicity_initial_marking"),
                          "simplicity_initial_marking");
  }
  if (supplied("simplicity_final_marking")) {
    params.simplicity_options.final_marking =
        parse_int_marking(require("simplicity_final_marking"),
                          "simplicity_final_marking");
  }
  return params;
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
  // execute each requested variant exactly once.
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

PetriAnalysisParams parse_petri_analysis_params(const FieldLookup& field) {
  return parse_petri_analysis_params(field, {});
}

PetriAnalysisParams parse_petri_analysis_params(const FieldLookup& field,
                                                const FieldPresence& present) {
  PetriAnalysisParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("petri_analysis_family")) {
    const auto value = require("petri_analysis_family");
    if (value == "marking_equation")
      params.family = PetriAnalysisFamily::kMarkingEquation;
    else if (value == "extended_marking_equation")
      params.family = PetriAnalysisFamily::kExtendedMarkingEquation;
    else if (value == "workflow_net")
      params.family = PetriAnalysisFamily::kWorkflowNet;
    else if (value == "soundness")
      params.family = PetriAnalysisFamily::kSoundness;
    else
      throw std::invalid_argument(
          "petri_analysis_family: expected marking_equation, "
          "extended_marking_equation, workflow_net, or soundness");
  }
  if (supplied("petri_analysis_miner"))
    params.miner = require("petri_analysis_miner");
  if (supplied("petri_analysis_trace_index"))
    params.trace_index = parse_nonnegative_size(
        require("petri_analysis_trace_index"), "petri_analysis_trace_index");
  const auto parse_marking_field = [&](std::string_view name, Marking& target) {
    if (!supplied(name)) return;
    const auto parsed = parse_marking(require(name), name);
    target.clear();
    target.reserve(parsed.size());
    for (const auto value : parsed) {
      if (value > static_cast<std::size_t>(
                      std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument(std::string(name) +
                                    ": token count is too large");
      target.push_back(static_cast<std::int64_t>(value));
    }
  };
  parse_marking_field("petri_analysis_initial_marking", params.initial_marking);
  parse_marking_field("petri_analysis_final_marking", params.final_marking);
  const auto parse_doubles = [&](std::string_view name,
                                 std::vector<double>& target) {
    if (!supplied(name)) return;
    const auto value = require(name);
    target.clear();
    const auto parts = split_csv(value);
    if (parts.empty())
      throw std::invalid_argument(std::string(name) + ": expected a non-empty list");
    target.reserve(parts.size());
    for (const auto& part : parts) {
      const auto cost = parse_finite_double(part, name);
      if (cost < 0.0)
        throw std::invalid_argument(std::string(name) +
                                    ": costs must be non-negative");
      target.push_back(cost);
    }
  };
  parse_doubles("petri_analysis_transition_costs", params.transition_costs);
  parse_doubles("petri_analysis_model_costs", params.model_transition_costs);
  parse_doubles("petri_analysis_sync_costs",
                params.synchronous_transition_costs);
  parse_doubles("petri_analysis_log_costs", params.log_move_costs);
  if (supplied("petri_analysis_split_indices")) {
    const auto value = require("petri_analysis_split_indices");
    const auto parts = split_csv(value);
    if (parts.empty())
      throw std::invalid_argument(
          "petri_analysis_split_indices: expected a non-empty list");
    for (const auto& part : parts)
      params.split_indices.push_back(parse_nonnegative_size(
          part, "petri_analysis_split_indices"));
  }
  if (supplied("petri_analysis_max_k"))
    params.max_k_value = parse_nonnegative_size(
        require("petri_analysis_max_k"), "petri_analysis_max_k");
  if (supplied("petri_analysis_max_states"))
    params.max_states = parse_positive_size(
        require("petri_analysis_max_states"), "petri_analysis_max_states");
  if (supplied("petri_analysis_full_bootstrap"))
    params.full_bootstrap_required = parse_boolean(
        require("petri_analysis_full_bootstrap"),
        "petri_analysis_full_bootstrap");
  if (supplied("petri_analysis_diagnostics"))
    params.diagnostics = parse_boolean(require("petri_analysis_diagnostics"),
                                       "petri_analysis_diagnostics");
  return params;
}

ProcessTreePlayoutParams parse_process_tree_playout_params(
    const FieldLookup& field) {
  return parse_process_tree_playout_params(field, {});
}

ProcessTreePlayoutParams parse_process_tree_playout_params(
    const FieldLookup& field, const FieldPresence& present) {
  ProcessTreePlayoutParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("playout_variant")) {
    const auto value = require("playout_variant");
    if (value == "BASIC_PLAYOUT" || value == "basic" || value == "basic_playout")
      params.variant = ProcessTreePlayoutVariant::kBasicPlayout;
    else if (value == "EXTENSIVE" || value == "extensive")
      params.variant = ProcessTreePlayoutVariant::kExtensive;
    else if (value == "TOPBOTTOM" || value == "TOP_BOTTOM" || value == "topbottom" || value == "top_bottom")
      params.variant = ProcessTreePlayoutVariant::kTopBottom;
    else
      throw std::invalid_argument(
          "playout_variant: expected BASIC_PLAYOUT, EXTENSIVE, or TOPBOTTOM");
  }
  if (supplied("playout_traces"))
    params.traces = parse_positive_size(require("playout_traces"), "playout_traces");
  if (supplied("playout_max_steps"))
    params.max_steps = parse_nonnegative_size(require("playout_max_steps"), "playout_max_steps");
  if (supplied("playout_seed"))
    params.seed = parse_nonnegative_u64(require("playout_seed"), "playout_seed");
  if (supplied("playout_min_length"))
    params.min_length = parse_nonnegative_size(require("playout_min_length"), "playout_min_length");
  if (supplied("playout_max_length"))
    params.max_length = parse_nonnegative_size(require("playout_max_length"), "playout_max_length");
  if (supplied("playout_max_loop_occurrences"))
    params.max_loop_occurrences = parse_nonnegative_size(require("playout_max_loop_occurrences"), "playout_max_loop_occurrences");
  if (supplied("playout_max_traces"))
    params.max_traces = parse_positive_size(require("playout_max_traces"), "playout_max_traces");
  if (supplied("playout_max_trace_cap"))
    params.max_traces = parse_positive_size(require("playout_max_trace_cap"), "playout_max_trace_cap");
  if (params.min_length > params.max_length)
    throw std::invalid_argument("playout_min_length must not exceed playout_max_length");
  return params;
}

DfgPlayoutParams parse_dfg_playout_params(const FieldLookup& field) {
  return parse_dfg_playout_params(field, {});
}

DfgPlayoutParams parse_dfg_playout_params(const FieldLookup& field,
                                           const FieldPresence& present) {
  DfgPlayoutParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty()) {
      throw std::invalid_argument(std::string(name) + ": expected a value");
    }
    return value;
  };
  const auto nonnegative = [&](std::string_view name) {
    return parse_nonnegative_size(require(name), name);
  };
  const auto positive = [&](std::string_view name) {
    return parse_positive_size(require(name), name);
  };
  const auto number = [&](std::string_view name) {
    return parse_finite_double(require(name), name);
  };

  if (supplied("dfg_playout_variant")) {
    const auto value = require("dfg_playout_variant");
    if (value == "classic" || value == "CLASSIC") {
      params.variant = DfgPlayoutVariant::kClassic;
    } else if (value == "performance" || value == "PERFORMANCE") {
      params.variant = DfgPlayoutVariant::kPerformance;
    } else {
      throw std::invalid_argument(
          "dfg_playout_variant: expected classic or performance");
    }
  }
  if (supplied("dfg_playout_max_variants"))
    params.max_variants = positive("dfg_playout_max_variants");
  if (supplied("dfg_playout_min_weighted_probability")) {
    params.min_weighted_probability =
        number("dfg_playout_min_weighted_probability");
    if (params.min_weighted_probability < 0.0 ||
        params.min_weighted_probability > 1.0)
      throw std::invalid_argument(
          "dfg_playout_min_weighted_probability must be between 0 and 1");
  }
  if (supplied("dfg_playout_max_occurrences_per_activity"))
    params.max_occurrences_per_activity =
        positive("dfg_playout_max_occurrences_per_activity");
  if (supplied("dfg_playout_interrupt_when_dfg_complete"))
    params.interrupt_when_dfg_complete = parse_boolean(
        require("dfg_playout_interrupt_when_dfg_complete"),
        "dfg_playout_interrupt_when_dfg_complete");
  if (supplied("dfg_playout_add_trace_if_takes_new_edges"))
    params.add_trace_if_takes_new_edges = parse_boolean(
        require("dfg_playout_add_trace_if_takes_new_edges"),
        "dfg_playout_add_trace_if_takes_new_edges");
  if (supplied("dfg_playout_max_execution_time_ms"))
    params.max_execution_time_ms =
        nonnegative("dfg_playout_max_execution_time_ms");
  if (supplied("dfg_playout_return_variants"))
    params.return_variants = parse_boolean(require("dfg_playout_return_variants"),
                                           "dfg_playout_return_variants");
  if (supplied("dfg_playout_return_only_if_complete"))
    params.return_only_if_complete = parse_boolean(
        require("dfg_playout_return_only_if_complete"),
        "dfg_playout_return_only_if_complete");
  if (supplied("dfg_playout_min_variant_occurrences"))
    params.min_variant_occurrences =
        positive("dfg_playout_min_variant_occurrences");
  if (supplied("dfg_playout_traces"))
    params.traces = positive("dfg_playout_traces");
  if (supplied("dfg_playout_case_arrival_rate")) {
    params.case_arrival_rate = number("dfg_playout_case_arrival_rate");
    if (params.case_arrival_rate < 0.0)
      throw std::invalid_argument(
          "dfg_playout_case_arrival_rate must be non-negative");
  }
  if (supplied("dfg_playout_seed"))
    params.seed = parse_nonnegative_u64(require("dfg_playout_seed"),
                                        "dfg_playout_seed");
  if (supplied("dfg_playout_max_events_per_trace"))
    params.max_events_per_trace =
        positive("dfg_playout_max_events_per_trace");
  if (supplied("dfg_playout_max_steps"))
    params.max_events_per_trace = positive("dfg_playout_max_steps");
  if (supplied("dfg_playout_start_time"))
    params.start_time = require("dfg_playout_start_time");
  return params;
}

StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field) {
  return parse_stochastic_playout_params(field, {});
}

StochasticPlayoutParams parse_stochastic_playout_params(
    const FieldLookup& field, const FieldPresence& present) {
  StochasticPlayoutParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("stochastic_traces"))
    params.traces = parse_positive_size(require("stochastic_traces"),
                                        "stochastic_traces");
  if (supplied("stochastic_max_steps"))
    params.max_steps = parse_nonnegative_size(require("stochastic_max_steps"),
                                              "stochastic_max_steps");
  if (supplied("stochastic_seed"))
    params.seed = parse_nonnegative_u64(require("stochastic_seed"),
                                        "stochastic_seed");
  if (supplied("stochastic_transition_weights")) {
    const auto value = require("stochastic_transition_weights");
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= value.size(); ++i) {
      if (i < value.size() && value[i] != ',') continue;
      const auto token = value.substr(begin, i - begin);
      const auto first = token.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos)
        throw std::invalid_argument(
            "stochastic_transition_weights: expected non-negative values");
      const auto last = token.find_last_not_of(" \t\r\n");
      const double weight = parse_finite_double(
          token.substr(first, last - first + 1),
          "stochastic_transition_weights");
      if (weight < 0.0)
        throw std::invalid_argument(
            "stochastic_transition_weights must be non-negative");
      params.transition_weights.push_back(weight);
      begin = i + 1;
    }
    if (params.transition_weights.empty())
      throw std::invalid_argument(
          "stochastic_transition_weights: expected a non-empty list");
  }
  if (supplied("stochastic_stop_at_final"))
    params.stop_at_final = parse_boolean(require("stochastic_stop_at_final"),
                                         "stochastic_stop_at_final");
  if (supplied("stochastic_start"))
    params.start = parse_finite_double(require("stochastic_start"),
                                       "stochastic_start");
  if (supplied("stochastic_case_interval")) {
    params.case_interval = parse_finite_double(
        require("stochastic_case_interval"), "stochastic_case_interval");
    if (params.case_interval < 0.0)
      throw std::invalid_argument(
          "stochastic_case_interval must be non-negative");
  }
  if (supplied("stochastic_event_interval")) {
    params.event_interval = parse_finite_double(
        require("stochastic_event_interval"), "stochastic_event_interval");
    if (params.event_interval < 0.0)
      throw std::invalid_argument(
          "stochastic_event_interval must be non-negative");
  }
  return params;
}

MonteCarloParams parse_monte_carlo_params(const FieldLookup& field) {
  return parse_monte_carlo_params(field, {});
}

MonteCarloParams parse_monte_carlo_params(const FieldLookup& field,
                                          const FieldPresence& present) {
  MonteCarloParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  const auto trim = [](std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return std::string_view{};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  };
  const auto nonnegative = [&](std::string_view name) {
    const double value = parse_finite_double(require(name), name);
    if (value < 0.0)
      throw std::invalid_argument(std::string(name) +
                                  " must be non-negative");
    return value;
  };

  if (supplied("monte_carlo_simulations"))
    params.simulations = parse_positive_size(require("monte_carlo_simulations"),
                                             "monte_carlo_simulations");
  if (supplied("monte_carlo_case_arrival_seconds"))
    params.case_arrival_seconds =
        nonnegative("monte_carlo_case_arrival_seconds");
  if (supplied("monte_carlo_max_steps"))
    params.max_steps =
        parse_nonnegative_size(require("monte_carlo_max_steps"),
                               "monte_carlo_max_steps");
  if (supplied("monte_carlo_seed"))
    params.seed = parse_nonnegative_u64(require("monte_carlo_seed"),
                                       "monte_carlo_seed");
  if (supplied("monte_carlo_start"))
    params.start = parse_finite_double(require("monte_carlo_start"),
                                       "monte_carlo_start");
  if (supplied("monte_carlo_default_duration_seconds"))
    params.default_duration_seconds =
        nonnegative("monte_carlo_default_duration_seconds");
  if (supplied("monte_carlo_default_resources_per_place"))
    params.default_resources_per_place = parse_positive_size(
        require("monte_carlo_default_resources_per_place"),
        "monte_carlo_default_resources_per_place");
  if (supplied("monte_carlo_enable_diagnostics"))
    params.enable_diagnostics = parse_boolean(
        require("monte_carlo_enable_diagnostics"),
        "monte_carlo_enable_diagnostics");
  if (supplied("monte_carlo_stop_at_final"))
    params.stop_at_final = parse_boolean(require("monte_carlo_stop_at_final"),
                                         "monte_carlo_stop_at_final");

  if (supplied("monte_carlo_transition_weights")) {
    const auto value = require("monte_carlo_transition_weights");
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= value.size(); ++i) {
      if (i < value.size() && value[i] != ',') continue;
      const auto token = value.substr(begin, i - begin);
      const auto first = token.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos)
        throw std::invalid_argument(
            "monte_carlo_transition_weights: expected non-negative values");
      const auto last = token.find_last_not_of(" \t\r\n");
      const double weight = parse_finite_double(
          token.substr(first, last - first + 1),
          "monte_carlo_transition_weights");
      if (weight < 0.0)
        throw std::invalid_argument(
            "monte_carlo_transition_weights must be non-negative");
      params.transition_weights.push_back(weight);
      begin = i + 1;
    }
    if (params.transition_weights.empty())
      throw std::invalid_argument(
          "monte_carlo_transition_weights: expected a non-empty list");
  }

  if (supplied("monte_carlo_transition_durations")) {
    const auto value = require("monte_carlo_transition_durations");
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= value.size(); ++i) {
      if (i < value.size() && value[i] != ',') continue;
      auto token = value.substr(begin, i - begin);
      const auto first = token.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos)
        throw std::invalid_argument(
            "monte_carlo_transition_durations: expected distribution:value entries");
      const auto last = token.find_last_not_of(" \t\r\n");
      token = token.substr(first, last - first + 1);
      std::size_t separator = token.find(':');
      if (separator == std::string_view::npos) separator = token.find('=');
      if (separator == std::string_view::npos || separator == 0 ||
          separator + 1 > token.size())
        throw std::invalid_argument(
            "monte_carlo_transition_durations: expected distribution:param entries");
      std::string distribution(trim(token.substr(0, separator)));
      std::transform(distribution.begin(), distribution.end(), distribution.begin(),
                     [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });
      MonteCarloDurationDistribution kind;
      if (distribution == "deterministic") {
        kind = MonteCarloDurationDistribution::kDeterministic;
      } else if (distribution == "exponential") {
        kind = MonteCarloDurationDistribution::kExponential;
      } else if (distribution == "constant0") {
        kind = MonteCarloDurationDistribution::kConstant0;
      } else if (distribution == "immediate") {
        kind = MonteCarloDurationDistribution::kImmediate;
      } else if (distribution == "normal") {
        kind = MonteCarloDurationDistribution::kNormal;
      } else if (distribution == "uniform") {
        kind = MonteCarloDurationDistribution::kUniform;
      } else if (distribution == "lognormal") {
        kind = MonteCarloDurationDistribution::kLogNormal;
      } else if (distribution == "gamma") {
        kind = MonteCarloDurationDistribution::kGamma;
      } else {
        throw std::invalid_argument(
            "monte_carlo_transition_durations: unknown distribution '" +
            distribution + "'");
      }
      std::vector<double> values;
      const auto parameter_text = token.substr(separator + 1);
      if (!parameter_text.empty()) {
        std::size_t parameter_begin = 0;
        for (std::size_t parameter_index = 0;
             parameter_index <= parameter_text.size(); ++parameter_index) {
          if (parameter_index < parameter_text.size() &&
              parameter_text[parameter_index] != ';')
            continue;
          const auto parameter = trim(parameter_text.substr(
              parameter_begin, parameter_index - parameter_begin));
          if (parameter.empty())
            throw std::invalid_argument(
                "monte_carlo_transition_durations: expected finite parameters");
          values.push_back(parse_finite_double(
              parameter, "monte_carlo_transition_durations"));
          parameter_begin = parameter_index + 1;
        }
      }
      const bool zero_arity =
          kind == MonteCarloDurationDistribution::kConstant0 ||
          kind == MonteCarloDurationDistribution::kImmediate;
      const std::size_t expected_arity =
          kind == MonteCarloDurationDistribution::kNormal ||
                  kind == MonteCarloDurationDistribution::kUniform
              ? 2
              : kind == MonteCarloDurationDistribution::kLogNormal ||
                        kind == MonteCarloDurationDistribution::kGamma
                    ? 3
                    : 1;
      if ((!zero_arity && values.size() != expected_arity) ||
          (zero_arity && values.size() > 1))
        throw std::invalid_argument(
            "monte_carlo_transition_durations: wrong parameter count for '" +
            distribution + "'");
      MonteCarloDuration duration;
      duration.distribution = kind;
      if (!values.empty()) duration.value_seconds = values[0];
      if (values.size() > 1) duration.secondary_seconds = values[1];
      if (values.size() > 2) duration.tertiary_seconds = values[2];
      if (duration.value_seconds < 0.0 || duration.secondary_seconds < 0.0 ||
          duration.tertiary_seconds < 0.0)
        throw std::invalid_argument(
            "monte_carlo_transition_durations must be non-negative");
      if ((kind == MonteCarloDurationDistribution::kLogNormal ||
           kind == MonteCarloDurationDistribution::kGamma) &&
          (duration.value_seconds <= 0.0 || duration.tertiary_seconds <= 0.0))
        throw std::invalid_argument(
            "monte_carlo_transition_durations lognormal/gamma shape and scale must be positive");
      params.transition_durations.push_back(duration);
      begin = i + 1;
    }
    if (params.transition_durations.empty())
      throw std::invalid_argument(
          "monte_carlo_transition_durations: expected a non-empty list");
  }

  if (supplied("monte_carlo_resources_per_place")) {
    const auto value = require("monte_carlo_resources_per_place");
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= value.size(); ++i) {
      if (i < value.size() && value[i] != ',') continue;
      auto token = value.substr(begin, i - begin);
      const auto first = token.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos)
        throw std::invalid_argument(
            "monte_carlo_resources_per_place: expected place=count entries");
      const auto last = token.find_last_not_of(" \t\r\n");
      token = token.substr(first, last - first + 1);
      const auto separator = token.find('=');
      if (separator == std::string_view::npos || separator == 0 ||
          separator + 1 >= token.size())
        throw std::invalid_argument(
            "monte_carlo_resources_per_place: expected place=count entries");
      const auto place = parse_nonnegative_size(
          trim(token.substr(0, separator)),
          "monte_carlo_resources_per_place");
      const auto capacity = parse_positive_size(
          trim(token.substr(separator + 1)),
          "monte_carlo_resources_per_place");
      if (!params.resources_per_place.emplace(place, capacity).second)
        throw std::invalid_argument(
            "monte_carlo_resources_per_place: duplicate place");
      begin = i + 1;
    }
    if (params.resources_per_place.empty())
      throw std::invalid_argument(
          "monte_carlo_resources_per_place: expected a non-empty map");
  }
  return params;
}

ConceptDriftParams parse_concept_drift_params(const FieldLookup& field) {
  return parse_concept_drift_params(field, {});
}

ConceptDriftParams parse_concept_drift_params(const FieldLookup& field,
                                             const FieldPresence& present) {
  ConceptDriftParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("drift_sub_log_size"))
    params.sub_log_size = parse_positive_size(require("drift_sub_log_size"), "drift_sub_log_size");
  if (supplied("drift_window_size"))
    params.window_size = parse_positive_size(require("drift_window_size"), "drift_window_size");
  if (supplied("drift_num_permutations")) {
    params.num_permutations = parse_positive_size(require("drift_num_permutations"), "drift_num_permutations");
    if (params.num_permutations > 1'000'000)
      throw std::invalid_argument("drift_num_permutations must be in [1, 1000000]");
  }
  if (supplied("drift_threshold")) {
    params.threshold = parse_finite_double(require("drift_threshold"), "drift_threshold");
    if (params.threshold < 0.0 || params.threshold > 1.0)
      throw std::invalid_argument("drift_threshold must be finite and in [0,1]");
  }
  if (supplied("drift_max_no_change_points"))
    params.max_no_change_points = parse_nonnegative_size(require("drift_max_no_change_points"), "drift_max_no_change_points");
  if (supplied("drift_seed"))
    params.seed = parse_nonnegative_u64(require("drift_seed"), "drift_seed");
  if (supplied("drift_include_timestamps"))
    params.include_timestamps = parse_boolean(require("drift_include_timestamps"), "drift_include_timestamps");
  return params;
}

LogToTargetParams parse_log_to_target_params(const FieldLookup& field) {
  return parse_log_to_target_params(field, {});
}

LogToTargetParams parse_log_to_target_params(const FieldLookup& field,
                                             const FieldPresence& present) {
  LogToTargetParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("target_variant")) {
    const auto value = require("target_variant");
    if (value == "next_activity" || value == "NEXT_ACTIVITY") {
      params.variant = LogToTargetVariant::NEXT_ACTIVITY;
    } else if (value == "next_time" || value == "NEXT_TIME") {
      params.variant = LogToTargetVariant::NEXT_TIME;
    } else if (value == "remaining_time" || value == "REMAINING_TIME") {
      params.variant = LogToTargetVariant::REMAINING_TIME;
    } else {
      throw std::invalid_argument(
          "target_variant: expected next_activity, next_time, or remaining_time");
    }
  }
  if (supplied("target_activity_key"))
    params.activity_key = require("target_activity_key");
  if (supplied("target_timestamp_key"))
    params.timestamp_key = require("target_timestamp_key");
  if (supplied("target_enable_padding")) {
    params.enable_padding = parse_boolean(require("target_enable_padding"),
                                          "target_enable_padding");
  }
  if (supplied("target_pad_size"))
    params.pad_size = parse_nonnegative_size(require("target_pad_size"),
                                             "target_pad_size");
  if (supplied("target_activities")) {
    const auto value = require("target_activities");
    params.activities = split_target_activities(value);
    if (params.activities.empty())
      throw std::invalid_argument("target_activities: expected a non-empty list");
  }
  return params;
}

TrieParams parse_trie_params(const FieldLookup& field) {
  return parse_trie_params(field, {});
}

TrieParams parse_trie_params(const FieldLookup& field,
                             const FieldPresence& present) {
  TrieParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("trie_activity_key"))
    params.activity_key = require("trie_activity_key");
  if (supplied("trie_max_path_length")) {
    params.max_path_length = parse_positive_size(
        require("trie_max_path_length"), "trie_max_path_length");
  }
  return params;
}

IntervalTreeParams parse_interval_tree_params(const FieldLookup& field) {
  return parse_interval_tree_params(field, {});
}

IntervalTreeParams parse_interval_tree_params(const FieldLookup& field,
                                              const FieldPresence& present) {
  IntervalTreeParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("interval_activity_key"))
    params.activity_key = require("interval_activity_key");
  if (supplied("interval_start_timestamp_key"))
    params.start_timestamp_key = require("interval_start_timestamp_key");
  if (supplied("interval_timestamp_key"))
    params.timestamp_key = require("interval_timestamp_key");
  if (supplied("interval_epsilon")) {
    params.epsilon = parse_finite_double(require("interval_epsilon"),
                                         "interval_epsilon");
    if (params.epsilon < 0.0)
      throw std::invalid_argument("interval_epsilon must be non-negative");
  }
  if (supplied("interval_filter_activity_couple")) {
    const auto parts = split_target_activities(
        require("interval_filter_activity_couple"));
    if (parts.size() != 2)
      throw std::invalid_argument(
          "interval_filter_activity_couple: expected two labels");
    params.filter_activity_couple = std::make_pair(parts[0], parts[1]);
  }
  return params;
}

CaseRelationsParams parse_case_relations_params(const FieldLookup& field) {
  return parse_case_relations_params(field, {});
}

CaseRelationsParams parse_case_relations_params(const FieldLookup& field,
                                                const FieldPresence& present) {
  CaseRelationsParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("merge_timestamp_key"))
    params.timestamp_key = require("merge_timestamp_key");
  return params;
}

LinkAnalysisParams parse_link_analysis_params(const FieldLookup& field) {
  return parse_link_analysis_params(field, {});
}

LinkAnalysisParams parse_link_analysis_params(const FieldLookup& field,
                                              const FieldPresence& present) {
  LinkAnalysisParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  const auto assign_string = [&](std::string_view name, std::string& target) {
    if (supplied(name)) target = require(name);
  };
  assign_string("link_analysis_out_column", params.out_column);
  assign_string("link_analysis_in_column", params.in_column);
  assign_string("link_analysis_sorting_column", params.sorting_column);
  if (supplied("link_analysis_look_forward"))
    params.look_forward = parse_boolean(
        require("link_analysis_look_forward"), "link_analysis_look_forward");
  if (supplied("link_analysis_keep_first_occurrence"))
    params.keep_first_occurrence = parse_boolean(
        require("link_analysis_keep_first_occurrence"),
        "link_analysis_keep_first_occurrence");
  if (supplied("link_analysis_propagate"))
    params.propagate = parse_boolean(require("link_analysis_propagate"),
                                     "link_analysis_propagate");
  if (supplied("link_analysis_max_links"))
    params.max_links = parse_positive_size(require("link_analysis_max_links"),
                                           "link_analysis_max_links");
  return params;
}

TimestampInterleavingsParams parse_timestamp_interleavings_params(
    const FieldLookup& field) {
  return parse_timestamp_interleavings_params(field, {});
}

TimestampInterleavingsParams parse_timestamp_interleavings_params(
    const FieldLookup& field, const FieldPresence& present) {
  TimestampInterleavingsParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("interleavings_timestamp_key"))
    params.timestamp_key = require("interleavings_timestamp_key");
  if (supplied("interleavings_max_results"))
    params.max_interleavings = parse_positive_size(
        require("interleavings_max_results"), "interleavings_max_results");
  return params;
}

RandomTreeParams parse_random_tree_params(const FieldLookup& field) {
  return parse_random_tree_params(field, {});
}

RandomTreeParams parse_random_tree_params(const FieldLookup& field,
                                          const FieldPresence& present) {
  RandomTreeParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("random_tree_variant")) {
    const auto value = require("random_tree_variant");
    if (value == "basic") {
      params.variant = RandomTreeVariant::kBasic;
    } else if (value == "ptandloggenerator" || value == "pt_and_log_generator" ||
               value == "ptandlog") {
      params.variant = RandomTreeVariant::kPtAndLogGenerator;
    } else {
      throw std::invalid_argument(
          "random_tree_variant: expected basic or ptandloggenerator");
    }
  }
  if (supplied("random_tree_rec_depth"))
    params.rec_depth = parse_nonnegative_size(require("random_tree_rec_depth"),
                                              "random_tree_rec_depth");
  if (supplied("random_tree_min_rec_depth"))
    params.min_rec_depth = parse_nonnegative_size(
        require("random_tree_min_rec_depth"), "random_tree_min_rec_depth");
  if (supplied("random_tree_max_rec_depth"))
    params.max_rec_depth = parse_nonnegative_size(
        require("random_tree_max_rec_depth"), "random_tree_max_rec_depth");
  if (supplied("random_tree_prob_leaf")) {
    params.prob_leaf = parse_finite_double(require("random_tree_prob_leaf"),
                                           "random_tree_prob_leaf");
    if (params.prob_leaf < 0.0 || params.prob_leaf > 1.0)
      throw std::invalid_argument("random_tree_prob_leaf must be in [0,1]");
  }
  if (supplied("random_tree_seed"))
    params.seed = static_cast<std::uint64_t>(
        parse_nonnegative_size(require("random_tree_seed"),
                               "random_tree_seed"));
  if (supplied("random_tree_activities")) {
    params.activities = split_target_activities(
        require("random_tree_activities"));
    if (params.activities.empty())
      throw std::invalid_argument("random_tree_activities: expected labels");
  }
  if (supplied("random_tree_mode"))
    params.mode = parse_nonnegative_size(require("random_tree_mode"),
                                         "random_tree_mode");
  if (supplied("random_tree_min"))
    params.min = parse_nonnegative_size(require("random_tree_min"),
                                        "random_tree_min");
  if (supplied("random_tree_max"))
    params.max = parse_nonnegative_size(require("random_tree_max"),
                                        "random_tree_max");
  const auto parse_probability = [&](std::string_view field,
                                     double& destination) {
    if (!supplied(field)) return;
    destination = parse_finite_double(require(field), field);
    if (destination < 0.0 || destination > 1.0)
      throw std::invalid_argument(std::string(field) + " must be in [0,1]");
  };
  parse_probability("random_tree_sequence", params.sequence);
  parse_probability("random_tree_choice", params.choice);
  parse_probability("random_tree_parallel", params.parallel);
  parse_probability("random_tree_loop", params.loop);
  parse_probability("random_tree_or", params.or_probability);
  parse_probability("random_tree_silent", params.silent);
  parse_probability("random_tree_duplicate", params.duplicate);
  if (supplied("random_tree_no_models"))
    params.no_models = parse_nonnegative_size(require("random_tree_no_models"),
                                              "random_tree_no_models");
  return params;
}

LogToFeaturesParams parse_log_to_features_params(const FieldLookup& field) {
  return parse_log_to_features_params(field, {});
}

LogToFeaturesParams parse_log_to_features_params(const FieldLookup& field,
                                                 const FieldPresence& present) {
  LogToFeaturesParams params;
  const auto key_for = [&](std::string_view suffix) {
    const std::string plural = "features_" + std::string(suffix);
    const std::string singular = "feature_" + std::string(suffix);
    if (present) {
      if (present(plural)) return plural;
      if (present(singular)) return singular;
    } else {
      if (!field(plural).empty()) return plural;
      if (!field(singular).empty()) return singular;
    }
    return plural;
  };
  const auto supplied = [&](std::string_view suffix) {
    const auto k = key_for(suffix);
    return present ? present(k) : !field(k).empty();
  };
  const auto require = [&](std::string_view suffix) {
    const auto k = key_for(suffix);
    const auto v = field(k);
    if (v.empty()) throw std::invalid_argument(k + ": expected a value");
    return v;
  };
  if (supplied("variant")) {
    const auto v = require("variant");
    if (v == "event" || v == "event_based") params.variant = LogToFeaturesVariant::EVENT_BASED;
    else if (v == "trace" || v == "trace_based") params.variant = LogToFeaturesVariant::TRACE_BASED;
    else if (v == "temporal") params.variant = LogToFeaturesVariant::TEMPORAL;
    else throw std::invalid_argument("features_variant: unknown variant '" + v + "'");
  }
  auto assign_string = [&](std::string_view suffix, std::string& out) {
    if (supplied(suffix)) out = require(suffix);
  };
  assign_string("activity_key", params.activity_key);
  assign_string("timestamp_key", params.timestamp_key);
  assign_string("start_timestamp_key", params.start_timestamp_key);
  assign_string("resource_key", params.resource_key);
  if (supplied("window_seconds")) {
    params.window_seconds = parse_finite_double(require("window_seconds"), key_for("window_seconds"));
    if (params.window_seconds <= 0.0) throw std::invalid_argument("features_window_seconds must be positive");
  }
  if (supplied("default_not_present"))
    params.default_not_present = parse_finite_double(require("default_not_present"), key_for("default_not_present"));
  const auto parse_list = [&](std::string_view suffix, std::vector<std::string>& out) {
    if (!supplied(suffix)) return;
    const auto k = key_for(suffix);
    try { out = split_target_activities(require(suffix)); }
    catch (const std::invalid_argument& e) {
      throw std::invalid_argument(k + ": " + e.what());
    }
    if (out.empty()) throw std::invalid_argument(k + ": expected a non-empty list");
  };
  parse_list("event_string_attributes", params.event_string_attributes);
  parse_list("event_numeric_attributes", params.event_numeric_attributes);
  parse_list("case_string_attributes", params.case_string_attributes);
  parse_list("case_numeric_attributes", params.case_numeric_attributes);
  parse_list("event_attributes", params.event_attributes);
  parse_list("case_attributes", params.case_attributes);
  parse_list("feature_names", params.feature_names);
  return params;
}

EmbeddingParams parse_embedding_params(const FieldLookup& field) {
  return parse_embedding_params(field, {});
}

EmbeddingParams parse_embedding_params(const FieldLookup& field,
                                       const FieldPresence& present) {
  EmbeddingParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("embedding_variant")) {
    const auto value = require("embedding_variant");
    if (value == "cases" || value == "cases_transformers" ||
        value == "CASES_TRANSFORMERS") {
      params.variant = EmbeddingVariant::CASES_TRANSFORMERS;
    } else if (value == "events" || value == "events_transformers" ||
               value == "EVENTS_TRANSFORMERS") {
      params.variant = EmbeddingVariant::EVENTS_TRANSFORMERS;
    } else {
      throw std::invalid_argument(
          "embedding_variant: expected cases or events");
    }
  }
  if (supplied("embedding_model"))
    params.embedding_model = require("embedding_model");
  if (supplied("embedding_attribute_key"))
    params.attribute_key = require("embedding_attribute_key");
  if (supplied("embedding_event_id_key"))
    params.event_id_key = require("embedding_event_id_key");
  if (supplied("embedding_dimensions"))
    params.dimensions = parse_positive_size(require("embedding_dimensions"),
                                            "embedding_dimensions");
  if (supplied("embedding_max_items"))
    params.max_items = parse_positive_size(require("embedding_max_items"),
                                           "embedding_max_items");
  if (supplied("embedding_keep_cases"))
    params.keep_cases = parse_boolean(require("embedding_keep_cases"),
                                      "embedding_keep_cases");
  return params;
}

LabelSplittingParams parse_label_splitting_params(const FieldLookup& field) {
  return parse_label_splitting_params(field, {});
}

LabelSplittingParams parse_label_splitting_params(const FieldLookup& field,
                                                  const FieldPresence& present) {
  LabelSplittingParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty()) throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("label_splitting_variant")) {
    const auto value = require("label_splitting_variant");
    if (value != "contextual") throw std::invalid_argument("label_splitting_variant: unknown variant '" + value + "'");
    params.variant = LabelSplittingVariant::CONTEXTUAL;
  }
  const auto assign_string = [&](std::string_view name, std::string& out) {
    if (supplied(name)) out = require(name);
  };
  assign_string("label_splitting_activity_key", params.activity_key);
  assign_string("label_splitting_target_key", params.target_key);
  assign_string("label_splitting_activities_suffix", params.activities_suffix);
  if (supplied("label_splitting_prefix_length"))
    params.prefix_length = parse_nonnegative_size(require("label_splitting_prefix_length"), "label_splitting_prefix_length");
  if (supplied("label_splitting_suffix_length"))
    params.suffix_length = parse_nonnegative_size(require("label_splitting_suffix_length"), "label_splitting_suffix_length");
  if (supplied("label_splitting_min_edge_weight"))
    params.min_edge_weight = parse_finite_double(require("label_splitting_min_edge_weight"), "label_splitting_min_edge_weight");
  if (supplied("label_splitting_target_activities")) {
    params.target_activities = split_target_activities(require("label_splitting_target_activities"));
    if (params.target_activities.empty()) throw std::invalid_argument("label_splitting_target_activities: expected a non-empty list");
    const std::set<std::string> unique(params.target_activities.begin(), params.target_activities.end());
    if (unique.size() != params.target_activities.size()) throw std::invalid_argument("label_splitting_target_activities: duplicate activity");
  }
  return params;
}

ILPMinerOptions parse_ilp_miner_params(const FieldLookup& field) {
  return parse_ilp_miner_params(field, {});
}

ILPMinerOptions parse_ilp_miner_params(const FieldLookup& field,
                                       const FieldPresence& present) {
  ILPMinerOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty()) {
      throw std::invalid_argument(std::string(name) + ": expected a value");
    }
    return value;
  };
  if (supplied("ilp_activity_key")) {
    options.activity_key = require("ilp_activity_key");
  }
  if (supplied("ilp_noise_threshold")) {
    const auto value = require("ilp_noise_threshold");
    options.noise_threshold = parse_finite_double(value, "ilp_noise_threshold");
    if (options.noise_threshold < 0.0 || options.noise_threshold > 1.0) {
      throw std::invalid_argument("ilp_noise_threshold must be in [0,1]");
    }
  }
  if (supplied("ilp_max_nodes")) {
    options.max_nodes = parse_positive_size(require("ilp_max_nodes"),
                                             "ilp_max_nodes");
  }
  return options;
}

GeneticMinerOptions parse_genetic_miner_params(const FieldLookup& field) {
  return parse_genetic_miner_params(field, {});
}

GeneticMinerOptions parse_genetic_miner_params(
    const FieldLookup& field, const FieldPresence& present) {
  GeneticMinerOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("genetic_activity_key"))
    options.activity_key = require("genetic_activity_key");
  if (supplied("genetic_seed"))
    options.seed = parse_nonnegative_u64(require("genetic_seed"),
                                         "genetic_seed");
  if (supplied("genetic_population_size")) {
    options.population_size = parse_positive_size(
        require("genetic_population_size"), "genetic_population_size");
    if (options.population_size < 2)
      throw std::invalid_argument(
          "genetic_population_size: expected at least 2");
  }
  if (supplied("genetic_generations"))
    options.generations = parse_nonnegative_size(
        require("genetic_generations"), "genetic_generations");
  const auto rate = [&](std::string_view name, double& destination) {
    if (!supplied(name)) return;
    destination = parse_finite_double(require(name), name);
    if (destination < 0.0 || destination > 1.0)
      throw std::invalid_argument(std::string(name) + ": expected 0 <= value <= 1");
  };
  rate("genetic_elitism_rate", options.elitism_rate);
  rate("genetic_crossover_rate", options.crossover_rate);
  rate("genetic_mutation_rate", options.mutation_rate);
  if (supplied("genetic_elitism_min_sample")) {
    options.elitism_min_sample = parse_positive_size(
        require("genetic_elitism_min_sample"),
        "genetic_elitism_min_sample");
  }
  if (supplied("genetic_max_activities")) {
    options.max_activities = parse_positive_size(
        require("genetic_max_activities"), "genetic_max_activities");
  }
  if (supplied("genetic_max_states")) {
    options.max_states = parse_positive_size(require("genetic_max_states"),
                                             "genetic_max_states");
  }
  if (options.elitism_min_sample >= options.population_size)
    throw std::invalid_argument(
        "genetic_elitism_min_sample: must be smaller than population size");
  return options;
}

TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field) {
  return parse_trace_variant_query_params(field, {});
}

TraceVariantQueryOptions parse_trace_variant_query_params(
    const FieldLookup& field, const FieldPresence& present) {
  TraceVariantQueryOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("tvq_variant")) {
    const auto value = require("tvq_variant");
    if (value == "laplace")
      options.variant = TraceVariantQueryVariant::kLaplace;
    else if (value == "sacofa")
      options.variant = TraceVariantQueryVariant::kSacofa;
    else
      throw std::invalid_argument("tvq_variant: expected laplace or sacofa");
  }
  if (supplied("tvq_epsilon")) {
    options.epsilon = parse_finite_double(require("tvq_epsilon"),
                                          "tvq_epsilon");
    if (options.epsilon <= 0.0)
      throw std::invalid_argument("tvq_epsilon must be positive");
  }
  if (supplied("tvq_k"))
    options.max_prefix_length = parse_nonnegative_size(
        require("tvq_k"), "tvq_k");
  if (supplied("tvq_p"))
    options.prune_threshold = static_cast<std::uint64_t>(
        parse_positive_size(require("tvq_p"), "tvq_p"));
  if (supplied("tvq_seed"))
    options.seed = static_cast<std::uint64_t>(
        parse_nonnegative_size(require("tvq_seed"), "tvq_seed"));
  if (supplied("tvq_max_output_traces"))
    options.max_output_traces = parse_nonnegative_size(
        require("tvq_max_output_traces"), "tvq_max_output_traces");
  if (supplied("tvq_max_output_variants"))
    options.max_output_variants = parse_nonnegative_size(
        require("tvq_max_output_variants"), "tvq_max_output_variants");
  return options;
}

PripelOptions parse_pripel_params(const FieldLookup& field) {
  return parse_pripel_params(field, {});
}

PripelOptions parse_pripel_params(const FieldLookup& field,
                                  const FieldPresence& present) {
  PripelOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("pripel_epsilon")) {
    options.epsilon = parse_finite_double(require("pripel_epsilon"),
                                          "pripel_epsilon");
    if (options.epsilon <= 0.0)
      throw std::invalid_argument("pripel_epsilon must be positive");
  }
  if (supplied("pripel_blocklist")) {
    options.blocklist = split_target_activities(require("pripel_blocklist"));
    if (options.blocklist.empty())
      throw std::invalid_argument("pripel_blocklist: expected labels");
    const std::set<std::string> unique(options.blocklist.begin(),
                                       options.blocklist.end());
    if (unique.size() != options.blocklist.size())
      throw std::invalid_argument("pripel_blocklist: duplicate label");
  }
  if (supplied("pripel_seed"))
    options.seed = static_cast<std::uint64_t>(
        parse_nonnegative_size(require("pripel_seed"), "pripel_seed"));
  if (supplied("pripel_max_output_traces"))
    options.max_output_traces = parse_nonnegative_size(
        require("pripel_max_output_traces"), "pripel_max_output_traces");
  if (supplied("pripel_max_distance"))
    options.max_distance = parse_nonnegative_size(
        require("pripel_max_distance"), "pripel_max_distance");
  return options;
}

OrganizationalSnaOptions parse_organizational_sna_params(
    const FieldLookup& field) {
  return parse_organizational_sna_params(field, {});
}

OrganizationalSnaOptions parse_organizational_sna_params(
    const FieldLookup& field, const FieldPresence& present) {
  OrganizationalSnaOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("sna_variant")) {
    const auto value = require("sna_variant");
    if (value == "handover") options.variant = OrganizationalSnaVariant::kHandover;
    else if (value == "working_together" || value == "working-together")
      options.variant = OrganizationalSnaVariant::kWorkingTogether;
    else if (value == "subcontracting")
      options.variant = OrganizationalSnaVariant::kSubcontracting;
    else if (value == "jointactivities" || value == "joint_activities" ||
             value == "joint-activities")
      options.variant = OrganizationalSnaVariant::kJointActivities;
    else
      throw std::invalid_argument(
          "sna_variant: expected handover, working_together, subcontracting, or jointactivities");
  }
  if (supplied("sna_beta")) {
    options.beta = parse_finite_double(require("sna_beta"), "sna_beta");
    if (options.beta < 0.0 || options.beta > 1.0)
      throw std::invalid_argument("sna_beta must be in [0,1]");
  }
  if (supplied("sna_n"))
    options.n = parse_positive_size(require("sna_n"), "sna_n");
  if (supplied("sna_metric_normalization"))
    options.metric_normalization =
        parse_boolean(require("sna_metric_normalization"),
                      "sna_metric_normalization");
  if (supplied("sna_max_resources"))
    options.max_resources =
        parse_positive_size(require("sna_max_resources"), "sna_max_resources");
  if (supplied("sna_max_edges"))
    options.max_edges = parse_positive_size(require("sna_max_edges"),
                                             "sna_max_edges");
  return options;
}

NetworkAnalysisOptions parse_network_analysis_params(const FieldLookup& field) {
  return parse_network_analysis_params(field, {});
}

NetworkAnalysisOptions parse_network_analysis_params(const FieldLookup& field,
                                                     const FieldPresence& present) {
  NetworkAnalysisOptions options;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (v.empty()) throw std::invalid_argument(std::string(n) + ": expected a value"); return v; };
  if (supplied("network_analysis_sorting_attribute")) options.sorting_attribute = require("network_analysis_sorting_attribute");
  if (supplied("network_analysis_timestamp_attribute")) options.timestamp_attribute = require("network_analysis_timestamp_attribute");
  if (supplied("network_analysis_source_node_attribute")) options.source_node_attribute = require("network_analysis_source_node_attribute");
  if (supplied("network_analysis_target_node_attribute")) options.target_node_attribute = require("network_analysis_target_node_attribute");
  if (supplied("network_analysis_edge_attribute")) options.edge_attribute = require("network_analysis_edge_attribute");
  if (supplied("network_analysis_edge_reference")) {
    const auto v = require("network_analysis_edge_reference");
    if (v == "_out") options.edge_reference = NetworkAnalysisEdgeReference::kOut;
    else if (v == "_in") options.edge_reference = NetworkAnalysisEdgeReference::kIn;
    else throw std::invalid_argument("network_analysis_edge_reference: expected _out or _in");
  }
  if (supplied("network_analysis_out_relation_attribute")) options.out_relation_attribute = require("network_analysis_out_relation_attribute");
  if (supplied("network_analysis_in_relation_attribute")) options.in_relation_attribute = require("network_analysis_in_relation_attribute");
  if (supplied("network_analysis_include_performance")) options.include_performance = parse_boolean(require("network_analysis_include_performance"), "network_analysis_include_performance");
  if (supplied("network_analysis_business_hours")) options.business_hours = parse_boolean(require("network_analysis_business_hours"), "network_analysis_business_hours");
  if (supplied("network_analysis_business_hour_slots")) options.business_hour_slots = parse_business_hour_slots(require("network_analysis_business_hour_slots"), "network_analysis_business_hour_slots");
  if (supplied("network_analysis_max_rows")) options.max_rows = parse_positive_size(require("network_analysis_max_rows"), "network_analysis_max_rows");
  if (supplied("network_analysis_max_values")) options.max_values = parse_positive_size(require("network_analysis_max_values"), "network_analysis_max_values");
  return options;
}

RoleDiscoveryOptions parse_role_discovery_params(const FieldLookup& field) {
  return parse_role_discovery_params(field, {});
}
RoleDiscoveryOptions parse_role_discovery_params(const FieldLookup& field,
                                                 const FieldPresence& present) {
  RoleDiscoveryOptions options;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (v.empty()) throw std::invalid_argument(std::string(n) + ": expected a value"); return v; };
  if (supplied("roles_threshold")) {
    options.threshold = parse_finite_double(require("roles_threshold"), "roles_threshold");
    if (options.threshold < 0 || options.threshold > 1) throw std::invalid_argument("roles_threshold must be in [0,1]");
  }
  if (supplied("roles_max_resources")) options.max_resources = parse_positive_size(require("roles_max_resources"), "roles_max_resources");
  if (supplied("roles_max_activities")) options.max_activities = parse_positive_size(require("roles_max_activities"), "roles_max_activities");
  return options;
}

ResourceProfilesOptions parse_resource_profiles_params(const FieldLookup& field) {
  return parse_resource_profiles_params(field, {});
}

ResourceProfilesOptions parse_resource_profiles_params(const FieldLookup& field,
                                                       const FieldPresence& present) {
  ResourceProfilesOptions options;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (v.empty()) throw std::invalid_argument(std::string(n) + ": expected a value"); return v; };
  if (supplied("resource_profiles_metric")) options.metric = resource_profile_metric_from_string(require("resource_profiles_metric"));
  if (supplied("resource_profiles_t1")) options.t1 = require("resource_profiles_t1");
  if (supplied("resource_profiles_t2")) options.t2 = require("resource_profiles_t2");
  if (supplied("resource_profiles_resource")) options.resource = require("resource_profiles_resource");
  if (supplied("resource_profiles_activity")) options.activity = require("resource_profiles_activity");
  if (supplied("resource_profiles_resource2")) options.resource2 = require("resource_profiles_resource2");
  if (supplied("resource_profiles_max_resources")) options.max_resources = parse_positive_size(require("resource_profiles_max_resources"), "resource_profiles_max_resources");
  if (supplied("resource_profiles_max_rows")) options.max_rows = parse_positive_size(require("resource_profiles_max_rows"), "resource_profiles_max_rows");
  if (options.metric == ResourceProfileMetric::kAverageWorkload && options.t2.empty()) throw std::invalid_argument("resource_profiles_t2: required for average_workload");
  if (options.metric == ResourceProfileMetric::kMultitasking && (options.t1.empty() || options.t2.empty())) throw std::invalid_argument("resource_profiles_t1/t2: required for multitasking");
  if (options.metric == ResourceProfileMetric::kInteractionTwoResources && (options.resource.empty() || options.resource2.empty())) throw std::invalid_argument("resource_profiles_resource/resource2: required for interaction_two_resources");
  if ((options.metric == ResourceProfileMetric::kActivityFrequency || options.metric == ResourceProfileMetric::kAverageDurationActivity) && options.activity.empty()) throw std::invalid_argument("resource_profiles_activity: required for selected metric");
  return options;
}

LocalDiagnosticsOptions parse_local_diagnostics_params(const FieldLookup& field) {
  return parse_local_diagnostics_params(field, {});
}

LocalDiagnosticsOptions parse_local_diagnostics_params(const FieldLookup& field,
                                                       const FieldPresence& present) {
  LocalDiagnosticsOptions options;
  const auto supplied = [&](std::string_view n) { return present ? present(n) : !field(n).empty(); };
  const auto require = [&](std::string_view n) { const auto v = field(n); if (v.empty()) throw std::invalid_argument(std::string(n) + ": expected a value"); return v; };
  if (supplied("local_diagnostics_group_attribute")) options.group_attribute = require("local_diagnostics_group_attribute");
  if (supplied("local_diagnostics_resource_attribute")) options.resource_attribute = require("local_diagnostics_resource_attribute");
  if (supplied("local_diagnostics_activity_attribute")) options.activity_attribute = require("local_diagnostics_activity_attribute");
  if (supplied("local_diagnostics_groups")) {
    const auto encoded = require("local_diagnostics_groups");
    const auto trim = [](std::string value) {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) return std::string{};
      const auto last = value.find_last_not_of(" \t\r\n");
      return value.substr(first, last - first + 1);
    };
    const auto strict_resources = [&](std::string_view value) {
      std::vector<std::string> resources;
      std::size_t start = 0;
      for (std::size_t i = 0; i <= value.size(); ++i) {
        if (i != value.size() && value[i] != ',') continue;
        const auto token = trim(std::string(value.substr(start, i - start)));
        if (token.empty()) throw std::invalid_argument("local_diagnostics_groups: empty resource");
        resources.push_back(token);
        start = i + 1;
      }
      return resources;
    };
    std::size_t pos = 0;
    while (pos < encoded.size()) {
      const auto end = encoded.find(';', pos); const auto part = encoded.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      if (trim(std::string(part)).empty()) throw std::invalid_argument("local_diagnostics_groups: empty group");
      const auto colon = part.find(':');
      if (colon == std::string::npos || colon == 0 || part.find(':', colon + 1) != std::string::npos) throw std::invalid_argument("local_diagnostics_groups: expected group:resource,...");
      const auto group = trim(part.substr(0, colon));
      const auto resources = strict_resources(part.substr(colon + 1));
      if (group.empty() || resources.empty() || options.explicit_groups.contains(group)) throw std::invalid_argument("local_diagnostics_groups: invalid or duplicate group");
      options.explicit_groups[group] = resources;
      if (end == std::string::npos) break;
      pos = end + 1;
      if (pos == encoded.size()) throw std::invalid_argument("local_diagnostics_groups: trailing separator");
    }
  }
  if (supplied("local_diagnostics_max_groups")) options.max_groups = parse_positive_size(require("local_diagnostics_max_groups"), "local_diagnostics_max_groups");
  if (supplied("local_diagnostics_max_resources")) options.max_resources = parse_positive_size(require("local_diagnostics_max_resources"), "local_diagnostics_max_resources");
  if (supplied("local_diagnostics_max_rows")) options.max_rows = parse_positive_size(require("local_diagnostics_max_rows"), "local_diagnostics_max_rows");
  return options;
}

TraceAttributeClusteringOptions parse_clustering_params(const FieldLookup& field) { return parse_clustering_params(field, {}); }
TraceAttributeClusteringOptions parse_clustering_params(const FieldLookup& field, const FieldPresence& present) {
  TraceAttributeClusteringOptions o;
  const auto supplied=[&](std::string_view n){return present?present(n):!field(n).empty();};
  const auto req=[&](std::string_view n){auto v=field(n); if(v.empty()) throw std::invalid_argument(std::string(n)+": expected a value"); return v;};
  if(supplied("clustering_trace_attribute")) o.trace_attribute=req("clustering_trace_attribute");
  if(supplied("clustering_variant")) { const auto v=req("clustering_variant");
    if(v=="DMM_LEVEN"||v=="dmm_leven") o.variant=TraceAttributeClusteringVariant::kDmmLeven;
    else if(v=="AVG_LEVEN"||v=="avg_leven") o.variant=TraceAttributeClusteringVariant::kAvgLeven;
    else if(v=="DMM_VEC"||v=="dmm_vec") o.variant=TraceAttributeClusteringVariant::kDmmVec;
    else if(v=="AVG_VEC"||v=="avg_vec") o.variant=TraceAttributeClusteringVariant::kAvgVec;
    else if(v=="DFG"||v=="dfg") o.variant=TraceAttributeClusteringVariant::kDfg;
    else throw std::invalid_argument("clustering_variant: unknown variant"); }
  if(supplied("clustering_alpha")) {o.alpha=parse_finite_double(req("clustering_alpha"),"clustering_alpha"); if(o.alpha<0||o.alpha>1) throw std::invalid_argument("clustering_alpha must be in [0,1]");}
  if(supplied("clustering_percent")) {o.percent=parse_finite_double(req("clustering_percent"),"clustering_percent"); if(o.percent<=0||o.percent>1) throw std::invalid_argument("clustering_percent must be in (0,1]");}
  if(supplied("clustering_max_groups")) o.max_groups=parse_positive_size(req("clustering_max_groups"),"clustering_max_groups");
  if(supplied("clustering_max_output_nodes")) o.max_output_nodes=parse_positive_size(req("clustering_max_output_nodes"),"clustering_max_output_nodes");
  if(o.trace_attribute.empty()) throw std::invalid_argument("clustering_trace_attribute: required");
  return o;
}

}  // namespace minera
