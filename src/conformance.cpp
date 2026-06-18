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

std::vector<Variant> collect_variants(const EventLog& log,
                                      const Replayer& replayer) {
  std::vector<Variant> variants;
  std::unordered_map<std::string, std::size_t> index;
  for (const auto& trace : log.traces) {
    std::string key;
    Variant variant;
    for (const auto& event : trace.events) {
      variant.ids.push_back(replayer.activity_id(event.activity));
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

}  // namespace minera
