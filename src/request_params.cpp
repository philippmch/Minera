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
#include <utility>

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

std::vector<std::string> parse_strict_csv(std::string_view value,
                                          std::string_view name) {
  if (value.empty())
    throw std::invalid_argument(std::string(name) + ": expected a non-empty list");
  std::vector<std::string> result;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i != value.size() && value[i] != ',') continue;
    auto token = value.substr(start, i - start);
    const auto first = token.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
      throw std::invalid_argument(std::string(name) + ": values must not be empty");
    const auto last = token.find_last_not_of(" \t\r\n");
    result.emplace_back(token.substr(first, last - first + 1));
    start = i + 1;
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> parse_ilp_causal_relation(
    std::string_view value, std::string_view name) {
  const auto items = parse_strict_csv(value, name);
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(items.size());
  for (const auto& item : items) {
    const auto separator = item.find("=>");
    if (separator == std::string::npos || separator == 0 ||
        separator + 2 == item.size() ||
        item.find("=>", separator + 2) != std::string::npos) {
      throw std::invalid_argument(std::string(name) +
                                  ": expected A=>B causal edges");
    }
    const auto left = item.substr(0, separator);
    const auto right = item.substr(separator + 2);
    if (left.empty() || right.empty())
      throw std::invalid_argument(std::string(name) +
                                  ": causal edge labels must not be empty");
    result.emplace_back(left, right);
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

LtlFilterVariant parse_ltl_variant(std::string_view value) {
  if (value == "eventually_follows" || value == "eventually-follows")
    return LtlFilterVariant::kEventuallyFollows;
  if (value == "a_next_b_next_c" || value == "a-next-b-next-c")
    return LtlFilterVariant::kANextBNextC;
  if (value == "four_eyes" || value == "four_eyes_principle" ||
      value == "four-eyes" || value == "four-eyes-principle")
    return LtlFilterVariant::kFourEyes;
  if (value == "attr_value_different_persons" ||
      value == "attribute_value_different_persons" ||
      value == "attr-value-different-persons")
    return LtlFilterVariant::kAttributeValueDifferentPersons;
  throw std::invalid_argument(
      "ltl_variant: expected eventually_follows, a_next_b_next_c, four_eyes_principle, or attr_value_different_persons, got '" +
      std::string(value) + "'");
}

std::vector<std::pair<double, double>> parse_ltl_boundaries(
    std::string_view value, std::string_view name) {
  const auto items = parse_strict_csv(value, name);
  std::vector<std::pair<double, double>> result;
  result.reserve(items.size());
  for (const auto& item : items) {
    const auto sep = item.find(':');
    if (sep == std::string::npos || item.find(':', sep + 1) != std::string::npos ||
        sep == 0 || sep + 1 == item.size())
      throw std::invalid_argument(std::string(name) + ": expected min:max pairs");
    const double lower = parse_finite_double(item.substr(0, sep), name);
    const double upper = parse_finite_double(item.substr(sep + 1), name);
    if (lower > upper)
      throw std::invalid_argument(std::string(name) + ": bounds must be ordered");
    result.emplace_back(lower, upper);
  }
  return result;
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

std::vector<std::uint32_t> parse_uint32_marking(std::string_view value,
                                                std::string_view name) {
  const auto parsed = parse_marking(value, name);
  std::vector<std::uint32_t> result;
  result.reserve(parsed.size());
  for (const auto entry : parsed) {
    if (entry > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(std::string(name) +
                                  ": marking entry is out of range");
    result.push_back(static_cast<std::uint32_t>(entry));
  }
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
    } else if (value == "composite") {
      params.family = EvaluationFamily::kComposite;
    } else {
      throw std::invalid_argument(
          "evaluation_family: expected replay_fitness, precision, "
          "generalization, simplicity, or composite, got '" + value + "'");
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
  const auto parse_composite_weight = [&](std::string_view name,
                                          double& destination) {
    if (!supplied(name)) return;
    destination = parse_finite_double(require(name), name);
    if (destination < 0.0) {
      throw std::invalid_argument(std::string(name) +
                                  ": expected a non-negative number");
    }
  };
  parse_composite_weight("fitness_weight",
                         params.composite_options.fitness_weight);
  parse_composite_weight("precision_weight",
                         params.composite_options.precision_weight);
  parse_composite_weight("simplicity_weight",
                         params.composite_options.simplicity_weight);
  parse_composite_weight("generalization_weight",
                         params.composite_options.generalization_weight);
  if (supplied("fitness_weight") || supplied("precision_weight") ||
      supplied("simplicity_weight") || supplied("generalization_weight")) {
    const double total = params.composite_options.fitness_weight +
                         params.composite_options.precision_weight +
                         params.composite_options.simplicity_weight +
                         params.composite_options.generalization_weight;
    if (!std::isfinite(total) || total <= 0.0) {
      throw std::invalid_argument(
          "composite weights must have a positive total");
    }
  }
  return params;
}

MineParams parse_mine_params(const FieldLookup& field) {
  return parse_mine_params(field, FieldPresence{});
}

MineParams parse_mine_params(const FieldLookup& field,
                             const FieldPresence& present) {
  MineParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };

  const bool segment_prefix_supplied =
      supplied("segment_prefix_activity") || supplied("segment_prefix_strict") ||
      supplied("segment_prefix_occurrence");
  const bool segment_suffix_supplied =
      supplied("segment_suffix_activity") || supplied("segment_suffix_strict") ||
      supplied("segment_suffix_occurrence");
  const bool segment_between_supplied =
      supplied("segment_between_starts") || supplied("segment_between_ends") ||
      supplied("segment_between_separator");
  if (segment_prefix_supplied || segment_suffix_supplied || segment_between_supplied) {
    SegmentFilterOptions options;
    const auto parse_occurrence = [&](std::string_view name) {
      const auto value = field(name);
      if (value == "first") return AnchorOccurrence::first;
      if (value == "last") return AnchorOccurrence::last;
      throw std::invalid_argument(std::string(name) + ": expected first or last");
    };
    if (segment_prefix_supplied) {
      if (!supplied("segment_prefix_activity") || field("segment_prefix_activity").empty())
        throw std::invalid_argument("segment_prefix_activity: required when segment prefix filtering is enabled");
      options.prefix.activity = field("segment_prefix_activity");
      if (supplied("segment_prefix_strict"))
        options.prefix.strict = parse_boolean(field("segment_prefix_strict"), "segment_prefix_strict");
      if (supplied("segment_prefix_occurrence"))
        options.prefix.occurrence = parse_occurrence("segment_prefix_occurrence");
      options.use_prefix = true;
    }
    if (segment_suffix_supplied) {
      if (!supplied("segment_suffix_activity") || field("segment_suffix_activity").empty())
        throw std::invalid_argument("segment_suffix_activity: required when segment suffix filtering is enabled");
      options.suffix.activity = field("segment_suffix_activity");
      if (supplied("segment_suffix_strict"))
        options.suffix.strict = parse_boolean(field("segment_suffix_strict"), "segment_suffix_strict");
      if (supplied("segment_suffix_occurrence"))
        options.suffix.occurrence = parse_occurrence("segment_suffix_occurrence");
      options.use_suffix = true;
    }
    if (segment_between_supplied) {
      if (!supplied("segment_between_starts") || !supplied("segment_between_ends"))
        throw std::invalid_argument("segment_between_starts and segment_between_ends: both are required");
      options.between.start_activities = parse_strict_csv(field("segment_between_starts"), "segment_between_starts");
      options.between.end_activities = parse_strict_csv(field("segment_between_ends"), "segment_between_ends");
      if (options.between.start_activities.empty() || options.between.end_activities.empty())
        throw std::invalid_argument("segment_between_starts/ends: at least one activity is required");
      if (supplied("segment_between_separator")) {
        options.between.case_id_separator = field("segment_between_separator");
        if (options.between.case_id_separator.empty())
          throw std::invalid_argument("segment_between_separator: must not be empty");
      }
      options.use_between = true;
    }
    params.segment = std::move(options);
    params.segment_enabled = true;
  }

  // Optional adjacent-path predicate. Presence of any path option opts in;
  // both predecessor and successor activities are required and all supplied
  // bounds/booleans are validated strictly.
  const bool path_supplied = supplied("path_predecessor") ||
                             supplied("path_successor") ||
                             supplied("path_min_performance") ||
                             supplied("path_max_performance") ||
                             supplied("path_positive") ||
                             supplied("path_activity_attribute") ||
                             supplied("path_timestamp_attribute");
  if (path_supplied) {
    AdjacentPathFilterOptions options;
    if (!supplied("path_predecessor") || field("path_predecessor").empty())
      throw std::invalid_argument(
          "path_predecessor: required when path filtering is enabled");
    if (!supplied("path_successor") || field("path_successor").empty())
      throw std::invalid_argument(
          "path_successor: required when path filtering is enabled");
    options.predecessor = field("path_predecessor");
    options.successor = field("path_successor");
    if (supplied("path_activity_attribute")) {
      options.activity_attribute = field("path_activity_attribute");
      if (options.activity_attribute.empty())
        throw std::invalid_argument(
            "path_activity_attribute: must not be empty when supplied");
    }
    if (supplied("path_timestamp_attribute")) {
      options.timestamp_attribute = field("path_timestamp_attribute");
      if (options.timestamp_attribute.empty())
        throw std::invalid_argument(
            "path_timestamp_attribute: must not be empty when supplied");
    }
    if (supplied("path_min_performance")) {
      options.min_elapsed_seconds = parse_finite_double(
          field("path_min_performance"), "path_min_performance");
    }
    if (supplied("path_max_performance")) {
      options.max_elapsed_seconds = parse_finite_double(
          field("path_max_performance"), "path_max_performance");
    }
    if (options.max_elapsed_seconds &&
        options.min_elapsed_seconds.value_or(0.0) >
            *options.max_elapsed_seconds)
      throw std::invalid_argument(
          "path_min_performance: must be less than or equal to path_max_performance");
    if (supplied("path_positive"))
      options.positive = parse_boolean(field("path_positive"), "path_positive");
    params.path = std::move(options);
    params.path_enabled = true;
  }

  // Optional bounded temporal predicate. Presence of any ltl_* field opts in;
  // all supplied values are then parsed strictly so malformed requests fail
  // before a potentially surprising discovery run.
  const bool ltl_supplied = supplied("ltl_variant") ||
                            supplied("ltl_attribute_key") ||
                            supplied("ltl_attribute_values") ||
                            supplied("ltl_positive") ||
                            supplied("ltl_enable_timestamp") ||
                            supplied("ltl_timestamp_diff_boundaries") ||
                            supplied("ltl_resource_key");
  if (ltl_supplied) {
    LtlFilterOptions options;
    if (supplied("ltl_variant"))
      options.variant = parse_ltl_variant(field("ltl_variant"));
    if (!supplied("ltl_attribute_values"))
      throw std::invalid_argument("ltl_attribute_values: field is required when LTL filtering is enabled");
    options.attribute_values =
        parse_strict_csv(field("ltl_attribute_values"), "ltl_attribute_values");
    if (supplied("ltl_attribute_key")) options.attribute_key = field("ltl_attribute_key");
    if (supplied("ltl_resource_key")) options.resource_key = field("ltl_resource_key");
    if (supplied("ltl_positive"))
      options.positive = parse_boolean(field("ltl_positive"), "ltl_positive");
    if (supplied("ltl_enable_timestamp"))
      options.enable_timestamp = parse_boolean(field("ltl_enable_timestamp"), "ltl_enable_timestamp");
    if (supplied("ltl_timestamp_diff_boundaries"))
      options.timestamp_diff_boundaries = parse_ltl_boundaries(
          field("ltl_timestamp_diff_boundaries"), "ltl_timestamp_diff_boundaries");
    if (options.variant != LtlFilterVariant::kEventuallyFollows &&
        (options.enable_timestamp || !options.timestamp_diff_boundaries.empty()))
      throw std::invalid_argument(
          "ltl_timestamp_diff_boundaries: timestamp bounds only apply to eventually_follows");
    if (options.variant == LtlFilterVariant::kEventuallyFollows &&
        options.enable_timestamp &&
        options.timestamp_diff_boundaries.size() != options.attribute_values.size() - 1)
      throw std::invalid_argument(
          "ltl_timestamp_diff_boundaries: expected one min:max pair per adjacent value pair");
    if (!options.enable_timestamp && !options.timestamp_diff_boundaries.empty())
      throw std::invalid_argument(
          "ltl_timestamp_diff_boundaries: requires ltl_enable_timestamp=true");
    const auto expected = options.variant == LtlFilterVariant::kEventuallyFollows ? 0u
                         : options.variant == LtlFilterVariant::kANextBNextC ? 3u
                         : options.variant == LtlFilterVariant::kFourEyes ? 2u : 1u;
    if ((expected == 0 && options.attribute_values.empty()) ||
        (expected != 0 && options.attribute_values.size() != expected))
      throw std::invalid_argument("ltl_attribute_values: wrong number of values for selected variant");
    params.ltl = std::move(options);
    params.ltl_enabled = true;
  }

  // Optional overlap statistics.  The explicit boolean is the opt-in gate;
  // dependent fields are still parsed when present so malformed requests are
  // rejected deterministically before mining starts.
  const bool overlap_supplied = supplied("overlap") ||
                                supplied("overlap_activity_attribute") ||
                                supplied("overlap_start_attribute") ||
                                supplied("overlap_completion_attribute") ||
                                supplied("overlap_epsilon") ||
                                supplied("overlap_strict");
  if (overlap_supplied) {
    if (supplied("overlap"))
      params.overlap = parse_boolean(field("overlap"), "overlap");
    if (supplied("overlap_start_attribute"))
      params.overlap_options.start_timestamp_attribute =
          field("overlap_start_attribute");
    if (supplied("overlap_activity_attribute"))
      params.overlap_options.activity_attribute =
          field("overlap_activity_attribute");
    if (supplied("overlap_completion_attribute"))
      params.overlap_options.completion_timestamp_attribute =
          field("overlap_completion_attribute");
    if (supplied("overlap_epsilon")) {
      params.overlap_options.epsilon =
          parse_finite_double(field("overlap_epsilon"), "overlap_epsilon");
      if (params.overlap_options.epsilon < 0.0)
        throw std::invalid_argument(
            "overlap_epsilon: expected a finite non-negative number");
    }
    if (supplied("overlap_strict"))
      params.overlap_options.strict =
          parse_boolean(field("overlap_strict"), "overlap_strict");
    // Any dependent overlap field is an explicit request as well, preserving
    // useful form semantics when clients omit the checkbox value.
    if (!supplied("overlap")) params.overlap = true;
  }

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
  const bool case_duration_min_supplied =
      supplied("case_duration_min") || supplied("min_case_performance");
  const bool case_duration_max_supplied =
      supplied("case_duration_max") || supplied("max_case_performance");
  if (case_duration_min_supplied || case_duration_max_supplied) {
    const auto duration_field = [&](std::string_view primary,
                                    std::string_view alias) {
      return supplied(primary) ? primary : alias;
    };
    if (case_duration_min_supplied) {
      const auto name = duration_field("case_duration_min", "min_case_performance");
      params.filter.min_case_duration_seconds =
          parse_finite_double(field(name), name);
    }
    if (case_duration_max_supplied) {
      const auto name = duration_field("case_duration_max", "max_case_performance");
      params.filter.max_case_duration_seconds =
          parse_finite_double(field(name), name);
    }
    if (params.filter.max_case_duration_seconds &&
        params.filter.min_case_duration_seconds.value_or(0.0) >
            *params.filter.max_case_duration_seconds) {
      throw std::invalid_argument(
          "case_duration_min: must not exceed case_duration_max");
    }
  }
  const bool max_cases_supplied = supplied("max_cases") ||
                                  supplied("max_no_cases") ||
                                  supplied("case_limit");
  if (max_cases_supplied) {
    const auto max_cases_field = supplied("max_cases")
                                     ? std::string_view("max_cases")
                                     : supplied("max_no_cases")
                                           ? std::string_view("max_no_cases")
                                           : std::string_view("case_limit");
    params.filter.max_cases = parse_nonnegative_size(
        field(max_cases_field), max_cases_field);
  }

  // Frequency-based activity/attribute projections.  The maximum-activity
  // field accepts the common aliases so old clients can opt into the same
  // deterministic top-value filter.  Relative occurrence defaults to the
  // activity field and counts each value once per case unless requested
  // otherwise.
  const bool top_activities_supplied =
      supplied("max_activities") || supplied("max_no_activities") ||
      supplied("top_activity_limit") ||
      supplied("top_activities_attribute_key");
  if (top_activities_supplied) {
    const auto limit_field = supplied("max_activities")
                                 ? std::string_view("max_activities")
                                 : supplied("max_no_activities")
                                       ? std::string_view("max_no_activities")
                                       : std::string_view("top_activity_limit");
    if (!supplied(limit_field)) {
      throw std::invalid_argument(
          "max_activities: a non-negative activity limit is required");
    }
    TopActivitiesFilterOptions top;
    top.max_activities = parse_nonnegative_size(
        field(limit_field), limit_field);
    if (supplied("top_activities_attribute_key"))
      top.attribute_key = field("top_activities_attribute_key");
    params.filter.top_activities = std::move(top);
  }

  const bool relative_occurrence_supplied =
      supplied("relative_attribute_key") ||
      supplied("relative_attribute_stake") ||
      supplied("relative_attribute_keep_once_per_case") ||
      supplied("relative_attribute_keep_once");
  if (relative_occurrence_supplied) {
    RelativeAttributeOccurrenceFilterOptions relative;
    if (supplied("relative_attribute_key"))
      relative.attribute_key = field("relative_attribute_key");
    if (supplied("relative_attribute_stake")) {
      relative.min_relative_stake = parse_finite_double(
          field("relative_attribute_stake"), "relative_attribute_stake");
      if (relative.min_relative_stake < 0.0 ||
          relative.min_relative_stake > 1.0) {
        throw std::invalid_argument(
            "relative_attribute_stake: expected a number in [0, 1]");
      }
    }
    const auto keep_field = supplied("relative_attribute_keep_once_per_case")
                                ? std::string_view(
                                      "relative_attribute_keep_once_per_case")
                                : std::string_view("relative_attribute_keep_once");
    if (supplied(keep_field)) {
      relative.keep_once_per_case = parse_boolean(
          field(keep_field), keep_field);
    }
    params.filter.relative_attribute_occurrence = std::move(relative);
  }
  const bool threshold_key_supplied =
      supplied("attribute_occurrence_key") || supplied("attribute_threshold_key");
  const bool threshold_min_supplied =
      supplied("attribute_occurrence_min") || supplied("attribute_threshold");
  if (threshold_key_supplied || threshold_min_supplied) {
    const auto key_field = supplied("attribute_occurrence_key")
                               ? std::string_view("attribute_occurrence_key")
                               : std::string_view("attribute_threshold_key");
    const auto min_field = supplied("attribute_occurrence_min")
                               ? std::string_view("attribute_occurrence_min")
                               : std::string_view("attribute_threshold");
    if (!threshold_min_supplied)
      throw std::invalid_argument(
          "attribute_occurrence_min: required for threshold filtering");
    AttributeOccurrenceThresholdFilterOptions threshold;
    if (threshold_key_supplied) threshold.attribute_key = field(key_field);
    threshold.min_occurrences = parse_nonnegative_size(
        field(min_field), min_field);
    params.filter.attribute_occurrence_threshold = std::move(threshold);
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
  const bool numeric_attribute_supplied =
      supplied("numeric_attribute_key") || supplied("numeric_attribute_min") ||
      supplied("numeric_attribute_max") || supplied("numeric_attribute_mode") ||
      supplied("numeric_attribute_positive");
  if (numeric_attribute_supplied) {
    NumericAttributeFilterOptions numeric;
    if (!supplied("numeric_attribute_key") ||
        field("numeric_attribute_key").empty()) {
      throw std::invalid_argument(
          "numeric_attribute_key: required when numeric filtering is enabled");
    }
    numeric.attribute_key = field("numeric_attribute_key");
    if (supplied("numeric_attribute_min")) {
      numeric.min_value = parse_finite_double(
          field("numeric_attribute_min"), "numeric_attribute_min");
    }
    if (supplied("numeric_attribute_max")) {
      numeric.max_value = parse_finite_double(
          field("numeric_attribute_max"), "numeric_attribute_max");
    }
    if (!numeric.min_value && !numeric.max_value) {
      throw std::invalid_argument(
          "numeric_attribute_min/max: at least one bound is required");
    }
    if (numeric.max_value && numeric.min_value &&
        *numeric.min_value > *numeric.max_value) {
      throw std::invalid_argument(
          "numeric_attribute_min: must not exceed numeric_attribute_max");
    }
    if (supplied("numeric_attribute_mode")) {
      const auto mode = parse_attribute_filter_mode(
          field("numeric_attribute_mode"));
      if (!mode) {
        throw std::invalid_argument(
            "numeric_attribute_mode: expected events or traces");
      }
      numeric.mode = *mode;
    }
    if (supplied("numeric_attribute_positive")) {
      numeric.positive = parse_boolean(field("numeric_attribute_positive"),
                                       "numeric_attribute_positive");
    }
    params.filter.numeric_attribute = std::move(numeric);
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
  const bool trace_patterns_supplied =
      supplied("trace_patterns") || supplied("trace_pattern") ||
      supplied("trace_pattern_positive");
  if (trace_patterns_supplied) {
    const std::string_view pattern_field =
        supplied("trace_patterns") ? "trace_patterns" : "trace_pattern";
    if (!supplied(pattern_field) || field(pattern_field).empty()) {
      throw std::invalid_argument(
          "trace_patterns: at least one pattern is required when trace-pattern filtering is enabled");
    }
    params.filter.trace_patterns = parse_trace_patterns(field(pattern_field));
    if (supplied("trace_pattern_positive")) {
      params.filter.trace_pattern_positive = parse_boolean(
          field("trace_pattern_positive"), "trace_pattern_positive");
    }
  }
  const bool admitted_variants_supplied =
      supplied("admitted_variants") || supplied("admitted_variants_positive");
  if (admitted_variants_supplied) {
    if (!supplied("admitted_variants") || field("admitted_variants").empty()) {
      throw std::invalid_argument(
          "admitted_variants: at least one exact variant is required");
    }
    params.filter.admitted_variants =
        parse_admitted_variants(field("admitted_variants"));
    if (supplied("admitted_variants_positive")) {
      params.filter.admitted_variants_positive = parse_boolean(
          field("admitted_variants_positive"), "admitted_variants_positive");
    }
  }
  const auto parse_filter_anchor_occurrence = [&](std::string_view name) {
    const auto value = field(name);
    if (value == "first") return FilterAnchorOccurrence::first;
    if (value == "last") return FilterAnchorOccurrence::last;
    throw std::invalid_argument(std::string(name) + ": expected first or last");
  };
  const bool prefix_supplied = supplied("prefix_activity") ||
                               supplied("prefix_strict") ||
                               supplied("prefix_occurrence");
  if (prefix_supplied) {
    if (!supplied("prefix_activity") || field("prefix_activity").empty())
      throw std::invalid_argument(
          "prefix_activity: required when prefix filtering is enabled");
    params.filter.prefix_activity = field("prefix_activity");
    if (supplied("prefix_strict"))
      params.filter.prefix_strict =
          parse_boolean(field("prefix_strict"), "prefix_strict");
    if (supplied("prefix_occurrence"))
      params.filter.prefix_occurrence =
          parse_filter_anchor_occurrence("prefix_occurrence");
  }
  const bool suffix_supplied = supplied("suffix_activity") ||
                               supplied("suffix_strict") ||
                               supplied("suffix_occurrence");
  if (suffix_supplied) {
    if (!supplied("suffix_activity") || field("suffix_activity").empty())
      throw std::invalid_argument(
          "suffix_activity: required when suffix filtering is enabled");
    params.filter.suffix_activity = field("suffix_activity");
    if (supplied("suffix_strict"))
      params.filter.suffix_strict =
          parse_boolean(field("suffix_strict"), "suffix_strict");
    if (supplied("suffix_occurrence"))
      params.filter.suffix_occurrence =
          parse_filter_anchor_occurrence("suffix_occurrence");
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
  const bool activity_split_supplied =
      supplied("activity_split") || supplied("activity_split_activities") ||
      supplied("activity_split_separator") ||
      supplied("activity_split_cut_mode");
  if (activity_split_supplied) {
    const auto activity_field = supplied("activity_split")
                                    ? std::string_view("activity_split")
                                    : std::string_view("activity_split_activities");
    if (!supplied(activity_field) || field(activity_field).empty()) {
      throw std::invalid_argument(
          "activity_split: at least one activity is required");
    }
    params.filter.activity_split_activities =
        parse_strict_csv(field(activity_field), "activity_split");
    if (supplied("activity_split_separator")) {
      params.filter.activity_split_separator = field("activity_split_separator");
      if (params.filter.activity_split_separator.empty()) {
        throw std::invalid_argument(
            "activity_split_separator: must not be empty");
      }
    }
    if (supplied("activity_split_cut_mode")) {
      const auto mode = field("activity_split_cut_mode");
      if (mode == "this") {
        params.filter.activity_split_cut_mode =
            ActivitySplitCutMode::kThis;
      } else if (mode == "next") {
        params.filter.activity_split_cut_mode =
            ActivitySplitCutMode::kNext;
      } else {
        throw std::invalid_argument(
            "activity_split_cut_mode: expected this or next");
      }
    }
  }
  const bool repetition_value_supplied =
      supplied("repetition_value") || supplied("attribute_value_repetition") ||
      supplied("attr_value_repetition");
  const bool repetition_supplied =
      repetition_value_supplied || supplied("repetition_attribute_key") ||
      supplied("repetition_min") || supplied("repetition_max");
  if (repetition_supplied) {
    std::string_view value_field;
    if (supplied("repetition_value")) {
      value_field = "repetition_value";
    } else if (supplied("attribute_value_repetition")) {
      value_field = "attribute_value_repetition";
    } else if (supplied("attr_value_repetition")) {
      value_field = "attr_value_repetition";
    } else {
      throw std::invalid_argument(
          "repetition_value: required when repetition filtering is enabled");
    }
    params.filter.attribute_value_repetition =
        AttributeValueRepetitionOptions{};
    auto& repetition = *params.filter.attribute_value_repetition;
    repetition.value = field(value_field);
    if (supplied("repetition_attribute_key"))
      repetition.attribute_key = field("repetition_attribute_key");
    if (supplied("repetition_min"))
      repetition.min_repetitions =
          parse_nonnegative_u64(field("repetition_min"), "repetition_min");
    if (supplied("repetition_max"))
      repetition.max_repetitions =
          parse_nonnegative_u64(field("repetition_max"), "repetition_max");
    if (repetition.min_repetitions > repetition.max_repetitions) {
      throw std::invalid_argument(
          "repetition_min: must not exceed repetition_max");
    }
  }
  const bool consecutive_grouping_supplied =
      supplied("consecutive_case_grouping") ||
      supplied("consecutive_case_grouping_type") ||
      supplied("consecutive_case_grouping_activity_key") ||
      supplied("consecutive_grouping");
  if (consecutive_grouping_supplied) {
    params.filter.consecutive_activity_grouping =
        ConsecutiveActivityGroupingOptions{};
    auto& grouping = *params.filter.consecutive_activity_grouping;
    const auto type = supplied("consecutive_case_grouping")
                          ? field("consecutive_case_grouping")
                          : supplied("consecutive_case_grouping_type")
                                ? field("consecutive_case_grouping_type")
                                : supplied("consecutive_grouping")
                                      ? field("consecutive_grouping")
                                      : std::string("first");
    if (type == "first") {
      grouping.filter_type = CaseGroupingFilterType::kFirst;
    } else if (type == "last") {
      grouping.filter_type = CaseGroupingFilterType::kLast;
    } else {
      throw std::invalid_argument(
          "consecutive_case_grouping: expected first or last");
    }
    if (supplied("consecutive_case_grouping_activity_key"))
      grouping.activity_key = field("consecutive_case_grouping_activity_key");
  }
  const bool timestamp_grouping_supplied =
      supplied("timestamp_case_grouping") ||
      supplied("timestamp_case_grouping_type") ||
      supplied("timestamp_case_grouping_activity_key") ||
      supplied("timestamp_case_grouping_timestamp_key") ||
      supplied("timestamp_case_grouping_separator") ||
      supplied("timestamp_grouping");
  if (timestamp_grouping_supplied) {
    params.filter.timestamp_case_grouping = TimestampCaseGroupingOptions{};
    auto& grouping = *params.filter.timestamp_case_grouping;
    const auto type = supplied("timestamp_case_grouping")
                          ? field("timestamp_case_grouping")
                          : supplied("timestamp_case_grouping_type")
                                ? field("timestamp_case_grouping_type")
                                : supplied("timestamp_grouping")
                                      ? field("timestamp_grouping")
                                      : std::string("first");
    if (type == "first") {
      grouping.filter_type = TimestampCaseGroupingType::kFirst;
    } else if (type == "last") {
      grouping.filter_type = TimestampCaseGroupingType::kLast;
    } else if (type == "concat") {
      grouping.filter_type = TimestampCaseGroupingType::kConcat;
    } else {
      throw std::invalid_argument(
          "timestamp_case_grouping: expected first, last, or concat");
    }
    if (supplied("timestamp_case_grouping_activity_key"))
      grouping.activity_key = field("timestamp_case_grouping_activity_key");
    if (supplied("timestamp_case_grouping_timestamp_key"))
      grouping.timestamp_key = field("timestamp_case_grouping_timestamp_key");
    if (supplied("timestamp_case_grouping_separator")) {
      grouping.concat_separator = field("timestamp_case_grouping_separator");
      if (grouping.concat_separator.empty())
        throw std::invalid_argument(
            "timestamp_case_grouping_separator: must not be empty");
    }
  }
  const bool start_activities_supplied =
      supplied("start_activities") || supplied("start_activities_positive");
  if (start_activities_supplied) {
    if (!supplied("start_activities") || field("start_activities").empty()) {
      throw std::invalid_argument(
          "start_activities: required when endpoint polarity is supplied");
    }
    params.filter.start_activities = split_csv(field("start_activities"));
    if (supplied("start_activities_positive")) {
      params.filter.start_activities_positive = parse_boolean(
          field("start_activities_positive"), "start_activities_positive");
    }
  }
  const bool end_activities_supplied =
      supplied("end_activities") || supplied("end_activities_positive");
  if (end_activities_supplied) {
    if (!supplied("end_activities") || field("end_activities").empty()) {
      throw std::invalid_argument(
          "end_activities: required when endpoint polarity is supplied");
    }
    params.filter.end_activities = split_csv(field("end_activities"));
    if (supplied("end_activities_positive")) {
      params.filter.end_activities_positive = parse_boolean(
          field("end_activities_positive"), "end_activities_positive");
    }
  }
  if (supplied("min_length")) {
    params.filter.min_length =
        parse_nonnegative_size(field("min_length"), "min_length");
  }
  if (supplied("max_length")) {
    params.filter.max_length =
        parse_nonnegative_size(field("max_length"), "max_length");
  }
  if (params.filter.max_length != 0 &&
      params.filter.min_length > params.filter.max_length) {
    throw std::invalid_argument("min_length: must not exceed max_length");
  }
  if (const auto value = field("min_variant_frequency"); !value.empty()) {
    params.filter.min_variant_frequency =
        static_cast<std::uint64_t>(std::atoll(value.c_str()));
  }
  if (const auto value = field("top_variants"); !value.empty()) {
    params.filter.top_variants = static_cast<std::size_t>(std::atoll(value.c_str()));
  }
  if (supplied("variant_coverage")) {
    params.filter.variant_coverage =
        parse_finite_double(field("variant_coverage"), "variant_coverage");
    if (params.filter.variant_coverage < 0.0 ||
        params.filter.variant_coverage > 1.0)
      throw std::invalid_argument(
          "variant_coverage: expected a number in [0, 1]");
  }
  const bool variant_min_supplied = supplied("variant_min_coverage");
  const bool variant_max_supplied = supplied("variant_max_coverage");
  const bool variant_percentage_supplied = supplied("variant_percentage");
  if (variant_min_supplied) {
    params.filter.variant_min_coverage = parse_finite_double(
        field("variant_min_coverage"), "variant_min_coverage");
    if (*params.filter.variant_min_coverage < 0.0 ||
        *params.filter.variant_min_coverage > 1.0)
      throw std::invalid_argument(
          "variant_min_coverage: expected a number in [0, 1]");
  }
  if (variant_max_supplied) {
    params.filter.variant_max_coverage = parse_finite_double(
        field("variant_max_coverage"), "variant_max_coverage");
    if (*params.filter.variant_max_coverage < 0.0 ||
        *params.filter.variant_max_coverage > 1.0)
      throw std::invalid_argument(
          "variant_max_coverage: expected a number in [0, 1]");
  }
  if (variant_percentage_supplied) {
    params.filter.variant_percentage = parse_finite_double(
        field("variant_percentage"), "variant_percentage");
    if (*params.filter.variant_percentage < 0.0 ||
        *params.filter.variant_percentage > 1.0)
      throw std::invalid_argument(
          "variant_percentage: expected a number in [0, 1]");
  }
  if (params.filter.variant_min_coverage &&
      params.filter.variant_max_coverage &&
      *params.filter.variant_min_coverage >
          *params.filter.variant_max_coverage) {
    throw std::invalid_argument(
        "variant_min_coverage: must not exceed variant_max_coverage");
  }
  if (const auto value = field("noise_threshold"); !value.empty()) {
    params.inductive.noise_threshold = std::atof(value.c_str());
  }
  if (const auto value = field("inductive_variant"); !value.empty()) {
    if (value != "log" && value != "imf" && value != "dfg") {
      throw std::invalid_argument(
          "inductive_variant: expected log, imf, or dfg, got '" + value + "'");
    }
    params.inductive.variant = value == "dfg"
                                   ? InductiveMinerVariant::kDfg
                                   : value == "imf" ? InductiveMinerVariant::kImf
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
      {"entire_event_log", "entire_dataframe", "polars_lazyframes",
       "trace_by_trace", "dfg", "process_tree", "powl",
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
                          std::string_view{"spectrum_sort_log"},
                          std::string_view{"spectrum_activity_key"},
                          std::string_view{"spectrum_timestamp_key"},
                          std::string_view{"spectrum_case_id_key"}}) {
    if (spectrum_option_supplied(name) && !spectrum_supplied()) {
      throw std::invalid_argument(std::string(name) +
                                  ": requires spectrum_activities");
    }
  }
  if (const auto value = field("spectrum_variant");
      present ? present("spectrum_variant") : !value.empty()) {
    if (value == "connected" || value == "CONNECTED") {
      params.spectrum.variant = PerformanceSpectrumVariant::kConnected;
    } else if (value == "disconnected" || value == "DISCONNECTED") {
      params.spectrum.variant = PerformanceSpectrumVariant::kDisconnected;
    } else if (value == "dataframe" || value == "DATAFRAME") {
      params.spectrum.variant = PerformanceSpectrumVariant::kDataframe;
    } else if (value == "log" || value == "LOG") {
      params.spectrum.variant = PerformanceSpectrumVariant::kLog;
    } else if (value == "dataframe_disconnected" ||
               value == "DATAFRAME_DISCONNECTED") {
      params.spectrum.variant = PerformanceSpectrumVariant::kDataframeDisconnected;
    } else if (value == "log_disconnected" ||
               value == "LOG_DISCONNECTED") {
      params.spectrum.variant = PerformanceSpectrumVariant::kLogDisconnected;
    } else {
      throw std::invalid_argument(
          "spectrum_variant: expected connected, disconnected, dataframe, log, dataframe_disconnected, or log_disconnected, got '" +
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
  for (const auto [name, target] : {
           std::pair<std::string_view, std::string*>{"spectrum_activity_key",
                                                     &params.spectrum.activity_attribute},
           std::pair<std::string_view, std::string*>{"spectrum_timestamp_key",
                                                     &params.spectrum.timestamp_attribute},
           std::pair<std::string_view, std::string*>{"spectrum_case_id_key",
                                                     &params.spectrum.case_id_attribute}}) {
    if (spectrum_option_supplied(name)) {
      const auto value = field(name);
      if (value.empty())
        throw std::invalid_argument(std::string(name) + ": expected a value");
      *target = value;
    }
  }
  if (const auto value = field("batches");
      present ? present("batches") : !value.empty()) {
    params.batches = parse_boolean(value, "batches");
  }
  if (const auto value = field("batch_variant");
      present ? present("batch_variant") : !value.empty()) {
    if (value == "log" || value == "LOG") {
      params.batch_options.variant = BatchDiscoveryVariant::kLog;
    } else if (value == "pandas" || value == "PANDAS") {
      params.batch_options.variant = BatchDiscoveryVariant::kPandas;
    } else if (value == "polars" || value == "POLARS") {
      params.batch_options.variant = BatchDiscoveryVariant::kPolars;
    } else {
      throw std::invalid_argument(
          "batch_variant: expected log, pandas, or polars, got '" + value + "'");
    }
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
  if (const auto value = field("minimum_self_distance_variant");
      present ? present("minimum_self_distance_variant") : !value.empty()) {
    if (value == "log" || value == "LOG") {
      params.minimum_self_distance.variant =
          MinimumSelfDistanceVariant::kLog;
    } else if (value == "pandas" || value == "PANDAS") {
      params.minimum_self_distance.variant =
          MinimumSelfDistanceVariant::kPandas;
    } else if (value == "polars" || value == "POLARS") {
      params.minimum_self_distance.variant =
          MinimumSelfDistanceVariant::kPolars;
    } else {
      throw std::invalid_argument(
          "minimum_self_distance_variant: expected log, pandas, or polars");
    }
  }
  const auto batches_supplied = present ? present("batches") : !field("batches").empty();
  for (const auto name : {std::string_view{"batch_variant"},
                          std::string_view{"batch_merge_distance_seconds"},
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
    else if (value == "woflan")
      params.family = PetriAnalysisFamily::kWoflan;
    else
      throw std::invalid_argument(
          "petri_analysis_family: expected marking_equation, "
          "extended_marking_equation, workflow_net, soundness, or woflan");
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
  if (supplied("petri_analysis_return_asap_when_unsound"))
    params.return_asap_when_unsound = parse_boolean(
        require("petri_analysis_return_asap_when_unsound"),
        "petri_analysis_return_asap_when_unsound");
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
  if (supplied("playout_return_set_strings"))
    params.return_set_strings = parse_boolean(
        require("playout_return_set_strings"),
        "playout_return_set_strings");
  if (params.min_length > params.max_length)
    throw std::invalid_argument("playout_min_length must not exceed playout_max_length");
  if (params.return_set_strings &&
      params.variant != ProcessTreePlayoutVariant::kExtensive)
    throw std::invalid_argument(
        "playout_return_set_strings requires EXTENSIVE variant");
  return params;
}

ExtensivePlayoutParams parse_extensive_playout_params(
    const FieldLookup& field) {
  return parse_extensive_playout_params(field, {});
}

ExtensivePlayoutParams parse_extensive_playout_params(
    const FieldLookup& field, const FieldPresence& present) {
  ExtensivePlayoutParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("extensive_return_elements")) {
    params.return_elements = parse_boolean(
        require("extensive_return_elements"), "extensive_return_elements");
  }
  if (supplied("extensive_max_length"))
    params.max_length = parse_nonnegative_size(
        require("extensive_max_length"), "extensive_max_length");
  if (supplied("extensive_max_marking_occ")) {
    const auto value = parse_nonnegative_size(
        require("extensive_max_marking_occ"), "extensive_max_marking_occ");
    if (value == 0)
      throw std::invalid_argument(
          "extensive_max_marking_occ must be positive when supplied");
    params.max_marking_occ = value;
  }
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
  if (supplied("stochastic_fit_from_log"))
    params.fit_from_log = parse_boolean(require("stochastic_fit_from_log"),
                                        "stochastic_fit_from_log");
  if (supplied("stochastic_stop_at_final"))
    params.stop_at_final = parse_boolean(require("stochastic_stop_at_final"),
                                         "stochastic_stop_at_final");
  if (supplied("stochastic_return_visited_elements"))
    params.return_visited_elements = parse_boolean(
        require("stochastic_return_visited_elements"),
        "stochastic_return_visited_elements");
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
  if (supplied("stochastic_initial_marking"))
    params.initial_marking = parse_uint32_marking(
        require("stochastic_initial_marking"), "stochastic_initial_marking");
  if (supplied("stochastic_final_marking"))
    params.final_marking = parse_uint32_marking(
        require("stochastic_final_marking"), "stochastic_final_marking");
  return params;
}

StochasticPetriParams parse_stochastic_petri_params(
    const FieldLookup& field) {
  return parse_stochastic_petri_params(field, {});
}

StochasticPetriParams parse_stochastic_petri_params(
    const FieldLookup& field, const FieldPresence& present) {
  StochasticPetriParams params;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("stochastic_petri_rates")) {
    for (const auto& token : split_csv(require("stochastic_petri_rates"))) {
      const auto rate = parse_finite_double(token, "stochastic_petri_rates");
      if (rate < 0.0)
        throw std::invalid_argument(
            "stochastic_petri_rates must be non-negative");
      params.transition_rates.push_back(rate);
    }
    if (params.transition_rates.empty())
      throw std::invalid_argument("stochastic_petri_rates: expected values");
  }
  if (supplied("stochastic_petri_initial_marking"))
    params.initial_marking = parse_uint32_marking(
        require("stochastic_petri_initial_marking"),
        "stochastic_petri_initial_marking");
  if (supplied("stochastic_petri_immediate_transitions")) {
    for (const auto& token :
         split_csv(require("stochastic_petri_immediate_transitions")))
      params.immediate_transitions.push_back(parse_nonnegative_size(
          token, "stochastic_petri_immediate_transitions"));
  }
  if (supplied("stochastic_petri_max_states"))
    params.max_states = parse_positive_size(
        require("stochastic_petri_max_states"),
        "stochastic_petri_max_states");
  if (supplied("stochastic_petri_analysis")) {
    const auto value = require("stochastic_petri_analysis");
    if (value == "reachability")
      params.analysis = StochasticPetriAnalysisVariant::kReachability;
    else if (value == "transient")
      params.analysis = StochasticPetriAnalysisVariant::kTransient;
    else if (value == "steady_state")
      params.analysis = StochasticPetriAnalysisVariant::kSteadyState;
    else
      throw std::invalid_argument(
          "stochastic_petri_analysis must be reachability, transient, or steady_state");
  }
  if (supplied("stochastic_petri_time")) {
    params.analysis_time_seconds =
        parse_finite_double(require("stochastic_petri_time"),
                            "stochastic_petri_time");
    if (params.analysis_time_seconds < 0.0)
      throw std::invalid_argument(
          "stochastic_petri_time must be non-negative");
    if (params.analysis_time_seconds > kStochasticPetriMaxTransientTimeSeconds)
      throw std::invalid_argument(
          "stochastic_petri_time exceeds the transient safety limit");
  }
  if (supplied("stochastic_petri_initial_state"))
    params.analysis_initial_state = parse_nonnegative_size(
        require("stochastic_petri_initial_state"),
        "stochastic_petri_initial_state");
  if (supplied("stochastic_petri_initial_probabilities")) {
    for (const auto& token : split_csv(
             require("stochastic_petri_initial_probabilities"))) {
      const auto probability = parse_finite_double(
          token, "stochastic_petri_initial_probabilities");
      if (probability < 0.0)
        throw std::invalid_argument(
            "stochastic_petri_initial_probabilities must be non-negative");
      params.analysis_initial_probabilities.push_back(probability);
    }
    if (params.analysis_initial_probabilities.empty())
      throw std::invalid_argument(
          "stochastic_petri_initial_probabilities: expected values");
    double mass = 0.0;
    for (const auto probability : params.analysis_initial_probabilities)
      mass += probability;
    if (!std::isfinite(mass) || std::abs(mass - 1.0) > 1e-10)
      throw std::invalid_argument(
          "stochastic_petri_initial_probabilities must have unit mass");
  }
  if (params.analysis_initial_state &&
      !params.analysis_initial_probabilities.empty())
    throw std::invalid_argument(
        "stochastic Petri analysis accepts an initial state or probability vector, not both");
  const bool transient_controls = supplied("stochastic_petri_time") ||
                                  params.analysis_initial_state.has_value() ||
                                  !params.analysis_initial_probabilities.empty();
  if (params.analysis == StochasticPetriAnalysisVariant::kReachability &&
      transient_controls)
    throw std::invalid_argument(
        "stochastic Petri transient controls require analysis=transient");
  if (params.analysis == StochasticPetriAnalysisVariant::kSteadyState &&
      transient_controls)
    throw std::invalid_argument(
        "stochastic Petri steady_state does not accept transient controls");
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
  if (supplied("monte_carlo_force_distribution")) {
    std::string distribution =
        std::string(trim(require("monte_carlo_force_distribution")));
    std::transform(distribution.begin(), distribution.end(), distribution.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
    if (distribution == "normal") {
      params.force_distribution = MonteCarloDurationDistribution::kNormal;
    } else if (distribution == "uniform") {
      params.force_distribution = MonteCarloDurationDistribution::kUniform;
    } else if (distribution == "exponential") {
      params.force_distribution = MonteCarloDurationDistribution::kExponential;
    } else {
      throw std::invalid_argument(
          "monte_carlo_force_distribution must be normal, uniform, or exponential");
    }
  }

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
  if (supplied("drift_variant")) {
    std::string variant(require("drift_variant"));
    std::transform(variant.begin(), variant.end(), variant.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
    if (variant != "bose")
      throw std::invalid_argument("drift_variant: expected bose");
    params.variant = ConceptDriftVariant::kBose;
  }
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
  if (supplied("drift_activity_attribute"))
    params.activity_attribute = require("drift_activity_attribute");
  if (supplied("drift_timestamp_attribute"))
    params.timestamp_attribute = require("drift_timestamp_attribute");
  if (supplied("drift_case_id_attribute"))
    params.case_id_attribute = require("drift_case_id_attribute");
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
  if (supplied("interval_query_point")) {
    params.query_point = parse_finite_double(require("interval_query_point"),
                                             "interval_query_point");
  }
  const bool query_start = supplied("interval_query_start");
  const bool query_end = supplied("interval_query_end");
  if (query_start != query_end)
    throw std::invalid_argument(
        "interval query range requires both start and end");
  if (query_start) {
    const double start = parse_finite_double(require("interval_query_start"),
                                             "interval_query_start");
    const double end = parse_finite_double(require("interval_query_end"),
                                           "interval_query_end");
    if (start > end)
      throw std::invalid_argument(
          "interval_query_start must not exceed interval_query_end");
    params.query_range = std::make_pair(start, end);
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
  if (supplied("merge_variant")) {
    const auto value = require("merge_variant");
    if (value != "pandas" && value != "PANDAS")
      throw std::invalid_argument("merge_variant: expected pandas");
    params.variant = CaseRelationsVariant::kPandas;
  }
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
  parse_list("event_successor_attributes", params.event_successor_attributes);
  auto assign_bool = [&](std::string_view suffix, bool& out) {
    if (supplied(suffix)) out = parse_boolean(require(suffix), key_for(suffix));
  };
  assign_bool("enable_activity_def_representation", params.enable_activity_def_representation);
  assign_bool("enable_succ_def_representation", params.enable_succ_def_representation);
  assign_bool("enable_all_extra_features", params.enable_all_extra_features);
  assign_bool("enable_case_duration", params.enable_case_duration);
  assign_bool("add_case_identifier_column", params.add_case_identifier_column);
  assign_bool("enable_times_from_first_occurrence", params.enable_times_from_first_occurrence);
  assign_bool("enable_times_from_last_occurrence", params.enable_times_from_last_occurrence);
  assign_bool("enable_direct_paths_times_last_occ", params.enable_direct_paths_times_last_occ);
  assign_bool("enable_indirect_paths_times_last_occ", params.enable_indirect_paths_times_last_occ);
  assign_bool("enable_work_in_progress", params.enable_work_in_progress);
  assign_bool("enable_resource_workload", params.enable_resource_workload);
  assign_bool("enable_first_last_activity_index", params.enable_first_last_activity_index);
  assign_bool("enable_max_concurrent_events", params.enable_max_concurrent_events);
  assign_bool("enable_max_concurrent_events_per_activity", params.enable_max_concurrent_events_per_activity);
  if (supplied("epsilon")) {
    params.epsilon = parse_finite_double(require("epsilon"), key_for("epsilon"));
    if (params.epsilon < 0.0) throw std::invalid_argument("features_epsilon must be non-negative");
  }
  if (supplied("min_num_different_string_values"))
    params.min_num_different_string_values = parse_nonnegative_size(require("min_num_different_string_values"), key_for("min_num_different_string_values"));
  if (supplied("max_num_different_string_values"))
    params.max_num_different_string_values = parse_nonnegative_size(require("max_num_different_string_values"), key_for("max_num_different_string_values"));
  if (params.min_num_different_string_values > params.max_num_different_string_values)
    throw std::invalid_argument("features string cardinality bounds are inverted");
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
  if (supplied("embedding_provider")) {
    const auto value = require("embedding_provider");
    if (value == "hashing" || value == "HASHING") {
      params.provider = EmbeddingProvider::HASHING;
    } else if (value == "transformer" || value == "TRANSFORMER") {
      params.provider = EmbeddingProvider::TRANSFORMER;
    } else {
      throw std::invalid_argument(
          "embedding_provider: expected hashing or transformer");
    }
  }
  if (supplied("embedding_model"))
    params.embedding_model = require("embedding_model");
  if (supplied("embedding_attribute_key"))
    params.attribute_key = require("embedding_attribute_key");
  if (supplied("embedding_event_id_key"))
    params.event_id_key = require("embedding_event_id_key");
  if (supplied("embedding_dimensions")) {
    params.dimensions = parse_nonnegative_size(require("embedding_dimensions"),
                                               "embedding_dimensions");
    if (params.dimensions == 0 &&
        params.provider != EmbeddingProvider::TRANSFORMER)
      throw std::invalid_argument(
          "embedding_dimensions: zero requires transformer provider");
  }
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
  if (supplied("ilp_artificial_start_activity")) {
    options.artificial_start_activity =
        require("ilp_artificial_start_activity");
  }
  if (supplied("ilp_artificial_end_activity")) {
    options.artificial_end_activity = require("ilp_artificial_end_activity");
  }
  if (supplied("ilp_alpha")) {
    options.alpha = parse_finite_double(require("ilp_alpha"), "ilp_alpha");
    if (options.alpha < 0.0 || options.alpha > 1.0)
      throw std::invalid_argument("ilp_alpha must be in [0,1]");
  }
  if (supplied("ilp_causal_relation")) {
    options.causal_relation = parse_ilp_causal_relation(
        require("ilp_causal_relation"), "ilp_causal_relation");
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
  if (supplied("genetic_timestamp_key"))
    options.timestamp_key = require("genetic_timestamp_key");
  if (supplied("genetic_case_id_key"))
    options.case_id_key = require("genetic_case_id_key");
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
  if (supplied("tvq_secure_random"))
    options.secure_random = parse_boolean(require("tvq_secure_random"),
                                          "tvq_secure_random");
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
  if (supplied("pripel_secure_random"))
    options.secure_random = parse_boolean(require("pripel_secure_random"),
                                          "pripel_secure_random");
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
    else if (value == "handover_pandas")
      options.variant = OrganizationalSnaVariant::kHandoverPandas;
    else if (value == "working_together" || value == "working-together")
      options.variant = OrganizationalSnaVariant::kWorkingTogether;
    else if (value == "working_together_pandas")
      options.variant = OrganizationalSnaVariant::kWorkingTogetherPandas;
    else if (value == "subcontracting")
      options.variant = OrganizationalSnaVariant::kSubcontracting;
    else if (value == "subcontracting_pandas")
      options.variant = OrganizationalSnaVariant::kSubcontractingPandas;
    else if (value == "jointactivities" || value == "joint_activities" ||
             value == "joint-activities")
      options.variant = OrganizationalSnaVariant::kJointActivities;
    else if (value == "jointactivities_pandas")
      options.variant = OrganizationalSnaVariant::kJointActivitiesPandas;
    else
      throw std::invalid_argument(
          "sna_variant: expected the four log variants or their _pandas variants");
  }
  if (supplied("sna_activity_attribute"))
    options.activity_attribute = require("sna_activity_attribute");
  if (supplied("sna_resource_attribute"))
    options.resource_attribute = require("sna_resource_attribute");
  if (supplied("sna_case_id_attribute"))
    options.case_id_attribute = require("sna_case_id_attribute");
  const auto default_case_id = [](const std::string& key) {
    return key.empty() || key == "case_id" || key == "case:id" ||
           key == "case:concept:name";
  };
  const auto case_id_variant = [&]() {
    return options.variant == OrganizationalSnaVariant::kHandoverPandas ||
           options.variant == OrganizationalSnaVariant::kWorkingTogetherPandas ||
           options.variant == OrganizationalSnaVariant::kSubcontractingPandas;
  };
  if (supplied("sna_case_id_attribute") &&
      !default_case_id(options.case_id_attribute) && !case_id_variant())
    throw std::invalid_argument(
        "sna_case_id_attribute requires a handover, working-together, or subcontracting pandas variant");
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
  if (supplied("network_analysis_variant")) {
    const auto value = require("network_analysis_variant");
    if (value != "dataframe" && value != "DATAFRAME")
      throw std::invalid_argument("network_analysis_variant: expected dataframe");
    options.variant = NetworkAnalysisVariant::kDataframe;
  }
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
  if (supplied("roles_variant")) {
    const auto value = require("roles_variant");
    if (value == "log" || value == "LOG") options.variant = RoleDiscoveryVariant::kLog;
    else if (value == "pandas" || value == "PANDAS") options.variant = RoleDiscoveryVariant::kPandas;
    else throw std::invalid_argument("roles_variant: expected log or pandas");
  }
  if (supplied("roles_activity_attribute"))
    options.activity_attribute = require("roles_activity_attribute");
  if (supplied("roles_resource_attribute"))
    options.resource_attribute = require("roles_resource_attribute");
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
  if (supplied("resource_profiles_activity_attribute")) options.activity_attribute = require("resource_profiles_activity_attribute");
  if (supplied("resource_profiles_resource_attribute")) options.resource_attribute = require("resource_profiles_resource_attribute");
  if (supplied("resource_profiles_timestamp_attribute")) options.timestamp_attribute = require("resource_profiles_timestamp_attribute");
  if (supplied("resource_profiles_start_timestamp_attribute")) options.start_timestamp_attribute = require("resource_profiles_start_timestamp_attribute");
  if (supplied("resource_profiles_case_id_attribute")) options.case_id_attribute = require("resource_profiles_case_id_attribute");
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

ProfileClusteringOptions parse_profile_clustering_params(const FieldLookup& field) {
  return parse_profile_clustering_params(field, {});
}

ProfileClusteringOptions parse_profile_clustering_params(
    const FieldLookup& field, const FieldPresence& present) {
  ProfileClusteringOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("profile_variant")) {
    std::string variant(require("profile_variant"));
    std::transform(variant.begin(), variant.end(), variant.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
    if (variant != "sklearn_profiles")
      throw std::invalid_argument(
          "profile_variant: expected sklearn_profiles");
    options.variant = ProfileClusteringVariant::kSklearnProfiles;
  }
  if (supplied("profile_cluster_count"))
    options.cluster_count = parse_positive_size(
        require("profile_cluster_count"), "profile_cluster_count");
  if (supplied("profile_max_iterations"))
    options.max_iterations = parse_positive_size(
        require("profile_max_iterations"), "profile_max_iterations");
  if (supplied("profile_max_traces"))
    options.max_traces = parse_positive_size(require("profile_max_traces"),
                                             "profile_max_traces");
  if (supplied("profile_max_features"))
    options.max_features = parse_positive_size(
        require("profile_max_features"), "profile_max_features");
  if (supplied("profile_seed"))
    options.seed = parse_nonnegative_size(require("profile_seed"),
                                          "profile_seed");
  if (supplied("profile_tolerance")) {
    options.tolerance = parse_finite_double(require("profile_tolerance"),
                                            "profile_tolerance");
    if (options.tolerance < 0.0)
      throw std::invalid_argument("profile_tolerance must be non-negative");
  }
  if (supplied("profile_include_activity"))
    options.include_activity_representation = parse_boolean(
        require("profile_include_activity"), "profile_include_activity");
  if (supplied("profile_include_succession"))
    options.include_succession_representation = parse_boolean(
        require("profile_include_succession"), "profile_include_succession");
  if (supplied("profile_trace_string_attributes"))
    options.trace_string_attributes = parse_strict_csv(
        require("profile_trace_string_attributes"),
        "profile_trace_string_attributes");
  if (supplied("profile_event_string_attributes"))
    options.event_string_attributes = parse_strict_csv(
        require("profile_event_string_attributes"),
        "profile_event_string_attributes");
  if (supplied("profile_trace_numeric_attributes"))
    options.trace_numeric_attributes = parse_strict_csv(
        require("profile_trace_numeric_attributes"),
        "profile_trace_numeric_attributes");
  if (supplied("profile_event_numeric_attributes"))
    options.event_numeric_attributes = parse_strict_csv(
        require("profile_event_numeric_attributes"),
        "profile_event_numeric_attributes");
  if (supplied("profile_feature_names"))
    options.feature_names = parse_strict_csv(require("profile_feature_names"),
                                             "profile_feature_names");
  return options;
}

ChaoticActivitiesOptions parse_chaotic_activities_params(
    const FieldLookup& field) {
  return parse_chaotic_activities_params(field, {});
}

ChaoticActivitiesOptions parse_chaotic_activities_params(
    const FieldLookup& field, const FieldPresence& present) {
  ChaoticActivitiesOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("chaotic_activity_key"))
    options.activity_key = require("chaotic_activity_key");
  if (supplied("chaotic_alpha")) {
    options.alpha = parse_finite_double(require("chaotic_alpha"),
                                        "chaotic_alpha");
    if (*options.alpha < 0.0)
      throw std::invalid_argument(
          "chaotic_alpha: expected a finite non-negative number");
  }
  if (supplied("chaotic_max_activities"))
    options.max_activities = parse_positive_size(
        require("chaotic_max_activities"), "chaotic_max_activities");
  if (supplied("chaotic_max_events"))
    options.max_events = parse_positive_size(require("chaotic_max_events"),
                                             "chaotic_max_events");
  return options;
}

PassedTimeOptions parse_passed_time_params(const FieldLookup& field) {
  return parse_passed_time_params(field, {});
}

PassedTimeOptions parse_passed_time_params(const FieldLookup& field,
                                           const FieldPresence& present) {
  PassedTimeOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (!supplied("passed_time_activity"))
    throw std::invalid_argument("passed_time_activity: required");
  options.activity = require("passed_time_activity");
  if (supplied("passed_time_variant")) {
    const auto value = require("passed_time_variant");
    if (value == "pre" || value == "PRE")
      options.variant = PassedTimeVariant::kPre;
    else if (value == "post" || value == "POST")
      options.variant = PassedTimeVariant::kPost;
    else if (value == "prepost" || value == "PREPOST" ||
             value == "pre_post")
      options.variant = PassedTimeVariant::kPrePost;
    else
      throw std::invalid_argument(
          "passed_time_variant: expected pre, post, or prepost");
  }
  if (supplied("passed_time_activity_key"))
    options.activity_key = require("passed_time_activity_key");
  if (supplied("passed_time_timestamp_key"))
    options.timestamp_key = require("passed_time_timestamp_key");
  if (supplied("passed_time_start_timestamp_key"))
    options.start_timestamp_key = require("passed_time_start_timestamp_key");
  if (supplied("passed_time_aggregation")) {
    const auto value = require("passed_time_aggregation");
    if (value == "mean")
      options.performance_aggregation = DfgPerformanceAggregation::kMean;
    else if (value == "median")
      options.performance_aggregation = DfgPerformanceAggregation::kMedian;
    else if (value == "min" || value == "minimum")
      options.performance_aggregation = DfgPerformanceAggregation::kMinimum;
    else if (value == "max" || value == "maximum")
      options.performance_aggregation = DfgPerformanceAggregation::kMaximum;
    else if (value == "stdev" || value == "standard_deviation")
      options.performance_aggregation =
          DfgPerformanceAggregation::kStandardDeviation;
    else if (value == "sum")
      options.performance_aggregation = DfgPerformanceAggregation::kSum;
    else
      throw std::invalid_argument(
          "passed_time_aggregation: expected mean, median, min, max, stdev, or sum");
  }
  if (supplied("passed_time_business_hours"))
    options.use_business_hours =
        parse_boolean(require("passed_time_business_hours"),
                      "passed_time_business_hours");
  if (supplied("passed_time_business_hour_slots"))
    options.business_hour_slots = parse_business_hour_slots(
        require("passed_time_business_hour_slots"),
        "passed_time_business_hour_slots");
  return options;
}

ServiceTimeOptions parse_service_time_params(const FieldLookup& field) {
  return parse_service_time_params(field, {});
}

ServiceTimeOptions parse_service_time_params(const FieldLookup& field,
                                             const FieldPresence& present) {
  ServiceTimeOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("service_time_activity_key"))
    options.activity_key = require("service_time_activity_key");
  if (supplied("service_time_start_timestamp_key"))
    options.start_timestamp_key = require("service_time_start_timestamp_key");
  if (supplied("service_time_timestamp_key"))
    options.timestamp_key = require("service_time_timestamp_key");
  if (supplied("service_time_aggregation")) {
    const auto value = require("service_time_aggregation");
    if (value == "mean")
      options.aggregation = DfgPerformanceAggregation::kMean;
    else if (value == "median")
      options.aggregation = DfgPerformanceAggregation::kMedian;
    else if (value == "min" || value == "minimum")
      options.aggregation = DfgPerformanceAggregation::kMinimum;
    else if (value == "max" || value == "maximum")
      options.aggregation = DfgPerformanceAggregation::kMaximum;
    else if (value == "sum")
      options.aggregation = DfgPerformanceAggregation::kSum;
    else
      throw std::invalid_argument(
          "service_time_aggregation: expected mean, median, min, max, or sum");
  }
  if (supplied("service_time_business_hours"))
    options.use_business_hours =
        parse_boolean(require("service_time_business_hours"),
                      "service_time_business_hours");
  if (supplied("service_time_business_hour_slots"))
    options.business_hour_slots = parse_business_hour_slots(
        require("service_time_business_hour_slots"),
        "service_time_business_hour_slots");
  return options;
}

ReworkOptions parse_rework_params(const FieldLookup& field) {
  return parse_rework_params(field, {});
}

ReworkOptions parse_rework_params(const FieldLookup& field,
                                  const FieldPresence& present) {
  ReworkOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("rework_variant")) {
    const auto value = require("rework_variant");
    if (value == "activities" || value == "ACTIVITIES")
      options.variant = ReworkVariant::kActivities;
    else if (value == "cases" || value == "CASES")
      options.variant = ReworkVariant::kCases;
    else
      throw std::invalid_argument(
          "rework_variant: expected activities or cases");
  }
  if (supplied("rework_activity_key"))
    options.activity_key = require("rework_activity_key");
  if (supplied("rework_case_id_key"))
    options.case_id_key = require("rework_case_id_key");
  return options;
}

CycleTimeOptions parse_cycle_time_params(const FieldLookup& field) {
  return parse_cycle_time_params(field, {});
}

CycleTimeOptions parse_cycle_time_params(const FieldLookup& field,
                                         const FieldPresence& present) {
  CycleTimeOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("cycle_time_timestamp_key"))
    options.timestamp_key = require("cycle_time_timestamp_key");
  if (supplied("cycle_time_start_timestamp_key"))
    options.start_timestamp_key = require("cycle_time_start_timestamp_key");
  return options;
}

CaseArrivalOptions parse_case_arrival_params(const FieldLookup& field) {
  return parse_case_arrival_params(field, {});
}

CaseArrivalOptions parse_case_arrival_params(const FieldLookup& field,
                                             const FieldPresence& present) {
  CaseArrivalOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("case_arrival_timestamp_key"))
    options.timestamp_key = require("case_arrival_timestamp_key");
  if (supplied("case_arrival_business_hours"))
    options.use_business_hours = parse_boolean(
        require("case_arrival_business_hours"), "case_arrival_business_hours");
  if (supplied("case_arrival_business_hour_slots"))
    options.business_hour_slots = parse_business_hour_slots(
        require("case_arrival_business_hour_slots"),
        "case_arrival_business_hour_slots");
  if (!options.use_business_hours && !options.business_hour_slots.empty())
    options.business_hour_slots.clear();
  return options;
}

CaseStatisticsOptions parse_case_statistics_params(const FieldLookup& field) {
  return parse_case_statistics_params(field, {});
}

CaseStatisticsOptions parse_case_statistics_params(
    const FieldLookup& field, const FieldPresence& present) {
  CaseStatisticsOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("case_statistics_case_id_key"))
    options.case_id_key = require("case_statistics_case_id_key");
  if (supplied("case_statistics_timestamp_key"))
    options.timestamp_key = require("case_statistics_timestamp_key");
  if (supplied("case_statistics_enable_sort"))
    options.enable_sort = parse_boolean(require("case_statistics_enable_sort"),
                                        "case_statistics_enable_sort");
  if (supplied("case_statistics_sort_by")) {
    const auto value = require("case_statistics_sort_by");
    if (value == "case_id" || value == "CASE_ID")
      options.sort_by = CaseStatisticsSortBy::kCaseId;
    else if (value == "start_time" || value == "START_TIME")
      options.sort_by = CaseStatisticsSortBy::kStartTime;
    else if (value == "end_time" || value == "END_TIME")
      options.sort_by = CaseStatisticsSortBy::kEndTime;
    else if (value == "duration" || value == "DURATION")
      options.sort_by = CaseStatisticsSortBy::kDuration;
    else
      throw std::invalid_argument(
          "case_statistics_sort_by: expected case_id, start_time, end_time, or duration");
  }
  if (supplied("case_statistics_sort_ascending"))
    options.sort_ascending = parse_boolean(
        require("case_statistics_sort_ascending"),
        "case_statistics_sort_ascending");
  if (supplied("case_statistics_max_cases"))
    options.max_cases = parse_nonnegative_size(
        require("case_statistics_max_cases"), "case_statistics_max_cases");
  if (supplied("case_statistics_business_hours"))
    options.use_business_hours = parse_boolean(
        require("case_statistics_business_hours"),
        "case_statistics_business_hours");
  if (supplied("case_statistics_business_hour_slots"))
    options.business_hour_slots = parse_business_hour_slots(
        require("case_statistics_business_hour_slots"),
        "case_statistics_business_hour_slots");
  if (!options.use_business_hours && !options.business_hour_slots.empty())
    options.business_hour_slots.clear();
  return options;
}

AttributeStatisticsOptions parse_attribute_statistics_params(
    const FieldLookup& field) {
  return parse_attribute_statistics_params(field, {});
}

AttributeStatisticsOptions parse_attribute_statistics_params(
    const FieldLookup& field, const FieldPresence& present) {
  AttributeStatisticsOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (!supplied("attribute_statistics_key"))
    throw std::invalid_argument("attribute_statistics_key: required");
  options.attribute_key = require("attribute_statistics_key");
  if (supplied("attribute_statistics_scope")) {
    const auto value = require("attribute_statistics_scope");
    if (value == "events" || value == "event" || value == "EVENTS")
      options.scope = AttributeStatisticsScope::kEvents;
    else if (value == "traces" || value == "trace" || value == "TRACES")
      options.scope = AttributeStatisticsScope::kTraces;
    else
      throw std::invalid_argument(
          "attribute_statistics_scope: expected events or traces");
  }
  if (supplied("attribute_statistics_keep_once_per_case"))
    options.keep_once_per_case = parse_boolean(
        require("attribute_statistics_keep_once_per_case"),
        "attribute_statistics_keep_once_per_case");
  if (options.scope == AttributeStatisticsScope::kTraces)
    options.keep_once_per_case = false;
  return options;
}

AttributeDistributionOptions parse_attribute_distribution_params(
    const FieldLookup& field) {
  return parse_attribute_distribution_params(field, {});
}

AttributeDistributionOptions parse_attribute_distribution_params(
    const FieldLookup& field, const FieldPresence& present) {
  AttributeDistributionOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("attribute_distribution_timestamp_key"))
    options.timestamp_key = require("attribute_distribution_timestamp_key");
  if (supplied("attribute_distribution_type")) {
    const auto value = require("attribute_distribution_type");
    if (value == "days_month" || value == "DAYS_MONTH")
      options.distribution = EventDistributionType::kDaysMonth;
    else if (value == "months" || value == "MONTHS")
      options.distribution = EventDistributionType::kMonths;
    else if (value == "years" || value == "YEARS")
      options.distribution = EventDistributionType::kYears;
    else if (value == "hours" || value == "HOURS")
      options.distribution = EventDistributionType::kHours;
    else if (value == "days_week" || value == "DAYS_WEEK")
      options.distribution = EventDistributionType::kDaysWeek;
    else if (value == "weeks" || value == "WEEKS")
      options.distribution = EventDistributionType::kWeeks;
    else
      throw std::invalid_argument(
          "attribute_distribution_type: expected days_month, months, years, hours, days_week, or weeks");
  }
  return options;
}

AttributeKdeOptions parse_attribute_kde_params(const FieldLookup& field) {
  return parse_attribute_kde_params(field, {});
}

AttributeKdeOptions parse_attribute_kde_params(const FieldLookup& field,
                                               const FieldPresence& present) {
  AttributeKdeOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (!supplied("attribute_kde_key"))
    throw std::invalid_argument("attribute_kde_key: required");
  options.attribute_key = require("attribute_kde_key");
  if (supplied("attribute_kde_variant")) {
    const auto value = require("attribute_kde_variant");
    if (value == "numeric" || value == "NUMERIC")
      options.variant = AttributeKdeVariant::kNumeric;
    else if (value == "date" || value == "DATE")
      options.variant = AttributeKdeVariant::kDate;
    else
      throw std::invalid_argument(
          "attribute_kde_variant: expected numeric or date");
  }
  if (supplied("attribute_kde_graph_points"))
    options.graph_points = parse_positive_size(
        require("attribute_kde_graph_points"), "attribute_kde_graph_points");
  if (supplied("attribute_kde_points_to_sample"))
    options.points_to_sample = parse_positive_size(
        require("attribute_kde_points_to_sample"),
        "attribute_kde_points_to_sample");
  return options;
}

BoundaryActivitiesOptions parse_boundary_activities_params(
    const FieldLookup& field) {
  return parse_boundary_activities_params(field, {});
}

BoundaryActivitiesOptions parse_boundary_activities_params(
    const FieldLookup& field, const FieldPresence& present) {
  BoundaryActivitiesOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("boundary_activities_variant")) {
    const auto value = require("boundary_activities_variant");
    if (value == "start" || value == "START")
      options.variant = BoundaryActivitiesVariant::kStart;
    else if (value == "end" || value == "END")
      options.variant = BoundaryActivitiesVariant::kEnd;
    else if (value == "both" || value == "BOTH")
      options.variant = BoundaryActivitiesVariant::kBoth;
    else
      throw std::invalid_argument(
          "boundary_activities_variant: expected start, end, or both");
  }
  if (supplied("boundary_activities_activity_key"))
    options.activity_key = require("boundary_activities_activity_key");
  return options;
}

VariantStatisticsOptions parse_variant_statistics_params(
    const FieldLookup& field) {
  return parse_variant_statistics_params(field, {});
}

VariantStatisticsOptions parse_variant_statistics_params(
    const FieldLookup& field, const FieldPresence& present) {
  VariantStatisticsOptions options;
  const auto supplied = [&](std::string_view name) {
    return present ? present(name) : !field(name).empty();
  };
  const auto require = [&](std::string_view name) {
    const auto value = field(name);
    if (value.empty())
      throw std::invalid_argument(std::string(name) + ": expected a value");
    return value;
  };
  if (supplied("variant_statistics_activity_key"))
    options.activity_key = require("variant_statistics_activity_key");
  if (supplied("variant_statistics_max_variants"))
    options.max_variants = parse_nonnegative_size(
        require("variant_statistics_max_variants"),
        "variant_statistics_max_variants");
  return options;
}

}  // namespace minera
