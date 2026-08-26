#include "minera/petri_net_reduction.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

struct WorkingTransition {
  std::optional<std::size_t> activity;
  std::vector<std::size_t> inputs;
  std::vector<std::size_t> outputs;
  std::vector<std::size_t> reset_inputs;
  std::vector<std::size_t> inhibitor_inputs;
};

struct WorkingNet {
  std::size_t place_count = 0;
  std::size_t source = 0;
  std::size_t sink = 0;
  std::vector<std::string> activities;
  std::vector<WorkingTransition> transitions;
};

bool contains(const std::vector<std::size_t>& values, std::size_t value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool only_place(const std::vector<std::size_t>& values,
                std::size_t expected) {
  return !values.empty() &&
         std::all_of(values.begin(), values.end(),
                     [&](std::size_t value) { return value == expected; });
}

bool has_extended_arcs(const WorkingTransition& transition) {
  return !transition.reset_inputs.empty() ||
         !transition.inhibitor_inputs.empty();
}

bool mentions_extended_place(const WorkingNet& net, std::size_t place) {
  for (const auto& transition : net.transitions) {
    if (contains(transition.reset_inputs, place) ||
        contains(transition.inhibitor_inputs, place))
      return true;
  }
  return false;
}

std::set<std::size_t> extended_consumers(const WorkingNet& net,
                                         std::size_t place,
                                         bool reset) {
  std::set<std::size_t> result;
  for (std::size_t transition = 0; transition < net.transitions.size();
       ++transition) {
    const auto& candidate = net.transitions[transition];
    const auto& arcs = reset ? candidate.reset_inputs
                             : candidate.inhibitor_inputs;
    if (contains(arcs, place)) result.insert(transition);
  }
  return result;
}

std::vector<std::size_t> ordinary_producers(const WorkingNet& net,
                                             std::size_t place) {
  std::vector<std::size_t> result;
  for (std::size_t transition = 0; transition < net.transitions.size();
       ++transition) {
    if (contains(net.transitions[transition].outputs, place))
      result.push_back(transition);
  }
  return result;
}

std::vector<std::size_t> ordinary_consumers(const WorkingNet& net,
                                            std::size_t place) {
  std::vector<std::size_t> result;
  for (std::size_t transition = 0; transition < net.transitions.size();
       ++transition) {
    if (contains(net.transitions[transition].inputs, place))
      result.push_back(transition);
  }
  return result;
}

void erase_place(std::vector<std::size_t>& values, std::size_t place) {
  values.erase(std::remove(values.begin(), values.end(), place), values.end());
}

void checked_add(std::int64_t& destination, std::int64_t value) {
  if ((value > 0 &&
       destination > std::numeric_limits<std::int64_t>::max() - value) ||
      (value < 0 &&
       destination < std::numeric_limits<std::int64_t>::min() - value)) {
    throw std::overflow_error("reduced Petri marking exceeds signed range");
  }
  destination += value;
}

void validate_marking(const Marking& marking, std::size_t places,
                      const char* name) {
  if (!marking.empty() && marking.size() != places) {
    throw std::invalid_argument(std::string(name) +
                                " must contain one value per Petri place");
  }
}

PetriNetReductionResult unchanged(const PetriNet& net,
                                  const PetriNetReductionOptions& options) {
  PetriNetReductionResult result;
  result.net = net;
  result.markings.initial = options.initial_marking;
  result.markings.final = options.final_marking;
  result.old_to_new_place.resize(net.place_count());
  for (std::size_t place = 0; place < net.place_count(); ++place)
    result.old_to_new_place[place] = place;
  return result;
}

bool is_protected_place(std::size_t place, const WorkingNet& net,
                        const PetriNetReductionOptions& options) {
  if (place == net.source || place == net.sink) return true;
  if (!options.initial_marking.empty() &&
      options.initial_marking[place] != 0)
    return true;
  if (!options.final_marking.empty() && options.final_marking[place] != 0)
    return true;
  return false;
}

bool reduce_single_entry(WorkingNet& net, std::vector<bool>& place_alive,
                         const PetriNetReductionOptions& options) {
  for (std::size_t silent = 0; silent < net.transitions.size(); ++silent) {
    const auto& transition = net.transitions[silent];
    if (transition.activity.has_value() || transition.inputs.empty() ||
        has_extended_arcs(transition))
      continue;

    const auto entry = transition.inputs.front();
    if (transition.inputs.size() != 1 || !only_place(transition.inputs, entry) ||
        entry >= place_alive.size() || !place_alive[entry] ||
        is_protected_place(entry, net, options) ||
        mentions_extended_place(net, entry))
      continue;

    std::size_t producer = std::numeric_limits<std::size_t>::max();
    std::size_t producer_count = 0;
    std::size_t consumer_count = 0;
    for (std::size_t candidate = 0; candidate < net.transitions.size();
         ++candidate) {
      const auto& other = net.transitions[candidate];
      if (contains(other.outputs, entry)) {
        producer = candidate;
        ++producer_count;
      }
      if (contains(other.inputs, entry)) ++consumer_count;
    }
    if (producer_count != 1 || consumer_count != 1 || producer == silent)
      continue;

    const auto targets = transition.outputs;
    if (std::any_of(targets.begin(), targets.end(), [&](std::size_t place) {
          return place >= place_alive.size() || !place_alive[place] ||
                 place == entry;
        }))
      continue;

    // The series-transition rule is only valid when the producer and the
    // removed silent transition do not both produce the same place.  Their
    // extended post-sets must also agree; with an unguarded private entry
    // this means the replacement targets cannot carry reset/inhibitor guards.
    if (std::any_of(targets.begin(), targets.end(), [&](std::size_t place) {
          return contains(net.transitions[producer].outputs, place) ||
                 !extended_consumers(net, place, true).empty() ||
                 !extended_consumers(net, place, false).empty();
        }))
      continue;

    // The entry place is only consumed by the silent transition under these
    // conditions. Remove it from its producer and splice in the silent
    // transition's former output arcs, preserving every duplicate arc.
    erase_place(net.transitions[producer].outputs, entry);
    for (const auto target : targets)
      net.transitions[producer].outputs.push_back(target);
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(silent));
    place_alive[entry] = false;
    return true;
  }
  return false;
}

// Fusion of Series Transitions (FST): remove a silent transition whose only
// input is a private place produced by one transition.  This is kept as a
// separate pass because the reference rule also compares reset/inhibitor
// post-sets of the private place and of every replacement target.
bool reduce_series_transition(WorkingNet& net,
                              std::vector<bool>& place_alive,
                              const PetriNetReductionOptions& options) {
  for (std::size_t place = 0; place < net.place_count; ++place) {
    if (!place_alive[place] || is_protected_place(place, net, options) ||
        mentions_extended_place(net, place))
      continue;
    const auto producers = ordinary_producers(net, place);
    const auto consumers = ordinary_consumers(net, place);
    if (producers.size() != 1 || consumers.size() != 1 ||
        producers.front() == consumers.front())
      continue;
    const auto producer = producers.front();
    const auto silent = consumers.front();
    const auto& transition = net.transitions[silent];
    if (transition.activity.has_value() || transition.inputs.size() != 1 ||
        transition.inputs.front() != place || has_extended_arcs(transition) ||
        std::count(net.transitions[producer].outputs.begin(),
                   net.transitions[producer].outputs.end(), place) != 1)
      continue;

    const auto targets = transition.outputs;
    if (std::any_of(targets.begin(), targets.end(), [&](std::size_t target) {
          return target >= place_alive.size() || !place_alive[target] ||
                 target == place ||
                 contains(net.transitions[producer].outputs, target);
        }))
      continue;

    std::set<std::size_t> producer_targets;
    for (const auto target : net.transitions[producer].outputs)
      producer_targets.insert(target);
    std::set<std::size_t> silent_targets;
    for (const auto target : targets) silent_targets.insert(target);
    std::set<std::size_t> intersection;
    std::set_intersection(producer_targets.begin(), producer_targets.end(),
                          silent_targets.begin(), silent_targets.end(),
                          std::inserter(intersection, intersection.begin()));
    if (!intersection.empty()) continue;

    const auto reset_on_place = extended_consumers(net, place, true);
    const auto inhibitor_on_place = extended_consumers(net, place, false);
    std::set<std::size_t> reset_on_targets;
    std::set<std::size_t> inhibitor_on_targets;
    for (const auto target : targets) {
      const auto reset = extended_consumers(net, target, true);
      reset_on_targets.insert(reset.begin(), reset.end());
      const auto inhibitor = extended_consumers(net, target, false);
      inhibitor_on_targets.insert(inhibitor.begin(), inhibitor.end());
    }
    if (reset_on_place != reset_on_targets ||
        inhibitor_on_place != inhibitor_on_targets)
      continue;

    erase_place(net.transitions[producer].outputs, place);
    net.transitions[producer].outputs.insert(
        net.transitions[producer].outputs.end(), targets.begin(), targets.end());
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(silent));
    place_alive[place] = false;
    return true;
  }
  return false;
}

bool reduce_single_exit(WorkingNet& net, std::vector<bool>& place_alive,
                        const PetriNetReductionOptions& options) {
  for (std::size_t silent = 0; silent < net.transitions.size(); ++silent) {
    const auto& transition = net.transitions[silent];
    if (transition.activity.has_value() || transition.outputs.empty() ||
        has_extended_arcs(transition))
      continue;

    const auto exit = transition.outputs.front();
    if (!only_place(transition.outputs, exit) ||
        exit >= place_alive.size() || !place_alive[exit] ||
        is_protected_place(exit, net, options) ||
        mentions_extended_place(net, exit))
      continue;

    std::size_t consumer = std::numeric_limits<std::size_t>::max();
    std::size_t producer_count = 0;
    std::size_t consumer_count = 0;
    for (std::size_t candidate = 0; candidate < net.transitions.size();
         ++candidate) {
      const auto& other = net.transitions[candidate];
      if (contains(other.outputs, exit)) ++producer_count;
      if (contains(other.inputs, exit)) {
        consumer = candidate;
        ++consumer_count;
      }
    }
    if (producer_count != 1 || consumer_count != 1 || consumer == silent)
      continue;

    const auto sources = transition.inputs;
    if (std::any_of(sources.begin(), sources.end(), [&](std::size_t place) {
          return place >= place_alive.size() || !place_alive[place] ||
                 place == exit;
        }))
      continue;

    // The exit place is only produced by the silent transition under these
    // conditions. Splice all of the silent transition's former input arcs
    // into its sole consumer without collapsing multiplicity.
    erase_place(net.transitions[consumer].inputs, exit);
    for (const auto source : sources)
      net.transitions[consumer].inputs.push_back(source);
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(silent));
    place_alive[exit] = false;
    return true;
  }
  return false;
}

// Fusion of series places: a silent transition with one input and one output
// can be removed even when several ordinary transitions feed its private
// input.  We deliberately reject any extended arc mentioning the removed
// place, because there is no equivalent place-level rewrite for a reset or
// inhibitor guard in this compact representation.
bool reduce_series_place(WorkingNet& net, std::vector<bool>& place_alive,
                         const PetriNetReductionOptions& options) {
  for (std::size_t silent = 0; silent < net.transitions.size(); ++silent) {
    const auto& transition = net.transitions[silent];
    if (transition.activity.has_value() || has_extended_arcs(transition) ||
        transition.inputs.size() != 1 || transition.outputs.size() != 1)
      continue;
    const auto input = transition.inputs.front();
    const auto output = transition.outputs.front();
    if (input == output || input >= place_alive.size() ||
        output >= place_alive.size() || !place_alive[input] ||
        !place_alive[output] || is_protected_place(input, net, options) ||
        mentions_extended_place(net, input))
      continue;

    const auto consumers = ordinary_consumers(net, input);
    if (consumers.size() != 1 || consumers.front() != silent) continue;
    const auto producers = ordinary_producers(net, input);
    const auto output_producers = ordinary_producers(net, output);
    bool shared_producer = false;
    for (const auto producer : producers) {
      if (std::find(output_producers.begin(), output_producers.end(),
                    producer) != output_producers.end()) {
        shared_producer = true;
        break;
      }
    }
    if (shared_producer) continue;

    // Preserve ordinary arc multiplicity when rewiring every producer.
    for (const auto producer : producers) {
      auto& outputs = net.transitions[producer].outputs;
      const auto count = static_cast<std::size_t>(
          std::count(outputs.begin(), outputs.end(), input));
      erase_place(outputs, input);
      for (std::size_t i = 0; i < count; ++i) outputs.push_back(output);
    }
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(silent));
    place_alive[input] = false;
    return true;
  }
  return false;
}

bool same_arc_multiset(const std::vector<std::size_t>& lhs,
                       const std::vector<std::size_t>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  auto left = lhs;
  auto right = rhs;
  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());
  return left == right;
}

std::size_t arc_multiplicity(const std::vector<std::size_t>& values,
                             std::size_t place) {
  return static_cast<std::size_t>(
      std::count(values.begin(), values.end(), place));
}

// Parallel-place fusion compares the complete place signature. Ordinary arc
// multiplicity is included so a fused representative can safely accumulate
// the removed place's weights; extended arcs are set-valued in the net model.
bool same_place_signature(const WorkingNet& net, std::size_t first,
                          std::size_t second) {
  for (const auto& transition : net.transitions) {
    if (arc_multiplicity(transition.inputs, first) !=
            arc_multiplicity(transition.inputs, second) ||
        arc_multiplicity(transition.outputs, first) !=
            arc_multiplicity(transition.outputs, second) ||
        contains(transition.reset_inputs, first) !=
            contains(transition.reset_inputs, second) ||
        contains(transition.inhibitor_inputs, first) !=
            contains(transition.inhibitor_inputs, second))
      return false;
  }
  return true;
}

bool marking_values_compatible(const Marking& marking, std::size_t first,
                               std::size_t second) {
  return marking.empty() || marking[first] == marking[second];
}

// Fusion of Parallel Places (FPP). Equivalent places are represented by one
// place; normal arcs are redirected with their multiplicity while reset and
// inhibitor arcs are deduplicated by the surviving representative. A whole
// equivalence class is fused in one pass so a three-way parallel bundle does
// not depend on iteration order.
bool reduce_parallel_places(WorkingNet& net, std::vector<bool>& place_alive,
                            std::vector<std::size_t>& place_alias,
                            const PetriNetReductionOptions& options) {
  for (std::size_t first = 0; first < net.place_count; ++first) {
    if (!place_alive[first] || first == net.source || first == net.sink)
      continue;

    std::vector<std::size_t> removals;
    for (std::size_t second = first + 1; second < net.place_count; ++second) {
      if (!place_alive[second] || second == net.source || second == net.sink ||
          !same_place_signature(net, first, second) ||
          !marking_values_compatible(options.initial_marking, first, second) ||
          !marking_values_compatible(options.final_marking, first, second))
        continue;
      removals.push_back(second);
    }
    if (removals.empty()) continue;

    for (const auto removed : removals) {
      for (auto& transition : net.transitions) {
        for (auto& place : transition.inputs)
          if (place == removed) place = first;
        for (auto& place : transition.outputs)
          if (place == removed) place = first;
        erase_place(transition.reset_inputs, removed);
        erase_place(transition.inhibitor_inputs, removed);
      }
      place_alias[removed] = first;
      place_alive[removed] = false;
    }
    return true;
  }
  return false;
}

// Abstraction (A). A silent bridge from a private place may be removed even
// when several transitions produce that place and the bridge fans out to
// several targets. The target places must carry the same extended post-set,
// and a direct producer-to-target arc is rejected to avoid introducing a
// duplicate route.
bool apply_abstraction_rule(WorkingNet& net,
                            std::vector<bool>& place_alive,
                            const PetriNetReductionOptions& options) {
  for (std::size_t private_place = 0; private_place < net.place_count;
       ++private_place) {
    if (!place_alive[private_place] || private_place == net.source ||
        private_place == net.sink ||
        is_protected_place(private_place, net, options))
      continue;

    const auto consumers = ordinary_consumers(net, private_place);
    if (consumers.size() != 1) continue;
    const auto silent = consumers.front();
    const auto& bridge = net.transitions[silent];
    if (bridge.activity.has_value() || has_extended_arcs(bridge) ||
        bridge.inputs.size() != 1 || bridge.inputs.front() != private_place ||
        bridge.outputs.empty())
      continue;

    std::vector<std::size_t> targets = bridge.outputs;
    std::sort(targets.begin(), targets.end());
    if (std::adjacent_find(targets.begin(), targets.end()) != targets.end())
      continue;
    if (std::any_of(targets.begin(), targets.end(), [&](std::size_t target) {
          return target >= place_alive.size() || !place_alive[target] ||
                 target == private_place;
        }))
      continue;

    const auto producers = ordinary_producers(net, private_place);
    if (producers.empty()) continue;
    std::set<std::size_t> unique_producers(producers.begin(), producers.end());
    if (unique_producers.size() != producers.size()) continue;
    if (std::any_of(producers.begin(), producers.end(), [&](std::size_t producer) {
          return arc_multiplicity(net.transitions[producer].outputs,
                                  private_place) != 1;
        }))
      continue;

    const auto reset_post = extended_consumers(net, private_place, true);
    const auto inhibitor_post = extended_consumers(net, private_place, false);
    bool valid_targets = true;
    for (const auto target : targets) {
      if (extended_consumers(net, target, true) != reset_post ||
          extended_consumers(net, target, false) != inhibitor_post)
        valid_targets = false;
    }
    if (!valid_targets) continue;

    bool direct_route = false;
    for (const auto producer : producers) {
      for (const auto target : targets) {
        if (contains(net.transitions[producer].outputs, target))
          direct_route = true;
      }
    }
    if (direct_route) continue;

    for (const auto producer : producers) {
      auto& outputs = net.transitions[producer].outputs;
      erase_place(outputs, private_place);
      outputs.insert(outputs.end(), targets.begin(), targets.end());
    }
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(silent));
    place_alive[private_place] = false;
    return true;
  }
  return false;
}

// Fusion of parallel transitions is restricted to silent transitions with
// exactly equal ordinary and extended arc multisets.  Removing one duplicate
// cannot change enabledness or marking effects.
bool reduce_parallel_transitions(WorkingNet& net) {
  for (std::size_t first = 0; first < net.transitions.size(); ++first) {
    const auto& lhs = net.transitions[first];
    if (lhs.activity.has_value()) continue;
    for (std::size_t second = first + 1; second < net.transitions.size();
         ++second) {
      const auto& rhs = net.transitions[second];
      if (rhs.activity.has_value() ||
          !same_arc_multiset(lhs.inputs, rhs.inputs) ||
          !same_arc_multiset(lhs.outputs, rhs.outputs) ||
          !same_arc_multiset(lhs.reset_inputs, rhs.reset_inputs) ||
          !same_arc_multiset(lhs.inhibitor_inputs, rhs.inhibitor_inputs))
        continue;
      net.transitions.erase(net.transitions.begin() +
                            static_cast<std::ptrdiff_t>(second));
      return true;
    }
  }
  return false;
}

// A silent ordinary self-loop has no observable effect when the place has a
// different ordinary producer.  The extended-arc check ensures that removing
// it cannot discard a reset/inhibitor side effect.
bool eliminate_self_loop(WorkingNet& net) {
  for (std::size_t transition = 0; transition < net.transitions.size();
       ++transition) {
    const auto& candidate = net.transitions[transition];
    if (candidate.activity.has_value() || has_extended_arcs(candidate) ||
        candidate.inputs.size() != 1 || candidate.outputs.size() != 1 ||
        candidate.inputs.front() != candidate.outputs.front())
      continue;
    const auto place = candidate.inputs.front();
    if (ordinary_producers(net, place).size() < 2) continue;
    net.transitions.erase(net.transitions.begin() +
                          static_cast<std::ptrdiff_t>(transition));
    return true;
  }
  return false;
}

// A reset and an inhibitor arc on the same transition/place are redundant:
// the inhibitor already requires an empty place, so the reset has no effect.
bool remove_redundant_resets(WorkingNet& net) {
  for (auto& transition : net.transitions) {
    const auto before = transition.reset_inputs.size();
    transition.reset_inputs.erase(
        std::remove_if(transition.reset_inputs.begin(),
                       transition.reset_inputs.end(), [&](std::size_t place) {
                         return contains(transition.inhibitor_inputs, place);
                       }),
        transition.reset_inputs.end());
    if (transition.reset_inputs.size() != before) return true;
  }
  return false;
}

// Elimination of Self-Loop Places (ELP): a positively marked place that is
// preserved by every ordinary producer/consumer cycle can be removed because
// it contributes no changing state.  Empty markings do not authorize this
// rule; the supplied initial marking is the proof that the invariant token is
// present.
bool eliminate_self_loop_place(WorkingNet& net,
                               std::vector<bool>& place_alive,
                               const PetriNetReductionOptions& options) {
  if (options.initial_marking.empty()) return false;
  for (std::size_t place = 0; place < net.place_count; ++place) {
    if (!place_alive[place] || place == net.source || place == net.sink ||
        options.initial_marking[place] < 1)
      continue;

    const auto inhibitor = extended_consumers(net, place, false);
    if (!inhibitor.empty()) continue;
    const auto producers = ordinary_producers(net, place);
    std::set<std::size_t> incoming(producers.begin(), producers.end());
    std::set<std::size_t> outgoing;
    for (const auto transition : ordinary_consumers(net, place))
      outgoing.insert(transition);
    const auto reset = extended_consumers(net, place, true);

    std::set<std::size_t> outgoing_all = outgoing;
    outgoing_all.insert(reset.begin(), reset.end());
    if (!std::includes(incoming.begin(), incoming.end(), outgoing_all.begin(),
                       outgoing_all.end()))
      continue;

    std::set<std::size_t> covered = outgoing;
    covered.insert(reset.begin(), reset.end());
    if (covered != incoming) continue;

    for (auto& transition : net.transitions) {
      erase_place(transition.inputs, place);
      erase_place(transition.outputs, place);
      erase_place(transition.reset_inputs, place);
      erase_place(transition.inhibitor_inputs, place);
    }
    place_alive[place] = false;
    return true;
  }
  return false;
}

}  // namespace

PetriNetReductionResult reduce_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options) {
  validate_marking(options.initial_marking, net.place_count(),
                   "initial marking");
  validate_marking(options.final_marking, net.place_count(),
                   "final marking");

  if (net.source() >= net.place_count() || net.sink() >= net.place_count())
    return unchanged(net, options);

  // add_transition intentionally stays lightweight and does not validate its
  // place vectors. A malformed net cannot be safely reduced, so preserve it
  // rather than indexing outside the working place table.
  for (const auto& transition : net.transitions()) {
    if (std::any_of(transition.inputs.begin(), transition.inputs.end(),
                    [&](std::size_t place) {
                      return place >= net.place_count();
                    }) ||
        std::any_of(transition.outputs.begin(), transition.outputs.end(),
                    [&](std::size_t place) {
                      return place >= net.place_count();
                    }) ||
        std::any_of(transition.reset_inputs.begin(),
                    transition.reset_inputs.end(), [&](std::size_t place) {
                      return place >= net.place_count();
                    }) ||
        std::any_of(transition.inhibitor_inputs.begin(),
                    transition.inhibitor_inputs.end(), [&](std::size_t place) {
                      return place >= net.place_count();
                    }))
      return unchanged(net, options);
  }

  WorkingNet working;
  working.place_count = net.place_count();
  working.source = net.source();
  working.sink = net.sink();
  working.activities = net.activities();
  working.transitions.reserve(net.transitions().size());
  for (const auto& transition : net.transitions()) {
    working.transitions.push_back(
        {transition.activity, transition.inputs, transition.outputs,
         transition.reset_inputs, transition.inhibitor_inputs});
  }

  std::vector<bool> place_alive(working.place_count, true);
  std::vector<std::size_t> place_alias(working.place_count, 0);
  for (std::size_t place = 0; place < working.place_count; ++place)
    place_alias[place] = place;
  bool changed = false;
  for (;;) {
    bool pass_changed = false;
    while (remove_redundant_resets(working)) {
      changed = true;
      pass_changed = true;
    }
    while (reduce_series_transition(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    while (reduce_single_entry(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    while (reduce_single_exit(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    while (reduce_series_place(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    if (options.enable_parallel_place_fusion) {
      while (reduce_parallel_places(working, place_alive, place_alias,
                                    options)) {
        changed = true;
        pass_changed = true;
      }
    }
    if (options.enable_abstraction) {
      while (apply_abstraction_rule(working, place_alive, options)) {
        changed = true;
        pass_changed = true;
      }
    }
    while (reduce_parallel_transitions(working)) {
      changed = true;
      pass_changed = true;
    }
    while (eliminate_self_loop(working)) {
      changed = true;
      pass_changed = true;
    }
    while (eliminate_self_loop_place(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    if (!pass_changed) break;
  }

  PetriNetReductionResult result;
  result.changed = changed;
  result.old_to_new_place.assign(working.place_count, kRemovedPlace);
  std::size_t reduced_places = 0;
  for (std::size_t place = 0; place < working.place_count; ++place) {
    if (place_alive[place]) result.old_to_new_place[place] = reduced_places++;
  }
  for (std::size_t place = 0; place < working.place_count; ++place) {
    if (result.old_to_new_place[place] != kRemovedPlace) continue;
    auto representative = place_alias[place];
    while (representative < place_alias.size() &&
           place_alias[representative] != representative)
      representative = place_alias[representative];
    if (representative < result.old_to_new_place.size() &&
        result.old_to_new_place[representative] != kRemovedPlace)
      result.old_to_new_place[place] = result.old_to_new_place[representative];
  }

  PetriNet reduced;
  reduced.set_activities(working.activities);
  for (std::size_t place = 0; place < reduced_places; ++place)
    (void)reduced.add_place();
  reduced.set_source(result.old_to_new_place[working.source]);
  reduced.set_sink(result.old_to_new_place[working.sink]);
  for (const auto& transition : working.transitions) {
    std::vector<std::size_t> inputs;
    inputs.reserve(transition.inputs.size());
    for (const auto place : transition.inputs) {
      if (place < result.old_to_new_place.size() &&
          result.old_to_new_place[place] != kRemovedPlace)
        inputs.push_back(result.old_to_new_place[place]);
    }
    std::vector<std::size_t> outputs;
    outputs.reserve(transition.outputs.size());
    for (const auto place : transition.outputs) {
      if (place < result.old_to_new_place.size() &&
          result.old_to_new_place[place] != kRemovedPlace)
        outputs.push_back(result.old_to_new_place[place]);
    }
    const auto index = reduced.add_transition(
        transition.activity, std::move(inputs), std::move(outputs));
    for (const auto place : transition.reset_inputs) {
      if (place < result.old_to_new_place.size() &&
          result.old_to_new_place[place] != kRemovedPlace)
        reduced.add_reset_arc(index, result.old_to_new_place[place]);
    }
    for (const auto place : transition.inhibitor_inputs) {
      if (place < result.old_to_new_place.size() &&
          result.old_to_new_place[place] != kRemovedPlace)
        reduced.add_inhibitor_arc(index, result.old_to_new_place[place]);
    }
  }
  result.net = std::move(reduced);

  const auto remap_marking = [&](const Marking& original) {
    if (original.empty()) return Marking{};
    Marking remapped(reduced_places, 0);
    for (std::size_t place = 0; place < original.size(); ++place) {
      const auto mapped = result.old_to_new_place[place];
      if (mapped != kRemovedPlace) checked_add(remapped[mapped], original[place]);
    }
    return remapped;
  };
  result.markings.initial = remap_marking(options.initial_marking);
  result.markings.final = remap_marking(options.final_marking);
  return result;
}

}  // namespace minera
