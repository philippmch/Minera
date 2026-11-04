#include "minera/petri_usage_comparison.hpp"
#include "minera/petri_reference_arcs.hpp"
#include "petri_replay_internal.hpp"

#include <map>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>

namespace minera {
namespace {

using Marking = std::vector<std::uint64_t>;
using MarkingKeys = std::vector<bool>;
constexpr std::size_t kSilentSearchCap = 4096;
using detail::CleaningComponents;
using detail::cleaning_components;
using detail::silent_place_paths;

struct Rgb {
  double red;
  double green;
  double blue;
};

std::string color_component(double value) {
  const auto channel = static_cast<int>(
      std::ceil(std::clamp(value, 0.0, 1.0) * 255.0));
  constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  result.push_back(digits[(channel >> 4) & 0xF]);
  result.push_back(digits[channel & 0xF]);
  return result;
}

std::string direction_color(double direction) {
  constexpr std::array<double, 9> stops = {
      0.0, 0.01, 0.25, 0.4, 0.45, 0.55, 0.75, 0.99, 1.0};
  constexpr std::array<Rgb, 9> colors = {
      Rgb{0.0, 191.0 / 255.0, 1.0},
      Rgb{135.0 / 255.0, 206.0 / 255.0, 235.0 / 255.0},
      Rgb{224.0 / 255.0, 1.0, 1.0},
      Rgb{211.0 / 255.0, 211.0 / 255.0, 211.0 / 255.0},
      Rgb{128.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0},
      Rgb{211.0 / 255.0, 211.0 / 255.0, 211.0 / 255.0},
      Rgb{1.0, 228.0 / 255.0, 225.0 / 255.0},
      Rgb{250.0 / 255.0, 128.0 / 255.0, 114.0 / 255.0},
      Rgb{1.0, 99.0 / 255.0, 71.0 / 255.0}};
  const double normalized = std::clamp(0.5 + 0.5 * direction, 0.0, 1.0);
  // The public palette samples a 256-entry lookup table, including both ends.
  const auto bin = std::min(255, static_cast<int>(std::floor(normalized * 256.0)));
  const double position = static_cast<double>(bin) / 255.0;
  std::size_t segment = 0;
  while (segment + 1 < stops.size() && position > stops[segment + 1])
    ++segment;
  if (segment + 1 == stops.size()) segment = stops.size() - 2;
  const double width = stops[segment + 1] - stops[segment];
  const double ratio = width > 0.0 ? (position - stops[segment]) / width : 0.0;
  const Rgb rgb{
      colors[segment].red + ratio *
          (colors[segment + 1].red - colors[segment].red),
      colors[segment].green + ratio *
          (colors[segment + 1].green - colors[segment].green),
      colors[segment].blue + ratio *
          (colors[segment + 1].blue - colors[segment].blue)};
  return "#" + color_component(rgb.red) + color_component(rgb.green) +
         color_component(rgb.blue);
}

std::string occurrence_label(std::uint64_t log1, std::uint64_t log2,
                             std::uint64_t total) {
  return "(" + std::to_string(log1) + "/" + std::to_string(log2) + "/" +
         std::to_string(total) + ")";
}

// Resolve the activity label selected by the caller.  The event model keeps
// the common resource, timestamp, and lifecycle values in dedicated fields;
// aliases for those fields therefore need to be handled before falling back
// to the generic attribute bag.
struct SelectedActivity {
  const std::string* value = nullptr;
  bool present = false;
};

SelectedActivity selected_activity_value(const Event& event, std::string_view attribute) {
  const auto projected = [](const std::string& value, FirstClassAttributeState state) {
    if (state == FirstClassAttributeState::kNull) return SelectedActivity{nullptr, true};
    if (state == FirstClassAttributeState::kMissing && value.empty()) return SelectedActivity{};
    return SelectedActivity{&value, true};
  };
  if (attribute.empty() || attribute == "activity" || attribute == "concept:name")
    return projected(event.activity, event.activity_state);
  // The first matching present projection wins for both values and nulls.
  // This is the same priority as the native first-class alias resolution.
  for (const auto& [source, value, state] : {
      std::tuple{std::string_view(event.activity_source_key), &event.activity, event.activity_state},
      std::tuple{std::string_view(event.resource_source_key), &event.resource, event.resource_state},
      std::tuple{std::string_view(event.timestamp_source_key), &event.timestamp, event.timestamp_state},
      std::tuple{std::string_view(event.lifecycle_source_key), &event.lifecycle, event.lifecycle_state},
      std::tuple{std::string_view(event.case_id_source_key), &event.case_id, event.case_id_state}}) {
    if (source != attribute) continue;
    const auto result = projected(*value, state);
    if (result.present) return result;
  }
  if (attribute == "resource" || attribute == "org:resource") return projected(event.resource, event.resource_state);
  if (attribute == "timestamp" || attribute == "time:timestamp") return projected(event.timestamp, event.timestamp_state);
  if (attribute == "lifecycle" || attribute == "lifecycle:transition") return projected(event.lifecycle, event.lifecycle_state);
  for (const auto& field : event.attributes)
    if (field.key == attribute) return {field.is_null ? nullptr : &field.value, true};
  return {};
}

std::string normalize_activity_attribute(std::string_view attribute) {
  const auto first = attribute.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    if (attribute.empty()) return {};
    throw std::invalid_argument(
        "Petri usage activity attribute must not be blank");
  }
  const auto last = attribute.find_last_not_of(" \t\r\n");
  return std::string(attribute.substr(first, last - first + 1));
}

struct MarkingHash {
  std::size_t operator()(const Marking& marking) const noexcept {
    std::size_t hash = 1469598103934665603ull;
    for (const auto token : marking) {
      hash ^= static_cast<std::size_t>(token) ^
              static_cast<std::size_t>(token >> 32);
      hash *= 1099511628211ull;
    }
    return hash;
  }
};

void checked_add(std::uint64_t& destination, std::uint64_t amount = 1) {
  if (amount > std::numeric_limits<std::uint64_t>::max() - destination) {
    throw std::overflow_error("Petri usage count overflow");
  }
  destination += amount;
}

void checked_token_add(std::uint64_t& destination) {
  checked_add(destination);
}

void validate_net(const PetriNet& net) {
  if (net.place_count() != 0 && (net.source() >= net.place_count() ||
      net.sink() >= net.place_count())) {
    throw std::invalid_argument("Petri net source/sink place index is invalid");
  }
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    if (transition.activity && *transition.activity >= net.activities().size()) {
      throw std::invalid_argument("Petri transition activity index is invalid");
    }
    for (const auto place : transition.inputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument("Petri transition input place index is invalid");
      }
    }
    for (const auto place : transition.outputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument("Petri transition output place index is invalid");
      }
    }
    for (const auto place : transition.reset_inputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument("Petri transition reset place index is invalid");
      }
    }
    for (const auto place : transition.inhibitor_inputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument(
            "Petri transition inhibitor place index is invalid");
      }
    }
  }
}

struct ReplayUsage {
  std::vector<std::uint64_t> transitions;
  std::vector<std::uint64_t> places;
};

bool enabled(const Marking& marking, const PetriTransition& transition) {
  std::vector<std::uint64_t> required(marking.size(), 0);
  for (const auto place : transition.inputs) checked_token_add(required[place]);
  for (std::size_t p = 0; p < marking.size(); ++p) {
    if (marking[p] < required[p]) return false;
  }
  for (const auto place : transition.inhibitor_inputs) {
    if (marking[place] != 0) return false;
  }
  return true;
}

void apply_marking(Marking& marking, const PetriTransition& transition) {
  for (const auto place : transition.inputs) {
    // A forced visible replay can fire without enough tokens; retain the
    // existing deviation-tolerant behavior instead of wrapping unsigned state.
    if (marking[place] != 0) --marking[place];
  }
  for (const auto place : transition.reset_inputs) marking[place] = 0;
  for (const auto place : transition.outputs) checked_token_add(marking[place]);
}

void fire(Marking& marking, MarkingKeys& keys, std::size_t transition_id, const PetriNet& net,
          ReplayUsage& usage, std::vector<std::size_t>& activated) {
  const auto& transition = net.transitions()[transition_id];
  checked_add(usage.transitions[transition_id]);
  activated.push_back(transition_id);
  for (const auto place : transition.inputs) {
    checked_add(usage.places[place]);
  }
  for (const auto place : transition.reset_inputs) {
    checked_add(usage.places[place], marking[place]);
  }
  apply_marking(marking, transition);
  // Consumed entries disappear at zero; untouched explicit zero entries stay.
  for (const auto place : transition.inputs) keys[place] = marking[place] != 0;
  for (const auto place : transition.reset_inputs) keys[place] = marking[place] != 0;
  for (const auto place : transition.outputs) keys[place] = marking[place] != 0;
}

Marking resolve_marking(const std::vector<std::uint32_t>& configured,
                        std::size_t place_count, std::size_t fallback,
                        std::string_view name) {
  if (configured.empty()) {
    Marking result(place_count, 0);
    if (place_count != 0) result[fallback] = 1;
    return result;
  }
  if (configured.size() != place_count) {
    throw std::invalid_argument(std::string(name) +
                                " must contain one entry per place");
  }
  return Marking(configured.begin(), configured.end());
}

MarkingKeys resolve_marking_keys(const MarkingKeys& configured,
                                const Marking& marking, std::string_view name) {
  if (!configured.empty() && configured.size() != marking.size())
    throw std::invalid_argument(std::string(name) + " must contain one entry per place");
  MarkingKeys result = configured.empty() ? MarkingKeys(marking.size(), false) : configured;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] == 0) continue;
    if (!configured.empty() && !configured[place])
      throw std::invalid_argument(std::string(name) + " must include every positive-token place");
    result[place] = true;
  }
  return result;
}

// Finds a shortest deterministic silent path to a marking enabling one target.
std::vector<std::size_t> silent_path(const Marking& start,
                                     const std::vector<std::size_t>& targets,
                                     const PetriNet& net) {
  const auto target_enabled = [&](const Marking& marking) {
    return std::any_of(targets.begin(), targets.end(), [&](const auto transition) {
      return enabled(marking, net.transitions()[transition]);
    });
  };
  if (target_enabled(start)) return {};

  struct Node {
    Marking marking;
    std::size_t transition = std::numeric_limits<std::size_t>::max();
    std::size_t parent = std::numeric_limits<std::size_t>::max();
  };
  std::vector<Node> arena;
  std::unordered_set<Marking, MarkingHash> seen{start};
  std::queue<std::size_t> frontier;
  frontier.push(std::numeric_limits<std::size_t>::max());
  std::size_t generated = 1;
  std::size_t expanded = 0;
  while (!frontier.empty() && expanded++ < kSilentSearchCap) {
    const auto parent = frontier.front();
    frontier.pop();
    const Marking current = parent == std::numeric_limits<std::size_t>::max()
                                ? start
                                : arena[parent].marking;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      if (!net.transitions()[t].silent() ||
          !enabled(current, net.transitions()[t])) {
        continue;
      }
      Marking next = current;
      // Silent search only visits enabled firings, so this local marking move
      // cannot incur missing tokens or alter usage counters.
      apply_marking(next, net.transitions()[t]);
      if (!seen.insert(next).second) continue;
      if (generated++ >= kSilentSearchCap) {
        throw std::runtime_error(
            "Petri usage replay exceeded the silent-routing state limit");
      }
      arena.push_back(Node{std::move(next), t, parent});
      const auto found = arena.size() - 1;
      if (target_enabled(arena[found].marking)) {
        std::vector<std::size_t> path;
        for (auto i = found; i != std::numeric_limits<std::size_t>::max();
             i = arena[i].parent) {
          path.push_back(arena[i].transition);
        }
        std::reverse(path.begin(), path.end());
        return path;
      }
      frontier.push(found);
    }
  }
  if (!frontier.empty()) {
    throw std::runtime_error(
        "Petri usage replay exceeded the silent-routing state limit");
  }
  return {};
}

void route_hidden_before_event(Marking& marking, MarkingKeys& keys, std::size_t target,
                               const PetriNet& net, ReplayUsage& usage,
                               std::vector<std::size_t>& activated,
                               const std::vector<std::string>& place_names,
                               bool exhaustive) {
  const auto required = [&](std::size_t selected) {
    Marking needed(marking.size(), 0);
    for (const auto place : net.transitions()[selected].inputs) checked_token_add(needed[place]);
    for (std::size_t place = 0; place < needed.size(); ++place)
      if (marking[place] >= needed[place]) needed[place] = 0;
    return resolve_marking_keys({}, needed, "required");
  };
  const auto candidates = [&](std::size_t /*selected*/, const MarkingKeys& required_keys) {
    return silent_place_paths(keys, required_keys, net, place_names, 51);
  };
  const auto trial = [&](std::size_t selected,
                         const std::vector<detail::HiddenPlacePath>& paths,
                         const std::vector<bool>& visited) {
    const auto fire_trial = [&](Marking& trial_marking, MarkingKeys& trial_keys,
                                std::size_t transition) {
      ReplayUsage scratch{std::vector<std::uint64_t>(net.transitions().size(), 0),
                          std::vector<std::uint64_t>(net.place_count(), 0)};
      std::vector<std::size_t> ignored;
      fire(trial_marking, trial_keys, transition, net, scratch, ignored);
    };
    return detail::exhaustive_hidden_transition_trial(
        selected, paths, visited, marking, keys,
        [&](const Marking& candidate, std::size_t transition) {
          return enabled(candidate, net.transitions()[transition]);
        },
        fire_trial);
  };
  detail::route_hidden_dependencies(net.transitions().size(), target, exhaustive,
      candidates, required,
      [&](auto transition) { return enabled(marking, net.transitions()[transition]); },
      [&](auto transition) { fire(marking, keys, transition, net, usage, activated); }, trial);
}

ReplayUsage replay_log(const EventLog& log, const PetriNet& net,
                       const std::unordered_map<std::string, std::size_t>& activities,
                       const Marking& initial_marking,
                       const Marking& final_marking,
                       const MarkingKeys& initial_keys,
                       const MarkingKeys& final_keys,
                       std::string_view activity_attribute,
                       bool walk_through_hidden_trans,
                       bool try_to_reach_final_marking_through_hidden,
                       bool stop_immediately_unfit,
                       bool exhaustive_invisible_exploration,
                       const CleaningComponents& components,
                       const std::vector<std::string>& place_names,
                       const std::vector<std::string>& transition_names) {
  ReplayUsage usage{std::vector<std::uint64_t>(net.transitions().size(), 0),
                    std::vector<std::uint64_t>(net.place_count(), 0)};
  std::vector<std::vector<std::size_t>> transitions_by_activity(net.activities().size());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    if (net.transitions()[t].activity) {
      transitions_by_activity[*net.transitions()[t].activity].push_back(t);
    }
  }
  for (auto& transitions : transitions_by_activity) {
    std::sort(transitions.begin(), transitions.end(), [&](const auto left, const auto right) {
      return std::tie(transition_names[left], left) < std::tie(transition_names[right], right);
    });
  }
  // Public replay validates the activity sequence before processing it, so
  // immediate-stop cannot hide a missing field later in the log.
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (!selected_activity_value(event, activity_attribute).present)
        throw std::invalid_argument("Petri usage event is missing activity attribute: " +
                                    std::string(activity_attribute.empty() ? "concept:name" : activity_attribute));
    }
  }

  for (const auto& trace : log.traces) {
    Marking marking = initial_marking;
    MarkingKeys keys = initial_keys;
    std::vector<std::size_t> activated;
    for (const auto& event : trace.events) {
      const auto selected = selected_activity_value(event, activity_attribute);
      if (!selected.value) continue;
      const auto activity = activities.find(*selected.value);
      if (activity == activities.end() || transitions_by_activity[activity->second].empty()) {
        // Unknown labels do not activate a transition or trigger the
        // disabled-transition immediate-stop control.
        continue;
      }
      const auto& targets = transitions_by_activity[activity->second];
      std::size_t chosen = targets.back();
      for (const auto transition : targets) {
        if (enabled(marking, net.transitions()[transition])) {
          chosen = transition;
          break;
        }
      }
      if (!enabled(marking, net.transitions()[chosen]) && walk_through_hidden_trans) {
        if (!net.has_extended_arcs()) {
          route_hidden_before_event(marking, keys, chosen, net, usage, activated,
                                    place_names, exhaustive_invisible_exploration);
        } else {
          const auto path = silent_path(marking, {chosen}, net);
          for (const auto t : path) fire(marking, keys, t, net, usage, activated);
        }
      }
      // Keep the selected duplicate-label transition even when routing makes
      // another candidate enabled along the way.
      const bool forced = !enabled(marking, net.transitions()[chosen]);
      if (forced && stop_immediately_unfit) break;
      const MarkingKeys before_cleaning = forced && !components.empty()
          ? keys : MarkingKeys{};
      if (forced) {
        std::vector<std::uint64_t> required(marking.size(), 0);
        for (const auto place : net.transitions()[chosen].inputs) checked_token_add(required[place]);
        for (std::size_t place = 0; place < marking.size(); ++place)
          if (marking[place] < required[place]) checked_add(marking[place], required[place]);
      }
      fire(marking, keys, chosen, net, usage, activated);
      if (!before_cleaning.empty()) {
        // A forced activation can leave an old place marked alongside a
        // newly marked place in the same structural component. Remove the
        // stale marking, recording one remaining occurrence for that place.
        // Names determine introduction: a newly marked alias of an existing
        // name does not start cleaning, even when its identity is new.
        const auto introduced_places = detail::cleaning_introduced_places(keys, before_cleaning, place_names);
        // Multiple currently marked identities with the same name survive,
        // including zero keys and aliases produced by the current firing.
        const auto unique_names = detail::cleaning_unique_place_names(keys, place_names);
        for (const auto& component : components) {
          const bool introduced_place = std::any_of(component.begin(), component.end(),
              [&](const auto place) { return keys[place] && introduced_places[place]; });
          if (!introduced_place) continue;
          for (const auto place : component) {
            if (keys[place] && before_cleaning[place] && unique_names[place]) {
              marking[place] = 0;
              keys[place] = false;
              checked_add(usage.places[place]);
            }
          }
        }
      }
    }

    const auto is_final_marking = [&]() {
      for (std::size_t place = 0; place < keys.size(); ++place)
        if (final_keys[place] && !keys[place]) return false;
      return true;
    };
    if (try_to_reach_final_marking_through_hidden &&
        !is_final_marking()) {
      // Public staged-join probes retain prefixes over at most six passes.
      // This is independent of the number of transitions in a single path.
      for (std::size_t pass = 0; pass < 6 && !is_final_marking(); ++pass) {
        const auto paths = silent_place_paths(keys, final_keys, net, place_names);
        bool fired = false;
        for (const auto& path : paths) {
          for (const auto t : path.transitions) {
            if (!enabled(marking, net.transitions()[t])) break;
            fire(marking, keys, t, net, usage, activated);
            fired = true;
          }
          if (is_final_marking()) break;
        }
        if (!fired) break;
      }
    }
    // Final marking demand contributes even when replay could not reach it;
    // surplus residual tokens remain part of the place usage.
    for (std::size_t p = 0; p < marking.size(); ++p) {
      checked_add(usage.places[p], std::max(marking[p], final_marking[p]));
    }
  }
  return usage;
}

PetriUsageRecord record(std::size_t id, std::uint64_t log1, std::uint64_t log2,
                        std::string label_prefix = {}) {
  PetriUsageRecord result;
  result.id = id;
  result.log1_occurrences = log1;
  result.log2_occurrences = log2;
  if (log2 > std::numeric_limits<std::uint64_t>::max() - log1) {
    throw std::overflow_error("Petri usage total overflow");
  }
  result.total_occurrences = log1 + log2;
  if (result.total_occurrences != 0) {
    result.direction = static_cast<double>(
        (static_cast<long double>(log2) - static_cast<long double>(log1)) /
        static_cast<long double>(result.total_occurrences));
  }
  if (!std::isfinite(result.direction)) throw std::overflow_error("Petri usage direction is not finite");
  result.label = std::move(label_prefix) +
                 occurrence_label(result.log1_occurrences,
                                   result.log2_occurrences,
                                   result.total_occurrences);
  result.color = petri_usage_direction_color(result.direction);
  return result;
}

}  // namespace

std::string petri_usage_direction_color_static(double direction) {
  if (!std::isfinite(direction) || direction < -1.0 || direction > 1.0)
    throw std::invalid_argument("Petri usage direction must be finite and within [-1, 1]");
  if (direction <= -0.5) return "#4444FF";
  if (direction <= -0.1) return "#AAAAFF";
  if (direction <= 0.0) return "#CCCCCC";
  if (direction <= 0.5) return "#FFAAAA";
  return "#FF4444";
}

std::string petri_usage_direction_color(double direction) {
  if (!std::isfinite(direction) || direction < -1.0 || direction > 1.0)
    throw std::invalid_argument("Petri usage direction must be finite and within [-1, 1]");
  return direction_color(direction);
}

PetriUsageComparison compare_petri_usage(const PetriNet& net, const EventLog& log1,
                                         const EventLog& log2,
                                         const PetriUsageComparisonOptions& options) {
  validate_net(net);
  // This replay moves one token per arc endpoint. A net with weighted or
  // parallel arcs goes through the token-replay composition instead, which
  // moves each arc's weight as the reference does.
  if (petri_net_has_weighted_arcs(net))
    return compare_petri_usage_reference(net, log1, log2, options);
  const auto activity_attribute =
      normalize_activity_attribute(options.activity_attribute);
  const auto initial_marking = resolve_marking(
      options.initial_marking, net.place_count(), net.source(), "initial_marking");
  const auto final_marking = resolve_marking(
      options.final_marking, net.place_count(), net.sink(), "final_marking");
  const auto initial_keys = resolve_marking_keys(options.initial_marking_keys, initial_marking, "initial_marking_keys");
  const auto final_keys = resolve_marking_keys(options.final_marking_keys, final_marking, "final_marking_keys");
  auto place_names = options.place_names;
  if (place_names.empty()) {
    place_names = detail::replay_place_names({}, net);
  } else if (place_names.size() != net.place_count()) {
    throw std::invalid_argument("place_names must contain one entry per place");
  }
  auto transition_names = options.transition_names;
  if (transition_names.empty()) {
    for (std::size_t transition = 0; transition < net.transitions().size(); ++transition)
      transition_names.push_back("t" + std::to_string(transition));
  } else if (transition_names.size() != net.transitions().size()) {
    throw std::invalid_argument("transition_names must contain one entry per transition");
  }
  const auto components = options.cleaning_token_flood
      ? cleaning_components(net, initial_keys, final_keys, &place_names) : CleaningComponents{};
  std::unordered_map<std::string, std::size_t> activities;
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    activities.emplace(net.activities()[i], i);
  }
  const auto usage1 = replay_log(log1, net, activities, initial_marking,
                                 final_marking, initial_keys, final_keys, activity_attribute,
                                 options.walk_through_hidden_trans,
                                 options.try_to_reach_final_marking_through_hidden,
                                 options.stop_immediately_unfit, options.exhaustive_invisible_exploration,
                                 components, place_names, transition_names);
  const auto usage2 = replay_log(log2, net, activities, initial_marking,
                                 final_marking, initial_keys, final_keys, activity_attribute,
                                 options.walk_through_hidden_trans,
                                 options.try_to_reach_final_marking_through_hidden,
                                 options.stop_immediately_unfit, options.exhaustive_invisible_exploration,
                                 components, place_names, transition_names);
  PetriUsageComparison result;
  result.places.reserve(net.place_count());
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    result.places.push_back(record(p, usage1.places[p], usage2.places[p]));
  }
  result.transitions.reserve(net.transitions().size());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    std::string label_prefix;
    const auto& transition = net.transitions()[t];
    if (transition.activity && *transition.activity < net.activities().size())
      label_prefix = net.activities()[*transition.activity] + " ";
    result.transitions.push_back(record(t, usage1.transitions[t],
                                        usage2.transitions[t],
                                        std::move(label_prefix)));
  }
  std::size_t arc_id = 0;
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    const auto append_arc = [&](std::size_t place, PetriArcUsageKind kind) {
      const auto stats = result.transitions[t];
      result.arcs.push_back({arc_id++, t, place, kind,
                             kind != PetriArcUsageKind::kOutput,
                             stats.log1_occurrences,
                             stats.log2_occurrences, stats.total_occurrences,
                             stats.direction, stats.label, stats.color});
    };
    for (const auto place : transition.inputs)
      append_arc(place, PetriArcUsageKind::kInput);
    for (const auto place : transition.outputs)
      append_arc(place, PetriArcUsageKind::kOutput);
    for (const auto place : transition.reset_inputs)
      append_arc(place, PetriArcUsageKind::kReset);
    for (const auto place : transition.inhibitor_inputs)
      append_arc(place, PetriArcUsageKind::kInhibitor);
  }
  return result;
}

PetriUsageComparison compare_element_usage_two_logs(const PetriNet& net,
                                                    const EventLog& log1,
                                                    const EventLog& log2,
                                                    const PetriUsageComparisonOptions& options) {
  return compare_petri_usage(net, log1, log2, options);
}

PetriUsageComparison compare_element_usage_token_replay(const PetriNet& net, const EventLog& log1,
                                                       const EventLog& log2, TokenReplayOptions replay,
                                                       const std::vector<PnmlArcOccurrence>* arc_occurrences) {
  replay.variant = TokenReplayVariant::kForward;
  replay.enable_pltr_fitness = true;
  const auto usage = [&](const EventLog& log) {
    const auto result = replay_token_result(log, net, replay);
    std::vector<std::uint64_t> places(net.place_count(), 0), transitions(net.transitions().size(), 0);
    for (const auto& trace : result.traces)
      for (const auto transition : trace.activated_transitions) ++transitions.at(transition);
    if (result.pltr_fitness) {
      for (std::size_t place = 0; place < result.pltr_fitness->places.size() && place < places.size(); ++place) {
        const auto& fitness = result.pltr_fitness->places[place];
        places[place] = std::stoull(fitness.consumed.decimal()) + std::stoull(fitness.remaining.decimal());
      }
    }
    // The reference place statistics key a transition's preset by place, so
    // physically parallel input arcs from one place count only the last
    // arc's weight per firing (the marking itself still loses every arc's
    // tokens). Replace their summed weight by that one weight.
    if (arc_occurrences) {
      std::map<std::pair<std::size_t, std::size_t>, std::pair<std::uint64_t, std::uint64_t>> parallel;
      std::map<std::pair<std::size_t, std::size_t>, std::size_t> arcs;
      for (const auto& arc : *arc_occurrences) {
        if (!arc.input || arc.type != PetriArcType::kNormal || arc.negative_weight) continue;
        const auto weight = std::stoull(TokenInteger(arc.weight).decimal());
        auto& [sum, last] = parallel[{arc.place, arc.transition}];
        sum += weight;
        last = weight;
        ++arcs[{arc.place, arc.transition}];
      }
      for (const auto& [key, weights] : parallel) {
        const auto [place, transition] = key;
        if (arcs[key] < 2 || place >= places.size() || transition >= transitions.size()) continue;
        const auto excess = (weights.first - weights.second) * transitions[transition];
        places[place] = places[place] > excess ? places[place] - excess : 0;
      }
    }
    return std::pair{places, transitions};
  };
  const auto [places1, transitions1] = usage(log1);
  const auto [places2, transitions2] = usage(log2);
  PetriUsageComparison result;
  for (std::size_t p = 0; p < net.place_count(); ++p) result.places.push_back(record(p, places1[p], places2[p]));
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    std::string label_prefix;
    if (transition.activity && *transition.activity < net.activities().size())
      label_prefix = net.activities()[*transition.activity] + " ";
    result.transitions.push_back(record(t, transitions1[t], transitions2[t], std::move(label_prefix)));
  }
  std::size_t arc_id = 0;
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    const auto& stats = result.transitions[t];
    const auto append_arc = [&](std::size_t place, PetriArcUsageKind kind) {
      result.arcs.push_back({arc_id++, t, place, kind, kind != PetriArcUsageKind::kOutput, stats.log1_occurrences,
                             stats.log2_occurrences, stats.total_occurrences, stats.direction, stats.label,
                             stats.color});
    };
    if (arc_occurrences) {
      const auto physical = [&](PetriArcUsageKind kind) {
        for (const auto& arc : *arc_occurrences) {
          if (arc.transition != t) continue;
          const auto arc_kind = !arc.input ? PetriArcUsageKind::kOutput
                                : arc.type == PetriArcType::kReset ? PetriArcUsageKind::kReset
                                : arc.type == PetriArcType::kInhibitor ? PetriArcUsageKind::kInhibitor
                                : PetriArcUsageKind::kInput;
          if (arc_kind == kind) append_arc(arc.place, kind);
        }
      };
      for (const auto kind : {PetriArcUsageKind::kInput, PetriArcUsageKind::kOutput, PetriArcUsageKind::kReset,
                              PetriArcUsageKind::kInhibitor})
        physical(kind);
      continue;
    }
    for (const auto place : transition.inputs) append_arc(place, PetriArcUsageKind::kInput);
    for (const auto place : transition.outputs) append_arc(place, PetriArcUsageKind::kOutput);
    for (const auto place : transition.reset_inputs) append_arc(place, PetriArcUsageKind::kReset);
    for (const auto place : transition.inhibitor_inputs) append_arc(place, PetriArcUsageKind::kInhibitor);
  }
  return result;
}

PetriUsageComparison compare_petri_usage_reference(const PetriNet& net, const EventLog& log1,
                                                   const EventLog& log2,
                                                   const PetriUsageComparisonOptions& options) {
  TokenReplayOptions replay;
  // The key is used exactly as given (a spaced " code " is its own field);
  // only an all-blank key is rejected.
  const auto& attribute = options.activity_attribute;
  if (!attribute.empty() && attribute.find_first_not_of(" \t\r\n") == std::string::npos)
    throw std::invalid_argument("Petri usage activity_attribute must not be blank");
  // The omitted selector names the log's first-class activity, whatever
  // source column it came from; an explicit key follows the reference
  // selector rule (its literal field, or a projection read from it).
  const auto projected_key = [&]() -> std::string {
    for (const auto* log : {&log1, &log2})
      for (const auto& trace : log->traces)
        for (const auto& event : trace.events)
          if (!event.activity_source_key.empty()) return event.activity_source_key;
    return "concept:name";
  };
  replay.activity_attribute = attribute.empty() ? projected_key() : attribute;
  replay.initial_marking.assign(options.initial_marking.begin(), options.initial_marking.end());
  replay.final_marking.assign(options.final_marking.begin(), options.final_marking.end());
  replay.walk_through_hidden_transitions = options.walk_through_hidden_trans;
  replay.try_to_reach_final_marking_through_hidden =
      options.try_to_reach_final_marking_through_hidden;
  replay.stop_immediately_unfit = options.stop_immediately_unfit;
  replay.cleaning_token_flood = options.cleaning_token_flood;
  replay.place_names = options.place_names;
  replay.transition_names = options.transition_names;
  replay.initial_marking_keys = options.initial_marking_keys;
  replay.final_marking_keys = options.final_marking_keys;
  replay.exhaustive_invisible_exploration = options.exhaustive_invisible_exploration;
  return compare_element_usage_token_replay(net, log1, log2, std::move(replay),
                                            options.arc_occurrences ? &*options.arc_occurrences : nullptr);
}

}  // namespace minera
