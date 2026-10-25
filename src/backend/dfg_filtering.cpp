#include "minera/dfg_filtering.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

using NameSet = std::unordered_set<std::string>;
using Adjacency = std::map<std::string, std::set<std::string>>;

[[noreturn]] void fail(std::string type, std::string message) {
  throw FilteringCommonError(std::move(type), std::move(message));
}

std::string quoted(const std::string& value) {
  const bool use_double = value.find('\'') != std::string::npos && value.find('"') == std::string::npos;
  const char quote = use_double ? '"' : '\'';
  std::string out(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (const char c : value) {
    const auto u = static_cast<unsigned char>(c);
    if (c == quote || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if (u < 0x20 || u == 0x7f) {
      out += "\\x";
      out += hex[u >> 4];
      out += hex[u & 15];
    } else {
      out += c;
    }
  }
  return out + quote;
}

[[noreturn]] void key_error(const std::string& key) { fail("KeyError", quoted(key)); }

template <class Key>
const std::int64_t* find_count(const std::vector<std::pair<Key, std::int64_t>>& map, const Key& key) {
  for (const auto& [k, v] : map)
    if (k == key) return &v;
  return nullptr;
}

std::int64_t required_count(const DfgFilteringCounts& map, const std::string& key) {
  const auto* found = find_count(map, key);
  if (!found) key_error(key);
  return *found;
}

bool contains(const DfgFilteringCounts& map, const std::string& key) { return find_count(map, key) != nullptr; }

template <class Map, class Keep>
Map filtered(const Map& map, Keep keep) {
  Map out;
  for (const auto& entry : map)
    if (keep(entry.first)) out.push_back(entry);
  return out;
}

Adjacency adjacency_of(const DfgFilteringEdges& dfg, const DfgFilteringCounts& sa, const DfgFilteringCounts& ea,
                       Adjacency* reverse) {
  Adjacency adj;
  Adjacency rev;
  for (const auto& [edge, count] : dfg) {
    adj[edge.first];
    adj[edge.second];
    rev[edge.first];
    rev[edge.second];
  }
  for (const auto& [a, count] : sa) {
    adj[a];
    rev[a];
  }
  for (const auto& [a, count] : ea) {
    adj[a];
    rev[a];
  }
  for (const auto& [edge, count] : dfg) {
    adj[edge.first].insert(edge.second);
    rev[edge.second].insert(edge.first);
  }
  adj[kDfgFilteringStartNode].clear();
  rev[kDfgFilteringStartNode].clear();
  adj[kDfgFilteringEndNode].clear();
  rev[kDfgFilteringEndNode].clear();
  for (const auto& [a, count] : sa) {
    adj[kDfgFilteringStartNode].insert(a);
    rev[a].insert(kDfgFilteringStartNode);
  }
  for (const auto& [a, count] : ea) {
    adj[a].insert(kDfgFilteringEndNode);
    rev[kDfgFilteringEndNode].insert(a);
  }
  if (reverse) *reverse = std::move(rev);
  return adj;
}

NameSet reach(const std::string& start, const Adjacency& adjacency) {
  NameSet visited{start};
  std::deque<std::string> queue{start};
  while (!queue.empty()) {
    const auto node = queue.front();
    queue.pop_front();
    const auto found = adjacency.find(node);
    if (found == adjacency.end()) key_error(node);
    for (const auto& next : found->second)
      if (visited.insert(next).second) queue.push_back(next);
  }
  return visited;
}

bool connected(const DfgFilteringEdges& dfg, const DfgFilteringCounts& sa, const DfgFilteringCounts& ea,
               const std::vector<std::string>& required) {
  Adjacency rev;
  const auto adj = adjacency_of(dfg, sa, ea, &rev);
  const auto forward = reach(kDfgFilteringStartNode, adj);
  const auto backward = reach(kDfgFilteringEndNode, rev);
  for (const auto& r : required)
    if (!forward.count(r) || !backward.count(r)) return false;
  return true;
}

// Drops the activities that are not both reachable from the start and able to
// reach the end, with their edges and boundary entries; then keeps only the
// edges between remaining activities.
DfgFilteringData prune(DfgFilteringData data) {
  Adjacency rev;
  const auto adj = adjacency_of(data.dfg, data.start_activities, data.end_activities, &rev);
  const auto forward = reach(kDfgFilteringStartNode, adj);
  const auto backward = reach(kDfgFilteringEndNode, rev);
  NameSet removed;
  for (const auto& [a, count] : data.activities)
    if (!forward.count(a) || !backward.count(a)) removed.insert(a);
  const auto kept = [&](const std::string& a) { return !removed.count(a); };
  data.activities = filtered(data.activities, kept);
  data.start_activities = filtered(data.start_activities, kept);
  data.end_activities = filtered(data.end_activities, kept);
  data.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) {
    return contains(data.activities, e.first) && contains(data.activities, e.second);
  });
  return data;
}

// The int of math.ceil(value) + 1 under Python's conversion errors.
std::int64_t keep_count(double value) {
  if (std::isnan(value)) fail("ValueError", "cannot convert float NaN to integer");
  if (std::isinf(value)) fail("OverflowError", "cannot convert float infinity to integer");
  const double c = std::ceil(value);
  if (c >= 4.0e18) return std::int64_t{4000000000000000000};
  if (c <= -4.0e18) return -std::int64_t{4000000000000000000};
  return static_cast<std::int64_t>(c) + 1;
}

// The split point of sequence[:k] for a Python slice bound.
std::size_t slice_end(std::int64_t k, std::size_t size) {
  const auto n = static_cast<std::int64_t>(size);
  if (k < 0) k = std::max<std::int64_t>(0, n + k);
  return static_cast<std::size_t>(std::min(k, n));
}

DfgFilteringData drop_activity(const DfgFilteringData& data, const std::string& x) {
  DfgFilteringData out;
  out.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) { return e.first != x && e.second != x; });
  const auto other = [&](const std::string& a) { return a != x; };
  out.start_activities = filtered(data.start_activities, other);
  out.end_activities = filtered(data.end_activities, other);
  out.activities = filtered(data.activities, other);
  return out;
}

void drop_edge(DfgFilteringData& data, const DfgFilteringEdge& edge) {
  if (edge.first == kDfgFilteringStartNode) {
    data.start_activities = filtered(data.start_activities, [&](const std::string& a) { return a != edge.second; });
  } else if (edge.second == kDfgFilteringEndNode) {
    data.end_activities = filtered(data.end_activities, [&](const std::string& a) { return a != edge.first; });
  } else {
    data.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) { return e != edge; });
  }
}

DfgFilteringEdges all_edges(const DfgFilteringData& data) {
  DfgFilteringEdges out = data.dfg;
  const auto put = [&](DfgFilteringEdge key, std::int64_t count) {
    for (auto& [k, v] : out)
      if (k == key) {
        v = count;
        return;
      }
    out.push_back({std::move(key), count});
  };
  for (const auto& [a, c] : data.start_activities) put({kDfgFilteringStartNode, a}, c);
  for (const auto& [a, c] : data.end_activities) put({a, kDfgFilteringEndNode}, c);
  return out;
}

std::vector<std::string> every_activity(const DfgFilteringData& data) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (const auto* map : {&data.activities, &data.start_activities, &data.end_activities})
    for (const auto& [a, c] : *map)
      if (seen.insert(a).second) out.push_back(a);
  return out;
}

std::vector<std::string> endpoints(const std::vector<DfgFilteringEdge>& edges) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (const auto& e : edges)
    for (const auto* n : {&e.first, &e.second})
      if (*n != kDfgFilteringStartNode && *n != kDfgFilteringEndNode && seen.insert(*n).second) out.push_back(*n);
  return out;
}

DfgFilteringData remove_edges_greedily(DfgFilteringData data, const std::vector<DfgFilteringEdge>& others,
                                       const std::vector<std::string>& required) {
  for (auto it = others.rbegin(); it != others.rend(); ++it) {
    auto trial = data;
    drop_edge(trial, *it);
    if (connected(trial.dfg, trial.start_activities, trial.end_activities, required)) data = std::move(trial);
  }
  return prune(std::move(data));
}

NameSet follow(const std::vector<std::string>& starts, const DfgFilteringEdges& dfg, bool forward) {
  std::unordered_map<std::string, std::vector<std::string>> next;
  for (const auto& [e, c] : dfg) {
    if (forward) {
      next[e.first].push_back(e.second);
    } else {
      next[e.second].push_back(e.first);
    }
  }
  NameSet visited(starts.begin(), starts.end());
  std::deque<std::string> queue(starts.begin(), starts.end());
  while (!queue.empty()) {
    const auto node = queue.front();
    queue.pop_front();
    const auto found = next.find(node);
    if (found == next.end()) continue;
    for (const auto& m : found->second)
      if (visited.insert(m).second) queue.push_back(m);
  }
  return visited;
}

void require_known(const DfgFilteringEdges& dfg, const DfgFilteringCounts& boundary, const DfgFilteringCounts& act) {
  for (const auto& [e, c] : dfg) {
    if (!contains(act, e.second)) key_error(e.second);
    if (!contains(act, e.first)) key_error(e.first);
  }
  for (const auto& [a, c] : boundary)
    if (!contains(act, a)) key_error(a);
}

std::vector<std::string> keys_of(const DfgFilteringCounts& map) {
  std::vector<std::string> out;
  for (const auto& [a, c] : map) out.push_back(a);
  return out;
}

DfgFilteringData restrict_to(const DfgFilteringData& data, const NameSet& keep) {
  const auto in = [&](const std::string& a) { return keep.count(a) > 0; };
  DfgFilteringData out;
  out.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) { return in(e.first) && in(e.second); });
  out.start_activities = filtered(data.start_activities, in);
  out.end_activities = filtered(data.end_activities, in);
  out.activities = filtered(data.activities, in);
  return out;
}

std::string uuid4() {
  static thread_local std::mt19937_64 engine{std::random_device{}()};
  std::uniform_int_distribution<std::uint64_t> bits;
  std::uint64_t hi = bits(engine);
  std::uint64_t lo = bits(engine);
  hi = (hi & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
  lo = (lo & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;
  char buffer[37];
  std::snprintf(buffer, sizeof buffer, "%08x-%04x-%04x-%04x-%012llx", static_cast<unsigned>(hi >> 32),
                static_cast<unsigned>((hi >> 16) & 0xffff), static_cast<unsigned>(hi & 0xffff),
                static_cast<unsigned>(lo >> 48), static_cast<unsigned long long>(lo & 0xffffffffffffULL));
  return buffer;
}

}  // namespace

DfgFilteringData filter_dfg_on_activities_percentage(const DfgFilteringData& input, double percentage) {
  DfgFilteringData data = input;
  if (data.activities.size() <= 1 || data.dfg.size() <= 1) return data;
  auto sorted = data.activities;
  std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first > b.first;
  });
  const auto k = keep_count(static_cast<double>(data.activities.size() - 1) * percentage);
  const auto split = slice_end(k, sorted.size());
  std::vector<std::string> keep;
  for (std::size_t i = 0; i < split; ++i) keep.push_back(sorted[i].first);
  for (std::size_t i = sorted.size(); i > split; --i) {
    auto trial = drop_activity(data, sorted[i - 1].first);
    if (connected(trial.dfg, trial.start_activities, trial.end_activities, keep)) data = std::move(trial);
  }
  return prune(std::move(data));
}

DfgFilteringData filter_dfg_on_paths_percentage(const DfgFilteringData& input, double percentage,
                                                bool keep_all_activities) {
  DfgFilteringData data = input;
  if (data.activities.size() <= 1 || data.dfg.size() <= 1) return data;
  auto edges = all_edges(data);
  std::stable_sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first > b.first;
  });
  const auto k = keep_count(static_cast<double>(edges.size() - 1) * percentage);
  const auto split = slice_end(k, edges.size());
  std::vector<DfgFilteringEdge> kept, others;
  for (std::size_t i = 0; i < edges.size(); ++i) (i < split ? kept : others).push_back(edges[i].first);
  const auto required = keep_all_activities ? every_activity(data) : endpoints(kept);
  return remove_edges_greedily(std::move(data), others, required);
}

DfgFilteringData filter_dfg_keep_connected(const DfgFilteringData& input, double threshold, bool keep_all_activities) {
  DfgFilteringData data = input;
  if (data.activities.size() <= 1 || data.dfg.size() <= 1) return data;
  const auto edges = all_edges(data);
  std::vector<std::pair<DfgFilteringEdge, double>> scored;
  for (const auto& [edge, count] : edges) {
    double score = 1.0;
    if (edge.first != kDfgFilteringStartNode && edge.second != kDfgFilteringEndNode) {
      const auto* inverse = find_count(data.dfg, DfgFilteringEdge{edge.second, edge.first});
      const std::int64_t inv = inverse ? *inverse : 0;
      const std::int64_t denominator = count + inv + 1;
      if (denominator == 0) fail("ZeroDivisionError", "division by zero");
      score = static_cast<double>(count - inv) / static_cast<double>(denominator);
    }
    scored.push_back({edge, score});
  }
  std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first > b.first;
  });
  std::vector<DfgFilteringEdge> kept, others;
  for (const auto& [edge, score] : scored) (score < threshold ? others : kept).push_back(edge);
  const auto required = keep_all_activities ? every_activity(data) : endpoints(kept);
  return remove_edges_greedily(std::move(data), others, required);
}

DfgFilteringEdges clean_dfg_based_on_noise_thresh(const DfgFilteringEdges& dfg,
                                                  const std::vector<std::string>& activities, double noise_threshold,
                                                  const std::optional<std::vector<DfgFilteringEdge>>& most_common_paths) {
  std::unordered_map<std::string, std::int64_t> maximum;
  for (const auto& a : activities) {
    std::int64_t best = 0;  // the reference's maxima start at zero
    for (const auto& [edge, count] : dfg)
      if ((edge.first == a || edge.second == a) && count > best) best = count;
    maximum[a] = best;
  }
  const auto lookup = [&](const std::string& a) {
    const auto found = maximum.find(a);
    if (found == maximum.end()) key_error(a);
    return found->second;
  };
  DfgFilteringEdges out;
  for (const auto& [edge, count] : dfg) {
    const bool common = most_common_paths &&
                        std::find(most_common_paths->begin(), most_common_paths->end(), edge) != most_common_paths->end();
    if (!common) {
      const double first = static_cast<double>(lookup(edge.first)) * noise_threshold;
      const double second = static_cast<double>(lookup(edge.second)) * noise_threshold;
      const double bound = second < first ? second : first;  // Python min(first, second)
      if (static_cast<double>(count) < bound) continue;
    }
    out.push_back({edge, count});
  }
  return out;
}

DfgFilteringData filter_dfg_to_activity(const DfgFilteringData& input, const std::string& target) {
  DfgFilteringData data;
  data.end_activities = {{target, required_count(input.activities, target)}};
  data.dfg = filtered(input.dfg, [&](const DfgFilteringEdge& e) { return e.first != target; });
  data.start_activities = input.start_activities;
  data.activities = input.activities;
  require_known(data.dfg, data.start_activities, data.activities);
  auto keep = follow(keys_of(data.start_activities), data.dfg, true);
  const auto back = follow({target}, data.dfg, false);
  NameSet both{target};
  for (const auto& n : keep)
    if (back.count(n)) both.insert(n);
  return restrict_to(data, both);
}

DfgFilteringData filter_dfg_from_activity(const DfgFilteringData& input, const std::string& source) {
  DfgFilteringData data;
  data.start_activities = {{source, required_count(input.activities, source)}};
  data.dfg = filtered(input.dfg, [&](const DfgFilteringEdge& e) { return e.second != source; });
  data.end_activities = input.end_activities;
  data.activities = input.activities;
  require_known(data.dfg, data.end_activities, data.activities);
  const auto forward = follow({source}, data.dfg, true);
  const auto back = follow(keys_of(data.end_activities), data.dfg, false);
  NameSet both{source};
  for (const auto& n : forward)
    if (back.count(n)) both.insert(n);
  return restrict_to(data, both);
}

DfgFilteringData filter_dfg_contain_activity(const DfgFilteringData& input, const std::string& activity) {
  require_known(input.dfg, {}, input.activities);
  (void)required_count(input.activities, activity);
  const auto backward = follow({activity}, input.dfg, false);
  const auto forward = follow({activity}, input.dfg, true);
  NameSet either = backward;
  either.insert(forward.begin(), forward.end());
  DfgFilteringData out = restrict_to(input, either);
  out.start_activities = filtered(input.start_activities, [&](const std::string& a) { return backward.count(a) > 0; });
  out.end_activities = filtered(input.end_activities, [&](const std::string& a) { return forward.count(a) > 0; });
  return out;
}

DfgFilteringAdjacency build_adjacency_structures(const DfgFilteringEdges& dfg, const DfgFilteringCounts& sa,
                                                 const DfgFilteringCounts& ea) {
  DfgFilteringAdjacency out;
  out.adjacency = adjacency_of(dfg, sa, ea, &out.reverse_adjacency);
  return out;
}

std::set<std::string> bfs_reachable(const std::string& start, const DfgFilteringSuccessors& adjacency) {
  std::set<std::string> visited{start};
  std::deque<std::string> queue{start};
  while (!queue.empty()) {
    const auto node = queue.front();
    queue.pop_front();
    const auto found = adjacency.find(node);
    if (found == adjacency.end()) key_error(node);
    for (const auto& next : found->second)
      if (visited.insert(next).second) queue.push_back(next);
  }
  return visited;
}

DfgFilteringData remove_unreachable_nodes(DfgFilteringData& data, const DfgFilteringSuccessors& adjacency,
                                          const DfgFilteringSuccessors& reverse_adjacency,
                                          const std::string& start_node, const std::string& end_node) {
  const auto forward = bfs_reachable(start_node, adjacency);
  const auto backward = bfs_reachable(end_node, reverse_adjacency);
  NameSet keep;
  for (const auto& n : forward)
    if (backward.count(n) && n != start_node && n != end_node) keep.insert(n);
  NameSet removed;
  for (const auto& [a, count] : data.activities)
    if (!keep.count(a)) removed.insert(a);
  const auto kept = [&](const std::string& a) { return !removed.count(a); };
  data.activities = filtered(data.activities, kept);
  data.start_activities = filtered(data.start_activities, kept);
  data.end_activities = filtered(data.end_activities, kept);
  DfgFilteringData out;
  out.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) {
    return contains(data.activities, e.first) && contains(data.activities, e.second);
  });
  out.start_activities = data.start_activities;
  out.end_activities = data.end_activities;
  out.activities = data.activities;
  return out;
}

DfgFilteringGraph generate_nx_graph_from_dfg(const DfgFilteringData& data) {
  DfgFilteringGraph graph;
  graph.start_node = uuid4();
  graph.end_node = uuid4();
  std::vector<std::vector<std::string>> successors;
  std::unordered_map<std::string, std::size_t> index;
  const auto node = [&](const std::string& name) {
    const auto [it, fresh] = index.emplace(name, graph.nodes.size());
    if (fresh) {
      graph.nodes.push_back(name);
      successors.emplace_back();
    }
    return it->second;
  };
  const auto edge = [&](const std::string& a, const std::string& b) {
    const auto from = node(a);
    node(b);
    auto& next = successors[from];
    if (std::find(next.begin(), next.end(), b) == next.end()) next.push_back(b);
  };
  node(graph.start_node);
  node(graph.end_node);
  for (const auto& [a, c] : data.activities) node(a);
  for (const auto& [e, c] : data.dfg) edge(e.first, e.second);
  for (const auto& [a, c] : data.start_activities) edge(graph.start_node, a);
  for (const auto& [a, c] : data.end_activities) edge(a, graph.end_node);
  for (std::size_t i = 0; i < graph.nodes.size(); ++i)
    for (const auto& b : successors[i]) graph.edges.push_back({graph.nodes[i], b});
  return graph;
}

}  // namespace minera
