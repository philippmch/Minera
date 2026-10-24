// Black-box equivalence of every model-quality selector against the public
// 2.7.22 results captured by scripts/capture-evaluation-reference.py.  Each
// case imports the PNML written by the reference exporter, replays the same
// traces, and compares every captured number (or the rejection of the input).

#include "minera/evaluation.hpp"
#include "minera/pnml.hpp"
#include "../evaluation_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace minera;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

enum ExpectState { kValue, kError, kUnresolved };

struct Expect {
  ExpectState state;
  std::vector<double> values;
};

struct ReferenceCase {
  const char* name;
  const char* pnml;
  std::vector<std::vector<std::string>> traces;
  Expect fitness_token;           // percentage fit, average trace, log fitness
  Expect fitness_alignment;       // percentage fit, average trace, log fitness
  Expect fitness_auto;            // alignment selected, then the three above
  Expect precision_token;
  Expect precision_alignment;
  Expect precision_automaton;
  Expect precision_auto;
  Expect generalization_token;
  Expect simplicity_arc_degree;
  Expect simplicity_extended_cardoso;
  Expect simplicity_extended_cyclomatic;
  Expect composite;  // token fitness triple, precision, generalization,
                     // simplicity, weighted average, F-score
};

const std::vector<ReferenceCase>& reference_cases() {
  static const std::vector<ReferenceCase> cases = {
#include "evaluation_reference_cases.inc"
  };
  return cases;
}

EventLog make_log(const std::vector<std::vector<std::string>>& traces) {
  EventLog result;
  std::size_t order = 0;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    Trace trace;
    trace.case_id = std::to_string(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    for (const auto& activity : traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

int failures = 0;

// Inputs on which Minera knowingly diverges from the pinned reference. Each is
// owned by a dependency outside the evaluators and is listed so the gap stays
// visible: an entry that starts matching fails the run, so the list cannot go
// stale.
struct KnownDivergence {
  std::string_view case_prefix;
  std::string_view metric;
  std::string_view reason;
};
constexpr KnownDivergence kKnownDivergences[] = {
    // Forward token replay routes hidden transitions differently on this
    // heuristics net; token fitness, generalization and the composite inherit
    // it. Owned by conformance token replay (exact routing still pending).
    {"seed1_heuristics_noisy", "fitness TOKEN_BASED", "token-replay hidden routing"},
    {"seed1_heuristics_noisy", "generalization GENERALIZATION_TOKEN", "token-replay hidden routing"},
    {"seed1_heuristics_noisy", "composite default", "token-replay hidden routing"},
};

const KnownDivergence* known_divergence(const ReferenceCase& reference, std::string_view metric) {
  for (const auto& entry : kKnownDivergences) {
    if (entry.metric == metric && std::string_view(reference.name).starts_with(entry.case_prefix))
      return &entry;
  }
  return nullptr;
}

bool same(double actual, double expected) {
  if (std::isnan(expected)) return std::isnan(actual);
  if (std::isinf(expected)) return actual == expected;
  return std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}

// Empty when Minera agrees with the reference, otherwise a description.
std::string difference(const Expect& expected, bool computed, const std::string& error,
                       const std::vector<double>& actual) {
  if (expected.state == kError) {
    return computed ? "reference rejects the input but Minera computed it" : "";
  }
  if (!computed) return "Minera rejected the input: " + error;
  std::string detail;
  for (std::size_t index = 0; index < expected.values.size(); ++index) {
    if (!same(actual[index], expected.values[index])) {
      char buffer[160];
      std::snprintf(buffer, sizeof buffer, "value %zu is %.17g, reference %.17g; ", index,
                    actual[index], expected.values[index]);
      detail += buffer;
    }
  }
  return detail;
}

struct Tally {
  std::size_t compared = 0;
  std::size_t agreed = 0;
  std::size_t known = 0;
};

// Exact comparison: any disagreement fails unless it is a listed divergence,
// and a listed divergence that agrees fails too.
template <class Evaluate>
void compare(Tally& tally, const ReferenceCase& reference, const char* metric,
             const Expect& expected, Evaluate evaluate) {
  if (expected.state == kUnresolved) return;
  ++tally.compared;
  std::string detail;
  try {
    detail = evaluate();
  } catch (const std::exception& error) {
    // A native exception is the counterpart of a reference exception.
    if (expected.state == kValue) detail = std::string("threw ") + error.what();
  }
  const auto* known = known_divergence(reference, metric);
  if (detail.empty() && !known) {
    ++tally.agreed;
    return;
  }
  if (!detail.empty() && known) {
    ++tally.known;
    return;
  }
  ++failures;
  std::fprintf(stderr, "%s / %s: %s\n", reference.name, metric,
               known ? "listed divergence now agrees; remove it" : detail.c_str());
}

// Every asserted selector must be compared on most of the corpus, so a capture
// that lost its observations cannot pass vacuously.
void require_coverage(const char* metric, const Tally& tally) {
  std::printf("%-38s %3zu compared, %3zu agree, %zu listed divergences\n", metric,
              tally.compared, tally.agreed, tally.known);
  if (tally.compared < 100) {
    ++failures;
    std::fprintf(stderr, "%s compared only %zu cases\n", metric, tally.compared);
  }
}

std::vector<double> fitness_values(const ReplayFitnessResult& result) {
  return {result.percentage_fit_traces, result.average_trace_fitness, result.log_fitness};
}

struct LoadedCase {
  const ReferenceCase* reference;
  PnmlDocument document;
  EventLog log;
};

const std::vector<LoadedCase>& loaded_cases() {
  static const std::vector<LoadedCase> cases = [] {
    std::vector<LoadedCase> result;
    PnmlImportOptions import;
    import.public_marking_semantics = true;
    for (const auto& reference : reference_cases()) {
      result.push_back({&reference, parse_pnml_document(reference.pnml, import),
                        make_log(reference.traces)});
    }
    return result;
  }();
  return cases;
}

void replay_fitness_token_based_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness TOKEN_BASED", item.reference->fitness_token, [&] {
      const auto result =
          evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::TOKEN_BASED);
      return difference(item.reference->fitness_token, result.computed, result.error,
                        fitness_values(result));
    });
  }
  require_coverage("replay fitness TOKEN_BASED", tally);
}

void replay_fitness_alignment_based_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness ALIGNMENT_BASED", item.reference->fitness_alignment,
            [&] {
      const auto result =
          evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::ALIGNMENT_BASED);
      return difference(item.reference->fitness_alignment, result.computed, result.error,
                        fitness_values(result));
    });
  }
  require_coverage("replay fitness ALIGNMENT_BASED", tally);
}

void replay_fitness_default_dispatch_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "fitness default", item.reference->fitness_auto, [&] {
      const auto result = evaluate_replay_fitness(item.log, item.document, ReplayFitnessVariant::AUTO);
      auto values = fitness_values(result);
      values.insert(values.begin(),
                    result.variant == ReplayFitnessVariant::ALIGNMENT_BASED ? 1.0 : 0.0);
      return difference(item.reference->fitness_auto, result.computed, result.error, values);
    });
  }
  require_coverage("replay fitness default", tally);
}

Tally compare_precision(const char* metric, PrecisionVariant variant,
                        Expect ReferenceCase::*expected) {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, metric, item.reference->*expected, [&] {
      const auto result = evaluate_precision(item.log, item.document, variant);
      return difference(item.reference->*expected, result.computed, result.error,
                        {result.precision});
    });
  }
  return tally;
}

void precision_etconformance_token_equivalence() {
  require_coverage("precision ETCONFORMANCE_TOKEN",
                   compare_precision("precision ETCONFORMANCE_TOKEN",
                                     PrecisionVariant::ETCONFORMANCE_TOKEN,
                                     &ReferenceCase::precision_token));
}

void precision_align_etconformance_equivalence() {
  require_coverage("precision ALIGN_ETCONFORMANCE",
                   compare_precision("precision ALIGN_ETCONFORMANCE",
                                     PrecisionVariant::ALIGN_ETCONFORMANCE,
                                     &ReferenceCase::precision_alignment));
}

void precision_default_dispatch_equivalence() {
  require_coverage("precision default",
                   compare_precision("precision default", PrecisionVariant::AUTO,
                                     &ReferenceCase::precision_auto));
}

// Standard evaluator costs: synchronous 0, silent 1, log or visible model
// move 10000.
constexpr double kDeviationCost = 10000.0;

double standard_cost(const PetriNet& net, const std::vector<AlignmentMove>& moves) {
  double cost = 0.0;
  for (const auto& move : moves) {
    if (move.type == AlignmentMoveType::kLog) cost += kDeviationCost;
    if (move.type != AlignmentMoveType::kModel) continue;
    cost += net.transitions().at(move.transition).silent() ? 1.0 : kDeviationCost;
  }
  return cost;
}

// The automaton scores a projection only through its visible labels and the
// labels available after each of them, so projections that agree on both are
// interchangeable.
using ProjectionKey = std::vector<std::pair<std::string, std::set<std::string>>>;

struct AvailableLabels {
  const PetriNet& net;
  const std::vector<std::string>& names;
  std::map<std::vector<int>, std::set<std::string>> cache;
  const std::set<std::string>& operator()(const std::vector<int>& marking) {
    auto found = cache.find(marking);
    if (found == cache.end()) {
      std::set<std::string> labels;
      if (!detail::eventually_enabled_labels(net, marking, names, 100000, false, labels))
        labels = {"<silent closure exhausted>"};
      found = cache.emplace(marking, std::move(labels)).first;
    }
    return found->second;
  }
};

ProjectionKey projection_key(const std::vector<ProjectedAlignmentMove>& moves,
                             AvailableLabels& available) {
  ProjectionKey key;
  for (const auto& move : moves) key.emplace_back(move.activity, available(move.marking_after));
  return key;
}

struct OptimalProjections {
  bool complete = true;  // every projection was enumerated
  bool cost_known = false;
  double cost = 0.0;
  std::vector<std::vector<ProjectedAlignmentMove>> projections;
};

// Every distinct visible model projection of the optimal alignments of one
// trace, from a forward search of the synchronous product followed by a walk
// of its tight (cost-preserving) edges.
OptimalProjections optimal_projections(const PetriNet& net, const std::vector<int>& initial,
                                       const std::vector<int>& final_marking,
                                       const std::vector<std::string>& trace,
                                       AvailableLabels& available) {
  constexpr std::size_t kMaxStates = 200000;
  constexpr std::size_t kMaxProjections = 512;
  OptimalProjections result;
  struct State {
    std::vector<int> marking;
    std::size_t position;
    double distance;
  };
  struct Edge {
    std::size_t from;
    std::size_t to;
    std::size_t transition;  // npos for a log move
    double cost;
  };
  std::vector<State> states{{initial, 0, 0.0}};
  std::map<std::pair<std::vector<int>, std::size_t>, std::size_t> ids{{{initial, 0}, 0}};
  using Item = std::pair<double, std::size_t>;
  std::priority_queue<Item, std::vector<Item>, std::greater<>> queue;
  queue.push({0.0, 0});
  std::vector<bool> settled(1, false);
  double best = kInf;
  const auto successors = [&](std::size_t id, auto&& visit) {
    const auto marking = states[id].marking;
    const auto position = states[id].position;
    if (position < trace.size())
      visit(marking, position + 1, static_cast<std::size_t>(-1), kDeviationCost);
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      if (!petri_transition_enabled(transition, marking)) continue;
      auto next = marking;
      fire_petri_transition(transition, next);
      if (transition.silent()) {
        visit(next, position, t, 1.0);
        continue;
      }
      visit(next, position, t, kDeviationCost);
      if (position < trace.size() && net.activities()[*transition.activity] == trace[position])
        visit(next, position + 1, t, 0.0);
    }
  };
  while (!queue.empty()) {
    const auto [distance, id] = queue.top();
    queue.pop();
    if (settled[id] || distance > states[id].distance) continue;
    if (distance > best) break;
    settled[id] = true;
    if (states[id].position == trace.size() && states[id].marking == final_marking) {
      best = distance;
      continue;
    }
    successors(id, [&](const std::vector<int>& marking, std::size_t position, std::size_t,
                       double cost) {
      const auto key = std::make_pair(marking, position);
      auto found = ids.find(key);
      if (found == ids.end()) {
        if (states.size() >= kMaxStates) {
          result.complete = false;
          return;
        }
        found = ids.emplace(key, states.size()).first;
        states.push_back({marking, position, kInf});
        settled.push_back(false);
      }
      if (distance + cost < states[found->second].distance) {
        states[found->second].distance = distance + cost;
        queue.push({distance + cost, found->second});
      }
    });
    if (!result.complete) return result;
  }
  if (best == kInf) {
    result.complete = false;
    return result;
  }
  result.cost = best;
  result.cost_known = true;
  // Tight edges between settled states, then the states that reach the goal.
  std::vector<Edge> tight;
  std::vector<std::vector<std::size_t>> incoming(states.size());
  for (std::size_t id = 0; id < states.size(); ++id) {
    if (!settled[id] || states[id].distance > best) continue;
    if (states[id].position == trace.size() && states[id].marking == final_marking) continue;
    successors(id, [&](const std::vector<int>& marking, std::size_t position,
                       std::size_t transition, double cost) {
      const auto found = ids.find({marking, position});
      if (found == ids.end() || !settled[found->second]) return;
      if (states[id].distance + cost != states[found->second].distance) return;
      incoming[found->second].push_back(tight.size());
      tight.push_back({id, found->second, transition, cost});
    });
  }
  std::vector<bool> useful(states.size(), false);
  std::vector<std::size_t> stack;
  for (std::size_t id = 0; id < states.size(); ++id) {
    if (settled[id] && states[id].distance == best && states[id].position == trace.size() &&
        states[id].marking == final_marking) {
      useful[id] = true;
      stack.push_back(id);
    }
  }
  const auto goals = stack;
  while (!stack.empty()) {
    const auto id = stack.back();
    stack.pop_back();
    for (const auto edge : incoming[id]) {
      if (useful[tight[edge].from]) continue;
      useful[tight[edge].from] = true;
      stack.push_back(tight[edge].from);
    }
  }
  // Tight edges raise the distance or, at equal distance, the trace position,
  // so this order visits every edge source before its target.
  std::vector<std::size_t> order;
  for (std::size_t id = 0; id < states.size(); ++id)
    if (useful[id]) order.push_back(id);
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return std::make_pair(states[a].distance, states[a].position) <
           std::make_pair(states[b].distance, states[b].position);
  });
  std::map<std::size_t, std::map<ProjectionKey, std::vector<ProjectedAlignmentMove>>> reached;
  reached[0][{}] = {};
  for (const auto id : order) {
    if (id == 0) continue;
    auto& here = reached[id];
    for (const auto edge_index : incoming[id]) {
      const auto& edge = tight[edge_index];
      if (!useful[edge.from]) continue;
      for (const auto& [key, moves] : reached[edge.from]) {
        auto extended = moves;
        if (edge.transition != static_cast<std::size_t>(-1) &&
            !net.transitions()[edge.transition].silent()) {
          extended.push_back({net.activities()[*net.transitions()[edge.transition].activity],
                              edge.transition, states[edge.from].marking, states[id].marking});
        }
        here.emplace(projection_key(extended, available), std::move(extended));
        if (here.size() > kMaxProjections) {
          result.complete = false;
          return result;
        }
      }
    }
  }
  std::map<ProjectionKey, std::vector<ProjectedAlignmentMove>> distinct;
  for (const auto goal : goals)
    for (const auto& [key, moves] : reached[goal]) distinct.emplace(key, moves);
  for (auto& [key, moves] : distinct) result.projections.push_back(std::move(moves));
  return result;
}

// Automaton-after-align scores one optimal alignment per trace variant, and
// the reference picks among equally cheap alignments by its search order,
// which is not a public contract. Each case therefore enumerates every optimal
// alignment of every variant and scores their combinations with Minera's
// automaton (all of them up to kMaxCombinations, otherwise a bounded search):
// the reference value and Minera's value must both be attainable. An exact
// match is counted separately.
void precision_automaton_after_align_tie_contract() {
  constexpr std::size_t kMaxCombinations = 4096;
  constexpr std::size_t kSearchSteps = 5000;
  Tally tally;
  std::size_t exact = 0;
  std::size_t optimal_only = 0;
  for (const auto& item : loaded_cases()) {
    const auto& expected = item.reference->precision_automaton;
    if (expected.state == kUnresolved) continue;
    ++tally.compared;
    const auto native =
        evaluate_precision(item.log, item.document, PrecisionVariant::AUTOMATON_AFTER_ALIGN);
    const auto fail = [&](const std::string& detail) {
      ++failures;
      std::fprintf(stderr, "%s / precision AUTOMATON_AFTER_ALIGN: %s\n", item.reference->name,
                   detail.c_str());
    };
    if (expected.state == kError || !native.computed) {
      const auto detail = difference(expected, native.computed, native.error, {native.precision});
      if (detail.empty()) ++tally.agreed, ++exact;
      else fail(detail);
      continue;
    }
    if (same(native.precision, expected.values[0])) ++exact;

    std::vector<int> initial(item.document.initial_marking.begin(),
                             item.document.initial_marking.end());
    std::vector<int> final_marking(item.document.final_marking.begin(),
                                   item.document.final_marking.end());
    const auto& net = item.document.net;
    AvailableLabels available_labels{net, item.document.transition_ids, {}};
    // Distinct projections per variant. Minera's own alignment must be
    // optimal and its projection one of them.
    std::vector<std::vector<std::vector<ProjectedAlignmentMove>>> choices;
    std::vector<std::uint64_t> counts;
    std::vector<std::size_t> native_pick;
    std::size_t combinations = 1;
    bool valid = true;
    bool complete = true;
    for (const auto& chosen : native.alignment_result.variants) {
      auto optimal =
          optimal_projections(net, initial, final_marking, chosen.activities, available_labels);
      if (!optimal.complete) {
        // Too many optimal alignments to enumerate: Minera's alignment must
        // still be optimal, and the reference choice stays unchecked.
        complete = false;
        if (!optimal.cost_known || standard_cost(net, chosen.moves) != optimal.cost) valid = false;
        continue;
      }
      const auto chosen_key = projection_key(chosen.projected_model_moves, available_labels);
      const auto own = std::find_if(optimal.projections.begin(), optimal.projections.end(),
                                    [&](const auto& other) {
                                      return projection_key(other, available_labels) == chosen_key;
                                    });
      if (standard_cost(net, chosen.moves) != optimal.cost || own == optimal.projections.end()) {
        valid = false;
        break;
      }
      native_pick.push_back(static_cast<std::size_t>(own - optimal.projections.begin()));
      combinations = std::min(combinations * optimal.projections.size(), kMaxCombinations + 1);
      choices.push_back(std::move(optimal.projections));
      counts.push_back(chosen.count);
    }
    if (!valid) {
      fail("Minera's alignment is not among the optimal alignments");
      continue;
    }
    if (!complete) {
      ++optimal_only;
      std::printf("  %s: too many optimal alignments to enumerate; Minera's is optimal\n",
                  item.reference->name);
      continue;
    }
    const auto score_of = [&](const std::vector<std::size_t>& pick) {
      std::vector<detail::AutomatonPrecisionPath> paths;
      for (std::size_t v = 0; v < choices.size(); ++v)
        paths.push_back({counts[v], choices[v][pick[v]]});
      const auto score = detail::automaton_after_align_precision(
          net, initial, paths, item.document.transition_ids, 100000, false,
          &available_labels.cache);
      return score.computed ? score.precision : kNaN;
    };
    const double target = expected.values[0];
    const bool native_attainable = same(score_of(native_pick), native.precision);
    bool reference_attainable = false;
    if (combinations <= kMaxCombinations) {
      std::vector<std::size_t> pick(choices.size(), 0);
      while (!reference_attainable) {
        reference_attainable = same(score_of(pick), target);
        std::size_t v = 0;
        while (v < pick.size() && ++pick[v] == choices[v].size()) pick[v++] = 0;
        if (v == pick.size()) break;
      }
    } else {
      // Too many combinations to enumerate: a seeded local search changes one
      // variant's alignment at a time, keeping changes that do not move the
      // score away from the reference value. It starts from Minera's choice
      // and from uniform choices (the same rank of every variant's
      // projections, in both directions).
      std::vector<std::vector<std::size_t>> starts{native_pick};
      for (std::size_t rank = 0; rank < 4; ++rank) {
        std::vector<std::size_t> low, high;
        for (const auto& options : choices) {
          low.push_back(std::min(rank, options.size() - 1));
          high.push_back(options.size() - 1 - std::min(rank, options.size() - 1));
        }
        starts.push_back(std::move(low));
        starts.push_back(std::move(high));
      }
      std::mt19937 random(7);
      for (auto pick : starts) {
        double score = score_of(pick);
        for (std::size_t step = 0; step < kSearchSteps && !same(score, target); ++step) {
          const auto v = random() % pick.size();
          const auto kept = pick[v];
          pick[v] = random() % choices[v].size();
          const double candidate = score_of(pick);
          if (std::abs(candidate - target) <= std::abs(score - target)) score = candidate;
          else pick[v] = kept;
        }
        reference_attainable = same(score, target);
        if (reference_attainable) break;
      }
    }
    if (reference_attainable && native_attainable) {
      ++tally.agreed;
    } else {
      char buffer[200];
      std::snprintf(buffer, sizeof buffer,
                    "reference %.17g %s, Minera %.17g %s over %zu%s optimal choices",
                    expected.values[0], reference_attainable ? "attainable" : "not attainable",
                    native.precision, native_attainable ? "attainable" : "not attainable",
                    combinations, combinations > kMaxCombinations ? "+ (searched)" : "");
      fail(buffer);
    }
  }
  std::printf("%-38s %3zu compared, %3zu attainable, %zu exact, %zu optimal only\n",
              "precision AUTOMATON_AFTER_ALIGN (ties)", tally.compared, tally.agreed, exact,
              optimal_only);
  if (tally.compared < 100) {
    ++failures;
    std::fprintf(stderr, "precision AUTOMATON_AFTER_ALIGN compared only %zu cases\n",
                 tally.compared);
  }
}

void generalization_token_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "generalization GENERALIZATION_TOKEN",
            item.reference->generalization_token, [&] {
      const auto result = evaluate_generalization(item.log, item.document,
                                                  GeneralizationVariant::GENERALIZATION_TOKEN);
      return difference(item.reference->generalization_token, result.computed, result.error,
                        {result.generalization});
    });
  }
  require_coverage("generalization GENERALIZATION_TOKEN", tally);
}

Tally compare_simplicity(const char* metric, SimplicityVariant variant,
                         Expect ReferenceCase::*expected) {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, metric, item.reference->*expected, [&] {
      const auto result = evaluate_simplicity(item.document, variant);
      return difference(item.reference->*expected, result.computed, result.error,
                        {result.simplicity});
    });
  }
  return tally;
}

void simplicity_arc_degree_equivalence() {
  require_coverage("simplicity SIMPLICITY_ARC_DEGREE",
                   compare_simplicity("simplicity SIMPLICITY_ARC_DEGREE",
                                      SimplicityVariant::ARC_DEGREE,
                                      &ReferenceCase::simplicity_arc_degree));
}

void simplicity_extended_cardoso_equivalence() {
  require_coverage("simplicity EXTENDED_CARDOSO",
                   compare_simplicity("simplicity EXTENDED_CARDOSO",
                                      SimplicityVariant::EXTENDED_CARDOSO,
                                      &ReferenceCase::simplicity_extended_cardoso));
}

void simplicity_extended_cyclomatic_equivalence() {
  require_coverage("simplicity EXTENDED_CYCLOMATIC",
                   compare_simplicity("simplicity EXTENDED_CYCLOMATIC",
                                      SimplicityVariant::EXTENDED_CYCLOMATIC,
                                      &ReferenceCase::simplicity_extended_cyclomatic));
}

void composite_default_equivalence() {
  Tally tally;
  for (const auto& item : loaded_cases()) {
    compare(tally, *item.reference, "composite default", item.reference->composite, [&] {
      const auto result = evaluate_composite(item.log, item.document);
      auto values = fitness_values(result.fitness_details);
      values.insert(values.end(), {result.precision, result.generalization, result.simplicity,
                                   result.metrics_average_weight, result.fscore});
      return difference(item.reference->composite, result.computed, result.error, values);
    });
  }
  require_coverage("composite evaluation default", tally);
}

}  // namespace

int main() {
  replay_fitness_token_based_equivalence();
  replay_fitness_alignment_based_equivalence();
  replay_fitness_default_dispatch_equivalence();
  precision_etconformance_token_equivalence();
  precision_align_etconformance_equivalence();
  precision_default_dispatch_equivalence();
  precision_automaton_after_align_tie_contract();
  generalization_token_equivalence();
  simplicity_arc_degree_equivalence();
  simplicity_extended_cardoso_equivalence();
  simplicity_extended_cyclomatic_equivalence();
  composite_default_equivalence();
  std::printf("%zu reference cases, %d failures\n", reference_cases().size(), failures);
  return failures == 0 ? 0 : 1;
}
