#include "minera/inductive_miner.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

using ActId = std::size_t;

// A trace (as activity ids) together with how many cases share it. Working on
// these variants instead of raw cases keeps every step proportional to the
// number of distinct behaviours rather than the number of cases.
struct WeightedTrace {
  std::vector<ActId> activities;
  std::uint64_t count = 1;
};

using Sublog = std::vector<WeightedTrace>;

// Dense, matrix-friendly view of a sublog. Activities are renumbered to
// 0..size()-1 for cut detection and mapped back to global ids via to_global.
// The `follows`/`succ`/`is_start`/`is_end` relations are what the cut detectors
// read. They start out unfiltered; when no cut is found, the infrequent variant
// recomputes them with the noise filter (from the retained raw counts) and the
// cut detectors run once more.
struct LocalDfg {
  std::vector<ActId> to_global;
  std::unordered_map<ActId, std::size_t> to_local;
  std::vector<std::vector<char>> follows;     // follows[a][b]: a -> b kept
  std::vector<std::vector<std::size_t>> succ;  // adjacency list of follows
  std::vector<char> is_start;
  std::vector<char> is_end;

  // Raw, unfiltered weighted counts (a -> b directly-follows, and how often each
  // activity starts/ends a trace), summed over the weighted sublog.
  std::vector<std::vector<std::uint64_t>> edge_count;
  std::vector<std::uint64_t> start_count;
  std::vector<std::uint64_t> end_count;

  [[nodiscard]] std::size_t size() const { return to_global.size(); }
};

// Recomputes the boolean relations the cut detectors use from the raw counts,
// dropping behaviour that falls below the noise threshold:
//   * an edge a->b survives only when its count is greater than `f` times a's
//     strongest outgoing behaviour -- its heaviest outgoing edge or how often
//     the trace ends at a, whichever is larger (so a node always keeps its
//     dominant successor, and an end activity's stray edges count as noise);
//   * an activity counts as a start when its start count clears `f` times the
//     most frequent start; every observed end activity remains an end.
// With f == 0 nothing is dropped: every observed edge and every observed
// start/end is kept, exactly reproducing the classic Inductive Miner.
void apply_noise_filter(LocalDfg& dfg, double f) {
  const std::size_t n = dfg.size();
  dfg.follows.assign(n, std::vector<char>(n, 0));
  dfg.succ.assign(n, {});
  dfg.is_start.assign(n, 0);
  dfg.is_end.assign(n, 0);

  for (std::size_t a = 0; a < n; ++a) {
    // Ending the trace competes as a's dominant behaviour too: without this,
    // a stray outgoing edge of an activity that mostly *ends* traces is its
    // strongest (often only) edge and can never be dropped as noise.
    std::uint64_t max_out = dfg.end_count[a];
    for (std::size_t b = 0; b < n; ++b) {
      max_out = std::max(max_out, dfg.edge_count[a][b]);
    }
    const double cutoff = f * static_cast<double>(max_out);
    for (std::size_t b = 0; b < n; ++b) {
      const std::uint64_t count = dfg.edge_count[a][b];
      if (count > 0 && static_cast<double>(count) > cutoff) {
        dfg.follows[a][b] = 1;
        dfg.succ[a].push_back(b);
      }
    }
  }

  std::uint64_t max_start = 0;
  for (std::size_t a = 0; a < n; ++a) {
    max_start = std::max(max_start, dfg.start_count[a]);
  }
  for (std::size_t a = 0; a < n; ++a) {
    if (dfg.start_count[a] > 0 &&
        static_cast<double>(dfg.start_count[a]) >= f * static_cast<double>(max_start)) {
      dfg.is_start[a] = 1;
    }
    // The infrequent variant filters start activities but preserves every
    // observed end activity. End counts still participate in the outgoing
    // edge baseline above, so stray departures from an end remain removable.
    if (dfg.end_count[a] > 0) dfg.is_end[a] = 1;
  }
}

LocalDfg build_local_dfg(const Sublog& log) {
  LocalDfg dfg;
  for (const auto& wt : log) {
    for (const ActId act : wt.activities) {
      if (dfg.to_local.emplace(act, dfg.to_global.size()).second) {
        dfg.to_global.push_back(act);
      }
    }
  }

  const std::size_t n = dfg.size();
  dfg.edge_count.assign(n, std::vector<std::uint64_t>(n, 0));
  dfg.start_count.assign(n, 0);
  dfg.end_count.assign(n, 0);

  for (const auto& wt : log) {
    if (wt.activities.empty()) {
      continue;
    }
    dfg.start_count[dfg.to_local.at(wt.activities.front())] += wt.count;
    dfg.end_count[dfg.to_local.at(wt.activities.back())] += wt.count;
    for (std::size_t i = 1; i < wt.activities.size(); ++i) {
      const std::size_t a = dfg.to_local.at(wt.activities[i - 1]);
      const std::size_t b = dfg.to_local.at(wt.activities[i]);
      dfg.edge_count[a][b] += wt.count;
    }
  }

  apply_noise_filter(dfg, 0.0);
  return dfg;
}

LocalDfg build_local_dfg(const DirectlyFollowsGraph& graph) {
  LocalDfg dfg;
  const auto& names = graph.activities();
  const std::size_t n = names.size();
  dfg.to_global.resize(n);
  for (std::size_t activity = 0; activity < n; ++activity) {
    dfg.to_global[activity] = activity;
    dfg.to_local.emplace(activity, activity);
  }
  dfg.edge_count.assign(n, std::vector<std::uint64_t>(n, 0));
  dfg.start_count.assign(n, 0);
  dfg.end_count.assign(n, 0);
  for (std::size_t activity = 0; activity < n; ++activity) {
    dfg.start_count[activity] =
        graph.start_activity_count(names[activity]);
    dfg.end_count[activity] = graph.end_activity_count(names[activity]);
  }
  for (const auto& edge : graph.edges()) {
    if (edge.source >= n || edge.target >= n)
      throw std::invalid_argument(
          "Inductive: graph edge activity id is out of range");
    dfg.edge_count[edge.source][edge.target] += edge.count;
  }
  apply_noise_filter(dfg, 0.0);
  return dfg;
}

// Transitive closure of the follows relation, computed lazily: only the
// sequence cut needs it, and an exclusive cut (tried first) often makes it
// unnecessary. Adjacency-list BFS keeps this at O(n * (n + edges)) rather than
// the O(n^3) of a dense all-pairs scan.
std::vector<std::vector<char>> reachability(const LocalDfg& dfg) {
  const std::size_t n = dfg.size();
  std::vector<std::vector<char>> reachable(n, std::vector<char>(n, 0));
  std::vector<std::size_t> stack;
  for (std::size_t source = 0; source < n; ++source) {
    auto& seen = reachable[source];
    stack.assign(dfg.succ[source].begin(), dfg.succ[source].end());
    for (const std::size_t t : dfg.succ[source]) {
      seen[t] = 1;
    }
    while (!stack.empty()) {
      const std::size_t cur = stack.back();
      stack.pop_back();
      for (const std::size_t t : dfg.succ[cur]) {
        if (!seen[t]) {
          seen[t] = 1;
          stack.push_back(t);
        }
      }
    }
  }
  return reachable;
}

// Minimal union-find for grouping activities during cut detection.
struct UnionFind {
  std::vector<std::size_t> parent;

  explicit UnionFind(std::size_t n) : parent(n) {
    std::iota(parent.begin(), parent.end(), std::size_t{0});
  }

  std::size_t find(std::size_t x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  }

  void unite(std::size_t a, std::size_t b) { parent[find(a)] = find(b); }
};

// Collects union-find classes into groups of local ids, ordered by first
// appearance so results are deterministic.
std::vector<std::vector<std::size_t>> classes_to_groups(UnionFind& uf,
                                                        std::size_t n) {
  std::unordered_map<std::size_t, std::size_t> root_to_index;
  std::vector<std::vector<std::size_t>> groups;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t root = uf.find(i);
    auto [it, inserted] = root_to_index.emplace(root, groups.size());
    if (inserted) {
      groups.emplace_back();
    }
    groups[it->second].push_back(i);
  }
  return groups;
}

std::vector<std::vector<ActId>> to_global(
    const LocalDfg& dfg,
    const std::vector<std::vector<std::size_t>>& local_groups) {
  std::vector<std::vector<ActId>> groups;
  groups.reserve(local_groups.size());
  for (const auto& local_group : local_groups) {
    std::vector<ActId>& out = groups.emplace_back();
    out.reserve(local_group.size());
    for (const std::size_t loc : local_group) {
      out.push_back(dfg.to_global[loc]);
    }
  }
  return groups;
}

// --- Cut detection ---------------------------------------------------------
//
// Each detector returns the activity partition (in global ids) for its cut, or
// nullopt when the sublog does not exhibit that structure. They are tried in
// the order exclusive -> sequence -> parallel -> loop.

// Exclusive choice: connected components of the undirected directly-follows
// graph. Disconnected activity sets are mutually exclusive alternatives.
std::optional<std::vector<std::vector<ActId>>> exclusive_cut(
    const LocalDfg& dfg) {
  const std::size_t n = dfg.size();
  UnionFind uf(n);
  for (std::size_t a = 0; a < n; ++a) {
    for (const std::size_t b : dfg.succ[a]) {
      uf.unite(a, b);
    }
  }
  auto groups = classes_to_groups(uf, n);
  if (groups.size() < 2) {
    return std::nullopt;
  }
  return to_global(dfg, groups);
}

// Sequence: two activities belong to the same group when reachability between
// them is symmetric (mutually reachable, i.e. on a cycle, or mutually
// unreachable). Distinct groups then admit a strict order and are emitted in
// that order.
std::optional<std::vector<std::vector<ActId>>> sequence_cut(
    const LocalDfg& dfg) {
  const std::size_t n = dfg.size();
  const std::vector<std::vector<char>> reachable = reachability(dfg);

  UnionFind uf(n);
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = a + 1; b < n; ++b) {
      if (static_cast<bool>(reachable[a][b]) ==
          static_cast<bool>(reachable[b][a])) {
        uf.unite(a, b);
      }
    }
  }

  auto local_groups = classes_to_groups(uf, n);
  const std::size_t g = local_groups.size();
  if (g < 2) {
    return std::nullopt;
  }

  std::vector<std::size_t> group_of(n);
  for (std::size_t k = 0; k < g; ++k) {
    for (const std::size_t loc : local_groups[k]) {
      group_of[loc] = k;
    }
  }

  // Reachability lifted to groups. The ordering key is computed from the
  // representative activity's transitive predecessor/successor counts, as in
  // the sequence-cut contract; the local-id tie break keeps equal keys stable.
  std::vector<std::vector<char>> greach(g, std::vector<char>(g, 0));
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = 0; b < n; ++b) {
      if (reachable[a][b] && group_of[a] != group_of[b]) {
        greach[group_of[a]][group_of[b]] = 1;
      }
    }
  }

  std::vector<std::size_t> order(g);
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::vector<std::size_t> order_key(g, 0);
  for (std::size_t k = 0; k < g; ++k) {
    const auto representative = local_groups[k].front();
    std::size_t predecessors = 0;
    std::size_t successors = 0;
    for (std::size_t activity = 0; activity < n; ++activity) {
      predecessors += static_cast<std::size_t>(reachable[activity][representative]);
      successors += static_cast<std::size_t>(reachable[representative][activity]);
    }
    order_key[k] = predecessors + (n - successors);
  }
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (order_key[a] != order_key[b]) return order_key[a] < order_key[b];
    return a < b;
  });

  // Reject anything that is not a genuine sequence: no later group may reach an
  // earlier one. Such logs fall through to the parallel/loop detectors.
  for (std::size_t i = 0; i < g; ++i) {
    for (std::size_t j = i + 1; j < g; ++j) {
      if (greach[order[j]][order[i]]) {
        return std::nullopt;
      }
    }
  }

  std::vector<std::vector<std::size_t>> ordered;
  ordered.reserve(g);
  for (const std::size_t k : order) {
    ordered.push_back(std::move(local_groups[k]));
  }
  return to_global(dfg, ordered);
}

// Strict sequence cuts merge groups that can be skipped as a unit. This keeps
// optional middle sections together instead of exposing each skipped group as
// an independent sequence child.
std::optional<std::vector<std::vector<ActId>>> strict_sequence_cut(
    const LocalDfg& dfg) {
  auto global_groups = sequence_cut(dfg);
  if (!global_groups || global_groups->size() < 2) return std::nullopt;

  // Cut detectors expose global activity ids to the recursion.  The
  // reachability and boundary arrays of a projected graph, however, are
  // indexed by its compact local ids, so normalize the groups before doing
  // any graph arithmetic.
  std::vector<std::vector<std::size_t>> groups;
  groups.reserve(global_groups->size());
  for (const auto& global_group : *global_groups) {
    auto& local_group = groups.emplace_back();
    local_group.reserve(global_group.size());
    for (const ActId activity : global_group) {
      local_group.push_back(dfg.to_local.at(activity));
    }
  }

  const std::size_t group_count = groups.size();
  std::vector<std::size_t> group_of(dfg.size());
  for (std::size_t group = 0; group < group_count; ++group) {
    for (const std::size_t activity : groups[group]) {
      group_of[activity] = group;
    }
  }

  constexpr std::int64_t kNegInf = -1;
  const auto kPosInf = static_cast<std::int64_t>(group_count);
  std::vector<std::int64_t> min_from(group_count, kPosInf);
  std::vector<std::int64_t> max_to(group_count, kNegInf);
  std::vector<char> has_start(group_count, 0);
  std::vector<char> has_end(group_count, 0);
  for (std::size_t group = 0; group < group_count; ++group) {
    for (const std::size_t activity : groups[group]) {
      if (dfg.is_start[activity]) {
        min_from[group] = kNegInf;
        has_start[group] = 1;
      }
      if (dfg.is_end[activity]) {
        max_to[group] = kPosInf;
        has_end[group] = 1;
      }
    }
  }
  for (std::size_t source = 0; source < dfg.size(); ++source) {
    for (const std::size_t target : dfg.succ[source]) {
      const auto source_group = group_of[source];
      const auto target_group = group_of[target];
      if (source_group == target_group) continue;
      min_from[target_group] =
          std::min(min_from[target_group], static_cast<std::int64_t>(source_group));
      max_to[source_group] =
          std::max(max_to[source_group], static_cast<std::int64_t>(target_group));
    }
  }

  for (std::size_t pivot = 0; pivot < group_count; ++pivot) {
    bool skippable = false;
    for (std::size_t before = 0; before < pivot && !skippable; ++before) {
      for (std::size_t after = pivot + 1; after < group_count; ++after) {
        if (max_to[before] >= static_cast<std::int64_t>(after)) {
          skippable = true;
          break;
        }
      }
    }
    for (std::size_t after = pivot + 1; after < group_count && !skippable;
         ++after) {
      if (has_start[after]) skippable = true;
    }
    for (std::size_t before = 0; before < pivot && !skippable; ++before) {
      if (has_end[before]) skippable = true;
    }
    if (!skippable) continue;

    for (std::size_t group = pivot; group > 0;) {
      const auto previous = group - 1;
      if (max_to[previous] > static_cast<std::int64_t>(pivot)) break;
      groups[pivot].insert(groups[pivot].end(), groups[previous].begin(),
                           groups[previous].end());
      groups[previous].clear();
      group = previous;
    }
    for (std::size_t group = pivot + 1; group < group_count;) {
      if (min_from[group] < static_cast<std::int64_t>(pivot)) break;
      groups[pivot].insert(groups[pivot].end(), groups[group].begin(),
                           groups[group].end());
      groups[group].clear();
      ++group;
    }
  }

  std::vector<std::vector<ActId>> result;
  for (const auto& local_group : groups) {
    if (local_group.empty()) continue;
    auto& global_group = result.emplace_back();
    global_group.reserve(local_group.size());
    for (const std::size_t activity : local_group) {
      global_group.push_back(dfg.to_global[activity]);
    }
  }
  return result.size() > 1
             ? std::optional<std::vector<std::vector<ActId>>>(std::move(result))
             : std::nullopt;
}

// Parallel: activities that are not concurrent (not connected in both
// directions) must share a branch. Each resulting branch must be able to both
// start and end the trace.
std::optional<std::vector<std::vector<ActId>>> parallel_cut(
    const LocalDfg& dfg) {
  const std::size_t n = dfg.size();
  UnionFind uf(n);
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = a + 1; b < n; ++b) {
      const bool concurrent = dfg.follows[a][b] && dfg.follows[b][a];
      if (!concurrent) {
        uf.unite(a, b);
      }
    }
  }

  auto local_groups = classes_to_groups(uf, n);
  if (local_groups.size() < 2) {
    return std::nullopt;
  }
  for (const auto& group : local_groups) {
    bool has_start = false;
    bool has_end = false;
    for (const std::size_t loc : group) {
      has_start = has_start || dfg.is_start[loc];
      has_end = has_end || dfg.is_end[loc];
    }
    if (!has_start || !has_end) {
      return std::nullopt;
    }
  }
  return to_global(dfg, local_groups);
}

// Loop: the body (child 0) holds every start and end activity; the remaining
// connected components are candidate redo paths. A component is a valid redo
// only when it satisfies the formal loop-cut conditions:
//   * the body is left into the redo only from end activities, and the redo
//     returns to the body only at start activities;
//   * every redo entry reached from an end activity is reachable from *all*
//     end activities, and every redo exit reaching a start activity reaches
//     *all* start activities (completeness).
// Components that violate these are folded back into the body until the
// partition is stable; if no redo survives there is no loop cut.
std::optional<std::vector<std::vector<ActId>>> loop_cut(const LocalDfg& dfg) {
  const std::size_t n = dfg.size();
  if (n == 0 || std::none_of(dfg.follows.begin(), dfg.follows.end(),
                             [](const auto& row) {
                               return std::any_of(row.begin(), row.end(),
                                                  [](char value) { return value != 0; });
                             })) {
    return std::nullopt;
  }

  // The body starts with the union of boundary activities. Removing boundary
  // nodes and their incident edges leaves weakly connected redo components.
  std::vector<std::vector<std::size_t>> groups(1);
  for (std::size_t activity = 0; activity < n; ++activity)
    if (dfg.is_start[activity] || dfg.is_end[activity])
      groups.front().push_back(activity);

  std::vector<char> assigned(n, 0);
  for (const auto activity : groups.front()) assigned[activity] = 1;
  std::vector<std::size_t> stack;
  for (std::size_t seed = 0; seed < n; ++seed) {
    if (assigned[seed]) continue;
    groups.emplace_back();
    const auto group = groups.size() - 1;
    stack.assign(1, seed);
    assigned[seed] = 1;
    while (!stack.empty()) {
      const auto current = stack.back();
      stack.pop_back();
      groups[group].push_back(current);
      for (std::size_t next = 0; next < n; ++next) {
        if (!assigned[next] &&
            (dfg.follows[current][next] || dfg.follows[next][current])) {
          assigned[next] = 1;
          stack.push_back(next);
        }
      }
    }
  }

  const auto group_containing = [&](std::size_t activity) {
    for (std::size_t group = 0; group < groups.size(); ++group)
      if (std::find(groups[group].begin(), groups[group].end(), activity) !=
          groups[group].end())
        return group;
    return groups.size();
  };
  const auto merge_into_body = [&](std::size_t group) {
    if (group == 0 || group >= groups.size()) return;
    groups.front().insert(groups.front().end(), groups[group].begin(),
                          groups[group].end());
    groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(group));
  };

  // A start-only activity and every activity it directly enables belong to the
  // do part. The symmetric rule applies to an end-only activity and its direct
  // predecessors. These are deliberately staged before completeness checks.
  for (std::size_t start = 0; start < n; ++start) {
    if (!dfg.is_start[start] || dfg.is_end[start]) continue;
    for (std::size_t target = 0; target < n; ++target) {
      if (!dfg.follows[start][target]) continue;
      const auto left = group_containing(start);
      const auto right = group_containing(target);
      if (left != right) {
        // The body is always group zero; merging the other group into it keeps
        // the boundary component stable even after earlier merges.
        merge_into_body(std::max(left, right));
      }
    }
  }
  for (std::size_t end = 0; end < n; ++end) {
    if (!dfg.is_end[end] || dfg.is_start[end]) continue;
    for (std::size_t source = 0; source < n; ++source) {
      if (!dfg.follows[source][end]) continue;
      const auto left = group_containing(source);
      const auto right = group_containing(end);
      if (left != right) merge_into_body(std::max(left, right));
    }
  }

  // A redo component that reaches one start/end must reach all activities in
  // that boundary set. A failure folds the entire component into the body.
  // Notice that edges from redo to an interior body activity are allowed; only
  // the boundary completeness conditions constrain the loop cut.
  for (std::size_t group = 1; group < groups.size();) {
    bool merge = false;
    for (const auto activity : groups[group]) {
      for (std::size_t start = 0; start < n; ++start) {
        if (!dfg.is_start[start] || !dfg.follows[activity][start]) continue;
        for (std::size_t other = 0; other < n; ++other) {
          if (dfg.is_start[other] && !dfg.follows[activity][other]) {
            merge = true;
            break;
          }
        }
        if (merge) break;
      }
      if (merge) break;
    }
    if (merge) {
      merge_into_body(group);
      continue;
    }
    ++group;
  }
  for (std::size_t group = 1; group < groups.size();) {
    bool merge = false;
    for (const auto activity : groups[group]) {
      for (std::size_t end = 0; end < n; ++end) {
        if (!dfg.is_end[end] || !dfg.follows[end][activity]) continue;
        for (std::size_t other = 0; other < n; ++other) {
          if (dfg.is_end[other] && !dfg.follows[other][activity]) {
            merge = true;
            break;
          }
        }
        if (merge) break;
      }
      if (merge) break;
    }
    if (merge) {
      merge_into_body(group);
      continue;
    }
    ++group;
  }

  groups.erase(std::remove_if(groups.begin(), groups.end(),
                              [](const auto& group) { return group.empty(); }),
               groups.end());
  if (groups.size() < 2) return std::nullopt;
  std::vector<std::vector<std::size_t>> result;
  result.reserve(2);
  result.push_back(std::move(groups.front()));
  result.emplace_back();
  for (std::size_t group = 1; group < groups.size(); ++group)
    result.back().insert(result.back().end(), groups[group].begin(),
                         groups[group].end());
  if (result.back().empty()) return std::nullopt;
  return to_global(dfg, result);
}

// --- Log splitting ---------------------------------------------------------

std::string trace_key(const std::vector<ActId>& activities) {
  std::string key;
  key.reserve(activities.size() * sizeof(ActId));
  for (const ActId act : activities) {
    key.append(reinterpret_cast<const char*>(&act), sizeof(ActId));
  }
  return key;
}

// Merges identical variants so recursion stays proportional to distinct
// behaviour rather than the number of cases routed into a branch.
Sublog compress(Sublog log) {
  std::unordered_map<std::string, std::size_t> index;
  Sublog out;
  out.reserve(log.size());
  for (auto& wt : log) {
    auto [it, inserted] = index.emplace(trace_key(wt.activities), out.size());
    if (inserted) {
      out.push_back(std::move(wt));
    } else {
      out[it->second].count += wt.count;
    }
  }
  return out;
}

using GroupOf = std::unordered_map<ActId, std::size_t>;

GroupOf index_groups(const std::vector<std::vector<ActId>>& groups) {
  GroupOf group_of;
  for (std::size_t i = 0; i < groups.size(); ++i) {
    for (const ActId act : groups[i]) {
      group_of.emplace(act, i);
    }
  }
  return group_of;
}

// Exclusive: a trace is routed to the group holding most of its events, and
// events outside that group are dropped as noise. On an unfiltered cut the
// groups are true components, so every trace lands whole in its own group;
// on a noise-filtered cut a trace may span groups, and projecting it onto the
// winning group is what keeps each sublog's alphabet strictly smaller than
// its parent's -- without it the recursion need not terminate.
std::vector<Sublog> split_exclusive(const Sublog& log,
                                    std::size_t groups,
                                    const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  std::vector<std::uint64_t> hits(groups);
  for (const auto& wt : log) {
    if (wt.activities.empty()) {
      out[0].push_back(wt);
      continue;
    }
    hits.assign(groups, 0);
    for (const ActId act : wt.activities) {
      ++hits[group_of.at(act)];
    }
    const std::size_t target = static_cast<std::size_t>(
        std::max_element(hits.begin(), hits.end()) - hits.begin());
    std::vector<ActId> kept;
    kept.reserve(wt.activities.size());
    for (const ActId act : wt.activities) {
      if (group_of.at(act) == target) {
        kept.push_back(act);
      }
    }
    out[target].push_back(WeightedTrace{std::move(kept), wt.count});
  }
  for (auto& sublog : out) {
    sublog = compress(std::move(sublog));
  }
  return out;
}

// Sequence and parallel: project each trace onto every group, preserving order.
std::vector<Sublog> split_projection(const Sublog& log,
                                     std::size_t groups,
                                     const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  for (const auto& wt : log) {
    std::vector<std::vector<ActId>> parts(groups);
    for (const ActId act : wt.activities) {
      parts[group_of.at(act)].push_back(act);
    }
    for (std::size_t i = 0; i < groups; ++i) {
      out[i].push_back(WeightedTrace{std::move(parts[i]), wt.count});
    }
  }
  for (auto& sublog : out) {
    sublog = compress(std::move(sublog));
  }
  return out;
}

// Loop: split each trace into maximal runs of one group. Body runs (group 0)
// become separate body executions; redo runs feed their redo child.
std::vector<Sublog> split_loop(const Sublog& log,
                               std::size_t groups,
                               const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  for (const auto& wt : log) {
    if (wt.activities.empty()) {
      out[0].push_back(WeightedTrace{{}, wt.count});
      continue;
    }
    std::size_t current = group_of.at(wt.activities.front());
    std::vector<ActId> run;
    for (const ActId act : wt.activities) {
      const std::size_t group = group_of.at(act);
      if (group != current) {
        out[current].push_back(WeightedTrace{std::move(run), wt.count});
        run.clear();
        current = group;
      }
      run.push_back(act);
    }
    out[current].push_back(WeightedTrace{std::move(run), wt.count});
  }
  for (auto& sublog : out) {
    sublog = compress(std::move(sublog));
  }
  return out;
}

// --- Tree assembly ---------------------------------------------------------

// Flattens nested associative operators and drops redundant taus so the tree
// reads cleanly (e.g. X(t, X(a, b)) becomes X(t, a, b)).
ProcessTreeNode make_node(ProcessTreeOperator op,
                          std::vector<ProcessTreeNode> children) {
  const bool associative = op == ProcessTreeOperator::kSequence ||
                           op == ProcessTreeOperator::kExclusive ||
                           op == ProcessTreeOperator::kParallel;
  if (associative) {
    std::vector<ProcessTreeNode> flat;
    bool kept_tau = false;
    for (auto& child : children) {
      if (child.op == op) {
        for (auto& grandchild : child.children) {
          flat.push_back(std::move(grandchild));
        }
      } else {
        flat.push_back(std::move(child));
      }
    }
    if (op == ProcessTreeOperator::kExclusive) {
      std::vector<ProcessTreeNode> deduped;
      for (auto& child : flat) {
        if (child.op == ProcessTreeOperator::kTau) {
          if (kept_tau) {
            continue;
          }
          kept_tau = true;
        }
        deduped.push_back(std::move(child));
      }
      flat = std::move(deduped);
    }
    children = std::move(flat);
  }

  if (children.size() == 1) {
    return std::move(children.front());
  }

  ProcessTreeNode node;
  node.op = op;
  node.children = std::move(children);
  return node;
}

ProcessTreeNode mine(const Sublog& log, double f,
                     const InductiveMinerOptions& options);

ProcessTreeNode build_from_cut(ProcessTreeOperator op,
                               const std::vector<std::vector<ActId>>& groups,
                               const Sublog& log,
                               double f,
                               const InductiveMinerOptions& options) {
  const GroupOf group_of = index_groups(groups);
  std::vector<Sublog> sublogs;
  switch (op) {
    case ProcessTreeOperator::kExclusive:
      sublogs = split_exclusive(log, groups.size(), group_of);
      break;
    case ProcessTreeOperator::kSequence:
    case ProcessTreeOperator::kParallel:
      sublogs = split_projection(log, groups.size(), group_of);
      break;
    case ProcessTreeOperator::kLoop:
      sublogs = split_loop(log, groups.size(), group_of);
      break;
    default:
      break;
  }

  std::vector<ProcessTreeNode> children;
  children.reserve(sublogs.size());
  for (const auto& sublog : sublogs) {
    children.push_back(mine(sublog, f, options));
  }
  return make_node(op, std::move(children));
}

// Flower model: a loop with a silent body and every activity as a redo path.
// The most permissive fall-through, used when no cut applies.
ProcessTreeNode flower(const std::vector<ActId>& activities) {
  std::vector<ProcessTreeNode> children;
  children.reserve(activities.size() + 1);
  children.push_back(ProcessTreeNode::tau());
  for (const ActId act : activities) {
    children.push_back(ProcessTreeNode::leaf(act));
  }
  ProcessTreeNode node;
  node.op = ProcessTreeOperator::kLoop;
  node.children = std::move(children);
  return node;
}

// Standard Inductive Miner recursion: empty traces are separated first, then
// base cases and the four cut detectors run on the non-empty sublog, followed
// by the remaining fall-throughs. Keeping the empty-trace decision outside the
// cut search preserves the optional skip as one explicit branch.
ProcessTreeNode mine(const Sublog& log, double f,
                     const InductiveMinerOptions& options) {
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;
  for (const auto& wt : log) {
    (wt.activities.empty() ? empty_weight : nonempty_weight) += wt.count;
  }
  const bool has_empty = empty_weight > 0;

  // Base case: an empty log (or only empty traces) is a silent step.
  if (nonempty_weight == 0) {
    return ProcessTreeNode::tau();
  }

  if (has_empty) {
    const auto total_weight = empty_weight + nonempty_weight;
    Sublog nonempty;
    nonempty.reserve(log.size());
    for (const auto& wt : log) {
      if (!wt.activities.empty()) nonempty.push_back(wt);
    }
    // Empty traces are retained when their share is strictly larger than the
    // configured noise threshold. The comparison uses all traces, not only
    // the non-empty denominator; at threshold zero every observed empty trace
    // is therefore part of the discovered language.
    if (static_cast<double>(empty_weight) >
        f * static_cast<double>(total_weight)) {
      return make_node(ProcessTreeOperator::kExclusive,
                       {ProcessTreeNode::tau(), mine(nonempty, f, options)});
    }
    return mine(nonempty, f, options);
  }

  LocalDfg dfg = build_local_dfg(log);

  // Base case: a single activity with no empty traces. It is a bare leaf when
  // it always occurs once; a self-loop when it genuinely repeats. Under the
  // infrequent variant, repetitions rarer than the threshold are treated as
  // noise and still yield a leaf.
  if (dfg.size() == 1 && !has_empty) {
    const ActId act = dfg.to_global.front();
    std::uint64_t multi_weight = 0;
    for (const auto& wt : log) {
      if (wt.activities.size() > 1) {
        multi_weight += wt.count;
      }
    }
    if (static_cast<double>(multi_weight) <=
        f * static_cast<double>(nonempty_weight)) {
      return ProcessTreeNode::leaf(act);
    }
    ProcessTreeNode node;
    node.op = ProcessTreeOperator::kLoop;
    node.children = {ProcessTreeNode::leaf(act), ProcessTreeNode::tau()};
    return node;
  }

  const auto try_cuts = [&](const LocalDfg& d) -> std::optional<ProcessTreeNode> {
    if (auto cut = exclusive_cut(d)) {
      return build_from_cut(ProcessTreeOperator::kExclusive, *cut, log, f,
                            options);
    }
    const auto sequence = options.disable_strict_sequence_cut
                              ? sequence_cut(d)
                              : strict_sequence_cut(d);
    if (sequence) {
      return build_from_cut(ProcessTreeOperator::kSequence, *sequence, log, f,
                            options);
    }
    if (auto cut = parallel_cut(d)) {
      return build_from_cut(ProcessTreeOperator::kParallel, *cut, log, f,
                            options);
    }
    if (auto cut = loop_cut(d)) {
      return build_from_cut(ProcessTreeOperator::kLoop, *cut, log, f, options);
    }
    return std::nullopt;
  };

  if (auto node = try_cuts(dfg)) {
    return std::move(*node);
  }

  // IMf: filtering is a fall-through, not a preprocessing step. Cuts are
  // sought on the full relations first, and only a sublog that has no cut
  // gets its weak edges and rare starts dropped for a second attempt; observed
  // end activities remain available to endpoint-aware cuts.
  // Filtering up front can erase structure the unfiltered relations exhibit,
  // collapsing a merely noisy log to a flower.
  if (f > 0.0) {
    apply_noise_filter(dfg, f);
    if (auto node = try_cuts(dfg)) {
      return std::move(*node);
    }
  }

  // The native fall-through set already consists of the empty-trace handling
  // above and this flower fallback, both of which remain valid when callers
  // request the reduced fall-through policy.
  return flower(dfg.to_global);
}

// --- IMd: recursion over the directly-follows graph -------------------------
//
// The log is read exactly once (build_local_dfg on the whole log); afterwards
// each cut projects the *graph* onto its groups instead of splitting traces,
// so recursion cost depends on the number of activities, not events.

// Projects the parent graph onto one group of a cut. Internal edges keep their
// counts and the group inherits its members' start/end counts. With
// `crossing_edges` set (sequence, parallel, and loop cuts), an edge entering
// the group from outside makes its target a start of the sub-graph and an edge
// leaving makes its source an end — the sub-behaviour begins and ends at the
// crossing points. An exclusive cut has no meaningful crossing edges (its
// groups are components; anything across is filtered noise), so they are
// dropped there.
LocalDfg project_dfg(const LocalDfg& parent,
                     const std::vector<ActId>& group,
                     bool crossing_edges) {
  LocalDfg sub;
  for (const ActId act : group) {
    sub.to_local.emplace(act, sub.to_global.size());
    sub.to_global.push_back(act);
  }
  const std::size_t m = sub.size();
  sub.edge_count.assign(m, std::vector<std::uint64_t>(m, 0));
  sub.start_count.assign(m, 0);
  sub.end_count.assign(m, 0);

  constexpr std::size_t kOutside = static_cast<std::size_t>(-1);
  std::vector<std::size_t> to_sub(parent.size(), kOutside);
  for (std::size_t i = 0; i < m; ++i) {
    to_sub[parent.to_local.at(sub.to_global[i])] = i;
  }

  for (std::size_t a = 0; a < parent.size(); ++a) {
    if (to_sub[a] != kOutside) {
      sub.start_count[to_sub[a]] += parent.start_count[a];
      sub.end_count[to_sub[a]] += parent.end_count[a];
    }
    for (std::size_t b = 0; b < parent.size(); ++b) {
      const std::uint64_t weight = parent.edge_count[a][b];
      if (weight == 0) {
        continue;
      }
      const bool a_in = to_sub[a] != kOutside;
      const bool b_in = to_sub[b] != kOutside;
      if (a_in && b_in) {
        sub.edge_count[to_sub[a]][to_sub[b]] += weight;
      } else if (crossing_edges && !a_in && b_in) {
        sub.start_count[to_sub[b]] += weight;
      } else if (crossing_edges && a_in && !b_in) {
        sub.end_count[to_sub[a]] += weight;
      }
    }
  }

  apply_noise_filter(sub, 0.0);
  return sub;
}

ProcessTreeNode mine_dfg(LocalDfg dfg, double f,
                         const InductiveMinerOptions& options) {
  if (dfg.size() == 0) {
    return ProcessTreeNode::tau();
  }

  // Base case: a single activity is a bare leaf, or a self-loop when its
  // self-edge survives the noise filter.
  if (dfg.size() == 1) {
    if (f > 0.0) {
      apply_noise_filter(dfg, f);
    }
    if (!dfg.follows[0][0]) {
      return ProcessTreeNode::leaf(dfg.to_global.front());
    }
    ProcessTreeNode node;
    node.op = ProcessTreeOperator::kLoop;
    node.children = {ProcessTreeNode::leaf(dfg.to_global.front()),
                     ProcessTreeNode::tau()};
    return node;
  }

  const auto build = [&](ProcessTreeOperator op,
                         const std::vector<std::vector<ActId>>& groups) {
    const bool crossing_edges = op != ProcessTreeOperator::kExclusive;

    // A sequence group is skippable when some trace evidently jumps over it:
    // an edge from an earlier group straight to a later one, a trace starting
    // in a later group, or one ending in an earlier group. The log recursion
    // sees such skips as empty projected traces; a graph projection never
    // receives that evidence, so it must be read off the parent graph here
    // and modelled as an explicit optional child.
    std::vector<char> skippable(groups.size(), 0);
    if (op == ProcessTreeOperator::kSequence) {
      std::vector<std::size_t> group_of(dfg.size(), 0);
      for (std::size_t i = 0; i < groups.size(); ++i) {
        for (const ActId act : groups[i]) {
          group_of[dfg.to_local.at(act)] = i;
        }
      }
      for (std::size_t a = 0; a < dfg.size(); ++a) {
        if (dfg.is_start[a]) {
          for (std::size_t i = 0; i < group_of[a]; ++i) {
            skippable[i] = 1;
          }
        }
        if (dfg.is_end[a]) {
          for (std::size_t i = group_of[a] + 1; i < groups.size(); ++i) {
            skippable[i] = 1;
          }
        }
        for (const std::size_t b : dfg.succ[a]) {
          for (std::size_t i = group_of[a] + 1; i < group_of[b]; ++i) {
            skippable[i] = 1;
          }
        }
      }
    }

    std::vector<ProcessTreeNode> children;
    children.reserve(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
      auto child = mine_dfg(project_dfg(dfg, groups[i], crossing_edges), f,
                            options);
      if (skippable[i]) {
        child = make_node(ProcessTreeOperator::kExclusive,
                          {ProcessTreeNode::tau(), std::move(child)});
      }
      children.push_back(std::move(child));
    }
    return make_node(op, std::move(children));
  };

  const auto try_cuts = [&](const LocalDfg& d) -> std::optional<ProcessTreeNode> {
    if (auto cut = exclusive_cut(d)) {
      return build(ProcessTreeOperator::kExclusive, *cut);
    }
    const auto sequence = options.disable_strict_sequence_cut
                              ? sequence_cut(d)
                              : strict_sequence_cut(d);
    if (sequence) {
      return build(ProcessTreeOperator::kSequence, *sequence);
    }
    if (auto cut = parallel_cut(d)) {
      return build(ProcessTreeOperator::kParallel, *cut);
    }
    if (auto cut = loop_cut(d)) {
      return build(ProcessTreeOperator::kLoop, *cut);
    }
    return std::nullopt;
  };

  if (auto node = try_cuts(dfg)) {
    return std::move(*node);
  }

  // Same fall-through order as the log recursion: filtering only when the
  // unfiltered relations exhibit no cut.
  if (f > 0.0) {
    apply_noise_filter(dfg, f);
    if (auto node = try_cuts(dfg)) {
      return std::move(*node);
    }
  }

  return flower(dfg.to_global);
}

// IMd entry: one pass over the (variant-compressed) log builds the root graph,
// then the recursion never touches traces again. Empty traces are invisible to
// a directly-follows graph, so their explicit skip branch is decided here —
// the one place the log itself is still in hand.
ProcessTreeNode mine_dfg_root(const Sublog& log, double f,
                              const InductiveMinerOptions& options) {
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;
  for (const auto& wt : log) {
    (wt.activities.empty() ? empty_weight : nonempty_weight) += wt.count;
  }
  if (nonempty_weight == 0) {
    return ProcessTreeNode::tau();
  }

  ProcessTreeNode root = mine_dfg(build_local_dfg(log), f, options);
  // The graph-backed variant carries the UVCL skip flag independently of the
  // infrequent threshold: if an empty trace was observed, retain it as the
  // optional tau branch.
  if (empty_weight > 0) {
    return make_node(ProcessTreeOperator::kExclusive,
                     {ProcessTreeNode::tau(), std::move(root)});
  }
  return root;
}

}  // namespace

ProcessTree discover_process_tree(const EventLog& event_log,
                                  const InductiveMinerOptions& options) {
  std::vector<std::string> names;
  std::unordered_map<std::string, ActId> ids;

  // Compress cases into variants up front, interning activity names in first-
  // seen order to match the directly-follows graph's activity ids.
  Sublog sublog;
  std::unordered_map<std::string, std::size_t> variant_index;
  for (const auto& trace : event_log.traces) {
    std::vector<ActId> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      auto [it, inserted] = ids.emplace(event.activity, names.size());
      if (inserted) {
        names.push_back(event.activity);
      }
      activities.push_back(it->second);
    }

    auto [vit, vinserted] =
        variant_index.emplace(trace_key(activities), sublog.size());
    if (vinserted) {
      sublog.push_back(WeightedTrace{std::move(activities), 1});
    } else {
      ++sublog[vit->second].count;
    }
  }

  const double f = std::clamp(options.noise_threshold, 0.0, 1.0);
  ProcessTreeNode root = options.variant == InductiveMinerVariant::kDfg
                             ? mine_dfg_root(sublog, f, options)
                             : mine(sublog, f, options);
  return ProcessTree(std::move(names), std::move(root));
}

ProcessTree discover_process_tree(const DirectlyFollowsGraph& graph,
                                  const InductiveMinerOptions& options) {
  // A graph input has already discarded trace bodies, so all selectors use the
  // graph-recursive IMd semantics just as the reference DFG entry point does.
  // The noise threshold still applies to raw edge and endpoint counts during
  // graph fall-throughs.
  const double f = std::clamp(options.noise_threshold, 0.0, 1.0);
  ProcessTreeNode root = mine_dfg(build_local_dfg(graph), f, options);
  return ProcessTree(graph.activities(), std::move(root));
}

}  // namespace minera
