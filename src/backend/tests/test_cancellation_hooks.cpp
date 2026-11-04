// Cancellation hooks of the long-running library calls the server wires to a
// client disconnect (connection_cancel_hook): a callback that never fires is
// polled and changes nothing, one that fires stops the call with
// OperationCancelled, and a callback that starts returning true at some
// instant (a client that went away) stops a long run within a second.
#include "minera/cancellation.hpp"
#include "minera/concept_drift.hpp"
#include "minera/conformance.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/model_decoration.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree_alignment.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

minera::EventLog random_log(std::size_t cases, std::size_t min_length, std::size_t max_length,
                            std::size_t alphabet, std::uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<std::size_t> length(min_length, max_length);
  std::uniform_int_distribution<std::size_t> letter(0, alphabet - 1);
  minera::EventLog log;
  for (std::size_t c = 0; c < cases; ++c) {
    minera::Trace trace;
    trace.case_id = "c" + std::to_string(c);
    const auto n = length(rng);
    for (std::size_t i = 0; i < n; ++i) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = "a" + std::to_string(letter(rng));
      event.timestamp = "2024-01-01T00:00:00Z";
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// Runs `call` with a callback that never fires and checks it is polled and
// that the serialized result equals the run without a callback; then with a
// callback that fires on its `fire_at`-th call, which must throw.
template <class Options, class Call>
void check_hook(const char* name, Options options, Call call, std::size_t fire_at = 2) {
  const auto expected = call(options);
  std::size_t polls = 0;
  options.cancelled = [&polls] { ++polls; return false; };
  const auto actual = call(options);
  if (polls == 0 || actual != expected) {
    std::cerr << name << ": polls " << polls << ", identical " << (actual == expected) << '\n';
    assert(false);
  }
  std::size_t calls = 0;
  options.cancelled = [&calls, fire_at] { return ++calls >= fire_at; };
  bool threw = false;
  try { (void)call(options); }
  catch (const minera::OperationCancelled&) { threw = true; }
  if (!threw) std::cerr << name << ": a firing callback did not cancel\n";
  assert(threw);
}

// A client that disconnects `after` into a long run: the call must still be
// running then (it throws, so it had not finished) and stop within a second.
template <class Options, class Call>
void check_disconnect(const char* name, Options options, Call call,
                      std::chrono::milliseconds after = std::chrono::milliseconds(150)) {
  const auto started = Clock::now();
  const auto disconnected = started + after;
  options.cancelled = [disconnected] { return Clock::now() >= disconnected; };
  bool threw = false;
  try { (void)call(options); }
  catch (const minera::OperationCancelled&) { threw = true; }
  const auto stopped = Clock::now();
  const auto late = std::chrono::duration_cast<std::chrono::milliseconds>(stopped - disconnected);
  std::cerr << name << ": stopped " << late.count() << " ms after the disconnect"
            << (threw ? "" : " (finished first)") << '\n';
  assert(threw);
  assert(late < std::chrono::seconds(1));
}

// For calls that take the callback alone.
struct CallbackOnly {
  std::function<bool()> cancelled;
};

std::map<std::string, std::uint64_t> starts_of(const minera::EventLog& log) {
  std::map<std::string, std::uint64_t> out;
  for (const auto& trace : log.traces) if (!trace.events.empty()) ++out[trace.events.front().activity];
  return out;
}

std::map<std::string, std::uint64_t> ends_of(const minera::EventLog& log) {
  std::map<std::string, std::uint64_t> out;
  for (const auto& trace : log.traces) if (!trace.events.empty()) ++out[trace.events.back().activity];
  return out;
}

}  // namespace

int main() {
  const auto small = random_log(40, 2, 7, 5, 7);
  const auto tree = minera::discover_process_tree(small);
  const auto net = minera::to_petri_net(tree);
  const auto dfg = minera::discover_frequency_dfg(small);
  const auto starts = starts_of(small);
  const auto ends = ends_of(small);

  // DFG alignment: polled per search step and per distinct variant.
  minera::DfgAlignmentOptions dfg_options;
  dfg_options.routing_names = std::make_pair(std::string("@source"), std::string("@sink"));
  const auto align_dfg = [&](const minera::EventLog& log) {
    return [&](const minera::DfgAlignmentOptions& o) {
      return minera::dfg_alignment_json(minera::align_log_on_dfg(log, dfg, starts, ends, o));
    };
  };
  check_hook("DFG alignment", dfg_options, align_dfg(small));

  // Process-tree alignment, through each reference port and the native search.
  for (const auto variant : {minera::ProcessTreeAlignmentVariant::kSearchGraphPt,
                             minera::ProcessTreeAlignmentVariant::kDynamicProgramming,
                             minera::ProcessTreeAlignmentVariant::kMilp,
                             minera::ProcessTreeAlignmentVariant::kApproximatedOriginal}) {
    for (const bool native : {false, true}) {
      minera::ProcessTreeAlignmentOptions options;
      options.variant = variant;
      options.native_search = native;
      check_hook("process-tree alignment", options, [&](const auto& o) {
        return minera::process_tree_alignment_json(minera::align_process_tree(small, tree, o));
      });
    }
  }

  // Decomposed alignment: the reference port polls per variant, the native
  // search per trace and per search step.
  for (const bool native : {false, true}) {
    minera::DecomposedAlignmentOptions options;
    options.native_search = native;
    check_hook("decomposed alignment", options, [&](const auto& o) {
      return minera::decomposed_alignment_json(minera::align_decomposed(small, net, o));
    });
  }

  // Edit distance against a reference log.
  const auto reference = random_log(30, 2, 7, 5, 11);
  check_hook("edit-distance alignment", minera::EditDistanceOptions{}, [&](const auto& o) {
    return minera::typed_edit_distance_json(minera::align_typed_edit_distance(small, reference, o));
  });

  // Token replay, forward (full result) and backwards (trace rows).
  check_hook("forward token replay", minera::TokenReplayOptions{}, [&](const auto& o) {
    return minera::token_replay_result_json(minera::replay_token_result(small, net, o));
  });
  minera::TokenReplayOptions backwards;
  backwards.variant = minera::TokenReplayVariant::kBackwards;
  check_hook("backwards token replay", backwards, [&](const auto& o) {
    return minera::token_replay_traces_json(minera::replay_token_traces(small, net, o), o.variant);
  });

  // Concept drift: polled per sub-log and per permutation.
  minera::ConceptDriftOptions drift;
  drift.sub_log_size = 2;
  drift.window_size = 3;
  drift.num_permutations = 40;
  check_hook("concept drift", drift, [&](const auto& o) {
    return minera::concept_drift_json(minera::detect_concept_drift(small, o));
  });

  // Monte Carlo: polled while fitting (token replay) and per scheduler event.
  minera::MonteCarloOptions monte_carlo;
  monte_carlo.simulations = 60;
  check_hook("Monte Carlo", monte_carlo, [&](const auto& o) {
    return minera::monte_carlo_json(minera::monte_carlo(small, net, o));
  });

  // Mining stages: the token conformance check (fitness replay and the
  // precision prefix walk) and the model decoration, on enough variants to
  // reach their polls.
  const auto varied = random_log(400, 2, 9, 6, 9);
  const auto varied_net = minera::to_petri_net(minera::discover_process_tree(varied));
  check_hook("token conformance check", minera::TokenReplayOptions{}, [&](const auto& o) {
    const auto result = minera::check_conformance(varied, varied_net, o);
    std::string out = std::to_string(result.fitness) + ' ' + std::to_string(result.precision) +
                      ' ' + std::to_string(result.fitting_traces);
    for (const auto& variant : result.variants) out += ' ' + std::to_string(variant.fitness);
    return out;
  });
  check_hook("model decoration", CallbackOnly{}, [&](const CallbackOnly& o) {
    const auto decoration = minera::decorate_model(varied, varied_net, "concept:name",
                                                   "time:timestamp", o.cancelled);
    return std::make_tuple(decoration.transition_counts, decoration.place_counts,
                           decoration.decorated_traces);
  });

  // Disconnects 150 ms into runs that take far longer.
  const auto noisy = random_log(30000, 30, 50, 14, 3);
  check_disconnect("DFG alignment", dfg_options, align_dfg(noisy));
  // Concept drift's work bounds cap a run at about a second; this one is
  // near them (about 1.5 s at -O1).
  minera::ConceptDriftOptions long_drift;
  long_drift.sub_log_size = 50;
  long_drift.window_size = 5;
  long_drift.num_permutations = 20000;
  long_drift.materialize_sublogs = false;
  const auto drift_log = random_log(5000, 10, 10, 3, 3);
  check_disconnect("concept drift", long_drift, [&](const auto& o) {
    return minera::detect_concept_drift(drift_log, o).change_points.size();
  });
  // Fitted on the noisy log (a token replay of 1.2M events), then simulated
  // to the native limit of 10,000 cases.
  minera::MonteCarloOptions long_simulation;
  long_simulation.simulations = 10000;
  check_disconnect("Monte Carlo", long_simulation, [&](const auto& o) {
    return minera::monte_carlo(noisy, net, o).log.traces.size();
  });
  const auto wide = random_log(500, 30, 60, 40, 5);
  check_disconnect("edit-distance alignment", minera::EditDistanceOptions{}, [&](const auto& o) {
    return minera::align_typed_edit_distance(wide, wide, o).size();
  });
  return 0;
}
