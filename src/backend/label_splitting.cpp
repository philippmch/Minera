#include "minera/label_splitting.hpp"

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "frame_cells.hpp"
#include "python_scalar.hpp"

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

// A label as the reference's Python code holds it (round CR). An upload's
// frame column gives typed values (ints, floats, bools, Timestamps) and its
// missing cells as pandas holds them: NaT in a date column, a NaN float that
// is a new object per cell in a float64 column, and the one NaN object in
// any other column. `id` is the dict identity: two labels with equal ids
// are the same dict key and pass `a is b or a == b`.
struct Token {
  python::Scalar value;
  std::string id;
  bool nat = false;
};

Token text_token(std::string text) {
  Token token;
  token.id = "S" + text;
  token.value = python::text(std::move(text));
  return token;
}

bool same(const Token& left, const Token& right) { return left.id == right.id; }

// Python's left < right for two labels that are not the same; raises
// TypeError where Python does (NaT orders with no date and with nothing
// else either way).
bool token_less(const Token& left, const Token& right) {
  if (left.nat || right.nat) {
    const auto date = [](const Token& t) { return t.nat || t.value.kind == python::Scalar::Kind::kDate; };
    if (date(left) && date(right)) return false;
    throw python::TypeError("'<' not supported");
  }
  return python::compare(left.value, right.value) < 0;
}

// Python's tuple < over label sequences: the first pair that is not the
// same decides, else the shorter sequence is less.
bool sequence_less(const std::vector<Token>& left, const std::vector<Token>& right) {
  const auto n = std::min(left.size(), right.size());
  for (std::size_t i = 0; i < n; ++i)
    if (!same(left[i], right[i])) return token_less(left[i], right[i]);
  return left.size() < right.size();
}

std::string type_name(const Token& token) {
  return token.nat ? std::string("NaTType") : python::type_name(token.value);
}

std::string display(const Token& token) {
  return token.nat ? std::string("NaT") : python::str(token.value);
}

using Context = std::vector<Token>;
using ContextKey = std::vector<std::string>;
using EncodedContext = std::vector<std::size_t>;

ContextKey context_key(const Context& context) {
  ContextKey key;
  key.reserve(context.size());
  for (const auto& token : context) key.push_back(token.id);
  return key;
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
  Token activity;
  Context context;
  EncodedContext encoded_context;
  std::vector<std::pair<std::size_t, std::size_t>> events;
};

struct SegmentCollection {
  // The vector preserves first-seen context order. The lookup map only
  // identifies an existing segment and never defines its output order.
  std::vector<SegmentRef> nodes;
  std::map<ContextKey, std::size_t> indices;
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
      if (!same(nodes[i].activity, nodes[j].activity)) continue;
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
  // Python's (node_u, node_v) < (node_x, node_y) over (activity, context)
  // nodes; a node is itself (`is`), and two distinct nodes always differ.
  const auto node_less = [&](std::size_t x, std::size_t y) {
    if (!same(nodes[x].activity, nodes[y].activity)) return token_less(nodes[x].activity, nodes[y].activity);
    return sequence_less(nodes[x].context, nodes[y].context);
  };
  const auto pair_less = [&](std::size_t u, std::size_t v, std::size_t x, std::size_t y) {
    if (u != x) return node_less(u, x);
    return v != y && node_less(v, y);
  };

  while (true) {
    // The reference heap pops the largest gain and breaks ties by the
    // smallest ordered pair of node identifiers, (activity, context).
    bool found = false;
    double best_gain = 0.0;
    std::size_t best_u = n, best_v = n;
    for (std::size_t u = 0; u < n; ++u) {
      if (!alive[u]) continue;
      for (const auto& [v, gain] : dq[u]) {
        bool better = !found || gain > best_gain;
        // Round CR: labels Python cannot order raise in the graph library's
        // heap, which words the TypeError its own way.
        if (!better && gain == best_gain) {
          try {
            better = pair_less(u, v, best_u, best_v);
          } catch (const python::TypeError&) {
            throw std::invalid_argument("TypeError: Consider using a tuple, with a priority value that can be compared.");
          }
        }
        if (better) {
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

// The labels of a plain log: the activity or the attribute's text ("" when
// the event lacks it).
std::string activity_text(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

bool activity_field(const Event& event, const std::string& key) {
  return key.empty() || key == "activity" || key == "concept:name" ||
         (!event.activity_source_key.empty() && key == event.activity_source_key);
}

// Writes a label into the target: the activity field (its text) or an
// attribute typed as the label's Python value; a missing label leaves a
// missing activity or a null attribute.
void set_label(Event& event, const std::string& key, const Token& label, bool missing) {
  using Kind = python::Scalar::Kind;
  if (activity_field(event, key)) {
    if (missing) {
      event.activity.clear();
      event.activity_state = FirstClassAttributeState::kMissing;
      event.activity_attribute.reset();
      return;
    }
    const auto source = event;
    set_event_activity_context(source, label.value.kind == Kind::kText ? label.value.text : display(label), event);
    return;
  }
  EventAttribute attribute{key, missing ? std::string{} : display(label), AttributeKind::kString};
  if (missing) {
    attribute.is_null = true;
  } else {
    switch (label.value.kind) {
      case Kind::kBool:
        attribute.kind = AttributeKind::kBoolean;
        attribute.typed_value = label.value.boolean;
        break;
      case Kind::kInt:
        attribute.kind = AttributeKind::kInt;
        if (const auto parsed = python::integer_literal(attribute.value); parsed && parsed->digits.size() < 19)
          attribute.typed_value = static_cast<std::int64_t>(std::stoll(attribute.value));
        break;
      case Kind::kFloat:
        attribute.kind = AttributeKind::kFloat;
        attribute.typed_value = label.value.real;
        break;
      case Kind::kDate:
        attribute.kind = AttributeKind::kDate;
        break;
      default: break;
    }
  }
  for (auto& existing : event.attributes) {
    if (existing.key == key) {
      // A label replaces the selected value, including its old typed payload.
      existing = std::move(attribute);
      return;
    }
  }
  event.attributes.push_back(std::move(attribute));
}

// The labels pm4py reads from an upload's frame (round CR, observed
// black-box): the column's Python values, typed, with pandas' missing cells
// (NaT in a date column, a fresh NaN per cell in a float64 column, the one
// NaN object elsewhere). nullopt for a log without a frame view.
std::optional<std::vector<std::vector<Token>>> frame_labels(const EventLog& log, const std::string& requested,
                                                            std::vector<std::vector<bool>>& missing) {
  if (!log.dataframe_columns) return std::nullopt;
  const auto frame = dataframe_frame_event_log(log);
  if (frame.traces.size() != log.traces.size()) return std::nullopt;
  const std::string key = requested.empty() || requested == "activity" ? std::string("concept:name") : requested;
  if (!frame_keys(frame, FrameKeyScope::kDataFrame).contains(key))
    throw std::invalid_argument("KeyError: \"Columns not found: '" + key + "'\"");
  const auto column = frame_column_type(frame, key);
  std::vector<std::vector<Token>> labels(frame.traces.size());
  missing.assign(frame.traces.size(), {});
  std::size_t row = 0;
  for (std::size_t t = 0; t < frame.traces.size(); ++t) {
    const auto& trace = frame.traces[t];
    if (trace.events.size() != log.traces[t].events.size()) return std::nullopt;
    for (const auto& event : trace.events) {
      const auto cell = frame_scalar(frame, trace, event, key, FrameKeyScope::kDataFrame);
      Token token;
      const bool absent = !cell || python::is_missing(*cell);
      if (!absent) {
        token.value = *cell;
        token.id = python::identity(*cell);
      } else if (column && *column == DataFrameColumnType::kDate) {
        token.nat = true;
        token.id = "NaT";
      } else {
        token.value = python::real(std::numeric_limits<double>::quiet_NaN());
        token.id = column && *column == DataFrameColumnType::kFloat64 ? "Fnan#" + std::to_string(row) : "Fnan";
      }
      labels[t].push_back(std::move(token));
      missing[t].push_back(absent);
      ++row;
    }
  }
  return labels;
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
  // Round CR: an upload's labels are its frame's typed cells.
  std::vector<std::vector<bool>> missing;
  auto frame = frame_labels(log, options.activity_key, missing);
  std::vector<std::vector<Token>> all_labels;
  if (frame) {
    all_labels = std::move(*frame);
  } else {
    missing.clear();
    for (const auto& trace : log.traces) {
      auto& labels = all_labels.emplace_back();
      for (const auto& event : trace.events) labels.push_back(text_token(activity_text(event, options.activity_key)));
      missing.emplace_back(trace.events.size(), false);
    }
  }
  std::map<std::string, SegmentCollection> segments;
  std::vector<std::string> activity_order;
  std::map<std::string, std::size_t> context_tokens;
  const auto encode_context = [&](const Context& context) {
    EncodedContext encoded;
    encoded.reserve(context.size());
    for (const auto& token : context) {
      auto it = context_tokens.find(token.id);
      if (it == context_tokens.end()) {
        const auto next_id = context_tokens.size();
        it = context_tokens.emplace(token.id, next_id).first;
      }
      encoded.push_back(it->second);
    }
    return encoded;
  };
  for (std::size_t trace_index = 0; trace_index < all_labels.size(); ++trace_index) {
    const auto& labels = all_labels[trace_index];
    for (std::size_t event_index = 0; event_index < labels.size(); ++event_index) {
      const auto& label = labels[event_index];
      // Only a str label can equal a target activity.
      if (!targets.empty() && (label.value.kind != python::Scalar::Kind::kText || label.nat ||
                               targets.count(label.value.text) == 0)) continue;
      Context context;
      const auto prefix_begin = event_index > options.prefix_length
                                    ? event_index - options.prefix_length
                                    : 0;
      for (std::size_t i = prefix_begin; i < event_index; ++i) context.push_back(labels[i]);
      const auto suffix_end = std::min(labels.size(), event_index + options.suffix_length + 1);
      for (std::size_t i = event_index + 1; i < suffix_end; ++i) context.push_back(labels[i]);
      auto [collection_it, inserted_activity] = segments.try_emplace(label.id);
      if (inserted_activity) activity_order.push_back(label.id);
      auto& collection = collection_it->second;
      const auto [found, inserted] = collection.indices.emplace(
          context_key(context), collection.nodes.size());
      if (inserted) {
        SegmentRef segment;
        segment.activity = label;
        segment.encoded_context = encode_context(context);
        segment.context = std::move(context);
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
      auto& piece = pieces[all_nodes[node].activity.id];
      piece.output_order = community.output_order;
      piece.nodes.push_back(node);
    }
    for (auto& [label, piece] : pieces)
      communities_by_activity[label].push_back(std::move(piece));
  }
  // Round CR: the reference relabels community by community, largest first
  // (the graph library's order), and a split label that is no str fails
  // there: label + suffix is a TypeError (observed black-box).
  for (const auto& community : all_communities) {
    if (community.nodes.empty()) continue;
    const auto& activity = all_nodes[community.nodes.front()].activity;
    const auto it = communities_by_activity.find(activity.id);
    if (it == communities_by_activity.end() || it->second.size() < 2) continue;
    if (activity.nat || activity.value.kind != python::Scalar::Kind::kText)
      throw std::invalid_argument("TypeError: unsupported operand type(s) for +: '" + type_name(activity) + "' and 'str'");
  }
  // A separate target column starts as a copy of the activity column, so
  // events that are not relabeled keep their original label there. An
  // upload's target holds the frame's typed values.
  const auto canonical = [](const std::string& key) {
    return key == "concept:name" ? std::string{"activity"} : key;
  };
  if (frame || canonical(options.target_key) != canonical(options.activity_key))
    for (std::size_t t = 0; t < result.log.traces.size(); ++t)
      for (std::size_t e = 0; e < result.log.traces[t].events.size(); ++e)
        set_label(result.log.traces[t].events[e], options.target_key, all_labels[t][e], missing[t][e]);
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
    const auto& text = all_nodes[communities.front().nodes.front()].activity.value.text;
    for (std::size_t community_index = 0; community_index < communities.size(); ++community_index) {
      const auto replacement = text_token(text + options.activities_suffix + std::to_string(community_index));
      for (const auto node : communities[community_index].nodes) {
        for (const auto [trace_index, event_index] : all_nodes[node].events)
          set_label(result.log.traces[trace_index].events[event_index],
                    options.target_key, replacement, false);
      }
    }
  }
  return result;
}

}  // namespace minera
