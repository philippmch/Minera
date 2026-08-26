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
#include <string_view>
#include <unordered_map>
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
double cosine_distance(const Counts& a, const Counts& b,
                       bool empty_pair_is_distance = false) {
  if (a.empty() && b.empty()) return empty_pair_is_distance ? 1.0 : 0.0;
  double aa = 0, bb = 0, ab = 0;
  auto i = a.begin(), j = b.begin();
  while (i != a.end() || j != b.end()) {
    if (j == b.end() || (i != a.end() && i->first < j->first)) { aa += i->second*i->second; ++i; }
    else if (i == a.end() || j->first < i->first) { bb += j->second*j->second; ++j; }
    else { aa += i->second*i->second; bb += j->second*j->second; ab += i->second*j->second; ++i; ++j; }
  }
  if (aa == 0.0 && bb == 0.0)
    return empty_pair_is_distance ? 1.0 : 0.0;
  if (aa == 0.0 || bb == 0.0) return 1.0;
  const double sim = ab / std::sqrt(aa * bb);
  return std::clamp(1.0 - sim, 0.0, 1.0);
}

template <typename Distance>
double dmm_distance(const Profile& a, const Profile& b, Distance distance) {
  if (a.empty() || b.empty()) return a.empty() == b.empty() ? 0.0 : 1.0;

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
      distances[i][j] = distance((*max_profile)[i], (*min_profile)[j]);
  }

  std::vector<bool> matched(min_profile->size(), false);
  double sum = 0.0;
  double weight = 0.0;
  for (std::size_t i = 0; i < max_profile->size(); ++i) {
    std::size_t best_index = 0;
    double best = distances[i][0];
    for (std::size_t j = 1; j < min_profile->size(); ++j) {
      if (distances[i][j] < best) {
        best = distances[i][j];
        best_index = j;
      }
    }
    const double pair_weight =
        static_cast<double>((*max_profile)[i].count) *
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
    if (matched[j]) continue;
    std::size_t best_index = 0;
    double best = distances[0][j];
    for (std::size_t i = 1; i < max_profile->size(); ++i) {
      if (distances[i][j] < best) {
        best = distances[i][j];
        best_index = i;
      }
    }
    const double pair_weight =
        static_cast<double>((*max_profile)[best_index].count) *
        static_cast<double>((*min_profile)[j].count);
    weight += pair_weight;
    sum += best * pair_weight;
  }
  return weight > 0.0 ? std::clamp(sum / weight, 0.0, 1.0) : 0.0;
}

Profile profile(const std::vector<const Trace*>& traces, double percent) {
  // Variant statistics retain first-seen order for equal frequencies.  The
  // order is observable by the minimum-matching distance when two candidates
  // have the same distance, so a lexical map iteration would change the
  // clustering tree for otherwise equivalent logs.
  std::map<Sequence, std::size_t> indices;
  Profile p;
  p.reserve(traces.size());
  for (const auto* t : traces) {
    Sequence sequence;
    sequence.reserve(t->events.size());
    for (const auto& event : t->events) sequence.push_back(event.activity);
    const auto [it, inserted] = indices.emplace(sequence, p.size());
    if (inserted) {
      p.push_back({std::move(sequence), 1});
    } else {
      ++p[it->second].count;
    }
  }
  std::stable_sort(p.begin(), p.end(), [](const Variant& a, const Variant& b) {
    return a.count > b.count;
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
  auto vector_component = [&](const Variant& x, const Variant& y,
                              bool succession) {
    if (succession) {
      return cosine_distance(succession_counts(x.seq),
                              succession_counts(y.seq), true);
    }
    return cosine_distance(activity_counts(x.seq), activity_counts(y.seq));
  };
  auto one = [&](const Variant& x, const Variant& y) {
    if (variant == TraceAttributeClusteringVariant::kAvgLeven || variant == TraceAttributeClusteringVariant::kDmmLeven)
      return levenshtein(x.seq, y.seq);
    const auto act = vector_component(x, y, false);
    const auto suc = vector_component(x, y, true);
    return std::clamp(act * alpha + suc * (1.0 - alpha), 0.0, 1.0);
  };
  const bool dmm = variant == TraceAttributeClusteringVariant::kDmmLeven || variant == TraceAttributeClusteringVariant::kDmmVec;
  if (variant == TraceAttributeClusteringVariant::kDfg) {
    double act = 0, suc = 0; Counts ac, bc, as, bs;
    for (const auto& x : a) { for (const auto& [k,v] : activity_counts(x.seq)) ac[k] += v*x.count; for (const auto& [k,v] : succession_counts(x.seq)) as[k] += v*x.count; }
    for (const auto& x : b) { for (const auto& [k,v] : activity_counts(x.seq)) bc[k] += v*x.count; for (const auto& [k,v] : succession_counts(x.seq)) bs[k] += v*x.count; }
    act = cosine_distance(ac, bc); suc = cosine_distance(as, bs, true); return std::clamp(act*alpha + suc*(1-alpha),0.0,1.0);
  }
  if (variant == TraceAttributeClusteringVariant::kDmmVec) {
    const auto act = dmm_distance(a, b, [&](const Variant& x, const Variant& y) {
      return vector_component(x, y, false);
    });
    const auto suc = dmm_distance(a, b, [&](const Variant& x, const Variant& y) {
      return vector_component(x, y, true);
    });
    return std::clamp(act * alpha + suc * (1.0 - alpha), 0.0, 1.0);
  }
  if (dmm) {
    // The directional minimum-matching distance first matches every variant
    // on the larger side to its nearest variant on the smaller side. Exact
    // matches consume that smaller-side variant and receive a doubled weight;
    // only unmatched smaller-side variants are then matched in reverse. This
    // keeps duplicate exact variants from being counted as two independent
    // penalties while preserving the reference frequency weighting.
    return dmm_distance(a, b, one);
  } else {
    double sum = 0.0;
    double weight = 0.0;
    for (const auto& x : a) for (const auto& y : b) {
      sum += one(x,y)*x.count*y.count;
      weight += static_cast<double>(x.count)*y.count;
    }
    return weight > 0 ? std::clamp(sum / weight, 0.0, 1.0) : 0.0;
  }
}

struct Cluster { std::vector<std::size_t> members; std::string name; TraceAttributeClusteringNode node; };
}

TraceAttributeClusteringResult discover_trace_attribute_clustering(const EventLog& log, const TraceAttributeClusteringOptions& o) {
  if (o.trace_attribute.empty()) throw std::invalid_argument("clustering: trace_attribute must not be empty");
  if (!std::isfinite(o.alpha) || o.alpha < 0 || o.alpha > 1) throw std::invalid_argument("clustering: alpha must be in [0,1]");
  if (!std::isfinite(o.percent) || o.percent <= 0 || o.percent > 1) throw std::invalid_argument("clustering: percent must be in (0,1]");
  if (o.max_groups == 0 || o.max_groups > kHardGroups || o.max_output_nodes == 0 || o.max_output_nodes > kHardNodes) throw std::invalid_argument("clustering: safety bound exceeded");
  TraceAttributeClusteringResult r; r.diagnostics.trace_count = log.traces.size();
  // Attribute values are discovered in input order.  Keep a separate lookup
  // map so grouping remains linear without imposing lexical output order.
  std::map<std::string, std::size_t> group_indices;
  struct TraceGroup {
    std::string name;
    std::vector<const Trace*> traces;
  };
  std::vector<TraceGroup> groups;
  for (const auto& t : log.traces) {
    const std::string key = o.trace_attribute.rfind("case:", 0) == 0 ? o.trace_attribute.substr(5) : o.trace_attribute;
    const bool case_id_selector = key == "concept:name" || key == "id" ||
                                  key == "case:id" || key == "case_id";
    const auto* v = case_id_selector ? &t.case_id : t.attribute(o.trace_attribute);
    if (!v) v = t.attribute(key);
    if (!v && o.trace_attribute.rfind("case:", 0) != 0) v = t.attribute("case:" + o.trace_attribute);
    if (!v) continue;
    const std::string name = *v;
    const auto [it, inserted] = group_indices.emplace(name, groups.size());
    if (inserted) groups.push_back({name, {}});
    groups[it->second].traces.push_back(&t);
  }
  if (groups.size() > o.max_groups) throw std::invalid_argument("clustering: group bound exceeded");
  r.diagnostics.group_count = groups.size();
  std::vector<std::string> names; std::vector<Profile> profiles;
  for (const auto& group : groups) {
    names.push_back(group.name);
    // The DFG distance is defined over each complete group sublog.  Its
    // reference evaluator accepts the percentage setting for API symmetry,
    // but does not use it to discard trace variants.  Keep the sampling
    // control for the other distance families only.
    profiles.push_back(profile(
        group.traces, o.variant == TraceAttributeClusteringVariant::kDfg
                          ? 1.0
                          : o.percent));
    r.leaf_names.push_back(group.name);
  }
  if (groups.empty()) {
    r.tree = {"root", {}, 0.0, 0, true};
    r.diagnostics.computed = true;
    r.diagnostics.message = "empty event log";
    return r;
  }
  std::vector<Cluster> cs; cs.reserve(groups.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    Cluster c;
    c.members = {i};
    c.name = names[i];
    c.node = {names[i], {}, 0.0, groups[i].traces.size(), true};
    cs.push_back(std::move(c));
  }
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
      if (x < bd) {
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

namespace {

const EventAttribute* profile_event_attribute(const Event& event,
                                              const std::string& key) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return &attribute;
  return nullptr;
}

const EventAttribute* profile_trace_attribute(const Trace& trace,
                                              const std::string& key) {
  for (const auto& attribute : trace.attributes)
    if (attribute.key == key) return &attribute;
  return nullptr;
}

bool profile_numeric_kind(AttributeKind kind) {
  return kind == AttributeKind::kInt || kind == AttributeKind::kFloat ||
         kind == AttributeKind::kBoolean;
}

double profile_number(const std::string& value, const std::string& key) {
  try {
    std::size_t end = 0;
    const double number = std::stod(value, &end);
    if (end != value.size() || !std::isfinite(number)) throw std::invalid_argument("bad");
    return number;
  } catch (...) {
    if (value == "true" || value == "TRUE" || value == "True") return 1.0;
    if (value == "false" || value == "FALSE" || value == "False") return 0.0;
    throw std::invalid_argument("profile clustering: invalid numeric attribute " + key);
  }
}

void validate_profile_names(const std::vector<std::string>& names,
                            const char* kind) {
  std::set<std::string> seen;
  for (const auto& name : names) {
    if (name.empty() || !seen.insert(name).second)
      throw std::invalid_argument(std::string("profile clustering: invalid ") + kind);
  }
}

struct ProfileRows {
  std::vector<std::string> names;
  std::vector<std::vector<double>> rows;
};

ProfileRows profile_rows(const EventLog& log,
                         const ProfileClusteringOptions& options) {
  std::set<std::string> trace_keys, event_keys;
  std::map<std::string, AttributeKind> trace_kinds, event_kinds;
  for (const auto& trace : log.traces) {
    for (const auto& attribute : trace.attributes) {
      trace_keys.insert(attribute.key);
      trace_kinds.emplace(attribute.key, attribute.kind);
    }
    for (const auto& event : trace.events) {
      for (const auto& attribute : event.attributes) {
        event_keys.insert(attribute.key);
        event_kinds.emplace(attribute.key, attribute.kind);
      }
    }
  }
  const auto complete_trace = [&](const std::string& key) {
    return std::all_of(log.traces.begin(), log.traces.end(), [&](const auto& trace) {
      return profile_trace_attribute(trace, key) != nullptr;
    });
  };
  const auto complete_event = [&](const std::string& key) {
    return std::all_of(log.traces.begin(), log.traces.end(), [&](const auto& trace) {
      return std::any_of(trace.events.begin(), trace.events.end(), [&](const auto& event) {
        return profile_event_attribute(event, key) != nullptr;
      });
    });
  };
  const auto distinct_trace = [&](const std::string& key) {
    std::set<std::string> values;
    for (const auto& trace : log.traces) {
      const auto* attribute = profile_trace_attribute(trace, key);
      values.insert(attribute ? attribute->value : "UNDEFINED");
    }
    return values;
  };
  const auto distinct_event = [&](const std::string& key) {
    std::set<std::string> values;
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        if (const auto* attribute = profile_event_attribute(event, key))
          values.insert(attribute->value);
    return values;
  };

  auto trace_strings = options.trace_string_attributes;
  auto event_strings = options.event_string_attributes;
  auto trace_numbers = options.trace_numeric_attributes;
  auto event_numbers = options.event_numeric_attributes;
  if (trace_strings.empty() && trace_numbers.empty()) {
    for (const auto& key : trace_keys) {
      if (!complete_trace(key)) continue;
      const auto kind = trace_kinds.at(key);
      if (!profile_numeric_kind(kind) && distinct_trace(key).size() < 12.5)
        trace_strings.push_back(key);
    }
    for (const auto& key : trace_keys) {
      if (!complete_trace(key)) continue;
      if (profile_numeric_kind(trace_kinds.at(key))) trace_numbers.push_back(key);
    }
  }
  if (event_strings.empty() && event_numbers.empty()) {
    for (const auto& key : event_keys) {
      if (!complete_event(key)) continue;
      const auto kind = event_kinds.at(key);
      if (!profile_numeric_kind(kind) && distinct_event(key).size() < 12.5)
        event_strings.push_back(key);
    }
    for (const auto& key : event_keys) {
      if (!complete_event(key)) continue;
      if (profile_numeric_kind(event_kinds.at(key))) event_numbers.push_back(key);
    }
  }
  std::sort(trace_strings.begin(), trace_strings.end());
  std::sort(event_strings.begin(), event_strings.end());
  std::sort(trace_numbers.begin(), trace_numbers.end());
  std::sort(event_numbers.begin(), event_numbers.end());
  validate_profile_names(trace_strings, "trace string attributes");
  validate_profile_names(event_strings, "event string attributes");
  validate_profile_names(trace_numbers, "trace numeric attributes");
  validate_profile_names(event_numbers, "event numeric attributes");

  std::set<std::string> names;
  if (options.include_activity_representation) {
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        names.insert("event:concept:name@" + event.activity);
    if (std::any_of(log.traces.begin(), log.traces.end(),
                    [](const auto& trace) { return trace.events.empty(); }))
      names.insert("event:concept:name@UNDEFINED");
  }
  for (const auto& key : trace_strings)
    for (const auto& value : distinct_trace(key))
      names.insert("trace:" + key + "@" + value);
  for (const auto& key : event_strings) {
    for (const auto& value : distinct_event(key))
      names.insert("event:" + key + "@" + value);
    if (std::any_of(log.traces.begin(), log.traces.end(), [&](const auto& trace) {
          return std::none_of(trace.events.begin(), trace.events.end(), [&](const auto& event) {
            return profile_event_attribute(event, key) != nullptr;
          });
        }))
      names.insert("event:" + key + "@UNDEFINED");
  }
  std::vector<std::string> ordered_names;
  for (const auto& key : trace_strings)
    for (const auto& value : distinct_trace(key))
      ordered_names.push_back("trace:" + key + "@" + value);
  for (const auto& key : event_strings)
    for (const auto& value : distinct_event(key))
      ordered_names.push_back("event:" + key + "@" + value);
  for (const auto& key : trace_numbers) ordered_names.push_back("trace:" + key);
  for (const auto& key : event_numbers) ordered_names.push_back("event:" + key);
  if (options.include_activity_representation) {
    std::set<std::string> activity_names;
    for (const auto& name : names)
      if (name.rfind("event:concept:name@", 0) == 0) activity_names.insert(name);
    const auto numeric_begin = ordered_names.begin() +
                               static_cast<std::ptrdiff_t>(
                                   ordered_names.size() - trace_numbers.size() -
                                   event_numbers.size());
    ordered_names.insert(numeric_begin, activity_names.begin(), activity_names.end());
  }
  if (options.include_succession_representation) {
    std::set<std::string> successions;
    for (const auto& trace : log.traces) {
      if (trace.events.size() < 2) {
        successions.insert("succession:concept:name@UNDEFINED");
        continue;
      }
      for (std::size_t i = 1; i < trace.events.size(); ++i)
        successions.insert("succession:concept:name@" + trace.events[i - 1].activity +
                           "#" + trace.events[i].activity);
    }
    ordered_names.insert(ordered_names.end(), successions.begin(), successions.end());
  }
  if (!options.feature_names.empty()) {
    validate_profile_names(options.feature_names, "feature names");
    ordered_names = options.feature_names;
  }
  if (ordered_names.size() > options.max_features)
    throw std::invalid_argument("profile clustering: feature bound exceeded");
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < ordered_names.size(); ++i) index[ordered_names[i]] = i;
  ProfileRows result;
  result.names = ordered_names;
  result.rows.reserve(log.traces.size());
  const auto set = [&](std::vector<double>& row, const std::string& name, double value) {
    const auto it = index.find(name);
    if (it != index.end()) row[it->second] = value;
  };
  for (const auto& trace : log.traces) {
    std::vector<double> row(ordered_names.size(), 0.0);
    for (const auto& key : trace_strings) {
      const auto* attribute = profile_trace_attribute(trace, key);
      set(row, "trace:" + key + "@" + (attribute ? attribute->value : "UNDEFINED"), 1.0);
    }
    for (const auto& key : event_strings) {
      bool found = false;
      for (const auto& event : trace.events)
        if (const auto* attribute = profile_event_attribute(event, key)) {
          set(row, "event:" + key + "@" + attribute->value, 1.0);
          found = true;
        }
      if (!found) set(row, "event:" + key + "@UNDEFINED", 1.0);
    }
    for (const auto& key : trace_numbers) {
      const auto* attribute = profile_trace_attribute(trace, key);
      if (!attribute) throw std::invalid_argument("profile clustering: missing numeric trace attribute " + key);
      set(row, "trace:" + key, profile_number(attribute->value, key));
    }
    for (const auto& key : event_numbers) {
      const EventAttribute* last = nullptr;
      for (const auto& event : trace.events)
        if (const auto* attribute = profile_event_attribute(event, key)) last = attribute;
      if (!last) throw std::invalid_argument("profile clustering: missing numeric event attribute " + key);
      set(row, "event:" + key, profile_number(last->value, key));
    }
    if (options.include_activity_representation) {
      if (trace.events.empty()) set(row, "event:concept:name@UNDEFINED", 1.0);
      for (const auto& event : trace.events)
        set(row, "event:concept:name@" + event.activity, 1.0);
    }
    if (options.include_succession_representation) {
      if (trace.events.size() < 2) set(row, "succession:concept:name@UNDEFINED", 1.0);
      for (std::size_t i = 1; i < trace.events.size(); ++i)
        set(row, "succession:concept:name@" + trace.events[i - 1].activity +
                     "#" + trace.events[i].activity, 1.0);
    }
    result.rows.push_back(std::move(row));
  }
  return result;
}

double profile_squared_distance(const std::vector<double>& left,
                                const std::vector<double>& right) {
  double result = 0.0;
  for (std::size_t i = 0; i < left.size(); ++i) {
    const double difference = left[i] - right[i];
    result += difference * difference;
  }
  return result;
}

}  // namespace

ProfileClusteringResult discover_profile_clustering(
    const EventLog& log, const ProfileClusteringOptions& options) {
  if (options.variant != ProfileClusteringVariant::kSklearnProfiles)
    throw std::invalid_argument("profile clustering: unsupported variant");
  if (options.cluster_count == 0 || options.max_iterations == 0 ||
      options.max_traces == 0 || options.max_features == 0)
    throw std::invalid_argument("profile clustering: bounds must be positive");
  if (!std::isfinite(options.tolerance) || options.tolerance < 0.0)
    throw std::invalid_argument("profile clustering: tolerance must be finite and non-negative");
  if (log.traces.size() > options.max_traces)
    throw std::invalid_argument("profile clustering: trace bound exceeded");
  ProfileClusteringResult result;
  result.variant = options.variant;
  result.diagnostics.trace_count = log.traces.size();
  if (log.traces.empty()) {
    result.diagnostics.computed = true;
    result.diagnostics.message = "empty event log";
    return result;
  }
  if (options.cluster_count > log.traces.size())
    throw std::invalid_argument("profile clustering: cluster_count exceeds trace count");
  const auto features = profile_rows(log, options);
  result.feature_names = features.names;
  result.diagnostics.feature_count = features.names.size();
  result.diagnostics.cluster_count = options.cluster_count;
  if (features.names.empty())
    throw std::invalid_argument("profile clustering: feature representation is empty");
  const std::size_t k = options.cluster_count;
  result.centroids.assign(k, std::vector<double>(features.names.size(), 0.0));
  const std::size_t first = static_cast<std::size_t>(options.seed % features.rows.size());
  std::vector<std::size_t> centers(k);
  centers[0] = first;
  for (std::size_t c = 1; c < k; ++c) {
    double best_distance = -1.0;
    std::size_t best_index = 0;
    for (std::size_t row = 0; row < features.rows.size(); ++row) {
      double nearest = std::numeric_limits<double>::infinity();
      for (std::size_t previous = 0; previous < c; ++previous)
        nearest = std::min(nearest, profile_squared_distance(features.rows[row], features.rows[centers[previous]]));
      if (nearest > best_distance + 1e-12 ||
          (std::abs(nearest - best_distance) <= 1e-12 && row < best_index)) {
        best_distance = nearest;
        best_index = row;
      }
    }
    centers[c] = best_index;
  }
  for (std::size_t c = 0; c < k; ++c) result.centroids[c] = features.rows[centers[c]];
  result.labels.assign(features.rows.size(), 0);
  for (std::size_t iteration = 0; iteration < options.max_iterations; ++iteration) {
    bool labels_changed = false;
    for (std::size_t row = 0; row < features.rows.size(); ++row) {
      int best = 0;
      double distance = profile_squared_distance(features.rows[row], result.centroids[0]);
      for (std::size_t c = 1; c < k; ++c) {
        const double candidate = profile_squared_distance(features.rows[row], result.centroids[c]);
        if (candidate < distance - 1e-12) {
          distance = candidate;
          best = static_cast<int>(c);
        }
      }
      if (result.labels[row] != best) labels_changed = true;
      result.labels[row] = best;
    }
    std::vector<std::vector<double>> sums(k, std::vector<double>(features.names.size(), 0.0));
    std::vector<std::size_t> counts(k, 0);
    for (std::size_t row = 0; row < features.rows.size(); ++row) {
      const std::size_t cluster = static_cast<std::size_t>(result.labels[row]);
      ++counts[cluster];
      for (std::size_t feature = 0; feature < features.names.size(); ++feature)
        sums[cluster][feature] += features.rows[row][feature];
    }
    double shift = 0.0;
    for (std::size_t c = 0; c < k; ++c) {
      if (counts[c] == 0) continue;
      for (std::size_t feature = 0; feature < features.names.size(); ++feature)
        sums[c][feature] /= static_cast<double>(counts[c]);
      shift = std::max(shift, profile_squared_distance(result.centroids[c], sums[c]));
    }
    result.centroids = std::move(sums);
    result.diagnostics.iterations = iteration + 1;
    if (!labels_changed || shift <= options.tolerance * options.tolerance) {
      result.diagnostics.converged = true;
      break;
    }
  }
  result.clusters.clear();
  result.clusters.resize(k);
  for (std::size_t row = 0; row < result.labels.size(); ++row) {
    result.clusters[static_cast<std::size_t>(result.labels[row])].push_back(row);
  }
  result.inertia = 0.0;
  for (std::size_t row = 0; row < features.rows.size(); ++row)
    result.inertia += profile_squared_distance(
        features.rows[row], result.centroids[static_cast<std::size_t>(result.labels[row])]);
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.converged ? "converged" : "iteration limit";
  return result;
}

std::string profile_clustering_json(const ProfileClusteringResult& result) {
  std::ostringstream out;
  const std::string variant =
      result.variant == ProfileClusteringVariant::kSklearnProfiles
          ? "sklearn_profiles"
          : "unknown";
  out << "{\"family\":\"PROFILE_CLUSTERING\",\"variant\":";
  out << json_quote(variant) << ",\"labels\":[";
  for (std::size_t i = 0; i < result.labels.size(); ++i) {
    if (i != 0) out << ',';
    out << result.labels[i];
  }
  out << "],\"clusters\":[";
  for (std::size_t cluster = 0; cluster < result.clusters.size(); ++cluster) {
    if (cluster != 0) out << ',';
    out << '[';
    for (std::size_t i = 0; i < result.clusters[cluster].size(); ++i) {
      if (i != 0) out << ',';
      out << result.clusters[cluster][i];
    }
    out << ']';
  }
  out << "],\"feature_names\":[";
  for (std::size_t i = 0; i < result.feature_names.size(); ++i) {
    if (i != 0) out << ',';
    out << json_quote(result.feature_names[i]);
  }
  out << "],\"inertia\":" << std::setprecision(17) << result.inertia
      << ",\"diagnostics\":{\"computed\":"
      << (result.diagnostics.computed ? "true" : "false")
      << ",\"converged\":" << (result.diagnostics.converged ? "true" : "false")
      << ",\"trace_count\":" << result.diagnostics.trace_count
      << ",\"feature_count\":" << result.diagnostics.feature_count
      << ",\"cluster_count\":" << result.diagnostics.cluster_count
      << ",\"iterations\":" << result.diagnostics.iterations
      << ",\"message\":" << json_quote(result.diagnostics.message) << "}}";
  return out.str();
}
} // namespace minera
