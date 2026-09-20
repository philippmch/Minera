#include "minera/conformance.hpp"
#include "petri_replay_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <chrono>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <stdexcept>
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

// Cap on the aligner's cross-variant bound cache (entries); past it, solves
// simply stop being remembered rather than growing memory without limit.
constexpr std::size_t kAlignerBoundCacheCap = 2'000'000;

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

Marking resolve_workflow_marking(const PetriNet& net,
                                 const Marking& requested,
                                 bool initial) {
  if (requested.empty()) {
    Marking marking(net.place_count(), 0);
    const std::size_t place = initial ? net.source() : net.sink();
    if (place < marking.size()) marking[place] = 1;
    return marking;
  }
  if (requested.size() != net.place_count()) {
    throw std::invalid_argument(initial
                                    ? "initial marking dimension does not match net"
                                    : "final marking dimension does not match net");
  }
  if (std::any_of(requested.begin(), requested.end(),
                  [](int tokens) { return tokens < 0; })) {
    throw std::invalid_argument(initial ? "initial marking contains a negative token count"
                                        : "final marking contains a negative token count");
  }
  return requested;
}

bool marking_covers(const Marking& marking, const Marking& target) {
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] < target[place]) return false;
  }
  return true;
}

bool marking_has_final_places(const Marking& marking, const Marking& target) {
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (target[place] > 0 && marking[place] <= 0) return false;
  }
  return true;
}

bool valid_explicit_marking(const PetriNet& net, const Marking& marking) {
  return marking.empty() ||
         (marking.size() == net.place_count() &&
          std::none_of(marking.begin(), marking.end(),
                       [](int tokens) { return tokens < 0; }));
}

bool valid_token_replay_variant(TokenReplayVariant variant) {
  switch (variant) {
    case TokenReplayVariant::kForward:
    case TokenReplayVariant::kBackwards:
      return true;
  }
  return false;
}

void validate_token_replay_event_limit(const EventLog& log,
                                     const TokenReplayOptions& options) {
  if (!options.max_rows_to_materialize) return;
  std::size_t available = *options.max_rows_to_materialize;
  for (const auto& trace : log.traces) {
    if (trace.events.size() > available)
      throw std::length_error("token replay input exceeds max_rows_to_materialize");
    available -= trace.events.size();
  }
}

bool valid_alignment_variant(AlignmentVariant variant) {
  switch (variant) {
    case AlignmentVariant::kStateEquationAStar:
    case AlignmentVariant::kDijkstraNoHeuristics:
    case AlignmentVariant::kDijkstraLessMemory:
    case AlignmentVariant::kDiscountedAStar:
    case AlignmentVariant::kGeneratorDijkstraNoHeuristics:
    case AlignmentVariant::kGeneratorDijkstraLessMemory:
      return true;
  }
  return false;
}

bool valid_alignment_cost_profile(AlignmentCostProfile profile) {
  switch (profile) {
    case AlignmentCostProfile::kLegacyUnit:
    case AlignmentCostProfile::kStandard:
      return true;
  }
  return false;
}

// Holds the net plus the lookups token replay needs and drives both the
// fitness and precision passes over a single shared translation.
class Replayer {
  struct ForwardConfiguration {
    detail::ReplayMarkingKeys initial_keys;
    detail::ReplayMarkingKeys final_keys;
    std::vector<std::string> place_names;
    std::vector<std::string> transition_names;
    detail::CleaningComponents components;
  };
 public:
  explicit Replayer(const PetriNet& net, const Marking& initial = {},
                    const Marking& final = {},
                    const TokenReplayOptions* replay_options = nullptr)
      : net_(net),
        initial_(resolve_workflow_marking(net, initial, true)),
        final_(resolve_workflow_marking(net, final, false)) {
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
    if (replay_options && replay_options->variant == TokenReplayVariant::kForward)
      forward_configuration_ = prepare_forward(*replay_options);
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
    std::uint64_t missing_before_final_check = 0;
    bool has_unknown_activity = false;
    bool forward_replay = false;
    std::vector<TokenReplayPlaceDiagnostics> places;

    [[nodiscard]] bool forward_fitting(const TokenReplayOptions& options) const {
      return missing_before_final_check == 0 &&
          (!options.consider_remaining_in_fitness || remaining == 0) &&
          (!options.consider_activities_not_in_model_in_fitness || !has_unknown_activity);
    }

    [[nodiscard]] double fitness() const {
      if (forward_replay && (consumed == 0 || produced == 0)) return 1.0;
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
      std::vector<std::uint64_t>* executions = nullptr,
      const TokenReplayOptions& options = {},
      TokenReplayTraceDiagnostics* diagnostics = nullptr) const {
    Marking marking = initial_marking();
    std::optional<ForwardConfiguration> local_configuration;
    if (!forward_configuration_) local_configuration = prepare_forward(options);
    const auto& configuration = forward_configuration_ ? *forward_configuration_ : *local_configuration;
    auto keys = configuration.initial_keys;
    const auto& final_keys = configuration.final_keys;
    const auto& place_names = configuration.place_names;
    const auto& transition_names = configuration.transition_names;
    const auto& components = configuration.components;
    Counts counts;
    counts.forward_replay = true;
    counts.places.resize(net_.place_count());
    for (std::size_t place = 0; place < initial_.size(); ++place) {
      const auto tokens = static_cast<std::uint64_t>(initial_[place]);
      counts.produced += tokens;
      counts.places[place].produced += tokens;
    }
    const auto emit = [&](std::size_t transition, bool event) {
      forward_fire(marking, keys, transition, counts);
      if (executions) ++(*executions)[transition];
      if (diagnostics) {
        diagnostics->activated_transitions.push_back(transition);
        if (event) diagnostics->event_diagnostics.back().activated_transitions.push_back(transition);
      }
    };
    for (const auto& step : trace) {
      if (diagnostics) {
        diagnostics->event_diagnostics.push_back({diagnostics->replayed_events, {}, {}});
        ++diagnostics->replayed_events;
      }
      if (!step || transitions_by_activity_[*step].empty()) {
        counts.has_unknown_activity = true;
        if (diagnostics)
          diagnostics->unknown_event_indices.push_back(diagnostics->replayed_events - 1);
        continue;
      }
      auto targets = transitions_by_activity_[*step];
      std::sort(targets.begin(), targets.end(), [&](auto left, auto right) {
        return std::tie(transition_names[left], left) < std::tie(transition_names[right], right);
      });
      auto chosen = targets.back();
      for (const auto transition : targets) {
        if (enabled(marking, transition)) { chosen = transition; break; }
      }
      if (!enabled(marking, chosen) && options.walk_through_hidden_transitions) {
        if (net_.has_extended_arcs()) {
          const auto path = silent_path(marking, [&](const Marking& candidate) { return enabled(candidate, chosen); });
          if (path) for (const auto transition : *path) emit(transition, true);
        } else {
          forward_route_hidden(marking, keys, chosen, place_names,
                               options.exhaustive_invisible_exploration,
                               [&](auto transition) { emit(transition, true); });
        }
      }
      const bool forced = !enabled(marking, chosen);
      if (forced && diagnostics) {
        diagnostics->transitions_with_problems.push_back(chosen);
        diagnostics->event_diagnostics.back().transitions_with_problems.push_back(chosen);
      }
      if (forced && options.stop_immediately_unfit) {
        ++counts.missing;
        if (diagnostics) diagnostics->replay_stopped = true;
        break;
      }
      const auto before_cleaning = forced && !components.empty()
          ? keys : detail::ReplayMarkingKeys{};
      emit(chosen, true);
      if (!before_cleaning.empty()) {
        for (const auto& component : components) {
          const bool introduced = std::any_of(component.begin(), component.end(),
              [&](auto place) { return keys[place] && !before_cleaning[place]; });
          if (!introduced) continue;
          for (const auto place : component) {
            if (keys[place] && before_cleaning[place]) {
              marking[place] = 0;
              keys[place] = false;
              ++counts.remaining;
              ++counts.places[place].remaining;
            }
          }
        }
      }
    }
    const auto final_present = [&]() {
      for (std::size_t place = 0; place < keys.size(); ++place)
        if (final_keys[place] && !keys[place]) return false;
      return true;
    };
    if (options.try_to_reach_final_marking_through_hidden) {
      if (net_.has_extended_arcs()) {
        // Inhibitor/reset-only transitions need not be reachable from a
        // marked ordinary input. Preserve enabled-state routing for these nets.
        const auto path = silent_path(marking, [&](const Marking& candidate) {
          return marking_has_final_places(candidate, final_);
        });
        if (path) for (const auto transition : *path) emit(transition, false);
      } else for (std::size_t pass = 0; pass < 6 && !final_present(); ++pass) {
        const auto paths = detail::silent_place_paths(keys, final_keys, net_, place_names);
        bool fired = false;
        for (const auto& path : paths) {
          for (const auto transition : path.transitions) {
            if (!enabled(marking, transition)) break;
            emit(transition, false);
            fired = true;
          }
          if (final_present()) break;
        }
        if (!fired) break;
      }
    }
    counts.missing_before_final_check = counts.missing;
    if (diagnostics) {
      diagnostics->reached_marking = marking;
      diagnostics->reached_marking_keys = keys;
    }
    for (std::size_t place = 0; place < marking.size(); ++place) {
      const auto required = static_cast<std::uint64_t>(final_[place]);
      counts.consumed += required;
      counts.places[place].consumed += required;
      if (marking[place] >= final_[place]) {
        marking[place] -= final_[place];
      } else {
        const auto missing = static_cast<std::uint64_t>(final_[place] - marking[place]);
        counts.missing += missing;
        counts.places[place].missing += missing;
        marking[place] = 0;
      }
      if (marking[place] > 0) {
        const auto tokens = static_cast<std::uint64_t>(marking[place]);
        counts.remaining += tokens;
        counts.places[place].remaining += tokens;
      }
    }
    return counts;
  }

  struct BackwardsOutcome {
    Counts counts;
    bool fitting = true;
    Marking marking;
  };

  // Legacy backwards token replay. Unlike forward replay, unknown
  // labels are ignored and silent routing is discovered by walking required
  // input markings backwards from a visible transition.
  [[nodiscard]] BackwardsOutcome replay_backwards(
      const std::vector<std::optional<std::size_t>>& trace,
      std::vector<std::uint64_t>* executions = nullptr,
      bool stop_immediately_unfit = false,
      bool walk_through_hidden_transitions = true,
      TokenReplayTraceDiagnostics* diagnostics = nullptr) const {
    Marking marking = initial_marking();
    BackwardsOutcome outcome;
    auto& counts = outcome.counts;
    counts.places.resize(net_.place_count());
    for (std::size_t place = 0; place < initial_.size(); ++place) {
      const auto tokens = static_cast<std::uint64_t>(initial_[place]);
      counts.produced += tokens;
      counts.places[place].produced += tokens;
    }

    for (const auto& step : trace) {
      if (diagnostics) {
        diagnostics->event_diagnostics.push_back({diagnostics->replayed_events, {}, {}});
        ++diagnostics->replayed_events;
      }
      // The deprecated variation silently discards labels absent from the net.
      if (!step || transitions_by_activity_[*step].empty()) {
        if (diagnostics)
          diagnostics->unknown_event_indices.push_back(diagnostics->replayed_events - 1);
        continue;
      }
      const auto& targets = transitions_by_activity_[*step];
      bool fired_any = false;
      for (const std::size_t t : targets) {
        // Check each duplicate-label transition against the evolving marking;
        // an earlier firing may enable a later transition.
        if (enabled(marking, t)) {
          fire(marking, t, counts);
          if (executions) ++(*executions)[t];
          if (diagnostics) {
            diagnostics->activated_transitions.push_back(t);
            diagnostics->event_diagnostics.back().activated_transitions.push_back(t);
          }
          fired_any = true;
        }
      }
      if (fired_any) {
        continue;
      }

      if (targets.size() != 1) {
        outcome.fitting = false;  // ambiguous duplicate label: interrupt trace
        if (diagnostics) diagnostics->replay_stopped = true;
        break;
      }

      const std::size_t visible = targets.front();
      const auto path = walk_through_hidden_transitions
                            ? reverse_silent_path(marking, visible)
                            : std::nullopt;
      if (path) {
        for (const std::size_t s : *path) {
          fire(marking, s, counts);
          if (executions) ++(*executions)[s];
          if (diagnostics) {
            diagnostics->activated_transitions.push_back(s);
            diagnostics->event_diagnostics.back().activated_transitions.push_back(s);
          }
        }
        fire(marking, visible, counts);
        if (executions) ++(*executions)[visible];
        if (diagnostics) {
          diagnostics->activated_transitions.push_back(visible);
          diagnostics->event_diagnostics.back().activated_transitions.push_back(visible);
        }
      } else {
        // No route: weak-fire the unique visible transition, creating any
        // missing inputs and retaining the deviation for fitness.
        fire(marking, visible, counts);
        if (executions) ++(*executions)[visible];
        if (diagnostics) {
          diagnostics->activated_transitions.push_back(visible);
          diagnostics->transitions_with_problems.push_back(visible);
          diagnostics->event_diagnostics.back().activated_transitions.push_back(visible);
          diagnostics->event_diagnostics.back().transitions_with_problems.push_back(visible);
        }
        outcome.fitting = false;
        if (stop_immediately_unfit) {
          if (diagnostics) diagnostics->replay_stopped = true;
          break;
        }
      }
    }

    // Compare the exact requested final marking, with no end routing in the
    // legacy variation.
    for (std::size_t p = 0; p < marking.size(); ++p) {
      const int target = final_[p];
      if (target > 0 && marking[p] > 0) {
        const auto available = static_cast<std::uint64_t>(marking[p]);
        counts.consumed += available;
        counts.places[p].consumed += available;
      }
      if (marking[p] > target) {
        const auto value = static_cast<std::uint64_t>(marking[p] - target);
        counts.remaining += value;
        counts.places[p].remaining += value;
      } else if (marking[p] < target) {
        const auto value = static_cast<std::uint64_t>(target - marking[p]);
        counts.missing += value;
        counts.places[p].missing += value;
      }
    }
    if (counts.missing != 0 || counts.remaining != 0) outcome.fitting = false;
    outcome.marking = marking;
    if (diagnostics) diagnostics->reached_marking = marking;
    return outcome;
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
    if (!marking_covers(marking, final_)) return std::nullopt;
    for (std::size_t place = 0; place < marking.size(); ++place) {
      marking[place] -= final_[place];
    }
    for (const int tokens : marking) {
      if (tokens > 0) return std::nullopt;
    }
    steps.push_back(finish.fired);
    return steps;
  }

  [[nodiscard]] Marking initial_marking() const { return initial_; }

  // Advances a precision prefix using its enabled-state routing cache. The
  // forward trace replay separately retains partial structural routes and
  // key-aware cleaning; precision's prefix contract is independently audited.
  [[nodiscard]] bool advance(
      Marking& marking, std::optional<std::size_t> activity,
      bool walk_through_hidden_transitions = true) const {
    // Activities absent from the model are ignored by evaluator replay.
    if (!activity || transitions_by_activity_[*activity].empty()) return true;
    const auto& step = cached_step(marking, *activity, walk_through_hidden_transitions);
    marking = step.marking;
    return step.missing == 0;
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
    // transitions. The exact answer needs a breadth-first silent closure
    // (bounded so a pathological model cannot loop forever) — but on a net
    // whose silent structure interleaves several tokens, that closure used to
    // run to its cap on every distinct marking, which dominated whole
    // conformance checks. Two sound cuts keep it honest and fast: activities
    // the relaxed reachability disproves are dropped before the search, and
    // the search stops the moment every surviving candidate has been found.
    std::unordered_set<std::size_t> acts;
    const auto reachable = relaxed_silent_places(marking);
    std::vector<std::size_t> remaining;
    for (std::size_t a = 0; a < transitions_by_activity_.size(); ++a) {
      bool possible = false;
      for (const std::size_t t : transitions_by_activity_[a]) {
        if (relaxed_enables(reachable, t)) {
          possible = true;
          break;
        }
      }
      if (!possible) continue;  // provably not enableable from here
      if (any_enabled(marking, transitions_by_activity_[a])) {
        acts.insert(a);
      } else {
        remaining.push_back(a);
      }
    }
    if (remaining.empty()) return acts;

    const auto collect = [&](const Marking& m) {
      for (std::size_t i = 0; i < remaining.size();) {
        if (any_enabled(m, transitions_by_activity_[remaining[i]])) {
          acts.insert(remaining[i]);
          remaining[i] = remaining.back();
          remaining.pop_back();
        } else {
          ++i;
        }
      }
    };

    std::unordered_set<Marking, MarkingHash> seen{marking};
    std::queue<Marking> frontier;
    frontier.push(marking);
    std::size_t expanded = 0;
    while (!frontier.empty() && !remaining.empty() &&
           expanded++ < kSilentSearchCap) {
      const Marking current = std::move(frontier.front());
      frontier.pop();
      for (const std::size_t s : silent_transitions_) {
        if (!enabled(current, s)) continue;
        Marking next = current;
        Counts scratch;
        fire(next, s, scratch);
        if (!seen.insert(next).second) continue;
        collect(next);
        if (remaining.empty()) break;
        frontier.push(std::move(next));
      }
    }
    return acts;
  }

  [[nodiscard]] std::vector<std::size_t> enabled_transition_ids(
      const Marking& marking,
      const std::vector<std::string>& configured_transition_names) const {
    if (net_.has_extended_arcs()) {
      std::vector<std::size_t> result;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!net_.transitions()[t].silent() &&
            silent_path(marking, [&](const Marking& m) { return enabled(m, t); }))
          result.push_back(t);
      }
      return result;
    }

    const auto transition_names = detail::replay_element_names(
        configured_transition_names, net_.transitions().size(), "t",
        "transition_names");
    std::vector<std::size_t> order(net_.transitions().size());
    for (std::size_t t = 0; t < order.size(); ++t) order[t] = t;
    std::sort(order.begin(), order.end(), [&](const auto left, const auto right) {
      return std::tie(transition_names[left], left) <
             std::tie(transition_names[right], right);
    });

    std::vector<bool> visible_enabled(net_.transitions().size(), false);
    std::vector<std::optional<Marking>> latest_marking(net_.transitions().size());
    std::queue<std::size_t> pending;
    std::vector<std::unordered_set<Marking, MarkingHash>> visited(
        net_.transitions().size());
    const auto transition_count = net_.transitions().size();
    const auto expansion_slots = kSilentSearchCap + 1;
    const std::size_t worklist_cap =
        transition_count != 0 &&
                expansion_slots > std::numeric_limits<std::size_t>::max() /
                    transition_count
            ? std::numeric_limits<std::size_t>::max()
            : expansion_slots * transition_count;
    const auto enqueue = [&](std::size_t transition) {
      if (pending.size() >= worklist_cap)
        throw std::runtime_error("enabled transition diagnostic worklist exceeded limit");
      pending.push(transition);
    };
    const auto discover = [&](const Marking& reached) {
      // The public helper walks a FIFO worklist.  Every newly reached marking
      // refreshes the latest state for each enabled silent transition and
      // appends that transition, including duplicates, to the worklist.
      for (const auto transition : order) {
        if (!enabled(reached, transition)) continue;
        if (!net_.transitions()[transition].silent()) {
          visible_enabled[transition] = true;
          continue;
        }
        latest_marking[transition] = reached;
        enqueue(transition);
      }
    };
    discover(marking);

    // A token-producing silent cycle can generate infinitely many distinct
    // markings.  Bound diagnostic-only exploration just as the other silent
    // searches are bounded, while keeping finite self-loops terminating via
    // the transition-plus-marking visited key.
    std::size_t expanded = 0;
    while (!pending.empty()) {
      const auto transition = pending.front();
      pending.pop();
      const auto& latest = *latest_marking[transition];
      if (!visited[transition].insert(latest).second) continue;
      if (expanded++ >= kSilentSearchCap)
        throw std::runtime_error("enabled transition diagnostic search exceeded limit");
      Marking next = latest;
      Counts scratch;
      fire(next, transition, scratch);
      discover(next);
    }

    // The public helper returns a set. Keep the existing native diagnostic
    // ordering stable by emitting transition IDs in index order.
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < visible_enabled.size(); ++t)
      if (visible_enabled[t]) result.push_back(t);
    return result;
  }

 private:
  ForwardConfiguration prepare_forward(const TokenReplayOptions& options) const {
    ForwardConfiguration result;
    result.initial_keys = detail::resolve_replay_marking_keys(options.initial_marking_keys, initial_, "initial_marking_keys");
    result.final_keys = detail::resolve_replay_marking_keys(options.final_marking_keys, final_, "final_marking_keys");
    result.place_names = detail::replay_element_names(options.place_names, net_.place_count(), "p", "place_names");
    result.transition_names = detail::replay_element_names(options.transition_names, net_.transitions().size(), "t", "transition_names");
    if (options.cleaning_token_flood)
      result.components = detail::cleaning_components(net_, result.initial_keys, result.final_keys);
    return result;
  }

  void forward_fire(Marking& marking, detail::ReplayMarkingKeys& keys,
                    std::size_t transition, Counts& counts) const {
    Marking required(marking.size(), 0);
    for (const auto place : net_.transitions()[transition].inputs) {
      if (required[place] == std::numeric_limits<int>::max())
        throw std::overflow_error("token replay input multiplicity exceeds signed range");
      ++required[place];
    }
    for (std::size_t place = 0; place < marking.size(); ++place) {
      if (marking[place] >= required[place]) continue;
      const auto missing = static_cast<std::uint64_t>(required[place] - marking[place]);
      counts.missing += missing;
      counts.places[place].missing += missing;
      // Public forward repair adds the full input weight before consumption,
      // retaining any partially available tokens after the firing.
      if (required[place] > std::numeric_limits<int>::max() - marking[place])
        throw std::overflow_error("token replay marking exceeds signed range");
      marking[place] += required[place];
    }
    fire(marking, transition, counts);
    const auto& fired = net_.transitions()[transition];
    for (const auto place : fired.inputs) keys[place] = marking[place] != 0;
    for (const auto place : fired.reset_inputs) keys[place] = marking[place] != 0;
    for (const auto place : fired.outputs) keys[place] = marking[place] != 0;
  }

  template <class Emit>
  void forward_route_hidden(Marking& marking, detail::ReplayMarkingKeys& keys,
                            std::size_t target,
                            const std::vector<std::string>& place_names,
                            bool exhaustive,
                            const Emit& emit) const {
    const auto required = [&](std::size_t selected) {
      Marking needed(marking.size(), 0);
      for (const auto place : net_.transitions()[selected].inputs) {
        if (needed[place] == std::numeric_limits<int>::max())
          throw std::overflow_error("token replay input multiplicity exceeds signed range");
        ++needed[place];
      }
      for (std::size_t place = 0; place < needed.size(); ++place)
        if (marking[place] >= needed[place]) needed[place] = 0;
      return detail::resolve_replay_marking_keys({}, needed, "required");
    };
    const auto candidates = [&](std::size_t /*selected*/,
                                const detail::ReplayMarkingKeys& required_keys) {
      return detail::silent_place_paths(keys, required_keys, net_, place_names);
    };
    const auto trial = [&](std::size_t selected, const detail::HiddenPlacePath& path,
                           const std::vector<bool>& visited) {
      Marking trial_marking = marking;
      auto trial_keys = keys;
      Counts scratch;
      scratch.places.resize(trial_marking.size());
      std::vector<std::size_t> accepted;
      for (const auto transition : path.transitions) {
        if (visited[transition]) continue;
        if (!enabled(trial_marking, transition)) break;
        // The transition is enabled, so forward_fire performs the same
        // checked marking/key update without contributing diagnostics or
        // usage counts to the real replay.
        forward_fire(trial_marking, trial_keys, transition, scratch);
        accepted.push_back(transition);
      }
      if (!enabled(trial_marking, selected)) accepted.clear();
      return accepted;
    };
    detail::route_hidden_dependencies(net_.transitions().size(), target, exhaustive,
        candidates, required,
        [&](auto transition) { return enabled(marking, transition); }, emit,
        trial);
  }

  // One precision/clean-run search step's outcome — its marking and token
  // bookkeeping it added. Replay is a deterministic function of the marking
  // and the activity, and real logs repeat the same steps constantly (shared
  // variant prefixes, recurring states), so each distinct step is computed
  // (with its silent-routing search) once and replayed from the cache after
  // that. These caches support prefix precision and clean-run searches; the
  // forward diagnostics path also retains incomplete routing and cleaning.
  struct Step {
    Marking marking;
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;
    std::uint64_t missing = 0;
    std::vector<TokenReplayPlaceDiagnostics> places;
    std::vector<std::size_t> fired;  // transitions fired, silent routing included
  };

  struct StepKeyHash {
    std::size_t operator()(
        const std::pair<Marking, std::size_t>& key) const noexcept {
      return MarkingHash{}(key.first) ^ (key.second * 0x9e3779b97f4a7c15ull);
    }
  };

  [[nodiscard]] const Step& cached_step(const Marking& marking,
                                        std::size_t activity,
                                        bool walk_through_hidden_transitions = true) const {
    auto& cache = walk_through_hidden_transitions ? step_cache_ : direct_step_cache_;
    const auto [it, inserted] =
        cache.try_emplace(std::make_pair(marking, activity));
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      const auto& targets = transitions_by_activity_[activity];
      if (walk_through_hidden_transitions)
        route_silently(step.marking, targets, counts, &step.fired);
      const std::size_t chosen = pick_enabled(step.marking, targets);
      fire(step.marking, chosen, counts);
      step.fired.push_back(chosen);
      step.produced = counts.produced;
      step.consumed = counts.consumed;
      step.missing = counts.missing;
      step.places = std::move(counts.places);
    }
    return it->second;
  }

  // The end-of-trace routing toward the requested final marking, cached like
  // an ordinary replay step.
  [[nodiscard]] const Step& cached_finish(const Marking& marking) const {
    const auto [it, inserted] = finish_cache_.try_emplace(marking);
    if (inserted) {
      Step& step = it->second;
      step.marking = marking;
      Counts counts;
      route_silently_to_marking(step.marking, counts, &step.fired);
      step.produced = counts.produced;
      step.consumed = counts.consumed;
      step.missing = counts.missing;
      step.places = std::move(counts.places);
    }
    return it->second;
  }

  // Detects whether the exact per-place closure applies (see
  // enabled_activities) and precomputes, for every place, which activities a
  // single token there can eventually enable through silent firings.
  void build_place_closure() {
    for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
      const auto& transition = net_.transitions()[t];
      const bool ok = !transition.has_extended_arcs() &&
                      (transition.silent()
                          ? transition.inputs.size() == 1 &&
                                transition.outputs.size() == 1
                          : transition.inputs.size() == 1);
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

  // The places a token could ever occupy from `marking` through silent
  // firings when token counts are ignored — a cheap over-approximation of the
  // silent closure. Sound by induction: a real firing needs every input place
  // tokened, so every place a real silent run can mark lands in the fixpoint.
  // What the fixpoint rules out is therefore truly unreachable, which lets
  // the exact (and expensive) searches skip provably hopeless goals.
  [[nodiscard]] std::vector<char> relaxed_silent_places(
      const Marking& marking) const {
    std::vector<char> reachable(marking.size(), 0);
    for (std::size_t p = 0; p < marking.size(); ++p) {
      reachable[p] = marking[p] > 0 ? 1 : 0;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (const std::size_t s : silent_transitions_) {
        if (!relaxed_enables(reachable, s)) continue;
        for (const std::size_t place : net_.transitions()[s].outputs) {
          if (!reachable[place]) {
            reachable[place] = 1;
            changed = true;
          }
        }
      }
    }
    return reachable;
  }

  [[nodiscard]] bool relaxed_enables(const std::vector<char>& reachable,
                                     std::size_t t) const {
    for (const std::size_t place : net_.transitions()[t].inputs) {
      if (!reachable[place]) return false;
    }
    return true;
  }

  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    return petri_transition_enabled(net_.transitions()[t], marking);
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
    if (counts.places.size() < marking.size())
      counts.places.resize(marking.size());
    const auto& transition = net_.transitions()[t];
    for (const std::size_t place : transition.inputs) {
      if (marking[place] > 0) {
        --marking[place];
      } else {
        ++counts.missing;  // token had to be created to fire
        ++counts.places[place].missing;
      }
      ++counts.consumed;
      ++counts.places[place].consumed;
    }
    for (const std::size_t place : transition.reset_inputs) {
      if (marking[place] > 0) {
        const auto value = static_cast<std::uint64_t>(marking[place]);
        counts.consumed += value;
        counts.places[place].consumed += value;
        marking[place] = 0;
      }
    }
    for (const std::size_t place : transition.outputs) {
      if (marking[place] == std::numeric_limits<int>::max())
        throw std::overflow_error("token replay marking exceeds signed range");
      ++marking[place];
      ++counts.produced;
      ++counts.places[place].produced;
    }
  }

  static void add_place_counts(
      std::vector<TokenReplayPlaceDiagnostics>& target,
      const std::vector<TokenReplayPlaceDiagnostics>& source) {
    if (target.size() < source.size()) target.resize(source.size());
    for (std::size_t place = 0; place < source.size(); ++place) {
      target[place].missing += source[place].missing;
      target[place].remaining += source[place].remaining;
      target[place].consumed += source[place].consumed;
      target[place].produced += source[place].produced;
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

  // Reverse silent routing used by the deprecated backwards variation. The
  // search state is the marking required immediately before the visible
  // transition. A silent transition can precede it when its output multiset
  // covers that requirement; inverse application removes available output
  // tokens and adds the transition's inputs. A global visited-transition set,
  // marking deduplication, and the expansion cap keep this bounded/deterministic.
  [[nodiscard]] std::optional<std::vector<std::size_t>> reverse_silent_path(
      const Marking& marking, std::size_t visible) const {
    const auto& target = net_.transitions()[visible];
    if (target.has_extended_arcs()) return std::nullopt;
    Marking required(marking.size(), 0);
    for (const std::size_t p : target.inputs) ++required[p];
    const auto covers = [](const Marking& have, const Marking& need) {
      for (std::size_t p = 0; p < have.size(); ++p) {
        if (have[p] < need[p]) return false;
      }
      return true;
    };
    if (covers(marking, required)) return std::vector<std::size_t>{};

    constexpr std::size_t kStart = std::numeric_limits<std::size_t>::max();
    struct Visited {
      Marking required;
      std::size_t transition;
      std::size_t parent;
    };
    std::vector<Visited> arena;
    std::unordered_set<Marking, MarkingHash> seen{required};
    std::queue<std::size_t> frontier;
    frontier.push(kStart);
    std::vector<char> expanded(net_.transitions().size(), 0);
    const auto reconstruct = [&](std::size_t index) {
      std::vector<std::size_t> path;
      for (std::size_t i = index; i != kStart; i = arena[i].parent) {
        path.push_back(arena[i].transition);
      }
      // Parent links lead from the discovered predecessor toward the visible
      // requirement, which is already forward firing order here.
      return path;
    };

    // The global transition-visit rule bounds the search by the number of
    // silent transitions, so the legacy variation needs no heuristic cap.
    while (!frontier.empty()) {
      const std::size_t index = frontier.front();
      frontier.pop();
      const Marking current = index == kStart ? required : arena[index].required;
      for (const std::size_t s : silent_transitions_) {
        if (expanded[s]) continue;
        const auto& transition = net_.transitions()[s];
        if (transition.has_extended_arcs()) continue;
        bool output_covers = true;
        Marking output_count(marking.size(), 0);
        for (const std::size_t p : transition.outputs) ++output_count[p];
        for (std::size_t p = 0; p < current.size(); ++p) {
          if (output_count[p] < current[p]) {
            output_covers = false;
            break;
          }
        }
        if (!output_covers) continue;
        expanded[s] = 1;

        Marking predecessor = current;
        // Subtract an output bundle only when the requirement carries its full
        // arc weight. Repeated place ids are Minera's representation of a
        // weighted arc and therefore must be handled as one bundle.
        for (std::size_t p = 0; p < predecessor.size(); ++p) {
          if (output_count[p] != 0 &&
              predecessor[p] >= output_count[p]) {
            predecessor[p] -= output_count[p];
          }
        }
        for (const std::size_t p : transition.inputs) ++predecessor[p];
        if (!seen.insert(predecessor).second) continue;
        arena.push_back(Visited{predecessor, s, index});
        const std::size_t found = arena.size() - 1;
        if (covers(marking, arena[found].required)) {
          auto path = reconstruct(found);
          return path;
        }
        frontier.push(found);
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
    // A search that cannot succeed would still run to its cap; the relaxed
    // fixpoint disproves most of those misses for the price of one pass.
    const auto reachable = relaxed_silent_places(marking);
    if (std::none_of(targets.begin(), targets.end(), [&](std::size_t t) {
          return relaxed_enables(reachable, t);
        })) {
      return;  // provably no silent route; the visible firing creates tokens
    }
    const auto path = silent_path(
        marking, [&](const Marking& m) { return any_enabled(m, targets); });
    if (!path) return;  // no silent route; the visible firing will create tokens
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  void route_silently_to_marking(
      Marking& marking, Counts& counts,
      std::vector<std::size_t>* fired = nullptr) const {
    if (marking_has_final_places(marking, final_)) return;
    const auto path = silent_path(
        marking, [&](const Marking& candidate) {
          return marking_has_final_places(candidate, final_);
        });
    if (!path) return;
    for (const std::size_t s : *path) fire(marking, s, counts);
    if (fired) fired->insert(fired->end(), path->begin(), path->end());
  }

  const PetriNet& net_;
  Marking initial_;
  Marking final_;
  std::optional<ForwardConfiguration> forward_configuration_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  std::vector<std::vector<std::size_t>> transitions_by_activity_;
  std::vector<std::size_t> silent_transitions_;
  mutable std::unordered_map<std::pair<Marking, std::size_t>, Step, StepKeyHash>
      step_cache_, direct_step_cache_;
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
  std::vector<std::size_t> null_activity_indices;
};

bool standard_activity_attribute(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

std::string selected_activity(const Event& event,
                              std::string_view activity_attribute) {
  if (standard_activity_attribute(activity_attribute)) return event.activity;
  if (const auto* value = event.attribute(activity_attribute)) return *value;
  throw std::invalid_argument(
      "conformance: event is missing activity attribute '" +
      std::string(activity_attribute) + "'");
}

std::optional<std::string> selected_replay_activity(
    const Event& event, std::string_view selector) {
  if (selector.empty()) selector = "concept:name";
  const EventAttribute* selected = nullptr;
  int priority = 0;
  std::size_t matches = 0;
  for (const auto& attribute : event.attributes) {
    const int candidate = attribute.key == selector ? 3
        : !attribute.first_class_source_key.empty() &&
          attribute.first_class_source_key == selector ? 2 : 0;
    if (candidate == 0 || candidate < priority) continue;
    if (candidate > priority) {
      selected = &attribute;
      priority = candidate;
      matches = 1;
    } else {
      ++matches;
    }
  }
  if (matches > 1)
    throw std::invalid_argument("conformance: duplicate selected activity attribute");
  if (selected) {
    if (selected->first_class_role != FirstClassAttributeRole::kNone &&
        selected->first_class_state == FirstClassAttributeState::kMissing)
      throw std::invalid_argument("conformance: selected activity attribute is missing");
    if (selected->is_null ||
        selected->first_class_state == FirstClassAttributeState::kNull)
      return std::nullopt;
    return selected->value;
  }
  if (standard_activity_attribute(selector) ||
      (!event.activity_source_key.empty() && event.activity_source_key == selector)) {
    if (event.activity_state == FirstClassAttributeState::kNull) return std::nullopt;
    if (event.has_activity()) return event.activity;
  }
  throw std::invalid_argument("conformance: event is missing activity attribute '" +
                              std::string(selector) + "'");
}

std::vector<Variant> collect_replay_variants(
    const EventLog& log, const Replayer& resolver, std::string_view selector) {
  std::vector<Variant> variants;
  std::map<std::vector<std::optional<std::string>>, std::size_t> index;
  for (const auto& trace : log.traces) {
    std::vector<std::optional<std::string>> key;
    Variant variant;
    for (const auto& event : trace.events) {
      auto activity = selected_replay_activity(event, selector);
      if (!activity) variant.null_activity_indices.push_back(variant.names.size());
      variant.ids.push_back(activity ? resolver.activity_id(*activity) : std::nullopt);
      variant.names.push_back(activity.value_or(""));
      key.push_back(std::move(activity));
    }
    const auto [it, inserted] = index.emplace(std::move(key), variants.size());
    if (inserted) {
      variant.count = 1;
      variants.push_back(std::move(variant));
    } else {
      ++variants[it->second].count;
    }
  }
  return variants;
}

// `resolver.activity_id(name)` maps an activity name to its model id (or
// std::nullopt when absent), so this works for any net wrapper.
template <typename Resolver>
std::vector<Variant> collect_variants(const EventLog& log,
                                      const Resolver& resolver,
                                      std::string_view activity_attribute =
                                          "concept:name") {
  std::vector<Variant> variants;
  std::unordered_map<std::string, std::size_t> index;
  for (const auto& trace : log.traces) {
    std::string key;
    Variant variant;
    for (const auto& event : trace.events) {
      const auto activity = selected_activity(event, activity_attribute);
      variant.ids.push_back(resolver.activity_id(activity));
      variant.names.push_back(activity);
      key.push_back('\x1f');
      key += activity;
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
                         const Replayer& replayer,
                         TokenReplayVariant replay_variant,
                         bool stop_immediately_unfit,
                         bool walk_through_hidden_transitions) {
  // Prefix automaton as a trie: each node is a distinct prefix and records how
  // many traces pass through it and which activities they go on to take.
  // Activities absent from the model all collapse onto one edge key, matching
  // how replay treats them (the marking does not move).
  constexpr std::size_t kAbsent = std::numeric_limits<std::size_t>::max();
  struct Node {
    std::unordered_map<std::size_t, std::size_t> children;  // edge -> node
    std::uint64_t freq = 0;
    std::unordered_set<std::size_t> observed;  // next activities the log takes
    std::vector<std::optional<std::size_t>> prefix;
  };
  std::vector<Node> nodes(1);
  for (const auto& variant : variants) {
    std::size_t current = 0;
    nodes[current].freq += variant.count;
    for (std::size_t depth = 0; depth < variant.ids.size(); ++depth) {
      const auto& id = variant.ids[depth];
      if (id) nodes[current].observed.insert(*id);
      const auto edge = id ? *id : kAbsent;
      const auto existing = nodes[current].children.find(edge);
      std::size_t child = 0;
      if (existing == nodes[current].children.end()) {
        child = nodes.size();
        auto child_prefix = nodes[current].prefix;
        child_prefix.push_back(id);
        nodes[current].children.emplace(edge, child);
        nodes.emplace_back();
        nodes.back().prefix = std::move(child_prefix);
      } else {
        child = existing->second;
      }
      current = child;
      // The evaluator scores proper prefixes only. A node reached by the final
      // event is terminal and contributes no weight; a shorter variant that
      // shares a prefix contributes only when another event follows it.
      if (depth + 1 < variant.ids.size()) nodes[current].freq += variant.count;
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

  const auto score = [&](const Node& node, Marking marking) {
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
  };

  if (replay_variant == TokenReplayVariant::kBackwards) {
    score(nodes.front(), replayer.initial_marking());
    for (std::size_t index = 1; index < nodes.size(); ++index) {
      const auto outcome = replayer.replay_backwards(
          nodes[index].prefix, nullptr, stop_immediately_unfit,
          walk_through_hidden_transitions);
      if (outcome.fitting) score(nodes[index], outcome.marking);
    }
  } else {
    // Depth-first walk carrying the marking: a child's marking is its parent's
    // advanced by one replay step, so each trie node costs one step rather than
    // a from-scratch replay of its whole prefix. Evaluator precision discards
    // a prefix as soon as replay needs a missing token.
    struct Frame {
      std::size_t index = 0;
      Marking marking;
      bool fitting = true;
    };
    std::vector<Frame> stack;
    stack.push_back({0, replayer.initial_marking(), true});
    while (!stack.empty()) {
      auto frame = std::move(stack.back());
      stack.pop_back();
      const Node& node = nodes[frame.index];
      if (!stop_immediately_unfit || frame.fitting) {
        score(node, frame.marking);
      }
      if (stop_immediately_unfit && !frame.fitting) continue;
      for (const auto& [edge, child] : node.children) {
        Marking next = frame.marking;
        const bool fitting = replayer.advance(
            next, edge == kAbsent
                      ? std::nullopt
                      : std::optional<std::size_t>(edge),
            walk_through_hidden_transitions);
        stack.push_back({child, std::move(next), fitting});
      }
    }
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
      if (d + 1 < variant.ids.size()) {
        nodes[current].freq += variant.count;
        nodes[current].markings.insert(markings[d + 1]);
      }
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
    // The evaluator penalises transitions which were never exercised as strongly as
    // a transition seen once.  Silent transitions are included by the caller
    // (their firings are part of the replay), so this remains a model-level
    // measure rather than an activity-only shortcut.
    rarity += count == 0 ? 1.0 : 1.0 / std::sqrt(double(count));
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
    // Inputs/outputs may contain repeated place ids to encode a weighted arc;
    // arc-degree simplicity counts distinct arc objects, not multiplicity.
    std::unordered_set<std::size_t> in(transition.inputs.begin(), transition.inputs.end());
    std::unordered_set<std::size_t> out(transition.outputs.begin(), transition.outputs.end());
    in.insert(transition.reset_inputs.begin(), transition.reset_inputs.end());
    in.insert(transition.inhibitor_inputs.begin(), transition.inhibitor_inputs.end());
    arcs += in.size() + out.size();
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
  explicit Aligner(
      const PetriNet& net, const AlignmentOptions& options = AlignmentOptions{},
      std::chrono::steady_clock::time_point deadline =
          std::chrono::steady_clock::time_point::max())
      : net_(net),
        initial_(resolve_workflow_marking(net, options.initial_marking, true)),
        final_(resolve_workflow_marking(net, options.final_marking, false)) {
    for (std::size_t i = 0; i < net_.activities().size(); ++i) {
      name_to_activity_.emplace(net_.activities()[i], i);
    }
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

    if (deadline == std::chrono::steady_clock::time_point::max()) {
      const double limit = options.max_total_time_seconds > 0.0 &&
                                   options.max_time_seconds > 0.0
                               ? std::min(options.max_total_time_seconds,
                                          options.max_time_seconds)
                               : (options.max_total_time_seconds > 0.0
                                      ? options.max_total_time_seconds
                                      : options.max_time_seconds);
      if (limit > 0.0)
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                       std::chrono::duration<double>(limit));
    }
    bool preflight_exhausted = false;
    if (const auto cost = shortest_model_run(options, deadline, preflight_exhausted)) {
      reachable_ = true;
      empty_run_cost_ = *cost;
    }
    reachability_exhausted_ = preflight_exhausted;
  }

  // Whether the net's final marking is reachable from its initial marking — the
  // minimum bar for it to be a workflow net the aligner can score.
  [[nodiscard]] bool reachable() const { return reachable_; }

  [[nodiscard]] bool reachability_exhausted() const {
    return reachability_exhausted_;
  }

  [[nodiscard]] std::optional<std::size_t> activity_id(
      const std::string& name) const {
    const auto it = name_to_activity_.find(name);
    return it == name_to_activity_.end() ? std::nullopt
                                         : std::optional<std::size_t>(it->second);
  }

  // The fewest visible activities any complete model run fires — the model-move
  // half of the worst-case alignment cost used to normalize fitness.
  [[nodiscard]] std::uint64_t empty_run_cost() const { return empty_run_cost_; }

  [[nodiscard]] double weighted_empty_run_cost(const AlignmentOptions& options) const {
    const double model_default = options.default_model_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double silent_default = options.default_silent_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 1.0 : 0.0);
    auto transition_cost = [&](std::size_t t) {
      const auto& tr = net_.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
      auto it = options.model_costs.find(name);
      double value = tr.silent() ? silent_default
                                 : (it == options.model_costs.end() ? model_default : it->second);
      if (t < options.model_costs_by_transition.size()) value = options.model_costs_by_transition[t];
      if (options.model_cost_function) value = options.model_cost_function(name, t);
      return value;
    };
    Marking start = initial_;
    std::priority_queue<std::pair<double, Marking>,
                        std::vector<std::pair<double, Marking>>,
                        std::greater<>> queue;
    std::unordered_map<Marking, double, MarkingHash> best{{start, 0.0}};
    queue.push({0.0, start});
    std::size_t expanded = 0;
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap : options.max_states;
    const auto started = std::chrono::steady_clock::now();
    while (!queue.empty() && expanded++ < budget) {
      const double time_limit = options.max_total_time_seconds > 0.0 && options.max_time_seconds > 0.0
                                    ? std::min(options.max_total_time_seconds, options.max_time_seconds)
                                    : (options.max_total_time_seconds > 0.0 ? options.max_total_time_seconds
                                                                           : options.max_time_seconds);
      if (time_limit > 0.0 &&
          std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >=
              time_limit)
        return std::numeric_limits<double>::infinity();
      auto [cost, marking] = queue.top(); queue.pop();
      const auto found = best.find(marking);
      if (found == best.end() || cost > found->second + 1e-9) continue;
      if (marking == final_) return cost;
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        if (!enabled(marking, t)) continue;
        const double next_cost = cost + transition_cost(t);
        Marking next = fire(marking, t);
        const auto old = best.find(next);
        if (old != best.end() && old->second <= next_cost + 1e-9) continue;
        best[next] = next_cost;
        queue.push({next_cost, std::move(next)});
      }
    }
    return std::numeric_limits<double>::infinity();
  }

  struct Outcome {
    std::vector<AlignmentMove> moves;
    std::vector<AlignmentMove> complete_moves;
    double cost = 0.0;
    bool capped = false;  // search hit its bound before reaching the goal
    // The model marking the optimal alignment is in right before it consumes
    // log event d (after any model/silent moves scheduled first), for
    // d = 0..len-1, plus the final marking at index len. These are the states
    // alignment-based precision inspects. Empty when the search was capped.
    std::vector<Marking> prefix_markings;
    std::vector<ProjectedAlignmentMove> projected_model_moves;
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
    State start_key = initial_;
    start_key.push_back(0);
    State final_key = final_;
    final_key.push_back(static_cast<int>(len));

    // Distinct model-known activities of the trace, with their transition
    // matches. Trace positions carrying the same activity are interchangeable
    // for the marking-equation relaxation, so the heuristic below groups them
    // by activity: its rows scale with the alphabet instead of the remaining
    // trace length, and its value depends only on the marking and the
    // remaining activity counts — which is what makes it cacheable across
    // states and across variants.
    std::vector<std::size_t> slot_activity;  // slot -> activity id
    std::vector<std::vector<std::size_t>> slot_matches;
    std::unordered_map<std::size_t, std::size_t> slot_of;
    std::vector<std::vector<std::size_t>> matches(len);
    for (std::size_t d = 0; d < len; ++d) {
      if (!ids[d]) continue;
      const auto [it, inserted] = slot_of.try_emplace(*ids[d], slot_activity.size());
      if (inserted) {
        slot_activity.push_back(*ids[d]);
        std::vector<std::size_t> ts;
        for (std::size_t t = 0; t < tcount; ++t) {
          const auto& transition = net_.transitions()[t];
          if (!transition.silent() && *transition.activity == *ids[d]) {
            ts.push_back(t);
          }
        }
        slot_matches.push_back(std::move(ts));
      }
      matches[d] = slot_matches[it->second];
    }
    const std::size_t slots = slot_activity.size();

    // suffix_counts[d][slot]: occurrences of the slot's activity in the
    // remaining trace ids[d..len). Canonical slot order (ascending activity
    // id) keys the cross-variant cache, since slots are discovered in
    // per-variant order.
    std::vector<std::vector<std::uint32_t>> suffix_counts(
        len + 1, std::vector<std::uint32_t>(slots, 0));
    for (std::size_t d = len; d-- > 0;) {
      suffix_counts[d] = suffix_counts[d + 1];
      if (ids[d]) ++suffix_counts[d][slot_of.at(*ids[d])];
    }
    std::vector<std::size_t> canonical_slots(slots);
    for (std::size_t i = 0; i < slots; ++i) canonical_slots[i] = i;
    std::sort(canonical_slots.begin(), canonical_slots.end(),
              [&](std::size_t a, std::size_t b) {
                return slot_activity[a] < slot_activity[b];
              });

    // Marking-equation heuristic: the remaining alignment cost is bounded
    // from below by relaxing it to an LP over continuous move counts x >= 0.
    // Columns are the net's transitions fired as model moves plus, per
    // remaining activity, one aggregated synchronous column per matching
    // transition and one slack; the activity's row caps synchronous use at
    // its remaining count, with the slack standing for the positions aligned
    // as log moves instead. Every remaining position pays a log move up
    // front (the `constant` below) and each synchronous unit refunds one, so
    // the aggregated optimum equals the per-position formulation's — the
    // positions it merges are interchangeable. Every real completion is a
    // feasible integer solution, so the LP optimum never overestimates;
    // costs are integral, so its ceiling is still a lower bound. Infeasible
    // means the final marking is unreachable from this state: the branch can
    // be pruned outright.
    struct LpBound {
      std::uint64_t bound = 0;
      bool solved = false;  // false: bound only (cache hit or aborted solve)
      std::vector<double> model_use;  // x_t per transition, when solved
      std::vector<double> sync_use;   // per matches[pos] entry, when solved
      double log_slack = 0.0;         // unmet remaining count of ids[pos]
      bool log_free = false;  // ids[pos] absent from the model: log is exact
    };
    const auto heuristic = [&](const State& state) -> std::optional<LpBound> {
      if (net_.has_extended_arcs()) return LpBound{};
      const std::size_t pos = static_cast<std::size_t>(state.back());
      const auto& counts = suffix_counts[pos];
      const double constant = static_cast<double>(len - pos);

      // The bound is a function of (marking, remaining counts) alone, so one
      // solve serves every state — of any variant — that shares them. Each
      // value packs into one byte (they are tiny in practice), spilling to an
      // escaped five-byte form when one ever is not — a prefix code, so the
      // fixed decode order (pc markings, then activity/count pairs) keeps the
      // key injective while millions of cached bounds stay affordable.
      std::string key;
      key.reserve(pc + slots * 2 + 4);
      const auto pack = [&key](std::uint32_t value) {
        if (value < 0xFE) {
          key += static_cast<char>(value);
        } else {
          key += static_cast<char>(0xFF);
          key += static_cast<char>(value & 0xFF);
          key += static_cast<char>((value >> 8) & 0xFF);
          key += static_cast<char>((value >> 16) & 0xFF);
          key += static_cast<char>((value >> 24) & 0xFF);
        }
      };
      for (std::size_t p = 0; p < pc; ++p) {
        pack(static_cast<std::uint32_t>(state[p]));
      }
      for (const std::size_t slot : canonical_slots) {
        if (counts[slot] == 0) continue;
        pack(static_cast<std::uint32_t>(slot_activity[slot]));
        pack(counts[slot]);
      }
      const auto cached = bound_cache_.find(key);
      LpBound result;
      if (cached != bound_cache_.end()) {
        if (!cached->second) return std::nullopt;  // known infeasible
        result.bound = *cached->second;
      } else {
        std::size_t active = 0;
        std::size_t sync_cols = 0;
        for (std::size_t slot = 0; slot < slots; ++slot) {
          if (counts[slot] == 0) continue;
          ++active;
          sync_cols += slot_matches[slot].size();
        }
        const std::size_t rows = pc + active;
        const std::size_t vars = tcount + sync_cols + active;

        std::vector<std::vector<double>> a(rows,
                                           std::vector<double>(vars, 0.0));
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
        std::size_t row = pc;
        for (std::size_t slot = 0; slot < slots; ++slot) {
          if (counts[slot] == 0) continue;
          b[row] = static_cast<double>(counts[slot]);
          for (const std::size_t t : slot_matches[slot]) {
            for (const auto& [place, coef] : incidence_[t]) a[place][col] = coef;
            a[row][col] = 1.0;
            c[col] = -1.0;  // each synchronous unit refunds one paid log move
            ++col;
          }
          a[row][col] = 1.0;  // slack: positions left to align as log moves
          ++col;
          ++row;
        }

        auto lp = solve_lp(std::move(a), std::move(b), c);
        if (lp.aborted) return LpBound{};  // no usable bound, nothing dead
        if (!lp.feasible) {
          if (bound_cache_.size() < kAlignerBoundCacheCap) {
            bound_cache_.try_emplace(std::move(key), std::nullopt);
          }
          return std::nullopt;
        }
        result.bound = static_cast<std::uint64_t>(
            std::ceil(std::max(lp.objective + constant, 0.0) - 1e-6));
        if (bound_cache_.size() < kAlignerBoundCacheCap) {
          bound_cache_.try_emplace(std::move(key), result.bound);
        }

        // Unpack what the propagation needs for the current position.
        result.solved = true;
        result.model_use.assign(lp.solution.begin(),
                                lp.solution.begin() + tcount);
        if (pos < len && ids[pos]) {
          const std::size_t want = slot_of.at(*ids[pos]);
          std::size_t scan_col = tcount;
          for (std::size_t slot = 0; slot < slots; ++slot) {
            if (counts[slot] == 0) continue;
            if (slot == want) {
              result.sync_use.assign(
                  lp.solution.begin() + scan_col,
                  lp.solution.begin() + scan_col + slot_matches[slot].size());
              result.log_slack =
                  lp.solution[scan_col + slot_matches[slot].size()];
              break;
            }
            scan_col += slot_matches[slot].size() + 1;
          }
        }
      }
      if (pos < len && !ids[pos]) {
        // An activity the model lacks can only be a log move; consuming it
        // drops the constant by exactly one, so that step is always exact.
        result.log_free = true;
      }
      return result;
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
        std::int32_t transition = -1;
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
                             std::int32_t transition,
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
        entry.transition = transition;
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
        // predicted path exact, sparing them their own solve. A bound served
        // from the cache carries no solution; its children simply inherit.
        LpBound info;
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
          info = std::move(*solved);
        }

        entries[node.state].closed = true;
        ++expanded;

        const std::uint64_t g = entries[node.state].cost;
        const std::uint64_t h = entries[node.state].heuristic;
        const std::uint64_t h_less_one = h > 0 ? h - 1 : 0;
        const State& state = *by_id[node.state];
        const std::size_t pos = static_cast<std::size_t>(state.back());
        // A move the LP optimum provably takes (the aggregated positions are
        // interchangeable, so "some remaining position of this activity"
        // stands for the current one) makes the child's inherited bound its
        // exact LP value, sparing the child its own solve.
        const auto walked_model = [&](std::size_t t) {
          return info.solved && info.model_use[t] >= 1.0 - 1e-6;
        };
        const auto walked_sync = [&](std::size_t k) {
          return info.solved && k < info.sync_use.size() &&
                 info.sync_use[k] >= 1.0 - 1e-6;
        };
        const bool walked_log =
            info.log_free || (info.solved && info.log_slack >= 1.0 - 1e-6);

        // Log move: the trace advances while the model stands still.
        if (pos < len) {
          State next = state;
          next.back() = static_cast<int>(pos + 1);
          relax(std::move(next), g + 1, node.state, AlignmentMoveType::kLog,
                static_cast<std::int32_t>(pos), false, h_less_one,
                -1, !use_lp || walked_log);
        }

        // Model moves (and the synchronous move when the labels agree).
        for (std::size_t t = 0; t < tcount; ++t) {
          if (!enabled(state, t)) continue;
          const auto& transition = net_.transitions()[t];
          State fired = fire(state, t);
          if (transition.silent()) {
            relax(std::move(fired), g, node.state, AlignmentMoveType::kModel,
                  -1, true, h, static_cast<std::int32_t>(t), !use_lp || walked_model(t));
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
                  static_cast<std::int32_t>(t), !use_lp || walked_model(t));
            relax(std::move(synced), g, node.state,
                  AlignmentMoveType::kSynchronous, activity, false, h,
                  static_cast<std::int32_t>(t), !use_lp || walked_sync(k));
          } else {
            relax(std::move(fired), g + 1, node.state,
                  AlignmentMoveType::kModel, activity, false, h_less_one,
                  static_cast<std::int32_t>(t), !use_lp || walked_model(t));
          }
        }
      }

      if (!reached) return std::nullopt;

      Outcome outcome;
      outcome.cost = entries[final_id].cost;
      std::vector<std::uint32_t> path{final_id};
      for (std::uint32_t id = final_id; id != start_id;) {
        const Entry& entry = entries[id];
        const AlignmentMove move{
            entry.type,
            entry.silent
                ? std::string{}
                : (entry.type == AlignmentMoveType::kLog
                       ? names[static_cast<std::size_t>(entry.ref)]
                       : net_.activities()[static_cast<std::size_t>(entry.ref)]),
            entry.transition < 0 ? static_cast<std::size_t>(-1)
                                 : static_cast<std::size_t>(entry.transition)};
        outcome.complete_moves.push_back(move);
        if (!entry.silent) outcome.moves.push_back(move);
        id = entry.prev;
        path.push_back(id);
      }
      std::reverse(outcome.complete_moves.begin(), outcome.complete_moves.end());
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
      for (std::size_t i=0;i+1<path.size();++i) {
        const auto& e=entries[path[i+1]]; if (e.silent || e.type==AlignmentMoveType::kLog || e.transition<0) continue;
        outcome.projected_model_moves.push_back({net_.activities()[static_cast<std::size_t>(e.ref)], static_cast<std::size_t>(e.transition), strip(*by_id[path[i]]), strip(*by_id[path[i+1]])});
      }
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
      const AlignmentMove move{AlignmentMoveType::kLog, names[i]};
      outcome.moves.push_back(move);
      outcome.complete_moves.push_back(move);
    }
    outcome.cost = static_cast<std::uint64_t>(len) + empty_run_cost_;
    outcome.capped = true;
    return outcome;
  }

  struct ConfiguredOutcome {
    Outcome primary;
    std::vector<std::vector<AlignmentMove>> alternatives;
    AlignmentDiagnostics diagnostics;
    bool invalid_cost = false;
  };

  // Clean-room implementation of the Dijkstra/generator/discounted
  // variants.  The search deliberately keeps the product state explicit and
  // uses floating point edge costs, which is required by discounted A* and
  // custom cost functions.
  [[nodiscard]] ConfiguredOutcome align_configured(
      const std::vector<std::optional<std::size_t>>& ids,
      const std::vector<std::string>& names,
      const AlignmentOptions& input_options) const {
    AlignmentOptions options = input_options;
    bool callback_invalid = false;
    // Resolve callbacks exactly once for this concrete trace/model pair. The
    // search and LP then consume stable vectors, avoiding repeated/stateful
    // callback evaluation and making invalid values deterministic.
    if (options.model_cost_function || options.sync_cost_function) {
      for (std::size_t t = 0; t < net_.transitions().size(); ++t) {
        const auto& tr = net_.transitions()[t];
        const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
        if (options.model_cost_function) {
          const double value = options.model_cost_function(name, t);
          if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
          options.model_costs_by_transition.resize(net_.transitions().size());
          options.model_costs_by_transition[t] = value;
        }
        if (options.sync_cost_function && !tr.silent()) {
          const double value = options.sync_cost_function(name, t);
          if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
          options.sync_costs_by_transition.resize(net_.transitions().size());
          options.sync_costs_by_transition[t] = value;
        }
      }
      options.model_cost_function = {};
      options.sync_cost_function = {};
    }
    if (options.trace_cost_function) {
      options.trace_costs_by_position.resize(names.size());
      for (std::size_t i = 0; i < names.size(); ++i) {
        options.trace_costs_by_position[i] = options.trace_cost_function(names[i], i);
        if (!std::isfinite(options.trace_costs_by_position[i]) || options.trace_costs_by_position[i] < 0.0)
          callback_invalid = true;
      }
      options.trace_cost_function = {};
    }
    if (callback_invalid) {
      ConfiguredOutcome invalid;
      invalid.primary.capped = true;
      invalid.invalid_cost = true;
      return invalid;
    }
    using Clock = std::chrono::steady_clock;
    const bool generator =
        options.variant == AlignmentVariant::kGeneratorDijkstraNoHeuristics ||
        options.variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
    const bool less_memory =
        options.variant == AlignmentVariant::kDijkstraLessMemory ||
        options.variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
    const bool discounted = options.variant == AlignmentVariant::kDiscountedAStar;
    const double exponent = options.exponent > 1e-9 ? options.exponent : 1.0;
    const double default_trace = options.default_trace_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double default_model = options.default_model_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    const double default_sync = options.default_sync_cost.value_or(0.0);
    const double default_silent = options.default_silent_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 1.0 : 0.0);
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    const std::size_t alignment_limit = options.max_alignments == 0 ? 32 : options.max_alignments;
    const std::size_t len = ids.size();
    const std::size_t pc = net_.place_count();
    using State = Marking;
    State start = initial_;
    start.push_back(0);
    State goal = final_;
    goal.push_back(static_cast<int>(len));

    struct Node {
      double cost = 0.0;
      double priority = 0.0;
      std::uint64_t seq = 0;
      std::size_t depth = 0;
      bool discounted_order = false;
      State state;
      std::vector<AlignmentMove> moves;
      std::vector<Marking> prefixes;
      std::vector<State> path_states;
      bool operator>(const Node& o) const {
        if (priority != o.priority) return priority > o.priority;
        if (cost != o.cost) return cost > o.cost;
        if (discounted_order && depth != o.depth) return depth < o.depth;
        return seq > o.seq;
      }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<>> frontier;
    // The discounted variant closes product states (marking + trace
    // position), deliberately ignoring path depth.  This keeps cyclic models
    // finite and deterministic rather than chasing an unattained discounted
    // infimum through repeated positive-cost cycles.
    std::unordered_map<State, double, MarkingHash> best;
    std::unordered_map<Marking, std::vector<std::pair<std::size_t, double>>, MarkingHash>
        pareto_positions;
    best.emplace(start, 0.0);
    std::size_t lp_solved = 0;
    const auto lookup_cost = [](const std::unordered_map<std::string, double>& table,
                                const std::string& name, double fallback) {
      const auto it = table.find(name);
      return it == table.end() ? fallback : it->second;
    };
    const auto model_cost_for = [&](std::size_t t) {
      const auto& tr = net_.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net_.activities()[*tr.activity];
      const auto found = options.model_costs.find(name);
      double value = tr.silent()
                         ? default_silent
                         : (found == options.model_costs.end() ? default_model : found->second);
      if (t < options.model_costs_by_transition.size()) value = options.model_costs_by_transition[t];
      if (options.model_cost_function) value = options.model_cost_function(name, t);
      return value;
    };
    const auto trace_cost_for = [&](std::size_t d) {
      const std::string& name = names[d];
      double value = lookup_cost(options.trace_costs, name, default_trace);
      if (d < options.trace_costs_by_position.size()) value = options.trace_costs_by_position[d];
      if (options.trace_cost_function) value = options.trace_cost_function(name, d);
      return value;
    };
    const auto sync_cost_for = [&](std::size_t t) {
      const std::string& name = net_.activities()[*net_.transitions()[t].activity];
      double value = lookup_cost(options.sync_costs, name, default_sync);
      if (t < options.sync_costs_by_transition.size()) value = options.sync_costs_by_transition[t];
      if (options.sync_cost_function) value = options.sync_cost_function(name, t);
      return value;
    };
    const auto lp_heuristic = [&](const State& state) {
      // The marking-equation bound is in undiscounted units and may
      // overestimate a discounted suffix.  Discounted search therefore uses
      // a zero heuristic (Dijkstra), preserving optimal goal termination.
      if (net_.has_extended_arcs() ||
          options.variant != AlignmentVariant::kStateEquationAStar || discounted)
        return 0.0;
      const std::size_t pos = static_cast<std::size_t>(state.back());
      const std::size_t remaining = len - pos;
      const std::size_t tcount = net_.transitions().size();
      std::size_t sync_count = 0;
      for (std::size_t d = pos; d < len; ++d) {
        if (!ids[d]) continue;
        for (std::size_t t = 0; t < tcount; ++t)
          if (!net_.transitions()[t].silent() && *net_.transitions()[t].activity == *ids[d]) ++sync_count;
      }
      const std::size_t vars = tcount + remaining + sync_count;
      const std::size_t rows = pc + remaining;
      std::vector<std::vector<double>> a(rows, std::vector<double>(vars, 0.0));
      std::vector<double> b(rows, 0.0), c(vars, 0.0);
      for (std::size_t p = 0; p < pc; ++p) b[p] = double(final_[p] - state[p]);
      for (std::size_t t = 0; t < tcount; ++t) {
        for (const auto& [p, coef] : incidence_[t]) a[p][t] = coef;
        c[t] = model_cost_for(t);
      }
      std::size_t col = tcount;
      for (std::size_t d = pos; d < len; ++d) {
        const std::size_t row = pc + (d - pos);
        b[row] = 1.0;
        a[row][col] = 1.0;
        c[col] = trace_cost_for(d);
        ++col;
        if (!ids[d]) continue;
        for (std::size_t t = 0; t < tcount; ++t) {
          if (net_.transitions()[t].silent() || *net_.transitions()[t].activity != *ids[d]) continue;
          for (const auto& [p, coef] : incidence_[t]) a[p][col] = coef;
          a[row][col] = 1.0;
          c[col] = sync_cost_for(t);
          ++col;
        }
      }
      ++lp_solved;
      auto lp = solve_lp(std::move(a), std::move(b), c);
      if (lp.aborted) return 0.0;
      if (!lp.feasible) return std::numeric_limits<double>::infinity();
      return std::max(0.0, lp.objective);
    };
    frontier.push(Node{0.0, lp_heuristic(start), 0, 0, discounted, start, {}, {}, {start}});
    std::unordered_set<State, MarkingHash> discounted_closed;
    std::unordered_map<State, std::unordered_set<std::string>, MarkingHash>
        generator_seen;
    const auto start_time = Clock::now();
    std::size_t seq = 1;
    std::size_t expanded = 0;
    std::size_t arcs = 0;
    bool invalid_cost = false;
    std::size_t queued_total = 0;
    double goal_cost = std::numeric_limits<double>::infinity();
    std::vector<Node> goals;
    const auto move_key = [](const std::vector<AlignmentMove>& moves) {
      std::string key;
      for (const auto& m : moves) {
        key.push_back(static_cast<char>('0' + static_cast<int>(m.type)));
        key += std::to_string(m.transition);
        key.push_back(':');
        key += m.activity;
        key.push_back(';');
      }
      return key;
    };
    const auto timed_out = [&]() {
      return options.max_time_seconds > 0.0 &&
             std::chrono::duration<double>(Clock::now() - start_time).count() >=
                 options.max_time_seconds;
    };
    while (!frontier.empty() && expanded < budget && !timed_out()) {
      Node node = frontier.top();
      frontier.pop();
      const auto it_best = best.find(node.state);
      if (it_best == best.end() || node.cost > it_best->second + 1e-9) continue;
      if (discounted && !discounted_closed.insert(node.state).second) continue;
      if (!generator && node.state == goal) {
        if (node.prefixes.size() == len)
          node.prefixes.push_back(Marking(final_.begin(), final_.end()));
        goals.push_back(std::move(node));
        goal_cost = goals.front().cost;
        break;
      }
      if (generator && node.state == goal) {
        if (node.cost > goal_cost + 1e-9) break;
        if (!std::isfinite(goal_cost)) goal_cost = node.cost;
        if (node.prefixes.size() == len)
          node.prefixes.push_back(Marking(final_.begin(), final_.end()));
        const auto key = move_key(node.moves);
        auto& seen = generator_seen[node.state];
        if (seen.insert(key).second && goals.size() < alignment_limit) {
          goals.push_back(std::move(node));
        }
        if (goals.size() >= alignment_limit) break;
        continue;
      }
      ++expanded;
      const std::size_t pos = static_cast<std::size_t>(node.state.back());
      const double factor = discounted
                                ? std::pow(exponent, -static_cast<double>(node.depth))
                                : 1.0;
      auto push = [&](State next, double edge_cost, AlignmentMove move,
                      bool consume) {
        ++arcs;
        if (!std::isfinite(edge_cost) || edge_cost < 0.0) {
          invalid_cost = true;
          return;
        }
        Node child = node;
        child.state = std::move(next);
        if (discounted && discounted_closed.find(child.state) != discounted_closed.end()) return;
        if (generator && std::find(child.path_states.begin(), child.path_states.end(), child.state) !=
                             child.path_states.end()) return;
        child.path_states.push_back(child.state);
        child.depth = node.depth + 1;
        child.seq = seq++;
        child.cost = node.cost + edge_cost * factor;
        child.priority = child.cost + lp_heuristic(child.state);
        if (!std::isfinite(child.priority)) return;
        if (consume) {
          child.prefixes.push_back(Marking(node.state.begin(), node.state.end() - 1));
        }
        // Configured variants retain tau/model routing moves explicitly;
        // an empty activity denotes a silent transition.
        if (!move.activity.empty() || move.type == AlignmentMoveType::kModel)
          child.moves.push_back(std::move(move));
        if (less_memory) {
          Marking marking(child.state.begin(), child.state.end() - 1);
          const std::size_t child_pos = static_cast<std::size_t>(child.state.back());
          auto& frontier_points = pareto_positions[marking];
          for (const auto& [prior_pos, prior_cost] : frontier_points)
            if ((!generator || prior_pos > child_pos || prior_cost < child.cost - 1e-9) &&
                prior_pos >= child_pos && prior_cost <= child.cost + 1e-9) return;
          frontier_points.erase(
              std::remove_if(frontier_points.begin(), frontier_points.end(),
                             [&](const auto& point) {
                               return child_pos >= point.first &&
                                      child.cost <= point.second + 1e-9;
                             }),
              frontier_points.end());
          frontier_points.emplace_back(child_pos, child.cost);
        }
        const auto old = best.find(child.state);
        if (!generator) {
          if (old != best.end()) {
            if (child.cost > old->second + 1e-9) return;
            if (!discounted && old->second <= child.cost + 1e-9) return;
            if (child.cost >= old->second - 1e-9) {
              // Discounted equal-cost arrivals remain queued so depth tie
              // ordering can select the deeper pinned path.
              frontier.push(std::move(child));
              ++queued_total;
              return;
            }
          }
          best[child.state] = child.cost;
        } else if (old == best.end() || child.cost < old->second - 1e-9) {
          best[child.state] = child.cost;
        } else if (child.cost > old->second + 1e-9) {
          return;
        }
        frontier.push(std::move(child));
        ++queued_total;
      };
      // Explicit mode enumerates the raw log successor first, matching the
      // pinned non-synchronous product ordering; compact mode emits it after
      // model successors so synchronous destinations are coalesced eagerly.
      if (!options.synchronous_dijkstra && pos < len) {
        State next = node.state;
        next.back() = static_cast<int>(pos + 1);
        const std::string& activity = names[pos];
        double trace_cost = lookup_cost(options.trace_costs, activity, default_trace);
        if (pos < options.trace_costs_by_position.size()) trace_cost = options.trace_costs_by_position[pos];
        if (options.trace_cost_function) trace_cost = options.trace_cost_function(activity, pos);
        push(std::move(next), trace_cost, {AlignmentMoveType::kLog, activity}, true);
      }
      for (std::size_t order = 0; order < net_.transitions().size(); ++order) {
        const std::size_t t = options.synchronous_dijkstra
                                  ? order
                                  : net_.transitions().size() - 1 - order;
        if (!enabled(node.state, t)) continue;
        const auto& tr = net_.transitions()[t];
        State fired = node.state;
        fire_petri_transition(tr, fired);
        if (tr.silent()) {
          push(std::move(fired), model_cost_for(t),
               {AlignmentMoveType::kModel, {}, t}, false);
          continue;
        }
        const std::string& activity = net_.activities()[*tr.activity];
        double model_cost = lookup_cost(options.model_costs, activity, default_model);
        if (t < options.model_costs_by_transition.size())
          model_cost = options.model_costs_by_transition[t];
        if (options.model_cost_function) model_cost = options.model_cost_function(activity, t);
        push(fired, model_cost,
             {AlignmentMoveType::kModel, activity, t}, false);
        if (pos < len && ids[pos] && *ids[pos] == *tr.activity) {
          State synced = fired;
          synced.back() = static_cast<int>(pos + 1);
          double sync_cost = lookup_cost(options.sync_costs, activity, default_sync);
          if (t < options.sync_costs_by_transition.size())
            sync_cost = options.sync_costs_by_transition[t];
          if (options.sync_cost_function) sync_cost = options.sync_cost_function(activity, t);
          push(std::move(synced), sync_cost,
               {AlignmentMoveType::kSynchronous, activity, t}, true);
        }
      }
      if (options.synchronous_dijkstra && pos < len) {
        State next = node.state;
        next.back() = static_cast<int>(pos + 1);
        const std::string& activity = names[pos];
        double trace_cost = lookup_cost(options.trace_costs, activity, default_trace);
        if (pos < options.trace_costs_by_position.size())
          trace_cost = options.trace_costs_by_position[pos];
        if (options.trace_cost_function) trace_cost = options.trace_cost_function(activity, pos);
        push(std::move(next), trace_cost,
             {AlignmentMoveType::kLog, activity}, true);
      }
    }
    ConfiguredOutcome result;
    result.diagnostics.visited_states = expanded;
    result.diagnostics.queued_states = queued_total;
    result.diagnostics.traversed_arcs = arcs;
    result.diagnostics.lp_solved = lp_solved;
    result.diagnostics.closed_set_length = best.size();
    result.diagnostics.num_visited_markings = result.diagnostics.closed_set_length;
    if (goals.empty() || invalid_cost) {
      result.primary.capped = true;
      result.invalid_cost = invalid_cost;
      return result;
    }
    auto& primary = goals.front();
    result.primary.cost = primary.cost;
    for (std::size_t i=0; i<primary.moves.size() && i+1<primary.path_states.size(); ++i) {
      const auto& mv=primary.moves[i];
      if (mv.activity.empty() || mv.type==AlignmentMoveType::kLog || mv.transition==static_cast<std::size_t>(-1)) continue;
      auto before=primary.path_states[i]; if(!before.empty()) before.pop_back();
      auto after=primary.path_states[i+1]; if(!after.empty()) after.pop_back();
      result.primary.projected_model_moves.push_back({mv.activity,mv.transition,
        std::move(before), std::move(after)});
    }
    result.primary.moves = std::move(primary.moves);
    result.primary.complete_moves = result.primary.moves;
    result.primary.prefix_markings = std::move(primary.prefixes);
    for (std::size_t i = 1; i < goals.size(); ++i)
      result.alternatives.push_back(std::move(goals[i].moves));
    return result;
  }

 private:
  [[nodiscard]] bool enabled(const Marking& marking, std::size_t t) const {
    return petri_transition_enabled(net_.transitions()[t], marking);
  }

  [[nodiscard]] Marking fire(const Marking& marking, std::size_t t) const {
    Marking next = marking;
    fire_petri_transition(net_.transitions()[t], next);
    return next;
  }

  // Cheapest count of visible transitions on any run from the initial to the
  // final marking, found by the same uniform-cost search with no trace, or
  // std::nullopt when the final marking is unreachable within the search bound.
  [[nodiscard]] std::optional<std::uint64_t> shortest_model_run(
      const AlignmentOptions& options,
      std::chrono::steady_clock::time_point deadline,
      bool& exhausted) const {
    Marking start = initial_;
    std::priority_queue<std::pair<std::uint64_t, Marking>,
                        std::vector<std::pair<std::uint64_t, Marking>>,
                        std::greater<>>
        frontier;
    std::unordered_map<Marking, std::uint64_t, MarkingHash> best{{start, 0}};
    frontier.push({0, start});
    const std::size_t budget = options.max_states == 0 ? kAlignSearchCap
                                                        : options.max_states;
    std::size_t expanded = 0;
    while (!frontier.empty() && expanded < budget) {
      if (std::chrono::steady_clock::now() >= deadline) {
        exhausted = true;
        return std::nullopt;
      }
      ++expanded;
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
    exhausted = !frontier.empty();
    return std::nullopt;  // final marking unreachable: the net is not a sound WF-net
  }

  const PetriNet& net_;
  std::unordered_map<std::string, std::size_t> name_to_activity_;
  Marking initial_;
  Marking final_;
  bool reachable_ = false;
  bool reachability_exhausted_ = false;
  std::uint64_t empty_run_cost_ = 0;
  // Per-transition sparse LP column for the marking-equation heuristic:
  // produced minus consumed tokens per place.
  std::vector<std::vector<std::pair<std::size_t, double>>> incidence_;
  // Exact heuristic bounds keyed by marking + remaining activity counts —
  // the aggregated LP depends on nothing else, so a bound computed for one
  // search state serves every state (of any trace variant) that shares them.
  // nullopt marks a marking-equation-infeasible key: a provable dead end.
  // Bounded so a pathological log cannot grow it without limit; past the cap
  // the aligner just solves without remembering.
  mutable std::unordered_map<std::string, std::optional<std::uint64_t>>
      bound_cache_;
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
Relation model_footprint(const PetriNet& net, const Marking& initial,
                         bool& exhausted) {
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
    return petri_transition_enabled(t, m);
  };
  const auto fire = [&](const Marking& m, const PetriTransition& t) {
    Marking next = m;
    fire_petri_transition(t, next);
    return next;
  };

  // Bounded breadth-first exploration. Only the silent edges are stored (for
  // the label propagation); visible successors are re-fired in the final pass
  // so the usually far larger visible edge set never has to be kept.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> silent_edges;
  std::vector<char> was_expanded;
  std::queue<std::uint32_t> frontier;
  frontier.push(intern(initial).first);
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
  return check_footprints(event_log, net, {});
}

FootprintResult check_footprints(const EventLog& event_log, const PetriNet& net,
                                 const std::vector<int>& initial_marking) {
  const auto initial = resolve_workflow_marking(net, initial_marking, true);
  bool exhausted = false;
  const auto model = model_footprint(net, initial, exhausted);
  return compare_footprints(log_footprint(event_log), model, exhausted);
}

FootprintResult check_footprints(const EventLog& event_log,
                                 const ProcessTree& tree) {
  return check_footprints(event_log, to_petri_net(tree));
}

ConformanceResult check_conformance(const EventLog& event_log,
                                    const PetriNet& net,
                                    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant)) {
    throw std::invalid_argument("unsupported token replay variant");
  }
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options);
  const std::string activity_attribute =
      options.activity_attribute.empty() ? "concept:name"
                                         : options.activity_attribute;
  const auto variants = collect_replay_variants(event_log, replayer,
                                          activity_attribute);

  ConformanceResult result;
  result.trace_count = event_log.trace_count();
  result.fitness = 1.0;
  result.precision = 1.0;
  result.activity_attribute = activity_attribute;

  std::vector<std::uint64_t> executions(net.transitions().size(), 0);
  std::vector<std::uint64_t> variant_executions;
  for (const auto& variant : variants) {
    variant_executions.assign(net.transitions().size(), 0);
    const bool backwards = options.variant == TokenReplayVariant::kBackwards;
    const auto backwards_outcome =
        backwards ? std::optional<Replayer::BackwardsOutcome>(
                        replayer.replay_backwards(variant.ids, &variant_executions,
                                                  options.stop_immediately_unfit,
                                                  options.walk_through_hidden_transitions))
                  : std::nullopt;
    const auto counts = backwards
                            ? backwards_outcome->counts
                            : replayer.replay(variant.ids, &variant_executions,
                                              options);
    for (std::size_t t = 0; t < executions.size(); ++t) {
      executions[t] += variant_executions[t] * variant.count;
    }
    result.produced += counts.produced * variant.count;
    result.consumed += counts.consumed * variant.count;
    result.missing += counts.missing * variant.count;
    result.remaining += counts.remaining * variant.count;
    const bool fitting = backwards
                             ? backwards_outcome->fitting
                             : counts.forward_fitting(options);
    if (fitting) {
      result.fitting_traces += variant.count;
    }
    result.variants.push_back(
        VariantConformance{variant.names, variant.count, counts.fitness(),
                           variant.null_activity_indices});
  }

  if (result.consumed > 0 || result.produced > 0) {
    const double consume_term =
        result.consumed == 0 ? 0.0 : double(result.missing) / double(result.consumed);
    const double produce_term =
        result.produced == 0 ? 0.0 : double(result.remaining) / double(result.produced);
    result.fitness = 0.5 * (1.0 - consume_term) + 0.5 * (1.0 - produce_term);
  }

  result.precision = compute_precision(
      variants, replayer, options.variant, options.stop_immediately_unfit,
      options.walk_through_hidden_transitions);
  result.generalization = compute_generalization(executions);
  result.transition_frequencies = executions;
  result.simplicity = compute_simplicity(net);
  if (options.include_trace_diagnostics || options.include_duration_diagnostics ||
      options.include_root_cause_diagnostics) {
    auto traces = replay_token_traces(event_log, net, options);
    if (options.include_duration_diagnostics)
      result.duration_diagnostics = compute_token_replay_duration_diagnostics(
          event_log, net, traces, options);
    if (options.include_root_cause_diagnostics)
      result.root_cause_diagnostics = compute_token_replay_root_cause_diagnostics(
          event_log, net, traces, options);
    if (options.include_trace_diagnostics)
      result.trace_diagnostics = std::move(traces);
  }

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
                                    const ProcessTree& tree,
                                    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  return check_conformance(event_log, to_petri_net(tree), options);
}

TokenReplayDiagnostics replay_token_diagnostics(
    const EventLog& event_log, const PetriNet& net,
    TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant)) {
    throw std::invalid_argument("unsupported token replay variant");
  }
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options);
  const std::string activity_attribute =
      options.activity_attribute.empty() ? "concept:name"
                                         : options.activity_attribute;
  TokenReplayDiagnostics result;
  result.trace_count = event_log.trace_count();
  result.places.resize(net.place_count());
  result.transition_activations.resize(net.transitions().size());
  for (const auto& trace : event_log.traces) {
    std::vector<std::optional<std::size_t>> ids;
    ids.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto activity = selected_replay_activity(event, activity_attribute);
      ids.push_back(activity ? replayer.activity_id(*activity) : std::nullopt);
    }

    std::vector<std::uint64_t> executions(net.transitions().size(), 0);
    const bool backwards = options.variant == TokenReplayVariant::kBackwards;
    const auto backwards_outcome =
        backwards
            ? std::optional<Replayer::BackwardsOutcome>(replayer.replay_backwards(
                  ids, &executions, options.stop_immediately_unfit,
                  options.walk_through_hidden_transitions))
            : std::nullopt;
    const auto counts = backwards
                            ? backwards_outcome->counts
                            : replayer.replay(ids, &executions, options);
    for (std::size_t place = 0; place < result.places.size(); ++place) {
      result.places[place].missing += counts.places[place].missing;
      result.places[place].remaining += counts.places[place].remaining;
      result.places[place].consumed += counts.places[place].consumed;
      result.places[place].produced += counts.places[place].produced;
    }
    for (std::size_t transition = 0;
         transition < result.transition_activations.size(); ++transition) {
      result.transition_activations[transition] += executions[transition];
    }
    const bool fitting = backwards
                             ? backwards_outcome->fitting
                             : counts.forward_fitting(options);
    if (fitting) ++result.fitting_traces;
  }
  return result;
}

std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const EventLog& event_log, const PetriNet& net, TokenReplayOptions options) {
  validate_token_replay_event_limit(event_log, options);
  if (!valid_token_replay_variant(options.variant))
    throw std::invalid_argument("unsupported token replay variant");
  const Replayer replayer(net, options.initial_marking, options.final_marking, &options);
  const auto activity_attribute = options.activity_attribute.empty()
                                      ? std::string("concept:name")
                                      : options.activity_attribute;
  std::vector<TokenReplayTraceDiagnostics> result;
  result.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    TokenReplayTraceDiagnostics row;
    row.trace_index = result.size();
    row.case_id = trace.case_id;
    std::vector<std::optional<std::size_t>> ids;
    std::vector<std::optional<std::string>> names;
    for (const auto& event : trace.events) {
      names.push_back(selected_replay_activity(event, activity_attribute));
      ids.push_back(names.back() ? replayer.activity_id(*names.back()) : std::nullopt);
    }
    Replayer::Counts counts;
    if (options.variant == TokenReplayVariant::kBackwards) {
      auto outcome = replayer.replay_backwards(
          ids, nullptr, options.stop_immediately_unfit,
          options.walk_through_hidden_transitions, &row);
      row.trace_is_fit = outcome.fitting;
      counts = std::move(outcome.counts);
    } else {
      counts = replayer.replay(ids, nullptr, options, &row);
      row.trace_is_fit = counts.forward_fitting(options);
    }
    row.trace_fitness = counts.fitness();
    row.produced = counts.produced;
    row.consumed = counts.consumed;
    row.missing = counts.missing;
    row.remaining = counts.remaining;
    row.places = std::move(counts.places);
    row.enabled_transitions = replayer.enabled_transition_ids(
        row.reached_marking, options.transition_names);
    for (const auto index : row.unknown_event_indices) {
      row.unknown_activities.push_back(names[index].value_or(""));
      row.unknown_activity_is_null.push_back(!names[index]);
    }
    result.push_back(std::move(row));
  }
  return result;
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net) {
  return check_alignment(event_log, net, AlignmentOptions{});
}

AlignmentResult check_alignment(const EventLog& event_log,
                                const PetriNet& net,
                                const AlignmentOptions& input_options) {
  AlignmentResult result;
  AlignmentOptions options = input_options;
  result.variant = options.variant;
  const std::string activity_attribute =
      options.activity_attribute.empty() ? "concept:name"
                                         : options.activity_attribute;
  result.activity_attribute = activity_attribute;
  result.trace_count = event_log.trace_count();
  if (!valid_alignment_variant(options.variant) ||
      !valid_alignment_cost_profile(options.cost_profile) ||
      !valid_explicit_marking(net, options.initial_marking) ||
      !valid_explicit_marking(net, options.final_marking)) {
    result.computed = false;
    return result;
  }
  if (result.trace_count == 0) return result;
  const auto total_start = std::chrono::steady_clock::now();
  const auto valid_table = [](const std::unordered_map<std::string, double>& table) {
    for (const auto& [_, value] : table)
      if (!std::isfinite(value) || value < 0.0) return false;
    return true;
  };
  const auto valid_vector = [](const std::vector<double>& values) {
    for (const double value : values)
      if (!std::isfinite(value) || value < 0.0) return false;
    return true;
  };
  const auto valid_optional = [](const std::optional<double>& value) {
    return !value || (std::isfinite(*value) && *value >= 0.0);
  };
  if (!valid_optional(options.default_trace_cost) ||
      !valid_optional(options.default_model_cost) ||
      !valid_optional(options.default_sync_cost) ||
      !valid_optional(options.default_silent_cost) ||
      !valid_table(options.trace_costs) || !valid_table(options.model_costs) ||
      !valid_table(options.sync_costs) ||
      !valid_vector(options.trace_costs_by_position) ||
      !valid_vector(options.model_costs_by_transition) ||
      !valid_vector(options.sync_costs_by_transition) ||
      (options.variant == AlignmentVariant::kDiscountedAStar &&
       (!std::isfinite(options.exponent) || options.exponent <= 0.0))) {
    result.computed = false;
    return result;
  }
  bool callback_invalid = false;
  if (options.model_cost_function || options.sync_cost_function) {
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& tr = net.transitions()[t];
      const std::string name = tr.silent() ? std::string{} : net.activities()[*tr.activity];
      if (options.model_cost_function) {
        const double value = options.model_cost_function(name, t);
        if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
        options.model_costs_by_transition.resize(net.transitions().size());
        options.model_costs_by_transition[t] = value;
      }
      if (options.sync_cost_function && !tr.silent()) {
        const double value = options.sync_cost_function(name, t);
        if (!std::isfinite(value) || value < 0.0) callback_invalid = true;
        options.sync_costs_by_transition.resize(net.transitions().size());
        options.sync_costs_by_transition[t] = value;
      }
    }
    options.model_cost_function = {};
    options.sync_cost_function = {};
  }
  if (callback_invalid) {
    result.computed = false;
    return result;
  }
  const double preflight_limit =
      options.max_total_time_seconds > 0.0 && options.max_time_seconds > 0.0
          ? std::min(options.max_total_time_seconds, options.max_time_seconds)
          : (options.max_total_time_seconds > 0.0 ? options.max_total_time_seconds
                                                   : options.max_time_seconds);
  const auto preflight_deadline =
      preflight_limit > 0.0
          ? total_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<double>(preflight_limit))
          : std::chrono::steady_clock::time_point::max();
  const Aligner aligner(net, options, preflight_deadline);
  if (!aligner.reachable()) {
    result.computed = false;
    result.exhausted = aligner.reachability_exhausted();
    return result;
  }
  const auto variants = collect_variants(event_log, aligner,
                                          activity_attribute);
  std::vector<std::vector<Marking>> variant_markings;
  variant_markings.reserve(variants.size());
  double total_worst = 0.0;
  double weighted_empty = 0.0;
  AlignmentOptions baseline_options = options;
  if (options.max_total_time_seconds > 0.0) {
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - total_start).count();
    const double remaining = options.max_total_time_seconds - elapsed;
    if (remaining <= 0.0) {
      result.computed = false;
      result.exhausted = true;
      return result;
    }
    if (baseline_options.max_time_seconds <= 0.0 ||
        remaining < baseline_options.max_time_seconds)
      baseline_options.max_time_seconds = remaining;
  }
  if (options.variant == AlignmentVariant::kDiscountedAStar ||
      options.model_cost_function || options.sync_cost_function || options.trace_cost_function) {
    const auto empty = aligner.align_configured({}, {}, baseline_options);
    if (empty.primary.capped) {
      result.computed = false;
      result.exhausted = true;
      return result;
    }
    weighted_empty = empty.primary.cost;
  } else {
    weighted_empty = aligner.weighted_empty_run_cost(baseline_options);
  }
  if (!std::isfinite(weighted_empty)) {
    result.computed = false;
    result.exhausted = true;
    return result;
  }
  const double baseline_model_cost = weighted_empty;
  for (const auto& variant : variants) {
    if (options.max_total_time_seconds > 0.0 &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - total_start)
                .count() >= options.max_total_time_seconds) {
      result.computed = false;
      result.exhausted = true;
      return result;
    }
    AlignmentOptions trace_options = options;
    if (options.max_total_time_seconds > 0.0) {
      const double elapsed = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - total_start).count();
      const double remaining = std::max(0.0, options.max_total_time_seconds - elapsed);
      if (trace_options.max_time_seconds <= 0.0 || remaining < trace_options.max_time_seconds)
        trace_options.max_time_seconds = remaining;
    }
    if (trace_options.trace_cost_function) {
      trace_options.trace_costs_by_position.resize(variant.names.size());
      for (std::size_t i = 0; i < variant.names.size(); ++i) {
        trace_options.trace_costs_by_position[i] =
            trace_options.trace_cost_function(variant.names[i], i);
        if (!std::isfinite(trace_options.trace_costs_by_position[i]) ||
            trace_options.trace_costs_by_position[i] < 0.0) {
          result.computed = false;
          result.exhausted = false;
          return result;
        }
      }
      trace_options.trace_cost_function = {};
    }
    auto configured = aligner.align_configured(variant.ids, variant.names, trace_options);
    result.diagnostics.visited_states += configured.diagnostics.visited_states;
    result.diagnostics.queued_states = std::max(
        result.diagnostics.queued_states, configured.diagnostics.queued_states);
    result.diagnostics.traversed_arcs += configured.diagnostics.traversed_arcs;
    result.diagnostics.lp_solved += configured.diagnostics.lp_solved;
    result.diagnostics.closed_set_length = std::max(
        result.diagnostics.closed_set_length, configured.diagnostics.closed_set_length);
    result.diagnostics.num_visited_markings = std::max(
        result.diagnostics.num_visited_markings,
        configured.diagnostics.num_visited_markings);
    if (configured.invalid_cost) {
      result.computed = false;
      result.exhausted = false;
      return result;
    }
    if (configured.primary.capped) {
      result.computed = false;
      result.exhausted = true;
      result.variants.clear();
      return result;
    }
    const double profile_trace = options.default_trace_cost.value_or(
        options.cost_profile == AlignmentCostProfile::kStandard ? 10000.0 : 1.0);
    double trace_worst = 0.0;
    for (std::size_t i = 0; i < variant.names.size(); ++i) {
      const auto trace_it = options.trace_costs.find(variant.names[i]);
      double value = trace_it == options.trace_costs.end() ? profile_trace : trace_it->second;
      if (i < trace_options.trace_costs_by_position.size())
        value = trace_options.trace_costs_by_position[i];
      trace_worst += value;
    }
    const double worst = trace_worst + baseline_model_cost;
    const double fitness = worst <= 1e-12
                               ? 1.0
                               : std::clamp(1.0 - configured.primary.cost / worst, 0.0, 1.0);
    result.total_cost += configured.primary.cost * variant.count;
    total_worst += worst * variant.count;
    if (configured.primary.cost <= 1e-9) result.fitting_traces += variant.count;
    VariantAlignment item{variant.names, variant.count,
                          std::move(configured.primary.moves),
                          configured.primary.cost, fitness};
    item.diagnostics = configured.diagnostics;
    item.worst_cost = worst;
    item.projected_model_moves = configured.primary.projected_model_moves;
    item.complete_moves = configured.primary.complete_moves.empty()
                              ? item.moves
                              : std::move(configured.primary.complete_moves);
    item.optimal_alignments.push_back(item.moves);
    for (auto& alt : configured.alternatives)
      item.optimal_alignments.push_back(std::move(alt));
    result.variants.push_back(std::move(item));
    variant_markings.push_back(std::move(configured.primary.prefix_markings));
  }
  result.fitness = total_worst <= 1e-12
                     ? 1.0
                     : std::clamp(1.0 - result.total_cost / total_worst, 0.0, 1.0);
  const Replayer replayer(net, options.initial_marking, options.final_marking);
  result.precision = compute_alignment_precision(variants, variant_markings, replayer);
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

AlignmentResult check_alignment(const EventLog& event_log,
                                const ProcessTree& tree,
                                const AlignmentOptions& options) {
  return check_alignment(event_log, to_petri_net(tree), options);
}

std::vector<ActivityDeviation> summarize_alignment_moves(
    const AlignmentResult& alignment) {
  std::unordered_map<std::string, ActivityDeviation> by_activity;
  for (const auto& variant : alignment.variants) {
    for (const auto& move : variant.moves) {
      if (move.activity.empty()) continue;  // silent tau routing is not an activity deviation
      auto& tally = by_activity[move.activity];
      if (tally.activity.empty()) tally.activity = move.activity;
      switch (move.type) {
        case AlignmentMoveType::kSynchronous:
          tally.synchronous += variant.count;
          break;
        case AlignmentMoveType::kLog:
          tally.log_moves += variant.count;
          break;
        case AlignmentMoveType::kModel:
          tally.model_moves += variant.count;
          break;
      }
    }
  }

  std::vector<ActivityDeviation> result;
  result.reserve(by_activity.size());
  for (auto& [name, tally] : by_activity) {
    result.push_back(std::move(tally));
  }
  std::sort(result.begin(), result.end(),
            [](const ActivityDeviation& a, const ActivityDeviation& b) {
              const auto deviations_a = a.log_moves + a.model_moves;
              const auto deviations_b = b.log_moves + b.model_moves;
              if (deviations_a != deviations_b) {
                return deviations_a > deviations_b;
              }
              if (a.synchronous != b.synchronous) {
                return a.synchronous > b.synchronous;
              }
              return a.activity < b.activity;
            });
  return result;
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
  return final_marking_reachable(net, {}, {});
}

bool final_marking_reachable(const PetriNet& net,
                             const std::vector<int>& initial_marking,
                             const std::vector<int>& final_marking) {
  // The Aligner computes final-marking reachability in its constructor as the
  // precondition for scoring, so reuse it rather than duplicate the search.
  AlignmentOptions options;
  options.initial_marking = initial_marking;
  options.final_marking = final_marking;
  return Aligner(net, options).reachable();
}

}  // namespace minera
