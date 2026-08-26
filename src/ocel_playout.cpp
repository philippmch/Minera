#include "minera/ocel_playout.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/playout.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct ObjectChoice {
  std::string type;
  std::string object_id;
  std::vector<Trace> traces;
  struct TransitionStep {
    std::string activity;
    std::size_t transition_index = 0;
  };
  std::vector<std::vector<TransitionStep>> transition_traces;
};

struct PendingEvent {
  std::string activity;
  std::string type;
  std::string object_id;
  std::size_t local_index = 0;
  std::size_t choice_index = 0;
  std::size_t transition_index = 0;
};

struct SynchronizedObject {
  std::string type;
  std::string id;
  const PetriNet* net = nullptr;
  std::vector<std::uint32_t> marking;
  std::vector<std::uint32_t> final_marking;
};

struct SynchronizedState {
  std::vector<std::vector<std::uint32_t>> markings;
  std::vector<std::size_t> activity_counts;
  std::vector<OcpnBinding> sequence;
  std::vector<OcpnTraceEvent> traces;
};

struct SynchronizedAction {
  bool silent = false;
  std::size_t object_index = 0;
  std::size_t transition_index = 0;
  OcpnBinding binding;
  std::vector<std::pair<std::size_t, std::size_t>> firings;
  OcpnTraceEvent trace;
};

void validate_net(const PetriNet& net, const std::string& type) {
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
    throw std::invalid_argument("OCPN playout net for type " + type +
                                " has invalid workflow places");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.inputs.empty())
      throw std::invalid_argument("OCPN playout net contains a source transition");
    for (const auto place : transition.inputs) {
      if (place >= net.place_count())
        throw std::invalid_argument("OCPN playout transition input is invalid");
    }
    for (const auto place : transition.outputs) {
      if (place >= net.place_count())
        throw std::invalid_argument("OCPN playout transition output is invalid");
    }
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("OCPN playout transition activity is invalid");
  }
}

void validate_marker_map(
    const OcpnGraph& graph,
    const std::map<std::string, std::vector<std::uint32_t>>& markers,
    const char* label) {
  for (const auto& [type, marking] : markers) {
    const auto net = graph.petri_nets.find(type);
    if (net == graph.petri_nets.end()) {
      throw std::invalid_argument(std::string("OCPN playout ") + label +
                                  " marking names an unknown object type: " +
                                  type);
    }
    if (marking.size() != net->second.place_count()) {
      throw std::invalid_argument(std::string("OCPN playout ") + label +
                                  " marking for object type " + type +
                                  " must match the place count");
    }
  }
}

void validate_marker_maps(const OcpnGraph& graph,
                          const OcpnPlayoutOptions& options) {
  validate_marker_map(graph, options.initial_markings, "initial");
  validate_marker_map(graph, options.final_markings, "final");
}

std::vector<std::uint32_t> marker_for(
    const PetriNet& net,
    const std::map<std::string, std::vector<std::uint32_t>>& markers,
    const std::string& type, std::size_t boundary) {
  const auto found = markers.find(type);
  if (found != markers.end()) return found->second;
  std::vector<std::uint32_t> result(net.place_count(), 0);
  result[boundary] = 1;
  return result;
}

std::size_t visible_bound(const PetriNet& net,
                         std::size_t max_bindings_per_activity) {
  std::size_t visible = 0;
  for (const auto& transition : net.transitions())
    if (transition.activity) ++visible;
  const auto factor = std::max<std::size_t>(1, visible);
  if (max_bindings_per_activity >
      std::numeric_limits<std::size_t>::max() / factor) {
    return std::numeric_limits<std::size_t>::max();
  }
  return max_bindings_per_activity * factor;
}

std::size_t rounded_branching_limit(double cap, std::mt19937_64& rng) {
  if (cap <= 0.0) return 0;
  const auto integral = std::floor(cap);
  auto limit = integral >=
                       static_cast<double>(std::numeric_limits<std::size_t>::max())
                   ? std::numeric_limits<std::size_t>::max()
                   : static_cast<std::size_t>(integral);
  const auto fraction = cap - integral;
  if (fraction > 0.0) {
    std::uniform_real_distribution<double> sample(0.0, 1.0);
    if (sample(rng) < fraction &&
        limit != std::numeric_limits<std::size_t>::max())
      ++limit;
  }
  return limit;
}

template <typename Value>
void sample_branch(std::vector<Value>& values, double cap,
                   std::mt19937_64& rng) {
  if (cap <= 0.0 || values.empty()) return;
  const auto limit = rounded_branching_limit(cap, rng);
  if (limit >= values.size()) return;
  std::shuffle(values.begin(), values.end(), rng);
  values.resize(limit);
}

struct TransitionTraceState {
  std::vector<std::uint32_t> marking;
  std::vector<std::size_t> visible_transitions;
};

std::string transition_trace_state_key(const TransitionTraceState& state) {
  std::string key;
  for (const auto token : state.marking) {
    key += std::to_string(token);
    key += ',';
  }
  key += '|';
  for (const auto transition : state.visible_transitions) {
    key += std::to_string(transition);
    key += ',';
  }
  return key;
}

std::vector<std::vector<ObjectChoice::TransitionStep>>
enumerate_transition_traces(
    const PetriNet& net, const std::vector<std::uint32_t>& initial_marking,
    const std::vector<std::uint32_t>& final_marking, std::size_t max_length,
    std::size_t max_traces, std::size_t max_states,
    double branching_factor_transitions, std::uint64_t seed) {
  std::vector<std::vector<ObjectChoice::TransitionStep>> result;
  if (max_traces == 0 || max_states == 0) return result;

  std::mt19937_64 rng(seed);
  std::deque<TransitionTraceState> queue;
  std::unordered_set<std::string> visited;
  TransitionTraceState initial{initial_marking, {}};
  visited.insert(transition_trace_state_key(initial));
  queue.push_back(initial);

  const auto emit = [&](const TransitionTraceState& state) {
    std::vector<ObjectChoice::TransitionStep> trace;
    trace.reserve(state.visible_transitions.size());
    for (const auto transition_index : state.visible_transitions) {
      const auto& transition = net.transitions().at(transition_index);
      trace.push_back({net.activities().at(*transition.activity),
                       transition_index});
    }
    result.push_back(std::move(trace));
  };

  if (initial.marking == final_marking) emit(initial);
  while (!queue.empty() && result.size() < max_traces &&
         visited.size() < max_states) {
    const auto state = std::move(queue.front());
    queue.pop_front();
    std::vector<std::size_t> enabled;
    enabled.reserve(net.transitions().size());
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (transition.inputs.empty() && !transition.has_extended_arcs())
        continue;
      if (!transition.silent() && state.visible_transitions.size() >= max_length)
        continue;
      if (petri_transition_enabled(transition, state.marking))
        enabled.push_back(index);
    }
    sample_branch(enabled, branching_factor_transitions, rng);
    for (const auto transition_index : enabled) {
      const auto& transition = net.transitions()[transition_index];
      TransitionTraceState next = state;
      fire_petri_transition(transition, next.marking);
      if (!transition.silent())
        next.visible_transitions.push_back(transition_index);
      if (!visited.insert(transition_trace_state_key(next)).second) continue;
      if (next.marking == final_marking) {
        emit(next);
        if (result.size() >= max_traces) break;
      }
      queue.push_back(std::move(next));
      if (visited.size() >= max_states) break;
    }
  }
  return result;
}

std::uint64_t derived_seed(std::uint64_t seed, const std::string& type,
                           const std::string& object_id,
                           const std::string& purpose) {
  std::uint64_t value = seed ^ 1469598103934665603ULL;
  for (const unsigned char c : type + '\n' + object_id + '\n' + purpose) {
    value ^= c;
    value *= 1099511628211ULL;
  }
  return value;
}

void write_json_string(std::ostringstream& out, const std::string& value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << static_cast<char>(c); break;
    }
  }
  out << '"';
}

void append_key_part(std::string& key, const std::string& value) {
  key += std::to_string(value.size());
  key += ':';
  key += value;
  key += ';';
}

std::string synchronized_state_key(const SynchronizedState& state,
                                   bool include_transition_identity) {
  std::string key;
  for (const auto& marking : state.markings) {
    key += 'm';
    for (const auto token : marking) {
      key += std::to_string(token);
      key += ',';
    }
    key += '|';
  }
  key += 'c';
  for (const auto count : state.activity_counts) {
    key += std::to_string(count);
    key += ',';
  }
  key += '|';
  for (const auto& binding : state.sequence) {
    append_key_part(key, binding.activity);
    for (const auto& [type, object_id] : binding.objects) {
      append_key_part(key, type);
      append_key_part(key, object_id);
    }
    key += '|';
  }
  if (include_transition_identity) {
    key += 't';
    for (const auto& event : state.traces) {
      append_key_part(key, event.activity);
      for (const auto& transition : event.transitions) {
        append_key_part(key, transition.object_type);
        append_key_part(key, transition.object_id);
        key += std::to_string(transition.transition_index);
        key += ',';
      }
      key += '|';
    }
  }
  return key;
}

bool at_final_marking(const SynchronizedObject& object,
                     const std::vector<std::uint32_t>& marking) {
  return marking == object.final_marking;
}

bool has_visible_activity(const PetriNet& net, const std::string& activity) {
  for (const auto& transition : net.transitions()) {
    if (!transition.activity || *transition.activity >= net.activities().size())
      continue;
    if (net.activities()[*transition.activity] == activity) return true;
  }
  return false;
}

struct ActivityBindingSlots {
  std::vector<std::string> fixed;
  std::vector<std::string> variable;
};

ActivityBindingSlots synchronized_activity_slots(
    const OcpnGraph& graph, const std::string& activity,
    const std::vector<SynchronizedObject>& objects) {
  ActivityBindingSlots slots;
  const auto fixed = graph.activity_object_types.find(activity);
  const auto variable = graph.activity_variable_object_types.find(activity);
  const auto copy_unique = [](const auto& source, auto& target) {
    for (const auto& type : source) {
      if (type.empty() ||
          std::find(target.begin(), target.end(), type) != target.end())
        continue;
      target.push_back(type);
    }
  };
  if (fixed != graph.activity_object_types.end())
    copy_unique(fixed->second, slots.fixed);
  if (variable != graph.activity_variable_object_types.end())
    copy_unique(variable->second, slots.variable);
  if (!slots.fixed.empty() || !slots.variable.empty()) return slots;

  // Hand-built graphs may not carry discovery annotations.  In that case,
  // bind one object from every type whose projection exposes the activity.
  for (const auto& object : objects) {
    if (has_visible_activity(*object.net, activity) &&
        std::find(slots.fixed.begin(), slots.fixed.end(), object.type) ==
            slots.fixed.end())
      slots.fixed.push_back(object.type);
  }
  return slots;
}

void enumerate_variable_types(
    const std::vector<std::string>& variable_types, std::size_t type_index,
    const std::string& activity, const std::vector<SynchronizedObject>& objects,
    const SynchronizedState& state, std::vector<bool>& used,
    std::vector<std::pair<std::size_t, std::size_t>>& firings,
    std::vector<std::pair<std::string, std::string>>& bound_objects,
    std::vector<SynchronizedAction>& actions, std::size_t limit);

void enumerate_variable_objects(
    const std::vector<std::string>& variable_types, std::size_t type_index,
    std::size_t object_index, const std::string& activity,
    const std::vector<SynchronizedObject>& objects, const SynchronizedState& state,
    std::vector<bool>& used,
    std::vector<std::pair<std::size_t, std::size_t>>& firings,
    std::vector<std::pair<std::string, std::string>>& bound_objects,
    std::vector<SynchronizedAction>& actions, std::size_t limit) {
  if (limit != 0 && actions.size() >= limit) return;
  if (object_index == objects.size()) {
    enumerate_variable_types(variable_types, type_index + 1, activity, objects,
                             state, used, firings, bound_objects, actions, limit);
    return;
  }
  const auto& object = objects[object_index];
  if (object.type != variable_types[type_index] || used[object_index]) {
    enumerate_variable_objects(variable_types, type_index, object_index + 1,
                               activity, objects, state, used, firings,
                               bound_objects, actions, limit);
    return;
  }

  // A variable arc may omit this object.  The recursive branch also permits
  // selecting several objects of the same type for one activity.
  enumerate_variable_objects(variable_types, type_index, object_index + 1,
                             activity, objects, state, used, firings,
                             bound_objects, actions, limit);
  for (std::size_t transition_index = 0;
       transition_index < object.net->transitions().size(); ++transition_index) {
    const auto& transition = object.net->transitions()[transition_index];
    if (!transition.activity ||
        *transition.activity >= object.net->activities().size() ||
        object.net->activities()[*transition.activity] != activity ||
        !petri_transition_enabled(transition, state.markings[object_index]))
      continue;
    used[object_index] = true;
    firings.emplace_back(object_index, transition_index);
    bound_objects.emplace_back(object.type, object.id);
    enumerate_variable_objects(variable_types, type_index, object_index + 1,
                               activity, objects, state, used, firings,
                               bound_objects, actions, limit);
    bound_objects.pop_back();
    firings.pop_back();
    used[object_index] = false;
    if (limit != 0 && actions.size() >= limit) return;
  }
}

void enumerate_variable_types(
    const std::vector<std::string>& variable_types, std::size_t type_index,
    const std::string& activity, const std::vector<SynchronizedObject>& objects,
    const SynchronizedState& state, std::vector<bool>& used,
    std::vector<std::pair<std::size_t, std::size_t>>& firings,
    std::vector<std::pair<std::string, std::string>>& bound_objects,
    std::vector<SynchronizedAction>& actions, std::size_t limit) {
  if (limit != 0 && actions.size() >= limit) return;
  if (type_index == variable_types.size()) {
    if (bound_objects.empty()) return;
    SynchronizedAction action;
    action.binding.activity = activity;
    action.binding.objects = bound_objects;
    action.firings = firings;
    actions.push_back(std::move(action));
    return;
  }
  enumerate_variable_objects(variable_types, type_index, 0, activity, objects,
                             state, used, firings, bound_objects, actions, limit);
}

void enumerate_binding_slots(
    const std::vector<std::string>& slots, std::size_t slot,
    const std::string& activity, const std::vector<SynchronizedObject>& objects,
    const SynchronizedState& state, std::vector<bool>& used,
    std::vector<std::pair<std::size_t, std::size_t>>& firings,
    std::vector<std::pair<std::string, std::string>>& bound_objects,
    std::vector<SynchronizedAction>& actions, std::size_t limit,
    const std::vector<std::string>& variable_slots) {
  if (limit != 0 && actions.size() >= limit) return;
  if (slot == slots.size()) {
    enumerate_variable_types(variable_slots, 0, activity, objects, state, used,
                             firings, bound_objects, actions, limit);
    return;
  }

  for (std::size_t object_index = 0; object_index < objects.size();
       ++object_index) {
    if (used[object_index] || objects[object_index].type != slots[slot]) continue;
    const auto& object = objects[object_index];
    for (std::size_t transition_index = 0;
         transition_index < object.net->transitions().size(); ++transition_index) {
      const auto& transition = object.net->transitions()[transition_index];
      if (!transition.activity ||
          *transition.activity >= object.net->activities().size() ||
          object.net->activities()[*transition.activity] != activity ||
          !petri_transition_enabled(transition, state.markings[object_index]))
        continue;
      used[object_index] = true;
      firings.emplace_back(object_index, transition_index);
      bound_objects.emplace_back(object.type, object.id);
      enumerate_binding_slots(slots, slot + 1, activity, objects, state, used,
                              firings, bound_objects, actions, limit,
                              variable_slots);
      bound_objects.pop_back();
      firings.pop_back();
      used[object_index] = false;
      if (limit != 0 && actions.size() >= limit) return;
    }
  }
}

std::vector<SynchronizedAction> synchronized_actions(
    const OcpnGraph& graph, const std::vector<SynchronizedObject>& objects,
    const SynchronizedState& state, const std::vector<std::string>& activities,
    const OcpnPlayoutOptions& options, std::mt19937_64& rng) {
  std::vector<SynchronizedAction> actions;
  for (std::size_t object_index = 0; object_index < objects.size();
       ++object_index) {
    const auto& object = objects[object_index];
    for (std::size_t transition_index = 0;
         transition_index < object.net->transitions().size(); ++transition_index) {
      const auto& transition = object.net->transitions()[transition_index];
      if (!transition.silent() && transition.activity &&
          *transition.activity < object.net->activities().size())
        continue;
      if (transition.inputs.empty() && !transition.has_extended_arcs()) continue;
      if (!petri_transition_enabled(transition, state.markings[object_index]))
        continue;
      SynchronizedAction action;
      action.silent = true;
      action.object_index = object_index;
      action.transition_index = transition_index;
      actions.push_back(std::move(action));
    }
  }

  for (const auto& activity : activities) {
    const auto found = std::lower_bound(activities.begin(), activities.end(), activity);
    const auto activity_index = static_cast<std::size_t>(found - activities.begin());
    if (activity_index >= state.activity_counts.size() ||
        state.activity_counts[activity_index] >= options.max_bindings_per_activity)
      continue;
    const auto slots = synchronized_activity_slots(graph, activity, objects);
    if (slots.fixed.empty() && slots.variable.empty()) continue;
    std::vector<bool> used(objects.size(), false);
    std::vector<std::pair<std::size_t, std::size_t>> firings;
    std::vector<std::pair<std::string, std::string>> bound_objects;
    std::vector<SynchronizedAction> bindings;
    enumerate_binding_slots(slots.fixed, 0, activity, objects, state, used,
                            firings, bound_objects, bindings,
                            0, slots.variable);
    for (auto& action : bindings) {
      action.trace.activity = action.binding.activity;
      action.trace.objects = action.binding.objects;
      for (const auto [object_index, transition_index] : action.firings) {
        action.trace.transitions.push_back(
            {objects[object_index].type, objects[object_index].id,
             transition_index});
      }
    }
    sample_branch(bindings, options.branching_factor_bindings, rng);
    actions.insert(actions.end(), std::make_move_iterator(bindings.begin()),
                   std::make_move_iterator(bindings.end()));
  }
  sample_branch(actions, options.branching_factor_transitions, rng);
  return actions;
}

OcpnPlayoutResult synchronized_playout(const OcpnGraph& graph,
                                       const OcpnPlayoutOptions& options) {
  OcpnPlayoutResult result;
  result.exists_trace_requested = options.exists_trace;
  result.return_sequences_requested = options.return_sequences;
  result.return_traces_requested = options.return_traces;
  if (!std::isfinite(options.branching_factor_transitions) ||
      options.branching_factor_transitions < 0.0 ||
      !std::isfinite(options.branching_factor_bindings) ||
      options.branching_factor_bindings < 0.0) {
    throw std::invalid_argument(
        "OCPN branching factors must be finite and non-negative");
  }
  validate_marker_maps(graph, options);

  for (const auto& [type, object_ids] : options.objects) {
    if (!graph.petri_nets.contains(type))
      throw std::invalid_argument("OCPN playout object type is not in the model: " +
                                  type);
    std::unordered_set<std::string> unique_ids;
    for (const auto& id : object_ids) {
      if (id.empty())
        throw std::invalid_argument("OCPN playout object ids must not be empty");
      if (!unique_ids.insert(id).second)
        throw std::invalid_argument("OCPN playout object ids must be unique");
    }
  }

  std::vector<SynchronizedObject> objects;
  std::map<std::string, std::string> supplied_object_types;
  for (const auto& [type, net] : graph.petri_nets) {
    validate_net(net, type);
    std::vector<std::string> ids;
    const auto supplied = options.objects.find(type);
    if (supplied == options.objects.end()) {
      ids.push_back(type + "_1");
    } else {
      ids = supplied->second;
    }
    for (const auto& id : ids) {
      if (supplied_object_types.contains(id))
        throw std::invalid_argument(
            "OCPN playout object ids must be unique across object types");
      supplied_object_types[id] = type;
      SynchronizedObject object;
      object.type = type;
      object.id = id;
      object.net = &net;
      object.marking = marker_for(net, options.initial_markings, type,
                                  net.source());
      object.final_marking = marker_for(net, options.final_markings, type,
                                        net.sink());
      objects.push_back(std::move(object));
    }
  }
  if (objects.empty() || options.max_states == 0) return result;

  std::vector<std::string> activities = graph.activities;
  for (const auto& object : objects)
    for (const auto& activity : object.net->activities()) activities.push_back(activity);
  std::sort(activities.begin(), activities.end());
  activities.erase(std::unique(activities.begin(), activities.end()), activities.end());

  SynchronizedState initial;
  initial.activity_counts.assign(activities.size(), 0);
  for (const auto& object : objects) initial.markings.push_back(object.marking);
  std::deque<SynchronizedState> queue;
  std::unordered_set<std::string> visited;
  std::mt19937_64 rng(options.seed);
  visited.insert(synchronized_state_key(initial, options.return_traces));
  queue.push_back(std::move(initial));
  const auto trace_limit = options.exists_trace ? std::size_t{1} : options.max_traces;
  if (!options.exists_trace && options.max_traces == 0) return result;
  std::size_t generated_traces = 0;

  const auto is_final = [&](const SynchronizedState& state) {
    for (std::size_t index = 0; index < objects.size(); ++index)
      if (!at_final_marking(objects[index], state.markings[index])) return false;
    return true;
  };
  const auto emit = [&](const SynchronizedState& state, std::size_t sequence_index) {
    result.trace_exists = true;
    ++generated_traces;
    if (options.exists_trace) return;
    const std::string suffix = options.objects_unique_per_trace
                                   ? "_" + std::to_string(sequence_index + 1)
                                   : "";
    std::vector<std::string> emitted_ids;
    emitted_ids.reserve(objects.size());
    for (const auto& object : objects) {
      const auto id = object.id + suffix;
      emitted_ids.push_back(id);
      if (std::none_of(result.log.objects.begin(), result.log.objects.end(),
                       [&](const OcelObject& existing) { return existing.id == id; }))
        result.log.objects.push_back({id, object.type, {}});
    }
    for (std::size_t step_index = 0; step_index < state.sequence.size(); ++step_index) {
      const auto& binding = state.sequence[step_index];
      const auto event_id = "event_" + std::to_string(result.log.events.size() + 1);
      const auto timestamp = format_timestamp(
          options.start + static_cast<double>(sequence_index) * options.case_interval +
          static_cast<double>(step_index) * options.event_interval);
      result.log.events.push_back({event_id, binding.activity, timestamp, {}});
      for (const auto& [type, object_id] : binding.objects) {
        for (std::size_t object_index = 0; object_index < objects.size(); ++object_index) {
          if (objects[object_index].type == type && objects[object_index].id == object_id) {
            result.log.relations.push_back({event_id, emitted_ids[object_index], ""});
            break;
          }
        }
      }
    }
    if (options.return_sequences) result.sequences.push_back(state.sequence);
    if (options.return_traces) result.traces.push_back(state.traces);
  };

  while (!queue.empty() && (trace_limit == 0 || generated_traces < trace_limit)) {
    auto state = std::move(queue.front());
    queue.pop_front();
    if (is_final(state)) {
      emit(state, generated_traces);
      if (options.exists_trace ||
          (trace_limit != 0 && generated_traces >= trace_limit))
        break;
      continue;
    }
    const auto actions = synchronized_actions(
        graph, objects, state, activities, options, rng);
    for (const auto& action : actions) {
      SynchronizedState next = state;
      if (action.silent) {
        fire_petri_transition(
            objects[action.object_index].net->transitions()[action.transition_index],
            next.markings[action.object_index]);
      } else {
        const auto activity_it = std::lower_bound(
            activities.begin(), activities.end(), action.binding.activity);
        const auto activity_index = static_cast<std::size_t>(activity_it - activities.begin());
        ++next.activity_counts[activity_index];
        for (const auto [object_index, transition_index] : action.firings)
          fire_petri_transition(objects[object_index].net->transitions()[transition_index],
                                next.markings[object_index]);
        next.sequence.push_back(action.binding);
        next.traces.push_back(action.trace);
      }
      if (visited.insert(synchronized_state_key(next, options.return_traces)).second) {
        queue.push_back(std::move(next));
        if (visited.size() >= options.max_states) break;
      }
    }
    if (visited.size() >= options.max_states) break;
  }
  return result;
}

}  // namespace

const char* ocpn_event_binding_name(OcpnEventBinding binding) {
  switch (binding) {
    case OcpnEventBinding::kIndependent: return "independent";
    case OcpnEventBinding::kSharedActivity: return "shared_activity";
    case OcpnEventBinding::kSynchronized: return "synchronized";
  }
  throw std::invalid_argument("OCPN event binding is invalid");
}

Ocel legacy_ocpn_extensive_playout(
    const OcpnGraph& graph, const OcpnPlayoutOptions& options,
    std::vector<std::vector<OcpnBinding>>* sequences,
    std::vector<std::vector<OcpnTraceEvent>>* traces,
    bool* trace_exists) {
  if (options.max_bindings_per_activity == 0)
    throw std::invalid_argument(
        "OCPN max_bindings_per_activity must be positive");
  if (options.event_binding != OcpnEventBinding::kIndependent &&
      options.event_binding != OcpnEventBinding::kSharedActivity) {
    throw std::invalid_argument(
        "OCPN event_binding must be independent or shared_activity");
  }
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval) || options.case_interval < 0.0 ||
      options.event_interval < 0.0) {
    throw std::invalid_argument("OCPN playout timestamps must be finite and non-negative");
  }
  validate_marker_maps(graph, options);
  for (const auto& [type, object_ids] : options.objects) {
    if (!graph.petri_nets.contains(type))
      throw std::invalid_argument("OCPN playout object type is not in the model: " +
                                  type);
    for (const auto& object_id : object_ids) {
      if (object_id.empty())
        throw std::invalid_argument("OCPN playout object ids must not be empty");
    }
  }

  std::vector<ObjectChoice> choices;
  std::map<std::string, std::string> supplied_object_types;
  for (const auto& [type, net] : graph.petri_nets) {
    validate_net(net, type);
    std::vector<std::string> object_ids;
    const auto supplied = options.objects.find(type);
    if (supplied == options.objects.end()) {
      object_ids.push_back(type + "_1");
    } else {
      object_ids = supplied->second;
    }
    if (object_ids.empty()) continue;

    for (const auto& object_id : object_ids) {
      const auto old_type = supplied_object_types.find(object_id);
      if (old_type != supplied_object_types.end())
        throw std::invalid_argument(
            "OCPN playout object ids must be unique across object types");
      supplied_object_types[object_id] = type;
      if (traces != nullptr) {
        const auto transition_traces = enumerate_transition_traces(
            net, marker_for(net, options.initial_markings, type, net.source()),
            marker_for(net, options.final_markings, type, net.sink()),
            visible_bound(net, options.max_bindings_per_activity),
            options.max_traces, options.max_states,
            options.branching_factor_transitions,
            derived_seed(options.seed, type, object_id, "trace-expansion"));
        if (transition_traces.empty()) return {};
        auto binding_rng = std::mt19937_64(
            derived_seed(options.seed, type, object_id, "binding-selection"));
        auto selected_traces = transition_traces;
        sample_branch(selected_traces, options.branching_factor_bindings,
                      binding_rng);
        choices.push_back({type, object_id, {}, std::move(selected_traces)});
        continue;
      }
      ExtensivePlayoutOptions trace_options;
      trace_options.max_length = visible_bound(
          net, options.max_bindings_per_activity);
      trace_options.max_traces = options.max_traces;
      trace_options.max_states = options.max_states;
      trace_options.branching_factor_transitions =
          options.branching_factor_transitions;
      trace_options.seed = derived_seed(options.seed, type, object_id,
                                        "trace-expansion");
      trace_options.start = 0.0;
      trace_options.case_interval = 0.0;
      trace_options.event_interval = 1.0;
      trace_options.initial_marking =
          marker_for(net, options.initial_markings, type, net.source());
      trace_options.final_marking =
          marker_for(net, options.final_markings, type, net.sink());
      const auto traces_log = extensive_playout(net, trace_options);
      if (traces_log.traces.empty()) return {};
      auto object_traces = traces_log.traces;
      auto binding_rng = std::mt19937_64(
          derived_seed(options.seed, type, object_id, "binding-selection"));
      sample_branch(object_traces, options.branching_factor_bindings,
                    binding_rng);
      choices.push_back({type, object_id, std::move(object_traces)});
    }
  }

  Ocel result;
  if (choices.empty() || options.max_traces == 0) return result;
  std::map<std::string, std::string> object_types;
  std::vector<std::size_t> selected(choices.size(), 0);
  std::size_t sequence_index = 0;

  const auto emit = [&]() {
    if (trace_exists) *trace_exists = true;
    std::vector<PendingEvent> pending;
    for (std::size_t choice = 0; choice < choices.size(); ++choice) {
      if (traces != nullptr) {
        const auto& selected_trace =
            choices[choice].transition_traces[selected[choice]];
        for (std::size_t index = 0; index < selected_trace.size(); ++index) {
          pending.push_back({selected_trace[index].activity,
                             choices[choice].type, choices[choice].object_id,
                             index, choice,
                             selected_trace[index].transition_index});
        }
      } else {
        const auto& selected_trace = choices[choice].traces[selected[choice]];
        for (std::size_t index = 0; index < selected_trace.events.size(); ++index) {
          pending.push_back({selected_trace.events[index].activity,
                             choices[choice].type, choices[choice].object_id,
                             index, choice, 0});
        }
      }
    }
    const bool shared_activity =
        options.event_binding == OcpnEventBinding::kSharedActivity;
    std::stable_sort(pending.begin(), pending.end(),
                     [shared_activity](const PendingEvent& left,
                                       const PendingEvent& right) {
                       if (shared_activity) {
                         return std::tie(left.local_index, left.activity,
                                         left.type, left.object_id,
                                         left.choice_index) <
                                std::tie(right.local_index, right.activity,
                                         right.type, right.object_id,
                                         right.choice_index);
                       }
                       return std::tie(left.local_index, left.type, left.object_id,
                                       left.choice_index) <
                              std::tie(right.local_index, right.type,
                                       right.object_id, right.choice_index);
                     });
    const std::string suffix = options.objects_unique_per_trace
                                   ? "_" + std::to_string(sequence_index + 1)
                                   : "";
    std::map<std::string, std::string> emitted_object_ids;
    for (const auto& choice : choices) {
      const auto id = choice.object_id + suffix;
      emitted_object_ids[choice.object_id + "\n" + choice.type] = id;
      if (!object_types.contains(id)) {
        result.objects.push_back({id, choice.type, {}});
        object_types[id] = choice.type;
      }
    }
    std::size_t emitted_index = 0;
    std::vector<OcpnBinding> sequence;
    std::vector<OcpnTraceEvent> emitted_trace;
    for (std::size_t index = 0; index < pending.size();) {
      const auto& event = pending[index];
      std::size_t end = index + 1;
      if (shared_activity) {
        while (end < pending.size() &&
               pending[end].local_index == event.local_index &&
               pending[end].activity == event.activity) {
          ++end;
        }
      }
      const auto event_id = "event_" + std::to_string(result.events.size() + 1);
      const auto timestamp = format_timestamp(
          options.start + static_cast<double>(sequence_index) *
                              options.case_interval +
          static_cast<double>(emitted_index) * options.event_interval);
      result.events.push_back({event_id, event.activity, timestamp, {}});
      for (std::size_t relation = index; relation < end; ++relation) {
        const auto& related = pending[relation];
        const auto id = emitted_object_ids.at(related.object_id + "\n" +
                                              related.type);
        result.relations.push_back({event_id, id, ""});
      }
      if (sequences) {
        OcpnBinding binding;
        binding.activity = event.activity;
        for (std::size_t relation = index; relation < end; ++relation) {
          const auto& related = pending[relation];
          binding.objects.emplace_back(related.type, related.object_id);
        }
        sequence.push_back(std::move(binding));
      }
      if (traces != nullptr) {
        OcpnTraceEvent trace_event;
        trace_event.activity = event.activity;
        for (std::size_t relation = index; relation < end; ++relation) {
          const auto& related = pending[relation];
          trace_event.objects.emplace_back(related.type, related.object_id);
          trace_event.transitions.push_back(
              {related.type, related.object_id, related.transition_index});
        }
        // The trace output follows the emitted event order, matching the
        // existing sequence projection while retaining transition identity.
        emitted_trace.push_back(std::move(trace_event));
      }
      ++emitted_index;
      index = end;
    }
    if (sequences) sequences->push_back(std::move(sequence));
    if (traces != nullptr) traces->push_back(std::move(emitted_trace));
    ++sequence_index;
  };

  std::function<void(std::size_t)> enumerate = [&](std::size_t index) {
    if (sequence_index >= options.max_traces) return;
    if (index == choices.size()) {
      emit();
      return;
    }
    const auto trace_count =
        traces != nullptr ? choices[index].transition_traces.size()
                          : choices[index].traces.size();
    for (std::size_t trace = 0; trace < trace_count; ++trace) {
      selected[index] = trace;
      enumerate(index + 1);
      if (sequence_index >= options.max_traces) return;
    }
  };
  enumerate(0);
  return result;
}

OcpnPlayoutResult ocpn_extensive_playout_result(
    const OcpnGraph& graph, const OcpnPlayoutOptions& options) {
  if (options.max_bindings_per_activity == 0)
    throw std::invalid_argument(
        "OCPN max_bindings_per_activity must be positive");
  if (options.event_binding != OcpnEventBinding::kIndependent &&
      options.event_binding != OcpnEventBinding::kSharedActivity &&
      options.event_binding != OcpnEventBinding::kSynchronized) {
    throw std::invalid_argument(
        "OCPN event_binding must be independent, shared_activity, or synchronized");
  }
  if (!std::isfinite(options.branching_factor_transitions) ||
      options.branching_factor_transitions < 0.0 ||
      !std::isfinite(options.branching_factor_bindings) ||
      options.branching_factor_bindings < 0.0) {
    throw std::invalid_argument(
        "OCPN branching factors must be finite and non-negative");
  }
  if (!std::isfinite(options.start) || !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval) || options.case_interval < 0.0 ||
      options.event_interval < 0.0) {
    throw std::invalid_argument(
        "OCPN playout timestamps must be finite and non-negative");
  }
  for (const auto& [type, object_ids] : options.objects) {
    if (!graph.petri_nets.contains(type))
      throw std::invalid_argument("OCPN playout object type is not in the model: " +
                                  type);
    for (const auto& object_id : object_ids) {
      if (object_id.empty())
        throw std::invalid_argument("OCPN playout object ids must not be empty");
    }
  }

  if (options.event_binding == OcpnEventBinding::kSynchronized) {
    return synchronized_playout(graph, options);
  }

  OcpnPlayoutResult result;
  result.exists_trace_requested = options.exists_trace;
  result.return_sequences_requested = options.return_sequences;
  result.return_traces_requested = options.return_traces;
  auto legacy_options = options;
  if (options.exists_trace) legacy_options.max_traces = 1;
  result.log = legacy_ocpn_extensive_playout(
      graph, legacy_options,
      options.return_sequences ? &result.sequences : nullptr,
      options.return_traces ? &result.traces : nullptr,
      &result.trace_exists);
  if (options.exists_trace) result.log = {};
  return result;
}

Ocel ocpn_extensive_playout(const OcpnGraph& graph,
                            const OcpnPlayoutOptions& options) {
  return ocpn_extensive_playout_result(graph, options).log;
}

std::string serialize_ocpn_playout_json(const Ocel& log) {
  return serialize_ocpn_playout_json(log, OcpnEventBinding::kIndependent);
}

std::string serialize_ocpn_playout_json(const Ocel& log,
                                        OcpnEventBinding binding) {
  return std::string("{\"family\":\"OCEL_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"event_binding\":\"") +
         ocpn_event_binding_name(binding) + "\",\"log\":" +
         serialize_ocel_json(log) + "}";
}

std::string serialize_ocpn_playout_json(const OcpnPlayoutResult& result,
                                        OcpnEventBinding binding) {
  std::ostringstream out;
  out << "{\"family\":\"OCEL_PLAYOUT\",\"variant\":\"EXTENSIVE\",\"event_binding\":";
  write_json_string(out, ocpn_event_binding_name(binding));
  if (result.exists_trace_requested)
    out << ",\"exists_trace\":" << (result.trace_exists ? "true" : "false");
  if (result.return_sequences_requested) {
    out << ",\"sequences\":[";
    for (std::size_t sequence = 0; sequence < result.sequences.size(); ++sequence) {
      if (sequence != 0) out << ',';
      out << '[';
      const auto& values = result.sequences[sequence];
      for (std::size_t step = 0; step < values.size(); ++step) {
        if (step != 0) out << ',';
        out << "{\"activity\":";
        write_json_string(out, values[step].activity);
        out << ",\"objects\":[";
        for (std::size_t object = 0; object < values[step].objects.size(); ++object) {
          if (object != 0) out << ',';
          out << "{\"type\":";
          write_json_string(out, values[step].objects[object].first);
          out << ",\"id\":";
          write_json_string(out, values[step].objects[object].second);
          out << '}';
        }
        out << "]}";
      }
      out << ']';
    }
    out << ']';
  }
  if (result.return_traces_requested) {
    out << ",\"traces\":[";
    for (std::size_t trace = 0; trace < result.traces.size(); ++trace) {
      if (trace != 0) out << ',';
      out << '[';
      const auto& values = result.traces[trace];
      for (std::size_t step = 0; step < values.size(); ++step) {
        if (step != 0) out << ',';
        out << "{\"activity\":";
        write_json_string(out, values[step].activity);
        out << ",\"objects\":[";
        for (std::size_t object = 0; object < values[step].objects.size();
             ++object) {
          if (object != 0) out << ',';
          out << "{\"type\":";
          write_json_string(out, values[step].objects[object].first);
          out << ",\"id\":";
          write_json_string(out, values[step].objects[object].second);
          out << '}';
        }
        out << "],\"transitions\":[";
        for (std::size_t transition = 0;
             transition < values[step].transitions.size(); ++transition) {
          if (transition != 0) out << ',';
          const auto& fired = values[step].transitions[transition];
          out << "{\"object_type\":";
          write_json_string(out, fired.object_type);
          out << ",\"object_id\":";
          write_json_string(out, fired.object_id);
          out << ",\"transition_index\":" << fired.transition_index
              << '}';
        }
        out << "]}";
      }
      out << ']';
    }
    out << ']';
  }
  out << ",\"log\":" << serialize_ocel_json(result.log) << '}';
  return out.str();
}

}  // namespace minera
