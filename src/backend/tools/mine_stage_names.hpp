#pragma once

// Readable stage names for GET /api/mine-progress (mine_progress.hpp).

#include <string>
#include <string_view>
#include <utility>

// The stage name a native-source dispatch family reports ("alpha" is "Alpha
// miner", "inductive_conformance" is "Inductive net: conformance"); an
// unlisted id reads with spaces and a capital first letter.
inline std::string mine_family_stage_name(std::string_view family) {
  static constexpr std::pair<std::string_view, std::string_view> kNames[] = {
      {"source_log_preparation", "Reading the event log"},
      {"directly_follows_graph", "Directly-follows graph"},
      {"alpha", "Alpha miner"},
      {"alpha_log_preparation", "Alpha miner: log preparation"},
      {"heuristic", "Heuristics miner"},
      {"heuristic_petri_conversion", "Heuristics miner: Petri net"},
      {"inductive", "Inductive miner"},
      {"inductive_log_preparation", "Inductive miner: log preparation"},
      {"inductive_petri_conversion", "Inductive miner: Petri net"},
      {"powl", "POWL miner"},
      {"powl_petri_conversion", "POWL miner: Petri net"},
      {"temporal_profile", "Temporal profile"},
      {"transition_system", "Transition system"},
      {"minimum_self_distance", "Minimum self-distances"},
      {"footprints", "Model footprints"},
      {"log_statistics_preparation", "Performance and resources"},
      {"performance_spectrum", "Performance spectrum"},
      {"log_skeleton", "Log skeleton"},
      {"log_skeleton_conformance", "Log skeleton conformance"},
      {"declare", "Declare constraints"},
      {"declare_conformance", "Declare conformance"},
      {"dfg_variants", "Directly-follows variants"},
      {"correlation", "Correlation miner"},
      {"batches", "Batches"},
      {"overlap", "Overlap analysis"},
  };
  for (const auto& [id, name] : kNames)
    if (id == family) return std::string(name);
  static constexpr std::pair<std::string_view, std::string_view> kNets[] = {
      {"alpha", "Alpha net"}, {"inductive", "Inductive net"},
      {"heuristic", "Heuristics net"}, {"powl", "POWL net"}};
  for (const auto& [suffix, label] : {std::pair<std::string_view, std::string_view>{"_conformance", "conformance"},
                                      {"_decoration", "decoration"}}) {
    if (family.size() <= suffix.size() || !family.ends_with(suffix)) continue;
    const auto net = family.substr(0, family.size() - suffix.size());
    for (const auto& [id, name] : kNets)
      if (id == net) return std::string(name) + ": " + std::string(label);
  }
  std::string text(family);
  for (auto& ch : text) if (ch == '_') ch = ' ';
  if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') text[0] = static_cast<char>(text[0] - 'a' + 'A');
  return text;
}
