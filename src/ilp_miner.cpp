#include "minera/ilp_miner.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

struct Region {
  // One bit per activity.  A place may both consume and produce an activity
  // (a self-loop), which is deliberately distinct from a zero net change.
  std::uint64_t input = 0;
  std::uint64_t output = 0;
  std::size_t initial = 0;
  std::size_t final = 0;
};

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

// Check one candidate signature and compute its least feasible initial token
// count.  This is the integer-region LP: every trace prefix must be enabled
// without borrowing tokens, and all complete traces must have one common final
// marking.  The least initial marking gives a deterministic objective.
bool solve_region(std::uint64_t input, std::uint64_t output,
                  const std::vector<NormalizedLog::Trace>& traces,
                  std::size_t max_tokens, Region& out) {
  if ((input | output) == 0) return false;
  for (std::size_t initial = 0; initial <= max_tokens; ++initial) {
    bool valid = true;
    std::size_t common_final = 0;
    bool have_complete_final = false;
    bool have_observed_prefix = false;
    for (const auto& trace_entry : traces) {
      const auto& trace = trace_entry.sequence;
      const auto retained_prefix =
          std::min(trace_entry.retained_prefix, trace.size());
      if (retained_prefix != 0) have_observed_prefix = true;
      std::size_t marking = initial;
      for (std::size_t i = 0; i < retained_prefix; ++i) {
        const auto activity = trace[i];
        const bool consumes = (input & (std::uint64_t{1} << activity)) != 0;
        const bool produces = (output & (std::uint64_t{1} << activity)) != 0;
        // Pre-fire non-negativity is essential for input/output self-loops.
        if (consumes && marking == 0) {
          valid = false;
          break;
        }
        if (consumes) --marking;
        if (produces) ++marking;
      }
      if (!valid) break;
      // A fully retained variant contributes the final-marking equality.  If
      // alpha cut its suffix, the reference LP has no later equality row for
      // that variant, so its partial marking must not reject the region.
      if (retained_prefix == trace.size()) {
        if (!have_complete_final) {
          common_final = marking;
          have_complete_final = true;
        } else if (common_final != marking) {
          valid = false;
          break;
        }
      }
    }
    if (valid && have_observed_prefix) {
      out = Region{input, output, initial, common_final};
      return true;
    }
  }
  return false;
}

bool region_less(const Region& a, const Region& b) {
  const auto arcs = [](const Region& r) {
    return static_cast<std::size_t>(std::popcount(r.input)) +
           static_cast<std::size_t>(std::popcount(r.output));
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
  const std::size_t max_tokens = [&] {
    std::size_t longest = 0;
    for (const auto& trace : traces)
      longest = std::max(longest, trace.sequence.size());
    return longest;
  }();
  const std::uint64_t combinations = std::uint64_t{1} << (2 * n);
  std::vector<Region> regions;
  std::vector<std::uint64_t> candidates;
  candidates.reserve(static_cast<std::size_t>(combinations - 1));
  std::set<std::uint64_t> seen;
  for (const auto& [from, to] : causal_relation) {
    // A causal pair from=>to is represented by a place produced by `from`
    // and consumed by `to`: output bits encode producers, input bits consume.
    const auto code = (std::uint64_t{1} << to) |
                      (std::uint64_t{1} << (n + from));
    if (seen.insert(code).second) candidates.push_back(code);
  }
  // Self-loop regions are not part of the one-way causal relation because an
  // activity is its own reverse edge.  They still have to be considered for
  // repeated occurrences, so add the bounded one-activity loop signatures
  // explicitly; non-repeating loops are discarded by the region policy below.
  for (std::size_t activity = 0; activity < n; ++activity) {
    const auto code = (std::uint64_t{1} << activity) |
                      (std::uint64_t{1} << (n + activity));
    if (seen.insert(code).second) candidates.push_back(code);
  }
  if (!causal_relation.empty()) {
    const std::set<std::pair<std::size_t, std::size_t>> causal_pairs(
        causal_relation.begin(), causal_relation.end());
    // A causal pair fixes one producer/consumer bit in the reference LP, but
    // the resulting place may contain additional compatible activities.  Add
    // those bounded combinations after the deterministic singleton order so
    // choice and repeated-activity places remain discoverable without falling
    // back to an unbounded signature sweep.
    for (std::uint64_t code = 1; code < combinations; ++code) {
      if (seen.count(code) != 0) continue;
      const std::uint64_t input = code & ((std::uint64_t{1} << n) - 1);
      const std::uint64_t output = code >> n;
      if (input == 0 || output == 0) continue;
      bool contains_requested_pair = false;
      bool compatible = true;
      for (std::size_t from = 0; from < n && compatible; ++from) {
        if ((output & (std::uint64_t{1} << from)) == 0) continue;
        for (std::size_t to = 0; to < n; ++to) {
          if ((input & (std::uint64_t{1} << to)) == 0) continue;
          if (causal_pairs.count({from, to}) != 0)
            contains_requested_pair = true;
          if (!causal_relation_supplied) {
            if (causal_pairs.count({from, to}) == 0) {
              // Self-loops are absent from the one-way causal relation but
              // are valid when their cumulative continuation was retained.
              if (from != to || !sequence_edges.count({from, to})) {
                compatible = false;
                break;
              }
            }
            if (!sequence_edges.count({from, to})) {
              compatible = false;
              break;
            }
          }
        }
      }
      // Explicit pairs are required constraints, not an exhaustive list:
      // permit additional producer/consumer arcs once one requested pair is
      // present so a composite region can fit all observed variants.
      if (causal_relation_supplied && !contains_requested_pair) compatible = false;
      if (compatible && seen.insert(code).second) candidates.push_back(code);
    }
  }
  if (causal_relation.empty()) {
    for (std::uint64_t code = 1; code < combinations; ++code)
      if (seen.insert(code).second) candidates.push_back(code);
  }
  const auto visit_candidate = [&](std::uint64_t code, Region& region) {
    ++diagnostics.visited_nodes;
    ++diagnostics.lp_solved;
    // Each candidate is a tiny integer feasibility problem.  Count its
    // deterministic branch (initial-token search) separately from candidates.
    ++diagnostics.branch_and_bound_nodes;
    const std::uint64_t input = code & ((std::uint64_t{1} << n) - 1);
    const std::uint64_t output = code >> n;
    if (std::popcount(input) == 1 && std::popcount(output) == 1) {
      const auto from = static_cast<std::size_t>(std::countr_zero(output));
      const auto to = static_cast<std::size_t>(std::countr_zero(input));
      if (!causal_relation_supplied && sequence_edges.count({from, to}) == 0)
        return false;
    }
    return solve_region(input, output, traces, max_tokens, region);
  };
  if (!causal_relation_supplied) {
    for (const auto code : candidates) {
      if (diagnostics.visited_nodes >= max_nodes) {
        diagnostics.exhausted = true;
        break;
      }
      Region region;
      if (visit_candidate(code, region)) regions.push_back(region);
    }
  } else {
    // Solve each supplied pair independently.  A composite region may satisfy
    // several pairs, but each pair still receives its own best feasible
    // region; only exact duplicate regions are removed below.
    std::set<std::pair<std::size_t, std::size_t>> solved_pairs;
    for (const auto& target : causal_relation) {
      if (!solved_pairs.insert(target).second) continue;
      const auto [from, to] = target;
      const auto from_bit = std::uint64_t{1} << from;
      const auto to_bit = std::uint64_t{1} << to;
      bool found = false;
      Region best;
      for (const auto code : candidates) {
        const std::uint64_t input = code & ((std::uint64_t{1} << n) - 1);
        const std::uint64_t output = code >> n;
        if ((output & from_bit) == 0 || (input & to_bit) == 0) continue;
        if (diagnostics.visited_nodes >= max_nodes) {
          diagnostics.exhausted = true;
          break;
        }
        Region candidate;
        if (!visit_candidate(code, candidate)) continue;
        // Explicit causal places are internal workflow regions.  Boundary
        // tokens are routed separately through the source/sink, so a
        // nonzero-marking candidate would be discarded during net assembly
        // after winning the per-pair objective.
        if (candidate.initial != 0 || candidate.final != 0) continue;
        if (!found || region_less(candidate, best)) {
          best = candidate;
          found = true;
        }
      }
      if (found) regions.push_back(best);
      if (diagnostics.exhausted) break;
    }
  }
  std::sort(regions.begin(), regions.end(), region_less);
  regions.erase(std::unique(regions.begin(), regions.end(), [](const Region& a,
                                                               const Region& b) {
                  return a.input == b.input && a.output == b.output &&
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
  // Region enumeration is exponential in the activity cardinality.  Refuse
  // pathological logs before allocating signatures or prefix matrices.
  if (log.names.size() > 8)
    throw std::invalid_argument("ILP miner supports at most 8 activities");

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
    const bool self_loop_region = (region.input & region.output) != 0;
    const auto overlap = region.input & region.output;
    // A pure self-loop is only needed when that activity actually repeats;
    // otherwise it contributes an unconstrained token and makes reachability
    // needlessly branch.  Mixed regions (for example a-loop -> b) remain.
    if (self_loop_region && region.input == region.output) {
      bool repeated = false;
      for (const auto& trace_entry : log.traces) {
        const auto& trace = trace_entry.sequence;
        std::size_t count = 0;
        for (const auto activity : trace)
          if ((region.input & (std::uint64_t{1} << activity)) != 0) ++count;
        if (count > 1) { repeated = true; break; }
      }
      if (!repeated) continue;
    }
    if (std::popcount(overlap) > 1) continue;
    // Workflow markings have one source token and one sink token.  A silent
    // edge cannot manufacture an initial multiplicity greater than one, and
    // retaining every such region creates a combinatorial silent-routing
    // search.  Repeated traces are represented by the canonical one-token
    // self-loop region instead.
    if (region.initial > 1 || region.final > 1 ||
        (region.initial != 0 && !self_loop_region) ||
        (region.final != 0 && !self_loop_region))
      continue;
    const std::size_t place = result.net.add_place();
    for (std::size_t a = 0; a < n; ++a) {
      const auto bit = std::uint64_t{1} << a;
      if ((region.input & bit) != 0) inputs[a].push_back(place);
      if ((region.output & bit) != 0) {
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
  result.markings.initial[source] = 1;
  result.markings.final[sink] = 1;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.exhausted
                                  ? "node bound reached; partial region search returned"
                                  : "bounded integer-region search completed";
  return result;
}

}  // namespace minera
