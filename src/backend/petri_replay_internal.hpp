#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera::detail {

using ReplayMarkingKeys = std::vector<bool>;
using CleaningComponents = std::vector<std::vector<std::size_t>>;
inline constexpr std::size_t kReplayStructuralSearchCap = 4096;

inline CleaningComponents cleaning_components(const PetriNet& net,
                                       const ReplayMarkingKeys& initial_keys,
                                       const ReplayMarkingKeys& final_keys) {
  const auto marked_count = [](const ReplayMarkingKeys& keys) {
    return std::count(keys.begin(), keys.end(), true);
  };
  const auto initial_count = marked_count(initial_keys);
  // The public structural helper declines multiple boundary places before
  // attempting to select its initial place. Final-place identity is unused.
  if (initial_count > 1 || marked_count(final_keys) > 1) return {};
  if (initial_count == 0)
    throw std::invalid_argument("Petri token-flood cleaning requires a non-empty initial marking");
  const auto root = static_cast<std::size_t>(
      std::find(initial_keys.begin(), initial_keys.end(), true) - initial_keys.begin());
  std::vector<std::vector<std::size_t>> outgoing(net.place_count());
  std::vector<std::vector<std::size_t>> outputs(net.transitions().size());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    std::set<std::size_t> inputs(transition.inputs.begin(), transition.inputs.end());
    inputs.insert(transition.reset_inputs.begin(), transition.reset_inputs.end());
    inputs.insert(transition.inhibitor_inputs.begin(), transition.inhibitor_inputs.end());
    for (const auto place : inputs) outgoing[place].push_back(t);
    const std::set<std::size_t> distinct_outputs(transition.outputs.begin(), transition.outputs.end());
    outputs[t].assign(distinct_outputs.begin(), distinct_outputs.end());
  }
  std::set<std::vector<std::size_t>> unique;
  std::size_t generated = 0;
  std::size_t stored_members = 0;
  const auto expand = [&](auto&& self, std::set<std::size_t> component,
                          std::set<std::size_t> visited, std::size_t depth) -> void {
    if (++generated > kReplayStructuralSearchCap)
      throw std::runtime_error("Petri usage replay exceeded the cleaning-component state limit");
    std::set<std::size_t> branches;
    if (depth < 6) {
      std::queue<std::size_t> pending;
      for (const auto place : component) if (!visited.contains(place)) pending.push(place);
      while (!pending.empty()) {
        const auto place = pending.front(); pending.pop();
        if (!visited.insert(place).second) continue;
        for (const auto t : outgoing[place]) {
          if (outputs[t].size() > 1) branches.insert(t);
          else if (outputs[t].size() == 1 && component.insert(outputs[t][0]).second)
            pending.push(outputs[t][0]);
        }
      }
    }
    // Prefix components are part of the public traversal. Each split is
    // explored independently, with a separate visited set for every branch.
    if (unique.emplace(component.begin(), component.end()).second) {
      if (component.size() > 1'000'000 - stored_members)
        throw std::runtime_error("Petri usage replay exceeded the cleaning-component size limit");
      stored_members += component.size();
    }
    for (const auto transition : branches) {
      for (const auto place : outputs[transition]) {
        auto branch = component;
        branch.insert(place);
        self(self, std::move(branch), visited, depth + 1);
      }
    }
  };
  expand(expand, {root}, {}, 0);
  return {unique.begin(), unique.end()};
}

struct HiddenPlacePath {
  std::size_t source;
  std::size_t target;
  std::vector<std::size_t> transitions;
};

// Structural place paths deliberately ignore enabling while searching. Replay
// can retain an enabled prefix even when a later multi-input join is blocked.
inline std::vector<HiddenPlacePath> silent_place_paths(
    const ReplayMarkingKeys& source_keys, const ReplayMarkingKeys& target_keys, const PetriNet& net,
    const std::vector<std::string>& place_names,
    std::size_t max_path_length = 51) {
  std::vector<std::vector<std::size_t>> outgoing(net.place_count());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    if (!transition.silent()) continue;
    std::set<std::size_t> inputs(transition.inputs.begin(), transition.inputs.end());
    inputs.insert(transition.reset_inputs.begin(), transition.reset_inputs.end());
    inputs.insert(transition.inhibitor_inputs.begin(), transition.inhibitor_inputs.end());
    for (const auto place : inputs) outgoing[place].push_back(t);
  }
  const auto none = std::numeric_limits<std::size_t>::max();
  std::vector<HiddenPlacePath> result;
  std::size_t stored_steps = 0;
  for (std::size_t source = 0; source < source_keys.size(); ++source) {
    if (!source_keys[source]) continue;
    std::vector<std::size_t> parent(source_keys.size(), none), via(source_keys.size(), none);
    std::vector<std::size_t> distance(source_keys.size(), 0);
    parent[source] = source;
    std::queue<std::size_t> pending;
    pending.push(source);
    std::size_t visited = 1;
    while (!pending.empty()) {
      const auto place = pending.front(); pending.pop();
      if (distance[place] >= max_path_length) continue;
      for (const auto t : outgoing[place]) {
        for (const auto output : net.transitions()[t].outputs) {
          if (parent[output] != none) continue;
          if (++visited > kReplayStructuralSearchCap)
            throw std::runtime_error("Petri usage replay exceeded the silent place-path state limit");
          parent[output] = place;
          via[output] = t;
          distance[output] = distance[place] + 1;
          pending.push(output);
        }
      }
    }
    for (std::size_t target = 0; target < target_keys.size(); ++target) {
      if (!target_keys[target] || target == source || parent[target] == none) continue;
      HiddenPlacePath path{source, target, {}};
      for (auto place = target; place != source; place = parent[place]) path.transitions.push_back(via[place]);
      std::reverse(path.transitions.begin(), path.transitions.end());
      if (path.transitions.size() > 1'000'000 - stored_steps)
        throw std::runtime_error("Petri usage replay exceeded the silent place-path size limit");
      stored_steps += path.transitions.size();
      result.push_back(std::move(path));
    }
  }
  std::sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
    return std::tuple{left.transitions.size(), std::string_view(place_names[left.target]), std::string_view(place_names[left.source]), left.target, left.source}
         < std::tuple{right.transitions.size(), std::string_view(place_names[right.target]), std::string_view(place_names[right.source]), right.target, right.source};
  });
  return result;
}

}  // namespace minera::detail
