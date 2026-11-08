#include "minera/dfg_filtering.hpp"

#include "minera/directly_follows_graph.hpp"

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

// The filters' own graph. Node 0 is the start node and node 1 the end node;
// activities get ids from 2 on, so an activity named like the reference's
// "_S_START_"/"_S_END_" nodes stays an ordinary activity (review X1-2). Arcs
// are the directly-follows edges, start arcs (0 -> a) and end arcs (a -> 1).
constexpr int kStartId = 0;
constexpr int kEndId = 1;

struct PairHash {
  std::size_t operator()(const DfgFilteringEdge& e) const noexcept {
    const std::hash<std::string> h;
    return h(e.first) * 1000003u ^ h(e.second);
  }
};

class FilterGraph {
 public:
  struct Arc {
    int from;
    int to;
  };

  explicit FilterGraph(const DfgFilteringData& data) {
    names_ = {kDfgFilteringStartNode, kDfgFilteringEndNode};
    for (const auto& [e, c] : data.dfg) add_arc(node(e.first), node(e.second));
    for (const auto& [a, c] : data.start_activities) add_arc(kStartId, node(a));
    for (const auto& [a, c] : data.end_activities) add_arc(node(a), kEndId);
  }

  [[nodiscard]] const std::vector<Arc>& arcs() const noexcept { return arcs_; }
  [[nodiscard]] std::size_t node_count() const noexcept { return names_.size(); }
  // The activity's node id, or -1 when it is on no arc.
  [[nodiscard]] int find(const std::string& name) const {
    const auto found = ids_.find(name);
    return found == ids_.end() ? -1 : found->second;
  }

  void set_arc_alive(std::size_t arc, bool alive) { arc_alive_[arc] = alive; }
  [[nodiscard]] bool arc_alive(std::size_t arc) const { return arc_alive_[arc]; }
  void set_node_alive(int node, bool alive) { node_alive_[static_cast<std::size_t>(node)] = alive; }

  // Breadth-first reachability from the start node (forward) and to the end
  // node (backward) over live arcs and nodes, with the arc each node was
  // first reached by (-1 for none).
  struct Search {
    std::vector<char> forward, backward;
    std::vector<int> forward_parent, backward_parent;
  };
  [[nodiscard]] Search search() const {
    Search s;
    sweep(true, s.forward, s.forward_parent);
    sweep(false, s.backward, s.backward_parent);
    return s;
  }

  [[nodiscard]] bool reaches(const Search& s, const std::vector<int>& required) const {
    for (const int r : required) {
      if (r < 0) return false;
      const auto i = static_cast<std::size_t>(r);
      if (!s.forward[i] || !s.backward[i]) return false;
    }
    return true;
  }

 private:
  int node(const std::string& name) {
    const auto [it, fresh] = ids_.try_emplace(name, static_cast<int>(names_.size()));
    if (fresh) {
      names_.push_back(name);
      out_.emplace_back();
      in_.emplace_back();
      node_alive_.push_back(1);
    }
    return it->second;
  }

  void add_arc(int from, int to) {
    const auto index = static_cast<int>(arcs_.size());
    arcs_.push_back({from, to});
    arc_alive_.push_back(1);
    out_[static_cast<std::size_t>(from)].push_back(index);
    in_[static_cast<std::size_t>(to)].push_back(index);
  }

  void sweep(bool forward, std::vector<char>& seen, std::vector<int>& parent) const {
    seen.assign(names_.size(), 0);
    parent.assign(names_.size(), -1);
    const int root = forward ? kStartId : kEndId;
    seen[static_cast<std::size_t>(root)] = 1;
    std::vector<int> queue{root};
    for (std::size_t head = 0; head < queue.size(); ++head) {
      const auto n = static_cast<std::size_t>(queue[head]);
      for (const int arc : forward ? out_[n] : in_[n]) {
        if (!arc_alive_[static_cast<std::size_t>(arc)]) continue;
        const auto& a = arcs_[static_cast<std::size_t>(arc)];
        const auto m = static_cast<std::size_t>(forward ? a.to : a.from);
        if (seen[m] || !node_alive_[m]) continue;
        seen[m] = 1;
        parent[m] = arc;
        queue.push_back(static_cast<int>(m));
      }
    }
  }

  std::vector<std::string> names_;
  std::unordered_map<std::string, int> ids_;
  std::vector<Arc> arcs_;
  std::vector<char> arc_alive_;
  std::vector<char> node_alive_{1, 1};
  std::vector<std::vector<int>> out_{{}, {}};
  std::vector<std::vector<int>> in_{{}, {}};
};

std::vector<int> node_ids(const FilterGraph& graph, const std::vector<std::string>& names) {
  std::vector<int> out;
  out.reserve(names.size());
  for (const auto& n : names) out.push_back(graph.find(n));
  return out;
}

// Drops the activities that are not both reachable from the start and able to
// reach the end, with their edges and boundary entries; then keeps only the
// edges between remaining activities.
DfgFilteringData prune(DfgFilteringData data) {
  const FilterGraph graph(data);
  const auto s = graph.search();
  NameSet removed;
  for (const auto& [a, count] : data.activities) {
    const int id = graph.find(a);
    if (id < 0 || !s.forward[static_cast<std::size_t>(id)] || !s.backward[static_cast<std::size_t>(id)])
      removed.insert(a);
  }
  const auto kept = [&](const std::string& a) { return !removed.count(a); };
  data.activities = filtered(data.activities, kept);
  data.start_activities = filtered(data.start_activities, kept);
  data.end_activities = filtered(data.end_activities, kept);
  NameSet present;
  for (const auto& [a, c] : data.activities) present.insert(a);
  data.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) { return present.count(e.first) && present.count(e.second); });
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

// One arc of the paths and keep-connected filters: its arc index in a
// FilterGraph built from the same data, its count and its sort key (start and
// end arcs spelled with the reference's node names).
struct RankedArc {
  std::size_t arc;
  DfgFilteringEdge key;
  std::int64_t count;
  bool boundary;  // a start or end arc
};

// The directly-follows edges, then the start arcs, then the end arcs, in the
// order FilterGraph numbers them.
std::vector<RankedArc> all_arcs(const DfgFilteringData& data) {
  std::vector<RankedArc> out;
  out.reserve(data.dfg.size() + data.start_activities.size() + data.end_activities.size());
  for (const auto& [e, c] : data.dfg) out.push_back({out.size(), e, c, false});
  for (const auto& [a, c] : data.start_activities) out.push_back({out.size(), {kDfgFilteringStartNode, a}, c, true});
  for (const auto& [a, c] : data.end_activities) out.push_back({out.size(), {a, kDfgFilteringEndNode}, c, true});
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

// The activities on the kept arcs, start and end nodes excluded.
std::vector<std::string> endpoints(const DfgFilteringData& data, const std::vector<RankedArc>& kept) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  const auto see = [&](const std::string& a) {
    if (seen.insert(a).second) out.push_back(a);
  };
  const auto start_count = data.dfg.size();
  const auto end_from = start_count + data.start_activities.size();
  for (const auto& r : kept) {
    if (r.arc < start_count) {
      see(r.key.first);
      see(r.key.second);
    } else if (r.arc < end_from) {
      see(r.key.second);
    } else {
      see(r.key.first);
    }
  }
  return out;
}

// Tries the arcs of `others` from the last one up: an arc is removed when
// every required activity stays reachable from the start and able to reach
// the end. Reachability only shrinks, so an arc on neither breadth-first tree
// is removed without a new search, and a failed trial restores the trees.
DfgFilteringData remove_edges_greedily(DfgFilteringData data, const std::vector<RankedArc>& others,
                                       const std::vector<std::string>& required_names) {
  FilterGraph graph(data);
  const auto required = node_ids(graph, required_names);
  auto current = graph.search();
  if (graph.reaches(current, required)) {
    std::vector<char> on_tree(graph.arcs().size(), 0);
    const auto mark = [&](const FilterGraph::Search& s) {
      std::fill(on_tree.begin(), on_tree.end(), 0);
      for (const int p : s.forward_parent)
        if (p >= 0) on_tree[static_cast<std::size_t>(p)] = 1;
      for (const int p : s.backward_parent)
        if (p >= 0) on_tree[static_cast<std::size_t>(p)] = 1;
    };
    mark(current);
    for (auto it = others.rbegin(); it != others.rend(); ++it) {
      graph.set_arc_alive(it->arc, false);
      if (!on_tree[it->arc]) continue;
      auto trial = graph.search();
      if (graph.reaches(trial, required)) {
        current = std::move(trial);
        mark(current);
      } else {
        graph.set_arc_alive(it->arc, true);
      }
    }
  }
  const auto start_from = data.dfg.size();
  const auto end_from = start_from + data.start_activities.size();
  std::size_t i = 0;
  data.dfg = filtered(data.dfg, [&](const DfgFilteringEdge&) { return graph.arc_alive(i++); });
  i = start_from;
  data.start_activities = filtered(data.start_activities, [&](const std::string&) { return graph.arc_alive(i++); });
  i = end_from;
  data.end_activities = filtered(data.end_activities, [&](const std::string&) { return graph.arc_alive(i++); });
  return prune(std::move(data));
}

void sort_ranked(std::vector<RankedArc>& arcs) {
  std::stable_sort(arcs.begin(), arcs.end(), [](const RankedArc& a, const RankedArc& b) {
    return a.count != b.count ? a.count > b.count : a.key > b.key;
  });
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
  // An activity is dropped when every kept activity stays reachable from the
  // start and able to reach the end without it; reachability only shrinks, so
  // nothing is dropped when the kept activities are not connected to begin
  // with, and an activity on no arc is always dropped then.
  FilterGraph graph(data);
  const auto required = node_ids(graph, keep);
  NameSet dropped;
  if (graph.reaches(graph.search(), required)) {
    for (std::size_t i = sorted.size(); i > split; --i) {
      const auto& x = sorted[i - 1].first;
      const int id = graph.find(x);
      if (id >= 0) {
        graph.set_node_alive(id, false);
        if (!graph.reaches(graph.search(), required)) {
          graph.set_node_alive(id, true);
          continue;
        }
      }
      dropped.insert(x);
    }
  }
  if (!dropped.empty()) {
    const auto kept = [&](const std::string& a) { return !dropped.count(a); };
    data.dfg = filtered(data.dfg, [&](const DfgFilteringEdge& e) { return kept(e.first) && kept(e.second); });
    data.start_activities = filtered(data.start_activities, kept);
    data.end_activities = filtered(data.end_activities, kept);
    data.activities = filtered(data.activities, kept);
  }
  return prune(std::move(data));
}

DfgFilteringData filter_dfg_on_paths_percentage(const DfgFilteringData& input, double percentage,
                                                bool keep_all_activities) {
  DfgFilteringData data = input;
  if (data.activities.size() <= 1 || data.dfg.size() <= 1) return data;
  auto arcs = all_arcs(data);
  sort_ranked(arcs);
  const auto k = keep_count(static_cast<double>(arcs.size() - 1) * percentage);
  const auto split = slice_end(k, arcs.size());
  const std::vector<RankedArc> kept(arcs.begin(), arcs.begin() + static_cast<std::ptrdiff_t>(split));
  const std::vector<RankedArc> others(arcs.begin() + static_cast<std::ptrdiff_t>(split), arcs.end());
  const auto required = keep_all_activities ? every_activity(data) : endpoints(data, kept);
  return remove_edges_greedily(std::move(data), others, required);
}

DfgFilteringData filter_dfg_keep_connected(const DfgFilteringData& input, double threshold, bool keep_all_activities) {
  DfgFilteringData data = input;
  if (data.activities.size() <= 1 || data.dfg.size() <= 1) return data;
  std::unordered_map<DfgFilteringEdge, std::int64_t, PairHash> counts;
  for (const auto& [edge, count] : data.dfg) counts.try_emplace(edge, count);
  std::vector<std::pair<RankedArc, double>> scored;
  for (const auto& arc : all_arcs(data)) {
    double score = 1.0;
    if (!arc.boundary) {
      const auto inverse = counts.find(DfgFilteringEdge{arc.key.second, arc.key.first});
      const std::int64_t inv = inverse == counts.end() ? 0 : inverse->second;
      const std::int64_t denominator = arc.count + inv + 1;
      if (denominator == 0) fail("ZeroDivisionError", "division by zero");
      score = static_cast<double>(arc.count - inv) / static_cast<double>(denominator);
    }
    scored.push_back({arc, score});
  }
  std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first.key > b.first.key;
  });
  std::vector<RankedArc> kept, others;
  for (const auto& [arc, score] : scored) (score < threshold ? others : kept).push_back(arc);
  const auto required = keep_all_activities ? every_activity(data) : endpoints(data, kept);
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

DfgFilteringData dfg_filtering_data_from_graph(const DirectlyFollowsGraph& graph) {
  DfgFilteringData data;
  const auto& names = graph.activities();
  for (const auto& edge : graph.edges()) {
    if (edge.source >= names.size() || edge.target >= names.size())
      throw std::invalid_argument("DFG edge refers to an unknown activity");
    if (edge.count == 0) continue;
    data.dfg.push_back({{names[edge.source], names[edge.target]}, static_cast<std::int64_t>(edge.count)});
  }
  for (const auto& name : names) {
    const auto count = graph.activity_count(name);
    const auto starts = graph.start_activity_count(name);
    const auto ends = graph.end_activity_count(name);
    if (count != 0) data.activities.push_back({name, static_cast<std::int64_t>(count)});
    if (starts != 0) data.start_activities.push_back({name, static_cast<std::int64_t>(starts)});
    if (ends != 0) data.end_activities.push_back({name, static_cast<std::int64_t>(ends)});
  }
  return data;
}

DfgFilteringData dfg_filtering_data_from_dfg(const DfgCleanAnalysis& dfg) {
  DfgFilteringData data;
  std::vector<std::string> order;
  std::unordered_map<std::string, std::pair<std::int64_t, std::int64_t>> sums;  // incoming, outgoing
  const auto see = [&](const std::string& name) {
    if (sums.try_emplace(name, 0, 0).second) order.push_back(name);
  };
  for (const auto& [edge, count] : dfg.graph) {
    const auto value = static_cast<std::int64_t>(count);
    data.dfg.push_back({edge, value});
    see(edge.first);
    see(edge.second);
    sums[edge.first].second += value;
    sums[edge.second].first += value;
  }
  for (const auto& name : order) {
    const auto& [incoming, outgoing] = sums.at(name);
    data.activities.push_back({name, std::max(incoming, outgoing)});
  }
  for (const auto& [name, count] : dfg.start_activities)
    data.start_activities.push_back({name, static_cast<std::int64_t>(count)});
  for (const auto& [name, count] : dfg.end_activities)
    data.end_activities.push_back({name, static_cast<std::int64_t>(count)});
  return data;
}

DfgFilteringData apply_dfg_filtering_request(DfgFilteringData data, const DfgFilteringRequest& request) {
  if (request.noise_threshold) {
    std::vector<std::string> activities;
    for (const auto& [name, count] : data.activities) activities.push_back(name);
    data.dfg = clean_dfg_based_on_noise_thresh(data.dfg, activities, *request.noise_threshold,
                                               request.most_common_paths);
  }
  const bool keep_all = request.keep_all_activities;
  if (request.activities_percentage)
    data = filter_dfg_on_activities_percentage(data, *request.activities_percentage);
  if (request.paths_percentage) data = filter_dfg_on_paths_percentage(data, *request.paths_percentage, keep_all);
  if (request.keep_connected) data = filter_dfg_keep_connected(data, *request.keep_connected, keep_all);
  if (request.count_boundary_activities &&
      (request.to_activity || request.from_activity || request.contain_activity)) {
    std::vector<std::pair<std::string, std::int64_t>> extra;
    const auto add = [&](const std::string& name, std::int64_t count) {
      if (contains(data.activities, name)) return;
      for (auto& [n, c] : extra)
        if (n == name) {
          c = std::max(c, count);
          return;
        }
      extra.push_back({name, count});
    };
    for (const auto& [name, count] : data.start_activities) add(name, count);
    for (const auto& [name, count] : data.end_activities) add(name, count);
    data.activities.insert(data.activities.end(), extra.begin(), extra.end());
  }
  if (request.to_activity) data = filter_dfg_to_activity(data, *request.to_activity);
  if (request.from_activity) data = filter_dfg_from_activity(data, *request.from_activity);
  if (request.contain_activity) data = filter_dfg_contain_activity(data, *request.contain_activity);
  return data;
}

DfgCleanAnalysis to_dfg_clean_analysis(const DfgFilteringData& data) {
  const auto unsigned_count = [](std::int64_t count) {
    if (count < 0) throw std::invalid_argument("DFG count is negative");
    return static_cast<std::uint64_t>(count);
  };
  DfgCleanAnalysis out;
  for (const auto& [edge, count] : data.dfg) out.graph[edge] = unsigned_count(count);
  for (const auto& [name, count] : data.start_activities) out.start_activities[name] = unsigned_count(count);
  for (const auto& [name, count] : data.end_activities) out.end_activities[name] = unsigned_count(count);
  return out;
}

}  // namespace minera
