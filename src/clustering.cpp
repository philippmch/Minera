#include "minera/clustering.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {
constexpr std::size_t kHardGroups = 100000;
constexpr std::size_t kHardNodes = 200000;

using Sequence = std::vector<std::string>;
struct Variant { Sequence seq; std::size_t count = 0; };
using Profile = std::vector<Variant>;

std::string json_quote(const std::string& s) {
  std::ostringstream o; o << '"';
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') o << '\\' << c;
    else if (c < 0x20) o << "\\u00" << std::hex << std::setw(2)
                              << std::setfill('0') << static_cast<int>(c)
                              << std::dec << std::setfill(' ');
    else o << c;
  }
  return o << '"', o.str();
}

double levenshtein(const Sequence& a, const Sequence& b) {
  if (a.empty() && b.empty()) return 0.0;
  std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
  std::iota(prev.begin(), prev.end(), 0);
  for (std::size_t i = 1; i <= a.size(); ++i) {
    cur[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j)
      cur[j] = std::min({cur[j - 1] + 1, prev[j] + 1,
                         prev[j - 1] + static_cast<std::size_t>(a[i - 1] != b[j - 1])});
    prev.swap(cur);
  }
  const auto n = std::max(a.size(), b.size());
  return n ? static_cast<double>(prev[b.size()]) / static_cast<double>(n) : 0.0;
}

using Counts = std::map<std::string, double>;
Counts activity_counts(const Sequence& seq) {
  Counts c; for (const auto& s : seq) ++c[s]; return c;
}
Counts succession_counts(const Sequence& seq) {
  Counts c; for (std::size_t i = 1; i < seq.size(); ++i) ++c[seq[i - 1] + "\x1f" + seq[i]]; return c;
}
double cosine_distance(const Counts& a, const Counts& b) {
  if (a.empty() && b.empty()) return 0.0;
  double aa = 0, bb = 0, ab = 0;
  auto i = a.begin(), j = b.begin();
  while (i != a.end() || j != b.end()) {
    if (j == b.end() || (i != a.end() && i->first < j->first)) { aa += i->second*i->second; ++i; }
    else if (i == a.end() || j->first < i->first) { bb += j->second*j->second; ++j; }
    else { aa += i->second*i->second; bb += j->second*j->second; ab += i->second*j->second; ++i; ++j; }
  }
  if (aa == 0.0 && bb == 0.0) return 0.0;
  if (aa == 0.0 || bb == 0.0) return 1.0;
  const double sim = ab / std::sqrt(aa * bb);
  return std::clamp(1.0 - sim, 0.0, 1.0);
}

Profile profile(const std::vector<const Trace*>& traces, double percent) {
  std::map<Sequence, std::size_t> counts;
  for (const auto* t : traces) { Sequence s; for (const auto& e : t->events) s.push_back(e.activity); ++counts[s]; }
  Profile p; p.reserve(counts.size());
  for (const auto& [s, n] : counts) p.push_back({s, n});
  std::sort(p.begin(), p.end(), [](const Variant& a, const Variant& b) {
    if (a.count != b.count) return a.count > b.count; return a.seq < b.seq;
  });
  if (percent >= 1.0 || p.empty()) return p;
  std::size_t total = 0, keep = 0;
  for (const auto& v : p) {
    if (static_cast<double>(total + v.count) / traces.size() > percent) break;
    total += v.count;
    ++keep;
  }
  p.resize(keep); return p;
}

double pair_distance(const Profile& a, const Profile& b,
                     TraceAttributeClusteringVariant variant, double alpha) {
  if (a.empty() || b.empty()) return a.empty() == b.empty() ? 0.0 : 1.0;
  auto one = [&](const Variant& x, const Variant& y) {
    if (variant == TraceAttributeClusteringVariant::kAvgLeven || variant == TraceAttributeClusteringVariant::kDmmLeven)
      return levenshtein(x.seq, y.seq);
    const auto act = cosine_distance(activity_counts(x.seq), activity_counts(y.seq));
    const auto suc = cosine_distance(succession_counts(x.seq), succession_counts(y.seq));
    return std::clamp(act * alpha + suc * (1.0 - alpha), 0.0, 1.0);
  };
  const bool dmm = variant == TraceAttributeClusteringVariant::kDmmLeven || variant == TraceAttributeClusteringVariant::kDmmVec;
  if (variant == TraceAttributeClusteringVariant::kDfg) {
    double act = 0, suc = 0; Counts ac, bc, as, bs;
    for (const auto& x : a) { for (const auto& [k,v] : activity_counts(x.seq)) ac[k] += v*x.count; for (const auto& [k,v] : succession_counts(x.seq)) as[k] += v*x.count; }
    for (const auto& x : b) { for (const auto& [k,v] : activity_counts(x.seq)) bc[k] += v*x.count; for (const auto& [k,v] : succession_counts(x.seq)) bs[k] += v*x.count; }
    act = cosine_distance(ac, bc); suc = cosine_distance(as, bs); return std::clamp(act*alpha + suc*(1-alpha),0.0,1.0);
  }
  double sum = 0, weight = 0;
  if (dmm) {
    // The directional minimum-matching distance first matches every variant
    // on the larger side to its nearest variant on the smaller side. Exact
    // matches consume that smaller-side variant and receive a doubled weight;
    // only unmatched smaller-side variants are then matched in reverse. This
    // keeps duplicate exact variants from being counted as two independent
    // penalties while preserving the reference frequency weighting.
    const Profile* max_profile = &a;
    const Profile* min_profile = &b;
    if (a.size() < b.size()) {
      max_profile = &b;
      min_profile = &a;
    }
    std::vector<std::vector<double>> distances(
        max_profile->size(), std::vector<double>(min_profile->size(), 1.0));
    for (std::size_t i = 0; i < max_profile->size(); ++i) {
      for (std::size_t j = 0; j < min_profile->size(); ++j)
        distances[i][j] = one((*max_profile)[i], (*min_profile)[j]);
    }
    std::vector<bool> matched(min_profile->size(), false);
    for (std::size_t i = 0; i < max_profile->size(); ++i) {
      std::size_t best_index = 0;
      double best = distances[i].empty() ? 1.0 : distances[i][0];
      for (std::size_t j = 1; j < min_profile->size(); ++j) {
        if (distances[i][j] < best) {
          best = distances[i][j];
          best_index = j;
        }
      }
      if (min_profile->empty()) continue;
      const double pair_weight = static_cast<double>((*max_profile)[i].count) *
                                 static_cast<double>((*min_profile)[best_index].count);
      if (best <= 1e-8) {
        matched[best_index] = true;
        weight += pair_weight * 2.0;
        sum += best * pair_weight * 2.0;
      } else {
        weight += pair_weight;
        sum += best * pair_weight;
      }
    }
    for (std::size_t j = 0; j < min_profile->size(); ++j) {
      if (matched[j] || max_profile->empty()) continue;
      std::size_t best_index = 0;
      double best = distances[0][j];
      for (std::size_t i = 1; i < max_profile->size(); ++i) {
        if (distances[i][j] < best) {
          best = distances[i][j];
          best_index = i;
        }
      }
      const double pair_weight = static_cast<double>((*max_profile)[best_index].count) *
                                 static_cast<double>((*min_profile)[j].count);
      weight += pair_weight;
      sum += best * pair_weight;
    }
  } else {
    for (const auto& x : a) for (const auto& y : b) { sum += one(x,y)*x.count*y.count; weight += static_cast<double>(x.count)*y.count; }
  }
  return weight > 0 ? std::clamp(sum / weight, 0.0, 1.0) : 0.0;
}

struct Cluster { std::vector<std::size_t> members; std::string name; TraceAttributeClusteringNode node; };
}

TraceAttributeClusteringResult discover_trace_attribute_clustering(const EventLog& log, const TraceAttributeClusteringOptions& o) {
  if (o.trace_attribute.empty()) throw std::invalid_argument("clustering: trace_attribute must not be empty");
  if (!std::isfinite(o.alpha) || o.alpha < 0 || o.alpha > 1) throw std::invalid_argument("clustering: alpha must be in [0,1]");
  if (!std::isfinite(o.percent) || o.percent <= 0 || o.percent > 1) throw std::invalid_argument("clustering: percent must be in (0,1]");
  if (o.max_groups == 0 || o.max_groups > kHardGroups || o.max_output_nodes == 0 || o.max_output_nodes > kHardNodes) throw std::invalid_argument("clustering: safety bound exceeded");
  TraceAttributeClusteringResult r; r.diagnostics.trace_count = log.traces.size();
  std::map<std::string, std::vector<const Trace*>> groups;
  for (const auto& t : log.traces) {
    const std::string key = o.trace_attribute.rfind("case:", 0) == 0 ? o.trace_attribute.substr(5) : o.trace_attribute;
    const bool case_id_selector = key == "concept:name" || key == "id" ||
                                  key == "case:id" || key == "case_id";
    const auto* v = case_id_selector ? &t.case_id : t.attribute(o.trace_attribute);
    if (!v) v = t.attribute(key);
    if (!v && o.trace_attribute.rfind("case:", 0) != 0) v = t.attribute("case:" + o.trace_attribute);
    groups[v ? *v : std::string{}].push_back(&t);
  }
  if (groups.size() > o.max_groups) throw std::invalid_argument("clustering: group bound exceeded");
  r.diagnostics.group_count = groups.size();
  std::vector<std::string> names; std::vector<Profile> profiles;
  for (const auto& [name, ts] : groups) { names.push_back(name); profiles.push_back(profile(ts, o.percent)); r.leaf_names.push_back(name); }
  if (groups.empty()) {
    r.tree = {"root", {}, 0.0, 0, true};
    r.diagnostics.computed = true;
    r.diagnostics.message = "empty event log";
    return r;
  }
  std::vector<Cluster> cs; cs.reserve(groups.size());
  for (std::size_t i = 0; i < names.size(); ++i) { Cluster c; c.members = {i}; c.name = names[i]; c.node = {names[i], {}, 0.0, groups.at(names[i]).size(), true}; cs.push_back(std::move(c)); }
  // Keep average-linkage distances between active clusters. Updating the
  // merged row by the Lance-Williams weighted average avoids rescanning every
  // pair of original leaves on every merge.
  std::vector<std::vector<double>> d(
      names.size(), std::vector<double>(names.size(), 0.0));
  for (std::size_t i = 0; i < names.size(); ++i)
    for (std::size_t j = i + 1; j < names.size(); ++j)
      d[i][j] = d[j][i] =
          pair_distance(profiles[i], profiles[j], o.variant, o.alpha);
  std::size_t node_count = cs.size();
  while (cs.size() > 1) {
    std::size_t bi=0,bj=1; double bd=std::numeric_limits<double>::infinity();
    for (std::size_t i=0;i<cs.size();++i) for (std::size_t j=i+1;j<cs.size();++j) {
      const double x = d[i][j];
      if (x < bd - 1e-12 ||
          (std::abs(x - bd) <= 1e-12 &&
           std::pair{cs[i].name, cs[j].name} <
               std::pair{cs[bi].name, cs[bj].name})) {
        bd=x;bi=i;bj=j;
      }
    }
    const std::size_t old_size = cs.size();
    const double left_weight = static_cast<double>(cs[bi].members.size());
    const double right_weight = static_cast<double>(cs[bj].members.size());
    const double merged_weight = left_weight + right_weight;
    std::vector<double> merged_distances(old_size, 0.0);
    for (std::size_t k = 0; k < old_size; ++k) {
      if (k == bi || k == bj) continue;
      merged_distances[k] =
          (left_weight * d[bi][k] + right_weight * d[bj][k]) /
          merged_weight;
    }
    Cluster merged;
    merged.members = cs[bi].members;
    merged.members.insert(merged.members.end(), cs[bj].members.begin(),
                          cs[bj].members.end());
    std::sort(merged.members.begin(), merged.members.end());
    std::vector<std::string> member_names;
    member_names.reserve(merged.members.size());
    for (const auto member : merged.members) member_names.push_back(names[member]);
    std::sort(member_names.begin(), member_names.end());
    merged.name = std::accumulate(
        member_names.begin(), member_names.end(), std::string{},
        [](std::string joined, const std::string& name) {
          if (!joined.empty()) joined.push_back('-');
          joined += name;
          return joined;
        });
    merged.node.name = merged.name;
    merged.node.distance = bd;
    merged.node.size = cs[bi].node.size + cs[bj].node.size;
    merged.node.children = {std::move(cs[bi].node), std::move(cs[bj].node)};
    std::sort(merged.node.children.begin(), merged.node.children.end(),
              [](const auto& x, const auto& y) { return x.name < y.name; });
    merged.node.leaf = false;
    std::vector<std::size_t> survivors;
    survivors.reserve(old_size - 2);
    for (std::size_t k = 0; k < old_size; ++k)
      if (k != bi && k != bj) survivors.push_back(k);
    // Remove the larger index first so the surviving order remains stable.
    cs.erase(cs.begin() + static_cast<std::ptrdiff_t>(bj));
    cs.erase(cs.begin() + static_cast<std::ptrdiff_t>(bi));
    d.erase(d.begin() + static_cast<std::ptrdiff_t>(bj));
    d.erase(d.begin() + static_cast<std::ptrdiff_t>(bi));
    for (auto& row : d) {
      row.erase(row.begin() + static_cast<std::ptrdiff_t>(bj));
      row.erase(row.begin() + static_cast<std::ptrdiff_t>(bi));
    }
    cs.push_back(std::move(merged));
    const std::size_t merged_index = cs.size() - 1;
    for (auto& row : d) row.push_back(0.0);
    d.emplace_back(cs.size(), 0.0);
    for (std::size_t k = 0; k < survivors.size(); ++k) {
      const double value = merged_distances[survivors[k]];
      d[k][merged_index] = d[merged_index][k] = value;
    }
    if (++node_count > o.max_output_nodes) {
      r.diagnostics.output_truncated = true;
      throw std::invalid_argument("clustering: output node bound exceeded");
    }
  }
  r.tree = std::move(cs.front().node);
  if (!r.tree.leaf) r.tree.name = "root";
  r.diagnostics.node_count = node_count;
  r.diagnostics.computed = true;
  r.diagnostics.message = "ok";
  return r;
}

std::string trace_attribute_clustering_json(const TraceAttributeClusteringResult& r) {
  std::function<void(std::ostringstream&, const TraceAttributeClusteringNode&)> emit = [&](auto& o, const auto& n){ o << "{\"name\":" << json_quote(n.name) << ",\"distance\":" << std::setprecision(17) << n.distance << ",\"size\":" << n.size << ",\"leaf\":" << (n.leaf?"true":"false") << ",\"children\":["; for(std::size_t i=0;i<n.children.size();++i){if(i)o<<',';emit(o,n.children[i]);} o << "]}"; };
  std::ostringstream out;
  out << "{\"family\":\"TRACE_ATTRIBUTE_CLUSTERING\",\"tree\":";
  emit(out, r.tree);
  out << ",\"leaf_names\":[";
  for (std::size_t i = 0; i < r.leaf_names.size(); ++i) {
    if (i) out << ',';
    out << json_quote(r.leaf_names[i]);
  }
  out << "],\"diagnostics\":{\"computed\":"
      << (r.diagnostics.computed ? "true" : "false")
      << ",\"output_truncated\":"
      << (r.diagnostics.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << r.diagnostics.trace_count
      << ",\"group_count\":" << r.diagnostics.group_count
      << ",\"node_count\":" << r.diagnostics.node_count
      << ",\"message\":" << json_quote(r.diagnostics.message) << "}}";
  return out.str();
}
} // namespace minera
