#include "minera/petri_net_reduction.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
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
    if (transition.activity.has_value() || transition.inputs.empty()) continue;

    const auto entry = transition.inputs.front();
    if (!only_place(transition.inputs, entry) ||
        entry >= place_alive.size() || !place_alive[entry] ||
        is_protected_place(entry, net, options))
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

bool reduce_single_exit(WorkingNet& net, std::vector<bool>& place_alive,
                        const PetriNetReductionOptions& options) {
  for (std::size_t silent = 0; silent < net.transitions.size(); ++silent) {
    const auto& transition = net.transitions[silent];
    if (transition.activity.has_value() || transition.outputs.empty()) continue;

    const auto exit = transition.outputs.front();
    if (!only_place(transition.outputs, exit) ||
        exit >= place_alive.size() || !place_alive[exit] ||
        is_protected_place(exit, net, options))
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

}  // namespace

PetriNetReductionResult reduce_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options) {
  validate_marking(options.initial_marking, net.place_count(),
                   "initial marking");
  validate_marking(options.final_marking, net.place_count(),
                   "final marking");

  // A rebuild that ignores extended arcs would change token semantics. Keep
  // these nets intact until matching reduction rules are implemented.
  if (net.has_extended_arcs() || net.source() >= net.place_count() ||
      net.sink() >= net.place_count())
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
        {transition.activity, transition.inputs, transition.outputs});
  }

  std::vector<bool> place_alive(working.place_count, true);
  bool changed = false;
  for (;;) {
    bool pass_changed = false;
    while (reduce_single_entry(working, place_alive, options)) {
      changed = true;
      pass_changed = true;
    }
    while (reduce_single_exit(working, place_alive, options)) {
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
    (void)reduced.add_transition(transition.activity, std::move(inputs),
                                 std::move(outputs));
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
