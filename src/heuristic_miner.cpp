#include "minera/heuristic_miner.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minera {
namespace {

// Direct-succession counts plus the start and end activities, the only log
// statistics the dependency measure needs.
struct Counts {
  std::vector<std::string> names;
  std::vector<std::vector<std::uint64_t>> follows;  // follows[a][b] = |a > b|
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;

  [[nodiscard]] std::size_t size() const { return names.size(); }
};

Counts build_counts(const EventLog& log) {
  Counts counts;
  std::unordered_map<std::string, std::size_t> ids;
  const auto intern = [&](const std::string& name) {
    const auto [it, inserted] = ids.try_emplace(name, counts.names.size());
    if (inserted) counts.names.push_back(name);
    return it->second;
  };

  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) intern(event.activity);
  }

  const std::size_t n = counts.names.size();
  counts.follows.assign(n, std::vector<std::uint64_t>(n, 0));
  for (const auto& trace : log.traces) {
    const auto& events = trace.events;
    if (events.empty()) continue;
    counts.starts.insert(ids[events.front().activity]);
    counts.ends.insert(ids[events.back().activity]);
    for (std::size_t i = 1; i < events.size(); ++i) {
      ++counts.follows[ids[events[i - 1].activity]][ids[events[i].activity]];
    }
  }
  return counts;
}

double dependency(const Counts& counts, std::size_t a, std::size_t b) {
  const double forward = static_cast<double>(counts.follows[a][b]);
  if (a == b) {
    return forward / (forward + 1.0);
  }
  const double backward = static_cast<double>(counts.follows[b][a]);
  return (forward - backward) / (forward + backward + 1.0);
}

// Groups the neighbours on one side of `center` into AND bundles. Two edges
// leaving (or entering) the same activity are bundled when their targets (or
// sources) follow each other often enough relative to the edges themselves —
// the AND measure (see HeuristicMinerOptions::and_threshold). The relation is
// closed transitively (b~c and c~d put b, c, d in one bundle), so bundles
// partition the neighbours: XOR across bundles, AND within.
std::vector<HeuristicBundle> to_bundles(const Counts& counts,
                                        std::size_t center,
                                        const std::vector<std::size_t>& neighbours,
                                        bool outgoing,
                                        double threshold) {
  const std::size_t k = neighbours.size();
  std::vector<std::size_t> root(k);
  for (std::size_t i = 0; i < k; ++i) root[i] = i;
  const auto find = [&](std::size_t x) {
    while (root[x] != x) x = root[x] = root[root[x]];
    return x;
  };

  for (std::size_t i = 0; i < k; ++i) {
    for (std::size_t j = i + 1; j < k; ++j) {
      const std::size_t b = neighbours[i];
      const std::size_t c = neighbours[j];
      const double between =
          static_cast<double>(counts.follows[b][c] + counts.follows[c][b]);
      const double base = static_cast<double>(
          outgoing ? counts.follows[center][b] + counts.follows[center][c]
                   : counts.follows[b][center] + counts.follows[c][center]);
      if (between / (base + 1.0) >= threshold) root[find(i)] = find(j);
    }
  }

  // Collect the classes in first-appearance order so bundles are deterministic.
  std::vector<HeuristicBundle> bundles;
  std::unordered_map<std::size_t, std::size_t> bundle_of_root;
  for (std::size_t i = 0; i < k; ++i) {
    const auto [it, inserted] = bundle_of_root.emplace(find(i), bundles.size());
    if (inserted) bundles.emplace_back();
    bundles[it->second].push_back(neighbours[i]);
  }
  return bundles;
}

}  // namespace

HeuristicsNet discover_heuristics_net(
    const EventLog& log,
    const HeuristicMinerOptions& options) {
  const Counts counts = build_counts(log);
  const std::size_t n = counts.size();

  HeuristicsNet net;
  net.activities = counts.names;
  net.start_activities.assign(counts.starts.begin(), counts.starts.end());
  net.end_activities.assign(counts.ends.begin(), counts.ends.end());

  // Accept an edge once, keyed by (source, target) so the threshold pass and
  // the connectivity pass never emit a duplicate.
  std::set<std::pair<std::size_t, std::size_t>> accepted;
  const auto accept = [&](std::size_t a, std::size_t b) {
    if (!accepted.emplace(a, b).second) return;
    net.edges.push_back(HeuristicDependency{a, b, dependency(counts, a, b),
                                            counts.follows[a][b]});
  };

  // Every relation that clears both thresholds.
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = 0; b < n; ++b) {
      if (counts.follows[a][b] >= options.min_frequency &&
          dependency(counts, a, b) >= options.dependency_threshold) {
        accept(a, b);
      }
    }
  }

  // Connectivity heuristic: give every non-end activity its strongest outgoing
  // dependency and every non-start activity its strongest incoming one, so the
  // graph stays connected even where the threshold would prune the last edge.
  if (options.connect_all) {
    const auto best_target = [&](std::size_t a) {
      bool found = false;
      std::size_t best = 0;
      double best_dep = 0.0;
      for (std::size_t b = 0; b < n; ++b) {
        if (b == a || counts.follows[a][b] == 0) continue;
        const double dep = dependency(counts, a, b);
        if (!found || dep > best_dep) {
          found = true;
          best = b;
          best_dep = dep;
        }
      }
      return std::pair<bool, std::size_t>{found && best_dep > 0.0, best};
    };
    const auto best_source = [&](std::size_t b) {
      bool found = false;
      std::size_t best = 0;
      double best_dep = 0.0;
      for (std::size_t a = 0; a < n; ++a) {
        if (a == b || counts.follows[a][b] == 0) continue;
        const double dep = dependency(counts, a, b);
        if (!found || dep > best_dep) {
          found = true;
          best = a;
          best_dep = dep;
        }
      }
      return std::pair<bool, std::size_t>{found && best_dep > 0.0, best};
    };

    std::vector<bool> has_out(n, false);
    std::vector<bool> has_in(n, false);
    for (const auto& [a, b] : accepted) {
      has_out[a] = true;
      has_in[b] = true;
    }
    for (std::size_t a = 0; a < n; ++a) {
      if (!has_out[a] && counts.ends.find(a) == counts.ends.end()) {
        if (const auto [ok, b] = best_target(a); ok) accept(a, b);
      }
      if (!has_in[a] && counts.starts.find(a) == counts.starts.end()) {
        if (const auto [ok, b] = best_source(a); ok) accept(b, a);
      }
    }
  }

  std::sort(net.edges.begin(), net.edges.end(),
            [](const HeuristicDependency& lhs, const HeuristicDependency& rhs) {
              if (lhs.source != rhs.source) return lhs.source < rhs.source;
              return lhs.target < rhs.target;
            });

  // Derive the split/join semantics: partition each activity's successors and
  // predecessors into AND bundles. A self-loop is kept out of the clustering
  // and appended as its own bundle — repeating is always an alternative to
  // moving on, never concurrent with it.
  net.output_bundles.assign(n, {});
  net.input_bundles.assign(n, {});
  std::vector<std::vector<std::size_t>> successors(n);
  std::vector<std::vector<std::size_t>> predecessors(n);
  std::vector<bool> self_loop(n, false);
  for (const auto& edge : net.edges) {
    if (edge.source == edge.target) {
      self_loop[edge.source] = true;
    } else {
      successors[edge.source].push_back(edge.target);
      predecessors[edge.target].push_back(edge.source);
    }
  }
  for (std::size_t a = 0; a < n; ++a) {
    net.output_bundles[a] =
        to_bundles(counts, a, successors[a], true, options.and_threshold);
    net.input_bundles[a] =
        to_bundles(counts, a, predecessors[a], false, options.and_threshold);
    if (self_loop[a]) {
      net.output_bundles[a].push_back({a});
      net.input_bundles[a].push_back({a});
    }
  }
  return net;
}

}  // namespace minera
