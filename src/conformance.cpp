#include "minera/conformance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {
namespace {

using Marking = std::vector<int>;

// Bounds the silent-transition search so a pathological model can never make a
// single replay step loop forever; reachable silent routing is shallow in the
// block-structured nets the inductive miner produces.
constexpr std::size_t kSilentSearchCap = 4096;

std::string encode(const Marking& marking) {
  std::string key;
  key.reserve(marking.size() * 3);
  for (const int tokens : marking) {
    key += std::to_string(tokens);  // decimal text: no overflow, no aliasing
    key.push_back(',');
  }
  return key;
}

// Holds the net plus the lookups token replay needs and drives both the
// fitness and precision passes over a single shared translation.
class Replayer {
 public:
  explicit Replayer(const PetriNet& net) : net_(net) {
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
    transitions_by_activity_.resize(net_.activities().size());
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      const auto& transition = net_.transitions()[t];
      if (transition.silent()) {
        silent_transitions_.push_back(t);
      } else {
        transitions_by_activity_[*transition.activity].push_back(t);
      }
    }
  }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      const std::string& name) const {
    const auto it = name_to_activity_.find(name);
    return it == name_to_activity_.end() ? std::nullopt
                                         : std::optional<std::size_t>(it->second);
  }

  // Token-replay bookkeeping for one trace.
  struct Counts {
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;
    std::uint64_t missing = 0;
    std::uint64_t remaining = 0;

    [[nodiscard]] double fitness() const {
      const double consume_term = consumed == 0 ? 0.0 : double(missing) / double(consumed);
      const double produce_term = produced == 0 ? 0.0 : double(remaining) / double(produced);
      return 0.5 * (1.0 - consume_term) + 0.5 * (1.0 - produce_term);
    }
  };

  // Replays a trace (activity ids, std::nullopt = activity absent from the
  // model) and returns the token counts. Perfect replay yields missing == 0
  // and remaining == 0.
  [[nodiscard]] Counts replay(
      const std::vector<std::optional<std::size_t>>& trace) const {
    Marking marking(net_.place_count(), 0);
    marking[net_.source()] = 1;
    Counts counts;
    counts.produced = 1;  // the initial token on the source place

    for (const auto& step : trace) {
      if (!step || transitions_by_activity_[*step].empty()) {
        // An activity the model has no transition for: an unavoidable log-move
        // deviation. Count it once and carry on with the current marking.
        ++counts.missing;
        ++counts.consumed;
        continue;
      }
      const auto& targets = transitions_by_activity_[*step];
      route_silently(marking, targets, counts);
      fire(marking, pick_enabled(marking, targets), counts);
    }

    // Route whatever is left toward the sink, then consume the final token.
    route_silently_to(marking, net_.sink(), counts);
    ++counts.consumed;
    if (marking[net_.sink()] > 0) {
      --marking[net_.sink()];
    } else {
      ++counts.missing;
    }
    for (const int tokens : marking) {
      if (tokens > 0) counts.remaining += static_cast<std::uint64_t>(tokens);
    }
    return counts;
  }

  // The marking reached after replaying the first `length` steps of a trace,
  // used to position the model for the precision pass.
  [[nodiscard]] Marking marking_after(
      const std::vector<std::optional<std::size_t>>& trace,
      std::size_t length) const {
    Marking marking(net_.place_count(), 0);
    marking[net_.source()] = 1;
    Counts scratch;
    for (std::size_t i = 0; i < length && i < trace.size(); ++i) {
      if (!trace[i] || transitions_by_activity_[*trace[i]].empty()) continue;
      const auto& targets = transitions_by_activity_[*trace[i]];
      route_silently(marking, targets, scratch);
      fire(marking, pick_enabled(marking, targets), scratch);
    }
    return marking;
  }

  // Activities whose transitions can be enabled from `marking` (possibly after
  // firing silent transitions) — the model's allowed next observable steps.
  [[nodiscard]] std::unordered_set<std::size_t> enabled_activities(
      const Marking& marking) const {
    std::unordered_set<std::size_t> enabled;
    for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
      if (silent_path(marking, [&](const Marking& m) {
            return any_enabled(m, transitions_by_activity_[a]);
          })) {
        enabled.insert(a);
      }
    }
    return enabled;
  }

 private:
  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    for (const std::size_t place : net_.transitions()[t].inputs) {
      if (marking[place] <= 0) return false;
    }
    return true;
  }

  [[nodiscard]] bool any_enabled(const Marking& marking,
                                 const std::vector<std::size_t>& ts) const {
    return std::any_of(ts.begin(), ts.end(),
                       [&](std::size_t t) { return enabled(marking, t); });
  }

  [[nodiscard]] std::size_t pick_enabled(
      const Marking& marking, const std::vector<std::size_t>& targets) const {
    for (const std::size_t t : targets) {
      if (enabled(marking, t)) return t;
    }
    return targets.front();  // forced firing: missing tokens are counted in fire()
  }

  void fire(Marking& marking, std::size_t t, Counts& counts) const {
    const auto& transition = net_.transitions()[t];
    for (const std::size_t place : transition.inputs) {
      if (marking[place] > 0) {
        --marking[place];
      } else {
        ++counts.missing;  // token had to be created to fire
      }
      ++counts.consumed;
    }
    for (const std::size_t place : transition.outputs) {
      ++marking[place];
      ++counts.produced;
    }
  }

  // Breadth-first search over silent-transition firings for a marking meeting
  // `goal`. Returns the silent transitions to fire, or std::nullopt if `goal`
  // is unreachable without creating tokens.
  [[nodiscard]] std::optional<std::vector<std::size_t>> silent_path(
      const Marking& start,
      const std::function<bool(const Marking&)>& goal) const {
    if (goal(start)) return std::vector<std::size_t>{};

    std::unordered_set<std::string> seen{encode(start)};
    std::queue<std::pair<Marking, std::vector<std::size_t>>> frontier;
    frontier.push({start, {}});
    std::size_t expanded = 0;

    while (!frontier.empty() && expanded++ < kSilentSearchCap) {
      auto [marking, path] = std::move(frontier.front());
      frontier.pop();
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(marking, s)) continue;
        Marking next = marking;
        Counts scratch;
        fire(next, s, scratch);
        if (!seen.insert(encode(next)).second) continue;
        std::vector<std::size_t> extended = path;
        extended.push_back(s);
        if (goal(next)) return extended;
        frontier.push({std::move(next), std::move(extended)});
      }
    }
    return std::nullopt;
  }

  // Fires silent transitions (if a no-cost path exists) so that one of
  // `targets` becomes enabled, updating the marking and counts in place.
  void route_silently(Marking& marking,
                      const std::vector<std::size_t>& targets,
                      Counts& counts) const {
    if (any_enabled(marking, targets)) return;
    const auto path = silent_path(
        marking, [&](const Marking& m) { return any_enabled(m, targets); });
    if (!path) return;  // no silent route; the visible firing will create tokens
    for (const std::size_t s : *path) fire(marking, s, counts);
  }

  void route_silently_to(Marking& marking,
                         std::size_t place,
                         Counts& counts) const {
    if (marking[place] > 0) return;
    const auto path =
        silent_path(marking, [&](const Marking& m) { return m[place] > 0; });
    if (!path) return;
    for (const std::size_t s : *path) fire(marking, s, counts);
  }

  const PetriNet& net_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  std::vector<std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
};

// Collapses the log into distinct activity-id sequences with occurrence counts,
// preserving first-seen order so results are deterministic.
struct Variant {
  std::vector<std::optional<std::size_t>> ids;  // std::nullopt = absent activity
  std::vector<std::string> names;
  std::uint64_t count = 0;
};

// `resolver.activity_id(name)` maps an activity name to its model id (or
// std::nullopt when absent), so this works for any net wrapper.
template <typename Resolver>
std::vector<Variant> collect_variants(const EventLog& log,
                                      const Resolver& resolver) {
  std::vector<Variant> variants;
  std::unordered_map<std::string, std::size_t> index;
  for (const auto& trace : log.traces) {
    std::string key;
    Variant variant;
    for (const auto& event : trace.events) {
      variant.ids.push_back(resolver.activity_id(event.activity));
      variant.names.push_back(event.activity);
      key.push_back('\x1f');
      key += event.activity;
    }
    auto [it, inserted] = index.emplace(std::move(key), variants.size());
    if (inserted) {
      variant.count = 1;
      variants.push_back(std::move(variant));
    } else {
      ++variants[it->second].count;
    }
  }
  return variants;
}

double compute_precision(const std::vector<Variant>& variants,
                         const Replayer& replayer) {
  // Prefix automaton: each distinct prefix records how many traces pass through
  // it and which activities they go on to take.
  struct State {
    std::uint64_t freq = 0;
    std::unordered_set<std::size_t> observed;
  };
  std::map<std::vector<std::optional<std::size_t>>, State> states;
  for (const auto& variant : variants) {
    for (std::size_t k = 0; k <= variant.ids.size(); ++k) {
      std::vector<std::optional<std::size_t>> prefix(variant.ids.begin(),
                                                     variant.ids.begin() + k);
      auto& state = states[prefix];
      state.freq += variant.count;
      if (k < variant.ids.size() && variant.ids[k]) {
        state.observed.insert(*variant.ids[k]);
      }
    }
  }

  std::uint64_t enabled_total = 0;
  std::uint64_t escaping_total = 0;
  for (const auto& [prefix, state] : states) {
    const Marking marking = replayer.marking_after(prefix, prefix.size());
    const auto enabled = replayer.enabled_activities(marking);
    std::uint64_t escaping = 0;
    for (const std::size_t a : enabled) {
      if (state.observed.find(a) == state.observed.end()) ++escaping;
    }
    enabled_total += state.freq * enabled.size();
    escaping_total += state.freq * escaping;
  }
  return enabled_total == 0 ? 1.0
                            : 1.0 - double(escaping_total) / double(enabled_total);
}

// Bounds the alignment search so a pathological model can never run forever; the
// bounded, block-structured nets the inductive miner produces reach the final
// marking well within this many state expansions.
constexpr std::size_t kAlignSearchCap = 200000;

// Computes optimal alignments by a uniform-cost (Dijkstra) search over the
// synchronous product of the trace and the workflow net. A search state is a
// (marking, trace-position) pair; the goal is the final marking with the whole
// trace consumed. Edge costs follow the standard alignment cost function:
// synchronous and silent moves are free, log-only and visible model-only moves
// cost one each.
class Aligner {
 public:
  explicit Aligner(const PetriNet& net) : net_(net) {
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
    final_ = Marking(net_.place_count(), 0);
    final_[net_.sink()] = 1;
    if (const auto cost = shortest_model_run()) {
      reachable_ = true;
      empty_run_cost_ = *cost;
    }
  }

  // Whether the net's final marking is reachable from its initial marking — the
  // minimum bar for it to be a workflow net the aligner can score.
  [[nodiscard]] bool reachable() const { return reachable_; }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      const std::string& name) const {
    const auto it = name_to_activity_.find(name);
    return it == name_to_activity_.end() ? std::nullopt
                                         : std::optional<std::size_t>(it->second);
  }

  // The fewest visible activities any complete model run fires — the model-move
  // half of the worst-case alignment cost used to normalize fitness.
  [[nodiscard]] std::uint64_t empty_run_cost() const { return empty_run_cost_; }

  struct Outcome {
    std::vector<AlignmentMove> moves;
    std::uint64_t cost = 0;
    bool capped = false;  // search hit its bound before reaching the goal
  };

  // Optimal alignment of one trace (activity ids; std::nullopt = activity absent
  // from the model) carrying the matching activity names for display.
  [[nodiscard]] Outcome align(
      const std::vector<std::optional<std::size_t>>& ids,
      const std::vector<std::string>& names) const {
    const std::size_t len = ids.size();
    Marking start(net_.place_count(), 0);
    start[net_.source()] = 1;

    // How a state was reached, for path reconstruction. A silent step carries no
    // visible move; its `name` is empty and it is dropped from the alignment.
    struct Recorded {
      std::string prev;
      AlignmentMoveType type = AlignmentMoveType::kModel;
      std::string name;
      bool silent = false;
    };

    struct Node {
      std::uint64_t cost;
      std::uint64_t seq;  // FIFO tie-break keeps the search deterministic
      Marking marking;
      std::size_t pos;
      bool operator>(const Node& other) const {
        return cost != other.cost ? cost > other.cost : seq > other.seq;
      }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<>> frontier;
    std::unordered_map<std::string, std::uint64_t> best;
    std::unordered_map<std::string, Recorded> came;
    std::uint64_t seq = 0;

    const auto key_of = [](const Marking& m, std::size_t pos) {
      return encode(m) + '#' + std::to_string(pos);
    };
    const std::string start_key = key_of(start, 0);
    best[start_key] = 0;
    frontier.push({0, seq++, start, 0});

    const auto relax = [&](const Marking& m, std::size_t pos,
                           std::uint64_t cost, const std::string& prev,
                           AlignmentMoveType type, std::string name,
                           bool silent) {
      const std::string key = key_of(m, pos);
      const auto it = best.find(key);
      if (it != best.end() && it->second <= cost) return;
      best[key] = cost;
      came[key] = Recorded{prev, type, std::move(name), silent};
      frontier.push({cost, seq++, m, pos});
    };

    std::string goal_key;
    std::size_t expanded = 0;
    while (!frontier.empty() && expanded < kAlignSearchCap) {
      Node node = std::move(const_cast<Node&>(frontier.top()));
      frontier.pop();
      const std::string key = key_of(node.marking, node.pos);
      const auto it = best.find(key);
      if (it != best.end() && node.cost > it->second) continue;  // stale entry
      ++expanded;

      if (node.pos == len && node.marking == final_) {
        goal_key = key;
        break;
      }

      // Log move: the trace advances while the model stands still.
      if (node.pos < len) {
        relax(node.marking, node.pos + 1, node.cost + 1, key,
              AlignmentMoveType::kLog, names[node.pos], false);
      }

      // Model moves (and the synchronous move when the labels agree).
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(node.marking, t)) continue;
        const auto& transition = net_.transitions()[t];
        Marking next = fire(node.marking, t);
        if (transition.silent()) {
          relax(next, node.pos, node.cost, key, AlignmentMoveType::kModel,
                std::string{}, true);
          continue;
        }
        const std::string& name = net_.activities()[*transition.activity];
        relax(next, node.pos, node.cost + 1, key, AlignmentMoveType::kModel,
              name, false);
        if (node.pos < len && ids[node.pos] &&
            *ids[node.pos] == *transition.activity) {
          relax(next, node.pos + 1, node.cost, key,
                AlignmentMoveType::kSynchronous, name, false);
        }
      }
    }

    Outcome outcome;
    if (goal_key.empty()) {
      // Search capped out (only possible for unsound models): fall back to the
      // worst-case alignment so callers still get a well-formed result, and flag
      // that the numbers are not trustworthy.
      for (std::size_t i = 0; i < len; ++i) {
        outcome.moves.push_back({AlignmentMoveType::kLog, names[i]});
      }
      outcome.cost = static_cast<std::uint64_t>(len) + empty_run_cost_;
      outcome.capped = true;
      return outcome;
    }

    outcome.cost = best[goal_key];
    for (std::string key = goal_key; key != start_key;) {
      const Recorded& rec = came[key];
      if (!rec.silent) {
        outcome.moves.push_back({rec.type, rec.name});
      }
      key = rec.prev;
    }
    std::reverse(outcome.moves.begin(), outcome.moves.end());
    return outcome;
  }

 private:
  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    for (const std::size_t place : net_.transitions()[t].inputs) {
      if (marking[place] <= 0) return false;
    }
    return true;
  }

  [[nodiscard]] Marking fire(const Marking& marking, std::size_t t) const {
    Marking next = marking;
    const auto& transition = net_.transitions()[t];
    for (const std::size_t place : transition.inputs) --next[place];
    for (const std::size_t place : transition.outputs) ++next[place];
    return next;
  }

  // Cheapest count of visible transitions on any run from the initial to the
  // final marking, found by the same uniform-cost search with no trace, or
  // std::nullopt when the final marking is unreachable within the search bound.
  [[nodiscard]] std::optional<std::uint64_t> shortest_model_run() const {
    Marking start(net_.place_count(), 0);
    start[net_.source()] = 1;
    std::priority_queue<std::pair<std::uint64_t, Marking>,
                        std::vector<std::pair<std::uint64_t, Marking>>,
                        std::greater<>>
        frontier;
    std::unordered_map<std::string, std::uint64_t> best{{encode(start), 0}};
    frontier.push({0, start});
    std::size_t expanded = 0;
    while (!frontier.empty() && expanded++ < kAlignSearchCap) {
      auto [cost, marking] = std::move(frontier.top());
      frontier.pop();
      const auto it = best.find(encode(marking));
      if (it != best.end() && cost > it->second) continue;
      if (marking == final_) return cost;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(marking, t)) continue;
        Marking next = fire(marking, t);
        const std::uint64_t next_cost =
            cost + (net_.transitions()[t].silent() ? 0 : 1);
        const auto found = best.find(encode(next));
        if (found != best.end() && found->second <= next_cost) continue;
        best[encode(next)] = next_cost;
        frontier.push({next_cost, std::move(next)});
      }
    }
    return std::nullopt;  // final marking unreachable: the net is not a sound WF-net
  }

  const PetriNet& net_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  Marking final_;
  bool reachable_ = false;
  std::uint64_t empty_run_cost_ = 0;
};

}  // namespace

ConformanceResult check_conformance(const EventLog& event_log,
                                    const ProcessTree& tree) {
  const PetriNet net = to_petri_net(tree);
  const Replayer replayer(net);
  const auto variants = collect_variants(event_log, replayer);

  ConformanceResult result;
  result.trace_count = event_log.trace_count();
  result.fitness = 1.0;
  result.precision = 1.0;

  for (const auto& variant : variants) {
    const auto counts = replayer.replay(variant.ids);
    result.produced += counts.produced * variant.count;
    result.consumed += counts.consumed * variant.count;
    result.missing += counts.missing * variant.count;
    result.remaining += counts.remaining * variant.count;
    if (counts.missing == 0 && counts.remaining == 0) {
      result.fitting_traces += variant.count;
    }
    result.variants.push_back(
        VariantConformance{variant.names, variant.count, counts.fitness()});
  }

  if (result.consumed > 0 || result.produced > 0) {
    const double consume_term =
        result.consumed == 0 ? 0.0 : double(result.missing) / double(result.consumed);
    const double produce_term =
        result.produced == 0 ? 0.0 : double(result.remaining) / double(result.produced);
    result.fitness = 0.5 * (1.0 - consume_term) + 0.5 * (1.0 - produce_term);
  }

  result.precision = compute_precision(variants, replayer);

  // Surface the least-fitting variants first; break ties by frequency so the
  // most impactful deviations lead.
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantConformance& a, const VariantConformance& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net) {
  const Aligner aligner(net);

  AlignmentResult result;
  result.trace_count = event_log.trace_count();

  if (!aligner.reachable()) {
    // Not a sound workflow net (e.g. an Alpha net whose sink is unreachable):
    // report that alignments could not be computed rather than fake numbers.
    result.computed = false;
    return result;
  }

  const auto variants = collect_variants(event_log, aligner);
  std::uint64_t total_worst = 0;
  for (const auto& variant : variants) {
    const auto outcome = aligner.align(variant.ids, variant.names);
    if (outcome.capped) {
      // The search could not align a trace within its bound — the net admits
      // unbounded behavior, so the results are not trustworthy.
      result.computed = false;
      result.variants.clear();
      result.fitting_traces = 0;
      result.total_cost = 0;
      result.fitness = 1.0;
      return result;
    }
    // Worst case: every event as a log move plus the cheapest complete model
    // run. A perfectly fitting trace has cost 0 and fitness 1.
    const std::uint64_t worst =
        static_cast<std::uint64_t>(variant.names.size()) +
        aligner.empty_run_cost();
    const double fitness =
        worst == 0 ? 1.0 : 1.0 - double(outcome.cost) / double(worst);

    result.total_cost += outcome.cost * variant.count;
    total_worst += worst * variant.count;
    if (outcome.cost == 0) result.fitting_traces += variant.count;

    result.variants.push_back(VariantAlignment{
        variant.names, variant.count, outcome.moves, outcome.cost, fitness});
  }

  result.fitness =
      total_worst == 0 ? 1.0 : 1.0 - double(result.total_cost) / double(total_worst);

  // Surface the costliest variants first; break ties by frequency so the most
  // impactful deviations lead.
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantAlignment& a, const VariantAlignment& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const ProcessTree& tree) {
  // A process tree always translates to a sound workflow net, so this path
  // never reports computed == false.
  return check_alignment(event_log, to_petri_net(tree));
}

}  // namespace minera
