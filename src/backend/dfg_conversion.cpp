#include "minera/dfg_conversion.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

struct NormalizedDfg {
  std::vector<std::string> activities;
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  std::map<std::pair<std::string, std::string>, std::uint64_t> edges;
};

NormalizedDfg normalize(const DfgCleanAnalysis& input) {
  NormalizedDfg result;
  result.edges = input.graph;

  std::set<std::string> names;
  for (const auto& [edge, count] : input.graph) {
    if (count == 0) continue;
    names.insert(edge.first);
    names.insert(edge.second);
  }
  for (const auto& [activity, count] : input.start_activities) {
    if (count == 0) continue;
    names.insert(activity);
  }
  for (const auto& [activity, count] : input.end_activities) {
    if (count == 0) continue;
    names.insert(activity);
  }

  result.starts = input.start_activities;
  result.ends = input.end_activities;
  for (auto it = result.starts.begin(); it != result.starts.end();) {
    if (it->second == 0) it = result.starts.erase(it);
    else ++it;
  }
  for (auto it = result.ends.begin(); it != result.ends.end();) {
    if (it->second == 0) it = result.ends.erase(it);
    else ++it;
  }

  // A raw DFG often omits boundary maps.  Infer them from the graph in that
  // case, matching the conversion utility's default behavior.
  if (result.starts.empty()) {
    std::set<std::string> targets;
    for (const auto& [edge, count] : result.edges) {
      if (count != 0) targets.insert(edge.second);
    }
    for (const auto& [edge, count] : result.edges) {
      if (count != 0 && !targets.contains(edge.first)) result.starts[edge.first] = 1;
    }
    if (result.starts.empty()) {
      for (const auto& name : names) result.starts[name] = 1;
    }
  }
  if (result.ends.empty()) {
    std::set<std::string> sources;
    for (const auto& [edge, count] : result.edges) {
      if (count != 0) sources.insert(edge.first);
    }
    for (const auto& [edge, count] : result.edges) {
      if (count != 0 && !sources.contains(edge.second)) result.ends[edge.second] = 1;
    }
    if (result.ends.empty()) {
      for (const auto& name : names) result.ends[name] = 1;
    }
  }

  for (const auto& [activity, count] : result.starts) {
    if (count != 0) names.insert(activity);
  }
  for (const auto& [activity, count] : result.ends) {
    if (count != 0) names.insert(activity);
  }
  result.activities.assign(names.begin(), names.end());
  return result;
}

NormalizedDfg normalize(const DirectlyFollowsGraph& graph) {
  NormalizedDfg result;
  const auto& names = graph.activities();
  for (const auto& edge : graph.edges()) {
    if (edge.source >= names.size() || edge.target >= names.size()) {
      throw std::invalid_argument("DFG conversion: edge activity id out of range");
    }
    if (edge.count != 0) {
      result.edges[{names[edge.source], names[edge.target]}] = edge.count;
    }
  }
  for (const auto& activity : names) {
    const auto start = graph.start_activity_count(activity);
    const auto end = graph.end_activity_count(activity);
    if (start != 0) result.starts[activity] = start;
    if (end != 0) result.ends[activity] = end;
  }
  DfgCleanAnalysis payload;
  payload.graph = result.edges;
  payload.start_activities = result.starts;
  payload.end_activities = result.ends;
  return normalize(payload);
}

void validate_artificial_names(const NormalizedDfg& dfg,
                               const DfgPetriConversionOptions& options) {
  if (options.artificial_start_activity.empty() ||
      options.artificial_end_activity.empty() ||
      options.artificial_start_activity == options.artificial_end_activity) {
    throw std::invalid_argument(
        "DFG conversion: artificial boundary activities must be distinct and non-empty");
  }
  if (std::binary_search(dfg.activities.begin(), dfg.activities.end(),
                         options.artificial_start_activity) ||
      std::binary_search(dfg.activities.begin(), dfg.activities.end(),
                         options.artificial_end_activity)) {
    throw std::invalid_argument(
        "DFG conversion: artificial boundary activity collides with a DFG activity");
  }
}

std::size_t add_place_for(PetriNet& net,
                          std::unordered_map<std::string, std::size_t>& places,
                          const std::string& name) {
  const auto place = net.add_place();
  places.emplace(name, place);
  return place;
}

std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(hex[c >> 4]);
          out.push_back(hex[c & 0x0f]);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

std::string variant_name(DfgPetriConversionVariant variant) {
  switch (variant) {
    case DfgPetriConversionVariant::kActivityDefinesPlace:
      return "ACTIVITY_DEFINES_PLACE";
    case DfgPetriConversionVariant::kInvisiblesNoDuplicates:
      return "INVISIBLES_NO_DUPLICATES";
  }
  throw std::invalid_argument("DFG conversion: unknown variant");
}

}  // namespace

PetriNet convert_dfg_to_petri_net(
    const DfgCleanAnalysis& input,
    const DfgPetriConversionOptions& options) {
  const auto dfg = normalize(input);
  PetriNet net;
  net.set_activities(dfg.activities);

  std::unordered_map<std::string, std::size_t> activity_ids;
  for (std::size_t i = 0; i < dfg.activities.size(); ++i) {
    activity_ids.emplace(dfg.activities[i], i);
  }

  if (options.variant == DfgPetriConversionVariant::kActivityDefinesPlace) {
    const auto source = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    std::unordered_map<std::string, std::size_t> places;
    for (const auto& activity : dfg.activities) add_place_for(net, places, activity);

    for (const auto& [activity, count] : dfg.starts) {
      if (count == 0) continue;
      const auto place = places.at(activity);
      const auto transition = net.add_transition(activity_ids.at(activity),
                                                  {source}, {place});
      (void)transition;
    }
    for (const auto& [activity, count] : dfg.ends) {
      if (count == 0) continue;
      const auto place = places.at(activity);
      net.add_transition(std::nullopt, {place}, {sink});
    }
    for (const auto& [edge, count] : dfg.edges) {
      if (count == 0) continue;
      const auto source_place = places.at(edge.first);
      const auto target_place = places.at(edge.second);
      net.add_transition(activity_ids.at(edge.second), {source_place},
                         {target_place});
    }
    return net;
  }

  if (options.variant != DfgPetriConversionVariant::kInvisiblesNoDuplicates) {
    throw std::invalid_argument("DFG conversion: unknown variant");
  }
  validate_artificial_names(dfg, options);

  std::unordered_map<std::string, std::size_t> left;
  std::unordered_map<std::string, std::size_t> right;
  std::vector<std::string> all = dfg.activities;
  all.push_back(options.artificial_start_activity);
  all.push_back(options.artificial_end_activity);
  for (const auto& activity : all) {
    left.emplace(activity, net.add_place());
    right.emplace(activity, net.add_place());
    const auto id = activity_ids.find(activity);
    net.add_transition(id == activity_ids.end()
                           ? std::optional<std::size_t>{}
                           : std::optional<std::size_t>{id->second},
                       {left.at(activity)}, {right.at(activity)});
  }
  net.set_source(left.at(options.artificial_start_activity));
  net.set_sink(right.at(options.artificial_end_activity));

  for (const auto& [activity, count] : dfg.starts) {
    if (count != 0) {
      net.add_transition(std::nullopt,
                         {right.at(options.artificial_start_activity)},
                         {left.at(activity)});
    }
  }
  for (const auto& [edge, count] : dfg.edges) {
    if (count != 0) {
      net.add_transition(std::nullopt, {right.at(edge.first)},
                         {left.at(edge.second)});
    }
  }
  for (const auto& [activity, count] : dfg.ends) {
    if (count != 0) {
      net.add_transition(std::nullopt, {right.at(activity)},
                         {left.at(options.artificial_end_activity)});
    }
  }
  return net;
}

PetriNet convert_dfg_to_petri_net(
    const DirectlyFollowsGraph& graph,
    const DfgPetriConversionOptions& options) {
  const auto normalized = normalize(graph);
  DfgCleanAnalysis payload;
  payload.graph = normalized.edges;
  payload.start_activities = normalized.starts;
  payload.end_activities = normalized.ends;
  return convert_dfg_to_petri_net(payload, options);
}

std::string serialize_dfg_petri_net_json(
    const PetriNet& net, DfgPetriConversionVariant variant) {
  const auto name = variant_name(variant);
  if (net.source() >= net.place_count() || net.sink() >= net.place_count()) {
    throw std::invalid_argument("DFG conversion: net source or sink is invalid");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.activity && *transition.activity >= net.activities().size()) {
      throw std::invalid_argument("DFG conversion: transition activity is invalid");
    }
    for (const auto place : transition.inputs) {
      if (place >= net.place_count()) throw std::invalid_argument("DFG conversion: input place is invalid");
    }
    for (const auto place : transition.outputs) {
      if (place >= net.place_count()) throw std::invalid_argument("DFG conversion: output place is invalid");
    }
  }

  std::ostringstream out;
  out << "{\"family\":\"DFG_CONVERSION\",\"variant\":\"" << name
      << "\",\"source\":" << net.source() << ",\"sink\":"
      << net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i != 0) out << ',';
    out << '\"' << json_escape(net.activities()[i]) << '\"';
  }
  out << "],\"place_count\":" << net.place_count()
      << ",\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i != 0) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"activity\":";
    if (transition.activity) out << *transition.activity;
    else out << "null";
    out << ",\"inputs\":[";
    for (std::size_t j = 0; j < transition.inputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.inputs[j];
    }
    out << "],\"outputs\":[";
    for (std::size_t j = 0; j < transition.outputs.size(); ++j) {
      if (j != 0) out << ',';
      out << transition.outputs[j];
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

DfgPetriConversionVariant parse_dfg_petri_conversion_variant(
    std::string_view name) {
  if (name == "activity_defines_place" || name == "ACTIVITY_DEFINES_PLACE" ||
      name == "activity") {
    return DfgPetriConversionVariant::kActivityDefinesPlace;
  }
  if (name == "invisibles_no_duplicates" ||
      name == "INVISIBLES_NO_DUPLICATES" || name == "invisibles") {
    return DfgPetriConversionVariant::kInvisiblesNoDuplicates;
  }
  throw std::invalid_argument(
      "DFG conversion variant: expected activity_defines_place or "
      "invisibles_no_duplicates");
}

}  // namespace minera
