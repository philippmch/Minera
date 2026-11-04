#include "minera/decision_mining.hpp"
#include "minera/petri_reference_arcs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "decision_replay.hpp"

namespace minera {
namespace {

// Parses a full numeric literal; partial parses ("12 kg") are not numbers.
std::optional<double> parse_number(const std::string& text) {
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size()) return std::nullopt;
  return value;
}

// One observed choice: the attribute values in scope when it was made and the
// branch (index into the decision point's outgoing transitions) it took.
struct Row {
  std::unordered_map<std::string, std::string> features;
  std::size_t label = 0;
};

bool row_satisfies(const Row& row, const DecisionSplit& split) {
  const auto it = row.features.find(split.key);
  if (it == row.features.end()) return false;
  if (!split.numeric) return it->second == split.value;
  const auto value = parse_number(it->second);
  return value && *value <= split.threshold;
}

double gini_impurity(const std::vector<std::uint64_t>& counts,
                     std::uint64_t total) {
  if (total == 0) return 0.0;
  double impurity = 1.0;
  for (const std::uint64_t count : counts) {
    const double share = static_cast<double>(count) / static_cast<double>(total);
    impurity -= share * share;
  }
  return impurity;
}

std::vector<std::uint64_t> label_counts(const std::vector<Row>& rows,
                                        const std::vector<std::size_t>& idx,
                                        std::size_t branches) {
  std::vector<std::uint64_t> counts(branches, 0);
  for (const std::size_t i : idx) ++counts[rows[i].label];
  return counts;
}

struct SplitCandidate {
  double gain = 0.0;
  DecisionSplit split;
};

// The best binary split of the node's rows, judged by Gini gain, or nullopt
// when nothing beats leaving the node a leaf. Numeric keys (every present
// value parses as a number, at least two distinct) offer threshold splits at
// midpoints between consecutive distinct values; every key offers equality
// splits on its most frequent values. Rows missing the key fall to the else
// side, so a split never needs the attribute to be present everywhere.
std::optional<SplitCandidate> best_split(
    const std::vector<Row>& rows,
    const std::vector<std::size_t>& idx,
    const std::vector<std::uint64_t>& node_counts,
    const DecisionMiningOptions& options) {
  const auto total = static_cast<std::uint64_t>(idx.size());
  const double node_impurity = gini_impurity(node_counts, total);
  const std::size_t branches = node_counts.size();

  std::optional<SplitCandidate> best;
  const auto consider = [&](DecisionSplit split,
                            const std::vector<std::uint64_t>& then_counts,
                            std::uint64_t then_total) {
    const std::uint64_t else_total = total - then_total;
    if (then_total < options.min_leaf || else_total < options.min_leaf) return;
    std::vector<std::uint64_t> else_counts(branches);
    for (std::size_t b = 0; b < branches; ++b) {
      else_counts[b] = node_counts[b] - then_counts[b];
    }
    const double weighted =
        (static_cast<double>(then_total) * gini_impurity(then_counts, then_total) +
         static_cast<double>(else_total) * gini_impurity(else_counts, else_total)) /
        static_cast<double>(total);
    const double gain = node_impurity - weighted;
    if (gain <= options.min_gain) return;
    if (!best || gain > best->gain) {
      best = SplitCandidate{gain, std::move(split)};
    }
  };

  // Keys present in this node's rows, in first-seen order for determinism.
  std::vector<std::string> keys;
  std::unordered_set<std::string> seen;
  for (const std::size_t i : idx) {
    for (const auto& [key, value] : rows[i].features) {
      if (seen.insert(key).second) keys.push_back(key);
    }
  }
  std::sort(keys.begin(), keys.end());

  for (const auto& key : keys) {
    // The rows carrying this key, with their values.
    std::vector<std::pair<const std::string*, std::size_t>> present;
    for (const std::size_t i : idx) {
      const auto it = rows[i].features.find(key);
      if (it != rows[i].features.end()) present.push_back({&it->second, i});
    }

    // Threshold splits, when the key is numeric throughout.
    std::vector<std::pair<double, std::size_t>> numeric;
    numeric.reserve(present.size());
    bool all_numeric = true;
    for (const auto& [value, i] : present) {
      const auto number = parse_number(*value);
      if (!number) {
        all_numeric = false;
        break;
      }
      numeric.push_back({*number, rows[i].label});
    }
    if (all_numeric && !numeric.empty()) {
      std::sort(numeric.begin(), numeric.end());
      // Prefix label counts over the sorted values let each threshold's
      // partition be read off in O(branches).
      std::vector<std::uint64_t> then_counts(branches, 0);
      std::vector<double> boundaries;  // candidate thresholds (midpoints)
      for (std::size_t j = 0; j + 1 < numeric.size(); ++j) {
        if (numeric[j].first < numeric[j + 1].first) {
          boundaries.push_back((numeric[j].first + numeric[j + 1].first) / 2.0);
        }
      }
      const std::size_t step =
          boundaries.size() > options.max_thresholds
              ? (boundaries.size() + options.max_thresholds - 1) /
                    options.max_thresholds
              : 1;
      std::size_t next_boundary = 0;
      std::uint64_t then_total = 0;
      for (std::size_t j = 0; j < numeric.size(); ++j) {
        ++then_counts[numeric[j].second];
        ++then_total;
        while (next_boundary < boundaries.size() &&
               boundaries[next_boundary] < numeric[j].first) {
          ++next_boundary;  // never true for sorted midpoints, kept for safety
        }
        if (next_boundary < boundaries.size() &&
            j + 1 < numeric.size() &&
            numeric[j].first < numeric[j + 1].first) {
          if (next_boundary % step == 0) {
            consider(DecisionSplit{key, true, {},
                                   boundaries[next_boundary]},
                     then_counts, then_total);
          }
          ++next_boundary;
        }
      }
    }

    // Equality splits on the key's most frequent values.
    struct ValueStats {
      std::vector<std::uint64_t> counts;
      std::uint64_t total = 0;
    };
    std::unordered_map<std::string, ValueStats> by_value;
    for (const auto& [value, i] : present) {
      auto& stats = by_value[*value];
      if (stats.counts.empty()) stats.counts.assign(branches, 0);
      ++stats.counts[rows[i].label];
      ++stats.total;
    }
    std::vector<const std::pair<const std::string, ValueStats>*> values;
    values.reserve(by_value.size());
    for (const auto& entry : by_value) values.push_back(&entry);
    std::sort(values.begin(), values.end(), [](const auto* a, const auto* b) {
      return a->second.total != b->second.total
                 ? a->second.total > b->second.total
                 : a->first < b->first;
    });
    if (values.size() < 2 && present.size() == idx.size()) {
      continue;  // one value everywhere: nothing to separate
    }
    const std::size_t listed = std::min(values.size(), options.max_categories);
    for (std::size_t v = 0; v < listed; ++v) {
      consider(DecisionSplit{key, false, values[v]->first, 0.0},
               values[v]->second.counts, values[v]->second.total);
    }
  }

  return best;
}

// Grows the tree depth-first into `nodes` and returns the new node's index.
int build_node(const std::vector<Row>& rows,
               std::vector<std::size_t> idx,
               std::size_t branches,
               std::size_t depth,
               const DecisionMiningOptions& options,
               std::vector<DecisionNode>* nodes) {
  auto counts = label_counts(rows, idx, branches);
  const auto total = static_cast<std::uint64_t>(idx.size());
  const int index = static_cast<int>(nodes->size());
  nodes->push_back(DecisionNode{std::move(counts), {}, -1, -1});

  const auto& node_counts = (*nodes)[static_cast<std::size_t>(index)].branch_counts;
  const bool pure =
      *std::max_element(node_counts.begin(), node_counts.end()) == total;
  if (pure || depth >= options.max_depth || idx.size() < options.min_split)
    return index;

  auto split = best_split(rows, idx, node_counts, options);
  if (!split) return index;

  std::vector<std::size_t> then_idx;
  std::vector<std::size_t> else_idx;
  for (const std::size_t i : idx) {
    (row_satisfies(rows[i], split->split) ? then_idx : else_idx).push_back(i);
  }
  idx.clear();
  idx.shrink_to_fit();

  const int then_child =
      build_node(rows, std::move(then_idx), branches, depth + 1, options, nodes);
  const int else_child =
      build_node(rows, std::move(else_idx), branches, depth + 1, options, nodes);
  auto& node = (*nodes)[static_cast<std::size_t>(index)];
  node.split = std::move(split->split);
  node.then_child = then_child;
  node.else_child = else_child;
  return index;
}

std::size_t predict(const std::vector<DecisionNode>& nodes, const Row& row) {
  std::size_t at = 0;
  while (nodes[at].then_child >= 0) {
    at = static_cast<std::size_t>(row_satisfies(row, nodes[at].split)
                                      ? nodes[at].then_child
                                      : nodes[at].else_child);
  }
  const auto& counts = nodes[at].branch_counts;
  return static_cast<std::size_t>(
      std::max_element(counts.begin(), counts.end()) - counts.begin());
}

// Display label for one outgoing transition of a decision place: a visible
// transition is its activity; a silent one is described by the visible
// activities it silently leads to (an over-approximating reachability that
// treats each place independently — a label, not a semantics).
std::string branch_label(const PetriNet& net, std::size_t transition) {
  const auto& start = net.transitions()[transition];
  if (!start.silent()) return net.activities()[*start.activity];

  std::vector<char> place_seen(net.place_count(), 0);
  std::vector<std::size_t> stack;
  for (const std::size_t place : start.outputs) {
    if (!place_seen[place]) {
      place_seen[place] = 1;
      stack.push_back(place);
    }
  }
  std::vector<std::string> reachable;
  std::unordered_set<std::string> named;
  bool reaches_sink = false;
  while (!stack.empty()) {
    const std::size_t place = stack.back();
    stack.pop_back();
    if (place == net.sink()) reaches_sink = true;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& candidate = net.transitions()[t];
      const bool touches =
          std::find(candidate.inputs.begin(), candidate.inputs.end(), place) !=
          candidate.inputs.end();
      if (!touches) continue;
      if (candidate.silent()) {
        for (const std::size_t out : candidate.outputs) {
          if (!place_seen[out]) {
            place_seen[out] = 1;
            stack.push_back(out);
          }
        }
      } else {
        const auto& name = net.activities()[*candidate.activity];
        if (named.insert(name).second) reachable.push_back(name);
      }
    }
  }

  std::string label = "\xE2\x86\x92 ";  // "→ "
  if (reachable.empty()) {
    label += reaches_sink ? "(end)" : "(silent)";
    return label;
  }
  const std::size_t listed = std::min<std::size_t>(reachable.size(), 3);
  for (std::size_t i = 0; i < listed; ++i) {
    if (i > 0) label += ", ";
    label += reachable[i];
  }
  if (reachable.size() > listed) label += ", \xE2\x80\xA6";  // ", …"
  return label;
}

}  // namespace

DecisionMiningResult mine_decisions(const EventLog& log, const PetriNet& net,
                                    const DecisionMiningOptions& options) {
  if (options.max_depth > 64)
    throw std::invalid_argument("decision mining max_depth exceeds 64");
  if (options.min_split < 2)
    throw std::invalid_argument("decision mining min_split must be at least 2");
  if (options.min_leaf == 0)
    throw std::invalid_argument("decision mining min_leaf must be positive");
  if (options.max_categories == 0 || options.max_thresholds == 0)
    throw std::invalid_argument("decision mining candidate limits must be positive");
  if (!std::isfinite(options.min_gain) || options.min_gain < 0.0)
    throw std::invalid_argument(
        "decision mining min_gain must be finite and non-negative");
  std::unordered_set<std::string> selected_attributes;
  for (const auto& attribute : options.attributes) {
    if (attribute.empty())
      throw std::invalid_argument("decision mining attributes must be non-empty");
    selected_attributes.insert(attribute);
  }
  DecisionMiningResult result;
  result.trace_count = log.trace_count();
  result.activity_attribute = options.activity_attribute.empty()
                                  ? "concept:name"
                                  : options.activity_attribute;

  // Outgoing transitions per place; a place with several is a decision point.
  std::vector<std::vector<std::size_t>> outgoing(net.place_count());
  // One entry per arc object, as the reference lists a place's out-arcs: a
  // weight-2 arc counts once and two parallel arcs twice.
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    for (const auto& arc : petri_reference_arcs(net.transitions()[t], true)) {
      outgoing[arc.place].push_back(t);
    }
  }
  std::vector<int> decision_index(net.place_count(), -1);
  std::vector<std::size_t> decision_places;
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    if (outgoing[p].size() >= 2) {
      decision_index[p] = static_cast<int>(decision_places.size());
      decision_places.push_back(p);
    }
  }

  // Every distinct activity variant is replayed once. Deviating variants
  // receive an alignment path, so both clean and non-fitting traces expose
  // the same model-choice observations to the tree learner.
  const auto paths = detail::collect_decision_replay_paths(
      log, net, options.activity_attribute);
  result.observed_traces = paths.observed_traces();
  result.replayed_traces = paths.replayed_traces;
  result.aligned_traces = paths.aligned_traces;
  result.alignment_failures = paths.alignment_failures;

  std::vector<std::vector<Row>> rows(decision_places.size());
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& path = paths.paths[paths.variant_of[i]];
    if (!path.available) continue;
    const auto& trace = log.traces[i];

    // The attributes in scope, growing as the trace unfolds: case attributes
    // up front, then each consumed event merges its own — after the choices
    // its consumption triggered, so a decision only sees what preceded it.
    std::unordered_map<std::string, std::string> context;
    if (options.include_trace_attributes) {
      for (const auto& attribute : trace.attributes) {
        if (attribute.is_null) continue;
        context[attribute.key] = attribute.value;
      }
    }
    std::size_t event_index = 0;
    for (const auto& move : path.moves) {
      if (move.transition != static_cast<std::size_t>(-1) &&
          move.transition < net.transitions().size()) {
        const std::size_t t = move.transition;
        // One row per firing and decision place, however many arcs join them.
        std::vector<std::size_t> input_places;
        for (const auto& arc : petri_reference_arcs(net.transitions()[t], true))
          if (std::find(input_places.begin(), input_places.end(), arc.place) == input_places.end())
            input_places.push_back(arc.place);
        for (const std::size_t place : input_places) {
          const int d = decision_index[place];
          if (d < 0) continue;
          const auto& choices = outgoing[place];
          const auto label = static_cast<std::size_t>(
              std::find(choices.begin(), choices.end(), t) - choices.begin());
          std::unordered_map<std::string, std::string> scoped = context;
          if (!selected_attributes.empty()) {
            for (auto it = scoped.begin(); it != scoped.end();) {
              if (!selected_attributes.contains(it->first))
                it = scoped.erase(it);
              else
                ++it;
            }
          }
          rows[static_cast<std::size_t>(d)].push_back(
              Row{std::move(scoped), label});
        }
      }
      // Synchronous and log-only moves consume the next log event. Model-only
      // moves (including visible insertions) leave it pending, so a later
      // decision sees the same pre-event context.
      if (move.type == AlignmentMoveType::kSynchronous ||
          move.type == AlignmentMoveType::kLog) {
        if (event_index < trace.events.size()) {
          const auto& event = trace.events[event_index];
          if (!event.resource.empty()) context["resource"] = event.resource;
          for (const auto& attribute : event.attributes) {
            if (attribute.is_null) continue;
            context[attribute.key] = attribute.value;
          }
        }
        ++event_index;
      }
    }
  }

  for (std::size_t d = 0; d < decision_places.size(); ++d) {
    if (rows[d].empty()) continue;  // the log never reached this choice
    DecisionPoint point;
    point.place = decision_places[d];
    for (const std::size_t t : outgoing[point.place]) {
      point.branches.push_back(branch_label(net, t));
    }
    point.observations = rows[d].size();

    std::vector<std::size_t> idx(rows[d].size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    build_node(rows[d], std::move(idx), point.branches.size(), 0, options,
               &point.nodes);

    const auto& root = point.nodes.front().branch_counts;
    point.majority_share =
        static_cast<double>(*std::max_element(root.begin(), root.end())) /
        static_cast<double>(point.observations);
    std::uint64_t correct = 0;
    for (const auto& row : rows[d]) {
      if (predict(point.nodes, row) == row.label) ++correct;
    }
    point.accuracy = static_cast<double>(correct) /
                     static_cast<double>(point.observations);
    result.decision_points.push_back(std::move(point));
  }

  // Most-consulted decision points first, so the interesting choices lead.
  std::stable_sort(result.decision_points.begin(), result.decision_points.end(),
                   [](const DecisionPoint& a, const DecisionPoint& b) {
                     return a.observations > b.observations;
                   });
  return result;
}

}  // namespace minera
