#include "minera/ocel_playout.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
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
};

struct PendingEvent {
  std::string activity;
  std::string type;
  std::string object_id;
  std::size_t local_index = 0;
  std::size_t choice_index = 0;
};

struct SynchronizedObject {
  std::string type;
  std::string id;
  const PetriNet* net = nullptr;
  std::vector<std::uint32_t> marking;
};

struct SynchronizedState {
  std::vector<std::vector<std::uint32_t>> markings;
  std::vector<std::size_t> activity_counts;
  std::vector<OcpnBinding> sequence;
};

struct SynchronizedAction {
  bool silent = false;
  std::size_t object_index = 0;
  std::size_t transition_index = 0;
  OcpnBinding binding;
  std::vector<std::pair<std::size_t, std::size_t>> firings;
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

std::string synchronized_state_key(const SynchronizedState& state) {
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
  return key;
}

bool at_final_marking(const SynchronizedObject& object,
                     const std::vector<std::uint32_t>& marking) {
  if (marking.size() != object.net->place_count()) return false;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] != (place == object.net->sink() ? 1U : 0U)) return false;
  }
  return true;
}

bool has_visible_activity(const PetriNet& net, const std::string& activity) {
  for (const auto& transition : net.transitions()) {
    if (!transition.activity || *transition.activity >= net.activities().size())
      continue;
    if (net.activities()[*transition.activity] == activity) return true;
  }
  return false;
}

std::vector<std::string> synchronized_activity_slots(
    const OcpnGraph& graph, const std::string& activity,
    const std::vector<SynchronizedObject>& objects) {
  const auto found = graph.activity_object_types.find(activity);
  if (found != graph.activity_object_types.end() && !found->second.empty())
    return found->second;

  // Hand-built graphs may not carry discovery annotations.  In that case,
  // bind one object from every type whose projection exposes the activity.
  std::vector<std::string> slots;
  for (const auto& object : objects) {
    if (has_visible_activity(*object.net, activity) &&
        std::find(slots.begin(), slots.end(), object.type) == slots.end())
      slots.push_back(object.type);
  }
  return slots;
}

void enumerate_binding_slots(
    const std::vector<std::string>& slots, std::size_t slot,
    const std::string& activity, const std::vector<SynchronizedObject>& objects,
    const SynchronizedState& state, std::vector<bool>& used,
    std::vector<std::pair<std::size_t, std::size_t>>& firings,
    std::vector<std::pair<std::string, std::string>>& bound_objects,
    std::vector<SynchronizedAction>& actions, std::size_t limit) {
  if (limit != 0 && actions.size() >= limit) return;
  if (slot == slots.size()) {
    SynchronizedAction action;
    action.binding.activity = activity;
    action.binding.objects = bound_objects;
    action.firings = firings;
    actions.push_back(std::move(action));
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
                              firings, bound_objects, actions, limit);
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
    const OcpnPlayoutOptions& options) {
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
    if (slots.empty()) continue;
    std::vector<bool> used(objects.size(), false);
    std::vector<std::pair<std::size_t, std::size_t>> firings;
    std::vector<std::pair<std::string, std::string>> bound_objects;
    std::vector<SynchronizedAction> bindings;
    enumerate_binding_slots(slots, 0, activity, objects, state, used, firings,
                            bound_objects, bindings,
                            options.branching_factor_bindings);
    actions.insert(actions.end(), std::make_move_iterator(bindings.begin()),
                   std::make_move_iterator(bindings.end()));
  }
  if (options.branching_factor_transitions != 0 &&
      actions.size() > options.branching_factor_transitions)
    actions.resize(options.branching_factor_transitions);
  return actions;
}

OcpnPlayoutResult synchronized_playout(const OcpnGraph& graph,
                                       const OcpnPlayoutOptions& options) {
  OcpnPlayoutResult result;
  result.exists_trace_requested = options.exists_trace;
  result.return_sequences_requested = options.return_sequences;

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
      object.marking.assign(net.place_count(), 0);
      object.marking[net.source()] = 1;
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
  visited.insert(synchronized_state_key(initial));
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
    const auto actions = synchronized_actions(graph, objects, state, activities, options);
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
      }
      if (visited.insert(synchronized_state_key(next)).second) {
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

    ExtensivePlayoutOptions trace_options;
    trace_options.max_length = visible_bound(
        net, options.max_bindings_per_activity);
    trace_options.max_traces = options.max_traces;
    trace_options.max_states = options.max_states;
    trace_options.start = 0.0;
    trace_options.case_interval = 0.0;
    trace_options.event_interval = 1.0;
    const auto traces_log = extensive_playout(net, trace_options);
    if (traces_log.traces.empty()) return {};
    for (const auto& object_id : object_ids) {
      const auto old_type = supplied_object_types.find(object_id);
      if (old_type != supplied_object_types.end())
        throw std::invalid_argument(
            "OCPN playout object ids must be unique across object types");
      supplied_object_types[object_id] = type;
      choices.push_back({type, object_id, traces_log.traces});
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
      const auto& selected_trace = choices[choice].traces[selected[choice]];
      for (std::size_t index = 0; index < selected_trace.events.size(); ++index) {
        pending.push_back({selected_trace.events[index].activity,
                           choices[choice].type, choices[choice].object_id,
                           index, choice});
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
      ++emitted_index;
      index = end;
    }
    if (sequences) sequences->push_back(std::move(sequence));
    ++sequence_index;
  };

  std::function<void(std::size_t)> enumerate = [&](std::size_t index) {
    if (sequence_index >= options.max_traces) return;
    if (index == choices.size()) {
      emit();
      return;
    }
    for (std::size_t trace = 0; trace < choices[index].traces.size(); ++trace) {
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
  auto legacy_options = options;
  if (options.exists_trace) legacy_options.max_traces = 1;
  result.log = legacy_ocpn_extensive_playout(
      graph, legacy_options,
      options.return_sequences ? &result.sequences : nullptr,
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
  out << ",\"log\":" << serialize_ocel_json(result.log) << '}';
  return out.str();
}

}  // namespace minera
