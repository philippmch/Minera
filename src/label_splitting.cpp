#include "minera/label_splitting.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

using Context = std::vector<std::string>;
using EncodedContext = std::vector<std::size_t>;

std::string activity_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

void set_activity_value(Event& event, const std::string& key,
                        const std::string& value) {
  if (key.empty() || key == "activity" || key == "concept:name") {
    event.activity = value;
    return;
  }
  for (auto& attribute : event.attributes) {
    if (attribute.key == key) {
      attribute.value = value;
      attribute.kind = AttributeKind::kString;
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
};

std::vector<Community> cluster_segments(const std::vector<SegmentRef>& nodes,
                                         double min_edge_weight) {
  const std::size_t n = nodes.size();
  std::vector<double> degree(n, 0.0);
  std::map<std::pair<std::size_t, std::size_t>, double> edges;
  double total_weight = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      const double weight = context_similarity(nodes[i].encoded_context,
                                               nodes[j].encoded_context);
      if (!(weight > min_edge_weight)) continue;
      edges[{i, j}] = weight;
      degree[i] += weight;
      degree[j] += weight;
      total_weight += weight;
    }
  }
  std::vector<Community> communities;
  communities.reserve(n);
  for (std::size_t i = 0; i < n; ++i) communities.push_back({{i}});
  if (total_weight <= 0.0) return communities;

  while (communities.size() > 1) {
    double best_gain = 0.0;
    std::size_t best_left = n;
    std::size_t best_right = n;
    for (std::size_t a = 0; a < communities.size(); ++a) {
      double total_a = 0.0;
      for (const auto node : communities[a].nodes) total_a += degree[node];
      for (std::size_t b = a + 1; b < communities.size(); ++b) {
        double total_b = 0.0;
        for (const auto node : communities[b].nodes) total_b += degree[node];
        double between = 0.0;
        for (const auto left : communities[a].nodes) {
          for (const auto right : communities[b].nodes) {
            const auto edge = std::minmax(left, right);
            if (const auto it = edges.find(edge); it != edges.end())
              between += it->second;
          }
        }
        const double gain = between / total_weight -
                            (total_a * total_b) /
                                (2.0 * total_weight * total_weight);
        const auto left_key = communities[a].nodes.front();
        const auto right_key = communities[b].nodes.front();
        if (gain > best_gain + 1e-12 ||
            (std::abs(gain - best_gain) <= 1e-12 &&
             std::pair{left_key, right_key} <
                 std::pair{best_left, best_right})) {
          best_gain = gain;
          best_left = a;
          best_right = b;
        }
      }
    }
    if (best_left == n || !(best_gain > 1e-12)) break;
    auto& destination = communities[best_left].nodes;
    destination.insert(destination.end(), communities[best_right].nodes.begin(),
                       communities[best_right].nodes.end());
    std::sort(destination.begin(), destination.end());
    communities.erase(communities.begin() +
                       static_cast<std::ptrdiff_t>(best_right));
  }
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
      auto& collection = segments[label];
      const auto [found, inserted] = collection.indices.emplace(
          context, collection.nodes.size());
      if (inserted) {
        SegmentRef segment;
        segment.context = context;
        segment.encoded_context = encode_context(context);
        collection.nodes.push_back(std::move(segment));
      }
      collection.nodes[found->second].events.emplace_back(trace_index,
                                                           event_index);
    }
  }

  for (auto& [label, collection] : segments) {
    if (!targets.empty() && targets.count(label) == 0) continue;
    auto& nodes = collection.nodes;
    if (nodes.size() < 2) continue;
    auto communities = cluster_segments(nodes, options.min_edge_weight);
    std::sort(communities.begin(), communities.end(), [&](const auto& left, const auto& right) {
      std::size_t left_size = 0, right_size = 0;
      for (const auto node : left.nodes) left_size += nodes[node].events.size();
      for (const auto node : right.nodes) right_size += nodes[node].events.size();
      if (left_size != right_size) return left_size > right_size;
      return left.nodes > right.nodes;
    });
    if (communities.size() < 2) continue;
    for (std::size_t community_index = 0; community_index < communities.size(); ++community_index) {
      const auto replacement = label + options.activities_suffix + std::to_string(community_index);
      for (const auto node : communities[community_index].nodes) {
        for (const auto [trace_index, event_index] : nodes[node].events)
          set_activity_value(result.log.traces[trace_index].events[event_index],
                             options.target_key, replacement);
      }
    }
  }
  return result;
}

}  // namespace minera
