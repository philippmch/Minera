#include "minera/evaluation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

using Marking = std::vector<int>;
struct MarkingHash { std::size_t operator()(const Marking& m) const noexcept {
  std::size_t h = 1469598103934665603ull; for (int x : m) h = (h ^ static_cast<unsigned>(x)) * 1099511628211ull; return h;
} };

Marking initial(const PetriNet& n, const SimplicityOptions& o) {
  if (!o.initial_marking.empty()) return o.initial_marking;
  Marking m(n.place_count(), 0); if (n.source()<m.size()) m[n.source()]=1;
  return m;
}
bool enabled(const PetriTransition& t, const Marking& m) {
  return petri_transition_enabled(t, m);
}
Marking fire(const PetriTransition& t, Marking m) {
  fire_petri_transition(t, m); return m;
}
bool silent_closure(const PetriNet& n, const Marking& start,
                    std::size_t cap, std::vector<Marking>& out) {
  out.clear();
  if (cap == 0) return false;
  out.push_back(start);
  std::queue<std::size_t> q;
  q.push(0);
  std::unordered_set<Marking, MarkingHash> seen;
  seen.insert(start);
  while (!q.empty()) {
    const auto i = q.front();
    q.pop();
    for (const auto& t : n.transitions()) {
      if (!t.silent() || !enabled(t, out[i])) continue;
      auto next = fire(t, out[i]);
      if (seen.insert(next).second) {
        if (out.size() >= cap) return false;
        out.push_back(std::move(next));
        q.push(out.size() - 1);
      }
    }
  }
  return true;
}

std::vector<Marking> closure(const PetriNet& n, Marking start,
                             std::size_t cap = 100000) {
  std::vector<Marking> out;
  silent_closure(n, start, cap, out);
  return out;
}

bool enabled_labels(const PetriNet& n, const Marking& m, std::size_t cap,
                    bool use_task_ids, std::set<std::string>& labels) {
  labels.clear();
  std::vector<Marking> states;
  if (!silent_closure(n, m, cap, states)) return false;
  for (const auto& state : states) {
    for (std::size_t i = 0; i < n.transitions().size(); ++i) {
      const auto& t = n.transitions()[i];
      if (t.silent() || !enabled(t, state)) continue;
      labels.insert(use_task_ids ? "t" + std::to_string(i)
                                 : n.activities()[*t.activity]);
    }
  }
  return true;
}

double levenshtein(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  std::vector<double> d(b.size()+1); std::iota(d.begin(), d.end(), 0.0);
  for (std::size_t i=1;i<=a.size();++i) { double prev=d[0]; d[0]=double(i);
    for (std::size_t j=1;j<=b.size();++j) { double x=d[j]; d[j]=std::min({d[j]+1,d[j-1]+1,prev+(a[i-1]==b[j-1]?0:1)}); prev=x; }
  }
  return d.back() / double(std::max(a.size(),b.size()));
}

} // namespace

double normalized_levenshtein_distance(const std::vector<std::string>& a,const std::vector<std::string>& b) {
  if (a.empty() && b.empty()) return 0.0; return levenshtein(a,b);
}

ReplayFitnessResult evaluate_replay_fitness(const EventLog& log, const PetriNet& net, ReplayFitnessVariant variant) {
  return evaluate_replay_fitness(log, net, variant, {});
}

namespace {

std::string selected_evaluation_activity(
    const Event& event, std::string_view activity_attribute) {
  if (activity_attribute.empty() || activity_attribute == "concept:name" ||
      activity_attribute == "activity") {
    return event.activity;
  }
  if (const auto* value = event.attribute(activity_attribute)) return *value;
  throw std::invalid_argument(
      "evaluation: event is missing activity attribute '" +
      std::string(activity_attribute) + "'");
}

EventLog project_evaluation_activity(
    const EventLog& log, std::string_view activity_attribute) {
  EventLog projected = log;
  for (std::size_t trace = 0; trace < log.traces.size(); ++trace) {
    for (std::size_t event = 0; event < log.traces[trace].events.size();
         ++event) {
      const auto& source = log.traces[trace].events[event];
      auto& target = projected.traces[trace].events[event];
      set_event_activity_context(
          source, selected_evaluation_activity(source, activity_attribute),
          target);
    }
  }
  return projected;
}

bool has_reachable_final_marking(const PetriNet& net) {
  EventLog probe;
  probe.traces.push_back(Trace{});
  const AlignmentOptions options{
      AlignmentVariant::kDijkstraLessMemory,
      AlignmentCostProfile::kStandard};
  return check_alignment(probe, net, options).computed;
}

}  // namespace

ReplayFitnessResult evaluate_replay_fitness(
    const EventLog& log, const PetriNet& net, ReplayFitnessVariant variant,
    const ReplayFitnessOptions& options) {
  ReplayFitnessResult out;
  out.trace_count = log.trace_count();
  AlignmentOptions alignment_options = options.alignment_options;
  alignment_options.activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  if (variant == ReplayFitnessVariant::AUTO) {
    variant = has_reachable_final_marking(net)
                  ? ReplayFitnessVariant::ALIGNMENT_BASED
                  : ReplayFitnessVariant::TOKEN_BASED;
  }
  out.variant = variant;
  if (log.trace_count() == 0) {
    // Evaluators have no observations from which to compute a score;
    // unlike the generic conformance API, their empty-log metrics are zero.
    // Retain the selected detail engine so serialization does not substitute
    // an unrelated default payload for an otherwise valid empty evaluation.
    if (variant == ReplayFitnessVariant::TOKEN_BASED) {
      out.token_result.activity_attribute = alignment_options.activity_attribute;
    } else if (options.decomposed_alignment_options) {
      out.decomposed_alignment_result = DecomposedAlignmentResult{};
    } else {
      out.alignment_result.variant = alignment_options.variant;
      out.alignment_result.activity_attribute =
          alignment_options.activity_attribute;
    }
    return out;
  }

  if (variant == ReplayFitnessVariant::TOKEN_BASED) {
    TokenReplayOptions replay_options;
    replay_options.variant = options.token_replay_variant;
    replay_options.stop_immediately_unfit = false;
    replay_options.consider_remaining_in_fitness = true;
    replay_options.try_to_reach_final_marking_through_hidden = true;
    replay_options.consider_activities_not_in_model_in_fitness =
        options.consider_activities_not_in_model_in_fitness;
    replay_options.activity_attribute = alignment_options.activity_attribute;
    out.token_result = check_conformance(log, net, replay_options);
    out.fitness = out.token_result.fitness;
    out.log_fitness = out.fitness;
    out.fitting_traces = out.token_result.fitting_traces;
    double average = 0.0;
    for (const auto& replay_variant : out.token_result.variants) {
      average += replay_variant.fitness * double(replay_variant.count);
    }
    out.average_trace_fitness = average / double(out.trace_count);
    if (out.token_result.produced == 0 || out.token_result.consumed == 0) {
      out.fitness = 0.0;
      out.log_fitness = 0.0;
      out.average_trace_fitness = 0.0;
    }
  } else if (options.decomposed_alignment_options) {
    auto decomposed_options = *options.decomposed_alignment_options;
    const auto projected = project_evaluation_activity(
        log, alignment_options.activity_attribute);
    auto decomposed = align_decomposed(projected, net, decomposed_options);
    out.computed = decomposed.complete;
    out.exhausted = decomposed.exhausted || decomposed.timed_out;
    out.fitness = out.computed ? decomposed.fitness : 0.0;
    out.log_fitness = out.fitness;
    out.total_cost = decomposed.total_cost;
    double average = 0.0;
    for (const auto& trace : decomposed.traces) {
      average += trace.fitness * double(trace.count);
      if (trace.complete && std::abs(trace.fitness - 1.0) <= 1e-12) {
        out.fitting_traces += trace.count;
      }
    }
    out.average_trace_fitness = average / double(out.trace_count);
    if (!out.computed) {
      out.error = decomposed.timed_out
                      ? "decomposed alignment timed out"
                      : decomposed.exhausted
                            ? "decomposed alignment search exhausted"
                            : "decomposed alignment failed";
    }
    out.decomposed_alignment_result = std::move(decomposed);
  } else {
    out.alignment_result = check_alignment(log, net, alignment_options);
    out.computed = out.alignment_result.computed;
    out.exhausted = out.alignment_result.exhausted;
    out.fitness = out.computed ? out.alignment_result.fitness : 0.0;
    out.log_fitness = out.fitness;
    out.fitting_traces = out.alignment_result.fitting_traces;
    out.total_cost = out.alignment_result.total_cost;
    double average = 0.0;
    for (const auto& alignment_variant : out.alignment_result.variants) {
      average += alignment_variant.fitness * double(alignment_variant.count);
    }
    out.average_trace_fitness = average / double(out.trace_count);
    if (!out.computed) {
      out.error = out.exhausted ? "alignment search exhausted"
                                : "final marking is unreachable";
    }
  }
  out.percentage_fit_traces =
      100.0 * double(out.fitting_traces) / double(out.trace_count);
  return out;
}
ReplayFitnessResult evaluate_replay_fitness(const EventLog& l,const ProcessTree& t,ReplayFitnessVariant v) { return evaluate_replay_fitness(l,to_petri_net(t),v); }
ReplayFitnessResult evaluate_replay_fitness(
    const EventLog& l, const ProcessTree& t, ReplayFitnessVariant v,
    const ReplayFitnessOptions& o) {
  return evaluate_replay_fitness(l, to_petri_net(t), v, o);
}

namespace {

struct PrefixInfo {
  std::uint64_t frequency = 0;
  std::set<std::string> observed;
};

std::map<std::vector<std::string>, PrefixInfo> collect_prefixes(
    const EventLog& log, std::string_view activity_attribute) {
  std::map<std::vector<std::string>, PrefixInfo> prefixes;
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    for (const auto& event : trace.events) {
      activities.push_back(
          selected_evaluation_activity(event, activity_attribute));
    }
    auto& root = prefixes[{}];
    root.frequency += 1;
    if (!activities.empty()) root.observed.insert(activities.front());
    std::vector<std::string> prefix;
    for (std::size_t i = 0; i + 1 < activities.size(); ++i) {
      prefix.push_back(activities[i]);
      auto& node = prefixes[prefix];
      node.frequency += 1;
      node.observed.insert(activities[i + 1]);
    }
  }
  return prefixes;
}

struct StopAlignmentResult {
  bool computed = true;
  bool exhausted = false;
  double cost = 0.0;
  std::vector<Marking> markings;
};

struct ProductKey {
  Marking marking;
  std::size_t position = 0;
  bool operator==(const ProductKey& other) const {
    return position == other.position && marking == other.marking;
  }
};
struct ProductHash {
  std::size_t operator()(const ProductKey& key) const noexcept {
    std::size_t h = MarkingHash{}(key.marking);
    return (h ^ (key.position + 0x9e3779b9u)) * 1099511628211ull;
  }
};

StopAlignmentResult align_to_stop_prefix(const PetriNet& net,
                                          const std::vector<std::string>& prefix,
                                          std::size_t cap) {
  StopAlignmentResult result;
  if (cap == 0) {
    result.computed = false;
    result.exhausted = true;
    return result;
  }
  struct State { Marking marking; std::size_t position; };
  struct QueueItem { double cost; std::size_t id; bool operator>(const QueueItem& o) const { return cost > o.cost; } };
  std::vector<State> states;
  std::vector<double> distances;
  std::unordered_map<ProductKey, std::size_t, ProductHash> ids;
  Marking start(net.place_count(), 0);
  if (net.source() < start.size()) start[net.source()] = 1;
  ids.emplace(ProductKey{start, 0}, 0);
  states.push_back({start, 0});
  distances.push_back(0.0);
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
  queue.push({0.0, 0});
  constexpr double kSync = 0.0;
  constexpr double kModelOrLog = 10000.0;
  constexpr double kTau = 1.0;
  double best = std::numeric_limits<double>::infinity();
  while (!queue.empty()) {
    const auto item = queue.top();
    queue.pop();
    if (item.cost != distances[item.id]) continue;
    if (item.cost > best) break;
    // Copy before relaxing successors: inserting a new product state may
    // reallocate `states` and invalidate references into the vector.
    const State state = states[item.id];
    if (state.position == prefix.size()) {
      if (item.cost < best) {
        best = item.cost;
        result.markings.clear();
      }
      if (item.cost == best) result.markings.push_back(state.marking);
      continue;
    }
    auto relax = [&](const Marking& marking, std::size_t position, double cost) {
      const double next_cost = item.cost + cost;
      ProductKey key{marking, position};
      auto it = ids.find(key);
      if (it == ids.end()) {
        if (states.size() >= cap) {
          result.computed = false;
          result.exhausted = true;
          return;
        }
        const auto id = states.size();
        ids.emplace(std::move(key), id);
        states.push_back({marking, position});
        distances.push_back(next_cost);
        queue.push({next_cost, id});
      } else if (next_cost < distances[it->second]) {
        distances[it->second] = next_cost;
        queue.push({next_cost, it->second});
      }
    };
    if (state.position < prefix.size()) relax(state.marking, state.position + 1, kModelOrLog);
    for (const auto& transition : net.transitions()) {
      if (!enabled(transition, state.marking)) continue;
      const auto next = fire(transition, state.marking);
      if (transition.silent()) {
        relax(next, state.position, kTau);
      } else {
        if (net.activities()[*transition.activity] == prefix[state.position])
          relax(next, state.position + 1, kSync);
        relax(next, state.position, kModelOrLog);
      }
    }
    if (!result.computed) return result;
  }
  if (!std::isfinite(best)) {
    result.computed = false;
    result.exhausted = false;
  }
  result.cost = best;
  std::sort(result.markings.begin(), result.markings.end());
  result.markings.erase(std::unique(result.markings.begin(), result.markings.end()), result.markings.end());
  return result;
}

PrecisionResult evaluate_precision_impl(const EventLog& log, const PetriNet& net,
                                         PrecisionVariant variant,
                                         const PrecisionOptions& options) {
  PrecisionResult out;
  out.variant = variant;
  const std::string activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  AlignmentOptions alignment_options{
      AlignmentVariant::kDijkstraLessMemory,
      AlignmentCostProfile::kStandard};
  alignment_options.activity_attribute = activity_attribute;
  if (options.max_states == 0) {
    out.computed = false;
    out.exhausted = true;
    out.error = "precision state cap must be positive";
    return out;
  }
  if (variant == PrecisionVariant::AUTO) {
    variant = has_reachable_final_marking(net)
                  ? PrecisionVariant::ALIGN_ETCONFORMANCE
                  : PrecisionVariant::ETCONFORMANCE_TOKEN;
  }
  out.variant = variant;
  if (variant == PrecisionVariant::ETCONFORMANCE_TOKEN) {
    TokenReplayOptions replay_options;
    replay_options.variant = options.token_replay_variant;
    replay_options.stop_immediately_unfit = true;
    replay_options.consider_remaining_in_fitness = false;
    replay_options.try_to_reach_final_marking_through_hidden = false;
    replay_options.consider_activities_not_in_model_in_fitness = false;
    replay_options.activity_attribute = activity_attribute;
    out.token_result = check_conformance(log, net, replay_options);
    out.precision = out.token_result.precision;
    return out;
  }

  // Check model easy-soundness independently of the input log.  The legacy
  // alignment API intentionally treats an empty log as trivially computable,
  // which would otherwise mask an unreachable final marking here.
  EventLog probe;
  probe.traces.push_back(Trace{});
  const auto soundness = check_alignment(probe, net, alignment_options);
  if (!soundness.computed) {
    out.computed = false;
    out.exhausted = soundness.exhausted;
    out.error = out.exhausted ? "alignment search exhausted" : "model is not easy sound";
    return out;
  }

  if (variant == PrecisionVariant::ALIGN_ETCONFORMANCE) {
    const auto prefixes = collect_prefixes(log, activity_attribute);
    double enabled_total = 0.0;
    double escaping_total = 0.0;
    for (const auto& [prefix, info] : prefixes) {
      if (info.frequency == 0) continue;
      std::vector<Marking> markings;
      if (prefix.empty()) {
        Marking start(net.place_count(), 0);
        if (net.source() < start.size()) start[net.source()] = 1;
        markings.push_back(std::move(start));
      } else {
        auto stop = align_to_stop_prefix(net, prefix, options.max_states);
        if (!stop.computed) {
          out.computed = false;
          out.exhausted = stop.exhausted;
          out.error = stop.exhausted ? "precision search exhausted" : "prefix cannot be aligned";
          return out;
        }
        markings = std::move(stop.markings);
      }
      std::set<std::string> available;
      for (const auto& marking : markings) {
        std::set<std::string> local;
        if (!enabled_labels(net, marking, options.max_states, false, local)) {
          out.computed = false;
          out.exhausted = true;
          out.error = "silent closure state cap exceeded";
          return out;
        }
        available.insert(local.begin(), local.end());
      }
      enabled_total += double(info.frequency) * available.size();
      for (const auto& symbol : available)
        if (!info.observed.count(symbol)) escaping_total += double(info.frequency);
    }
    out.precision = enabled_total == 0.0
                       ? 1.0
                       : std::clamp(1.0 - escaping_total / enabled_total, 0.0, 1.0);
    return out;
  }

  out.alignment_result = check_alignment(log, net, alignment_options);
  if (!out.alignment_result.computed) {
    out.computed = false;
    out.exhausted = out.alignment_result.exhausted;
    out.error = out.exhausted ? "alignment search exhausted" : "alignment failed";
    return out;
  }

  // Automaton-after-align projects each optimal alignment to visible model
  // moves.  A shared prefix is weighted by variants that have another move;
  // terminal projected prefixes are never scored.
  struct Node {
    std::map<std::string, std::size_t> children;
    std::uint64_t frequency = 0;
    std::set<std::string> observed;
    std::vector<Marking> markings;
  };
  std::vector<Node> trie(1);
  for (const auto& variant_alignment : out.alignment_result.variants) {
    auto& root = trie[0];
    root.frequency += variant_alignment.count;
    Marking start(net.place_count(), 0);
    if (net.source() < start.size()) start[net.source()] = 1;
    root.markings.push_back(start);
    std::size_t node = 0;
    for (std::size_t i = 0; i < variant_alignment.projected_model_moves.size(); ++i) {
      const auto& move = variant_alignment.projected_model_moves[i];
      const std::string symbol = options.use_task_ids
                                     ? "t" + std::to_string(move.transition)
                                     : move.activity;
      trie[node].observed.insert(symbol);
      auto child = trie[node].children.find(symbol);
      if (child == trie[node].children.end()) {
        const auto id = trie.size();
        trie[node].children.emplace(symbol, id);
        trie.emplace_back();
        child = trie[node].children.find(symbol);
      }
      node = child->second;
      if (i + 1 < variant_alignment.projected_model_moves.size()) {
        trie[node].frequency += variant_alignment.count;
        trie[node].markings.push_back(move.marking_after);
      }
    }
  }
  double enabled_total = 0.0;
  double escaping_total = 0.0;
  for (const auto& node : trie) {
    if (node.frequency == 0) continue;
    std::set<std::string> available;
    for (const auto& marking : node.markings) {
      std::set<std::string> local;
      if (!enabled_labels(net, marking, options.max_states, options.use_task_ids, local)) {
        out.computed = false;
        out.exhausted = true;
        out.error = "silent closure state cap exceeded";
        return out;
      }
      available.insert(local.begin(), local.end());
    }
    enabled_total += double(node.frequency) * available.size();
    for (const auto& symbol : available)
      if (!node.observed.count(symbol)) escaping_total += double(node.frequency);
  }
  out.precision = enabled_total == 0.0
                     ? 1.0
                     : std::clamp(1.0 - escaping_total / enabled_total, 0.0, 1.0);
  return out;
}
}  // namespace

PrecisionResult evaluate_precision(const EventLog& l, const PetriNet& n,
                                   PrecisionVariant v) {
  return evaluate_precision_impl(l, n, v, {});
}
PrecisionResult evaluate_precision(const EventLog& l, const ProcessTree& t,
                                   PrecisionVariant v) {
  return evaluate_precision(l, to_petri_net(t), v);
}
PrecisionResult evaluate_precision(const EventLog& l, const ProcessTree& t,
                                   PrecisionVariant v,
                                   const PrecisionOptions& o) {
  return evaluate_precision(l, to_petri_net(t), v, o);
}
PrecisionResult evaluate_precision(const EventLog& l, const PetriNet& n,
                                   PrecisionVariant v, const PrecisionOptions& o) {
  return evaluate_precision_impl(l, n, v, o);
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const PetriNet& net, GeneralizationVariant variant) {
  return evaluate_generalization(log, net, variant, {});
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const PetriNet& net, GeneralizationVariant v,
    const GeneralizationOptions& options) {
  GeneralizationResult out;
  out.variant = v;
  out.transition_count = net.transitions().size();
  out.transition_frequencies.assign(net.transitions().size(), 0);
  if (log.trace_count() == 0) {
    out.generalization = net.transitions().empty() ? 1.0 : 0.0;
    return out;
  }
  TokenReplayOptions replay_options;
  replay_options.consider_activities_not_in_model_in_fitness = false;
  replay_options.activity_attribute = options.activity_attribute.empty()
                                          ? "concept:name"
                                          : options.activity_attribute;
  const auto replay = check_conformance(log, net, replay_options);
  out.transition_frequencies = replay.transition_frequencies;
  if (out.transition_frequencies.size() < net.transitions().size())
    out.transition_frequencies.resize(net.transitions().size(), 0);
  for (const auto count : out.transition_frequencies)
    if (count) ++out.executed_transitions;
  double rarity = 0.0;
  for (const auto count : out.transition_frequencies)
    rarity += count ? 1.0 / std::sqrt(double(count)) : 1.0;
  out.generalization = out.transition_frequencies.empty()
                           ? 1.0
                           : 1.0 - rarity / double(out.transition_frequencies.size());
  return out;
}
GeneralizationResult evaluate_generalization(
    const EventLog& log, const ProcessTree& tree,
    GeneralizationVariant variant) {
  return evaluate_generalization(log, to_petri_net(tree), variant, {});
}

GeneralizationResult evaluate_generalization(
    const EventLog& log, const ProcessTree& tree,
    GeneralizationVariant variant, const GeneralizationOptions& options) {
  return evaluate_generalization(log, to_petri_net(tree), variant, options);
}

SimplicityResult evaluate_simplicity(const PetriNet& net,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  SimplicityResult out;
  out.variant = variant;
  if (variant == SimplicityVariant::ARC_DEGREE) {
    if (!std::isfinite(options.arc_degree_k) || options.arc_degree_k < 0.0) {
      out.computed = false;
      out.error = "invalid arc-degree k";
      return out;
    }
    std::size_t arcs = 0;
    for (const auto& transition : net.transitions()) {
      const std::unordered_set<std::size_t> inputs(
          transition.inputs.begin(), transition.inputs.end());
      const std::unordered_set<std::size_t> outputs(
          transition.outputs.begin(), transition.outputs.end());
      arcs += inputs.size() + outputs.size();
    }
    out.vertices = net.place_count() + net.transitions().size();
    const double mean_degree = out.vertices
                                   ? 2.0 * double(arcs) / double(out.vertices)
                                   : 0.0;
    out.complexity = std::max(mean_degree - options.arc_degree_k, 0.0);
    out.simplicity = 1.0 / (1.0 + out.complexity);
    out.value = out.simplicity;
    return out;
  }

  if (variant == SimplicityVariant::EXTENDED_CARDOSO) {
    std::size_t total = 0;
    for (std::size_t place = 0; place < net.place_count(); ++place) {
      std::set<std::vector<std::size_t>> downstream;
      for (const auto& transition : net.transitions()) {
        if (std::find(transition.inputs.begin(), transition.inputs.end(),
                      place) == transition.inputs.end())
          continue;
        auto places = transition.outputs;
        std::sort(places.begin(), places.end());
        places.erase(std::unique(places.begin(), places.end()), places.end());
        downstream.insert(std::move(places));
      }
      total += downstream.size();
    }
    out.vertices = net.place_count();
    out.complexity = out.value = double(total);
    out.simplicity = out.value;
    return out;
  }

  if (options.max_states == 0 ||
      (!options.initial_marking.empty() &&
       options.initial_marking.size() != net.place_count()) ||
      (!options.final_marking.empty() &&
       options.final_marking.size() != net.place_count())) {
    out.computed = false;
    out.error = "invalid marking dimension or max_states";
    return out;
  }
  const auto has_negative = [](const Marking& marking) {
    return std::any_of(marking.begin(), marking.end(),
                       [](int tokens) { return tokens < 0; });
  };
  if (has_negative(options.initial_marking) ||
      has_negative(options.final_marking)) {
    out.computed = false;
    out.error = "negative marking";
    return out;
  }

  Marking start = initial(net, options);
  if (options.initial_marking.empty()) {
    start.assign(net.place_count(), 0);
    for (std::size_t place = 0; place < net.place_count(); ++place) {
      const bool has_preset = std::any_of(
          net.transitions().begin(), net.transitions().end(),
          [place](const PetriTransition& transition) {
            return std::find(transition.outputs.begin(),
                             transition.outputs.end(), place) !=
                   transition.outputs.end();
          });
      if (!has_preset) start[place] = 1;
    }
  }

  std::vector<Marking> states{start};
  std::queue<std::size_t> queue;
  queue.push(0);
  std::unordered_map<Marking, std::size_t, MarkingHash> ids{{start, 0}};
  std::vector<std::vector<std::size_t>> adjacency(1);
  while (!queue.empty()) {
    const auto source = queue.front();
    queue.pop();
    for (const auto& transition : net.transitions()) {
      if (!enabled(transition, states[source])) continue;
      auto next = fire(transition, states[source]);
      const auto [it, inserted] = ids.emplace(next, states.size());
      if (inserted) {
        if (states.size() >= options.max_states) {
          out.exhausted = true;
          out.computed = false;
          out.error = "reachable-marking state cap exceeded";
          return out;
        }
        states.push_back(std::move(next));
        adjacency.emplace_back();
        queue.push(it->second);
      }
      // The reachability graph is directed-simple: transition labels do not
      // create parallel edges when they lead to the same marking.
      if (std::find(adjacency[source].begin(), adjacency[source].end(),
                    it->second) == adjacency[source].end()) {
        adjacency[source].push_back(it->second);
      }
    }
  }

  out.vertices = states.size();
  for (const auto& edges : adjacency) out.edges += edges.size();
  std::vector<std::vector<std::size_t>> reverse(out.vertices);
  for (std::size_t source = 0; source < out.vertices; ++source)
    for (const auto target : adjacency[source]) reverse[target].push_back(source);

  std::vector<char> seen(out.vertices, 0);
  std::vector<std::size_t> order;
  order.reserve(out.vertices);
  for (std::size_t root = 0; root < out.vertices; ++root) {
    if (seen[root]) continue;
    std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
    seen[root] = 1;
    while (!stack.empty()) {
      auto& [node, edge] = stack.back();
      if (edge < adjacency[node].size()) {
        const auto target = adjacency[node][edge++];
        if (!seen[target]) {
          seen[target] = 1;
          stack.push_back({target, 0});
        }
      } else {
        order.push_back(node);
        stack.pop_back();
      }
    }
  }

  std::fill(seen.begin(), seen.end(), char{0});
  std::size_t components = 0;
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (seen[*it]) continue;
    ++components;
    std::vector<std::size_t> stack{*it};
    seen[*it] = 1;
    while (!stack.empty()) {
      const auto node = stack.back();
      stack.pop_back();
      for (const auto target : reverse[node]) {
        if (!seen[target]) {
          seen[target] = 1;
          stack.push_back(target);
        }
      }
    }
  }
  out.strongly_connected_components = components;
  out.complexity = double(out.edges) - double(out.vertices) + double(components);
  out.value = out.complexity;
  out.simplicity = out.value;
  return out;
}

SimplicityResult evaluate_simplicity(const ProcessTree& tree,
                                     SimplicityVariant variant,
                                     const SimplicityOptions& options) {
  return evaluate_simplicity(to_petri_net(tree), variant, options);
}

CompositeEvaluationResult evaluate_composite(
    const EventLog& log, const PetriNet& net,
    const CompositeEvaluationOptions& options) {
  CompositeEvaluationResult out;
  const double total = options.fitness_weight + options.precision_weight +
                       options.simplicity_weight + options.generalization_weight;
  if (!std::isfinite(options.fitness_weight) ||
      !std::isfinite(options.precision_weight) ||
      !std::isfinite(options.simplicity_weight) ||
      !std::isfinite(options.generalization_weight) ||
      options.fitness_weight < 0.0 || options.precision_weight < 0.0 ||
      options.simplicity_weight < 0.0 || options.generalization_weight < 0.0 ||
      !std::isfinite(total) || total <= 0.0) {
    out.computed = false;
    out.error = "composite weights must be finite, non-negative, and have a positive total";
    return out;
  }
  out.fitness_weight = options.fitness_weight / total;
  out.precision_weight = options.precision_weight / total;
  out.simplicity_weight = options.simplicity_weight / total;
  out.generalization_weight = options.generalization_weight / total;

  const std::string activity_attribute = options.activity_attribute.empty()
                                             ? "concept:name"
                                             : options.activity_attribute;
  ReplayFitnessOptions fitness_options;
  fitness_options.activity_attribute = activity_attribute;
  PrecisionOptions precision_options;
  precision_options.activity_attribute = activity_attribute;
  GeneralizationOptions generalization_options;
  generalization_options.activity_attribute = activity_attribute;
  const auto fitness = evaluate_replay_fitness(
      log, net, ReplayFitnessVariant::TOKEN_BASED, fitness_options);
  const auto precision = evaluate_precision(
      log, net, PrecisionVariant::ETCONFORMANCE_TOKEN, precision_options);
  const auto generalization = evaluate_generalization(
      log, net, GeneralizationVariant::GENERALIZATION_TOKEN,
      generalization_options);
  const auto simplicity = evaluate_simplicity(
      net, SimplicityVariant::ARC_DEGREE);
  out.fitness = fitness.log_fitness;
  out.precision = precision.precision;
  out.generalization = generalization.generalization;
  out.simplicity = simplicity.simplicity;
  out.metrics_average_weight =
      out.fitness_weight * out.fitness +
      out.precision_weight * out.precision +
      out.generalization_weight * out.generalization +
      out.simplicity_weight * out.simplicity;
  const double denominator = out.fitness + out.precision;
  out.fscore = denominator > 0.0
                   ? 2.0 * out.fitness * out.precision / denominator
                   : 0.0;
  if (!fitness.computed || !precision.computed || !generalization.computed ||
      !simplicity.computed) {
    out.computed = false;
    if (!fitness.error.empty()) out.error = fitness.error;
    else if (!precision.error.empty()) out.error = precision.error;
    else if (!generalization.error.empty()) out.error = generalization.error;
    else out.error = simplicity.error;
  }
  return out;
}

CompositeEvaluationResult evaluate_composite(
    const EventLog& log, const ProcessTree& tree,
    const CompositeEvaluationOptions& options) {
  return evaluate_composite(log, to_petri_net(tree), options);
}

StochasticLanguage derive_stochastic_language(const EventLog& log,bool normalize) {
  StochasticLanguage out; std::map<std::vector<std::string>,double> counts; for(const auto&tr:log.traces){std::vector<std::string>a;for(const auto&e:tr.events)a.push_back(e.activity);counts[a]+=1.0;}
  for(auto&[a,w]:counts){out.traces.push_back({a,w});out.total_mass+=w;} if(normalize&&out.total_mass>0)for(auto&t:out.traces)t.weight/=out.total_mass; if(normalize&&out.total_mass>0)out.total_mass=1; return out;
}

EmdResult evaluate_emd(const StochasticLanguage& aa,const StochasticLanguage& bb,const EmdOptions& opt) {
  EmdResult out;
  out.variant = opt.variant;
  out.use_fast_emd = opt.use_fast_emd;
  switch (opt.variant) {
    case EmdVariant::EXACT:
    case EmdVariant::PYEMD:
      break;
    default:
      out.computed = false;
      out.error = "unsupported EMD variant";
      return out;
  }

  // Validate every source weight before consolidation. This prevents an
  // invalid negative entry from being hidden by a duplicate positive entry.
  // The ordered map gives duplicate traces a deterministic representation.
  auto consolidate=[](const StochasticLanguage& input,
                       StochasticLanguage& output) -> bool {
    std::map<std::vector<std::string>,double> c;
    for (const auto& trace : input.traces) {
      if (!std::isfinite(trace.weight) || trace.weight < 0.0) return false;
      c[trace.activities] += trace.weight;
    }
    output.traces.clear();
    output.total_mass = 0.0;
    for (const auto& [activities, weight] : c) {
      if (!std::isfinite(weight) || weight < 0.0) return false;
      output.traces.push_back({activities, weight});
      output.total_mass += weight;
    }
    return std::isfinite(output.total_mass);
  };
  StochasticLanguage a, b;
  if (!consolidate(aa, a) || !consolidate(bb, b)) {
    out.computed=false;
    out.error="invalid language weight";
    return out;
  }
  double ma=a.total_mass,mb=b.total_mass;
  if (a.traces.empty()&&b.traces.empty()){out.distance=0;return out;} if(ma<=0||mb<=0){out.computed=false;out.error="stochastic language has zero mass";return out;}
  if(std::abs(ma-mb)>1e-9){
    // Strict mode is deliberately checked before the default
    // normalization: callers opting into it asked to detect unequal mass,
    // not to silently rescale it away.
    if(opt.strict_equal_mass){out.computed=false;out.error="masses must be equal";return out;}
    // The accelerated implementation normalizes unequal histograms before
    // solving. Its fallback requires equal totals, even when the caller's
    // normalization extension is enabled.
    if(opt.variant == EmdVariant::PYEMD && !opt.use_fast_emd){
      out.computed=false;out.error="masses must be equal";return out;
    }
    if(opt.normalize_masses){for(auto&t:a.traces)t.weight/=ma;for(auto&t:b.traces)t.weight/=mb;ma=mb=1;out.normalized=true;}
    else {out.computed=false;out.error="masses must be equal";return out;}
  }
  // Both selectors use normalized Levenshtein by default. A native distance
  // callback is authoritative for either selector when supplied.
  const auto dist = opt.distance ? opt.distance
                                 : DistanceFunction(normalized_levenshtein_distance);
  const std::size_t n=a.traces.size(),m=b.traces.size();
  std::vector<std::vector<double>> cost(n,std::vector<double>(m)); for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<m;++j){cost[i][j]=dist(a.traces[i].activities,b.traces[j].activities);if(!std::isfinite(cost[i][j])||cost[i][j]<0){out.computed=false;out.error="invalid distance";return out;}}
  struct Edge{int to,rev;double cap,cost;}; const int S=0,I=1,J=int(1+n),T=J+int(m),N=T+1; std::vector<std::vector<Edge>> g(N);
  auto add=[&](int u,int v,double c,double w){g[u].push_back({v,(int)g[v].size(),c,w});g[v].push_back({u,(int)g[u].size()-1,0,-w});};
  for(std::size_t i=0;i<n;++i)add(S,I+int(i),a.traces[i].weight,0); for(std::size_t j=0;j<m;++j)add(J+int(j),T,b.traces[j].weight,0); for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<m;++j)add(I+int(i),J+int(j),std::numeric_limits<double>::infinity(),cost[i][j]);
  double total=0,flow=0,target=std::min(ma,mb); while(flow+1e-10<target){std::vector<double>d(N,std::numeric_limits<double>::infinity());std::vector<int>pv(N,-1),pe(N,-1);d[S]=0;for(int it=0;it<N-1;++it){bool ch=false;for(int u=0;u<N;++u)if(std::isfinite(d[u]))for(int k=0;k<(int)g[u].size();++k){auto&e=g[u][k];if(e.cap>1e-12&&d[e.to]>d[u]+e.cost+1e-12){d[e.to]=d[u]+e.cost;pv[e.to]=u;pe[e.to]=k;ch=true;}}if(!ch)break;}if(pv[T]<0){out.computed=false;out.error="transportation solve failed";return out;}double f=target-flow;for(int v=T;v!=S;v=pv[v])f=std::min(f,g[pv[v]][pe[v]].cap);for(int v=T;v!=S;v=pv[v]){auto&e=g[pv[v]][pe[v]];e.cap-=f;g[v][e.rev].cap+=f;}flow+=f;total+=f*d[T];}
  out.distance=total; return out;
}
double earth_movers_distance(const StochasticLanguage&a,const StochasticLanguage&b,const EmdOptions&o){auto r=evaluate_emd(a,b,o);if(!r.computed) throw std::invalid_argument(r.error);return r.distance;}

} // namespace minera
