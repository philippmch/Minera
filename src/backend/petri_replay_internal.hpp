#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
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

// Precision needs every visible activity a marking eventually enables through
// silent transitions. That search is bounded (silent cycles can make it
// unbounded); when the bound is hit the enabled set is incomplete, so the
// search throws this instead of returning a partial answer.
struct SilentSearchBudgetExhausted : std::runtime_error {
  SilentSearchBudgetExhausted()
      : std::runtime_error(
            "precision is not computable: the silent-transition search for "
            "eventually enabled activities exceeded its budget") {}
};

// Expansion budget of that search: markings expanded per queried marking.
inline constexpr std::size_t kEnabledSearchBudget = std::size_t{1} << 16;

// Lowers the budget on the current thread while alive (tests force exhaustion
// on small nets with it). Zero restores the default.
std::size_t enabled_search_budget() noexcept;
class ScopedEnabledSearchBudget {
 public:
  explicit ScopedEnabledSearchBudget(std::size_t budget) noexcept;
  ~ScopedEnabledSearchBudget();
  ScopedEnabledSearchBudget(const ScopedEnabledSearchBudget&) = delete;
  ScopedEnabledSearchBudget& operator=(const ScopedEnabledSearchBudget&) = delete;

 private:
  std::size_t previous_;
};

// Visible activities reachable from the exact initial marking through hidden
// transitions. This query does not calculate trace fitness. Throws
// SilentSearchBudgetExhausted when the search cannot finish.
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

// Explicit names win; otherwise a net that carries reference place names
// (process-tree and BPMN conversions) uses them, and any other net the
// positional `p<index>` placeholders.
inline std::vector<std::string> replay_place_names(
    const std::vector<std::string>& configured, const PetriNet& net) {
  if (configured.empty() && net.place_names()) {
    if (net.place_names()->size() != net.place_count())
      throw std::invalid_argument("Petri place name metadata has the wrong size");
    return *net.place_names();
  }
  return replay_element_names(configured, net.place_count(), "p", "place_names");
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
                                       const ReplayMarkingKeys& final_keys,
                                       const std::vector<std::string>* place_names = nullptr) {
  if (place_names && place_names->size() != initial_keys.size())
    throw std::invalid_argument("cleaning place names must contain one entry per marking place");
  const auto contains_place = [&](const std::set<std::size_t>& places, std::size_t place) {
    if (!place_names) return places.contains(place);
    return std::any_of(places.begin(), places.end(), [&](const auto existing) {
      return (*place_names)[existing] == (*place_names)[place];
    });
  };
  const auto insert_place = [&](std::set<std::size_t>& places, std::size_t place) {
    if (contains_place(places, place)) return false;
    places.insert(place);
    return true;
  };
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
    if (depth < 6) {
      std::queue<std::size_t> pending;
      for (const auto place : component) if (!contains_place(visited, place)) pending.push(place);
      while (!pending.empty()) {
        const auto place = pending.front(); pending.pop();
        if (!insert_place(visited, place)) continue;
        std::vector<std::vector<std::size_t>> branches;
        for (const auto t : outgoing[place]) {
          std::vector<std::size_t> choices;
          for (const auto output : outputs[t])
            if (!contains_place(visited, output)) choices.push_back(output);
          if (choices.size() > 1) branches.push_back(std::move(choices));
          else if (choices.size() == 1 && insert_place(component, choices[0]))
            pending.push(choices[0]);
        }
        // Recurse before draining the linear worklist. A queued place can
        // lead to another split and belongs to each of these branches.
        // Already visited output names do not contribute a split choice;
        // distinct unvisited identities still count separately.
        for (const auto& choices : branches) {
          for (const auto output : choices) {
            auto branch = component;
            insert_place(branch, output);
            self(self, std::move(branch), visited, depth + 1);
          }
        }
      }
    }
    // Prefix components are part of the public traversal. Each split is
    // explored independently, with a separate visited set for every branch.
    // Public components contain place names. Traversal stops when a name is
    // already present; cleaning membership includes every place with that
    // name, including aliases outside the traversed topology.
    std::vector<std::size_t> members;
    if (place_names) {
      for (std::size_t place = 0; place < place_names->size(); ++place)
        if (contains_place(component, place)) members.push_back(place);
    } else members.assign(component.begin(), component.end());
    if (unique.insert(members).second) {
      if (members.size() > 1'000'000 - stored_members)
        throw std::runtime_error("Petri usage replay exceeded the cleaning-component size limit");
      stored_members += members.size();
    }
  };
  expand(expand, {root}, {}, 0);
  return {unique.begin(), unique.end()};
}

inline ReplayMarkingKeys cleaning_introduced_places(
    const ReplayMarkingKeys& keys, const ReplayMarkingKeys& before,
    const std::vector<std::string>& place_names) {
  std::set<std::string> previous_names;
  for (std::size_t place = 0; place < before.size(); ++place)
    if (before[place]) previous_names.insert(place_names[place]);
  ReplayMarkingKeys introduced(keys.size(), false);
  for (std::size_t place = 0; place < keys.size(); ++place)
    introduced[place] = keys[place] && !previous_names.contains(place_names[place]);
  return introduced;
}

inline ReplayMarkingKeys cleaning_unique_place_names(
    const ReplayMarkingKeys& keys, const std::vector<std::string>& place_names) {
  std::map<std::string, std::size_t> occurrences;
  for (std::size_t place = 0; place < keys.size(); ++place)
    if (keys[place]) ++occurrences[place_names[place]];
  ReplayMarkingKeys unique(keys.size(), false);
  for (std::size_t place = 0; place < keys.size(); ++place)
    unique[place] = keys[place] && occurrences[place_names[place]] == 1;
  return unique;
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
//
// The breadth-first tree grown from a source place depends only on the net,
// the silent outgoing order and the length bound, never on the marking, so
// SilentPlacePathIndex grows each source's tree once and every later query
// only reads paths off it. silent_place_paths answers one query with a
// throwaway index; both produce the same paths, order and errors.
class SilentPlacePathIndex {
 public:
  SilentPlacePathIndex(const PetriNet& net, std::size_t place_count,
                       std::size_t max_path_length = 51,
                       const std::vector<std::vector<std::size_t>>* outgoing_order = nullptr)
      : net_(&net), max_path_length_(max_path_length), outgoing_(place_count),
        trees_(place_count) {
    if (outgoing_order) order_ = *outgoing_order;
    has_order_ = outgoing_order != nullptr;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      if (!transition.silent()) continue;
      std::set<std::size_t> inputs(transition.inputs.begin(), transition.inputs.end());
      inputs.insert(transition.reset_inputs.begin(), transition.reset_inputs.end());
      inputs.insert(transition.inhibitor_inputs.begin(), transition.inhibitor_inputs.end());
      for (const auto place : inputs) outgoing_[place].push_back(t);
    }
    if (outgoing_order) {
      outgoing_.assign(place_count, {});
      for (std::size_t place = 0; place < outgoing_order->size(); ++place)
        for (const auto transition : (*outgoing_order)[place])
          if (net.transitions()[transition].silent()) outgoing_[place].push_back(transition);
    }
  }

  // Whether this index answers queries with these parameters.
  [[nodiscard]] bool matches(const PetriNet& net, std::size_t place_count,
                             std::size_t max_path_length,
                             const std::vector<std::vector<std::size_t>>* outgoing_order) const {
    if (net_ != &net || trees_.size() != place_count || max_path_length_ != max_path_length ||
        has_order_ != (outgoing_order != nullptr))
      return false;
    return !outgoing_order || *outgoing_order == order_;
  }

  [[nodiscard]] std::vector<HiddenPlacePath> paths(
      const ReplayMarkingKeys& source_keys, const ReplayMarkingKeys& target_keys,
      const std::vector<std::string>& place_names) const {
    std::vector<HiddenPlacePath> result;
    std::size_t stored_steps = 0;
    // The marked targets, in ascending order, listed once for all sources.
    std::vector<std::size_t> targets;
    for (std::size_t target = 0; target < target_keys.size(); ++target)
      if (target_keys[target]) targets.push_back(target);
    for (std::size_t source = 0; source < source_keys.size(); ++source) {
      if (!source_keys[source]) continue;
      const Tree& tree = tree_from(source);
      if (tree.overflow)
        throw std::runtime_error("Petri usage replay exceeded the silent place-path state limit");
      for (const auto target : targets) {
        if (tree.parent[target] == kNone ||
            (target == source && tree.cycle_parent == kNone)) continue;
        HiddenPlacePath path{source, target, {}};
        auto place = target;
        if (target == source) {
          path.transitions.push_back(tree.cycle_transition);
          place = tree.cycle_parent;
        }
        for (; place != source; place = tree.parent[place]) path.transitions.push_back(tree.via[place]);
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

 private:
  static constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();
  struct Tree {
    std::vector<std::size_t> parent, via;
    std::size_t cycle_parent = kNone, cycle_transition = kNone;
    // The search passed kReplayStructuralSearchCap; a query from this source
    // fails, as the uncached search did.
    bool overflow = false;
  };

  const Tree& tree_from(std::size_t source) const {
    auto& slot = trees_[source];
    if (slot) return *slot;
    const std::size_t places = trees_.size();
    Tree tree;
    tree.parent.assign(places, kNone);
    tree.via.assign(places, kNone);
    std::vector<std::size_t> distance(places, 0);
    tree.parent[source] = source;
    std::queue<std::size_t> pending;
    pending.push(source);
    std::size_t visited = 1;
    while (!pending.empty() && !tree.overflow) {
      const auto place = pending.front(); pending.pop();
      if (distance[place] >= max_path_length_) continue;
      for (const auto t : outgoing_[place]) {
        for (const auto output : net_->transitions()[t].outputs) {
          // Retain the first nonempty source cycle without changing the empty
          // BFS seed. Re-enqueuing the source would corrupt its other paths.
          if (output == source && tree.cycle_parent == kNone) {
            tree.cycle_parent = place;
            tree.cycle_transition = t;
          }
          if (tree.parent[output] != kNone) continue;
          if (++visited > kReplayStructuralSearchCap) {
            tree.overflow = true;
            break;
          }
          tree.parent[output] = place;
          tree.via[output] = t;
          distance[output] = distance[place] + 1;
          pending.push(output);
        }
        if (tree.overflow) break;
      }
    }
    if (tree.overflow) {
      tree.parent.clear();
      tree.via.clear();
    }
    slot = std::move(tree);
    return *slot;
  }

  const PetriNet* net_;
  std::size_t max_path_length_;
  bool has_order_ = false;
  std::vector<std::vector<std::size_t>> order_;
  std::vector<std::vector<std::size_t>> outgoing_;
  mutable std::vector<std::optional<Tree>> trees_;
};

inline std::vector<HiddenPlacePath> silent_place_paths(
    const ReplayMarkingKeys& source_keys, const ReplayMarkingKeys& target_keys, const PetriNet& net,
    const std::vector<std::string>& place_names,
    std::size_t max_path_length = 51,
    const std::vector<std::vector<std::size_t>>* outgoing_order = nullptr) {
  return SilentPlacePathIndex(net, source_keys.size(), max_path_length, outgoing_order)
      .paths(source_keys, target_keys, place_names);
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
        // A still-blocked dependency does not discard later path steps, and
        // enabling the selected transition does not end the pass either: the
        // reference keeps firing (or recursing into) every remaining unvisited
        // step of every recomputed path, in both exploration modes.
      }
    }
  };
  explore(explore, target, 0);
}

}  // namespace minera::detail
