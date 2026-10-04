#include "minera/inductive_miner.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "inductive/semantics.hpp"

namespace minera {
namespace inductive_detail {

using ActId = ActivityId;
using Sublog = WeightedSublog;
using LocalDfg = InductiveDfg;

bool WeightedTrace::empty() const noexcept { return activities.empty(); }

bool InductiveDataStructure::empty() const noexcept {
  return variants.empty();
}

std::size_t InductiveDfg::size() const noexcept { return to_global.size(); }

bool InductiveDfg::has_edge(ActivityId source, ActivityId target) const {
  const auto source_local = to_local.find(source);
  const auto target_local = to_local.find(target);
  return source_local != to_local.end() && target_local != to_local.end() &&
         source_local->second < follows.size() &&
         target_local->second < follows[source_local->second].size() &&
         follows[source_local->second][target_local->second] != 0;
}

bool BaseCaseResult::has_empty() const noexcept { return empty_weight > 0; }

bool BaseCaseResult::all_empty() const noexcept {
  return nonempty_weight == 0;
}

bool EmptyTraceSplitResult::has_empty() const noexcept {
  return empty_weight > 0;
}

bool FallThrough::handled() const noexcept { return node.has_value(); }

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

  if (f <= 0.0) return;

  // Filtering also changes the graph alphabet. An activity whose incident
  // edges and boundary memberships were all removed must not participate in
  // a cut merely because it occurred in the unfiltered trace. The eventual
  // cut projection drops those invisible events from the original log.
  std::vector<char> visible(n, 0);
  for (std::size_t a = 0; a < n; ++a) {
    visible[a] = dfg.is_start[a] || dfg.is_end[a];
    for (std::size_t b = 0; b < n; ++b) {
      if (dfg.follows[a][b] || dfg.follows[b][a]) visible[a] = 1;
    }
  }
  if (std::all_of(visible.begin(), visible.end(),
                  [](char value) { return value != 0; })) {
    return;
  }

  constexpr std::size_t kMissing = static_cast<std::size_t>(-1);
  std::vector<std::size_t> remap(n, kMissing);
  LocalDfg compact;
  compact.skip = dfg.skip;
  for (std::size_t a = 0; a < n; ++a) {
    if (!visible[a]) continue;
    remap[a] = compact.to_global.size();
    compact.to_local.emplace(dfg.to_global[a], compact.to_global.size());
    compact.to_global.push_back(dfg.to_global[a]);
  }
  compact.edge_count.assign(
      compact.size(), std::vector<std::uint64_t>(compact.size(), 0));
  compact.start_count.assign(compact.size(), 0);
  compact.end_count.assign(compact.size(), 0);
  for (std::size_t a = 0; a < n; ++a) {
    if (remap[a] == kMissing) continue;
    const std::size_t mapped_a = remap[a];
    if (dfg.is_start[a]) compact.start_count[mapped_a] = dfg.start_count[a];
    if (dfg.is_end[a]) compact.end_count[mapped_a] = dfg.end_count[a];
    for (std::size_t b = 0; b < n; ++b) {
      if (remap[b] != kMissing && dfg.follows[a][b]) {
        compact.edge_count[mapped_a][remap[b]] = dfg.edge_count[a][b];
      }
    }
  }
  dfg = std::move(compact);
  apply_noise_filter(dfg, 0.0);
}

LocalDfg build_local_dfg(const Sublog& log) {
  LocalDfg dfg;
  std::set<ActId> activities;
  for (const auto& wt : log) {
    for (const ActId act : wt.activities) {
      activities.insert(act);
    }
  }
  for (const ActId act : activities) {
    dfg.to_local.emplace(act, dfg.to_global.size());
    dfg.to_global.push_back(act);
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
  const std::vector<std::vector<char>> reachable =
      CutUtils::reachability(dfg);

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
// directions) must share a branch. Candidate groups without both a trace start
// and a trace end are folded into a neighbouring group, smallest groups first;
// this preserves a valid coarser cut instead of rejecting it outright.
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
  std::stable_sort(local_groups.begin(), local_groups.end(),
                   [](const auto& left, const auto& right) {
                     return left.size() < right.size();
                   });
  std::size_t group = 0;
  while (group < local_groups.size() && local_groups.size() > 1) {
    bool has_start = false;
    bool has_end = false;
    for (const std::size_t loc : local_groups[group]) {
      has_start = has_start || dfg.is_start[loc];
      has_end = has_end || dfg.is_end[loc];
    }
    if (has_start && has_end) {
      ++group;
      continue;
    }

    auto rejected = std::move(local_groups[group]);
    local_groups.erase(local_groups.begin() +
                       static_cast<std::ptrdiff_t>(group));
    const std::size_t target = group == 0 ? 0 : group - 1;
    local_groups[target].insert(local_groups[target].end(), rejected.begin(),
                                rejected.end());
  }
  if (local_groups.size() < 2) return std::nullopt;
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

bool Cut::valid() const noexcept {
  return groups.size() >= 2 &&
         std::all_of(groups.begin(), groups.end(),
                     [](const auto& group) { return !group.empty(); });
}

ProcessTreeOperator Cut::operation() const noexcept {
  switch (kind) {
    case CutKind::kExclusive:
      return ProcessTreeOperator::kExclusive;
    case CutKind::kSequence:
      return ProcessTreeOperator::kSequence;
    case CutKind::kParallel:
      return ProcessTreeOperator::kParallel;
    case CutKind::kLoop:
      return ProcessTreeOperator::kLoop;
  }
  return ProcessTreeOperator::kExclusive;
}

std::vector<std::vector<char>> CutUtils::reachability(const LocalDfg& dfg) {
  return ::minera::inductive_detail::reachability(dfg);
}

namespace {

template <typename Groups>
std::optional<Cut> make_cut(CutKind kind, Groups groups) {
  Cut cut;
  cut.kind = kind;
  cut.groups = std::move(groups);
  if (!cut.valid()) return std::nullopt;
  return cut;
}

}  // namespace

std::optional<Cut> ExclusiveChoiceCut::detect(const LocalDfg& dfg) {
  auto groups = exclusive_cut(dfg);
  if (!groups) return std::nullopt;
  return make_cut(CutKind::kExclusive, std::move(*groups));
}

std::optional<Cut> SequenceCut::detect(const LocalDfg& dfg) {
  auto groups = sequence_cut(dfg);
  if (!groups) return std::nullopt;
  return make_cut(CutKind::kSequence, std::move(*groups));
}

std::optional<Cut> SequenceCut::detect_strict(const LocalDfg& dfg) {
  auto groups = strict_sequence_cut(dfg);
  if (!groups) return std::nullopt;
  return make_cut(CutKind::kSequence, std::move(*groups));
}

std::optional<Cut> ConcurrencyCut::detect(const LocalDfg& dfg) {
  auto groups = parallel_cut(dfg);
  if (!groups) return std::nullopt;
  return make_cut(CutKind::kParallel, std::move(*groups));
}

std::optional<Cut> LoopCut::detect(const LocalDfg& dfg) {
  auto groups = loop_cut(dfg);
  if (!groups) return std::nullopt;
  return make_cut(CutKind::kLoop, std::move(*groups));
}

std::optional<Cut> CutFactory::find(const LocalDfg& dfg,
                                    const InductiveMinerOptions& options) {
  if (auto cut = ExclusiveChoiceCut::detect(dfg)) return cut;
  if (options.disable_strict_sequence_cut) {
    if (auto cut = SequenceCut::detect(dfg)) return cut;
  } else if (auto cut = SequenceCut::detect_strict(dfg)) {
    return cut;
  }
  if (auto cut = ConcurrencyCut::detect(dfg)) return cut;
  return LoopCut::detect(dfg);
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
    hits.assign(groups, 0);
    for (const ActId act : wt.activities) {
      const auto group = group_of.find(act);
      if (group != group_of.end()) ++hits[group->second];
    }
    // Sorting the reference candidates by (hit count, group index) descending
    // means ties are deliberately routed to the highest-index group.  Keeping
    // that rule for empty traces as well preserves the same deterministic
    // projection when every group's hit count is zero.
    std::size_t target = 0;
    for (std::size_t group = 1; group < groups; ++group) {
      if (hits[group] >= hits[target]) target = group;
    }
    std::vector<ActId> kept;
    kept.reserve(wt.activities.size());
    for (const ActId act : wt.activities) {
      const auto group = group_of.find(act);
      if (group != group_of.end() && group->second == target) {
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

// Parallel projection keeps every event in its corresponding branch.  Unlike
// a sequence cut, concurrent branches are allowed to interleave, so there is
// no split window to infer: each child receives the full subsequence for its
// group.
std::vector<Sublog> split_parallel_projection(const Sublog& log,
                                              std::size_t groups,
                                              const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  for (const auto& wt : log) {
    std::vector<std::vector<ActId>> parts(groups);
    for (const ActId act : wt.activities) {
      const auto group = group_of.find(act);
      if (group != group_of.end()) parts[group->second].push_back(act);
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

// Sequence projection assigns each trace to ordered windows.  A noisy trace
// may contain an event from a later group before returning to the current
// group; projecting the whole subsequence would duplicate that detour in a
// child and can manufacture a self-loop.  For each group, choose the earliest
// minimum-cost split point after the previous window.  Events from the current
// group lower the cost (they belong in this window), events from groups not yet
// visited raise it, and events from already projected groups are neutral.  The
// selected window contributes only events belonging to the current group;
// empty windows are retained with their original weight.
std::vector<Sublog> split_sequence_projection(
    const Sublog& log, std::size_t groups, const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  std::vector<char> ignored(groups, 0);
  for (const auto& wt : log) {
    std::size_t split_point = 0;
    std::fill(ignored.begin(), ignored.end(), static_cast<char>(0));
    for (std::size_t group = 0; group < groups; ++group) {
      int least_cost = 0;
      std::size_t new_split_point = split_point;
      int cost = 0;
      for (std::size_t position = split_point; position < wt.activities.size();
           ++position) {
        const auto mapped = group_of.find(wt.activities[position]);
        if (mapped == group_of.end()) continue;
        const std::size_t activity_group = mapped->second;
        if (activity_group == group) {
          --cost;
        } else if (!ignored[activity_group]) {
          ++cost;
        }
        if (cost < least_cost) {
          least_cost = cost;
          new_split_point = position + 1;
        }
      }

      std::vector<ActId> projected;
      projected.reserve(new_split_point - split_point);
      for (std::size_t position = split_point; position < new_split_point;
           ++position) {
        const ActId activity = wt.activities[position];
        const auto mapped = group_of.find(activity);
        if (mapped != group_of.end() && mapped->second == group) {
          projected.push_back(activity);
        }
      }
      out[group].push_back(WeightedTrace{std::move(projected), wt.count});
      split_point = new_split_point;
      ignored[group] = 1;
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
    std::vector<ActId> visible;
    visible.reserve(wt.activities.size());
    for (const ActId act : wt.activities) {
      if (group_of.find(act) != group_of.end()) visible.push_back(act);
    }
    if (visible.empty()) {
      out[0].push_back(WeightedTrace{{}, wt.count});
      continue;
    }
    std::size_t current = group_of.at(visible.front());
    std::vector<ActId> run;
    for (const ActId act : visible) {
      const std::size_t group = group_of.at(act);
      if (group != current) {
        out[current].push_back(WeightedTrace{std::move(run), wt.count});
        run.clear();
        current = group;
      }
      run.push_back(act);
    }
    out[current].push_back(WeightedTrace{std::move(run), wt.count});
    if (current != 0) {
      out[0].push_back(WeightedTrace{{}, wt.count});
    }
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
      if ((op == ProcessTreeOperator::kSequence ||
           op == ProcessTreeOperator::kParallel) &&
          child.op == ProcessTreeOperator::kTau) {
        continue;
      }
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

  if (children.empty()) return ProcessTreeNode::tau();

  if (children.size() == 1) {
    return std::move(children.front());
  }

  ProcessTreeNode node;
  node.op = op;
  node.children = std::move(children);
  return node;
}

bool EmptyLogBaseCase::detects(const Sublog& log) {
  return std::all_of(log.begin(), log.end(),
                     [](const WeightedTrace& trace) { return trace.empty(); });
}

BaseCaseResult BaseCase::inspect(const Sublog& log) {
  BaseCaseResult result;
  for (const auto& trace : log) {
    (trace.empty() ? result.empty_weight : result.nonempty_weight) +=
        trace.count;
  }
  return result;
}

ProcessTreeNode BaseCaseFactory::empty() { return ProcessTreeNode::tau(); }

ProcessTreeNode BaseCaseFactory::single(ActivityId activity, bool repeated) {
  if (!repeated) return ProcessTreeNode::leaf(activity);
  ProcessTreeNode node;
  node.op = ProcessTreeOperator::kLoop;
  node.children = {ProcessTreeNode::leaf(activity), ProcessTreeNode::tau()};
  return node;
}

EmptyTraceSplitResult EmptyTraceFallThrough::split(const Sublog& log) {
  const BaseCaseResult counts = BaseCase::inspect(log);
  EmptyTraceSplitResult result;
  result.empty_weight = counts.empty_weight;
  result.nonempty_weight = counts.nonempty_weight;
  if (counts.all_empty()) return result;
  result.nonempty.reserve(log.size());
  for (const auto& trace : log) {
    if (!trace.empty()) result.nonempty.push_back(trace);
  }
  return result;
}

std::optional<ProcessTreeNode> SingleActivityBaseCase::resolve(
    const Sublog& log, const LocalDfg& dfg) {
  if (dfg.size() != 1) return std::nullopt;
  const ActivityId activity = dfg.to_global.front();
  const bool repeated = std::any_of(
      log.begin(), log.end(), [](const WeightedTrace& trace) {
        return trace.activities.size() > 1;
      });
  return BaseCaseFactory::single(activity, repeated);
}

ProcessTreeNode mine(const Sublog& log, double f,
                     const InductiveMinerOptions& options,
                     const ActivityLabels& labels);

ProcessTreeNode build_from_cut(ProcessTreeOperator op,
                               const std::vector<std::vector<ActId>>& groups,
                               const Sublog& log,
                               double f,
                               const InductiveMinerOptions& options,
                               const ActivityLabels& labels) {
  const GroupOf group_of = index_groups(groups);
  std::vector<Sublog> sublogs;
  switch (op) {
    case ProcessTreeOperator::kExclusive:
      sublogs = split_exclusive(log, groups.size(), group_of);
      break;
    case ProcessTreeOperator::kSequence:
      sublogs = split_sequence_projection(log, groups.size(), group_of);
      break;
    case ProcessTreeOperator::kParallel:
      sublogs = split_parallel_projection(log, groups.size(), group_of);
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
    children.push_back(mine(sublog, f, options, labels));
  }
  return make_node(op, std::move(children));
}

ProcessTreeNode build_from_cut(const Cut& cut, const Sublog& log, double f,
                               const InductiveMinerOptions& options,
                               const ActivityLabels& labels) {
  return build_from_cut(cut.operation(), cut.groups, log, f, options, labels);
}

// Flower model: a loop with a silent body and every activity as a redo path.
// The most permissive fall-through, used when no cut applies.
ProcessTreeNode FlowerFallThrough::build(
    const std::vector<ActId>& activities, const ActivityLabels& labels) {
  std::vector<ActId> ordered = activities;
  std::sort(ordered.begin(), ordered.end(), [&](ActId left, ActId right) {
    if (left < labels.size() && right < labels.size() &&
        labels[left] != labels[right]) {
      return labels[left] < labels[right];
    }
    return left < right;
  });
  if (ordered.empty()) return ProcessTreeNode::tau();

  std::vector<ProcessTreeNode> choices;
  choices.reserve(ordered.size());
  for (const ActId act : ordered) {
    choices.push_back(ProcessTreeNode::leaf(act));
  }
  ProcessTreeNode node;
  node.op = ProcessTreeOperator::kLoop;
  node.children = {
      ProcessTreeNode::tau(),
      make_node(ProcessTreeOperator::kExclusive, std::move(choices))};
  return node;
}

bool has_normal_cut(const LocalDfg& dfg,
                    const InductiveMinerOptions& options) {
  return CutFactory::find(dfg, options).has_value();
}

std::vector<ActId> ordered_activities(const std::vector<ActId>& activities,
                                      const ActivityLabels& labels) {
  std::vector<ActId> ordered = activities;
  std::sort(ordered.begin(), ordered.end(), [&](ActId left, ActId right) {
    if (left < labels.size() && right < labels.size() &&
        labels[left] != labels[right]) {
      return labels[left] < labels[right];
    }
    return left < right;
  });
  return ordered;
}

std::pair<Sublog, Sublog> split_activity(const Sublog& log, ActId candidate) {
  Sublog candidate_log;
  Sublog residual_log;
  candidate_log.reserve(log.size());
  residual_log.reserve(log.size());
  for (const auto& wt : log) {
    std::vector<ActId> selected;
    std::vector<ActId> residual;
    selected.reserve(wt.activities.size());
    residual.reserve(wt.activities.size());
    for (const ActId activity : wt.activities) {
      (activity == candidate ? selected : residual).push_back(activity);
    }
    candidate_log.push_back(
        WeightedTrace{std::move(selected), wt.count});
    residual_log.push_back(WeightedTrace{std::move(residual), wt.count});
  }
  return {compress(std::move(candidate_log)),
          compress(std::move(residual_log))};
}

std::optional<ProcessTreeNode> ActivityOncePerTraceFallThrough::apply(
    const Sublog& log, const LocalDfg& dfg, double f,
    const InductiveMinerOptions& options, const ActivityLabels& labels) {
  if (options.disable_fallthroughs) return std::nullopt;
  const auto candidates = ordered_activities(dfg.to_global, labels);
  for (const ActId candidate : candidates) {
    bool once_per_trace = true;
    for (const auto& wt : log) {
      if (std::count(wt.activities.begin(), wt.activities.end(), candidate) !=
          1) {
        once_per_trace = false;
        break;
      }
    }
    if (!once_per_trace) continue;
    auto [candidate_log, residual_log] = split_activity(log, candidate);
    return make_node(
        ProcessTreeOperator::kParallel,
        {mine(candidate_log, f, options, labels),
         mine(residual_log, f, options, labels)});
  }
  return std::nullopt;
}

std::optional<ProcessTreeNode> ActivityConcurrentFallThrough::apply(
    const Sublog& log, const LocalDfg& dfg, double f,
    const InductiveMinerOptions& options, const ActivityLabels& labels) {
  if (options.disable_fallthroughs) return std::nullopt;
  const auto candidates = ordered_activities(dfg.to_global, labels);

  // Viability checks are independent: projecting a candidate and looking for
  // an ordinary cut only reads the current recursion inputs.  Keep the
  // established serial path for small inputs and make the opt-in path bounded
  // to two workers.  Results are stored by candidate index, so selecting the
  // first viable entry remains deterministic regardless of completion order.
  if (options.multiprocessing && candidates.size() > 20) {
    std::vector<char> viable(candidates.size(), 0);
    std::atomic<std::size_t> next{0};
    const std::size_t worker_count =
        std::min<std::size_t>(2, candidates.size());
    std::vector<std::exception_ptr> errors(worker_count);
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
      workers.emplace_back([&, worker] {
        try {
          for (;;) {
            const std::size_t index =
                next.fetch_add(1, std::memory_order_relaxed);
            if (index >= candidates.size()) return;
            auto split = split_activity(log, candidates[index]);
            viable[index] =
                has_normal_cut(build_local_dfg(split.second), options);
          }
        } catch (...) {
          errors[worker] = std::current_exception();
        }
      });
    }
    for (auto& worker : workers) worker.join();
    for (const auto& error : errors) {
      if (error) std::rethrow_exception(error);
    }
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      if (!viable[index]) continue;
      auto split = split_activity(log, candidates[index]);
      return make_node(
          ProcessTreeOperator::kParallel,
          {mine(split.first, f, options, labels),
           mine(split.second, f, options, labels)});
    }
    return std::nullopt;
  }

  for (const ActId candidate : candidates) {
    auto [candidate_log, residual_log] = split_activity(log, candidate);
    if (!has_normal_cut(build_local_dfg(residual_log), options)) continue;
    return make_node(
        ProcessTreeOperator::kParallel,
        {mine(candidate_log, f, options, labels),
         mine(residual_log, f, options, labels)});
  }
  return std::nullopt;
}

Sublog project_silent_loop(const Sublog& log, const LocalDfg& dfg,
                           bool strict) {
  Sublog projected;
  projected.reserve(log.size());
  const auto is_start = [&](ActId activity) {
    const auto local = dfg.to_local.find(activity);
    return local != dfg.to_local.end() && dfg.is_start[local->second];
  };
  const auto is_end = [&](ActId activity) {
    const auto local = dfg.to_local.find(activity);
    return local != dfg.to_local.end() && dfg.is_end[local->second];
  };
  std::uint64_t projected_weight = 0;
  std::uint64_t original_weight = 0;
  for (const auto& wt : log) {
    original_weight += wt.count;
    std::size_t begin = 0;
    for (std::size_t i = 1; i < wt.activities.size(); ++i) {
      const bool split = is_start(wt.activities[i]) &&
                         (!strict || is_end(wt.activities[i - 1]));
      if (!split) continue;
      std::vector<ActId> fragment(
          wt.activities.begin() + static_cast<std::ptrdiff_t>(begin),
          wt.activities.begin() + static_cast<std::ptrdiff_t>(i));
      projected.push_back(WeightedTrace{std::move(fragment), wt.count});
      projected_weight += wt.count;
      begin = i;
    }
    std::vector<ActId> fragment(
        wt.activities.begin() + static_cast<std::ptrdiff_t>(begin),
        wt.activities.end());
    projected.push_back(WeightedTrace{std::move(fragment), wt.count});
    projected_weight += wt.count;
  }
  if (projected_weight <= original_weight) return Sublog{};
  return compress(std::move(projected));
}

Sublog StrictTauLoopFallThrough::project(const Sublog& log,
                                         const LocalDfg& dfg) {
  return project_silent_loop(log, dfg, true);
}

Sublog TauLoopFallThrough::project(const Sublog& log, const LocalDfg& dfg) {
  return project_silent_loop(log, dfg, false);
}

FallThrough FallThroughFactory::apply(
    const Sublog& log, const LocalDfg& dfg, double f,
    const InductiveMinerOptions& options, const ActivityLabels& labels) {
  if (options.disable_fallthroughs) return {};
  if (auto node = ActivityOncePerTraceFallThrough::apply(
          log, dfg, f, options, labels)) {
    return FallThrough{std::move(node)};
  }
  if (auto node = ActivityConcurrentFallThrough::apply(
          log, dfg, f, options, labels)) {
    return FallThrough{std::move(node)};
  }
  if (auto projected = StrictTauLoopFallThrough::project(log, dfg);
      !projected.empty()) {
    return FallThrough{make_node(
        ProcessTreeOperator::kLoop,
        {mine(std::move(projected), f, options, labels),
         mine(Sublog{}, f, options, labels)})};
  }
  if (auto projected = TauLoopFallThrough::project(log, dfg);
      !projected.empty()) {
    return FallThrough{make_node(
        ProcessTreeOperator::kLoop,
        {mine(std::move(projected), f, options, labels),
         mine(Sublog{}, f, options, labels)})};
  }
  return {};
}

// Standard Inductive Miner recursion: empty traces are separated first, then
// base cases and the four cut detectors run on the non-empty sublog, followed
// by the remaining fall-throughs. Keeping the empty-trace decision outside the
// cut search preserves the optional skip as one explicit branch.
ProcessTreeNode mine(const Sublog& log, double f,
                     const InductiveMinerOptions& options,
                     const ActivityLabels& labels) {
  const EmptyTraceSplitResult empty_split =
      EmptyTraceFallThrough::split(log);
  if (EmptyLogBaseCase::detects(log)) {
    return BaseCaseFactory::empty();
  }
  const bool has_empty = empty_split.has_empty();

  // Base case: an empty log (or only empty traces) is a silent step.
  if (empty_split.nonempty_weight == 0) {
    return BaseCaseFactory::empty();
  }

  if (has_empty) {
    const auto total_weight =
        empty_split.empty_weight + empty_split.nonempty_weight;
    // Empty traces are retained when their share is strictly larger than the
    // configured noise threshold. The comparison uses all traces, not only
    // the non-empty denominator; at threshold zero every observed empty trace
    // is therefore part of the discovered language.
    if (static_cast<double>(empty_split.empty_weight) >
        f * static_cast<double>(total_weight)) {
      return make_node(ProcessTreeOperator::kExclusive,
                       {ProcessTreeNode::tau(),
                        mine(empty_split.nonempty, f, options, labels)});
    }
    return mine(empty_split.nonempty, f, options, labels);
  }

  LocalDfg dfg = build_local_dfg(log);
  const LocalDfg original_dfg = dfg;

  // Base case: a single activity with no empty traces. It is a bare leaf when
  // it always occurs once and a self-loop when it genuinely repeats. Filtering
  // applies to graph relations, not to multiplicity inside a projected trace.
  if (dfg.size() == 1 && !has_empty) {
    if (auto result = SingleActivityBaseCase::resolve(log, dfg)) {
      if (options.disable_fallthroughs &&
          result->op == ProcessTreeOperator::kLoop) {
        return FlowerFallThrough::build(dfg.to_global, labels);
      }
      return std::move(*result);
    }
  }

  const auto try_cuts = [&](const LocalDfg& d) -> std::optional<ProcessTreeNode> {
    if (auto cut = CutFactory::find(d, options)) {
      return build_from_cut(*cut, log, f, options, labels);
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

  // Specialized fall-throughs inspect the original unfiltered log.  Filtering
  // is a graph-only retry in the infrequent variant; it must not change the
  // activity projections used by these deterministic repairs.
  if (auto result = FallThroughFactory::apply(log, original_dfg, f, options,
                                               labels);
      result.handled()) {
    return std::move(*result.node);
  }

  // The flower fallback is retained even when specialized fall-throughs are
  // disabled so every non-empty log still produces a process tree.
  return FlowerFallThrough::build(original_dfg.to_global, labels);
}

// --- IMd: recursion over the directly-follows graph -------------------------
//
// The log is read exactly once (build_local_dfg on the whole log); afterwards
// each cut projects the *graph* onto its groups instead of splitting traces,
// so recursion cost depends on the number of activities, not events.

LocalDfg induced_dfg(const LocalDfg& parent,
                     const std::vector<ActId>& group,
                     bool retain_boundaries) {
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
    if (retain_boundaries && to_sub[a] != kOutside) {
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
      }
    }
  }

  return sub;
}

void finalize_projected_dfg(LocalDfg& dfg) {
  std::vector<char> visible(dfg.size(), 0);
  for (std::size_t activity = 0; activity < dfg.size(); ++activity) {
    visible[activity] = dfg.start_count[activity] > 0 ||
                        dfg.end_count[activity] > 0;
    for (std::size_t other = 0; other < dfg.size(); ++other) {
      if (dfg.edge_count[activity][other] > 0 ||
          dfg.edge_count[other][activity] > 0) {
        visible[activity] = 1;
      }
    }
  }

  constexpr std::size_t kMissing = static_cast<std::size_t>(-1);
  std::vector<std::size_t> remap(dfg.size(), kMissing);
  LocalDfg compact;
  compact.skip = dfg.skip;
  for (std::size_t activity = 0; activity < dfg.size(); ++activity) {
    if (!visible[activity]) continue;
    remap[activity] = compact.to_global.size();
    compact.to_local.emplace(dfg.to_global[activity], compact.to_global.size());
    compact.to_global.push_back(dfg.to_global[activity]);
  }
  compact.edge_count.assign(
      compact.size(), std::vector<std::uint64_t>(compact.size(), 0));
  compact.start_count.assign(compact.size(), 0);
  compact.end_count.assign(compact.size(), 0);
  for (std::size_t activity = 0; activity < dfg.size(); ++activity) {
    if (remap[activity] == kMissing) continue;
    const std::size_t mapped = remap[activity];
    compact.start_count[mapped] = dfg.start_count[activity];
    compact.end_count[mapped] = dfg.end_count[activity];
    for (std::size_t other = 0; other < dfg.size(); ++other) {
      if (remap[other] != kMissing) {
        compact.edge_count[mapped][remap[other]] =
            dfg.edge_count[activity][other];
      }
    }
  }
  dfg = std::move(compact);
  apply_noise_filter(dfg, 0.0);
}

std::vector<LocalDfg> project_sequence_dfg(
    const LocalDfg& parent,
    const std::vector<std::vector<ActId>>& groups) {
  std::vector<std::size_t> group_of(parent.size(), 0);
  for (std::size_t group = 0; group < groups.size(); ++group) {
    for (const ActId activity : groups[group]) {
      group_of[parent.to_local.at(activity)] = group;
    }
  }

  std::vector<char> skippable(groups.size(), 0);
  for (std::size_t activity = 0; activity < parent.size(); ++activity) {
    if (parent.start_count[activity] > 0) {
      for (std::size_t group = 0; group < group_of[activity]; ++group) {
        skippable[group] = 1;
      }
    }
    if (parent.end_count[activity] > 0) {
      for (std::size_t group = group_of[activity] + 1; group < groups.size();
           ++group) {
        skippable[group] = 1;
      }
    }
    for (std::size_t target = 0; target < parent.size(); ++target) {
      if (parent.edge_count[activity][target] == 0) continue;
      for (std::size_t group = group_of[activity] + 1;
           group < group_of[target]; ++group) {
        skippable[group] = 1;
      }
    }
  }

  std::vector<LocalDfg> projected;
  projected.reserve(groups.size());
  for (std::size_t group = 0; group < groups.size(); ++group) {
    LocalDfg sub = induced_dfg(parent, groups[group], false);
    if (group == 0) {
      for (std::size_t activity = 0; activity < parent.size(); ++activity) {
        const auto target = sub.to_local.find(parent.to_global[activity]);
        if (target != sub.to_local.end()) {
          sub.start_count[target->second] += parent.start_count[activity];
        }
      }
    } else {
      for (const ActId source_global : groups[group - 1]) {
        const std::size_t source = parent.to_local.at(source_global);
        for (const ActId target_global : groups[group]) {
          const std::size_t target = parent.to_local.at(target_global);
          sub.start_count[sub.to_local.at(target_global)] +=
              parent.edge_count[source][target];
        }
      }
    }

    if (group + 1 == groups.size()) {
      for (std::size_t activity = 0; activity < parent.size(); ++activity) {
        const auto source = sub.to_local.find(parent.to_global[activity]);
        if (source != sub.to_local.end()) {
          sub.end_count[source->second] += parent.end_count[activity];
        }
      }
    } else {
      for (const ActId source_global : groups[group]) {
        const std::size_t source = parent.to_local.at(source_global);
        for (const ActId target_global : groups[group + 1]) {
          const std::size_t target = parent.to_local.at(target_global);
          sub.end_count[sub.to_local.at(source_global)] +=
              parent.edge_count[source][target];
        }
      }
    }
    sub.skip = skippable[group];
    finalize_projected_dfg(sub);
    projected.push_back(std::move(sub));
  }
  return projected;
}

std::vector<LocalDfg> project_loop_dfg(
    const LocalDfg& parent,
    const std::vector<std::vector<ActId>>& groups) {
  std::vector<LocalDfg> projected;
  projected.reserve(groups.size());
  bool redo_is_skippable = false;
  for (std::size_t source = 0; source < parent.size(); ++source) {
    if (parent.end_count[source] == 0) continue;
    for (std::size_t target = 0; target < parent.size(); ++target) {
      if (parent.start_count[target] > 0 &&
          parent.edge_count[source][target] > 0) {
        redo_is_skippable = true;
      }
    }
  }

  for (std::size_t group = 0; group < groups.size(); ++group) {
    LocalDfg sub = induced_dfg(parent, groups[group], false);
    if (group == 0) {
      for (std::size_t activity = 0; activity < parent.size(); ++activity) {
        const auto local = sub.to_local.find(parent.to_global[activity]);
        if (local != sub.to_local.end()) {
          sub.start_count[local->second] += parent.start_count[activity];
          sub.end_count[local->second] += parent.end_count[activity];
        } else if (parent.start_count[activity] > 0 ||
                   parent.end_count[activity] > 0) {
          sub.skip = true;
        }
      }
    } else {
      for (std::size_t activity = 0; activity < sub.size(); ++activity) {
        sub.start_count[activity] = 1;
        sub.end_count[activity] = 1;
      }
      sub.skip = redo_is_skippable;
    }
    finalize_projected_dfg(sub);
    projected.push_back(std::move(sub));
  }
  return projected;
}

ProcessTreeNode mine_dfg(LocalDfg dfg, double f,
                         const InductiveMinerOptions& options,
                         const ActivityLabels& labels) {
  // Imported graphs may register activities that have no occurrence, edge, or
  // boundary count. Keep those registry entries available to the surrounding
  // model while excluding them from the behavior being mined.
  finalize_projected_dfg(dfg);
  const bool has_graph_content =
      std::any_of(dfg.start_count.begin(), dfg.start_count.end(),
                  [](std::uint64_t count) { return count > 0; }) ||
      std::any_of(dfg.end_count.begin(), dfg.end_count.end(),
                  [](std::uint64_t count) { return count > 0; }) ||
      std::any_of(dfg.edge_count.begin(), dfg.edge_count.end(),
                  [](const auto& row) {
                    return std::any_of(
                        row.begin(), row.end(),
                        [](std::uint64_t count) { return count > 0; });
                  });
  if (dfg.size() == 0 || !has_graph_content) {
    return ProcessTreeNode::tau();
  }

  if (dfg.skip) {
    dfg.skip = false;
    return make_node(
        ProcessTreeOperator::kExclusive,
        {ProcessTreeNode::tau(), mine_dfg(std::move(dfg), f, options, labels)});
  }

  // A graph-recursive single activity is a leaf only when it has no self-edge.
  // A self-edge reaches the graph flower fall-through, whose silent body keeps
  // the graph abstraction's unknown empty-trace behavior explicit.
  if (dfg.size() == 1) {
    if (!dfg.has_edge(dfg.to_global.front(), dfg.to_global.front())) {
      return ProcessTreeNode::leaf(dfg.to_global.front());
    }
    return FlowerFallThrough::build(dfg.to_global, labels);
  }

  const auto build = [&](ProcessTreeOperator op,
                         const std::vector<std::vector<ActId>>& groups) {
    std::vector<LocalDfg> subgraphs;
    if (op == ProcessTreeOperator::kSequence) {
      subgraphs = project_sequence_dfg(dfg, groups);
    } else if (op == ProcessTreeOperator::kLoop) {
      subgraphs = project_loop_dfg(dfg, groups);
    } else {
      subgraphs.reserve(groups.size());
      for (const auto& group : groups) {
        LocalDfg subgraph = induced_dfg(dfg, group, true);
        finalize_projected_dfg(subgraph);
        subgraphs.push_back(std::move(subgraph));
      }
    }

    std::vector<ProcessTreeNode> children;
    children.reserve(subgraphs.size());
    for (auto& subgraph : subgraphs) {
      children.push_back(
          mine_dfg(std::move(subgraph), f, options, labels));
    }
    return make_node(op, std::move(children));
  };

  const auto try_cuts = [&](const LocalDfg& d) -> std::optional<ProcessTreeNode> {
    if (auto cut = CutFactory::find(d, options)) {
      return build(cut->operation(), cut->groups);
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

  return FlowerFallThrough::build(dfg.to_global, labels);
}

// IMd entry: one pass over the (variant-compressed) log builds the root graph,
// then the recursion never touches traces again. Empty traces are invisible to
// a directly-follows graph, so their explicit skip branch is decided here —
// the one place the log itself is still in hand.
ProcessTreeNode mine_dfg_root(const Sublog& log, double f,
                              const InductiveMinerOptions& options,
                              const ActivityLabels& labels) {
  std::uint64_t empty_weight = 0;
  std::uint64_t nonempty_weight = 0;
  for (const auto& wt : log) {
    (wt.activities.empty() ? empty_weight : nonempty_weight) += wt.count;
  }
  if (nonempty_weight == 0) {
    return ProcessTreeNode::tau();
  }

  LocalDfg dfg = build_local_dfg(log);
  // The graph-backed variant carries observed emptiness into the same
  // recursive skip channel used by projected sequence and loop children.
  dfg.skip = empty_weight > 0;
  return mine_dfg(std::move(dfg), f, options, labels);
}

void validate_options(const InductiveMinerOptions& options) {
  if (!std::isfinite(options.noise_threshold) ||
      options.noise_threshold < 0.0 || options.noise_threshold > 1.0) {
    throw std::invalid_argument(
        "inductive miner: noise_threshold must be finite and in [0, 1]");
  }
  if (!InductiveMinerInstances::supports(options.variant)) {
    throw std::invalid_argument("inductive miner: unsupported variant");
  }
  if (options.activity_key.empty() || options.timestamp_key.empty() ||
      options.case_id_key.empty()) {
    throw std::invalid_argument(
        "inductive miner: activity_key, timestamp_key, and case_id_key "
        "must not be empty");
  }
}

std::optional<std::string> event_value(const Event& event, const Trace& trace,
                                       std::string_view key,
                                       bool activity_selector) {
  if (activity_selector && (key == "concept:name" || key == "activity")) {
    if (event.has_activity()) return event.activity;
    return std::nullopt;
  }
  if (!activity_selector &&
      (key == "case:concept:name" || key == "case_id" ||
       key == "concept:name")) {
    if (trace.has_case_id()) return trace.case_id;
    return std::nullopt;
  }
  if (key == "time:timestamp" || key == "timestamp") {
    if (event.has_timestamp()) return event.timestamp;
    return std::nullopt;
  }
  if (key == "org:resource" || key == "resource") {
    if (event.has_resource()) return event.resource;
    return std::nullopt;
  }
  if (!event.activity_source_key.empty() &&
      key == event.activity_source_key && event.has_activity()) {
    return event.activity;
  }
  if (!event.timestamp_source_key.empty() &&
      key == event.timestamp_source_key && event.has_timestamp()) {
    return event.timestamp;
  }
  if (!event.resource_source_key.empty() &&
      key == event.resource_source_key && event.has_resource()) {
    return event.resource;
  }
  if (!trace.case_id_source_key.empty() &&
      key == trace.case_id_source_key && trace.has_case_id()) {
    return trace.case_id;
  }
  if (const auto* value = event.attribute(key)) return *value;
  if (const auto* value = trace.attribute(key)) return *value;
  return std::nullopt;
}

struct SelectedActivity {
  std::string value;
  bool is_null = false;
  std::optional<EventAttribute> attribute;
};

std::optional<SelectedActivity> selected_event_activity(
    const Event& event, const Trace& trace, std::string_view key) {
  const auto native = [](const std::string& value,
                         FirstClassAttributeState state, bool present,
                         const std::optional<EventAttribute>& attribute)
      -> std::optional<SelectedActivity> {
    if (!present) return std::nullopt;
    const bool is_null = state == FirstClassAttributeState::kNull;
    return SelectedActivity{is_null ? std::string{} : value, is_null,
                            attribute};
  };
  if (key == "concept:name" || key == "activity") {
    return native(event.activity, event.activity_state, event.has_activity(),
                  event.activity_attribute);
  }
  if (key == "time:timestamp" || key == "timestamp") {
    return native(event.timestamp, event.timestamp_state, event.has_timestamp(),
                  event.timestamp_attribute);
  }
  if (key == "org:resource" || key == "resource") {
    return native(event.resource, event.resource_state, event.has_resource(),
                  event.resource_attribute);
  }
  if (!event.activity_source_key.empty() && key == event.activity_source_key &&
      event.has_activity()) {
    return native(event.activity, event.activity_state, true,
                  event.activity_attribute);
  }
  if (!event.timestamp_source_key.empty() && key == event.timestamp_source_key &&
      event.has_timestamp()) {
    return native(event.timestamp, event.timestamp_state, true,
                  event.timestamp_attribute);
  }
  if (!event.resource_source_key.empty() && key == event.resource_source_key &&
      event.has_resource()) {
    return native(event.resource, event.resource_state, true,
                  event.resource_attribute);
  }
  if (!trace.case_id_source_key.empty() && key == trace.case_id_source_key &&
      trace.has_case_id()) {
    return native(trace.case_id, trace.case_id_state, true,
                  trace.case_id_attribute);
  }
  for (const auto* attributes : {&event.attributes, &trace.attributes}) {
    for (const auto& attribute : *attributes) {
      if (attribute.key == key)
        return SelectedActivity{
            attribute.is_null ? std::string{} : attribute.value,
            attribute.is_null, attribute};
    }
  }
  return std::nullopt;
}

TokenInteger activity_order_key(std::string_view label) {
  // RFC 1321 MD5 over the UTF-8 label bytes, interpreted as a digest integer.
  static constexpr std::array<std::uint32_t, 64> constants{
      0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
      0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
      0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
      0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
      0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
      0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
      0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
      0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
      0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
      0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
      0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
      0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
      0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
      0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
      0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
      0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};
  static constexpr std::array<int, 64> shifts{
      7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
      5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  std::vector<std::uint8_t> bytes;
  bytes.reserve(label.size() + 72);
  for (const char byte : label) bytes.push_back(static_cast<std::uint8_t>(byte));
  bytes.push_back(0x80);
  while (bytes.size() % 64 != 56) bytes.push_back(0);
  const auto bits = static_cast<std::uint64_t>(label.size()) * 8;
  for (unsigned index = 0; index < 8; ++index)
    bytes.push_back(static_cast<std::uint8_t>(bits >> (index * 8)));
  std::array<std::uint32_t, 4> state{
      0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
  for (std::size_t offset = 0; offset < bytes.size(); offset += 64) {
    std::array<std::uint32_t, 16> words{};
    for (std::size_t index = 0; index < 16; ++index) {
      for (unsigned byte = 0; byte < 4; ++byte)
        words[index] |= std::uint32_t{bytes[offset + index * 4 + byte]}
                        << (byte * 8);
    }
    auto [a, b, c, d] = state;
    for (std::size_t index = 0; index < 64; ++index) {
      std::uint32_t mixed;
      std::size_t word;
      if (index < 16) {
        mixed = (b & c) | (~b & d);
        word = index;
      } else if (index < 32) {
        mixed = (d & b) | (~d & c);
        word = (5 * index + 1) % 16;
      } else if (index < 48) {
        mixed = b ^ c ^ d;
        word = (3 * index + 5) % 16;
      } else {
        mixed = c ^ (b | ~d);
        word = (7 * index) % 16;
      }
      const auto previous_d = d;
      d = c;
      c = b;
      b += std::rotl(a + mixed + constants[index] + words[word], shifts[index]);
      a = previous_d;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
  }
  TokenInteger result;
  for (const auto word : state) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      result *= 256;
      result += static_cast<std::uint8_t>(word >> (byte * 8));
    }
  }
  return result;
}

TokenInteger tree_key(const ProcessTreeNode& node,
                      const std::vector<TokenInteger>& activity_keys) {
  if (node.op == ProcessTreeOperator::kTau) return 0;
  if (node.op == ProcessTreeOperator::kActivity)
    return activity_keys.at(node.activity);
  TokenInteger result;
  for (const auto& child : node.children) result += tree_key(child, activity_keys);
  return result;
}

void canonicalize_tree_children(
    ProcessTreeNode& node, const std::vector<TokenInteger>& activity_keys) {
  for (auto& child : node.children)
    canonicalize_tree_children(child, activity_keys);
  if (node.op != ProcessTreeOperator::kExclusive &&
      node.op != ProcessTreeOperator::kParallel) {
    return;
  }
  std::vector<ProcessTreeNode> flattened;
  flattened.reserve(node.children.size());
  for (auto& child : node.children) {
    if (child.op == node.op) {
      for (auto& grandchild : child.children)
        flattened.push_back(std::move(grandchild));
    } else {
      flattened.push_back(std::move(child));
    }
  }
  std::vector<std::pair<TokenInteger, ProcessTreeNode>> keyed;
  keyed.reserve(flattened.size());
  for (auto& child : flattened)
    keyed.emplace_back(tree_key(child, activity_keys), std::move(child));
  std::stable_sort(keyed.begin(), keyed.end(), [](const auto& left,
                                                const auto& right) {
    return left.first < right.first;
  });
  node.children.clear();
  node.children.reserve(keyed.size());
  for (auto& entry : keyed) node.children.push_back(std::move(entry.second));
}

void canonicalize_tree(ProcessTreeNode& node, const ActivityLabels& labels) {
  std::vector<TokenInteger> activity_keys;
  activity_keys.reserve(labels.size());
  for (const auto& label : labels) activity_keys.push_back(activity_order_key(label));
  canonicalize_tree_children(node, activity_keys);
}

EventLog select_event_log_fields(const EventLog& input,
                                 const InductiveMinerOptions& options) {
  const bool requested_regroup = options.case_id_key != "case:concept:name";
  const bool requested_timestamp = options.timestamp_key != "time:timestamp";
  const bool sort = options.sort_events_by_timestamp;
  EventLog output;
  output.attributes = input.attributes;
  output.metadata = input.metadata;
  std::unordered_map<std::string, std::size_t> trace_indexes;
  std::vector<char> sortable_traces;

  for (std::size_t trace_index = 0; trace_index < input.traces.size();
       ++trace_index) {
    const auto& source_trace = input.traces[trace_index];

    // An EventLog is already case-grouped.  A custom case selector is an
    // optional projection: use it when the source trace supplies the value,
    // but keep the native grouping when the field is absent.
    bool regroup_trace = requested_regroup;
    if (regroup_trace) {
      const Event empty_event;
      if (source_trace.events.empty()) {
        const auto value = event_value(empty_event, source_trace,
                                       options.case_id_key, false);
        regroup_trace = value.has_value() && !value->empty();
      } else {
        regroup_trace = std::all_of(
            source_trace.events.begin(), source_trace.events.end(),
            [&](const Event& event) {
              const auto value = event_value(event, source_trace,
                                             options.case_id_key, false);
              return value.has_value() && !value->empty();
            });
      }
    }

    // Timestamp projection and sorting are likewise opportunistic for an
    // EventLog.  Missing timestamps do not invalidate a trace that is already
    // ordered by its caller; a supplied complete selector still provides the
    // requested deterministic ordering.
    bool timestamp_available = true;
    if (requested_timestamp) {
      timestamp_available = std::all_of(
          source_trace.events.begin(), source_trace.events.end(),
          [&](const Event& event) {
            const auto value = event_value(event, source_trace,
                                           options.timestamp_key, false);
            return value.has_value() && !value->empty();
          });
    } else if (sort) {
      timestamp_available = std::all_of(
          source_trace.events.begin(), source_trace.events.end(),
          [](const Event& event) { return !event.timestamp.empty(); });
    }
    const bool project_timestamp = requested_timestamp && timestamp_available;

    if (source_trace.events.empty()) {
      if (!regroup_trace) {
        output.traces.push_back(source_trace);
        sortable_traces.push_back(1);
        continue;
      }
      const Event empty_event;
      const auto selected_case =
          event_value(empty_event, source_trace, options.case_id_key, false);
      const bool inserted =
          trace_indexes.emplace(*selected_case, output.traces.size()).second;
      if (inserted) {
        auto trace = source_trace;
        trace.events.clear();
        trace.case_id = *selected_case;
        trace.case_id_state = FirstClassAttributeState::kPresent;
        trace.case_id_source_key = options.case_id_key;
        output.traces.push_back(std::move(trace));
        sortable_traces.push_back(1);
      }
      continue;
    }
    std::size_t output_trace_index = std::numeric_limits<std::size_t>::max();
    std::optional<std::string> native_case;
    for (std::size_t event_index = 0; event_index < source_trace.events.size();
         ++event_index) {
      const auto& source_event = source_trace.events[event_index];
      auto activity =
          selected_event_activity(source_event, source_trace,
                                  options.activity_key);
      if (!activity) {
        throw std::invalid_argument(
            "inductive miner: activity field '" + options.activity_key +
            "' is missing in EventLog trace " +
            std::to_string(trace_index) + " event " +
            std::to_string(event_index));
      }
      auto event = source_event;
      event.activity = activity->value;
      event.activity_state = activity->is_null
                                 ? FirstClassAttributeState::kNull
                                 : FirstClassAttributeState::kPresent;
      event.activity_source_key = options.activity_key;
      event.activity_attribute = activity->attribute;

      if (project_timestamp) {
        const auto timestamp =
            event_value(source_event, source_trace, options.timestamp_key, false);
        event.timestamp = *timestamp;
        event.timestamp_state = FirstClassAttributeState::kPresent;
        event.timestamp_source_key = options.timestamp_key;
      }

      if (regroup_trace) {
        native_case =
            event_value(source_event, source_trace, options.case_id_key, false);
        const auto [it, inserted] =
            trace_indexes.emplace(*native_case, output.traces.size());
        if (inserted) {
          auto trace = source_trace;
          trace.events.clear();
          trace.case_id = *native_case;
          trace.case_id_state = FirstClassAttributeState::kPresent;
          trace.case_id_source_key = options.case_id_key;
          output.traces.push_back(std::move(trace));
          sortable_traces.push_back(1);
        }
        output_trace_index = it->second;
      } else if (output_trace_index == std::numeric_limits<std::size_t>::max()) {
        output_trace_index = output.traces.size();
        output.traces.push_back(source_trace);
        output.traces.back().events.clear();
        sortable_traces.push_back(1);
      }

      auto& target_trace = output.traces[output_trace_index];
      event.case_id = target_trace.case_id;
      event.case_id_state = target_trace.case_id_state;
      event.case_id_source_key = target_trace.case_id_source_key;
      target_trace.events.push_back(std::move(event));
      if (sort && !timestamp_available) {
        sortable_traces[output_trace_index] = 0;
      }
    }
  }
  if (sort) {
    for (std::size_t trace_index = 0; trace_index < output.traces.size();
         ++trace_index) {
      if (!sortable_traces[trace_index] || output.traces[trace_index].events.empty())
        continue;
      auto& trace = output.traces[trace_index];
      bool valid_timestamps = true;
      for (const auto& event : trace.events) {
        if (event.timestamp.empty()) {
          valid_timestamps = false;
          break;
        }
      }
      if (!valid_timestamps) continue;
      for (std::size_t event_index = 0; event_index < trace.events.size();
           ++event_index) {
        if (!parse_timestamp(trace.events[event_index].timestamp)) {
          throw std::invalid_argument(
              "inductive miner: timestamp field '" + options.timestamp_key +
              "' is invalid in EventLog trace " +
              std::to_string(trace_index) + " event " +
              std::to_string(event_index));
        }
      }
      order_events_by_time(trace.events);
    }
  }
  return output;
}

struct IndexedRecord {
  EventRecord record;
  std::size_t input_order = 0;
};

std::vector<IndexedRecord> event_data_records(const EventDataView& data) {
  std::vector<IndexedRecord> records;
  switch (data.kind()) {
    case EventDataKind::kEventStream:
      records.reserve(data.stream()->events.size());
      for (std::size_t i = 0; i < data.stream()->events.size(); ++i)
        records.push_back({data.stream()->events[i], i});
      return records;
    case EventDataKind::kEventTable: {
      const auto& table = *data.table();
      table.validate();
      records.reserve(table.rows.size());
      for (std::size_t row_index = 0; row_index < table.rows.size();
           ++row_index) {
        EventRecord record;
        record.fields.reserve(table.columns.size());
        for (std::size_t column = 0; column < table.columns.size(); ++column) {
          if (!table.rows[row_index][column]) continue;
          auto field = *table.rows[row_index][column];
          field.key = table.columns[column];
          record.fields.push_back(std::move(field));
        }
        records.push_back({std::move(record), row_index});
      }
      return records;
    }
    case EventDataKind::kLazyEventTable: {
      const auto table = data.lazy_table()->materialize();
      EventTable copy = table;
      EventDataView materialized(copy);
      return event_data_records(materialized);
    }
    case EventDataKind::kEventLog:
      break;
  }
  throw std::invalid_argument("inductive miner: unsupported event-data kind");
}

std::string required_record_value(const EventRecord& record,
                                  std::string_view key,
                                  std::string_view role,
                                  std::size_t row) {
  const auto* field = record.field(key);
  if (!field || field->is_null || field->value.empty()) {
    throw std::invalid_argument(
        "inductive miner: " + std::string(role) + " field '" +
        std::string(key) + "' is missing or null at row " +
        std::to_string(row));
  }
  return field->value;
}

EventLog event_data_to_log(const EventDataView& data,
                           const InductiveMinerOptions& options) {
  auto records = event_data_records(data);
  EventLog log;
  std::unordered_map<std::string, std::size_t> traces;
  for (auto& indexed : records) {
    const std::string case_id = required_record_value(
        indexed.record, options.case_id_key, "case id", indexed.input_order);
    const std::string activity = required_record_value(
        indexed.record, options.activity_key, "activity", indexed.input_order);
    const std::string timestamp = required_record_value(
        indexed.record, options.timestamp_key, "timestamp", indexed.input_order);
    if (!parse_timestamp(timestamp)) {
      throw std::invalid_argument(
          "inductive miner: timestamp field '" + options.timestamp_key +
          "' is invalid at row " + std::to_string(indexed.input_order));
    }

    auto [trace_it, inserted] = traces.emplace(case_id, log.traces.size());
    if (inserted) {
      Trace trace;
      trace.case_id = case_id;
      trace.case_id_state = FirstClassAttributeState::kPresent;
      trace.case_id_source_key = options.case_id_key;
      log.traces.push_back(std::move(trace));
    }
    auto& trace = log.traces[trace_it->second];
    Event event;
    event.case_id = case_id;
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = options.case_id_key;
    event.activity = activity;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_source_key = options.activity_key;
    event.timestamp = timestamp;
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.timestamp_source_key = options.timestamp_key;
    event.input_order = indexed.input_order;
    trace.events.push_back(std::move(event));
  }
  for (auto& trace : log.traces) {
    order_events_by_time(trace.events);
  }
  return log;
}

bool InductiveMinerInstances::supports(InductiveMinerVariant variant) noexcept {
  switch (variant) {
    case InductiveMinerVariant::kLog:
    case InductiveMinerVariant::kImf:
    case InductiveMinerVariant::kDfg:
      return true;
    default:
      return false;
  }
}

std::vector<InductiveMinerVariant> InductiveMinerInstances::all() {
  return {InductiveMinerVariant::kLog, InductiveMinerVariant::kImf,
          InductiveMinerVariant::kDfg};
}

double InductiveMinerInfrequentVariant::threshold(
    const InductiveMinerOptions& options) {
  return options.noise_threshold;
}

ProcessTree InductiveMinerLogVariant::discover(
    const InductiveDataStructure& data, const InductiveMinerOptions& options,
    const ActivityLabels& labels) {
  ProcessTreeNode root = mine(data.variants, 0.0, options, labels);
  canonicalize_tree(root, labels);
  return ProcessTree(labels, std::move(root));
}

ProcessTree InductiveMinerInfrequentVariant::discover(
    const InductiveDataStructure& data, const InductiveMinerOptions& options,
    const ActivityLabels& labels) {
  ProcessTreeNode root = mine(data.variants, threshold(options), options, labels);
  canonicalize_tree(root, labels);
  return ProcessTree(labels, std::move(root));
}

ProcessTree InductiveMinerDfgVariant::discover(
    const InductiveDataStructure& data, const InductiveMinerOptions& options,
    const ActivityLabels& labels) {
  ProcessTreeNode root = mine_dfg_root(data.variants, 0.0, options, labels);
  canonicalize_tree(root, labels);
  return ProcessTree(labels, std::move(root));
}

ProcessTree InductiveMinerDfgVariant::discover(
    const DirectlyFollowsGraph& graph, const InductiveMinerOptions& options) {
  const auto& labels = graph.activities();
  ProcessTreeNode root = mine_dfg(build_local_dfg(graph), 0.0, options, labels);
  canonicalize_tree(root, labels);
  return ProcessTree(graph.activities(), std::move(root));
}

ProcessTree InductiveMinerFramework::discover(
    const InductiveDataStructure& data, const InductiveMinerOptions& options,
    const ActivityLabels& labels) {
  if (data.empty()) return ProcessTree(labels, BaseCaseFactory::empty());
  switch (options.variant) {
    case InductiveMinerVariant::kLog:
      return InductiveMinerLogVariant::discover(data, options, labels);
    case InductiveMinerVariant::kImf:
      return InductiveMinerInfrequentVariant::discover(data, options, labels);
    case InductiveMinerVariant::kDfg:
      return InductiveMinerDfgVariant::discover(data, options, labels);
    default:
      throw std::invalid_argument("inductive miner: unsupported variant");
  }
}

ProcessTree discover_from_event_log(const EventLog& event_log,
                                    const InductiveMinerOptions& options) {
  bool has_null = false;
  bool has_string = false;
  bool repeated_null = false;
  for (const auto& trace : event_log.traces) {
    for (const auto& event : trace.events) {
      if (event.activity_state == FirstClassAttributeState::kNull) {
        has_null = true;
        repeated_null = repeated_null || trace.events.size() > 1;
      } else {
        has_string = true;
      }
    }
  }
  if (has_null) {
    // Null and string labels cannot share the ordered discovery alphabet.
    if (has_string)
      throw std::invalid_argument(
          "inductive miner: null and string activity labels are not orderable");
    if (repeated_null)
      throw std::invalid_argument("None cannot be a node");
    return ProcessTree({}, ProcessTreeNode::tau());
  }
  std::set<std::string> alphabet;
  for (const auto& trace : event_log.traces)
    for (const auto& event : trace.events) alphabet.insert(event.activity);
  std::vector<std::string> names(alphabet.begin(), alphabet.end());
  std::unordered_map<std::string, ActId> ids;
  for (ActId id = 0; id < names.size(); ++id) ids.emplace(names[id], id);

  Sublog sublog;
  std::unordered_map<std::string, std::size_t> variant_index;
  for (const auto& trace : event_log.traces) {
    std::vector<ActId> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      activities.push_back(ids.at(event.activity));
    }

    auto [vit, vinserted] =
        variant_index.emplace(trace_key(activities), sublog.size());
    if (vinserted) {
      sublog.push_back(WeightedTrace{std::move(activities), 1});
    } else {
      ++sublog[vit->second].count;
    }
  }

  return InductiveMinerFramework::discover(
      InductiveDataStructure{std::move(sublog)}, options, names);
}

ProcessTree discover_from_variants(const InductiveMinerVariantLog& variants,
                                   const InductiveMinerOptions& options) {
  std::set<std::string> alphabet;
  for (const auto& variant : variants)
    for (const auto& label : variant.activities) alphabet.insert(label);
  std::vector<std::string> names(alphabet.begin(), alphabet.end());
  std::unordered_map<std::string, ActId> ids;
  for (ActId id = 0; id < names.size(); ++id) ids.emplace(names[id], id);
  Sublog sublog;
  std::unordered_map<std::string, std::size_t> variant_index;
  for (const auto& variant : variants) {
    if (variant.count == 0) continue;
    std::vector<ActId> activities;
    activities.reserve(variant.activities.size());
    for (const auto& label : variant.activities)
      activities.push_back(ids.at(label));
    auto [it, inserted] =
        variant_index.emplace(trace_key(activities), sublog.size());
    if (inserted) {
      sublog.push_back(WeightedTrace{std::move(activities), variant.count});
    } else {
      sublog[it->second].count += variant.count;
    }
  }
  return InductiveMinerFramework::discover(
      InductiveDataStructure{std::move(sublog)}, options, names);
}

}  // namespace inductive_detail

using namespace inductive_detail;

EventLog prepare_inductive_event_log(const EventLog& event_log,
                                     const InductiveMinerOptions& options) {
  validate_options(options);
  return select_event_log_fields(event_log, options);
}

ProcessTree discover_process_tree(const EventLog& event_log,
                                  const InductiveMinerOptions& options) {
  validate_options(options);
  return discover_from_event_log(prepare_inductive_event_log(event_log, options),
                                 options);
}

ProcessTree discover_process_tree(const DirectlyFollowsGraph& graph,
                                  const InductiveMinerOptions& options) {
  validate_options(options);
  // A graph input has already discarded trace bodies, so it always uses the
  // graph-recursive semantics. Selector values are validated for a uniform
  // public contract but do not affect a graph that has no event columns.
  return InductiveMinerDfgVariant::discover(graph, options);
}

ProcessTree discover_process_tree(const EventDataView& data,
                                  const InductiveMinerOptions& options) {
  validate_options(options);
  if (data.kind() == EventDataKind::kEventLog) {
    return discover_from_event_log(
        prepare_inductive_event_log(*data.log(), options), options);
  }
  return discover_from_event_log(event_data_to_log(data, options), options);
}

ProcessTree discover_process_tree(const InductiveMinerVariantLog& variants,
                                  const InductiveMinerOptions& options) {
  validate_options(options);
  return discover_from_variants(variants, options);
}

}  // namespace minera
