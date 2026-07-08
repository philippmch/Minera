#include "minera/conformance.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {
namespace {

using Marking = std::vector<int>;

// Bounds the silent-transition search so a pathological model can never make a
// single replay step loop forever; reachable silent routing is shallow in the
// block-structured nets the inductive miner produces.
constexpr std::size_t kSilentSearchCap = 4096;

// FNV-1a hash of a token vector, used to key every marking-indexed search
// (token replay's silent routing, the precision pass, and alignment) directly
// on the marking instead of formatting a string per state — these searches
// visit far too many states for that to be cheap.
struct MarkingHash {
  std::size_t operator()(const std::vector<int>& v) const noexcept {
    std::size_t h = 1469598103934665603ull;
    for (const int x : v) {
      h = (h ^ static_cast<std::size_t>(static_cast<unsigned>(x))) *
          1099511628211ull;
    }
    return h;
  }
};

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
    build_place_closure();
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
  // and remaining == 0. When `executions` is given (sized to the net's
  // transition count) each transition the replay fires — silent routing
  // included — is tallied into it, feeding generalization.
  [[nodiscard]] Counts replay(
      const std::vector<std::optional<std::size_t>>& trace,
      std::vector<std::uint64_t>* executions = nullptr) const {
    Marking marking = initial_marking();
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
      const Step& result = cached_step(marking, *step);
      marking = result.marking;
      counts.produced += result.produced;
      counts.consumed += result.consumed;
      counts.missing += result.missing;
      if (executions) {
        for (const std::size_t t : result.fired) ++(*executions)[t];
      }
    }

    // Route whatever is left toward the sink, then consume the final token.
    const Step& finish = cached_finish(marking);
    marking = finish.marking;
    counts.produced += finish.produced;
    counts.consumed += finish.consumed;
    counts.missing += finish.missing;
    if (executions) {
      for (const std::size_t t : finish.fired) ++(*executions)[t];
    }
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

  // Like replay(), but only for traces that replay cleanly, and returning what
  // fired instead of token counts: one entry per consumed event holding the
  // transitions fired for it — silent routing first, the event's visible
  // transition last — plus one final entry for the routing to the sink.
  // nullopt as soon as the replay deviates (an activity the net lacks, a
  // missing token, or leftover tokens at the end), because a forced replay's
  // firing sequence would not be a real path through the model.
  [[nodiscard]] std::optional<std::vector<std::vector<std::size_t>>>
  clean_firing_sequence(
      const std::vector<std::optional<std::size_t>>& trace) const {
    Marking marking = initial_marking();
    std::vector<std::vector<std::size_t>> steps;
    steps.reserve(trace.size() + 1);
    for (const auto& step : trace) {
      if (!step || transitions_by_activity_[*step].empty()) {
        return std::nullopt;
      }
      const Step& result = cached_step(marking, *step);
      if (result.missing != 0) return std::nullopt;
      marking = result.marking;
      steps.push_back(result.fired);
    }
    const Step& finish = cached_finish(marking);
    if (finish.missing != 0) return std::nullopt;
    marking = finish.marking;
    if (marking[net_.sink()] <= 0) return std::nullopt;
    --marking[net_.sink()];
    for (const int tokens : marking) {
      if (tokens > 0) return std::nullopt;
    }
    steps.push_back(finish.fired);
    return steps;
  }

  // The marking replay starts from: one token on the source place.
  [[nodiscard]] Marking initial_marking() const {
    Marking marking(net_.place_count(), 0);
    marking[net_.source()] = 1;
    return marking;
  }

  // Advances `marking` by replaying one activity — the same step `replay`
  // takes, so walking a trace with `advance` reaches exactly the markings a
  // full replay would. Lets the precision pass extend a parent prefix's
  // marking by one step instead of replaying every prefix from scratch.
  void advance(Marking& marking, std::optional<std::size_t> activity) const {
    if (!activity || transitions_by_activity_[*activity].empty()) return;
    marking = cached_step(marking, *activity).marking;
  }

  // Activities whose transitions can be enabled from `marking` (possibly after
  // firing silent transitions) — the model's allowed next observable steps.
  [[nodiscard]] std::unordered_set<std::size_t> enabled_activities(
      const Marking& marking) const {
    // When every silent transition moves a single token (one input, one
    // output) and every visible transition needs a single place, tokens
    // wander the silent structure independently and the closure is exact and
    // cheap: an activity is allowed next iff some occupied place silently
    // reaches one of its input places. This covers the heuristic miner's
    // one-token nets — whose interleaved multi-token closures are far too
    // large to enumerate (and used to be cut off by the search cap, slightly
    // understating what the model allows).
    if (place_closure_exact_) {
      std::unordered_set<std::size_t> acts;
      for (std::size_t p = 0; p < marking.size(); ++p) {
        if (marking[p] <= 0) continue;
        for (const std::size_t a : place_enables_[p]) acts.insert(a);
      }
      return acts;
    }

    // General nets: an activity is allowed next if a transition for it is
    // enabled at this marking or at any marking reachable by firing silent
    // transitions. A single breadth-first silent closure (bounded so a
    // pathological model cannot loop forever) collects them all at once,
    // rather than re-walking the same silent space once per activity.
    std::unordered_set<std::size_t> acts;
    const auto collect = [&](const Marking& m) {
      for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
        if (acts.count(a)) continue;
        if (any_enabled(m, transitions_by_activity_[a])) acts.insert(a);
      }
    };
    collect(marking);

    std::unordered_set<Marking, MarkingHash> seen{marking};
    std::queue<Marking> frontier;
    frontier.push(marking);
    std::size_t expanded = 0;
    while (!frontier.empty() && expanded++ < kSilentSearchCap) {
      const Marking current = std::move(frontier.front());
      frontier.pop();
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(current, s)) continue;
        Marking next = current;
        Counts scratch;
        fire(next, s, scratch);
        if (!seen.insert(next).second) continue;
        collect(next);
        frontier.push(std::move(next));
      }
    }
    return acts;
  }

 private:
  // One replay step's outcome — the marking it leads to and the token
  // bookkeeping it added. Replay is a deterministic function of the marking
  // and the activity, and real logs repeat the same steps constantly (shared
  // variant prefixes, recurring states), so each distinct step is computed
  // (with its silent-routing search) once and replayed from the cache after
  // that. The fitness and precision passes share the caches through the one
  // Replayer a conformance check builds.
  struct Step {
    Marking marking;
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;
    std::uint64_t missing = 0;
    std::vector<std::size_t> fired;  // transitions fired, silent routing included
  };

  struct StepKeyHash {
    std::size_t operator()(
        const std::pair<Marking, std::size_t>& key) const noexcept {
      return MarkingHash{}(key.first) ^ (key.second * 0x9e3779b97f4a7c15ull);
    }
  };

  [[nodiscard]] const Step& cached_step(const Marking& marking,
                                        std::size_t activity) const {
    const auto [it, inserted] =
        step_cache_.try_emplace(std::make_pair(marking, activity));
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      const auto& targets = transitions_by_activity_[activity];
      route_silently(step.marking, targets, counts, &step.fired);
      const std::size_t chosen = pick_enabled(step.marking, targets);
      fire(step.marking, chosen, counts);
      step.fired.push_back(chosen);
      step.produced = counts.produced;
      step.consumed = counts.consumed;
      step.missing = counts.missing;
    }
    return it->second;
  }

  // The end-of-trace routing toward the sink, cached like a step.
  [[nodiscard]] const Step& cached_finish(const Marking& marking) const {
    const auto [it, inserted] = finish_cache_.try_emplace(marking);
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      route_silently_to(step.marking, net_.sink(), counts, &step.fired);
      step.produced = counts.produced;
      step.consumed = counts.consumed;
      step.missing = counts.missing;
    }
    return it->second;
  }

  // Detects whether the exact per-place closure applies (see
  // enabled_activities) and precomputes, for every place, which activities a
  // single token there can eventually enable through silent firings.
  void build_place_closure() {
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      const auto& transition = net_.transitions()[t];
      const bool ok = transition.silent()
                          ? transition.inputs.size() == 1 &&
                                transition.outputs.size() == 1
                          : transition.inputs.size() == 1;
      if (!ok) return;  // fall back to the breadth-first closure
    }

    const std::size_t places = net_.place_count();
    std::vector<std::vector<std::size_t>> silent_out(places);
    for (const std::size_t s : silent_transitions_) {
      silent_out[net_.transitions()[s].inputs.front()].push_back(
          net_.transitions()[s].outputs.front());
    }
    std::vector<std::vector<std::size_t>> enables_at(places);
    for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
      for (const std::size_t t : transitions_by_activity_[a]) {
        enables_at[net_.transitions()[t].inputs.front()].push_back(a);
      }
    }

    place_enables_.resize(places);
    std::vector<char> seen(places);
    std::vector<std::size_t> stack;
    std::vector<char> acts(transitions_by_activity_.size());
    for (std::size_t p = 0; p < places; ++p) {
      std::fill(seen.begin(), seen.end(), 0);
      std::fill(acts.begin(), acts.end(), 0);
      seen[p] = 1;
      stack.assign(1, p);
      while (!stack.empty()) {
        const std::size_t q = stack.back();
        stack.pop_back();
        for (const std::size_t a : enables_at[q]) acts[a] = 1;
        for (const std::size_t r : silent_out[q]) {
          if (!seen[r]) {
            seen[r] = 1;
            stack.push_back(r);
          }
        }
      }
      for (std::size_t a = 0; a < acts.size(); ++a) {
        if (acts[a]) place_enables_[p].push_back(a);
      }
    }
    place_closure_exact_ = true;
  }

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
  // is unreachable without creating tokens. Visited states live in an arena
  // with parent links, so the found path is reconstructed once at the end
  // instead of being copied into every frontier entry.
  template <typename Goal>
  [[nodiscard]] std::optional<std::vector<std::size_t>> silent_path(
      const Marking& start, const Goal& goal) const {
    if (goal(start)) return std::vector<std::size_t>{};

    constexpr std::size_t kStart = std::numeric_limits<std::size_t>::max();
    struct Visited {
      Marking marking;
      std::size_t transition;
      std::size_t parent;
    };
    std::vector<Visited> arena;
    std::unordered_set<Marking, MarkingHash> seen{start};
    std::queue<std::size_t> frontier;
    frontier.push(kStart);
    std::size_t expanded = 0;

    const auto reconstruct = [&](std::size_t index) {
      std::vector<std::size_t> path;
      for (std::size_t i = index; i != kStart; i = arena[i].parent) {
        path.push_back(arena[i].transition);
      }
      std::reverse(path.begin(), path.end());
      return path;
    };

    while (!frontier.empty() && expanded++ < kSilentSearchCap) {
      const std::size_t index = frontier.front();
      frontier.pop();
      // Copy: growing the arena below would invalidate a reference.
      const Marking current = index == kStart ? start : arena[index].marking;
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(current, s)) continue;
        Marking next = current;
        Counts scratch;
        fire(next, s, scratch);
        if (!seen.insert(next).second) continue;
        const bool done = goal(next);
        arena.push_back(Visited{std::move(next), s, index});
        if (done) return reconstruct(arena.size() - 1);
        frontier.push(arena.size() - 1);
      }
    }
    return std::nullopt;
  }

  // Fires silent transitions (if a no-cost path exists) so that one of
  // `targets` becomes enabled, updating the marking and counts in place. The
  // firings along the chosen path are appended to `fired` when given.
  void route_silently(Marking& marking,
                      const std::vector<std::size_t>& targets,
                      Counts& counts,
                      std::vector<std::size_t>* fired = nullptr) const {
    if (any_enabled(marking, targets)) return;
    const auto path = silent_path(
        marking, [&](const Marking& m) { return any_enabled(m, targets); });
    if (!path) return;  // no silent route; the visible firing will create tokens
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  void route_silently_to(Marking& marking,
                         std::size_t place,
                         Counts& counts,
                         std::vector<std::size_t>* fired = nullptr) const {
    if (marking[place] > 0) return;
    const auto path =
        silent_path(marking, [&](const Marking& m) { return m[place] > 0; });
    if (!path) return;
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  const PetriNet& net_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  std::vector<std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
  mutable std::unordered_map<std::pair<Marking, std::size_t>, Step, StepKeyHash>
      step_cache_;
  mutable std::unordered_map<Marking, Step, MarkingHash> finish_cache_;
  bool place_closure_exact_ = false;
  std::vector<std::vector<std::size_t>> place_enables_;
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
  // Prefix automaton as a trie: each node is a distinct prefix and records how
  // many traces pass through it and which activities they go on to take.
  // Activities absent from the model all collapse onto one edge key, matching
  // how replay treats them (the marking does not move).
  constexpr std::size_t kAbsent = std::numeric_limits<std::size_t>::max();
  struct Node {
    std::unordered_map<std::size_t, std::size_t> children;  // edge -> node
    std::uint64_t freq = 0;
    std::unordered_set<std::size_t> observed;  // next activities the log takes
  };
  std::vector<Node> nodes(1);
  for (const auto& variant : variants) {
    std::size_t current = 0;
    nodes[current].freq += variant.count;
    for (const auto& id : variant.ids) {
      if (id) nodes[current].observed.insert(*id);
      const auto [it, inserted] =
          nodes[current].children.emplace(id ? *id : kAbsent, nodes.size());
      if (inserted) nodes.emplace_back();
      current = it->second;
      nodes[current].freq += variant.count;
    }
  }

  // Distinct prefixes routinely reach the same marking, and computing a
  // marking's enabled activities is the costly step (a silent-transition search
  // per activity). Cache it by marking so that work happens once per marking
  // rather than once per prefix.
  std::unordered_map<Marking, std::unordered_set<std::size_t>, MarkingHash>
      enabled_cache;

  std::uint64_t enabled_total = 0;
  std::uint64_t escaping_total = 0;

  // Depth-first walk carrying the marking: a child's marking is its parent's
  // advanced by one replay step, so each trie node costs one step rather than
  // a from-scratch replay of its whole prefix.
  std::vector<std::pair<std::size_t, Marking>> stack;
  stack.emplace_back(0, replayer.initial_marking());
  while (!stack.empty()) {
    auto [index, marking] = std::move(stack.back());
    stack.pop_back();
    const Node& node = nodes[index];

    for (const auto& [edge, child] : node.children) {
      Marking next = marking;
      replayer.advance(
          next, edge == kAbsent ? std::nullopt : std::optional<std::size_t>(edge));
      stack.emplace_back(child, std::move(next));
    }

    const auto [it, inserted] = enabled_cache.try_emplace(std::move(marking));
    if (inserted) {
      it->second = replayer.enabled_activities(it->first);
    }
    const auto& enabled = it->second;
    std::uint64_t escaping = 0;
    for (const std::size_t a : enabled) {
      if (node.observed.find(a) == node.observed.end()) ++escaping;
    }
    enabled_total += node.freq * enabled.size();
    escaping_total += node.freq * escaping;
  }
  return enabled_total == 0 ? 1.0
                            : 1.0 - double(escaping_total) / double(enabled_total);
}

// Alignment-based escaping-edges precision (Adriansyah, Munoz-Gama, Carmona,
// van Dongen & van der Aalst): the same prefix-automaton walk as
// compute_precision, but each prefix's model state comes from the optimal
// alignments rather than from forced token replay — on a deviating log the
// replay marking can be an artifact of healed tokens, while the aligned
// marking is one the model genuinely reaches in the cheapest explanation.
// Variants sharing a prefix may align it to different markings; the enabled
// sets of all of them are unioned, mirroring that any of those explanations
// is optimal.
double compute_alignment_precision(
    const std::vector<Variant>& variants,
    const std::vector<std::vector<Marking>>& variant_markings,
    const Replayer& replayer) {
  constexpr std::size_t kAbsent = std::numeric_limits<std::size_t>::max();
  struct Node {
    std::unordered_map<std::size_t, std::size_t> children;  // edge -> node
    std::uint64_t freq = 0;
    std::unordered_set<std::size_t> observed;  // next activities the log takes
    std::unordered_set<Marking, MarkingHash> markings;  // aligned model states
  };
  std::vector<Node> nodes(1);
  for (std::size_t v = 0; v < variants.size(); ++v) {
    const auto& variant = variants[v];
    const auto& markings = variant_markings[v];
    std::size_t current = 0;
    nodes[current].freq += variant.count;
    nodes[current].markings.insert(markings[0]);
    for (std::size_t d = 0; d < variant.ids.size(); ++d) {
      const auto& id = variant.ids[d];
      if (id) nodes[current].observed.insert(*id);
      const auto [it, inserted] =
          nodes[current].children.emplace(id ? *id : kAbsent, nodes.size());
      if (inserted) nodes.emplace_back();
      current = it->second;
      nodes[current].freq += variant.count;
      nodes[current].markings.insert(markings[d + 1]);
    }
  }

  // Distinct prefixes routinely align to the same marking; cache the costly
  // enabled-activities closure per marking.
  std::unordered_map<Marking, std::unordered_set<std::size_t>, MarkingHash>
      enabled_cache;

  std::uint64_t enabled_total = 0;
  std::uint64_t escaping_total = 0;
  for (const auto& node : nodes) {
    std::unordered_set<std::size_t> enabled;
    for (const auto& marking : node.markings) {
      const auto [it, inserted] = enabled_cache.try_emplace(marking);
      if (inserted) it->second = replayer.enabled_activities(marking);
      enabled.insert(it->second.begin(), it->second.end());
    }
    std::uint64_t escaping = 0;
    for (const std::size_t a : enabled) {
      if (node.observed.find(a) == node.observed.end()) ++escaping;
    }
    enabled_total += node.freq * enabled.size();
    escaping_total += node.freq * escaping;
  }
  return enabled_total == 0 ? 1.0
                            : 1.0 - double(escaping_total) / double(enabled_total);
}

// Token-replay generalization (Buijs, van Dongen & van der Aalst): a model
// generalizes well when replaying the log spreads executions across its
// transitions instead of leaving parts of the net exercised only once or
// twice. Each executed transition contributes 1/sqrt(executions) — near 1 when
// barely used, vanishing as use grows — averaged over all transitions.
double compute_generalization(const std::vector<std::uint64_t>& executions) {
  if (executions.empty()) return 1.0;
  double rarity = 0.0;
  for (const auto count : executions) {
    if (count > 0) rarity += 1.0 / std::sqrt(double(count));
  }
  return 1.0 - rarity / double(executions.size());
}

// Inverse arc-degree simplicity: a workflow net needs two arcs per node just
// to chain behaviour, so only the mean degree beyond that counts against it.
// 1 for a plain sequence, decaying as routing density grows.
double compute_simplicity(const PetriNet& net) {
  const std::size_t nodes = net.place_count() + net.transitions().size();
  if (nodes == 0) return 1.0;
  std::size_t arcs = 0;
  for (const auto& transition : net.transitions()) {
    arcs += transition.inputs.size() + transition.outputs.size();
  }
  // Every arc joins one place and one transition, so it adds 2 to the total
  // degree.
  const double mean_degree = 2.0 * double(arcs) / double(nodes);
  return 1.0 / (1.0 + std::max(mean_degree - 2.0, 0.0));
}

// Bounds the alignment search so a pathological model can never run forever; the
// bounded, block-structured nets the inductive miner produces reach the final
// marking well within this many state expansions.
constexpr std::size_t kAlignSearchCap = 200000;

// --- Linear program for the marking-equation heuristic ----------------------

struct LpResult {
  bool feasible = false;
  bool aborted = false;  // iteration guard tripped: no usable bound, prune nothing
  double objective = 0.0;
  std::vector<double> solution;  // per variable; empty when infeasible
};

// Minimal dense two-phase primal simplex for `min c·x  s.t.  A·x = b, x >= 0`.
// Sized for alignment heuristics (tens of rows and columns) and tuned for
// robustness over speed: Bland's rule prevents cycling, and if the iteration
// guard ever trips the caller falls back to a heuristic of zero rather than
// risking an overestimate from an unfinished minimization.
LpResult solve_lp(std::vector<std::vector<double>> a,
                  std::vector<double> b,
                  const std::vector<double>& c) {
  constexpr double kEps = 1e-9;
  const std::size_t m = b.size();
  const std::size_t n = c.size();

  // Normalize to b >= 0 so the artificial basis is feasible.
  for (std::size_t i = 0; i < m; ++i) {
    if (b[i] < 0.0) {
      for (auto& v : a[i]) v = -v;
      b[i] = -b[i];
    }
  }

  // Tableau: constraint rows over [real vars | artificials | rhs], plus the
  // reduced-cost row z (z[cols] holds minus the current objective).
  const std::size_t cols = n + m;
  std::vector<std::vector<double>> tab(m, std::vector<double>(cols + 1, 0.0));
  std::vector<std::size_t> basis(m);
  for (std::size_t i = 0; i < m; ++i) {
    for (std::size_t j = 0; j < n; ++j) tab[i][j] = a[i][j];
    tab[i][n + i] = 1.0;
    tab[i][cols] = b[i];
    basis[i] = n + i;
  }

  std::vector<double> z(cols + 1, 0.0);

  const auto pivot = [&](std::size_t row, std::size_t col) {
    const double p = tab[row][col];
    for (auto& v : tab[row]) v /= p;
    for (std::size_t i = 0; i < m; ++i) {
      if (i == row || std::abs(tab[i][col]) < kEps) continue;
      const double factor = tab[i][col];
      for (std::size_t j = 0; j <= cols; ++j) tab[i][j] -= factor * tab[row][j];
    }
    const double zf = z[col];
    if (std::abs(zf) > 0.0) {
      for (std::size_t j = 0; j <= cols; ++j) z[j] -= zf * tab[row][j];
    }
    basis[row] = col;
  };

  // Dantzig pricing (steepest reduced cost) converges in far fewer pivots
  // than Bland's rule; after a run of degenerate pivots the entering rule
  // falls back to Bland's until progress resumes, which breaks cycles in
  // practice — and the iteration guard (whose trip the caller treats as "no
  // usable bound") backstops the pathological remainder. Leaving rows tie-
  // break on the lowest basis index either way.
  std::size_t iterations_left = 200 + 50 * (m + n);
  std::size_t degenerate = 0;
  const auto iterate = [&](std::size_t allowed_cols) -> bool {
    while (iterations_left-- > 0) {
      std::size_t enter = allowed_cols;
      if (degenerate < 24) {
        double best = -1e-7;
        for (std::size_t j = 0; j < allowed_cols; ++j) {
          if (z[j] < best) {
            best = z[j];
            enter = j;
          }
        }
      } else {
        for (std::size_t j = 0; j < allowed_cols; ++j) {
          if (z[j] < -1e-7) {
            enter = j;
            break;
          }
        }
      }
      if (enter == allowed_cols) return true;  // optimal
      std::size_t leave = m;
      double best_ratio = 0.0;
      for (std::size_t i = 0; i < m; ++i) {
        if (tab[i][enter] <= kEps) continue;
        const double ratio = tab[i][cols] / tab[i][enter];
        if (leave == m || ratio < best_ratio - kEps ||
            (ratio < best_ratio + kEps && basis[i] < basis[leave])) {
          leave = i;
          best_ratio = ratio;
        }
      }
      if (leave == m) return true;  // unbounded: impossible for our costs
      if (tab[leave][cols] < kEps) {
        ++degenerate;
      } else {
        degenerate = 0;
      }
      pivot(leave, enter);
    }
    return false;  // iteration guard tripped
  };

  // Phase 1: minimize the artificial sum. Reduced costs under the artificial
  // basis are 0 - sum of each column; the rhs entry starts at -sum(b).
  for (std::size_t j = 0; j <= cols; ++j) {
    double sum = 0.0;
    for (std::size_t i = 0; i < m; ++i) sum += tab[i][j];
    z[j] = (j >= n && j < cols ? 1.0 : 0.0) - sum;
  }
  LpResult aborted;
  aborted.aborted = true;
  if (!iterate(cols)) return aborted;
  if (-z[cols] > 1e-6) return {};  // artificials cannot vanish: infeasible

  // Pivot any degenerate artificial out of the basis where possible so phase 2
  // can bar the artificial columns entirely.
  for (std::size_t i = 0; i < m; ++i) {
    if (basis[i] < n) continue;
    for (std::size_t j = 0; j < n; ++j) {
      if (std::abs(tab[i][j]) > kEps) {
        pivot(i, j);
        break;
      }
    }
  }

  // Phase 2: minimize the real objective from the phase-1 basis.
  std::fill(z.begin(), z.end(), 0.0);
  for (std::size_t j = 0; j < n; ++j) z[j] = c[j];
  for (std::size_t i = 0; i < m; ++i) {
    const double cb = basis[i] < n ? c[basis[i]] : 0.0;
    if (cb == 0.0) continue;
    for (std::size_t j = 0; j <= cols; ++j) z[j] -= cb * tab[i][j];
  }
  if (!iterate(n)) return aborted;

  LpResult result;
  result.feasible = true;
  result.objective = -z[cols];
  result.solution.assign(n, 0.0);
  for (std::size_t i = 0; i < m; ++i) {
    if (basis[i] < n) result.solution[basis[i]] = tab[i][cols];
  }
  return result;
}

// Computes optimal alignments by an A* search over the synchronous product of
// the trace and the workflow net. A search state is a (marking, trace-position)
// pair; the goal is the final marking with the whole trace consumed. Edge costs
// follow the standard alignment cost function: synchronous and silent moves are
// free, log-only and visible model-only moves cost one each. The heuristic is
// the marking-equation lower bound (van Dongen), solved lazily: children
// inherit max(h - cost, 0) from their parent and the LP runs only when a state
// with an inherited bound is actually popped — so the search stays optimal
// while expanding a fraction of the states plain Dijkstra visits, which is what
// makes concurrent nets (parallel branches multiply the marking space)
// alignable within the search cap.
class Aligner {
 public:
  explicit Aligner(const PetriNet& net) : net_(net) {
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
    final_ = Marking(net_.place_count(), 0);
    final_[net_.sink()] = 1;

    // Net incidence (tokens produced minus consumed per place) as sparse
    // columns for the marking-equation heuristic.
    incidence_.resize(net_.transitions().size());
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      std::unordered_map<std::size_t, double> column;
      for (const std::size_t place : net_.transitions()[t].inputs) {
        column[place] -= 1.0;
      }
      for (const std::size_t place : net_.transitions()[t].outputs) {
        column[place] += 1.0;
      }
      for (const auto& [place, coef] : column) {
        if (coef != 0.0) incidence_[t].emplace_back(place, coef);
      }
    }

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
    // The model marking the optimal alignment is in right before it consumes
    // log event d (after any model/silent moves scheduled first), for
    // d = 0..len-1, plus the final marking at index len. These are the states
    // alignment-based precision inspects. Empty when the search was capped.
    std::vector<Marking> prefix_markings;
  };

  // Optimal alignment of one trace (activity ids; std::nullopt = activity absent
  // from the model) carrying the matching activity names for display. Runs a
  // cheap plain-Dijkstra pass first — most traces align within a handful of
  // expansions and never pay for an LP — and only when its budget runs out
  // restarts with the marking-equation heuristic, whose pruning is what keeps
  // concurrent or badly fitting nets tractable.
  [[nodiscard]] Outcome align(
      const std::vector<std::optional<std::size_t>>& ids,
      const std::vector<std::string>& names) const {
    const std::size_t len = ids.size();
    const std::size_t pc = net_.place_count();
    const std::size_t tcount = net_.transitions().size();

    // A search state is the marking with the trace position appended at index
    // `pc`, so the same flat vector is both the search node and its hash-map key
    // -- no string is formatted and the marking is never copied just to key it.
    // enabled()/the firing below only ever touch place indices < pc, so the
    // appended position rides along untouched until a log or sync move bumps it.
    using State = Marking;
    State start_key(pc, 0);
    start_key[net_.source()] = 1;
    start_key.push_back(0);
    State final_key = final_;
    final_key.push_back(static_cast<int>(len));

    // Transitions whose label matches each trace event, for the heuristic's
    // synchronous-move columns.
    std::vector<std::vector<std::size_t>> matches(len);
    for (std::size_t d = 0; d < len; ++d) {
      if (!ids[d]) continue;
      for (std::size_t t = 0; t < tcount; ++t) {
        const auto& transition = net_.transitions()[t];
        if (!transition.silent() && *transition.activity == *ids[d]) {
          matches[d].push_back(t);
        }
      }
    }

    // Marking-equation heuristic: the remaining alignment cost is bounded from
    // below by relaxing it to the LP  min c·x  s.t.  N·x = m_final - m  over
    // continuous move counts x >= 0. Columns are the net's transitions fired
    // as model moves, plus one log move and the matching synchronous moves per
    // remaining trace position (each position must advance exactly once) — so
    // a move out of `state` is column t (model), tcount (log at the current
    // position), or tcount + 1 + k (sync with matches[pos][k]). Every real
    // completion is a feasible integer solution, so the LP optimum never
    // overestimates; costs are integral, so its ceiling is still a lower
    // bound. Infeasible means the final marking is unreachable from this
    // state: the branch can be pruned outright.
    struct LpBound {
      std::uint64_t bound = 0;
      std::vector<double> solution;  // empty when the solve was aborted
    };
    const auto heuristic = [&](const State& state) -> std::optional<LpBound> {
      const std::size_t pos = static_cast<std::size_t>(state.back());
      const std::size_t rows = pc + (len - pos);
      std::size_t vars = tcount;
      for (std::size_t d = pos; d < len; ++d) vars += 1 + matches[d].size();

      std::vector<std::vector<double>> a(rows, std::vector<double>(vars, 0.0));
      std::vector<double> b(rows, 0.0);
      std::vector<double> c(vars, 0.0);
      for (std::size_t p = 0; p < pc; ++p) {
        b[p] = static_cast<double>(final_[p]) - static_cast<double>(state[p]);
      }
      for (std::size_t t = 0; t < tcount; ++t) {
        for (const auto& [place, coef] : incidence_[t]) a[place][t] = coef;
        c[t] = net_.transitions()[t].silent() ? 0.0 : 1.0;
      }
      std::size_t col = tcount;
      for (std::size_t d = pos; d < len; ++d) {
        const std::size_t row = pc + (d - pos);
        b[row] = 1.0;
        a[row][col] = 1.0;  // log move: advances the trace, costs one
        c[col] = 1.0;
        ++col;
        for (const std::size_t t : matches[d]) {  // synchronous moves: free
          for (const auto& [place, coef] : incidence_[t]) a[place][col] = coef;
          a[row][col] = 1.0;
          ++col;
        }
      }

      auto lp = solve_lp(std::move(a), std::move(b), c);
      if (lp.aborted) return LpBound{};  // no usable bound, nothing provably dead
      if (!lp.feasible) return std::nullopt;
      return LpBound{static_cast<std::uint64_t>(
                         std::ceil(std::max(lp.objective, 0.0) - 1e-6)),
                     std::move(lp.solution)};
    };

    // A state whose marking equation is infeasible can never reach the goal;
    // an effectively infinite h keeps it buried without special-casing.
    constexpr std::uint64_t kDeadEnd =
        std::numeric_limits<std::uint64_t>::max() / 4;

    // One bounded best-first search; nullopt when the goal was not reached
    // within `budget` expansions. Without the LP every state is treated as
    // exactly bounded by zero, which is plain Dijkstra.
    const auto run = [&](bool use_lp,
                         std::size_t budget) -> std::optional<Outcome> {
      // States are interned: the hash map owns each state once and hands out a
      // dense id, so back-pointers, the frontier, and the cost table are plain
      // integers rather than copies of the state vector. Map nodes are stable,
      // so by_id can point into the map.
      struct Entry {
        std::uint64_t cost = 0;       // g: cheapest known cost to the state
        std::uint64_t heuristic = 0;  // h: admissible lower bound to the goal
        std::uint32_t prev = 0;
        AlignmentMoveType type = AlignmentMoveType::kModel;
        std::int32_t ref = -1;  // kLog: trace position; otherwise: activity id
        bool silent = false;
        bool exact = false;   // h is the LP value, not an inherited bound
        bool closed = false;  // expanded with an exact h: g is final
      };

      struct Node {
        std::uint64_t f;    // g + h at push time
        std::uint64_t g;
        std::uint64_t seq;  // FIFO tie-break keeps the search deterministic
        std::uint32_t state;
        bool operator>(const Node& other) const {
          if (f != other.f) return f > other.f;
          if (g != other.g) return g < other.g;  // prefer deeper states on ties
          return seq > other.seq;
        }
      };

      constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
      std::priority_queue<Node, std::vector<Node>, std::greater<>> frontier;
      std::unordered_map<State, std::uint32_t, MarkingHash> index;
      std::vector<const State*> by_id;
      std::vector<Entry> entries;
      std::uint64_t seq = 0;
      std::uint32_t final_id = kNone;

      const auto intern = [&](State state) {
        const auto [it, inserted] =
            index.try_emplace(std::move(state), std::uint32_t(entries.size()));
        if (inserted) {
          by_id.push_back(&it->first);
          entries.emplace_back();
          if (it->first == final_key) final_id = it->second;
        }
        return std::make_pair(it->second, inserted);
      };

      // The start carries no bound yet: its first pop solves the LP (in LP
      // mode), so the expansion right after has the solution in hand.
      const std::uint32_t start_id = intern(start_key).first;
      entries[start_id].exact = !use_lp;
      frontier.push({0, 0, seq++, start_id});

      const auto relax = [&](State next, std::uint64_t cost, std::uint32_t prev,
                             AlignmentMoveType type, std::int32_t ref,
                             bool silent, std::uint64_t bound,
                             bool bound_exact) {
        const auto [id, inserted] = intern(std::move(next));
        Entry& entry = entries[id];
        // Inherited bounds are admissible for the child too, so keep whichever
        // lower bound is tighter; a bound the parent's LP solution walked is
        // the child's exact LP value and spares the child its own solve.
        if (bound > entry.heuristic) entry.heuristic = bound;
        if (bound_exact) entry.exact = true;
        if (!inserted && entry.cost <= cost) return;
        entry.cost = cost;
        entry.prev = prev;
        entry.type = type;
        entry.ref = ref;
        entry.silent = silent;
        frontier.push({cost + entry.heuristic, cost, seq++, id});
      };

      bool reached = false;
      std::size_t expanded = 0;
      while (!frontier.empty() && expanded < budget) {
        const Node node = frontier.top();
        frontier.pop();
        {
          const Entry& entry = entries[node.state];
          if (node.g > entry.cost || entry.closed) continue;  // stale
          if (node.f < entry.cost + entry.heuristic) {
            // The bound tightened after this queue entry was pushed: re-queue
            // at the honest priority instead of expanding too early.
            frontier.push({entry.cost + entry.heuristic, node.g, seq++,
                           node.state});
            continue;
          }
        }

        if (node.state == final_id) {
          reached = true;
          break;
        }

        // Lazy exactness: states carry inherited bounds until they are
        // actually popped, so the LP runs only for states the search truly
        // visits — and the solution just computed marks the children on its
        // predicted path exact, sparing them their own solve.
        const std::vector<double>* solution = nullptr;
        std::vector<double> solved_solution;
        if (!entries[node.state].exact) {
          auto solved = heuristic(*by_id[node.state]);
          Entry& entry = entries[node.state];
          entry.exact = true;
          if (!solved) {
            entry.heuristic = kDeadEnd;
            continue;
          }
          if (solved->bound > entry.heuristic) {
            entry.heuristic = solved->bound;
            if (entry.cost + entry.heuristic > node.f) {
              frontier.push({entry.cost + entry.heuristic, node.g, seq++,
                             node.state});
              continue;  // wait for its turn under the tightened bound
            }
          }
          solved_solution = std::move(solved->solution);
          if (!solved_solution.empty()) solution = &solved_solution;
        }

        entries[node.state].closed = true;
        ++expanded;

        const std::uint64_t g = entries[node.state].cost;
        const std::uint64_t h = entries[node.state].heuristic;
        const std::uint64_t h_less_one = h > 0 ? h - 1 : 0;
        const State& state = *by_id[node.state];
        const std::size_t pos = static_cast<std::size_t>(state.back());
        const auto walked = [&](std::size_t var) {
          return solution != nullptr && (*solution)[var] >= 1.0 - 1e-6;
        };

        // Log move: the trace advances while the model stands still.
        if (pos < len) {
          State next = state;
          next.back() = static_cast<int>(pos + 1);
          relax(std::move(next), g + 1, node.state, AlignmentMoveType::kLog,
                static_cast<std::int32_t>(pos), false, h_less_one,
                !use_lp || walked(tcount));
        }

        // Model moves (and the synchronous move when the labels agree).
        for (std::size_t t = 0; t < tcount; ++t) {
          if (!enabled(state, t)) continue;
          const auto& transition = net_.transitions()[t];
          State fired = state;
          for (const std::size_t place : transition.inputs) --fired[place];
          for (const std::size_t place : transition.outputs) ++fired[place];
          if (transition.silent()) {
            relax(std::move(fired), g, node.state, AlignmentMoveType::kModel,
                  -1, true, h, !use_lp || walked(t));
            continue;
          }
          const auto activity = static_cast<std::int32_t>(*transition.activity);
          const bool sync = pos < len && ids[pos] &&
                            *ids[pos] == *transition.activity;
          if (sync) {
            const std::size_t k = static_cast<std::size_t>(
                std::find(matches[pos].begin(), matches[pos].end(), t) -
                matches[pos].begin());
            State synced = fired;
            synced.back() = static_cast<int>(pos + 1);
            relax(std::move(fired), g + 1, node.state,
                  AlignmentMoveType::kModel, activity, false, h_less_one,
                  !use_lp || walked(t));
            relax(std::move(synced), g, node.state,
                  AlignmentMoveType::kSynchronous, activity, false, h,
                  !use_lp || walked(tcount + 1 + k));
          } else {
            relax(std::move(fired), g + 1, node.state,
                  AlignmentMoveType::kModel, activity, false, h_less_one,
                  !use_lp || walked(t));
          }
        }
      }

      if (!reached) return std::nullopt;

      Outcome outcome;
      outcome.cost = entries[final_id].cost;
      std::vector<std::uint32_t> path{final_id};
      for (std::uint32_t id = final_id; id != start_id;) {
        const Entry& entry = entries[id];
        if (!entry.silent) {
          outcome.moves.push_back(
              {entry.type, entry.type == AlignmentMoveType::kLog
                               ? names[static_cast<std::size_t>(entry.ref)]
                               : net_.activities()[static_cast<std::size_t>(
                                     entry.ref)]});
        }
        id = entry.prev;
        path.push_back(id);
      }
      std::reverse(outcome.moves.begin(), outcome.moves.end());
      std::reverse(path.begin(), path.end());

      // Prefix markings for alignment-based precision: along the optimal path,
      // the trace position advances exactly once per consumed log event, and
      // the state it advances from holds the model marking at that moment.
      const auto strip = [](const State& state) {
        return Marking(state.begin(), state.end() - 1);
      };
      outcome.prefix_markings.resize(len + 1);
      for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const State& state = *by_id[path[i]];
        const State& next = *by_id[path[i + 1]];
        if (next.back() == state.back() + 1) {
          outcome.prefix_markings[static_cast<std::size_t>(state.back())] =
              strip(state);
        }
      }
      outcome.prefix_markings[len] = strip(*by_id[path.back()]);
      return outcome;
    };

    const std::size_t quick =
        std::min<std::size_t>(kAlignSearchCap, 8 * (len + 1) + 256);
    if (auto outcome = run(false, quick)) return *outcome;
    if (auto outcome = run(true, kAlignSearchCap)) return *outcome;

    // Search capped out: fall back to the worst-case alignment so callers
    // still get a well-formed result, and flag that the numbers are not
    // trustworthy.
    Outcome outcome;
    for (std::size_t i = 0; i < len; ++i) {
      outcome.moves.push_back({AlignmentMoveType::kLog, names[i]});
    }
    outcome.cost = static_cast<std::uint64_t>(len) + empty_run_cost_;
    outcome.capped = true;
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
    std::unordered_map<Marking, std::uint64_t, MarkingHash> best{{start, 0}};
    frontier.push({0, start});
    std::size_t expanded = 0;
    while (!frontier.empty() && expanded++ < kAlignSearchCap) {
      auto [cost, marking] = frontier.top();  // top() is const&: this copies
      frontier.pop();
      const auto it = best.find(marking);
      if (it != best.end() && cost > it->second) continue;
      if (marking == final_) return cost;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(marking, t)) continue;
        Marking next = fire(marking, t);
        const std::uint64_t next_cost =
            cost + (net_.transitions()[t].silent() ? 0 : 1);
        const auto found = best.find(next);
        if (found != best.end() && found->second <= next_cost) continue;
        best[next] = next_cost;
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
  // Per-transition sparse LP column for the marking-equation heuristic:
  // produced minus consumed tokens per place.
  std::vector<std::vector<std::pair<std::size_t, double>>> incidence_;
};

// Bounds the model-footprint exploration so a highly concurrent net cannot
// enumerate markings forever; a bounded block-structured net is covered well
// within this many marking expansions.
constexpr std::size_t kFootprintCap = 200000;

using Relation = std::set<std::pair<std::string, std::string>>;

// The log's directly-follows relation: every ordered pair of adjacent
// activities seen in some trace.
Relation log_footprint(const EventLog& log) {
  Relation relation;
  for (const auto& trace : log.traces) {
    for (std::size_t i = 1; i < trace.events.size(); ++i) {
      relation.emplace(trace.events[i - 1].activity, trace.events[i].activity);
    }
  }
  return relation;
}

// The model's directly-follows relation: explore the net's reachable markings,
// and for every activity a enabled at a marking, record a->b for each activity
// b enabled right after a fires. `exhausted` is set when the marking budget runs
// out before the reachable space is covered.
// The model's directly-follows relation: a > b iff some reachable marking
// fires a and can then fire b after silent moves only. One bounded pass
// expands every reachable marking once over all transitions; the visible
// labels observable at each marking are then propagated backwards over the
// silent edges to a fixpoint, so every marking's silent closure is computed
// once for the whole graph rather than re-searched per query — which is what
// keeps concurrent nets (whose marking spaces are large) tractable.
Relation model_footprint(const PetriNet& net, bool& exhausted) {
  const auto& names = net.activities();
  const std::size_t acts = names.size();
  const std::size_t words = (acts + 63) / 64;

  std::unordered_map<Marking, std::uint32_t, MarkingHash> index;
  std::vector<const Marking*> by_id;  // map nodes are stable
  const auto intern = [&](Marking m) {
    const auto [it, inserted] =
        index.try_emplace(std::move(m), static_cast<std::uint32_t>(index.size()));
    if (inserted) by_id.push_back(&it->first);
    return std::make_pair(it->second, inserted);
  };

  const auto enabled = [&](const Marking& m, const PetriTransition& t) {
    for (const std::size_t place : t.inputs) {
      if (m[place] <= 0) return false;
    }
    return true;
  };
  const auto fire = [&](const Marking& m, const PetriTransition& t) {
    Marking next = m;
    for (const std::size_t place : t.inputs) --next[place];
    for (const std::size_t place : t.outputs) ++next[place];
    return next;
  };

  Marking start(net.place_count(), 0);
  start[net.source()] = 1;

  // Bounded breadth-first exploration. Only the silent edges are stored (for
  // the label propagation); visible successors are re-fired in the final pass
  // so the usually far larger visible edge set never has to be kept.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> silent_edges;
  std::vector<char> was_expanded;
  std::queue<std::uint32_t> frontier;
  frontier.push(intern(std::move(start)).first);
  std::size_t expanded = 0;
  while (!frontier.empty()) {
    if (expanded++ >= kFootprintCap) {
      exhausted = true;
      break;
    }
    const std::uint32_t id = frontier.front();
    frontier.pop();
    if (was_expanded.size() < by_id.size()) was_expanded.resize(by_id.size(), 0);
    was_expanded[id] = 1;
    const Marking& current = *by_id[id];
    for (const auto& transition : net.transitions()) {
      if (!enabled(current, transition)) continue;
      const auto [next, inserted] = intern(fire(current, transition));
      if (transition.silent()) silent_edges.emplace_back(id, next);
      if (inserted) frontier.push(next);
    }
  }

  const std::size_t count = by_id.size();
  was_expanded.resize(count, 0);

  // labels[m]: bitset of activities enabled at m after silent moves only.
  // Seed with each marking's directly enabled visible transitions, then close
  // backwards over silent edges (u -> v silently implies labels(v) ⊆
  // labels(u)) with a worklist until stable.
  std::vector<std::uint64_t> labels(count * words, 0);
  const auto label_or = [&](std::uint32_t dst, std::uint32_t src) {
    bool changed = false;
    for (std::size_t w = 0; w < words; ++w) {
      const std::uint64_t merged = labels[dst * words + w] | labels[src * words + w];
      changed |= merged != labels[dst * words + w];
      labels[dst * words + w] = merged;
    }
    return changed;
  };
  for (std::uint32_t id = 0; id < count; ++id) {
    const Marking& current = *by_id[id];
    for (const auto& transition : net.transitions()) {
      if (transition.silent() || !enabled(current, transition)) continue;
      labels[id * words + *transition.activity / 64] |=
          std::uint64_t{1} << (*transition.activity % 64);
    }
  }
  std::vector<std::vector<std::uint32_t>> silent_preds(count);
  for (const auto& [from, to] : silent_edges) silent_preds[to].push_back(from);
  std::queue<std::uint32_t> work;
  std::vector<char> queued(count, 1);
  for (std::uint32_t id = 0; id < count; ++id) work.push(id);
  while (!work.empty()) {
    const std::uint32_t id = work.front();
    work.pop();
    queued[id] = 0;
    for (const std::uint32_t pred : silent_preds[id]) {
      if (label_or(pred, id) && !queued[pred]) {
        queued[pred] = 1;
        work.push(pred);
      }
    }
  }

  // Emit the relation: for every visible edge m -a-> m', a is directly
  // followed by everything observable at m'.
  Relation relation;
  for (std::uint32_t id = 0; id < count; ++id) {
    if (!was_expanded[id]) continue;
    const Marking& current = *by_id[id];
    for (const auto& transition : net.transitions()) {
      if (transition.silent() || !enabled(current, transition)) continue;
      const auto found = index.find(fire(current, transition));
      if (found == index.end()) continue;
      const std::uint32_t next = found->second;
      for (std::size_t b = 0; b < acts; ++b) {
        if (labels[next * words + b / 64] & (std::uint64_t{1} << (b % 64))) {
          relation.emplace(names[*transition.activity], names[b]);
        }
      }
    }
  }
  return relation;
}

FootprintResult compare_footprints(const Relation& log, const Relation& model,
                                   bool exhausted) {
  FootprintResult result;
  result.exhausted = exhausted;
  result.log_relations = log.size();
  result.model_relations = model.size();
  for (const auto& pair : log) {
    if (model.count(pair)) {
      ++result.shared_relations;
    } else {
      result.log_only.push_back({pair.first, pair.second});
    }
  }
  for (const auto& pair : model) {
    if (!log.count(pair)) result.model_only.push_back({pair.first, pair.second});
  }
  result.fitness = log.empty()
                       ? 1.0
                       : double(result.shared_relations) / double(log.size());
  result.precision =
      model.empty() ? 1.0
                    : double(result.shared_relations) / double(model.size());
  return result;
}

}  // namespace

FootprintResult check_footprints(const EventLog& event_log,
                                 const PetriNet& net) {
  bool exhausted = false;
  const auto model = model_footprint(net, exhausted);
  return compare_footprints(log_footprint(event_log), model, exhausted);
}

FootprintResult check_footprints(const EventLog& event_log,
                                 const ProcessTree& tree) {
  return check_footprints(event_log, to_petri_net(tree));
}

ConformanceResult check_conformance(const EventLog& event_log,
                                    const PetriNet& net) {
  const Replayer replayer(net);
  const auto variants = collect_variants(event_log, replayer);

  ConformanceResult result;
  result.trace_count = event_log.trace_count();
  result.fitness = 1.0;
  result.precision = 1.0;

  std::vector<std::uint64_t> executions(net.transitions().size(), 0);
  std::vector<std::uint64_t> variant_executions;
  for (const auto& variant : variants) {
    variant_executions.assign(net.transitions().size(), 0);
    const auto counts = replayer.replay(variant.ids, &variant_executions);
    for (std::size_t t = 0; t < executions.size(); ++t) {
      executions[t] += variant_executions[t] * variant.count;
    }
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
  result.generalization = compute_generalization(executions);
  result.simplicity = compute_simplicity(net);

  // Surface the least-fitting variants first; break ties by frequency so the
  // most impactful deviations lead.
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantConformance& a, const VariantConformance& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

ConformanceResult check_conformance(const EventLog& event_log,
                                    const ProcessTree& tree) {
  return check_conformance(event_log, to_petri_net(tree));
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net) {
  AlignmentResult result;
  result.trace_count = event_log.trace_count();

  // No traces: there is nothing to align, so the result is trivially perfect.
  // Guard this before the soundness check so an empty log against an Alpha or
  // heuristic net (whose sink is unreachable) is not misreported as unsound.
  if (result.trace_count == 0) {
    return result;
  }

  const Aligner aligner(net);
  if (!aligner.reachable()) {
    // Not a sound workflow net (e.g. an Alpha net whose sink is unreachable):
    // report that alignments could not be computed rather than fake numbers.
    result.computed = false;
    return result;
  }

  const auto variants = collect_variants(event_log, aligner);
  std::vector<std::vector<Marking>> variant_markings;
  variant_markings.reserve(variants.size());
  std::uint64_t total_worst = 0;
  for (const auto& variant : variants) {
    auto outcome = aligner.align(variant.ids, variant.names);
    if (outcome.capped) {
      // The search could not align a trace within its bound — the net is
      // reachable but unbounded or simply too large, so the results are not
      // trustworthy. Flag it as exhausted (distinct from an unsound net).
      result.computed = false;
      result.exhausted = true;
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
        variant.names, variant.count, std::move(outcome.moves), outcome.cost,
        fitness});
    variant_markings.push_back(std::move(outcome.prefix_markings));
  }

  result.fitness =
      total_worst == 0 ? 1.0 : 1.0 - double(result.total_cost) / double(total_worst);

  // Precision over the aligned states. The Replayer supplies the same
  // enabled-activities closure token-replay precision uses, so the two
  // precision numbers differ only in where the markings come from.
  const Replayer replayer(net);
  result.precision =
      compute_alignment_precision(variants, variant_markings, replayer);

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

std::vector<std::optional<std::vector<std::vector<std::size_t>>>>
replay_firing_sequences(const PetriNet& net,
                        const std::vector<std::vector<std::string>>& variants) {
  const Replayer replayer(net);
  std::vector<std::optional<std::vector<std::vector<std::size_t>>>> result;
  result.reserve(variants.size());
  for (const auto& variant : variants) {
    std::vector<std::optional<std::size_t>> ids;
    ids.reserve(variant.size());
    for (const auto& name : variant) {
      ids.push_back(replayer.activity_id(name));
    }
    result.push_back(replayer.clean_firing_sequence(ids));
  }
  return result;
}

bool final_marking_reachable(const PetriNet& net) {
  // The Aligner computes final-marking reachability in its constructor as the
  // precondition for scoring, so reuse it rather than duplicate the search.
  return Aligner(net).reachable();
}

}  // namespace minera
