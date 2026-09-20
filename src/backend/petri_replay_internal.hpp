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

template <class Integer>
ReplayMarkingKeys resolve_replay_marking_keys(
    const ReplayMarkingKeys& configured, const std::vector<Integer>& marking,
    std::string_view name) {
  if (!configured.empty() && configured.size() != marking.size())
    throw std::invalid_argument(std::string(name) + " must contain one entry per place");
  auto result = configured.empty() ? ReplayMarkingKeys(marking.size(), false) : configured;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] == 0) continue;
    if (!configured.empty() && !configured[place])
      throw std::invalid_argument(std::string(name) + " must include every positive-token place");
    result[place] = true;
  }
  return result;
}

inline std::vector<std::string> replay_element_names(
    const std::vector<std::string>& configured, std::size_t count,
    std::string_view prefix, std::string_view name) {
  if (!configured.empty()) {
    if (configured.size() != count)
      throw std::invalid_argument(std::string(name) + " has the wrong element count");
    return configured;
  }
  std::vector<std::string> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
    result.push_back(std::string(prefix) + std::to_string(index));
  return result;
}

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
    return std::tuple{left.transitions.size(), std::string_view(place_names[left.source]), std::string_view(place_names[left.target]), left.source, left.target}
         < std::tuple{right.transitions.size(), std::string_view(place_names[right.source]), std::string_view(place_names[right.target]), right.source, right.target};
  });
  return result;
}

// Public hidden routing first fires enabled path prefixes, then recomputes
// paths and explores blocked dependencies. Visiting a target is distinct from
// firing it: a failed recursive attempt still suppresses later visits.
template <class Required, class Candidates, class Enabled, class Emit, class Trial>
void route_hidden_dependencies(std::size_t transition_count, std::size_t target,
                               bool exhaustive, const Candidates& candidates,
                               const Required& required,
                               const Enabled& enabled, const Emit& emit,
                               const Trial& trial) {
  std::vector<bool> visited(transition_count, false);
  // Both modes allow one dependency expansion below the selected transition.
  // Exhaustive mode differs by trying each candidate transactionally before
  // it commits any prefix; it does not reduce the search depth.
  constexpr std::size_t recursion_limit = 2;
  std::size_t calls = 0;
  const auto explore = [&](auto&& self, std::size_t selected, std::size_t depth) -> void {
    if (depth >= recursion_limit || visited[selected]) return;
    if (++calls > kReplayStructuralSearchCap)
      throw std::runtime_error("Petri replay exceeded the pre-event routing state limit");
    visited[selected] = true;
    // Freeze the selected transition's deficient input keys for this
    // invocation.  Candidate paths still recompute from the caller's live
    // marking keys, but the target requirement must not change after an
    // enabled prefix has fired.
    const auto required_keys = required(selected);
    const auto initial_paths = candidates(selected, required_keys);
    if (exhaustive) {
      // The public exhaustive helper trials every candidate against a marking
      // copy.  Only the prefix returned by a successful trial may affect the
      // caller's marking; failed candidates are discarded completely.
      for (const auto& path : initial_paths) {
        const auto accepted = trial(selected, path, visited);
        if (accepted.empty()) continue;
        for (const auto transition : accepted) {
          emit(transition);
        }
        if (enabled(selected)) return;
      }
    } else {
      for (const auto& path : initial_paths) {
        bool progressed = false;
        for (const auto transition : path.transitions) {
          if (visited[transition]) continue;
          if (!enabled(transition)) break;
          emit(transition);
          visited[transition] = true;
          progressed = true;
        }
        if (!progressed || enabled(selected)) break;
      }
    }
    if (enabled(selected)) return;
    const auto paths = candidates(selected, required_keys);
    for (const auto& path : paths) {
      for (const auto transition : path.transitions) {
        if (visited[transition]) continue;
        if (!enabled(transition)) self(self, transition, depth + 1);
        // Recursion may have visited this transition without firing it.
        if (enabled(transition)) {
          emit(transition);
          visited[transition] = true;
        }
        // A still-blocked dependency does not discard later path steps.
        if (enabled(selected)) break;
      }
      // Non-exhaustive routing keeps examining later paths even after one
      // path enables the selected transition.  Exhaustive mode retains the
      // original outer break after its transactional trial fallback.
      if (exhaustive && enabled(selected)) break;
    }
  };
  explore(explore, target, 0);
}

}  // namespace minera::detail
