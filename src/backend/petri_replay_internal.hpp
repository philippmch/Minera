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

namespace minera { struct TokenReplayOptions; }

namespace minera::detail {

// Visible activities reachable from the exact initial marking through hidden
// transitions. This query does not calculate trace fitness.
std::vector<std::size_t> initial_enabled_token_activities(
    const PetriNet& net, const TokenReplayOptions& options);

using ReplayMarkingKeys = std::vector<bool>;
using CleaningComponents = std::vector<std::vector<std::size_t>>;
inline constexpr std::size_t kReplayStructuralSearchCap = 4096;
inline constexpr std::size_t kReplayStructuralTrialMemoryCap = 64u * 1024u * 1024u;

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

inline PetriTransitionNameList replay_transition_names(
    const std::optional<PetriTransitionNameList>& nullable_names,
    const std::vector<std::string>& string_names, const PetriNet& net) {
  if (nullable_names) {
    if (nullable_names->size() != net.transitions().size())
      throw std::invalid_argument("nullable_transition_names must contain one entry per transition");
    return *nullable_names;
  }
  if (string_names.empty() && net.transition_names()) {
    if (net.transition_names()->size() != net.transitions().size())
      throw std::invalid_argument("Petri transition name metadata has the wrong size");
    return *net.transition_names();
  }
  const auto strings = replay_element_names(string_names, net.transitions().size(), "t", "transition_names");
  return PetriTransitionNameList(strings.begin(), strings.end());
}

inline void validate_forward_transition_name_order(const PetriTransitionNameList& names) {
  if (names.size() < 2) return;
  const auto first_null = std::find(names.begin(), names.end(), std::nullopt);
  if (first_null == names.end()) return;
  // Public stable sorting compares the second element against the first.
  // A later first null necessarily compares against a preceding string.
  const bool at_front = first_null == names.begin();
  const auto left = at_front && names[1] ? "str" : "NoneType";
  const auto right = at_front ? "NoneType" : "str";
  throw std::invalid_argument(std::string("'<' not supported between instances of '")
      + left + "' and '" + right + "'");
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
  // Detached marking slots are isolated vertices in replay's marking domain;
  // they are not added to the net's root topology.
  std::vector<std::vector<std::size_t>> outgoing(initial_keys.size());
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

// Exhaustive hidden routing trials pool the transitions from all structural
// candidate paths, then search each transition at most once per trial.  The
// marking and membership keys are copied into every breadth-first node so a
// failed trial cannot affect the caller's replay state.
template <class Marking, class MarkingKeys, class Enabled, class Fire>
std::vector<std::size_t> exhaustive_hidden_transition_trial(
    std::size_t selected, const std::vector<HiddenPlacePath>& paths,
    const std::vector<bool>& outer_visited, const Marking& initial_marking,
    const MarkingKeys& initial_keys, const Enabled& enabled, const Fire& fire) {
  std::vector<std::size_t> pool;
  std::set<std::size_t> pooled;
  for (const auto& path : paths) {
    for (const auto transition : path.transitions) {
      if (pooled.insert(transition).second) pool.push_back(transition);
    }
  }

  struct Node {
    Marking marking;
    MarkingKeys keys;
    std::vector<bool> used;
    std::vector<std::size_t> path;
  };

  const auto checked_add = [](std::size_t left, std::size_t right,
                             std::size_t& result) {
    if (right > std::numeric_limits<std::size_t>::max() - left) return false;
    result = left + right;
    return true;
  };
  const auto checked_product = [](std::size_t left, std::size_t right,
                                  std::size_t& result) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left)
      return false;
    result = left * right;
    return true;
  };
  const auto payload_bytes = [&](std::size_t marking_size, std::size_t keys_size,
                                 std::size_t used_size, std::size_t path_size) {
    std::size_t bytes = sizeof(Node);
    const auto add_product = [&](std::size_t count, std::size_t element_size) {
      std::size_t product = 0;
      if (!checked_product(count, element_size, product)) return false;
      return checked_add(bytes, product, bytes);
    };
    if (!add_product(marking_size, sizeof(typename Marking::value_type)) ||
        !add_product(keys_size, sizeof(typename MarkingKeys::value_type)) ||
        !add_product(used_size, sizeof(bool)) ||
        !add_product(path_size, sizeof(std::size_t)))
      return std::numeric_limits<std::size_t>::max();
    return bytes;
  };
  const auto memory_error = [] {
    throw std::runtime_error(
        "Petri replay exceeded the exhaustive hidden trial memory limit");
  };
  const auto initial_bytes = payload_bytes(initial_marking.size(), initial_keys.size(),
                                           outer_visited.size(), 0);
  if (initial_bytes > kReplayStructuralTrialMemoryCap) memory_error();

  std::queue<Node> pending;
  pending.push(Node{initial_marking, initial_keys, outer_visited, {}});
  std::size_t retained_bytes = initial_bytes;
  std::size_t queued = 1;
  std::size_t explored = 0;
  while (!pending.empty()) {
    if (++explored > kReplayStructuralSearchCap)
      throw std::runtime_error("Petri replay exceeded the exhaustive hidden trial state limit");
    Node current = std::move(pending.front());
    pending.pop();
    if (enabled(current.marking, selected)) return current.path;
    for (const auto transition : pool) {
      if (current.used[transition] || !enabled(current.marking, transition)) continue;
      const auto next_path_size = current.path.size() + 1;
      const auto next_bytes = payload_bytes(current.marking.size(), current.keys.size(),
                                            current.used.size(), next_path_size);
      std::size_t retained_with_next = 0;
      if (next_bytes == std::numeric_limits<std::size_t>::max() ||
          !checked_add(retained_bytes, next_bytes, retained_with_next) ||
          retained_with_next > kReplayStructuralTrialMemoryCap)
        memory_error();
      Node next = current;
      fire(next.marking, next.keys, transition);
      next.used[transition] = true;
      next.path.push_back(transition);
      retained_bytes = retained_with_next;
      if (++queued > kReplayStructuralSearchCap)
        throw std::runtime_error("Petri replay exceeded the exhaustive hidden trial queue limit");
      pending.push(std::move(next));
    }
    retained_bytes -= payload_bytes(current.marking.size(), current.keys.size(),
                                    current.used.size(), current.path.size());
  }
  return {};
}

// Structural place paths deliberately ignore enabling while searching. Replay
// can retain an enabled prefix even when a later multi-input join is blocked.
inline std::vector<HiddenPlacePath> silent_place_paths(
    const ReplayMarkingKeys& source_keys, const ReplayMarkingKeys& target_keys, const PetriNet& net,
    const std::vector<std::string>& place_names,
    std::size_t max_path_length = 51) {
  std::vector<std::vector<std::size_t>> outgoing(source_keys.size());
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
      // The public exhaustive helper pools candidate transitions and searches
      // the pool transactionally. Only a successful path affects the caller.
      const auto accepted = trial(selected, initial_paths, visited);
      for (const auto transition : accepted) emit(transition);
      if (enabled(selected)) return;
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
