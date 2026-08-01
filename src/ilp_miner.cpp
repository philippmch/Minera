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
  std::vector<std::vector<std::size_t>> traces;
};

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
    if (!sequence.empty()) raw.push_back(std::move(sequence));
  }
  if (raw.empty() || names.empty())
    throw std::invalid_argument("ILP miner found no activities");

  NormalizedLog out;
  out.names.assign(names.begin(), names.end());
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < out.names.size(); ++i) ids.emplace(out.names[i], i);
  out.traces.reserve(raw.size());
  for (const auto& sequence : raw) {
    std::vector<std::size_t> encoded;
    encoded.reserve(sequence.size());
    for (const auto& value : sequence) encoded.push_back(ids.at(value));
    out.traces.push_back(std::move(encoded));
  }
  return out;
}

// Check one candidate signature and compute its least feasible initial token
// count.  This is the integer-region LP: every trace prefix must be enabled
// without borrowing tokens, and all complete traces must have one common final
// marking.  The least initial marking gives a deterministic objective.
bool solve_region(std::uint64_t input, std::uint64_t output,
                  const std::vector<std::vector<std::size_t>>& traces,
                  std::size_t max_tokens, Region& out) {
  if ((input | output) == 0) return false;
  for (std::size_t initial = 0; initial <= max_tokens; ++initial) {
    bool valid = true;
    std::size_t common_final = 0;
    bool have_final = false;
    for (const auto& trace : traces) {
      std::size_t marking = initial;
      for (const auto activity : trace) {
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
      if (!have_final) {
        common_final = marking;
        have_final = true;
      } else if (common_final != marking) {
        valid = false;
        break;
      }
    }
    if (valid) {
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
    const std::vector<std::vector<std::size_t>>& traces, std::size_t n,
    ILPMinerDiagnostics& diagnostics, std::size_t max_nodes) {
  const std::size_t max_tokens = [&] {
    std::size_t longest = 0;
    for (const auto& trace : traces) longest = std::max(longest, trace.size());
    return longest;
  }();
  const std::uint64_t combinations = std::uint64_t{1} << (2 * n);
  std::vector<Region> regions;
  for (std::uint64_t code = 1; code < combinations; ++code) {
    if (diagnostics.visited_nodes >= max_nodes) {
      diagnostics.exhausted = true;
      break;
    }
    ++diagnostics.visited_nodes;
    ++diagnostics.lp_solved;
    // Each candidate is a tiny integer feasibility problem.  Count its
    // deterministic branch (initial-token search) separately from candidates.
    ++diagnostics.branch_and_bound_nodes;
    const std::uint64_t input = code & ((std::uint64_t{1} << n) - 1);
    const std::uint64_t output = code >> n;
    Region region;
    if (solve_region(input, output, traces, max_tokens, region))
      regions.push_back(region);
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
  std::vector<std::size_t> starts(n), ends(n), incoming(n), outgoing(n);
  for (const auto& trace : log.traces) {
    if (trace.empty()) continue;
    ++starts[trace.front()];
    ++ends[trace.back()];
    for (std::size_t i = 1; i < trace.size(); ++i) {
      ++outgoing[trace[i - 1]];
      ++incoming[trace[i]];
    }
  }

  ILPMinerResult result;
  result.net.set_activities(log.names);
  const std::size_t source = result.net.add_place();
  const std::size_t sink = result.net.add_place();
  result.net.set_source(source);
  result.net.set_sink(sink);

  const auto regions = discover_regions(log.traces, n, result.diagnostics, options.max_nodes);
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
      for (const auto& trace : log.traces) {
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
  for (std::size_t a = 0; a < n; ++a)
    result.net.add_transition(a, std::move(inputs[a]), std::move(outputs[a]));

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
