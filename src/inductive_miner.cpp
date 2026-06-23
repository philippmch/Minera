#include "minera/inductive_miner.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
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
// read; under the infrequent variant they are the *filtered* relations, while
// the raw counts are retained for the noise-aware base cases.
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
//   * an edge a->b survives only when its count is at least `f` times a's
//     strongest outgoing edge (so a node always keeps its dominant successor);
//   * an activity counts as a start/end only when its start/end count clears
//     `f` times the most frequent start/end.
// With f == 0 nothing is dropped: every observed edge and every observed
// start/end is kept, exactly reproducing the classic Inductive Miner.
void apply_noise_filter(LocalDfg& dfg, double f) {
  const std::size_t n = dfg.size();
  dfg.follows.assign(n, std::vector<char>(n, 0));
  dfg.succ.assign(n, {});
  dfg.is_start.assign(n, 0);
  dfg.is_end.assign(n, 0);

  for (std::size_t a = 0; a < n; ++a) {
    std::uint64_t max_out = 0;
    for (std::size_t b = 0; b < n; ++b) {
      max_out = std::max(max_out, dfg.edge_count[a][b]);
    }
    const double cutoff = f * static_cast<double>(max_out);
    for (std::size_t b = 0; b < n; ++b) {
      const std::uint64_t count = dfg.edge_count[a][b];
      if (count > 0 && static_cast<double>(count) >= cutoff) {
        dfg.follows[a][b] = 1;
        dfg.succ[a].push_back(b);
      }
    }
  }

  std::uint64_t max_start = 0;
  std::uint64_t max_end = 0;
  for (std::size_t a = 0; a < n; ++a) {
    max_start = std::max(max_start, dfg.start_count[a]);
    max_end = std::max(max_end, dfg.end_count[a]);
  }
  for (std::size_t a = 0; a < n; ++a) {
    if (dfg.start_count[a] > 0 &&
        static_cast<double>(dfg.start_count[a]) >= f * static_cast<double>(max_start)) {
      dfg.is_start[a] = 1;
    }
    if (dfg.end_count[a] > 0 &&
        static_cast<double>(dfg.end_count[a]) >= f * static_cast<double>(max_end)) {
      dfg.is_end[a] = 1;
    }
  }
}

LocalDfg build_local_dfg(const Sublog& log, double f) {
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

  apply_noise_filter(dfg, f);
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

  // Reachability lifted to groups, then a strict order by out-degree: the
  // earliest group reaches all others, the next all but one, and so on.
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
  std::vector<std::size_t> out_degree(g, 0);
  for (std::size_t k = 0; k < g; ++k) {
    for (std::size_t l = 0; l < g; ++l) {
      out_degree[k] += static_cast<std::size_t>(greach[k][l] != 0);
    }
  }
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return out_degree[a] > out_degree[b];
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
  constexpr std::size_t kBody = 0;

  std::vector<std::size_t> group_of(n, kBody);
  std::vector<char> assigned(n, 0);
  for (std::size_t i = 0; i < n; ++i) {
    if (dfg.is_start[i] || dfg.is_end[i]) {
      assigned[i] = 1;
    }
  }

  // Connected components of the activities outside the body become candidate
  // redo paths, numbered from 1.
  std::size_t next_group = 1;
  std::vector<std::size_t> stack;
  for (std::size_t i = 0; i < n; ++i) {
    if (assigned[i]) {
      continue;
    }
    const std::size_t group = next_group++;
    stack.assign(1, i);
    assigned[i] = 1;
    group_of[i] = group;
    while (!stack.empty()) {
      const std::size_t cur = stack.back();
      stack.pop_back();
      for (std::size_t j = 0; j < n; ++j) {
        if (!assigned[j] && (dfg.follows[cur][j] || dfg.follows[j][cur])) {
          assigned[j] = 1;
          group_of[j] = group;
          stack.push_back(j);
        }
      }
    }
  }

  // Whether redo activity `a` meets the connection and completeness conditions
  // against the current body. Re-evaluated as the body grows.
  const auto redo_activity_valid = [&](std::size_t a) {
    bool entered_from_end = false;
    bool exits_to_start = false;
    for (std::size_t b = 0; b < n; ++b) {
      if (group_of[b] != kBody) {
        continue;
      }
      if (dfg.follows[a][b] && !dfg.is_start[b]) {
        return false;  // redo returns to a non-start body activity
      }
      if (dfg.follows[b][a] && !dfg.is_end[b]) {
        return false;  // body enters redo from a non-end activity
      }
      if (dfg.follows[b][a]) {
        entered_from_end = true;  // b is an end activity (checked above)
      }
      if (dfg.follows[a][b]) {
        exits_to_start = true;  // b is a start activity (checked above)
      }
    }
    // Completeness: an entry reachable from one end must be reachable from all
    // end activities; an exit reaching one start must reach all of them.
    if (entered_from_end) {
      for (std::size_t e = 0; e < n; ++e) {
        if (dfg.is_end[e] && group_of[e] == kBody && !dfg.follows[e][a]) {
          return false;
        }
      }
    }
    if (exits_to_start) {
      for (std::size_t s = 0; s < n; ++s) {
        if (dfg.is_start[s] && group_of[s] == kBody && !dfg.follows[a][s]) {
          return false;
        }
      }
    }
    return true;
  };

  // Fold invalid redo paths back into the body until the partition is stable.
  // Growing the body can invalidate a previously valid path, so we re-check.
  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t a = 0; a < n && !changed; ++a) {
      if (group_of[a] == kBody || redo_activity_valid(a)) {
        continue;
      }
      const std::size_t fold = group_of[a];
      for (std::size_t k = 0; k < n; ++k) {
        if (group_of[k] == fold) {
          group_of[k] = kBody;
        }
      }
      changed = true;
    }
  }

  std::unordered_map<std::size_t, std::size_t> group_index;
  group_index.emplace(kBody, 0);  // body is always child 0
  std::vector<std::vector<std::size_t>> local_groups(1);
  for (std::size_t i = 0; i < n; ++i) {
    auto [it, inserted] = group_index.emplace(group_of[i], local_groups.size());
    if (inserted) {
      local_groups.emplace_back();
    }
    local_groups[it->second].push_back(i);
  }
  if (local_groups.size() < 2) {
    return std::nullopt;
  }
  return to_global(dfg, local_groups);
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

// Exclusive: every trace's activities live in one component, so the whole
// trace is routed there.
std::vector<Sublog> split_exclusive(const Sublog& log,
                                    std::size_t groups,
                                    const GroupOf& group_of) {
  std::vector<Sublog> out(groups);
  for (const auto& wt : log) {
    const std::size_t target =
        wt.activities.empty() ? 0 : group_of.at(wt.activities.front());
    out[target].push_back(wt);
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

ProcessTreeNode mine(const Sublog& log, double f);

ProcessTreeNode build_from_cut(ProcessTreeOperator op,
                               const std::vector<std::vector<ActId>>& groups,
                               const Sublog& log,
                               double f) {
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
    children.push_back(mine(sublog, f));
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

// Standard Inductive Miner recursion: base cases, then the four cut detectors
// in order, then the fall-throughs (empty trace, single activity, flower). The
// directly-follows graph ignores empty traces, so empty traces ride through any
// cut that fits the non-empty behaviour and are only modelled explicitly when
// no cut applies -- this matches the canonical algorithm rather than skipping
// the empty trace eagerly.
ProcessTreeNode mine(const Sublog& log, double f) {
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

  const LocalDfg dfg = build_local_dfg(log, f);

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

  if (auto cut = exclusive_cut(dfg)) {
    return build_from_cut(ProcessTreeOperator::kExclusive, *cut, log, f);
  }
  if (auto cut = sequence_cut(dfg)) {
    return build_from_cut(ProcessTreeOperator::kSequence, *cut, log, f);
  }
  if (auto cut = parallel_cut(dfg)) {
    return build_from_cut(ProcessTreeOperator::kParallel, *cut, log, f);
  }
  if (auto cut = loop_cut(dfg)) {
    return build_from_cut(ProcessTreeOperator::kLoop, *cut, log, f);
  }

  // Fall-through for empty traces. When they are frequent enough they are kept
  // as an explicit skip around the rest of the log; when they are noise (below
  // the threshold) they are dropped and the non-empty behaviour is re-mined.
  if (has_empty) {
    Sublog nonempty;
    nonempty.reserve(log.size());
    for (const auto& wt : log) {
      if (!wt.activities.empty()) {
        nonempty.push_back(wt);
      }
    }
    if (static_cast<double>(empty_weight) >
        f * static_cast<double>(nonempty_weight)) {
      return make_node(ProcessTreeOperator::kExclusive,
                       {ProcessTreeNode::tau(), mine(nonempty, f)});
    }
    return mine(nonempty, f);
  }

  // Fall-through: unstructured behaviour becomes a flower model.
  return flower(dfg.to_global);
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
  ProcessTreeNode root = mine(sublog, f);
  return ProcessTree(std::move(names), std::move(root));
}

}  // namespace minera
