#include "minera/ilp_miner.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

struct NormalizedLog {
  std::vector<std::string> names;
  struct Trace {
    std::vector<std::size_t> sequence;
    std::size_t frequency = 1;
    // Number of cumulative sequence-encoding rows retained for this variant
    // after alpha filtering; a shorter prefix means the remaining suffix is
    // intentionally absent from region feasibility constraints.
    std::size_t retained_prefix = 0;
  };
  std::vector<Trace> traces;
  std::vector<std::pair<std::size_t, std::size_t>> causal_relation;
  bool causal_relation_supplied = false;
  std::set<std::pair<std::size_t, std::size_t>> sequence_edges;
};

using SequenceVector = std::vector<std::int64_t>;

using BitMask = std::vector<std::uint64_t>;

BitMask empty_mask(std::size_t activity_count) {
  return BitMask((activity_count + 63) / 64, 0);
}

void set_bit(BitMask& mask, std::size_t activity) {
  mask[activity / 64] |= std::uint64_t{1} << (activity % 64);
}

bool has_bit(const BitMask& mask, std::size_t activity) {
  return (mask[activity / 64] & (std::uint64_t{1} << (activity % 64))) != 0;
}

std::size_t bit_count(const BitMask& mask) {
  std::size_t count = 0;
  for (const auto word : mask) count += std::popcount(word);
  return count;
}

bool any_bit(const BitMask& mask) {
  return std::any_of(mask.begin(), mask.end(),
                     [](std::uint64_t word) { return word != 0; });
}

bool masks_equal(const BitMask& left, const BitMask& right) {
  return left == right;
}

struct Candidate {
  BitMask input;
  BitMask output;
};

struct Region {
  // One bit per activity.  A place may both consume and produce an activity
  // (a self-loop), which is deliberately distinct from a zero net change.
  BitMask input;
  BitMask output;
  std::size_t initial = 0;
  std::size_t final = 0;
  // Cumulative-row objective used to choose the best region for a pair.
  std::int64_t objective = 0;
};

struct RegionRow {
  // Coefficients for input (place -> transition) and output
  // (transition -> place) arcs in one cumulative sequence row.
  std::vector<std::int64_t> input;
  std::vector<std::int64_t> output;
  bool equality = false;
};

struct RegionProblem {
  std::size_t activity_count = 0;
  std::vector<RegionRow> rows;
  std::vector<std::int64_t> objective_input;
  std::vector<std::int64_t> objective_output;
  bool have_observed_prefix = false;
};

std::int64_t saturating_add(std::int64_t left, std::int64_t right) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right)
    return std::numeric_limits<std::int64_t>::max();
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)
    return std::numeric_limits<std::int64_t>::min();
  return static_cast<std::int64_t>(left + right);
}

NormalizedLog normalize(const EventLog& log, const ILPMinerOptions& options) {
  if (log.traces.empty() || log.event_count() == 0)
    throw std::invalid_argument("ILP miner requires a non-empty event log");

  if (!std::isfinite(options.alpha) || options.alpha < 0.0 || options.alpha > 1.0)
    throw std::invalid_argument("ILP miner alpha must be in [0,1]");
  if (!options.artificial_start_activity.empty() &&
      options.artificial_start_activity == options.artificial_end_activity)
    throw std::invalid_argument("ILP miner artificial boundary labels must differ");

  std::set<std::string> names;
  std::vector<std::vector<std::string>> raw;
  for (const auto& trace : log.traces) {
    std::vector<std::string> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      std::string label = event.activity;
      if (!options.activity_key.empty() && options.activity_key != "activity" &&
          options.activity_key != "concept:name") {
        if (options.activity_key == "resource" || options.activity_key == "org:resource") {
          label = event.resource;
        } else if (const auto* value = event.attribute(options.activity_key)) {
          label = *value;
        }
      }
      if (!label.empty()) {
        names.insert(label);
        sequence.push_back(std::move(label));
      }
    }
    if (!sequence.empty()) {
      if (!options.artificial_start_activity.empty())
        sequence.insert(sequence.begin(), options.artificial_start_activity);
      if (!options.artificial_end_activity.empty())
        sequence.push_back(options.artificial_end_activity);
      raw.push_back(std::move(sequence));
    }
  }
  if (raw.empty() || names.empty())
    throw std::invalid_argument("ILP miner found no activities");

  if ((!options.artificial_start_activity.empty() &&
       names.count(options.artificial_start_activity) != 0) ||
      (!options.artificial_end_activity.empty() &&
       names.count(options.artificial_end_activity) != 0)) {
    throw std::invalid_argument(
        "ILP miner artificial boundary label collides with an activity");
  }
  if (!options.artificial_start_activity.empty())
    names.insert(options.artificial_start_activity);
  if (!options.artificial_end_activity.empty())
    names.insert(options.artificial_end_activity);

  NormalizedLog out;
  // Keep one representative per variant in first-seen order while retaining
  // its multiplicity for support-based filtering.  This bounds the region
  // search without discarding frequency information.
  std::map<std::vector<std::string>, std::size_t> variant_ids;
  std::vector<std::size_t> frequencies;
  std::vector<std::vector<std::string>> variants;
  for (auto& sequence : raw) {
    const auto [it, inserted] = variant_ids.emplace(sequence, variants.size());
    if (inserted) {
      frequencies.push_back(1);
      variants.push_back(std::move(sequence));
    } else {
      ++frequencies[it->second];
    }
  }
  std::size_t max_frequency = 0;
  for (const auto frequency : frequencies) max_frequency = std::max(max_frequency, frequency);
  std::vector<bool> retained(variants.size(), true);
  if (options.noise_threshold > 0.0) {
    for (std::size_t i = 0; i < frequencies.size(); ++i)
      retained[i] = static_cast<double>(frequencies[i]) + 1e-12 >=
                    options.noise_threshold * static_cast<double>(max_frequency);
    if (std::none_of(retained.begin(), retained.end(), [](bool value) { return value; }))
      retained[static_cast<std::size_t>(std::distance(
          frequencies.begin(), std::max_element(frequencies.begin(), frequencies.end())))] = true;
  }
  std::set<std::string> retained_names;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    if (!retained[i]) continue;
    retained_names.insert(variants[i].begin(), variants[i].end());
  }
  out.names.assign(retained_names.begin(), retained_names.end());
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < out.names.size(); ++i)
    ids.emplace(out.names[i], i);
  out.traces.reserve(variants.size());
  for (std::size_t i = 0; i < variants.size(); ++i) {
    if (!retained[i]) continue;
    std::vector<std::size_t> encoded;
    encoded.reserve(variants[i].size());
    for (const auto& value : variants[i]) encoded.push_back(ids.at(value));
    const auto encoded_size = encoded.size();
    out.traces.push_back({std::move(encoded), frequencies[i], encoded_size});
  }
  if (out.traces.empty()) throw std::invalid_argument("ILP miner found no retained variants");

  // Build the cumulative occurrence-vector sequence encoding.  Each event is
  // represented by the complete prefix count vector, while the predecessor
  // vector is negated to distinguish the source prefix from the current
  // prefix.  The alpha threshold is applied to continuations sharing the
  // same predecessor encoding, and a rejected continuation cuts the rest of
  // that variant just as the bounded sequence-encoding search does.
  struct EncodingStep {
    SequenceVector previous;
    SequenceVector current;
  };
  std::map<SequenceVector, std::map<SequenceVector, std::uint64_t>> support;
  std::vector<std::vector<EncodingStep>> encoding_steps;
  encoding_steps.reserve(out.traces.size());
  for (const auto& trace : out.traces) {
    SequenceVector cumulative(out.names.size(), 0);
    std::vector<EncodingStep> steps;
    steps.reserve(trace.sequence.size());
    for (std::size_t i = 0; i < trace.sequence.size(); ++i) {
      SequenceVector previous = cumulative;
      if (i != 0) {
        for (auto& value : previous) value = -value;
      }
      ++cumulative[trace.sequence[i]];
      SequenceVector current = cumulative;
      support[previous][current] += static_cast<std::uint64_t>(trace.frequency);
      steps.push_back({std::move(previous), std::move(current)});
    }
    encoding_steps.push_back(std::move(steps));
  }
  for (std::size_t trace_index = 0; trace_index < out.traces.size(); ++trace_index) {
    auto& trace = out.traces[trace_index];
    const auto& steps = encoding_steps[trace_index];
    trace.retained_prefix = 0;
    for (std::size_t i = 0; i < steps.size(); ++i) {
      const auto support_it = support.find(steps[i].previous);
      if (support_it == support.end()) break;
      std::uint64_t strongest = 0;
      for (const auto& [_, count] : support_it->second)
        strongest = std::max(strongest, count);
      const auto continuation_it = support_it->second.find(steps[i].current);
      if (continuation_it == support_it->second.end()) break;
      const auto count = continuation_it->second;
      const bool keep_step = options.alpha >= 1.0 ||
                             static_cast<long double>(count) >=
                                 (1.0L - static_cast<long double>(options.alpha)) *
                                     static_cast<long double>(strongest);
      if (!keep_step) break;
      trace.retained_prefix = i + 1;
      if (i > 0)
        out.sequence_edges.emplace(trace.sequence[i - 1], trace.sequence[i]);
    }
  }

  out.causal_relation_supplied = !options.causal_relation.empty();
  if (options.causal_relation.empty()) {
    // The default causal relation is the one-way portion of the normalized
    // directly-follows graph.  Reverse edges are excluded so loops and
    // concurrency do not turn every observed transition into a causal pair.
    std::map<std::pair<std::size_t, std::size_t>, std::uint64_t> dfg;
    for (const auto& trace : out.traces) {
      for (std::size_t i = 1; i < trace.sequence.size(); ++i)
        dfg[{trace.sequence[i - 1], trace.sequence[i]}] +=
            static_cast<std::uint64_t>(trace.frequency);
    }
    for (const auto& [edge, count] : dfg) {
      if (count == 0 || dfg.contains({edge.second, edge.first})) continue;
      out.causal_relation.push_back(edge);
    }
  } else {
    for (const auto& [from, to] : options.causal_relation) {
      const auto from_it = ids.find(from);
      const auto to_it = ids.find(to);
      if (from_it == ids.end() || to_it == ids.end())
        throw std::invalid_argument("ILP miner causal relation names an unknown activity");
      out.causal_relation.emplace_back(from_it->second, to_it->second);
    }
  }
  return out;
}

RegionProblem build_region_problem(
    const std::vector<NormalizedLog::Trace>& traces, std::size_t activity_count) {
  RegionProblem problem;
  problem.activity_count = activity_count;
  problem.objective_input.assign(activity_count, 0);
  problem.objective_output.assign(activity_count, 0);
  for (const auto& trace_entry : traces) {
    const auto& trace = trace_entry.sequence;
    const auto retained_prefix =
        std::min(trace_entry.retained_prefix, trace.size());
    std::vector<std::int64_t> cumulative(activity_count, 0);
    for (std::size_t i = 0; i < retained_prefix; ++i) {
      std::vector<std::int64_t> previous = cumulative;
      if (i != 0) {
        for (auto& value : previous) value = -value;
      }
      if (cumulative[trace[i]] < std::numeric_limits<std::int64_t>::max())
        ++cumulative[trace[i]];

      RegionRow row;
      row.input = cumulative;
      row.output = std::move(previous);
      row.equality = retained_prefix == trace.size() && i + 1 == retained_prefix;
      problem.rows.push_back(row);
      problem.have_observed_prefix = true;
      for (std::size_t activity = 0; activity < activity_count; ++activity) {
        problem.objective_output[activity] = saturating_add(
            problem.objective_output[activity], cumulative[activity]);
        problem.objective_input[activity] = saturating_add(
            problem.objective_input[activity], -cumulative[activity]);
      }
    }
  }
  return problem;
}

// Check one candidate signature against the cumulative sequence rows.  The
// binary variables describe transition-to-place (output) and
// place-to-transition (input) arcs.  The deviation variable is fixed to zero,
// so each retained row enforces pre-fire enablement and each complete trace's
// last row is an equality.
bool solve_region(const BitMask& input, const BitMask& output,
                  const RegionProblem& problem, Region& out) {
  if (!any_bit(input) && !any_bit(output)) return false;
  std::int64_t objective = 0;
  for (const auto& row : problem.rows) {
    std::int64_t lhs = 0;
    for (std::size_t activity = 0; activity < problem.activity_count;
         ++activity) {
      if (has_bit(output, activity))
        lhs = saturating_add(lhs, row.output[activity]);
      if (has_bit(input, activity))
        lhs = saturating_add(lhs, row.input[activity]);
    }
    if (row.equality ? lhs != 0 : lhs > 0) return false;
  }
  if (!problem.have_observed_prefix) return false;
  for (std::size_t activity = 0; activity < problem.activity_count;
       ++activity) {
    if (has_bit(output, activity))
      objective = saturating_add(objective, problem.objective_output[activity]);
    if (has_bit(input, activity))
      objective = saturating_add(objective, problem.objective_input[activity]);
  }
  out = Region{input, output, 0, 0, objective};
  return true;
}

bool region_less(const Region& a, const Region& b) {
  if (a.objective != b.objective) return a.objective < b.objective;
  const auto arcs = [](const Region& r) {
    return bit_count(r.input) + bit_count(r.output);
  };
  if (arcs(a) != arcs(b)) return arcs(a) < arcs(b);
  if (a.initial != b.initial) return a.initial < b.initial;
  if (a.final != b.final) return a.final < b.final;
  if (a.input != b.input) return a.input < b.input;
  return a.output < b.output;
}

std::vector<Region> discover_regions(
    const std::vector<NormalizedLog::Trace>& traces, std::size_t n,
    const std::vector<std::pair<std::size_t, std::size_t>>& causal_relation,
    const std::set<std::pair<std::size_t, std::size_t>>& sequence_edges,
    bool causal_relation_supplied, ILPMinerDiagnostics& diagnostics,
    std::size_t max_nodes) {
  (void)sequence_edges;
  (void)causal_relation_supplied;
  // A region is a pair of arbitrary-width input/output bit vectors. Candidate
  // generation is lazy: the safety bound applies before solving, so a large
  // alphabet never requires a 4^n allocation.
  const auto empty = [&] { return empty_mask(n); };
  const auto problem = build_region_problem(traces, n);
  std::vector<bool> final_activity(n, false);
  for (const auto& trace_entry : traces) {
    const auto retained_prefix =
        std::min(trace_entry.retained_prefix, trace_entry.sequence.size());
    if (retained_prefix == trace_entry.sequence.size() && retained_prefix != 0)
      final_activity[trace_entry.sequence.back()] = true;
  }
  std::vector<Region> regions;
  std::set<std::pair<BitMask, BitMask>> seen;
  std::size_t active_pair_start = 0;
  std::size_t active_pair_limit = max_nodes;
  bool active_pair_bounded = false;
  struct VisitResult {
    bool stop = false;
    bool feasible = false;
    Region region;
  };
  struct PairState {
    std::size_t from = 0;
    std::size_t to = 0;
    bool found = false;
    Region best;
  };
  const auto visit_candidate = [&](const Candidate& candidate) {
    VisitResult result;
    if (!any_bit(candidate.input) && !any_bit(candidate.output)) return result;
    if (!seen.insert({candidate.input, candidate.output}).second)
      return result;
    if (diagnostics.visited_nodes >= max_nodes) {
      diagnostics.exhausted = true;
      result.stop = true;
      return result;
    }
    if (active_pair_bounded &&
        diagnostics.visited_nodes - active_pair_start >= active_pair_limit) {
      result.stop = true;
      return result;
    }
    ++diagnostics.visited_nodes;
    ++diagnostics.lp_solved;
    ++diagnostics.branch_and_bound_nodes;
    result.feasible = solve_region(candidate.input, candidate.output, problem,
                                   result.region);
    return result;
  };
  const auto make_pair_candidate = [&](std::size_t from, std::size_t to) {
    Candidate candidate{empty(), empty()};
    set_bit(candidate.output, from);
    set_bit(candidate.input, to);
    return candidate;
  };
  const auto enumerate = [&](std::size_t required_from, std::size_t required_to,
                             const PairState& pair_state,
                             const std::function<void(const Region&)>& on_feasible) {
    (void)pair_state;
    Candidate candidate{empty(), empty()};
    std::vector<std::size_t> order;
    order.reserve(n);
    if (required_from < n) order.push_back(required_from);
    if (required_to < n && required_to != required_from)
      order.push_back(required_to);
    for (std::size_t activity = 0; activity < n; ++activity) {
      if (final_activity[activity] && activity != required_from &&
          activity != required_to)
        order.push_back(activity);
    }
    for (std::size_t activity = 0; activity < n; ++activity) {
      if (activity != required_from && activity != required_to &&
          !final_activity[activity])
        order.push_back(activity);
    }
    std::function<bool(std::size_t)> visit = [&](std::size_t depth) {
      if (depth == order.size()) {
        if (!any_bit(candidate.input) && !any_bit(candidate.output))
          return false;
        const auto outcome = visit_candidate(candidate);
        if (outcome.feasible) on_feasible(outcome.region);
        return outcome.stop;
      }
      const auto activity = order[depth];
      const bool must_output =
          required_from < n && activity == required_from;
      const bool must_input = required_to < n && activity == required_to;
      for (int state = 0; state < 4; ++state) {
        const bool output = state == 2 || state == 3;
        const bool input = state == 1 || state == 3;
        if ((must_output && !output) || (must_input && !input)) continue;
        if (output) set_bit(candidate.output, activity);
        if (input) set_bit(candidate.input, activity);
        if (visit(depth + 1)) return true;
        if (output)
          candidate.output[activity / 64] &=
              ~(std::uint64_t{1} << (activity % 64));
        if (input)
          candidate.input[activity / 64] &=
              ~(std::uint64_t{1} << (activity % 64));
      }
      return false;
    };
    return visit(0);
  };
  const auto collect = [&](const Candidate& candidate,
                           const std::function<void(const Region&)>& on_feasible) {
    const auto outcome = visit_candidate(candidate);
    if (outcome.feasible) on_feasible(outcome.region);
    return outcome.stop;
  };
  std::vector<PairState> pair_states;
  std::set<std::pair<std::size_t, std::size_t>> solved_pairs;
  for (const auto [from, to] : causal_relation) {
    if (!solved_pairs.insert({from, to}).second) continue;
    PairState state{from, to};
    seen.clear();
    const auto keep_best = [&](const Region& candidate) {
      if (!state.found || region_less(candidate, state.best)) {
        state.best = candidate;
        state.found = true;
      }
    };
    // Reserve one direct feasibility solve for every causal pair before the
    // optional exhaustive refinement.  A large alphabet must not prevent
    // later pairs from receiving their required place when the global bound
    // is reached while optimizing an earlier pair.
    const auto first = collect(make_pair_candidate(from, to), keep_best);
    pair_states.push_back(std::move(state));
    if (first) break;
  }

  // Refine each already-seeded pair while budget remains.  Pair states found
  // before exhaustion are retained even when a later refinement is partial.
  for (std::size_t pair_index = 0; pair_index < pair_states.size();
       ++pair_index) {
    auto& state = pair_states[pair_index];
    if (diagnostics.exhausted) break;
    seen.clear();
    const auto direct = make_pair_candidate(state.from, state.to);
    seen.insert({direct.input, direct.output});
    const auto keep_best = [&](const Region& candidate) {
      if (!state.found || region_less(candidate, state.best)) {
        state.best = candidate;
        state.found = true;
      }
    };
    const auto remaining_pairs = pair_states.size() - pair_index;
    active_pair_start = diagnostics.visited_nodes;
    active_pair_limit = std::max<std::size_t>(
        1, (max_nodes - diagnostics.visited_nodes + remaining_pairs - 1) /
               remaining_pairs);
    active_pair_bounded = true;
    enumerate(state.from, state.to, state, keep_best);
    active_pair_bounded = false;
  }
  for (const auto& state : pair_states) {
    if (state.found) regions.push_back(state.best);
  }
  std::sort(regions.begin(), regions.end(), region_less);
  regions.erase(std::unique(regions.begin(), regions.end(), [](const Region& a,
                                                               const Region& b) {
                  return masks_equal(a.input, b.input) &&
                         masks_equal(a.output, b.output) &&
                         a.initial == b.initial && a.final == b.final;
                }),
                regions.end());
  return regions;
}

}  // namespace

ILPMinerResult mine_ilp(const EventLog& event_log, const ILPMinerOptions& options) {
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("ILP miner noise_threshold must be in [0,1]");
  if (options.max_nodes == 0)
    throw std::invalid_argument("ILP miner max_nodes must be positive");

  const NormalizedLog log = normalize(event_log, options);
  const std::size_t n = log.names.size();
  const auto boundary_id = [&](const std::string& label) {
    if (label.empty()) return std::size_t{std::numeric_limits<std::size_t>::max()};
    const auto it = std::lower_bound(log.names.begin(), log.names.end(), label);
    return it == log.names.end() || *it != label
               ? std::size_t{std::numeric_limits<std::size_t>::max()}
               : static_cast<std::size_t>(it - log.names.begin());
  };
  const auto artificial_start = boundary_id(options.artificial_start_activity);
  const auto artificial_end = boundary_id(options.artificial_end_activity);
  std::vector<bool> artificial(n, false);
  if (artificial_start != std::numeric_limits<std::size_t>::max())
    artificial[artificial_start] = true;
  if (artificial_end != std::numeric_limits<std::size_t>::max())
    artificial[artificial_end] = true;
  std::vector<std::size_t> visible_id(n, std::numeric_limits<std::size_t>::max());
  std::vector<std::string> visible_names;
  visible_names.reserve(n);
  for (std::size_t a = 0; a < n; ++a) {
    if (artificial[a]) continue;
    visible_id[a] = visible_names.size();
    visible_names.push_back(log.names[a]);
  }
  std::vector<std::size_t> starts(n), ends(n), incoming(n), outgoing(n);
  for (const auto& trace_entry : log.traces) {
    const auto& trace = trace_entry.sequence;
    if (trace.empty()) continue;
    ++starts[trace.front()];
    ++ends[trace.back()];
    for (std::size_t i = 1; i < trace.size(); ++i) {
      ++outgoing[trace[i - 1]];
      ++incoming[trace[i]];
    }
  }

  ILPMinerResult result;
  result.net.set_activities(std::move(visible_names));
  const std::size_t source = result.net.add_place();
  const std::size_t sink = result.net.add_place();
  result.net.set_source(source);
  result.net.set_sink(sink);

  const auto regions = discover_regions(log.traces, n, log.causal_relation,
                                        log.sequence_edges,
                                        log.causal_relation_supplied,
                                        result.diagnostics, options.max_nodes);
  std::vector<std::vector<std::size_t>> inputs(n), outputs(n);
  std::vector<bool> persistent_output(n, false);
  std::vector<std::size_t> initial_places;
  std::vector<std::size_t> final_places;
  for (const auto& region : regions) {
    // Each selected integer region is a place in the discovered net.  The
    // solver's zero-deviation constraints already enforce all required
    // boundaries, so no post-hoc overlap or marking filters are applied here.
    const std::size_t place = result.net.add_place();
    for (std::size_t a = 0; a < n; ++a) {
      if (has_bit(region.input, a)) inputs[a].push_back(place);
      if (has_bit(region.output, a)) {
        outputs[a].push_back(place);
        if (region.final != 0) persistent_output[a] = true;
      }
    }
    // Initial/final tokens of a region are routed through the artificial
    // source/sink, keeping WorkflowMarkings canonical (one source token and
    // one sink token) while still supporting loops and persistent places.
    if (region.initial != 0) initial_places.push_back(place);
    if (region.final != 0) final_places.push_back(place);
  }
  for (std::size_t a = 0; a < n; ++a) {
    if (initial_places.empty() && (starts[a] != 0 || incoming[a] == 0))
      inputs[a].push_back(source);
    if (final_places.empty() && (ends[a] != 0 || outgoing[a] == 0) &&
        !persistent_output[a])
      outputs[a].push_back(sink);
  }
  // Every visible transition must lie on a source-to-sink path.  When a
  // persistent region split/join is present, give an otherwise unconstrained
  // activity its own routing place so it does not become an isolated node.
  for (std::size_t a = 0; a < n; ++a) {
    if (inputs[a].empty()) {
      if (initial_places.empty()) {
        inputs[a].push_back(source);
      } else {
        const auto place = result.net.add_place();
        initial_places.push_back(place);
        inputs[a].push_back(place);
      }
    }
    if (outputs[a].empty()) {
      if (final_places.empty()) {
        outputs[a].push_back(sink);
      } else {
        const auto place = result.net.add_place();
        final_places.push_back(place);
        outputs[a].push_back(place);
      }
    }
  }

  // A region marking is a vector, so initialize/finalize all persistent
  // regions in one source split and one sink join.  Separate silent edges
  // would compete for the single source token and make otherwise valid logs
  // unreplayable.
  if (!initial_places.empty())
    result.net.add_transition(std::nullopt, {source}, initial_places);
  if (!final_places.empty())
    result.net.add_transition(std::nullopt, final_places, {sink});
  for (std::size_t a = 0; a < n; ++a) {
    if (artificial[a])
      result.net.add_transition(std::nullopt, std::move(inputs[a]),
                                std::move(outputs[a]));
    else
      result.net.add_transition(visible_id[a], std::move(inputs[a]),
                                std::move(outputs[a]));
  }

  result.markings.initial.assign(result.net.place_count(), 0);
  result.markings.final.assign(result.net.place_count(), 0);
  result.markings.initial[result.net.source()] = 1;
  result.markings.final[result.net.sink()] = 1;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.exhausted
                                  ? "node bound reached; partial region search returned"
                                  : "bounded integer-region search completed";
  return result;
}

}  // namespace minera
