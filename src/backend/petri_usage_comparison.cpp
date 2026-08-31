#include "minera/petri_usage_comparison.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

using Marking = std::vector<std::uint64_t>;
constexpr std::size_t kSilentSearchCap = 4096;

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
  const double position = std::clamp(0.5 + 0.5 * direction, 0.0, 1.0);
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
const std::string* selected_activity_value(const Event& event,
                                           std::string_view attribute) {
  if (attribute.empty() || attribute == "activity" ||
      attribute == "concept:name") {
    return &event.activity;
  }
  const auto selected_first_class = [&](std::string_view source_key,
                                        FirstClassAttributeState state,
                                        const std::string& value)
      -> const std::string* {
    if (source_key != attribute) return nullptr;
    if (state == FirstClassAttributeState::kNull ||
        (state == FirstClassAttributeState::kMissing && value.empty()))
      return nullptr;
    return &value;
  };
  if (const auto* value = selected_first_class(
          event.activity_source_key, event.activity_state, event.activity))
    return value;
  if (const auto* value = selected_first_class(
          event.resource_source_key, event.resource_state, event.resource))
    return value;
  if (const auto* value = selected_first_class(
          event.timestamp_source_key, event.timestamp_state, event.timestamp))
    return value;
  if (const auto* value = selected_first_class(
          event.lifecycle_source_key, event.lifecycle_state, event.lifecycle))
    return value;
  if (const auto* value = selected_first_class(
          event.case_id_source_key, event.case_id_state, event.case_id))
    return value;
  if (attribute == "resource" || attribute == "org:resource") {
    return &event.resource;
  }
  if (attribute == "timestamp" || attribute == "time:timestamp") {
    return &event.timestamp;
  }
  if (attribute == "lifecycle" || attribute == "lifecycle:transition") {
    return &event.lifecycle;
  }
  return event.attribute(attribute);
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
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
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

void fire(Marking& marking, std::size_t transition_id, const PetriNet& net,
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
}

Marking resolve_marking(const std::vector<std::uint32_t>& configured,
                        std::size_t place_count, std::size_t fallback,
                        std::string_view name) {
  if (configured.empty()) {
    Marking result(place_count, 0);
    result[fallback] = 1;
    return result;
  }
  if (configured.size() != place_count) {
    throw std::invalid_argument(std::string(name) +
                                " must contain one entry per place");
  }
  return Marking(configured.begin(), configured.end());
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

ReplayUsage replay_log(const EventLog& log, const PetriNet& net,
                       const std::unordered_map<std::string, std::size_t>& activities,
                       const Marking& initial_marking,
                       const Marking& final_marking,
                       std::string_view activity_attribute,
                       bool walk_through_hidden_trans,
                       bool try_to_reach_final_marking_through_hidden,
                       bool stop_immediately_unfit) {
  ReplayUsage usage{std::vector<std::uint64_t>(net.transitions().size(), 0),
                    std::vector<std::uint64_t>(net.place_count(), 0)};
  std::vector<std::vector<std::size_t>> transitions_by_activity(net.activities().size());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    if (net.transitions()[t].activity) {
      transitions_by_activity[*net.transitions()[t].activity].push_back(t);
    }
  }

  for (const auto& trace : log.traces) {
    Marking marking = initial_marking;
    std::vector<std::size_t> activated;
    for (const auto& event : trace.events) {
      const auto* selected_activity =
          selected_activity_value(event, activity_attribute);
      if (!selected_activity) {
        if (stop_immediately_unfit) break;
        continue;
      }
      const auto activity = activities.find(*selected_activity);
      if (activity == activities.end() || transitions_by_activity[activity->second].empty()) {
        // An unknown log activity is a deviation but activates no model
        // element. Immediate-stop mode prevents later events from contributing.
        if (stop_immediately_unfit) break;
        continue;
      }
      const auto& targets = transitions_by_activity[activity->second];
      if (!std::any_of(targets.begin(), targets.end(), [&](const auto t) {
            return enabled(marking, net.transitions()[t]);
          }) && walk_through_hidden_trans) {
        const auto path = silent_path(marking, targets, net);
        for (const auto t : path) fire(marking, t, net, usage, activated);
      }
      std::size_t chosen = targets.front();
      bool forced = true;
      for (const auto t : targets) {
        if (enabled(marking, net.transitions()[t])) {
          chosen = t;
          forced = false;
          break;
        }
      }
      fire(marking, chosen, net, usage, activated);
      if (forced && stop_immediately_unfit) break;
    }

    const auto is_final_marking = [&](const Marking& candidate) {
      return candidate == final_marking;
    };
    if (try_to_reach_final_marking_through_hidden &&
        !is_final_marking(marking)) {
      // Route silently until the exact workflow final marking is reached.
      struct FinishNode {
        Marking marking;
        std::size_t transition = std::numeric_limits<std::size_t>::max();
        std::size_t parent = std::numeric_limits<std::size_t>::max();
      };
      std::vector<FinishNode> arena;
      std::unordered_set<Marking, MarkingHash> seen{marking};
      std::queue<std::size_t> frontier;
      frontier.push(std::numeric_limits<std::size_t>::max());
      std::vector<std::size_t> path;
      std::size_t generated = 1;
      std::size_t expanded = 0;
      while (!frontier.empty() && expanded++ < kSilentSearchCap && path.empty()) {
        const auto parent = frontier.front();
        frontier.pop();
        const Marking current = parent == std::numeric_limits<std::size_t>::max()
                                    ? marking
                                    : arena[parent].marking;
        for (std::size_t t = 0; t < net.transitions().size(); ++t) {
          if (!net.transitions()[t].silent() || !enabled(current, net.transitions()[t])) continue;
          Marking next = current;
          apply_marking(next, net.transitions()[t]);
          if (!seen.insert(next).second) continue;
          if (generated++ >= kSilentSearchCap) {
            throw std::runtime_error(
                "Petri usage replay exceeded the silent-routing state limit");
          }
          arena.push_back(FinishNode{std::move(next), t, parent});
          const auto found = arena.size() - 1;
          if (is_final_marking(arena[found].marking)) {
            for (auto i = found; i != std::numeric_limits<std::size_t>::max();
                 i = arena[i].parent) path.push_back(arena[i].transition);
            std::reverse(path.begin(), path.end());
            break;
          }
          frontier.push(found);
        }
      }
      if (path.empty() && !frontier.empty()) {
        throw std::runtime_error(
            "Petri usage replay exceeded the silent-routing state limit");
      }
      for (const auto t : path) fire(marking, t, net, usage, activated);
    }
    // Place usage includes tokens in the reached final marking, just as it
    // includes residual tokens when silent completion is unreachable.
    for (std::size_t p = 0; p < marking.size(); ++p) {
      checked_add(usage.places[p], marking[p]);
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
  result.color = direction_color(result.direction);
  return result;
}

}  // namespace

PetriUsageComparison compare_petri_usage(const PetriNet& net, const EventLog& log1,
                                         const EventLog& log2,
                                         const PetriUsageComparisonOptions& options) {
  validate_net(net);
  const auto activity_attribute =
      normalize_activity_attribute(options.activity_attribute);
  const auto initial_marking = resolve_marking(
      options.initial_marking, net.place_count(), net.source(), "initial_marking");
  const auto final_marking = resolve_marking(
      options.final_marking, net.place_count(), net.sink(), "final_marking");
  std::unordered_map<std::string, std::size_t> activities;
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    activities.emplace(net.activities()[i], i);
  }
  const auto usage1 = replay_log(log1, net, activities, initial_marking,
                                 final_marking, activity_attribute,
                                 options.walk_through_hidden_trans,
                                 options.try_to_reach_final_marking_through_hidden,
                                 options.stop_immediately_unfit);
  const auto usage2 = replay_log(log2, net, activities, initial_marking,
                                 final_marking, activity_attribute,
                                 options.walk_through_hidden_trans,
                                 options.try_to_reach_final_marking_through_hidden,
                                 options.stop_immediately_unfit);
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

}  // namespace minera
