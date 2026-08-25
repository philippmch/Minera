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
bool solve_region(const BitMask& input, const BitMask& output,
                  const std::vector<NormalizedLog::Trace>& traces,
                  std::size_t max_tokens, Region& out) {
  if (!any_bit(input) && !any_bit(output)) return false;
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
        const bool consumes = has_bit(input, activity);
        const bool produces = has_bit(output, activity);
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
  const std::size_t max_tokens = [&] {
    std::size_t longest = 0;
    for (const auto& trace : traces)
      longest = std::max(longest, trace.sequence.size());
    return longest;
  }();
  // A region is a pair of arbitrary-width input/output bit vectors. Candidate
  // generation below is lazy: the safety bound applies before solving, so a
  // large alphabet never requires a 4^n allocation.
  const auto empty = [&] { return empty_mask(n); };
  std::vector<Region> regions;
  std::set<std::pair<BitMask, BitMask>> seen;
  const auto compatible = [&](const Candidate& candidate) {
    // Automatically derived one-way relations constrain composite places to
    // retained continuations. Explicit relations remain permissive so one
    // composite place can satisfy multiple requested pairs.
    if (!causal_relation.empty() &&
        (!any_bit(candidate.input) || !any_bit(candidate.output)))
      return false;
    if (causal_relation_supplied || causal_relation.empty()) return true;
    for (std::size_t from = 0; from < n; ++from) {
      if (!has_bit(candidate.output, from)) continue;
      for (std::size_t to = 0; to < n; ++to) {
        if (has_bit(candidate.input, to) &&
            !sequence_edges.count({from, to}))
          return false;
      }
    }
    return true;
  };
  struct VisitResult {
    bool stop = false;
    bool feasible = false;
    Region region;
  };
  const auto visit_candidate = [&](const Candidate& candidate) {
    VisitResult result;
    if (!any_bit(candidate.input) && !any_bit(candidate.output)) return result;
    if (diagnostics.visited_nodes >= max_nodes) {
      diagnostics.exhausted = true;
      result.stop = true;
      return result;
    }
    ++diagnostics.visited_nodes;
    if (!compatible(candidate)) return result;
    if (!seen.insert({candidate.input, candidate.output}).second)
      return result;
    ++diagnostics.lp_solved;
    ++diagnostics.branch_and_bound_nodes;
    if (!causal_relation_supplied && bit_count(candidate.input) == 1 &&
        bit_count(candidate.output) == 1) {
      std::size_t from = n;
      std::size_t to = n;
      for (std::size_t activity = 0; activity < n; ++activity) {
        if (has_bit(candidate.output, activity)) from = activity;
        if (has_bit(candidate.input, activity)) to = activity;
      }
      if (from == n || to == n || !sequence_edges.count({from, to}))
        return result;
    }
    result.feasible = solve_region(candidate.input, candidate.output, traces,
                                   max_tokens, result.region);
    return result;
  };
  const auto make_pair_candidate = [&](std::size_t from, std::size_t to) {
    Candidate candidate{empty(), empty()};
    set_bit(candidate.output, from);
    set_bit(candidate.input, to);
    return candidate;
  };
  const auto make_loop_candidate = [&](std::size_t activity) {
    Candidate candidate{empty(), empty()};
    set_bit(candidate.input, activity);
    set_bit(candidate.output, activity);
    return candidate;
  };
  const auto enumerate = [&](std::size_t required_from, std::size_t required_to,
                             const std::function<void(const Region&)>& on_feasible) {
    Candidate candidate{empty(), empty()};
    std::function<bool(std::size_t)> visit = [&](std::size_t activity) {
      if (activity == n) {
        if (!any_bit(candidate.input) && !any_bit(candidate.output))
          return false;
        const auto outcome = visit_candidate(candidate);
        if (outcome.feasible) on_feasible(outcome.region);
        return outcome.stop;
      }
      const bool must_output = required_from != n && activity == required_from;
      const bool must_input = required_to != n && activity == required_to;
      for (int state = 0; state < 4; ++state) {
        const bool output = state == 2 || state == 3;
        const bool input = state == 1 || state == 3;
        if ((must_output && !output) || (must_input && !input)) continue;
        if (output) set_bit(candidate.output, activity);
        if (input) set_bit(candidate.input, activity);
        if (visit(activity + 1)) return true;
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
  const auto enumerate_legacy =
      [&](std::size_t required_from, std::size_t required_to,
          const std::function<void(const Region&)>& on_feasible) {
        const auto mask = (std::uint64_t{1} << n) - 1;
        const auto combinations = std::uint64_t{1} << (2 * n);
        for (std::uint64_t code = 1; code < combinations; ++code) {
          Candidate candidate{empty(), empty()};
          const auto input = code & mask;
          const auto output = code >> n;
          for (std::size_t activity = 0; activity < n; ++activity) {
            if ((input & (std::uint64_t{1} << activity)) != 0)
              set_bit(candidate.input, activity);
            if ((output & (std::uint64_t{1} << activity)) != 0)
              set_bit(candidate.output, activity);
          }
          if (!causal_relation.empty() &&
              (!any_bit(candidate.input) || !any_bit(candidate.output)))
            continue;
          if (required_from != n &&
              (!has_bit(candidate.output, required_from) ||
               !has_bit(candidate.input, required_to)))
            continue;
          const auto outcome = visit_candidate(candidate);
          if (outcome.feasible) on_feasible(outcome.region);
          if (outcome.stop) return true;
        }
        return false;
      };
  const auto append_region = [&](const Region& region) { regions.push_back(region); };

  if (!causal_relation_supplied) {
    for (const auto [from, to] : causal_relation) {
      if (collect(make_pair_candidate(from, to), append_region)) break;
    }
    if (!diagnostics.exhausted) {
      for (std::size_t activity = 0; activity < n; ++activity) {
        if (collect(make_loop_candidate(activity), append_region)) break;
      }
    }
    if (!diagnostics.exhausted) {
      if (n <= 8)
        enumerate_legacy(n, n, append_region);
      else
        enumerate(n, n, append_region);
    }
  } else {
    // Solve each supplied pair independently. A composite region may satisfy
    // several pairs, but each pair still receives its own best feasible
    // region; exact duplicate regions are removed after selection.
    std::set<std::pair<std::size_t, std::size_t>> solved_pairs;
    for (const auto [from, to] : causal_relation) {
      if (!solved_pairs.insert({from, to}).second) continue;
      // The reference optimizes each requested pair independently; a
      // composite candidate may therefore be evaluated again for a later
      // pair even when it was already considered for an earlier one.
      seen.clear();
      bool found = false;
      Region best;
      const auto keep_internal = [&](const Region& candidate) {
        if (candidate.initial != 0 || candidate.final != 0) return;
        if (!found || region_less(candidate, best)) {
          best = candidate;
          found = true;
        }
      };
      if (collect(make_pair_candidate(from, to), keep_internal) &&
          diagnostics.exhausted)
        break;
      if (!diagnostics.exhausted) {
        if (n <= 8)
          enumerate_legacy(from, to, keep_internal);
        else
          enumerate(from, to, keep_internal);
      }
      if (found) regions.push_back(best);
      if (diagnostics.exhausted) break;
    }
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
    const bool self_loop_region = [&] {
      for (std::size_t activity = 0; activity < n; ++activity)
        if (has_bit(region.input, activity) && has_bit(region.output, activity))
          return true;
      return false;
    }();
    const auto overlap_count = [&] {
      std::size_t count = 0;
      for (std::size_t activity = 0; activity < n; ++activity)
        if (has_bit(region.input, activity) && has_bit(region.output, activity))
          ++count;
      return count;
    };
    // A pure self-loop is only needed when that activity actually repeats;
    // otherwise it contributes an unconstrained token and makes reachability
    // needlessly branch.  Mixed regions (for example a-loop -> b) remain.
    if (self_loop_region && masks_equal(region.input, region.output)) {
      bool repeated = false;
      for (const auto& trace_entry : log.traces) {
        const auto& trace = trace_entry.sequence;
        std::size_t count = 0;
        for (const auto activity : trace)
          if (has_bit(region.input, activity)) ++count;
        if (count > 1) { repeated = true; break; }
      }
      if (!repeated) continue;
    }
    if (overlap_count() > 1) continue;
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
  result.markings.initial[source] = 1;
  result.markings.final[sink] = 1;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.exhausted
                                  ? "node bound reached; partial region search returned"
                                  : "bounded integer-region search completed";
  return result;
}

}  // namespace minera
