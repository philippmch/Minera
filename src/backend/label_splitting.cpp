#include "minera/label_splitting.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace minera {
namespace {

using Context = std::vector<std::string>;
using EncodedContext = std::vector<std::size_t>;
using NodeKey = std::pair<std::string, Context>;

std::string activity_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

void set_activity_value(Event& event, const std::string& key,
                        const std::string& value) {
  if (key.empty() || key == "activity" || key == "concept:name" ||
      (!event.activity_source_key.empty() && key == event.activity_source_key)) {
    const auto source = event;
    set_event_activity_context(source, value, event);
    return;
  }
  for (auto& attribute : event.attributes) {
    if (attribute.key == key) {
      // A label replaces the selected value, including its old typed payload.
      attribute = EventAttribute{key, value, AttributeKind::kString};
      return;
    }
  }
  event.attributes.push_back({key, value, AttributeKind::kString});
}

std::size_t edit_distance(const EncodedContext& left,
                          const EncodedContext& right) {
  std::vector<std::size_t> previous(right.size() + 1);
  std::vector<std::size_t> current(right.size() + 1);
  for (std::size_t j = 0; j <= right.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= left.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= right.size(); ++j) {
      const std::size_t substitution =
          previous[j - 1] + (left[i - 1] == right[j - 1] ? 0 : 1);
      current[j] = std::min({previous[j] + 1, current[j - 1] + 1,
                             substitution});
    }
    previous.swap(current);
  }
  return previous[right.size()];
}

double context_similarity(const EncodedContext& left,
                          const EncodedContext& right) {
  const auto width = std::max(left.size(), right.size());
  if (width == 0) return 1.0;
  return 1.0 - static_cast<double>(edit_distance(left, right)) /
                     static_cast<double>(width);
}

struct SegmentRef {
  std::string activity;
  Context context;
  EncodedContext encoded_context;
  std::vector<std::pair<std::size_t, std::size_t>> events;
};

struct SegmentCollection {
  // The vector preserves first-seen context order. The lookup map only
  // identifies an existing segment and never defines its output order.
  std::vector<SegmentRef> nodes;
  std::map<Context, std::size_t> indices;
};

struct Community {
  std::vector<std::size_t> nodes;
  // The graph implementation returns communities sorted by node count while
  // preserving the graph's insertion order for equal-sized communities.
  // Keep that order explicit so the later event-frequency ordering can use
  // the same deterministic tie break.
  std::size_t output_order = 0;
};

std::vector<Community> cluster_segments(const std::vector<SegmentRef>& nodes,
                                         double min_edge_weight) {
  const std::size_t n = nodes.size();
  std::vector<double> degree(n, 0.0);
  std::vector<std::vector<std::pair<std::size_t, double>>> adjacency(n);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      if (nodes[i].activity != nodes[j].activity) continue;
      const double weight = context_similarity(nodes[i].encoded_context,
                                               nodes[j].encoded_context);
      if (!(weight > min_edge_weight)) continue;
      adjacency[i].push_back({j, weight});
      adjacency[j].push_back({i, weight});
    }
  }
  // Degrees and the total weight are summed in graph adjacency order, as the
  // reference graph library does; the greedy merge order depends on the
  // exact floating-point values.
  for (std::size_t i = 0; i < n; ++i)
    for (const auto& [neighbour, weight] : adjacency[i]) degree[i] += weight;
  double degree_sum = 0.0;
  for (const auto value : degree) degree_sum += value;
  const double total_weight = degree_sum / 2.0;
  std::vector<Community> communities;
  communities.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    communities.push_back({{i}, i});
  if (total_weight <= 0.0) return communities;

  // Clauset-Newman-Moore greedy modularity with incrementally updated
  // gains: dq[u][v] starts at 2*q0*w - 2*a_u*a_v (q0 = 1/(2m), a = k*q0).
  // Merging u into v sets dq[v][w] to dq[u][w] + dq[v][w] for common
  // neighbours and subtracts 2*a_x*a_w for a neighbour of only one side x.
  const double q0 = 1.0 / (2.0 * total_weight);
  std::vector<double> a(n);
  for (std::size_t i = 0; i < n; ++i) a[i] = degree[i] * q0;
  std::vector<std::map<std::size_t, double>> dq(n);
  for (std::size_t i = 0; i < n; ++i)
    for (const auto& [neighbour, weight] : adjacency[i])
      dq[i][neighbour] = 2 * q0 * weight - 2 * a[i] * a[neighbour];
  std::vector<bool> alive(n, true);
  std::vector<NodeKey> labels;
  labels.reserve(n);
  for (const auto& node : nodes) labels.push_back({node.activity, node.context});

  while (true) {
    // The reference heap pops the largest gain and breaks ties by the
    // smallest ordered pair of node identifiers, (activity, context).
    bool found = false;
    double best_gain = 0.0;
    std::size_t best_u = n, best_v = n;
    for (std::size_t u = 0; u < n; ++u) {
      if (!alive[u]) continue;
      for (const auto& [v, gain] : dq[u]) {
        if (!found || gain > best_gain ||
            (gain == best_gain &&
             std::tie(labels[u], labels[v]) <
                 std::tie(labels[best_u], labels[best_v]))) {
          found = true;
          best_gain = gain;
          best_u = u;
          best_v = v;
        }
      }
    }
    // Zero-gain merges are accepted; a negative best gain stops.
    if (!found || best_gain < 0.0) break;
    const auto u = best_u;
    const auto v = best_v;
    std::set<std::size_t> neighbours;
    for (const auto& entry : dq[u]) neighbours.insert(entry.first);
    for (const auto& entry : dq[v]) neighbours.insert(entry.first);
    neighbours.erase(u);
    neighbours.erase(v);
    for (const auto w : neighbours) {
      const auto from_u = dq[u].find(w);
      const auto from_v = dq[v].find(w);
      double updated;
      if (from_u != dq[u].end() && from_v != dq[v].end())
        updated = from_v->second + from_u->second;
      else if (from_u != dq[u].end())
        updated = from_u->second - 2 * a[v] * a[w];
      else
        updated = from_v->second - 2 * a[u] * a[w];
      dq[v][w] = updated;
      dq[w][v] = updated;
      dq[w].erase(u);
    }
    dq[v].erase(u);
    dq[u].clear();
    alive[u] = false;
    a[v] = a[v] + a[u];
    // Communities are indexed by their surviving node.
    auto& members = communities[v].nodes;
    members.insert(members.end(), communities[u].nodes.begin(),
                   communities[u].nodes.end());
    std::sort(members.begin(), members.end());
    communities[u].nodes.clear();
  }
  std::erase_if(communities, [](const Community& community) {
    return community.nodes.empty();
  });
  // The Python graph routine returns largest communities first and preserves
  // insertion order for equal cardinalities. The vector remains in insertion
  // order because merges empty only the consumed community.
  std::stable_sort(communities.begin(), communities.end(),
                   [](const Community& left, const Community& right) {
                     return left.nodes.size() > right.nodes.size();
                   });
  for (std::size_t i = 0; i < communities.size(); ++i)
    communities[i].output_order = i;
  return communities;
}

}  // namespace

LabelSplittingResult split_activity_labels(const EventLog& log,
                                           const LabelSplittingOptions& options) {
  if (options.variant != LabelSplittingVariant::CONTEXTUAL)
    throw std::invalid_argument("label_splitting: unknown variant");
  if (options.activity_key.empty() || options.target_key.empty())
    throw std::invalid_argument("label_splitting: keys must not be empty");
  if (!std::isfinite(options.min_edge_weight))
    throw std::invalid_argument("label_splitting: min_edge_weight must be finite");
  if (options.prefix_length > 4096 || options.suffix_length > 4096)
    throw std::invalid_argument("label_splitting: context lengths are too large");
  std::set<std::string> targets(options.target_activities.begin(),
                                options.target_activities.end());
  if (targets.size() != options.target_activities.size())
    throw std::invalid_argument("label_splitting: duplicate target activity");

  LabelSplittingResult result;
  result.variant = options.variant;
  result.log = log;
  std::map<std::string, SegmentCollection> segments;
  std::vector<std::string> activity_order;
  std::map<std::string, std::size_t> context_tokens;
  const auto encode_context = [&](const Context& context) {
    EncodedContext encoded;
    encoded.reserve(context.size());
    for (const auto& token : context) {
      auto it = context_tokens.find(token);
      if (it == context_tokens.end()) {
        const auto next_id = context_tokens.size();
        it = context_tokens.emplace(token, next_id).first;
      }
      encoded.push_back(it->second);
    }
    return encoded;
  };
  for (std::size_t trace_index = 0; trace_index < log.traces.size(); ++trace_index) {
    const auto& events = log.traces[trace_index].events;
    std::vector<std::string> labels;
    labels.reserve(events.size());
    for (const auto& event : events) labels.push_back(activity_value(event, options.activity_key));
    for (std::size_t event_index = 0; event_index < events.size(); ++event_index) {
      const auto& label = labels[event_index];
      if (!targets.empty() && targets.count(label) == 0) continue;
      Context context;
      const auto prefix_begin = event_index > options.prefix_length
                                    ? event_index - options.prefix_length
                                    : 0;
      for (std::size_t i = prefix_begin; i < event_index; ++i) context.push_back(labels[i]);
      const auto suffix_end = std::min(labels.size(), event_index + options.suffix_length + 1);
      for (std::size_t i = event_index + 1; i < suffix_end; ++i) context.push_back(labels[i]);
      auto [collection_it, inserted_activity] = segments.try_emplace(label);
      if (inserted_activity) activity_order.push_back(label);
      auto& collection = collection_it->second;
      const auto [found, inserted] = collection.indices.emplace(
          context, collection.nodes.size());
      if (inserted) {
        SegmentRef segment;
        segment.activity = label;
        segment.context = context;
        segment.encoded_context = encode_context(context);
        collection.nodes.push_back(std::move(segment));
      }
      collection.nodes[found->second].events.emplace_back(trace_index,
                                                           event_index);
    }
  }

  // The modularity optimizer sees one graph containing every selected
  // activity. Although the graph has no cross-activity edges, its global
  // edge-weight total is part of every expected-edge term.
  std::vector<SegmentRef> all_nodes;
  for (const auto& label : activity_order) {
    const auto it = segments.find(label);
    if (it == segments.end()) continue;
    all_nodes.insert(all_nodes.end(), it->second.nodes.begin(),
                     it->second.nodes.end());
  }
  const auto all_communities =
      cluster_segments(all_nodes, options.min_edge_weight);
  std::map<std::string, std::vector<Community>> communities_by_activity;
  for (const auto& community : all_communities) {
    std::map<std::string, Community> pieces;
    for (const auto node : community.nodes) {
      auto& piece = pieces[all_nodes[node].activity];
      piece.output_order = community.output_order;
      piece.nodes.push_back(node);
    }
    for (auto& [label, piece] : pieces)
      communities_by_activity[label].push_back(std::move(piece));
  }
  // A separate target column starts as a copy of the activity column, so
  // events that are not relabeled keep their original label there.
  const auto canonical = [](const std::string& key) {
    return key == "concept:name" ? std::string{"activity"} : key;
  };
  if (canonical(options.target_key) != canonical(options.activity_key))
    for (auto& trace : result.log.traces)
      for (auto& event : trace.events)
        set_activity_value(event, options.target_key,
                           activity_value(event, options.activity_key));
  for (const auto& label : activity_order) {
    auto community_it = communities_by_activity.find(label);
    if (community_it == communities_by_activity.end()) continue;
    auto& communities = community_it->second;
    std::sort(communities.begin(), communities.end(), [&](const auto& left, const auto& right) {
      std::size_t left_size = 0, right_size = 0;
      for (const auto node : left.nodes) left_size += all_nodes[node].events.size();
      for (const auto node : right.nodes) right_size += all_nodes[node].events.size();
      if (left_size != right_size) return left_size > right_size;
      return left.output_order > right.output_order;
    });
    if (communities.size() < 2) continue;
    for (std::size_t community_index = 0; community_index < communities.size(); ++community_index) {
      const auto replacement = label + options.activities_suffix + std::to_string(community_index);
      for (const auto node : communities[community_index].nodes) {
        for (const auto [trace_index, event_index] : all_nodes[node].events)
          set_activity_value(result.log.traces[trace_index].events[event_index],
                             options.target_key, replacement);
      }
    }
  }
  return result;
}

}  // namespace minera
