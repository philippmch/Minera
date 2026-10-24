// Black-box parity contracts for Petri-net, DFG and token-replay conformance
// against the reference banks docs/reference-2.7.22-conformance-parity-bank.json.gz
// (nets converted from random process trees) and
// docs/reference-2.7.22-conformance-parity-nets-bank.json.gz (nets discovered by
// alpha, heuristics and ILP miners, and nets with duplicate labels, with
// random cost functions).
//
// The bank merges captures from several processes (hash seeds) and PNML
// element orders. Where the reference produced one value everywhere, the
// native value must equal it; where it varied, the native value must be one
// of the observed values. Equally optimal Petri alignments are a reference
// tie choice, so alignments are checked for validity and cost instead. The
// reference DISCOUNTED_A_STAR search is not always optimal; where native
// differs from it, native must reach a cost no higher than any run.
// Decomposed alignments are bound by the best-worst cost of every row and a
// pinned cost and fitness tally (see the contract below).
#include "minera/conformance.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/petri_trace_alignment.hpp"
#include "minera/pnml.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace minera;

struct ParityMove {
  std::string log;
  std::optional<std::string> model;
  bool operator==(const ParityMove&) const = default;
};

// What every reference run returned for one call.
enum class Outcome { kAlignment, kNone, kError };

struct PetriExpectation {
  AlignmentVariant variant;
  bool costed;  // the case's cost functions were supplied
  std::size_t trace;
  Outcome outcome;
  std::vector<std::tuple<double, double, double>> admissible;  // cost, bwc, fitness
};

struct GeneratorExpectation {
  AlignmentVariant variant;
  bool costed;
  std::size_t trace;
  double cost;
  bool truncated;  // a capture run hit its enumeration limit
  std::vector<std::vector<ParityMove>> alignments;
};

struct DfgExpectation {
  std::size_t trace;
  bool computed;
  std::vector<ParityMove> alignment;
  double cost;
  std::size_t visited_states;
};

struct ReplayExpectation {
  bool trace_is_fit;
  double trace_fitness;
  std::vector<std::string> activated;
  std::vector<std::string> problems;
  std::vector<std::string> enabled;
  std::optional<std::vector<std::pair<std::string, long long>>> reached;
  long long missing, consumed, remaining, produced;
};

struct ReplayFamily {
  TokenReplayVariant variant;
  bool nonempty_traces_only;
  std::string option;  // one public parameter toggled, e.g. "stop_immediately_unfit=True"
  bool error;
  std::vector<std::vector<ReplayExpectation>> rows;  // admissible rows per trace
};

// Absent from the reference result (bwc and fitness of the empty trace).
constexpr double kAbsent = std::numeric_limits<double>::quiet_NaN();

struct DecomposedExpectation {
  std::size_t trace;
  std::vector<std::tuple<double, double, double>> admissible;  // cost, bwc, fitness
};

struct ConformanceParityCase {
  std::string name;
  std::string tree;
  std::vector<std::vector<std::string>> traces;
  std::vector<std::string> places;
  std::vector<std::pair<std::string, std::optional<std::string>>> transitions;
  std::vector<std::tuple<std::string, std::string, int>> arcs;  // source, target, weight
  std::vector<std::pair<std::string, int>> initial, final;
  std::vector<std::tuple<std::string, std::string, std::uint64_t>> dfg_edges;
  std::vector<std::pair<std::string, std::uint64_t>> dfg_starts, dfg_ends;
  // Optional reference cost functions: one cost per transition, and one per
  // event of each trace.
  std::vector<double> model_costs, sync_costs;
  std::vector<std::vector<double>> trace_costs;
  std::vector<PetriExpectation> petri;
  std::vector<GeneratorExpectation> generators;
  std::vector<DfgExpectation> dfg;
  std::vector<ReplayFamily> replay;
  std::vector<DecomposedExpectation> decomposed;
};

#include "conformance_parity_reference_cases.inc"

constexpr double kStandardMoveCost = 10000.0;

bool same(double left, double right) {
  return left == right ||
         std::fabs(left - right) <= 1e-12 * std::max({1.0, std::fabs(left), std::fabs(right)});
}

std::string escape(const std::string& text) {
  std::string out;
  for (const char c : text) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

// Rebuilds the reference PNML (places and transitions in canonical order, so
// native place and transition ids are the fixture's indices).
std::string pnml(const ConformanceParityCase& c) {
  std::map<std::string, int> initial(c.initial.begin(), c.initial.end());
  std::string x =
      "<?xml version=\"1.0\"?><pnml><net id=\"n\" "
      "type=\"http://www.pnml.org/version-2009/grammar/pnmlcoremodel\"><page id=\"p\">";
  for (const auto& place : c.places) {
    x += "<place id=\"" + escape(place) + "\"><name><text>" + escape(place) + "</text></name>";
    if (const auto it = initial.find(place); it != initial.end())
      x += "<initialMarking><text>" + std::to_string(it->second) + "</text></initialMarking>";
    x += "</place>";
  }
  for (const auto& [id, label] : c.transitions) {
    if (label) {
      x += "<transition id=\"" + escape(id) + "\"><name><text>" + escape(*label) +
           "</text></name></transition>";
    } else {
      x += "<transition id=\"" + escape(id) + "\"><name><text>" + escape(id) +
           "</text></name><toolspecific tool=\"ProM\" version=\"6.4\" "
           "activity=\"$invisible$\" localNodeID=\"x\"/></transition>";
    }
  }
  for (std::size_t i = 0; i < c.arcs.size(); ++i) {
    const auto& [source, target, weight] = c.arcs[i];
    x += "<arc id=\"a" + std::to_string(i) + "\" source=\"" + escape(source) + "\" target=\"" +
         escape(target) + "\">";
    if (weight != 1) x += "<inscription><text>" + std::to_string(weight) + "</text></inscription>";
    x += "</arc>";
  }
  x += "</page><finalmarkings><marking>";
  for (const auto& [place, tokens] : c.final)
    x += "<place idref=\"" + escape(place) + "\"><text>" + std::to_string(tokens) + "</text></place>";
  x += "</marking></finalmarkings></net></pnml>";
  return x;
}

EventLog event_log(const std::vector<std::vector<std::string>>& traces) {
  EventLog log;
  for (std::size_t i = 0; i < traces.size(); ++i) {
    Trace trace;
    trace.case_id = std::to_string(i);
    for (std::size_t k = 0; k < traces[i].size(); ++k)
      trace.events.push_back(Event{trace.case_id, traces[i][k], {}, {}, k});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::vector<ParityMove> parity_moves(const std::vector<PetriTraceAlignmentMove>& moves) {
  std::vector<ParityMove> out;
  for (const auto& move : moves) out.push_back({move.log, move.model});
  return out;
}

// Replays an alignment on the fixture net: every model move must fire an
// enabled transition with its label, the log projection must be the trace and
// the final marking must be reached exactly. Only the less-memory variants
// honor arc weights; the others read every arc as weight 1.
bool valid_alignment(const ConformanceParityCase& c, const std::vector<std::string>& trace,
                     const std::vector<PetriTraceAlignmentMove>& moves, AlignmentVariant variant) {
  const bool weighted = variant == AlignmentVariant::kDijkstraLessMemory ||
                        variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
  std::map<std::string, std::size_t> place_index;
  for (std::size_t i = 0; i < c.places.size(); ++i) place_index[c.places[i]] = i;
  std::map<std::string, std::size_t> transition_index;
  for (std::size_t i = 0; i < c.transitions.size(); ++i)
    transition_index[c.transitions[i].first] = i;
  std::vector<std::map<std::size_t, int>> pre(c.transitions.size()), post(c.transitions.size());
  for (const auto& [source, target, weight] : c.arcs) {
    if (const auto t = transition_index.find(source); t != transition_index.end())
      post[t->second][place_index.at(target)] = weighted ? weight : 1;
    else
      pre[transition_index.at(target)][place_index.at(source)] = weighted ? weight : 1;
  }
  std::vector<int> marking(c.places.size(), 0), goal(c.places.size(), 0);
  for (const auto& [place, tokens] : c.initial) marking[place_index.at(place)] = tokens;
  for (const auto& [place, tokens] : c.final) goal[place_index.at(place)] = tokens;
  std::vector<std::string> projection;
  for (const auto& move : moves) {
    if (move.log != ">>") projection.push_back(move.log);
    if (move.model == std::optional<std::string>(">>")) continue;
    if (!move.transition || *move.transition >= c.transitions.size()) return false;
    const auto t = *move.transition;
    if (c.transitions[t].second != move.model) return false;
    if (move.log != ">>" && std::optional<std::string>(move.log) != move.model) return false;
    for (const auto& [place, weight] : pre[t]) {
      if (marking[place] < weight) return false;
      marking[place] -= weight;
    }
    for (const auto& [place, weight] : post[t]) marking[place] += weight;
  }
  return projection == trace && marking == goal;
}

// The reference cost of an alignment under a variant's cost function.
double alignment_cost(const ConformanceParityCase& c, const std::vector<std::string>& trace,
                      std::size_t trace_index, AlignmentVariant variant, bool costed,
                      const std::vector<PetriTraceAlignmentMove>& moves) {
  const bool less_memory = variant == AlignmentVariant::kDijkstraLessMemory ||
                           variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
  double cost = 0.0;
  std::size_t event = 0;
  for (std::size_t k = 0; k < moves.size(); ++k) {
    const auto& move = moves[k];
    const bool log_move = move.model == std::optional<std::string>(">>");
    const bool sync = move.log != ">>" && !log_move;
    // The reference discounted search ignores supplied cost functions.
    if (variant == AlignmentVariant::kDiscountedAStar) {
      if (!sync) cost += std::pow(2.0, -static_cast<double>(k));
      if (move.log != ">>") ++event;
      continue;
    }
    if (costed) {
      if (log_move) cost += c.trace_costs[trace_index][event];
      else if (sync) cost += less_memory ? 0.0 : c.sync_costs[*move.transition];
      else cost += c.model_costs[*move.transition];
      if (move.log != ">>") ++event;
      continue;
    }
    if (sync) continue;
    if (move.log != ">>" || move.model) {
      cost += kStandardMoveCost;
      continue;
    }
    if (variant == AlignmentVariant::kGeneratorDijkstraLessMemory || !less_memory) {
      cost += 1.0;
      continue;
    }
    // DIJKSTRA_LESS_MEMORY: a silent move costs 1 only when some visible
    // transition whose label occurs in the trace has all of its input places
    // among the silent transition's input places.
    const auto& silent = c.transitions[*move.transition].first;
    std::set<std::string> inputs;
    for (const auto& [source, target, weight] : c.arcs)
      if (target == silent) inputs.insert(source);
    bool conflict = false;
    for (const auto& [id, label] : c.transitions) {
      if (!label || std::find(trace.begin(), trace.end(), *label) == trace.end()) continue;
      bool contained = true;
      for (const auto& [source, target, weight] : c.arcs)
        if (target == id && !inputs.contains(source)) contained = false;
      conflict = conflict || contained;
    }
    cost += conflict ? 1.0 : 0.0;
  }
  return cost;
}

PetriTraceAlignmentOptions alignment_options(const ConformanceParityCase& c, AlignmentVariant variant,
                                             bool costed, std::size_t trace) {
  PetriTraceAlignmentOptions options;
  options.variant = variant;
  if (costed) {
    options.model_costs = c.model_costs;
    options.sync_costs = c.sync_costs;
    options.trace_costs = c.trace_costs.at(trace);
  }
  return options;
}

void petri_alignment_contract(AlignmentVariant variant) {
  std::map<Outcome, std::size_t> rows;
  for (const auto& c : conformance_parity_cases()) {
    const auto net = parse_pnml(pnml(c));
    for (const auto& expected : c.petri) {
      if (expected.variant != variant) continue;
      const auto& trace = c.traces[expected.trace];
      const auto options = alignment_options(c, expected.variant, expected.costed, expected.trace);
      ++rows[expected.outcome];
      if (expected.outcome == Outcome::kError) {
        bool threw = false;
        try {
          (void)align_petri_trace(trace, net, options);
        } catch (const std::exception&) {
          threw = true;
        }
        assert(threw);
        continue;
      }
      const auto actual = align_petri_trace(trace, net, options);
      if (expected.outcome == Outcome::kNone) {
        assert(!actual.computed);
        continue;
      }
      assert(actual.computed);
      assert(actual.bwc && actual.fitness);
      bool admissible = std::any_of(
          expected.admissible.begin(), expected.admissible.end(), [&](const auto& tuple) {
            return same(std::get<0>(tuple), actual.cost) && same(std::get<1>(tuple), *actual.bwc) &&
                   same(std::get<2>(tuple), *actual.fitness);
          });
      // The reference discounted search is not always optimal (its result can
      // vary between runs, and a few stable results are beaten); native must
      // then do at least as well as every run.
      if (expected.variant == AlignmentVariant::kDiscountedAStar && !admissible)
        admissible = std::all_of(
            expected.admissible.begin(), expected.admissible.end(), [&](const auto& tuple) {
              return actual.cost <= std::get<0>(tuple) + 1e-12 &&
                     same(std::get<1>(tuple), *actual.bwc) && same(std::get<2>(tuple), *actual.fitness);
            });
      if (!admissible)
        std::cerr << c.name << " trace " << expected.trace << " variant "
                  << static_cast<int>(expected.variant) << (expected.costed ? " costed" : "")
                  << ": cost " << actual.cost << '\n';
      assert(admissible);
      assert(valid_alignment(c, trace, actual.alignment, expected.variant));
      assert(same(alignment_cost(c, trace, expected.trace, expected.variant, expected.costed,
                                 actual.alignment),
                  actual.cost));
    }
  }
  const auto& expected_rows = kPetriRows[static_cast<int>(variant)];
  assert(rows[Outcome::kAlignment] == expected_rows[0] && expected_rows[0] > 0);
  assert(rows[Outcome::kNone] == expected_rows[1] && rows[Outcome::kError] == expected_rows[2]);
}

void state_equation_a_star_reference_contract() {
  petri_alignment_contract(AlignmentVariant::kStateEquationAStar);
}

void dijkstra_no_heuristics_reference_contract() {
  petri_alignment_contract(AlignmentVariant::kDijkstraNoHeuristics);
}

void dijkstra_less_memory_reference_contract() {
  petri_alignment_contract(AlignmentVariant::kDijkstraLessMemory);
}

void discounted_a_star_reference_contract() {
  petri_alignment_contract(AlignmentVariant::kDiscountedAStar);
}

void generator_alignment_contract(AlignmentVariant variant) {
  std::size_t rows = 0, empty = 0;
  for (const auto& c : conformance_parity_cases()) {
    const auto net = parse_pnml(pnml(c));
    for (const auto& expected : c.generators) {
      if (expected.variant != variant) continue;
      const auto& trace = c.traces[expected.trace];
      const auto options = alignment_options(c, expected.variant, expected.costed, expected.trace);
      const auto actual = align_petri_trace_all(trace, net, options);
      ++rows;
      // No alignment reaches the final marking.
      if (expected.alignments.empty()) {
        assert(actual.empty());
        ++empty;
        continue;
      }
      assert(!actual.empty() && actual.front().computed);
      assert(!actual.front().bwc && !actual.front().fitness);
      std::vector<std::vector<ParityMove>> native;
      for (const auto& alignment : actual) {
        assert(same(alignment.cost, expected.cost));
        assert(valid_alignment(c, trace, alignment.alignment, expected.variant));
        assert(same(alignment_cost(c, trace, expected.trace, expected.variant, expected.costed,
                                   alignment.alignment),
                    expected.cost));
        native.push_back(parity_moves(alignment.alignment));
      }
      // Every equally optimal alignment the reference enumerated is found,
      // unless either side stopped at its enumeration limit.
      if (!expected.truncated && actual.size() < options.max_alignments) {
        for (const auto& reference : expected.alignments)
          assert(std::find(native.begin(), native.end(), reference) != native.end());
      }
    }
  }
  const auto& expected_rows = kGeneratorRows[static_cast<int>(variant) - 4];
  assert(rows == expected_rows[0] && empty == expected_rows[1] && rows > 0);
}

void generator_dijkstra_no_heuristics_reference_contract() {
  generator_alignment_contract(AlignmentVariant::kGeneratorDijkstraNoHeuristics);
}

void generator_dijkstra_less_memory_reference_contract() {
  generator_alignment_contract(AlignmentVariant::kGeneratorDijkstraLessMemory);
}

void dfg_classic_alignment_reference_contract() {
  std::size_t rows = 0;
  for (const auto& c : conformance_parity_cases()) {
    DfgFrequency dfg;
    for (const auto& [from, to, count] : c.dfg_edges) dfg[{from, to}] = count;
    const std::map<std::string, std::uint64_t> starts(c.dfg_starts.begin(), c.dfg_starts.end());
    const std::map<std::string, std::uint64_t> ends(c.dfg_ends.begin(), c.dfg_ends.end());
    for (const auto& expected : c.dfg) {
      const auto actual = align_trace_on_dfg(c.traces[expected.trace], dfg, starts, ends);
      assert(actual.computed == expected.computed);
      if (expected.computed) {
        std::vector<ParityMove> moves;
        for (const auto& move : actual.alignment) moves.push_back({move.log, move.model});
        assert(moves == expected.alignment);
        assert(same(actual.cost, expected.cost));
        assert(actual.visited_states == expected.visited_states);
      }
      ++rows;
    }
  }
  assert(rows == kDfgRows);
}

bool replay_row_matches(const ConformanceParityCase& c, const TokenReplayTraceDiagnostics& row,
                        const ReplayExpectation& expected) {
  const auto names = [&](const std::vector<std::size_t>& ids) {
    std::vector<std::string> out;
    for (const auto id : ids) out.push_back(c.transitions.at(id).first);
    return out;
  };
  auto enabled = names(row.enabled_transitions);
  std::sort(enabled.begin(), enabled.end());
  if (row.trace_is_fit != expected.trace_is_fit || !same(row.trace_fitness, expected.trace_fitness) ||
      names(row.activated_transitions) != expected.activated ||
      names(row.transitions_with_problems) != expected.problems || enabled != expected.enabled ||
      static_cast<long long>(row.missing) != expected.missing ||
      static_cast<long long>(row.consumed) != expected.consumed ||
      static_cast<long long>(row.remaining) != expected.remaining ||
      static_cast<long long>(row.produced) != expected.produced)
    return false;
  if (!expected.reached) return true;
  // The reference marking lists places holding tokens, plus explicit keys.
  std::vector<std::pair<std::string, long long>> reached;
  for (std::size_t p = 0; p < row.reached_marking.size(); ++p) {
    const auto tokens = static_cast<long long>(row.reached_marking[p]);
    const bool key = row.reached_marking_keys.empty() ? tokens != 0 : row.reached_marking_keys[p];
    if (key) reached.emplace_back(c.places[p], tokens);
  }
  std::sort(reached.begin(), reached.end());
  return reached == *expected.reached;
}

void apply_replay_option(TokenReplayOptions& options, const std::string& option) {
  if (option.empty()) return;
  if (option == "consider_remaining_in_fitness=False") options.consider_remaining_in_fitness = false;
  else if (option == "try_to_reach_final_marking_through_hidden=False")
    options.try_to_reach_final_marking_through_hidden = false;
  else if (option == "walk_through_hidden_trans=False") options.walk_through_hidden_transitions = false;
  else if (option == "stop_immediately_unfit=True") options.stop_immediately_unfit = true;
  else if (option == "cleaning_token_flood=True") options.cleaning_token_flood = true;
  else if (option == "consider_activities_not_in_model_in_fitness=True")
    options.consider_activities_not_in_model_in_fitness = true;
  else if (option == "exhaustive_invisible_exploration=True")
    options.exhaustive_invisible_exploration = true;
  else throw std::invalid_argument("unknown replay option " + option);
}

void replay_family_contract(TokenReplayVariant variant) {
  std::size_t rows = 0, errors = 0;
  for (const auto& c : conformance_parity_cases()) {
    const auto net = parse_pnml(pnml(c));
    for (const auto& family : c.replay) {
      if (family.variant != variant) continue;
      std::vector<std::vector<std::string>> traces;
      for (const auto& trace : c.traces)
        if (!family.nonempty_traces_only || !trace.empty()) traces.push_back(trace);
      TokenReplayOptions options;
      options.variant = variant;
      // The reference orders routing candidates by transition and place name.
      for (const auto& [id, label] : c.transitions) options.transition_names.push_back(id);
      options.place_names = c.places;
      apply_replay_option(options, family.option);
      if (family.error) {
        bool threw = false;
        try {
          (void)replay_token_traces(event_log(traces), net, options);
        } catch (const std::exception&) {
          threw = true;
        }
        assert(threw);
        ++errors;
        continue;
      }
      const auto actual = replay_token_traces(event_log(traces), net, options);
      assert(actual.size() == family.rows.size());
      for (std::size_t k = 0; k < actual.size(); ++k) {
        const auto& admissible = family.rows[k];
        assert(std::any_of(admissible.begin(), admissible.end(), [&](const auto& expected) {
          return replay_row_matches(c, actual[k], expected);
        }));
        ++rows;
      }
    }
  }
  // Every corpus log holds an empty trace, which the reference backwards
  // replay rejects; some logs fail it without the empty trace as well.
  if (variant == TokenReplayVariant::kForward)
    assert(rows == kForwardReplayRows && errors == kForwardReplayErrors);
  else
    assert(rows == kBackwardsReplayRows && errors == kBackwardsReplayErrors);
}

void token_replay_reference_contract() { replay_family_contract(TokenReplayVariant::kForward); }

void backwards_token_replay_reference_contract() {
  replay_family_contract(TokenReplayVariant::kBackwards);
}

// The reference recomposition is not reproducible from its outputs alone: it
// sometimes reports moves twice or drops them, and on a few traces its cost
// has no optimal explanation (on ->('h', X('f','c'), tau) the trace [f,h]
// costs one log move while [f,h,f] costs nothing). Native implements the
// documented rules (maximal decomposition, components that see no event are
// skipped, events of unknown activities are dropped, disagreeing components
// are merged) and must agree on the best-worst cost of every row; the cost
// and fitness agreement is pinned as a tally.
void decomposed_recompos_maximal_reference_contract() {
  std::size_t rows = 0, equal = 0, differ = 0;
  for (const auto& c : conformance_parity_cases()) {
    if (c.decomposed.empty()) continue;
    const auto net = parse_pnml(pnml(c));
    for (const auto& expected : c.decomposed) {
      // The reference aligns each trace in a log of its own.
      const auto actual = align_decomposed(event_log({c.traces[expected.trace]}), net);
      assert(actual.traces.size() == 1);
      const auto& row = actual.traces.front();
      const bool empty = c.traces[expected.trace].empty();
      assert(std::all_of(expected.admissible.begin(), expected.admissible.end(), [&](const auto& t) {
        return empty ? std::isnan(std::get<1>(t)) && std::isnan(std::get<2>(t))
                     : same(std::get<1>(t), row.best_worst_cost);
      }));
      // Fitness discounts silent moves: 1 - (cost - silent moves) / bwc.
      if (!empty) {
        double visible = row.cost;
        for (const auto& move : row.alignment)
          if (!(move.log && *move.log != ">>") && !(move.model && *move.model != ">>")) visible -= DecomposedAlignmentOptions{}.tau_model_cost;
        assert(same(row.fitness, 1.0 - visible / row.best_worst_cost));
      }
      const bool match = std::any_of(expected.admissible.begin(), expected.admissible.end(), [&](const auto& t) {
        return same(std::get<0>(t), row.cost) && (empty || same(std::get<2>(t), row.fitness));
      });
      ++(match ? equal : differ);
      ++rows;
    }
  }
  assert(rows == kDecomposedRows);
  std::cout << "decomposed RECOMPOS_MAXIMAL: " << equal << " equal, " << differ << " differ\n";
  assert(equal == 770 && differ == 39);
}

}  // namespace

int main() {
  state_equation_a_star_reference_contract();
  dijkstra_no_heuristics_reference_contract();
  dijkstra_less_memory_reference_contract();
  discounted_a_star_reference_contract();
  generator_dijkstra_no_heuristics_reference_contract();
  generator_dijkstra_less_memory_reference_contract();
  dfg_classic_alignment_reference_contract();
  token_replay_reference_contract();
  backwards_token_replay_reference_contract();
  decomposed_recompos_maximal_reference_contract();
  return 0;
}
